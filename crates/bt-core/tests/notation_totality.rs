mod common;

use bt_core::Alg;

#[test]
fn all_vendor_algs_parse() {
    let rows = common::vendor_rows();
    assert_eq!(rows.len(), 1077);
    for (rel, text) in &rows {
        Alg::parse(text).unwrap_or_else(|e| panic!("{} alg {:?}: {}", rel, text, e));
    }
}

#[test]
fn no_sheet_uses_tokens_outside_the_dialect() {
    for (rel, text) in common::vendor_rows() {
        let alg = Alg::parse(&text).unwrap();
        let canon = alg.format();
        assert!(
            !canon.contains("2'"),
            "{} canonical form regressed: {:?} from {:?}",
            rel,
            canon,
            text
        );
    }
}
