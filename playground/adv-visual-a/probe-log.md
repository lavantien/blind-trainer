# adv-visual-a probe log

date 2026-10-28, probes against commits 6f198b4 and 4fca9f1

## exe contract matrix

run with BT_CONFIG and BT_ARTIFACTS cleared unless stated, exe build/bin/visual-diff.exe

| case | setup | exit | observed |
| --- | --- | --- | --- |
| both dirs absent | no dirs at all | 0 | report written, zero pairs, green |
| both dirs empty | two empty dirs | 0 | report written, zero pairs, green |
| bases empty, shots 1 png | menu.png only in shots | 1 | menu.png: missing on baselines side |
| extra png in shots | extra.png plus matching menu.png both sides | 1 | extra.png: missing on baselines side |
| baseline crc corrupted | one byte flipped in baseline idat | 1 | decode failed on baselines side, png chunk crc invalid |
| update with stale baseline | stale-old.png and keep-me.md in bases | 0 | stale-old.png deleted, keep-me.md kept |
| bt_artifacts hijack | env BT_ARTIFACTS=playground dir | 0 | report.md written under the hijack root |
| selftest | playground workdir | 0 | 4/4 checks passed |

## tgt visual retry matrix

each run: python bt.py visual with BT_CONFIG pinned to a scratch hub copy, paths.baselines and paths.artifacts redirected into this playground, real artifacts/e2e used as shots

| case | setup | exit | verdict line |
| --- | --- | --- | --- |
| drift | baselines copy with menu.png replaced by solve.png bytes | 2 | verdict: regression, both runs mismatched |
| infra | drifted baselines plus e2e.timeout_secs=0 in the hub | 4 | verdict: infra error, re-render failed |
| flake | one byte flipped in artifacts/e2e/menu.png, good baselines copy | 3 | verdict: infra flake, second run passed |

side effect recorded: an aborted smoke tour (timeout 0) empties artifacts/e2e completely, the driver recreates the full set on the next green run

## baselines

10 pngs decode rgba 1280x800, no solid blanks, 10 distinct sha256, slug set equals config e2e.shots, freshly regenerated shots byte-identical to the committed baselines on the current tree

## sandbox

all probe runs wrote the store under artifacts/e2e-user only, real APPDATA blind-trainer store last write 03:07, before the commit at 08:12 and before every probe
