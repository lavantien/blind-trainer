use std::collections::BTreeMap;
use std::fs::{self, File};
use std::io::{BufReader, Read};
use std::path::{Path, PathBuf};
use zip::ZipArchive;

pub type FileSet = BTreeMap<String, PathBuf>;

const READ_CHUNK: usize = 65536;

pub fn staging_file_set(
    staging: &Path,
    exe: &str,
    config_file: &str,
    fonts_dir: &str,
) -> Result<FileSet, String> {
    let mut set = FileSet::new();
    let exe_path = staging.join(exe);
    if !exe_path.is_file() {
        return Err(format!("staging missing {}", exe_path.display()));
    }
    set.insert(exe.to_string(), exe_path);
    let config_path = staging.join(config_file);
    if !config_path.is_file() {
        return Err(format!("staging missing {}", config_path.display()));
    }
    set.insert(config_file.to_string(), config_path);
    let fonts_path = staging.join(fonts_dir);
    if !fonts_path.is_dir() {
        return Err(format!("staging missing {}", fonts_path.display()));
    }
    collect_files(&fonts_path, staging, &mut set)?;
    Ok(set)
}

fn collect_files(dir: &Path, staging: &Path, set: &mut FileSet) -> Result<(), String> {
    for entry in fs::read_dir(dir).map_err(|err| format!("read {}: {err}", dir.display()))? {
        let entry = entry.map_err(|err| format!("read {}: {err}", dir.display()))?;
        let path = entry.path();
        if path.is_dir() {
            collect_files(&path, staging, set)?;
        } else if let Some(name) = rel_posix(&path, staging) {
            set.insert(name, path);
        }
    }
    Ok(())
}

fn rel_posix(path: &Path, base: &Path) -> Option<String> {
    path.strip_prefix(base)
        .ok()
        .map(|rel| rel.to_string_lossy().replace('\\', "/"))
}

pub fn normalize_entry(name: &str, root: &str) -> Option<String> {
    let normalized = name.replace('\\', "/");
    if normalized.ends_with('/') {
        return None;
    }
    let prefix = format!("{root}/");
    Some(
        normalized
            .strip_prefix(&prefix)
            .unwrap_or(&normalized)
            .to_string(),
    )
}

fn open_archive(zip_path: &Path) -> Result<ZipArchive<File>, String> {
    let file = File::open(zip_path).map_err(|err| format!("cannot open zip: {err}"))?;
    ZipArchive::new(file).map_err(|err| format!("cannot open zip: {err}"))
}

pub fn check_zip(zip_path: &Path, root: &str, staging: &FileSet) -> Result<usize, String> {
    let mut archive = open_archive(zip_path)?;
    let mut seen: BTreeMap<String, usize> = BTreeMap::new();
    for index in 0..archive.len() {
        let entry = archive
            .by_index(index)
            .map_err(|err| format!("zip entry {index} unreadable: {err}"))?;
        let Some(name) = normalize_entry(entry.name(), root) else {
            continue;
        };
        if seen.contains_key(&name) {
            return Err(format!("duplicate zip entry {name}"));
        }
        seen.insert(name, index);
    }
    for name in staging.keys() {
        if !seen.contains_key(name) {
            return Err(format!("zip missing {name}"));
        }
    }
    for name in seen.keys() {
        if !staging.contains_key(name) {
            return Err(format!("unexpected zip entry {name}"));
        }
    }
    for (name, index) in &seen {
        let staged = staging
            .get(name)
            .ok_or_else(|| format!("staging missing {name}"))?;
        let mut entry = archive
            .by_index(*index)
            .map_err(|err| format!("zip entry {name} unreadable: {err}"))?;
        if !entry_bytes_equal(&mut entry, staged)? {
            return Err(format!("byte mismatch in zip entry {name}"));
        }
    }
    Ok(seen.len())
}

fn entry_bytes_equal<R: Read>(entry: &mut R, staged: &Path) -> Result<bool, String> {
    let file = File::open(staged).map_err(|err| format!("read {}: {err}", staged.display()))?;
    let mut file = BufReader::new(file);
    let mut entry_chunk = vec![0u8; READ_CHUNK];
    let mut file_chunk = vec![0u8; READ_CHUNK];
    loop {
        let entry_read = read_full(entry, &mut entry_chunk)?;
        let file_read = read_full(&mut file, &mut file_chunk)?;
        if entry_read != file_read || entry_chunk != file_chunk {
            return Ok(false);
        }
        if entry_read == 0 {
            return Ok(true);
        }
    }
}

