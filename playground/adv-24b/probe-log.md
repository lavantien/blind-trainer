# adv-24b probe log

independent adversarial review of lane-24-docs (commits f36da8c, 3201c98)

## ran

| probe | result |
| --- | --- |
| python bt.py (no target) vs README table | identical, 33 rows, diff in bt_help_actual.txt readme_table.txt |
| python bt.py setup | pass, clang 23.1.4, clang-format, clang-tidy, llvm-cov, nvcc optional probe |
| python scripts/vendor.py | skip, lock unchanged, pins verified in config hub build.vendor (raylib 6.2 sha256, sqlite 3.54.1 sha3_256) |
| python bt.py gen-data | blocked at link mutate.exe by the detached mutation host lock, environmental, recorded as substitution |
| bt-app.exe --smoke with APPDATA redirected | exit 0, 10 shots, blind-trainer.db created under the sandbox APPDATA/blind-trainer |
| bt-app.exe --smoke a --usage b | prints the one-flag reason, exit 2 |
| all 10 puml re-rendered with the pinned jar 1.2026.8 and diagrams.py flags | all 10 png byte-identical, renders kept under dg/ |
| badgesvg.badge_svg byte-compare | coverage.svg equals badge_svg("coverage", "0.0%", 0.0), alg-coverage.svg equals badge_svg("alg coverage", "99.9%", 817/818) |

## include matrix measured

core -> common
scheme -> core
data -> core, scheme
puzzles -> common only
ga -> common only
app -> core, scheme, data, puzzles (no ga)
gendata -> core, data, scheme (no puzzles)
verify -> core, data, puzzles
ga-run -> data, ga, scheme
memofreq -> data, ga, scheme

diagram 01 solid arrows puzzles -> core, ga -> core, ga -> scheme, ga -> data, app -> ga, gendata -> puzzles exist only as build.src_deps/tool_deps declarations in the config hub, not as code dependencies.
