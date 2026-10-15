use serde::{Deserialize, Serialize};

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct VendorAlg {
    pub solver: String,
    pub rowid: usize,
    pub position: String,
    pub alg: String,
    pub qtm: u32,
    pub htm: u32,
    pub ok: bool,
    pub problems: Vec<String>,
    pub notes: Vec<String>,
    pub commutator: String,
    pub recon_link: String,
    pub youtube_link: String,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct CaseEntry {
    pub orbit: String,
    pub t1: u8,
    pub t2: u8,
    pub t1_letter: char,
    pub t2_letter: char,
    pub t1_name: String,
    pub t2_name: String,
    pub covered: bool,
    pub vendor_algs: Vec<VendorAlg>,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct SpecialEntry {
    pub class: String,
    pub solver: String,
    pub rowid: usize,
    pub position: String,
    pub alg: String,
    pub qtm: u32,
    pub htm: u32,
    pub ok: bool,
    pub problems: Vec<String>,
    pub notes: Vec<String>,
    pub commutator: String,
    pub recon_link: String,
    pub youtube_link: String,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct CasesFile {
    pub cases: Vec<CaseEntry>,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct SpecialsFile {
    pub specials: Vec<SpecialEntry>,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct CorpusFileEntry {
    pub solver: String,
    pub sheet: String,
    pub rows: usize,
    pub sha256: String,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct OrbitCoverage {
    pub total: usize,
    pub covered: usize,
    pub uncovered: usize,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct ClassCount {
    pub solver: String,
    pub class: String,
    pub rows: usize,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct CasesInfo {
    pub path: String,
    pub sha256: String,
    pub entries: usize,
    pub edge: OrbitCoverage,
    pub corner: OrbitCoverage,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct SpecialsInfo {
    pub path: String,
    pub sha256: String,
    pub entries: usize,
    pub counts: Vec<ClassCount>,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct Manifest {
    pub generated_at: String,
    pub generator: String,
    pub synth_params_sha256: String,
    pub corpus_sha256: String,
    pub corpus_rows: usize,
    pub corpus_files: Vec<CorpusFileEntry>,
    pub rows_in_pair_space: usize,
    pub rows_other_buffer: usize,
    pub cases: CasesInfo,
    pub specials: SpecialsInfo,
}

#[cfg(test)]
mod tests {
    use super::*;

    fn vendor_alg(solver: &str, rowid: usize) -> VendorAlg {
        VendorAlg {
            solver: solver.to_string(),
            rowid,
            position: "UF-UB-UR".to_string(),
            alg: "R2 U' S R2 S' R2 U R2".to_string(),
            qtm: 12,
            htm: 9,
            ok: true,
            problems: vec!["centers moved".to_string()],
            notes: vec!["flip:UF".to_string()],
            commutator: "R2 U':[S,R2]".to_string(),
            recon_link: "https://re.con".to_string(),
            youtube_link: "https://youtu.be/x".to_string(),
        }
    }

    fn special_entry(solver: &str, class: &str, rowid: usize) -> SpecialEntry {
        SpecialEntry {
            class: class.to_string(),
            solver: solver.to_string(),
            rowid,
            position: "UF & UB".to_string(),
            alg: "M2 U M2 U2 M2 U M2".to_string(),
            qtm: 14,
            htm: 7,
            ok: false,
            problems: Vec::new(),
            notes: Vec::new(),
            commutator: String::new(),
            recon_link: String::new(),
            youtube_link: String::new(),
        }
    }

    #[test]
    fn vendor_alg_round_trips_and_keeps_its_field_names() {
        let alg = vendor_alg("cherry", 4);
        let value = serde_json::to_value(&alg).unwrap();
        let keys: Vec<&str> = value
            .as_object()
            .unwrap()
            .keys()
            .map(|key| key.as_str())
            .collect();
        assert_eq!(
            keys,
            vec![
                "alg",
                "commutator",
                "htm",
                "notes",
                "ok",
                "position",
                "problems",
                "qtm",
                "recon_link",
                "rowid",
                "solver",
                "youtube_link",
            ]
        );
        let back: VendorAlg = serde_json::from_value(value).unwrap();
        assert_eq!(back, alg);
    }

    #[test]
    fn case_entry_and_files_round_trip() {
        let entry = CaseEntry {
            orbit: "edge".to_string(),
            t1: 24,
            t2: 25,
            t1_letter: 'A',
            t2_letter: 'B',
            t1_name: "UB".to_string(),
            t2_name: "UR".to_string(),
            covered: true,
            vendor_algs: vec![vendor_alg("cherry", 2)],
        };
        let file = CasesFile { cases: vec![entry] };
        let back: CasesFile = serde_json::from_str(&serde_json::to_string(&file).unwrap()).unwrap();
        assert_eq!(back, file);
    }

    #[test]
    fn specials_file_round_trips() {
        let file = SpecialsFile {
            specials: vec![special_entry("eggins", "twists", 3)],
        };
        let back: SpecialsFile =
            serde_json::from_str(&serde_json::to_string(&file).unwrap()).unwrap();
        assert_eq!(back, file);
    }

    fn manifest_fixture() -> Manifest {
        Manifest {
            generated_at: "2026-10-14T00:00:00Z".to_string(),
            generator: "bt-gendata".to_string(),
            synth_params_sha256: "34".repeat(32),
            corpus_sha256: "ab".repeat(32),
            corpus_rows: 1077,
            corpus_files: vec![CorpusFileEntry {
                solver: "cherry".to_string(),
                sheet: "edge".to_string(),
                rows: 551,
                sha256: "cd".repeat(32),
            }],
            rows_in_pair_space: 928,
            rows_other_buffer: 60,
            cases: CasesInfo {
                path: "cases.json".to_string(),
                sha256: "ef".repeat(32),
                entries: 818,
                edge: OrbitCoverage {
                    total: 440,
                    covered: 372,
                    uncovered: 68,
                },
                corner: OrbitCoverage {
                    total: 378,
                    covered: 288,
                    uncovered: 90,
                },
            },
            specials: SpecialsInfo {
                path: "specials.json".to_string(),
                sha256: "12".repeat(32),
                entries: 89,
                counts: vec![
                    ClassCount {
                        solver: "cherry".to_string(),
                        class: "flips".to_string(),
                        rows: 25,
                    },
                    ClassCount {
                        solver: "eggins".to_string(),
                        class: "flips".to_string(),
                        rows: 0,
                    },
                ],
            },
        }
    }

    #[test]
    fn manifest_round_trips_with_coverage_and_counts() {
        let manifest = manifest_fixture();
        let back: Manifest =
            serde_json::from_str(&serde_json::to_string(&manifest).unwrap()).unwrap();
        assert_eq!(back, manifest);
    }

    #[test]
    fn manifest_without_synth_params_hash_is_rejected() {
        let value = serde_json::to_value(&manifest_fixture()).unwrap();
        let stripped = {
            let mut object = value.as_object().unwrap().clone();
            object.remove("synth_params_sha256");
            serde_json::Value::Object(object)
        };
        let err = serde_json::from_value::<Manifest>(stripped).unwrap_err();
        assert!(err.to_string().contains("synth_params_sha256"), "{err}");
    }
}
