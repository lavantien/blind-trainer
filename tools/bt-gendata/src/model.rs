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
    pub corpus_sha256: String,
    pub corpus_rows: usize,
    pub corpus_files: Vec<CorpusFileEntry>,
    pub rows_in_pair_space: usize,
    pub rows_other_buffer: usize,
    pub cases: CasesInfo,
    pub specials: SpecialsInfo,
}