fn read_full<R: Read>(reader: &mut R, buf: &mut [u8]) -> Result<usize, String> {
    let mut filled = 0;
    while filled < buf.len() {
        match reader.read(&mut buf[filled..]) {
            Ok(0) => break,
            Ok(n) => filled += n,
            Err(err) => return Err(format!("zip read: {err}")),
        }
    }
    Ok(filled)
}

pub fn extract_zip(zip_path: &Path, dest: &Path, root: &str) -> Result<usize, String> {
    let mut archive = open_archive(zip_path)?;
    let mut extracted = 0;
    for index in 0..archive.len() {
        let mut entry = archive
            .by_index(index)
            .map_err(|err| format!("zip entry {index} unreadable: {err}"))?;
        let Some(name) = normalize_entry(entry.name(), root) else {
            continue;
        };
        if name.split('/').any(|part| part == "..") || name.is_empty() {
            return Err(format!("unsafe zip entry {name}"));
        }
        let mut bytes = Vec::new();
        entry
            .read_to_end(&mut bytes)
            .map_err(|err| format!("zip entry {name} unreadable: {err}"))?;
        let path = dest.join(&name);
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)
                .map_err(|err| format!("mkdir {}: {err}", parent.display()))?;
        }
        fs::write(&path, &bytes).map_err(|err| format!("write {}: {err}", path.display()))?;
        extracted += 1;
    }
    Ok(extracted)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::fs::File;
    use std::io::Write;
    use std::sync::atomic::{AtomicU64, Ordering};
    use zip::ZipWriter;
    use zip::write::SimpleFileOptions;

    static DIR_COUNTER: AtomicU64 = AtomicU64::new(0);

    fn temp_dir(tag: &str) -> PathBuf {
        let dir = std::env::temp_dir()
            .join("bt-release-verify-tests")
            .join(format!(
                "{tag}-{}-{}",
                std::process::id(),
                DIR_COUNTER.fetch_add(1, Ordering::Relaxed)
            ));
        fs::create_dir_all(&dir).expect("temp dir");
        dir
    }

    const EXE: &str = "blind-trainer.exe";
    const CONFIG: &str = "config/config.json";
    const FONTS: &str = "assets/fonts";
    const ROOT: &str = "blind-trainer";

    const EXE_BYTES: &[u8] = b"fake exe payload 0123456789";
    const CONFIG_BYTES: &[u8] = br#"{"app": {"window": {"width": 1280, "height": 800}}}"#;
    const FONT_MAIN: &[u8] = b"font main bytes";
    const FONT_BOLD: &[u8] = b"font bold bytes";

    fn make_staging(dir: &Path) {
        fs::create_dir_all(dir.join(FONTS)).expect("fonts dir");
        fs::create_dir_all(dir.join("config")).expect("config dir");
        fs::write(dir.join(EXE), EXE_BYTES).expect("exe");
        fs::write(dir.join(CONFIG), CONFIG_BYTES).expect("config");
        fs::write(dir.join("assets/fonts/main.ttf"), FONT_MAIN).expect("font");
        fs::write(dir.join("assets/fonts/bold.ttf"), FONT_BOLD).expect("font");
    }

    enum Entry<'a> {
        Dir(&'a str),
        File(&'a str, &'a [u8]),
    }

    fn write_zip(path: &Path, entries: &[Entry]) {
        let file = File::create(path).expect("zip file");
        let mut zip = ZipWriter::new(file);
        let options = SimpleFileOptions::default();
        for entry in entries {
            match entry {
                Entry::Dir(name) => zip.add_directory(*name, options).expect("dir entry"),
                Entry::File(name, bytes) => {
                    zip.start_file(*name, options).expect("file entry");
                    zip.write_all(bytes).expect("entry bytes");
                }
            }
        }
        zip.finish().expect("zip finish");
    }

    fn faithful_entries() -> Vec<Entry<'static>> {
        vec![
            Entry::Dir("blind-trainer/"),
            Entry::Dir("blind-trainer/assets/"),
            Entry::Dir("blind-trainer/assets/fonts/"),
            Entry::Dir("blind-trainer/config/"),
            Entry::File("blind-trainer\\blind-trainer.exe", EXE_BYTES),
            Entry::File("blind-trainer\\config\\config.json", CONFIG_BYTES),
            Entry::File("blind-trainer/assets/fonts/main.ttf", FONT_MAIN),
            Entry::File("blind-trainer/assets/fonts/bold.ttf", FONT_BOLD),
        ]
    }

    fn staged_set(dir: &Path) -> FileSet {
        staging_file_set(dir, EXE, CONFIG, FONTS).expect("staging set")
    }

    #[test]
    fn staging_file_set_lists_exe_config_and_whole_fonts_tree() {
        let dir = temp_dir("set");
        make_staging(&dir);
        fs::create_dir_all(dir.join("assets/fonts/condensed")).expect("nested dir");
        fs::write(dir.join("assets/fonts/condensed/italic.ttf"), b"italic").expect("font");
        let set = staged_set(&dir);
        let names: Vec<&str> = set.keys().map(String::as_str).collect();
        assert_eq!(
            names,
            vec![
                "assets/fonts/bold.ttf",
                "assets/fonts/condensed/italic.ttf",
                "assets/fonts/main.ttf",
                "blind-trainer.exe",
                "config/config.json",
            ]
        );
        assert_eq!(set.get("blind-trainer.exe"), Some(&dir.join(EXE)));
        assert_eq!(
            set.get("config/config.json"),
            Some(&dir.join("config").join("config.json"))
        );
    }

    #[test]
    fn staging_file_set_rejects_missing_layout_pieces() {
        let dir = temp_dir("set-missing");
        make_staging(&dir);
        fs::remove_file(dir.join(EXE)).expect("remove exe");
        let err = staging_file_set(&dir, EXE, CONFIG, FONTS).expect_err("missing exe");
        assert!(err.contains("missing"), "err: {err}");
        assert!(err.contains(EXE), "err: {err}");
        fs::write(dir.join(EXE), EXE_BYTES).expect("exe back");
        fs::remove_file(dir.join(CONFIG)).expect("remove config");
        let err = staging_file_set(&dir, EXE, CONFIG, FONTS).expect_err("missing config");
        assert!(err.contains(CONFIG), "err: {err}");
        fs::write(dir.join(CONFIG), CONFIG_BYTES).expect("config back");
        fs::remove_dir_all(dir.join(FONTS)).expect("remove fonts");
        let err = staging_file_set(&dir, EXE, CONFIG, FONTS).expect_err("missing fonts");
        assert!(err.contains(FONTS), "err: {err}");
    }

    #[test]
    fn normalize_entry_maps_backslashes_root_prefix_and_directory_markers() {
        assert_eq!(
            normalize_entry("blind-trainer\\config\\config.json", ROOT),
            Some("config/config.json".to_string())
        );
        assert_eq!(
            normalize_entry("blind-trainer/assets/fonts/main.ttf", ROOT),
            Some("assets/fonts/main.ttf".to_string())
        );
        assert_eq!(normalize_entry("blind-trainer/", ROOT), None);
        assert_eq!(
            normalize_entry("blind-trainer\\assets\\fonts\\", ROOT),
            None
        );
        assert_eq!(
            normalize_entry("other/readme.txt", ROOT),
            Some("other/readme.txt".to_string())
        );
        assert_eq!(normalize_entry(ROOT, ROOT), Some(ROOT.to_string()));
    }

    #[test]
    fn check_zip_accepts_a_faithful_archive_with_archive_quirks() {
        let staging_dir = temp_dir("zip-ok");
        make_staging(&staging_dir);
        let zip = staging_dir.parent().expect("parent").join("ok.zip");
        write_zip(&zip, &faithful_entries());
        assert_eq!(check_zip(&zip, ROOT, &staged_set(&staging_dir)), Ok(4));
    }

    #[test]
    fn check_zip_flags_duplicate_normalized_entries() {
        let staging_dir = temp_dir("zip-dup");
        make_staging(&staging_dir);
        let zip = staging_dir.parent().expect("parent").join("dup.zip");
        write_zip(
            &zip,
            &[
                Entry::File("blind-trainer/assets/fonts/main.ttf", FONT_MAIN),
                Entry::File("blind-trainer\\assets\\fonts\\main.ttf", FONT_MAIN),
                Entry::File("blind-trainer\\blind-trainer.exe", EXE_BYTES),
                Entry::File("blind-trainer\\config\\config.json", CONFIG_BYTES),
                Entry::File("blind-trainer/assets/fonts/bold.ttf", FONT_BOLD),
            ],
        );
        let err = check_zip(&zip, ROOT, &staged_set(&staging_dir)).expect_err("duplicate");
        assert!(
            err.contains("duplicate") && err.contains("assets/fonts/main.ttf"),
            "err: {err}"
        );
    }

    #[test]
    fn check_zip_flags_entries_missing_from_staging() {
        let staging_dir = temp_dir("zip-extra");
        make_staging(&staging_dir);
        let zip = staging_dir.parent().expect("parent").join("extra.zip");
        let mut entries = faithful_entries();
        entries.push(Entry::File("blind-trainer/readme.txt", b"extra"));
        write_zip(&zip, &entries);
        let err = check_zip(&zip, ROOT, &staged_set(&staging_dir)).expect_err("extra");
        assert!(
            err.contains("readme.txt") && err.contains("unexpected"),
            "err: {err}"
        );
    }

    #[test]
    fn check_zip_flags_staged_files_missing_from_zip() {
        let staging_dir = temp_dir("zip-missing");
        make_staging(&staging_dir);
        let zip = staging_dir.parent().expect("parent").join("missing.zip");
        write_zip(
            &zip,
            &[
                Entry::File("blind-trainer\\blind-trainer.exe", EXE_BYTES),
                Entry::File("blind-trainer\\config\\config.json", CONFIG_BYTES),
                Entry::File("blind-trainer/assets/fonts/main.ttf", FONT_MAIN),
            ],
        );
        let err = check_zip(&zip, ROOT, &staged_set(&staging_dir)).expect_err("missing");
        assert!(
            err.contains("missing") && err.contains("assets/fonts/bold.ttf"),
            "err: {err}"
        );
    }

    #[test]
    fn check_zip_flags_tampered_entry_bytes() {
        let staging_dir = temp_dir("zip-tamper");
        make_staging(&staging_dir);
        let zip = staging_dir.parent().expect("parent").join("tamper.zip");
        write_zip(
            &zip,
            &[
                Entry::File("blind-trainer\\blind-trainer.exe", EXE_BYTES),
                Entry::File("blind-trainer\\config\\config.json", CONFIG_BYTES),
                Entry::File("blind-trainer/assets/fonts/main.ttf", b"tampered font"),
                Entry::File("blind-trainer/assets/fonts/bold.ttf", FONT_BOLD),
            ],
        );
        let err = check_zip(&zip, ROOT, &staged_set(&staging_dir)).expect_err("tampered");
        assert!(
            err.contains("assets/fonts/main.ttf") && err.contains("byte"),
            "err: {err}"
        );
    }

    #[test]
    fn check_zip_rejects_unreadable_archive() {
        let staging_dir = temp_dir("zip-absent");
        make_staging(&staging_dir);
        let zip = staging_dir.parent().expect("parent").join("absent.zip");
        let err = check_zip(&zip, ROOT, &staged_set(&staging_dir)).expect_err("absent zip");
        assert!(err.contains("cannot open zip"), "err: {err}");
    }

    #[test]
    fn extract_zip_writes_staging_layout_without_the_root_folder() {
        let staging_dir = temp_dir("extract-ok");
        make_staging(&staging_dir);
        let zip = staging_dir.parent().expect("parent").join("extract-ok.zip");
        write_zip(&zip, &faithful_entries());
        let dest = temp_dir("extract-dest");
        assert_eq!(extract_zip(&zip, &dest, ROOT), Ok(4));
        assert_eq!(fs::read(dest.join(EXE)).expect("exe"), EXE_BYTES);
        assert_eq!(fs::read(dest.join(CONFIG)).expect("config"), CONFIG_BYTES);
        assert_eq!(
            fs::read(dest.join("assets/fonts/main.ttf")).expect("font"),
            FONT_MAIN
        );
        assert_eq!(
            fs::read(dest.join("assets/fonts/bold.ttf")).expect("font"),
            FONT_BOLD
        );
        assert!(!dest.join(ROOT).exists());
    }

    #[test]
    fn extract_zip_rejects_parent_escape_entries() {
        let dir = temp_dir("extract-escape");
        let zip = dir.join("escape.zip");
        write_zip(&zip, &[Entry::File("blind-trainer/../evil.txt", b"evil")]);
        let err = extract_zip(&zip, &dir.join("dest"), ROOT).expect_err("escape");
        assert!(
            err.contains("evil.txt") && err.contains("unsafe"),
            "err: {err}"
        );
        assert!(!dir.join("evil.txt").exists());
    }
}
