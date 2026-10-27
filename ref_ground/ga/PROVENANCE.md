# ga reference provenance

retrieval date 2026-10-27, captured by the l13 task.

## purpose

CPU golden references for the ga engine and its tools: the memo pair frequency catalog bt-memofreq writes, the syllable assignment bt-ga-run writes, and the ga run report. the ga overhaul and cuda parity lanes verify their outputs byte-identical or tolerance-equal against these files. expected values come only from these outputs, never from a new implementation under test.

## sources

| source | how produced | verification |
| --- | --- | --- |
| freq.json | bt-memofreq, memofreq.samples 200000, memofreq.seed 20261012, over ref_doc/vietnamese | c port regenerated 2026-10-27, byte-identical to the rust-era artifact of 2026-10-12 |
| syllables.json | bt-ga-run, ga seed 20261012 population 200 generations 500 elite 2 tournament 2 pool_top_k 1600 | c port regenerated 2026-10-27, byte-identical to the rust-era artifact of 2026-10-12 |
| report.md | same bt-ga-run run | c port regenerated 2026-10-27, byte-identical to the rust-era artifact of 2026-10-12 |
| report.json | same bt-ga-run run | c port regenerated 2026-10-27. run_config embeds the config at generation time: the rust-era copy held the 2026-10-12 config, this copy holds the current config. every other field, including both sha pins, per_gen_best, and best_parts, is identical to the rust-era copy |

the rust-era artifacts were produced by the rust tools on 2026-10-12 and lived untracked under data/ga and artifacts/ga, which stay gitignored. the rust side committed none of them, so these c-regenerated copies are the committed references. both sha pins inside report.json match the rust-era values: freq sha 11609bd494b8a4a9487ecdfda4237e5b7552500c0e9b77aa918a21ce04b657db, syllables sha c79c850a102aebf27215dea1b418b6faefffa2caf4dc646674dbe33dd7c0b4bb.

regenerate with python bt.py memofreq then python bt.py ga on an unchanged config, then byte-compare against this directory.

file sha-256 of this directory: freq.json 11609bd494b8a4a9487ecdfda4237e5b7552500c0e9b77aa918a21ce04b657db, syllables.json 80682880a1c6d39cf8f30d38627223ea1493e7296e45ce069e45d93db29372f3, report.json 38a16771f776367c3cadbda2520ef421da455c0373a5d6a0980ac6ce6ddcac62, report.md 61b403bbb325ff33b430fe6a06319018bc02353fed30cd8f907cf56992d4ffdd.

disposition: the rust-era working-tree artifacts under data/ga and artifacts/ga were deleted after the recorded byte-compare, so the cross-era identity rests on the recorded compare in the t34 log and the sha pins above, not on any retained rust-era copy.
