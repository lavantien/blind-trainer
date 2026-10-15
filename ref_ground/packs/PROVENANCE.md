# pack seed provenance

The 15 imported TSVs in this directory are byte-identical copies of cubing-trainer's seed packs at commit f7ecf2e19a32960d0c0338c528594e0d0c68804f (https://github.com/lavantien/cubing-trainer, MIT, same author), source path db/seeds/packs/<file>. Import date 2026-10-15, copied with a plain file copy and verified by sha-256 against the origin tree. Original data capture date for every row is 2026-10-10, recorded per row in the source_at column. Each row carries its own ultimate source in the source_url column. The full sourcing story per pack, including archived page snapshots, case key derivation, and recorded discrepancies, lives in cubing-trainer at db/seeds/packs/PROVENANCE.md at the pinned commit and is the reference of record for content questions.

The 3 cfop_*.tsv files were authored in this repo on 2026-10-16 from J Perm pages fetched live that day, see the authored cfop files section below.

Format: 5 tab separated columns, single header line, case_key, alg, front_json, source_url, source_at. One row per case, alg text stored verbatim from the source page. registry.json binds each file to event, method, stage, n, and the expected row count verified below.

## per-file summary

| file | rows | sha-256 | ultimate source hosts |
| --- | --- | --- | --- |
| twoby2_cll.tsv | 42 | c4ba3856bd6882bf0d7fe5439bf73b57df78ba8d9aee9b92828b258077a5fed2 | jperm.net/algs/2x2/cll |
| skewb_sarah.tsv | 15 | 1a84cafdec282e6b93161dbca31a1876524013a511fee6e8925bffbb7e652e04 | sarah.cubing.net/skewb/skewb-guide.pdf |
| pyraminx_l4e.tsv | 35 | d06274a4222bc4af2c33d62f11bf771627a2c919fff5f69e8af7b57ac2d1b02b | cubing.mikeg.me.uk/cubing-algs/html/l4e.html |
| megaminx_4lll.tsv | 39 | 01c471295f88989af8b962d4fbb4b292e01f16ca136ca8374022ac419b7ce77c | cubeskills.com, intermediate megaminx techniques PDF |
| sq1_cubeshape.tsv | 169 | 359014e94e11748e2286dc89b946c62e7877641a747d1bd73a5ee8286cbe51a9 | cubingapp.com/algorithms/SQ1-Cube-Shape |
| sq1_co.tsv | 7 | f77affbdd36e7528349bbf86510f2267882affcc271c6b9a5a1f522c9234c64d | speedcube.us, square-1 CO page |
| sq1_eo.tsv | 7 | 086d0dac5d7ffbe6f966f803f45d6e78beced2b488ee9a9fe0f45fb2d82f0416 | sarah.cubing.net/square-1/eo |
| sq1_cp.tsv | 16 | 431d6de24d89ddeb993ae5981c7b95ebf12afd7be3b22ca59f585ecf3a853451 | sarah.cubing.net/square-1/cp |
| sq1_ep.tsv | 99 | 6ce1ff8dd6c996a20885b456e22c732350c48a061a81288e0b3c72049de4f5ae | sarah.cubing.net/square-1/ep |
| bigcube_parity.tsv | 5 | c5a7aa3eabb2aca27dad83588451cef2ddd0bab34259af2aa8bbf2b71ccbca37 | speedcube.us, 4x4 OLL parity, 4x4 PLL parity, 5x5 edge parity pages |
| bigcube_l2e.tsv | 12 | d083a3ca9a5d5bd5fe03c13fbd0cda0cb65f403313869fc297605f9ecd35e12e | speedcube.us/blogs/5x5-last-two-edges |
| fto_l4t_redux.tsv | 36 | af7f0ab528243053fe4aec8f9c51ee36be314b4e2be4ce6c37709ec215c33812 | docs.google.com, James Straughan L4T Redux L4E sheet (35 rows) plus Vincent Trang guide (1 row) |
| bld_3style_specials.tsv | 85 | 8afb0790ad2b6c4d83b0f432b3e9a931f095f56488ce2030817c50e3bf86ca57 | per-row bit.ly and tinyurl.com short links to youtube reconstructions, data derived from the blddb workbooks |
| bld_oldpoch.tsv | 40 | 61f8edb533f0742d3320e5c56c6e76240645c51465f4cdfdc8699361602cfe11 | jperm.net/bld (33), stefan-pochmann.de (5), speedsolving.com wiki (1), bldtrainer.com (1) |
| bld_m2.tsv | 24 | 82a2e92b5c47ee97bd394ab44a7d24865cf04ec83129996879d5054c114f9f11 | speedsolving.com wiki M2 page (22), youtube.com J Perm M2 tutorial (1), cubefreak.net (1) |
| cfop_pll.tsv | 21 | bf049a10d44c00b0e475e2589278b041d8f5e82517bb51f0f4bde5598e360676 | jperm.net/algs/pll |
| cfop_oll.tsv | 57 | d8e8388bde68b5d66252cfccfc5bde0eda4ba8633fce9ab5a36e75c4b3b70e83 | jperm.net/algs/oll |
| cfop_f2l.tsv | 164 | e5027c7c69d4c6058166b8e6c4daa9ae41a88d6f8f486375a30f775aeb9360a7 | jperm.net/3x3/cfop/f2l via bit.ly/bestf2l, J Perm advanced F2L sheet |

