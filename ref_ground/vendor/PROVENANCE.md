# data vendor reference provenance

re-pin date 2026-11-06.

## purpose

golden references for the gendata tool: the pair-space case catalog cases.json, the special sheet catalog specials.json, and the generation manifest manifest.json. the c port of bt-gendata verifies its outputs byte-identical against these files. expected values come only from these outputs, never from a new implementation under test.

## sources

| source | how produced | verification |
| --- | --- | --- |
| cases.json | c bt-gendata production regen 2026-11-05T23:03:18Z, config synth max_setup 3 max_insert 4 node_budget 50000000, algorithm first-setup-plateau-1+inverse-1 | copied byte-identical from the run output at data/vendor, 575499 bytes, verify.exe passes 1077 of 1077 corpus rows against it |
| specials.json | same run | copied byte-identical from the run output at data/vendor, 38621 bytes |
| manifest.json | same run, generated_at 2026-11-05T23:03:18Z | copied byte-identical from the run output at data/vendor, 3528 bytes, generated_at is the run stamp by design |

## divergence from the previous reference

the previous reference was the rust-era output of 2026-10-15 under algorithm first-setup-plateau-1. two committed changes moved the canonical outputs since. the mirrored-inverse derivation fills cases the synth search could not cover, every derived alg is the mechanically re-verified inverse of a verified mirror row, this added 1 case, the corner N-I row derived from cherry corner row 109 at position UFR-RUB-FUL. the parity verdict fix 51373ea checks the sticker level swap instead of noting orientation defects, so the 18 parity specials no longer carry twist notes, every other field on those rows is unchanged.

the first-era artifacts were produced by the rust tool on 2026-10-15 and byte-verified against the c port on 2026-10-28. the synth params pin inside manifest.json is d7050d616b2d64269416e425ca9bc6771a877fc94441439a09f4e40755d6cb55; the c port reproduces it from the live config through its serde-parity serializer.

regenerate with python bt.py gen-data on an unchanged config and corpus, or with playground/gendata/run_tool.py against a redirected paths.data_vendor when foreign in-flight trees block the whole-area build, then byte-compare with playground/gendata/compare_regen.py, which reads this directory.

disposition: data/vendor remains the tool's runtime output directory and may be rewritten by later runs, this committed copy is the immutable reference. the cross-era identity rests on the recorded byte-compares and the sha pins below.

file sha-256 of this directory: cases.json 107df1a0061d6170c282fceed724e2358686fad9d84a0b4abb4c48cad4c4bcd9, specials.json a238c29d381b9ed4fa94342c74a458f40d91b8b8d21bd3aa122f2a584553f2c0, manifest.json ab2a6dffb0e265b7e9da2beb64e446bd889ca65a0ebc0fa10c04ad8c3a2a43ab.
