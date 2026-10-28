# visual diff report

screenshots: C:\Users\lavantien\dev\github\blind-trainer\artifacts\e2e
baselines: C:\Users\lavantien\dev\github\blind-trainer\playground\adv-visual-a\bases-drift
tolerance: 48

| pair | verdict | max delta | pixels over | pixels |
| ---- | ------- | --------- | ----------- | ------ |
| algs.png | ok | 0 | 0 | 1024000 |
| case-drill.png | ok | 0 | 0 | 1024000 |
| drill.png | ok | 0 | 0 | 1024000 |
| fatal.png | ok | 0 | 0 | 1024000 |
| memo.png | ok | 0 | 0 | 1024000 |
| menu.png | over tolerance (80624 px) | 222 | 80624 | 1024000 |
| replay.png | ok | 0 | 0 | 1024000 |
| settings.png | ok | 0 | 0 | 1024000 |
| solve.png | ok | 0 | 0 | 1024000 |
| stats.png | ok | 0 | 0 | 1024000 |

10 pairs, 9 ok, 1 failed

verdict: regression, both runs mismatched