## authored cfop files

All three files were captured on 2026-10-16 from jperm.net pages fetched live that day. The pll and oll case data is served to the pages by the site scripts /lib/pll.js and /lib/oll.js, archived as ref_doc/jperm/pll-algs.js and ref_doc/jperm/oll-algs.js with entries in ref_doc/MANIFEST.md. Alg text is stored verbatim from the source, including grouping parentheses. Row order follows page order, and every row points its source_url at the serving page.

cfop_pll.tsv: 21 rows, one per PLL case on the page. The alg column carries the first algorithm listed for each case, verbatim. Case keys are the page's case names lowercased (pll.aa for Aa through pll.z for Z), front_json records the page case name. Five rows keep a whole-cube rotation inside the alg text, x or x' in pll.aa, pll.ab, pll.e, x in pll.ja, y in pll.v: those are J Perm's own primary algs and bt-core's grammar parses rotations, so they stay verbatim. Six rows carry grouping parentheses (pll.t, pll.nb, pll.ga, pll.gb, pll.gc, pll.gd) that the planned seed loader normalizer strips.

cfop_oll.tsv: 57 rows, one per OLL case on the page, numbered 1 to 57 in the page's own numbering, which matches the standard OLL numbering (27 is Sune, 21 is the H cross case). The alg column carries the first algorithm listed for each case, verbatim. Case keys are oll.01 through oll.57 padded to two digits, front_json records the page case number and the page's shape group label. No oll row carries a rotation, four carry grouping parentheses (oll.22, oll.26, oll.54, oll.56).

cfop_f2l.tsv: 164 rows, the 41 basic cases of the sheet's Section 1 instantiated for the 4 slots fr, fl, br, bl, keys f2l.<slot>.<nn>. jperm.net has no per-case F2L web page, its F2L algorithms ship as the free sheet Best Algorithms for ALL Advanced F2L Cases (updated 2021 August 8) linked from jperm.net/3x3/cfop/f2l through bit.ly/bestf2l, archived as ref_doc/jperm/f2l-bestf2l.pdf. The sheet states 41 basic cases and holds exactly 41 case diagrams in Section 1, verified by image extraction; the per-case alg lists were read from the PDF layout and cross-checked to reproduce the PDF's own content-stream order exactly. Cases are numbered 01 to 41 in sheet reading order (row-major, left column before right) and grouped by the sheet's subsection labels in front_json. Every sheet case is depicted for the front-right slot, confirmed visually on rendered pages: y-prefixed alternatives on the sheet are left-handed executions for the same front-right case, not other slots.

