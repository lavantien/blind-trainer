# data vendor reference provenance

retrieval date 2026-10-28, captured by the l9 t31 task.

## purpose

golden references for the gendata tool: the pair-space case catalog cases.json, the special sheet catalog specials.json, and the generation manifest manifest.json. the c port of bt-gendata verifies its outputs byte-identical against these files. expected values come only from these outputs, never from a new implementation under test.

## sources

| source | how produced | verification |
| --- | --- | --- |
| cases.json | rust bt-gendata over ref_ground solver sheets, config synth max_setup 3 max_insert 4 node_budget 50000000, algorithm first-setup-plateau-1 | c port regenerated 2026-10-28, byte-identical, 575004 bytes |
| specials.json | same run | c port regenerated 2026-10-28, byte-identical, 39593 bytes |
| manifest.json | same run, generated_at 2026-10-15T15:17:45Z | c port regenerated 2026-10-28, identical on every field except generated_at, which is the run stamp by design |

the rust-era artifacts were produced by the rust tool on 2026-10-15 and lived untracked under data/vendor, which stays gitignored as the tool's live output directory. the synth params pin inside manifest.json is dead579f6d96481fa46f82680b33d19b301de595daebb0fcae7707fe4b339106; the c port reproduces it from the live config through its serde-parity serializer.

regenerate with python bt.py gen-data on an unchanged config and corpus, or with playground/gendata/run_tool.py against a redirected paths.data_vendor when foreign in-flight trees block the whole-area build, then byte-compare with playground/gendata/compare_regen.py, which reads this directory.

disposition: data/vendor remains the tool's runtime output directory and may be rewritten by later runs, this committed copy is the immutable reference. the cross-era identity rests on the byte-compares recorded in the t31 log and the sha pins below.

file sha-256 of this directory: cases.json fecefb09a6b6e462a6e4fdf29bbafd42e2d91d6a0cc6a87eaa69743880b5fa9b, specials.json 627d2bdeac98061ebdf56252677b07ecc5a68832565021196ef5e62a644348df, manifest.json a7e72cacef736c44900fb244d8d1dd73dfec92712f2004088775443804e704bf.
