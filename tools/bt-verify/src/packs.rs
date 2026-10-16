use crate::ev_bigcube;
use crate::ev_bld;
use crate::ev_fto;
use crate::ev_mega;
use crate::ev_nxn;
use crate::ev_pyra;
use crate::ev_skewb;
use crate::ev_sq1;
use bt_data::seedpack::{NSpec, PackEntry, SeedRow, load_registry};
use std::path::Path;

pub enum Puzzle {
    Core,
    Nxn(usize),
    Sq1,
    Pyra,
    Skewb,
    Mega,
    Fto,
}

impl Puzzle {
    pub fn for_event(event: &str) -> Result<Puzzle, String> {
        match event {
            "3x3" | "oh" | "3bld" => Ok(Puzzle::Core),
            "2x2" => Ok(Puzzle::Nxn(2)),
            "sq1" => Ok(Puzzle::Sq1),
            "pyraminx" => Ok(Puzzle::Pyra),
            "skewb" => Ok(Puzzle::Skewb),
            "megaminx" => Ok(Puzzle::Mega),
            "fto" => Ok(Puzzle::Fto),
            "bigcube" => Err("bigcube rows resolve their own n".to_string()),
            other => Err(format!("unknown event {other}")),
        }
    }
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub enum Tier2Outcome {
    Pass,
    Fail(String),
    Skipped(&'static str),
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct RowOutcome {
    pub case_key: String,
    pub tier1: Result<(), String>,
    pub tier2: Tier2Outcome,
}

#[derive(Clone, PartialEq, Eq, Debug, Default)]
pub struct SectionResult {
    pub rows: Vec<RowOutcome>,
    pub honesty: Vec<String>,
}

impl SectionResult {
    fn unimplemented(event: &str, method: &str, stage: &str, rows: &[SeedRow]) -> SectionResult {
        SectionResult {
            rows: rows
                .iter()
                .map(|row| RowOutcome {
                    case_key: row.case_key.clone(),
                    tier1: Err(format!(
                        "section {event}/{method}/{stage} is not implemented"
                    )),
                    tier2: Tier2Outcome::Skipped("section not implemented"),
                })
                .collect(),
            honesty: Vec::new(),
        }
    }
}

pub fn verify_rows(entry: &PackEntry, rows: &[SeedRow]) -> SectionResult {
    match (
        entry.event.as_str(),
        entry.method.as_str(),
        entry.stage.as_str(),
    ) {
        ("2x2", "cll", "cll") => ev_nxn::verify_twoby2(rows),
        ("sq1", "lbl", "cubeshape") => ev_sq1::verify_cubeshape(rows),
        ("sq1", "lbl", "co") => ev_sq1::verify_co(rows),
        ("sq1", "lbl", "eo") => ev_sq1::verify_eo(rows),
        ("sq1", "lbl", "cp") => ev_sq1::verify_cp(rows),
        ("sq1", "lbl", "ep") => ev_sq1::verify_ep(rows),
        ("skewb", "sarah_intermediate", "sarah") => ev_skewb::verify_sarah(rows),
        ("pyraminx", "l4e", "l4e") => ev_pyra::verify_l4e(rows),
        ("megaminx", "4lll", "4lll") => ev_mega::verify_4lll(rows),
        ("fto", "l4t_redux", "l4t") => ev_fto::verify_l4t(rows),
        ("bigcube", "yau", "parity") | ("bigcube", "reduction", "parity") => {
            ev_bigcube::verify_parity(entry, rows)
        }
        ("3bld", "old_pochmann", "concept") => ev_bld::verify_oldpoch(rows),
        ("3bld", "m2", "concept") => ev_bld::verify_m2(rows),
        ("3bld", "3style", "parity") => ev_bld::verify_3style(rows),
        (event, method, stage) => SectionResult::unimplemented(event, method, stage, rows),
    }
}

pub struct PackPin {
    pub file: &'static str,
    pub event: &'static str,
    pub method: &'static str,
    pub stage: &'static str,
    pub rows: usize,
}

const fn pin(
    file: &'static str,
    event: &'static str,
    method: &'static str,
    stage: &'static str,
    rows: usize,
) -> PackPin {
    PackPin {
        file,
        event,
        method,
        stage,
        rows,
    }
}

pub const PACK_EXPECTED_ROWS: [PackPin; 21] = [
    pin("twoby2_cll.tsv", "2x2", "cll", "cll", 42),
    pin(
        "skewb_sarah.tsv",
        "skewb",
        "sarah_intermediate",
        "sarah",
        15,
    ),
    pin("pyraminx_l4e.tsv", "pyraminx", "l4e", "l4e", 35),
    pin("megaminx_4lll.tsv", "megaminx", "4lll", "4lll", 39),
    pin("sq1_cubeshape.tsv", "sq1", "lbl", "cubeshape", 169),
    pin("sq1_co.tsv", "sq1", "lbl", "co", 7),
    pin("sq1_eo.tsv", "sq1", "lbl", "eo", 7),
    pin("sq1_cp.tsv", "sq1", "lbl", "cp", 16),
    pin("sq1_ep.tsv", "sq1", "lbl", "ep", 99),
    pin("bigcube_parity.tsv", "bigcube", "yau", "parity", 5),
    pin("bigcube_l2e.tsv", "bigcube", "reduction", "parity", 12),
    pin("fto_l4t_redux.tsv", "fto", "l4t_redux", "l4t", 36),
    pin("bld_3style_specials.tsv", "3bld", "3style", "parity", 85),
    pin("bld_oldpoch.tsv", "3bld", "old_pochmann", "concept", 40),
    pin("bld_m2.tsv", "3bld", "m2", "concept", 24),
    pin("cfop_pll.tsv", "3x3", "cfop", "pll", 21),
    pin("cfop_oll.tsv", "3x3", "cfop", "oll", 57),
    pin("cfop_f2l.tsv", "3x3", "cfop", "f2l", 164),
    pin("cfop_pll.tsv", "oh", "cfop", "pll", 21),
    pin("cfop_oll.tsv", "oh", "cfop", "oll", 57),
    pin("cfop_f2l.tsv", "oh", "cfop", "f2l", 164),
];

pub fn nspec_text(n: &NSpec) -> String {
    match n {
        NSpec::Unset => "unset".to_string(),
        NSpec::Fixed(size) => format!("n={size}"),
        NSpec::Sizes(sizes) => {
            let joined = sizes
                .iter()
                .map(|size| size.to_string())
                .collect::<Vec<_>>()
                .join(",");
            format!("n={joined}")
        }
    }
}

pub fn pinned_vs_entries(entries: &[PackEntry]) -> Vec<String> {
    let mut offenders = Vec::new();
    for entry in entries {
        let Some(pinned) = PACK_EXPECTED_ROWS.iter().find(|pin| {
            pin.file == entry.file
                && pin.event == entry.event
                && pin.method == entry.method
                && pin.stage == entry.stage
        }) else {
            offenders.push(format!(
                "registry entry {}/{}/{}/{} ({}) is not pinned",
                entry.event,
                entry.method,
                entry.stage,
                entry.file,
                nspec_text(&entry.n)
            ));
            continue;
        };
        if entry.expected_rows != pinned.rows {
            offenders.push(format!(
                "{}/{}/{}/{} registry holds {} rows, pin holds {}",
                entry.event,
                entry.method,
                entry.stage,
                entry.file,
                entry.expected_rows,
                pinned.rows
            ));
        }
    }
    for pinned in PACK_EXPECTED_ROWS.iter() {
        if !entries.iter().any(|entry| {
            entry.file == pinned.file
                && entry.event == pinned.event
                && entry.method == pinned.method
                && entry.stage == pinned.stage
        }) {
            offenders.push(format!(
                "pin {}/{}/{}/{} is missing from the registry",
                pinned.event, pinned.method, pinned.stage, pinned.file
            ));
        }
    }
    offenders
}

pub fn pinned_vs_registry(packs_root: &Path) -> Vec<String> {
    match load_registry(packs_root) {
        Ok(entries) => pinned_vs_entries(&entries),
        Err(err) => vec![err.to_string()],
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use bt_data::seedpack::parse_registry;

    fn mini_registry(rows: &str) -> Vec<PackEntry> {
        parse_registry(rows).expect("registry parses")
    }

    #[test]
    fn a_matching_entry_and_pin_pair_produce_no_offenders() {
        let entries = mini_registry(
            r#"[{"file":"twoby2_cll.tsv","event":"2x2","method":"cll","stage":"cll","n":null,"expected_rows":42}]"#,
        );
        assert!(
            !pinned_vs_entries(&entries)
                .iter()
                .any(|offender| offender.contains("twoby2_cll"))
        );
    }

    #[test]
    fn row_drift_between_registry_and_pin_is_an_offender() {
        let entries = mini_registry(
            r#"[{"file":"twoby2_cll.tsv","event":"2x2","method":"cll","stage":"cll","n":null,"expected_rows":41}]"#,
        );
        assert!(pinned_vs_entries(&entries).contains(
            &"2x2/cll/cll/twoby2_cll.tsv registry holds 41 rows, pin holds 42".to_string()
        ));
    }

    #[test]
    fn an_unpinned_registry_entry_is_an_offender() {
        let entries = mini_registry(
            r#"[{"file":"new_pack.tsv","event":"fto","method":"l4t","stage":"l4t","n":null,"expected_rows":10}]"#,
        );
        assert!(
            pinned_vs_entries(&entries).iter().any(
                |offender| offender.contains("new_pack.tsv") && offender.contains("not pinned")
            )
        );
    }

    #[test]
    fn a_pin_without_a_registry_entry_is_an_offender() {
        assert!(
            pinned_vs_entries(&[])
                .iter()
                .any(|offender| offender.contains("missing from the registry"))
        );
    }

    #[test]
    fn nspec_text_renders_all_three_shapes() {
        assert_eq!(nspec_text(&NSpec::Unset), "unset");
        assert_eq!(nspec_text(&NSpec::Fixed(3)), "n=3");
        assert_eq!(nspec_text(&NSpec::Sizes(vec![4, 5])), "n=4,5");
    }
}
