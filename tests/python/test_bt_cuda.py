import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

import bt


def sample_hub():
    with open(bt.REPO_ROOT / "config" / "config.json", encoding="utf-8") as f:
        hub = json.load(f)
    return hub


class CudaConfigTests(unittest.TestCase):
    def test_hub_carries_the_cuda_section(self):
        hub = sample_hub()
        self.assertFalse(hub["cuda"]["enable"])
        self.assertIsInstance(hub["cuda"]["parity_tolerance"], float)
        self.assertGreater(hub["cuda"]["parity_tolerance"], 0.0)
        self.assertIsInstance(hub["cuda"]["bin_dir"], str)
        self.assertEqual(hub["cuda"]["doctor_override"], "")

    def test_cuda_cfg_reads_overrides(self):
        hub = sample_hub()
        hub["cuda"]["arch"] = "9.0"
        self.assertEqual(bt.cuda_cfg(hub)["arch"], "9.0")


class CudaFlagTests(unittest.TestCase):
    def test_cuda_compile_flags_add_the_define_and_keep_base_flags(self):
        hub = sample_hub()
        plain = bt.compile_flags(hub)
        cuda = bt.cuda_compile_flags(hub, cuda=True)
        self.assertNotIn(bt.CUDA_DEFINE, plain)
        self.assertIn(bt.CUDA_DEFINE, cuda)
        for flag in plain:
            self.assertIn(flag, cuda)
        self.assertEqual(bt.cuda_compile_flags(hub, cuda=False), plain)

    def test_objects_dirname_splits_between_plain_and_cuda(self):
        hub = sample_hub()
        self.assertNotEqual(bt.objects_dirname(hub), bt.objects_dirname(hub, cuda=True))
        self.assertEqual(bt.objects_dirname(hub), bt.objects_dirname(hub, cuda=False))


class CudaArchTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def write_stub(self, name, body):
        stub = self.root / name
        stub.write_text(body, encoding="utf-8")
        return stub

    def test_gpu_compute_cap_parses_the_smi_row(self):
        stub = self.write_stub("smi_ok.py", 'import sys\nprint("8.6")\n')
        self.assertEqual(bt.gpu_compute_cap([sys.executable, "-I", str(stub)]), "8.6")

    def test_gpu_compute_cap_rejects_garbage_output(self):
        stub = self.write_stub("smi_junk.py", 'import sys\nprint("N/A")\n')
        self.assertIsNone(bt.gpu_compute_cap([sys.executable, "-I", str(stub)]))

    def test_gpu_compute_cap_rejects_failed_smi(self):
        stub = self.write_stub("smi_dead.py", "import sys\nsys.exit(1)\n")
        self.assertIsNone(bt.gpu_compute_cap([sys.executable, "-I", str(stub)]))

    def test_sm_arch_maps_capability_to_arch(self):
        self.assertEqual(bt.sm_arch("8.6"), "sm_86")
        self.assertEqual(bt.sm_arch("12.0"), "sm_120")

    def test_cuda_arch_prefers_the_config_override(self):
        hub = sample_hub()
        hub["cuda"]["arch"] = "9.0"
        self.assertEqual(bt.cuda_arch(hub, "8.6"), "sm_90")
        hub["cuda"]["arch"] = ""
        self.assertEqual(bt.cuda_arch(hub, "8.6"), "sm_86")


class CudaPathsTests(unittest.TestCase):
    def test_cuda_lib_dir_derives_from_the_nvcc_location(self):
        nvcc = Path("C:/tools/cuda/v12.8/bin/nvcc.exe")
        self.assertEqual(bt.cuda_lib_dir(nvcc), Path("C:/tools/cuda/v12.8/lib/x64"))

    def test_cl_candidates_prefer_the_config_override(self):
        hub = sample_hub()
        with tempfile.TemporaryDirectory() as tmp:
            override = Path(tmp) / "hostx64"
            override.mkdir()
            (override / "cl.exe").write_bytes(b"")
            hub["build"]["tools"]["cl"] = str(override)
            candidates = bt.cl_ccbin_candidates(hub)
            self.assertEqual(candidates[0], override)

    def test_cuda_bin_dir_comes_from_the_hub(self):
        hub = sample_hub()
        self.assertEqual(bt.cuda_bin_dir(hub), bt.REPO_ROOT / hub["cuda"]["bin_dir"])