The base alg per case is the first listed line that has no cube-rotation prefix and is not pink-highlighted on the sheet (the sheet's own marker for algs affecting more than one slot). When every listed line carries a rotation (9 cases: 04, 08, 10, 12, 14, 16, 17, 24, 34) or the only line is multi-slot (case 07), the first listed line is kept verbatim; front_json records the chosen line index in alt whenever it is not the first. The fr rows store the base alg verbatim, parentheses included. The fl, bl, and br rows store the same base alg conjugated to the target slot by whole-cube y rotations, applied as a mechanical face relabeling: the F R B L cycle appropriate to the slot with U and D fixed, wide moves following their face, and slices following their face with direction inverted when the followed face maps to its opposite (M follows L, E follows D, S follows F). Generation is scripted in playground/a8/gen_tsv.py with the per-case picks in playground/a8/f2l_picks.json, so the derivation is reproducible from the archived PDF.

Notes for the loader and verify stages: f2l case 29 uses the sheet's (g)3 repeat notation, so the planned seed loader normalizer must expand (g)n for n of both 2 and 3; f2l case 07 is multi-slot by the sheet's own marking, so a strict slot-local tier 2 predicate will flag its rows; the 9 rotation-prefixed base cases carry a leading y in all four of their slot rows. Every token of all three authored files falls inside the bt-core Alg token grammar of crates/bt-core/src/alg.rs, verified by playground/a8/grammar_scan.py with the report kept at playground/a8/grammar_scan_report.md (pll 22 distinct tokens, oll 23, f2l 31).

## registry notes

Event names follow the adopt plan: 2x2, sq1, pyraminx, megaminx, skewb, fto, bigcube, 3bld, 3x3, oh. Method and stage names follow cubing-trainer's seed registry at the pinned commit, src/packs/seed_registry.c, lowered to plain strings. cubing-trainer registers bigcube files once per cube size: bigcube_parity.tsv as 4x4 yau plus 5x5 to 7x7 reduction, bigcube_l2e.tsv as 5x5 to 7x7 reduction, both stage parity, merging into one 17 row deck per size. This registry keeps one row per file with event bigcube and the n array holding the registered sizes, method yau for parity (the 4x4 registration of record) and reduction for l2e. Applicability caveats recorded in cubing-trainer's provenance still stand: the oll4 and pll4 rows describe dedge parity which occurs on even cubes only, and they ride along on the odd sizes because the registry binds whole files.

The 3 cfop files register twice, once under event 3x3 and once under event oh with the same file keys, method cfop, stages pll, oll, f2l, n 3: the TSVs are shared and the oh rows exist only so oh practice gets its own cards, matching the adopt plan. The 21, 57, and 164 expected rows cross-check cubing-trainer's hardcoded case lists (21 pll case names, oll 01 to 57, f2l as 41 cases by 4 slots fr, fl, br, bl).

The bld files keep cubing-trainer's method and stage bindings: bld_3style_specials as 3style parity, bld_oldpoch as old_pochmann concept, bld_m2 as m2 concept. The sq1 files share method lbl, cubing-trainer's beginner-method bucket, with one stage per file.

bld_3style_specials.tsv derives from the two blddb workbooks already committed under ref_ground/ per the GPL carve-out in DATA_PROVENANCE.md, so its license posture follows that section. Every other pack transcribes public web pages with per-row source URLs and no explicit license, same posture as the speedsolving wiki snapshots in ref_doc.

## data defects (recorded 2026-10-16, rows stay verbatim)

Nine sq1_cubeshape.tsv rows fail mechanical execution under the wave A square-1 engine. The pack files stay byte-verbatim per the a3 ruling, the seed loader in crates/bt-data/src/seedpack.rs carries this exact list through `expected_failure`, and bt-verify treats exactly these rows as expected failures with the recorded reason. Every other row of every pack stays fail-closed.

| row | defect |
| --- | --- |
| cs.119 | unexecutable slash sequence, hand traced: cs.122 differs from it by one twist sign and executes, so the row is held as a source typo |
| cs.075, cs.078, cs.084, cs.089, cs.097, cs.109, cs.118, cs.129 | source follows a flipped layer convention for these shapes, hand verified against the engine, recorded as a convention mismatch rather than a typo |

## recorded loader normalizations

The seed loader normalizes alg text before storage and hashing, and records every non-syntax transformation in a front_json `loader_notes` array so the source text stays recoverable. Transformations that change meaning-carrying text beyond spacing and grouping: bigcube_parity.oll4 writes the whole-cube rotation as literal `X`, the loader maps exactly that token to `x` (the nxn grammar token) and notes it. sq1_ep rows carry leading AUF groups, forms (U), (U'), (U2), (D), (D'), (D2), (U D), (U' D), (U D2), (U' D2), (U2 D), (U2 D'), and 37 rows end with a standalone `*` equator marker, both stripped into loader_notes verbatim because the engine grammar models only (x,y) twists and the slash, and converting them to twists would rest on an unpinned sign convention. bld_oldpoch op.09 and op.32 hold prose in the alg column, `apply swap directly, no setup`, moved verbatim into loader_notes with an empty stored alg. Syntax-only steps that no row can observe as text change: commas to spaces outside sq1 tuples, whitespace collapsing, grouping paren stripping, (g)2, (g)3, and bracket star repeat expansion, sq1 bare tuple wrapping, and the `->` chain suffix of skewb_sarah rows, kept as a chain note.

## verification

Row counts of the 15 imported files were checked against the origin files at import time: 42, 15, 35, 39, 169, 7, 7, 16, 99, 5, 12, 36, 85, 40, 24, total 631 rows. Every copy matched its origin by sha-256 before commit. The token inventory in token_inventory.md enumerates the distinct alg tokens per file and feeds the engine grammar pinning.

The 3 authored cfop files add 21, 57, and 164 rows, captured 2026-10-16: column format, key uniqueness, row counts, and token grammar scope were verified by playground/a8/grammar_scan.py against the bt-core Alg token grammar, and the f2l case-to-alg pairing was verified to reproduce the archived PDF's content-stream order exactly. Mechanical verification of the algs themselves is wave B work, not claimed here.
