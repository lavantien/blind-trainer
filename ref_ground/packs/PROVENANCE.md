# pack seed provenance

The 15 TSVs in this directory are byte-identical copies of cubing-trainer's seed packs at commit f7ecf2e19a32960d0c0338c528594e0d0c68804f (https://github.com/lavantien/cubing-trainer, MIT, same author), source path db/seeds/packs/<file>. Import date 2026-10-15, copied with a plain file copy and verified by sha-256 against the origin tree. Original data capture date for every row is 2026-10-10, recorded per row in the source_at column. Each row carries its own ultimate source in the source_url column. The full sourcing story per pack, including archived page snapshots, case key derivation, and recorded discrepancies, lives in cubing-trainer at db/seeds/packs/PROVENANCE.md at the pinned commit and is the reference of record for content questions.

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

## registry notes

Event names follow the adopt plan: 2x2, sq1, pyraminx, megaminx, skewb, fto, bigcube, 3bld. Method and stage names follow cubing-trainer's seed registry at the pinned commit, src/packs/seed_registry.c, lowered to plain strings. cubing-trainer registers bigcube files once per cube size: bigcube_parity.tsv as 4x4 yau plus 5x5 to 7x7 reduction, bigcube_l2e.tsv as 5x5 to 7x7 reduction, both stage parity, merging into one 17 row deck per size. This registry keeps one row per file with event bigcube and the n array holding the registered sizes, method yau for parity (the 4x4 registration of record) and reduction for l2e. Applicability caveats recorded in cubing-trainer's provenance still stand: the oll4 and pll4 rows describe dedge parity which occurs on even cubes only, and they ride along on the odd sizes because the registry binds whole files.

The bld files keep cubing-trainer's method and stage bindings: bld_3style_specials as 3style parity, bld_oldpoch as old_pochmann concept, bld_m2 as m2 concept. The sq1 files share method lbl, cubing-trainer's beginner-method bucket, with one stage per file.

bld_3style_specials.tsv derives from the two blddb workbooks already committed under ref_ground/ per the GPL carve-out in DATA_PROVENANCE.md, so its license posture follows that section. Every other pack transcribes public web pages with per-row source URLs and no explicit license, same posture as the speedsolving wiki snapshots in ref_doc.

## verification

Row counts above were checked against the origin files at import time: 42, 15, 35, 39, 169, 7, 7, 16, 99, 5, 12, 36, 85, 40, 24, total 633 rows. Every copy matched its origin by sha-256 before commit. The token inventory in token_inventory.md enumerates the distinct alg tokens per file and feeds the engine grammar pinning.