class CudaCommandTests(unittest.TestCase):
    def test_nvcc_compile_command_carries_ccbin_arch_ieee_and_static_cudart(self):
        ccbin = Path("C:/vs/bin/hostx64/x64")
        source = Path("src/ga/cuda/ga_eval.cu")
        obj = Path("build/obj/src/ga/cuda/ga_eval.obj")
        include = Path("repo/src")
        argv = bt.nvcc_compile_command([Path("nvcc.exe")], ccbin, "sm_86", source, obj,
                                       includes=[include])
        self.assertEqual(argv[0], "nvcc.exe")
        self.assertEqual(argv[argv.index("-ccbin") + 1], str(ccbin))
        self.assertIn("-arch=sm_86", argv)
        for flag in bt.CUDA_IEEE_FLAGS:
            self.assertIn(flag, argv)
        self.assertIn("-O3", argv)
        self.assertIn("-cudart=static", argv)
        self.assertEqual(argv[argv.index("-I") + 1], str(include))
        self.assertEqual(argv[argv.index("-c") + 1], str(source))
        self.assertEqual(argv[argv.index("-o") + 1], str(obj))

    def test_link_command_carries_lib_dirs_before_the_cuda_lib(self):
        lib_dir = Path("C:/cuda/lib/x64")
        argv = bt.link_command(
            ["clang"],
            bt.compile_flags(sample_hub()),
            [Path("a.obj")],
            ["cudart_static"],
            Path("app.exe"),
            lib_dirs=[lib_dir],
        )
        lib_pos = argv.index("-L")
        self.assertEqual(argv[lib_pos + 1], str(lib_dir))
        self.assertLess(lib_pos, argv.index("-lcudart_static"))


class CudaProbeTests(unittest.TestCase):
    def test_doctor_override_verdicts_short_circuit_the_probe(self):
        hub = sample_hub()
        hub["cuda"]["doctor_override"] = "pass"
        verdict = bt.probe_cuda(hub, Path(tempfile.gettempdir()))
        self.assertTrue(verdict["ok"])
        self.assertIn("override", verdict["detail"])
        hub["cuda"]["doctor_override"] = "fail"
        verdict = bt.probe_cuda(hub, Path(tempfile.gettempdir()))
        self.assertFalse(verdict["ok"])
        self.assertIn("override", verdict["detail"])

    def test_cached_cuda_verdict_round_trips(self):
        hub = sample_hub()
        with tempfile.TemporaryDirectory() as tmp:
            hub["build"]["dirs"]["env"] = str(Path(tmp) / "env.json")
            verdict = {"ok": True, "arch": "sm_86", "detail": "stub"}
            bt.cache_cuda_verdict(hub, verdict)
            self.assertEqual(bt.cached_env(hub).get("cuda"), verdict)

    def test_cached_env_is_empty_when_the_file_is_missing(self):
        hub = sample_hub()
        with tempfile.TemporaryDirectory() as tmp:
            hub["build"]["dirs"]["env"] = str(Path(tmp) / "absent.json")
            self.assertEqual(bt.cached_env(hub), {})


class CudaPruneTests(unittest.TestCase):
    def test_prune_keeps_the_sibling_cuda_objects_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            current = base / "aaaa"
            sibling = base / "bbbb"
            stale = base / "cccc"
            for child in (current, sibling, stale):
                child.mkdir()
            bt.prune_stale_obj_roots(base, current, bt.Logger(), keep={sibling.name})
            self.assertTrue(current.is_dir())
            self.assertTrue(sibling.is_dir())
            self.assertFalse(stale.is_dir())


class CudaCompileStageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / "src" / "common").mkdir(parents=True)
        (self.root / "src" / "common" / "x.c").write_text("int x;\n", encoding="utf-8")
        (self.root / "vendor" / "sqlite").mkdir(parents=True)
        (self.root / "vendor" / "sqlite" / "sqlite3.c").write_text("int sq;\n", encoding="utf-8")
        self.cu = self.root / "src" / "ga" / "cuda" / "ga_eval.cu"
        self.cu.parent.mkdir(parents=True)
        self.cu.write_text("int cu;\n", encoding="utf-8")
        (self.root / "src" / "ga" / "cuda" / "ga_eval.h").write_text("int cu_h;\n",
                                                                     encoding="utf-8")
        self.log = self.root / "argv.log"
        self.stub_clang = self.write_tool_stub("clang_stub.py")
        self.stub_nvcc = self.write_tool_stub("nvcc_stub.py")

    def tearDown(self):
        self.tmp.cleanup()

    def write_tool_stub(self, name):
        stub = self.root / name
        stub.write_text(
            "import sys\n"
            "from pathlib import Path\n"
            "argv = sys.argv[1:]\n"
            "with open(r'%s', 'a', encoding='utf-8') as log:\n"
            "    log.write(repr(argv) + '\\n')\n"
            "if '-o' in argv:\n"
            "    out = Path(argv[argv.index('-o') + 1])\n"
            "    out.parent.mkdir(parents=True, exist_ok=True)\n"
            "    out.write_bytes(b'stub')\n" % str(self.log),
            encoding="utf-8",
        )
        return stub

    def cuda_plan(self):
        return {
            "ok": True,
            "nvcc": [sys.executable, "-I", str(self.stub_nvcc)],
            "ccbin": str(self.root / "hostx64"),
            "arch": "sm_86",
            "lib_dir": str(self.root / "lib" / "x64"),
            "detail": "stub",
        }

    def argv_lines(self):
        return [line for line in self.log.read_text(encoding="utf-8").splitlines() if line]

    def test_compile_stage_cuda_routes_the_cu_through_nvcc_with_the_define_on_clang(self):
        hub = sample_hub()
        obj_root, entries = bt.compile_stage(
            hub,
            self.root,
            [sys.executable, "-I", str(self.stub_clang)],
            bt.Logger(),
            cuda=self.cuda_plan(),
        )
        lines = self.argv_lines()
        nvcc_lines = [line for line in lines if "-ccbin" in line]
        clang_lines = [line for line in lines if "-ccbin" not in line]
        self.assertTrue(clang_lines)
        self.assertTrue(any(bt.CUDA_DEFINE in line for line in clang_lines))
        self.assertEqual(len(nvcc_lines), 1)
        self.assertIn("-arch=sm_86", nvcc_lines[0])
        self.assertIn("-ccbin", nvcc_lines[0])
        cu_obj = obj_root / "src" / "ga" / "cuda" / "ga_eval.obj"
        self.assertIn(repr(str(cu_obj)), nvcc_lines[0])
        self.assertTrue(cu_obj.is_file())
        self.assertIn((self.cu, cu_obj), entries)
        second_root, second_entries = bt.compile_stage(
            hub,
            self.root,
            [sys.executable, "-I", str(self.stub_clang)],
            bt.Logger(),
            cuda=self.cuda_plan(),
        )
        self.assertEqual(second_root, obj_root)
        still_nvcc = [line for line in self.argv_lines() if "-ccbin" in line]
        self.assertEqual(len(still_nvcc), 1)

    def test_compile_stage_plain_never_invokes_nvcc(self):
        hub = sample_hub()
        obj_root, entries = bt.compile_stage(
            hub,
            self.root,
            [sys.executable, "-I", str(self.stub_clang)],
            bt.Logger(),
        )
        self.assertFalse(self.log.exists() and any("-ccbin" in line for line in self.argv_lines()))
        self.assertNotIn(
            obj_root / "src" / "ga" / "cuda" / "ga_eval.obj", [obj for _, obj in entries]
        )


if __name__ == "__main__":
    unittest.main()
