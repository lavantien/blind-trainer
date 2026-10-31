import hashlib
import json
import shutil
import stat
import sys
import time
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import bt


class VendorError(RuntimeError):
    pass


def stamp():
    return time.strftime("%Y-%m-%dT%H:%M:%S")


def remove_tree(path):
    def clear_readonly(func, target, *_):
        Path(target).chmod(stat.S_IWRITE)
        func(target)

    shutil.rmtree(path, onexc=clear_readonly)


def digest_file(path, algorithm):
    hasher = hashlib.new(algorithm)
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 16), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def verify_digest(path, algorithm, expected):
    if not path.is_file():
        raise VendorError(f"digest target missing: {path}")
    actual = digest_file(path, algorithm)
    if actual != expected:
        raise VendorError(
            f"digest mismatch for {path}: expected {expected} got {actual}"
        )
    return actual


def pin_algorithm(pin):
    if "sha256" in pin:
        return "sha256", pin["sha256"]
    if "sha3_256" in pin:
        return "sha3_256", pin["sha3_256"]
    raise VendorError(f"pin {pin['ref']} carries no digest")


def fetch_archive(url, target):
    try:
        with (
            urllib.request.urlopen(url, timeout=300) as response,
            open(target, "wb") as out,
        ):
            shutil.copyfileobj(response, out)
    except (urllib.error.URLError, OSError) as e:
        raise VendorError(f"download {url} failed: {e}") from e


def safe_extract(archive, staging):
    with zipfile.ZipFile(archive) as zf:
        for info in zf.infolist():
            name = info.filename.replace("\\", "/")
            if name.startswith("/") or ".." in name.split("/"):
                raise VendorError(f"unsafe zip entry: {info.filename}")
        zf.extractall(staging)


def ensure_pin(name, pin, root, staging_root, downloads_dir, prior, logger):
    unit = f"vendor {name}"
    algorithm, expected = pin_algorithm(pin)
    dest = root / pin["destination"]
    lock_matches = prior is not None and prior.get("hash") == expected
    landmarks = pin.get("files", ["src/raylib.h"])
    files_ok = all((dest / fname).is_file() for fname in landmarks)
    if lock_matches and files_ok:
        logger.line("skip", f"{unit} {dest} matches pin {pin['ref']}")
        return None
    downloads_dir.mkdir(parents=True, exist_ok=True)
    archive = downloads_dir / Path(pin["url"]).name
    if archive.is_file():
        try:
            verify_digest(archive, algorithm, expected)
            logger.line("skip", f"{unit} cached {archive.name} digest matches")
        except VendorError:
            archive.unlink()
    if not archive.is_file():
        logger.entry(unit, pin["url"])
        fetch_archive(pin["url"], archive)
        logger.line("fetch", f"{archive} {archive.stat().st_size} bytes")
    verify_digest(archive, algorithm, expected)
    staging = staging_root / name
    if staging.exists():
        remove_tree(staging)
    staging.mkdir(parents=True)
    extract_unit = f"extract {name}"
    logger.entry(extract_unit, str(archive))
    safe_extract(archive, staging)
    logger.exit(extract_unit, 0)
    if dest.exists():
        remove_tree(dest)
    root_part = staging / pin["archive_root"] if pin["archive_root"] else staging
    if "files" in pin:
        dest.mkdir(parents=True, exist_ok=True)
        for fname in pin["files"]:
            matches = sorted(root_part.rglob(fname))
            if not matches:
                raise VendorError(
                    f"{extract_unit}: {fname} not found inside {archive.name}"
                )
            shutil.copy2(matches[0], dest / fname)
        remove_tree(staging)
    else:
        dest.parent.mkdir(parents=True, exist_ok=True)
        root_part.rename(dest)
    logger.line("move", f"{extract_unit} -> {dest}")
    return {
        "name": name,
        "ref": pin["ref"],
        "url": pin["url"],
        "hash": expected,
        "algorithm": algorithm,
        "timestamp": stamp(),
    }


def lib_is_fresh(lib, sources):
    if not lib.is_file():
        return False
    lib_time = lib.stat().st_mtime
    for source in sources:
        if not source.is_file():
            return False
        if source.stat().st_mtime >= lib_time:
            return False
    return True


