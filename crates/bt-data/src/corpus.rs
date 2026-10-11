use crate::tsv::parse_sheet;
use bt_core::Alg;
use std::collections::HashMap;
use std::fmt;
use std::path::Path;

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum Solver {
    Cherry,
    Eggins,
    User,
}

impl Solver {
    pub const VENDOR: [Solver; 2] = [Solver::Cherry, Solver::Eggins];

    pub fn dir(self) -> &'static str {
        match self {
            Solver::Cherry => "cherry",
            Solver::Eggins => "eggins",
            Solver::User => "user",
        }
    }

    pub fn parse(text: &str) -> Option<Solver> {
        match text {
            "cherry" => Some(Solver::Cherry),
            "eggins" => Some(Solver::Eggins),
            "user" => Some(Solver::User),
            _ => None,
        }
    }
}

impl fmt::Display for Solver {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.dir())
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub enum SheetClass {
    Edge,
    Corner,
    Flips,
    Twists,
    Parity,
    Ltct,
}

impl SheetClass {
    pub const ALL: [SheetClass; 6] = [
        SheetClass::Edge,
        SheetClass::Corner,
        SheetClass::Flips,
        SheetClass::Twists,
        SheetClass::Parity,
        SheetClass::Ltct,
    ];

    pub fn file(self) -> &'static str {
        match self {
            SheetClass::Edge => "edge",
            SheetClass::Corner => "corner",
            SheetClass::Flips => "flips",
            SheetClass::Twists => "twists",
            SheetClass::Parity => "parity",
            SheetClass::Ltct => "ltct",
        }
    }

    pub fn parse(text: &str) -> Option<SheetClass> {
        SheetClass::ALL
            .into_iter()
            .find(|sheet| sheet.file() == text)
    }
}

impl fmt::Display for SheetClass {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.file())
    }
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Row {
    pub solver: Solver,
    pub sheet: SheetClass,
    pub rowid: usize,
    pub position: String,
    pub alg: Alg,
    pub alg_text: String,
    pub commutator: String,
    pub recon_link: String,
    pub youtube_link: String,
}

impl Row {
    pub fn qtm(&self) -> u32 {
        self.alg.qtm()
    }

    pub fn htm(&self) -> u32 {
        self.alg.htm()
    }
}

#[derive(Debug)]
pub enum LoadError {
    Read {
        file: std::path::PathBuf,
        source: std::io::Error,
    },
    Parse {
        file: std::path::PathBuf,
        source: crate::tsv::TsvError,
    },
    Alg {
        file: std::path::PathBuf,
        rowid: usize,
        source: bt_core::ParseError,
    },
}

impl fmt::Display for LoadError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            LoadError::Read { file, source } => write!(f, "read {}: {}", file.display(), source),
            LoadError::Parse { file, source } => write!(f, "parse {}: {}", file.display(), source),
            LoadError::Alg {
                file,
                rowid,
                source,
            } => {
                write!(f, "alg in {} rowid {rowid}: {}", file.display(), source)
            }
        }
    }
}

impl std::error::Error for LoadError {}

pub struct Corpus {
    rows: Vec<Row>,
    by_key: HashMap<(Solver, SheetClass, usize), usize>,
    by_position: HashMap<(SheetClass, String), Vec<usize>>,
}

impl fmt::Debug for Corpus {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Corpus")
            .field("rows", &self.rows.len())
            .finish()
    }
}

impl Corpus {
    pub fn load(root: &Path) -> Result<Corpus, LoadError> {
        let mut rows = Vec::new();
        for solver in Solver::VENDOR {
            for sheet in SheetClass::ALL {
                let file = root
                    .join(solver.dir())
                    .join(format!("{}.tsv", sheet.file()));
                let text = std::fs::read_to_string(&file).map_err(|source| LoadError::Read {
                    file: file.clone(),
                    source,
                })?;
                for (rowid, parsed) in parse_sheet(&text).map_err(|source| LoadError::Parse {
                    file: file.clone(),
                    source,
                })? {
                    let alg = Alg::parse(&parsed.alg).map_err(|source| LoadError::Alg {
                        file: file.clone(),
                        rowid,
                        source,
                    })?;
                    rows.push(Row {
                        solver,
                        sheet,
                        rowid,
                        position: parsed.position,
                        alg_text: parsed.alg,
                        alg,
                        commutator: parsed.commutator,
                        recon_link: parsed.recon_link,
                        youtube_link: parsed.youtube_link,
                    });
                }
            }
        }
        Ok(Corpus::from_rows(rows))
    }

    pub fn from_rows(rows: Vec<Row>) -> Corpus {
        let mut by_key = HashMap::new();
        let mut by_position: HashMap<(SheetClass, String), Vec<usize>> = HashMap::new();
        for (index, row) in rows.iter().enumerate() {
            by_key.insert((row.solver, row.sheet, row.rowid), index);
            by_position
                .entry((row.sheet, row.position.clone()))
                .or_default()
                .push(index);
        }
        Corpus {
            rows,
            by_key,
            by_position,
        }
    }

    pub fn rows(&self) -> &[Row] {
        &self.rows
    }

    pub fn total_rows(&self) -> usize {
        self.rows.len()
    }

    pub fn is_empty(&self) -> bool {
        self.rows.is_empty()
    }

    pub fn row(&self, solver: Solver, sheet: SheetClass, rowid: usize) -> Option<&Row> {
        self.by_key
            .get(&(solver, sheet, rowid))
            .map(|&index| &self.rows[index])
    }

    pub fn sheet_rows(&self, solver: Solver, sheet: SheetClass) -> Vec<&Row> {
        self.rows
            .iter()
            .filter(|row| row.solver == solver && row.sheet == sheet)
            .collect()
    }

    pub fn rows_for_position(&self, sheet: SheetClass, position: &str) -> Vec<&Row> {
        self.by_position
            .get(&(sheet, position.to_string()))
            .map(|indices| indices.iter().map(|&index| &self.rows[index]).collect())
            .unwrap_or_default()
    }

    pub fn distinct_positions(&self, solver: Solver, sheet: SheetClass) -> usize {
        self.rows
            .iter()
            .filter(|row| row.solver == solver && row.sheet == sheet)
            .map(|row| row.position.as_str())
            .collect::<std::collections::HashSet<&str>>()
            .len()
    }
}
