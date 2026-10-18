use bt_data::seedpack::{NSpec, PackEntry, load_pack};

fn main() {
    let entry = PackEntry {
        file: "pyraminx_l4e.tsv".to_string(),
        event: "pyraminx".to_string(),
        method: "l4e".to_string(),
        stage: "l4e".to_string(),
        n: NSpec::Unset,
        expected_rows: 35,
    };
    eprintln!("before load");
    let rows = load_pack("../../ref_ground/packs".as_ref(), &entry).expect("pack loads");
    eprintln!("after load: {} rows", rows.len());
    for row in &rows {
        eprintln!("{} {:?}", row.case_key, row.alg);
    }
}
