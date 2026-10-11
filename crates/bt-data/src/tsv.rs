use std::fmt;

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct SheetRow {
    pub position: String,
    pub alg: String,
    pub commutator: String,
    pub recon_link: String,
    pub youtube_link: String,
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub enum TsvError {
    BadHeader { line: usize },
    ColumnCount { line: usize, found: usize },
    EmptyField { line: usize, column: &'static str },
}

impl fmt::Display for TsvError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            TsvError::BadHeader { line } => {
                write!(
                    f,
                    "line {line}: header must be Position/Alg/Commutator/Reconstruction Link/Youtube Link"
                )
            }
            TsvError::ColumnCount { line, found } => {
                write!(f, "line {line}: expected 5 columns, found {found}")
            }
            TsvError::EmptyField { line, column } => {
                write!(f, "line {line}: {column} column is empty")
            }
        }
    }
}

impl std::error::Error for TsvError {}

const HEADER_CELLS: [&str; 5] = [
    "Position",
    "Alg",
    "Commutator",
    "Reconstruction Link",
    "Youtube Link",
];

pub fn parse_sheet(text: &str) -> Result<Vec<(usize, SheetRow)>, TsvError> {
    let mut rows = Vec::new();
    let mut saw_header = false;
    for (index, raw_line) in text.split('\n').enumerate() {
        let line = raw_line.strip_suffix('\r').unwrap_or(raw_line);
        if line.trim().is_empty() {
            continue;
        }
        let line_number = index + 1;
        if !saw_header {
            validate_header(line, line_number)?;
            saw_header = true;
            continue;
        }
        let mut cells = line.split('\t').collect::<Vec<&str>>();
        while cells.len() > HEADER_CELLS.len() && cells.last().is_some_and(|c| c.trim().is_empty())
        {
            cells.pop();
        }
        if cells.len() != HEADER_CELLS.len() {
            return Err(TsvError::ColumnCount {
                line: line_number,
                found: cells.len(),
            });
        }
        for (column, cell) in HEADER_CELLS.iter().zip(cells.iter()) {
            if matches!(*column, "Position" | "Alg") && cell.trim().is_empty() {
                return Err(TsvError::EmptyField {
                    line: line_number,
                    column: match *column {
                        "Position" => "Position",
                        _ => "Alg",
                    },
                });
            }
        }
        rows.push((
            line_number,
            SheetRow {
                position: cells[0].to_string(),
                alg: cells[1].to_string(),
                commutator: cells[2].to_string(),
                recon_link: cells[3].to_string(),
                youtube_link: cells[4].to_string(),
            },
        ));
    }
    Ok(rows)
}

fn validate_header(line: &str, line_number: usize) -> Result<(), TsvError> {
    let mut cells = line.split('\t').collect::<Vec<&str>>();
    if !cells[0].trim().starts_with("Position") {
        return Err(TsvError::BadHeader { line: line_number });
    }
    while cells.len() > HEADER_CELLS.len() && cells.last().is_some_and(|c| c.trim().is_empty()) {
        cells.pop();
    }
    if cells.len() < HEADER_CELLS.len() {
        return Err(TsvError::ColumnCount {
            line: line_number,
            found: cells.len(),
        });
    }
    let trimmed: Vec<&str> = cells.iter().map(|c| c.trim()).collect();
    if (1..HEADER_CELLS.len()).any(|i| trimmed[i] != HEADER_CELLS[i]) {
        return Err(TsvError::BadHeader { line: line_number });
    }
    Ok(())
}