def build_raylib(hub, root, logger):
    cfg = hub["build"]["raylib_build"]
    unit = "build raylib"
    src_dir = root / cfg["source_dir"]
    sources = sorted(src_dir.glob("*.c"))
    if not sources:
        raise VendorError(f"{unit}: no sources under {src_dir}")
    lib_path = root / cfg["lib"]
    if lib_is_fresh(lib_path, sources):
        logger.entry(unit, str(lib_path))
        logger.line("skip", f"{unit} {lib_path} newer than {len(sources)} sources")
        logger.exit(unit, 0)
        return None
    logger.entry(unit, f"{len(sources)} translation units from {src_dir}")
    clang = bt.resolve_tool("clang", hub)
    ar = bt.resolve_tool("llvm_ar", hub)
    obj_dir = root / cfg["obj_dir"]
    obj_dir.mkdir(parents=True, exist_ok=True)
    lib_path.parent.mkdir(parents=True, exist_ok=True)
    flags = [
        "-target",
        hub["build"]["target"],
        f"-std={hub['build']['std']}",
        *[flag for flag in hub["build"]["cflags"] if flag != "-Werror"],
        *hub["build"]["release_flags"],
        *cfg["extra_flags"],
        *[f"-D{define}" for define in cfg["defines"]],
        *[
            part
            for include in cfg["include_dirs"]
            for part in ("-I", str(root / include))
        ],
    ]
    units = []
    for source in sources:
        obj = obj_dir / f"{source.stem}.obj"
        units.append(
            (f"compile {source.name}", [str(clang), *flags, "-c", source, "-o", obj])
        )
    results = bt.run_units(units, jobs=bt.jobs_of(hub))
    failed = [r for r in results if r.returncode != 0]
    for result in failed:
        logger.fail(result.label, result.output[-2000:] or "no output")
    if failed:
        raise VendorError(f"{unit}: {len(failed)} of {len(units)} compiles failed")
    patched = sum(
        1
        for source in sources
        if bt.patch_coff_timestamp(obj_dir / f"{source.stem}.obj")
    )
    logger.line("repro", f"{patched} object timestamps zeroed")
    bt.run([ar, "rcs", lib_path, *[obj_dir / f"{s.stem}.obj" for s in sources]], unit)
    size = lib_path.stat().st_size
    sha256 = digest_file(lib_path, "sha256")
    logger.line("artifact", f"{lib_path} size={size} sha256={sha256}")
    logger.exit(unit, 0)
    return {"sha256": sha256, "size": size}


def read_lock(path):
    if not path.is_file():
        return []
    try:
        return json.loads(path.read_text(encoding="utf-8"))["items"]
    except (OSError, json.JSONDecodeError, KeyError):
        return []


def write_lock(path, items):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps({"items": items}, indent=2) + "\n", encoding="utf-8")
    tmp.replace(path)


def main():
    logger = bt.Logger()
    logger.entry("vendor")
    try:
        hub = bt.load_hub()
        root = bt.REPO_ROOT
        dirs = hub["build"]["dirs"]
        staging_root = root / dirs["staging"]
        downloads_dir = root / dirs["downloads"]
        lock_path = root / dirs["vendor_lock"]
        prior = {item["name"]: item for item in read_lock(lock_path)}
        updated = {}
        for name, pin in hub["build"]["vendor"].items():
            item = ensure_pin(
                name, pin, root, staging_root, downloads_dir, prior.get(name), logger
            )
            if item is not None:
                updated[name] = item
        lib = build_raylib(hub, root, logger)
        items = []
        for name in hub["build"]["vendor"]:
            if name in updated:
                items.append(updated[name])
            elif name in prior:
                items.append(prior[name])
            else:
                raise VendorError(f"no lock item for {name}")
        if lib is not None:
            raylib_item = next(item for item in items if item["name"] == "raylib")
            raylib_item["lib_sha256"] = lib["sha256"]
            raylib_item["lib_size"] = lib["size"]
            raylib_item["timestamp"] = stamp()
        if items != [prior.get(item["name"]) for item in items]:
            write_lock(lock_path, items)
            logger.line("write", f"{lock_path}")
        else:
            logger.line("skip", f"lock unchanged {lock_path}")
        logger.exit("vendor", 0)
        return 0
    except (
        VendorError,
        bt.BtError,
        OSError,
        KeyError,
        ValueError,
        zipfile.BadZipFile,
    ) as e:
        logger.fail("vendor", f"{type(e).__name__}: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
