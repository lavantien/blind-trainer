use bt_data::seedpack::{NSpec, PackEntry, load_pack};

fn main() {
    let entry = PackEntry {
        file: "skewb_sarah.tsv".to_string(),
        event: "skewb".to_string(),
        method: "sarah_intermediate".to_string(),
        stage: "sarah".to_string(),
        n: NSpec::Unset,
        expected_rows: 15,
    };
    let rows = load_pack("../../ref_ground/packs".as_ref(), &entry).expect("pack loads");
    for row in &rows {
        println!("{}: [{}]", row.case_key, row.alg);
    }
}
