use serde_json::Value;
use std::collections::BTreeMap;
use std::fmt;

pub const EMPTY_SENTINEL: &str = "(empty)";

#[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd)]
pub struct GateKey {
    pub file: String,
    pub function: String,
    pub genre: String,
    pub replacement: String,
}

impl fmt::Display for GateKey {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "file={} function={} genre={} replacement={}",
            self.file, self.function, self.genre, self.replacement
        )
    }
}

#[derive(Debug)]
pub enum GateError {
    Usage(String),
    Violation(String),
}

impl fmt::Display for GateError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            GateError::Usage(message) | GateError::Violation(message) => write!(f, "{message}"),
        }
    }
}

pub fn gate(outcomes_json: &str, allowlist_text: &str) -> Result<String, GateError> {
    let survivors = survivor_multiset(outcomes_json)?;
    let allowlist = parse_allowlist(allowlist_text)?;
    let mut problems = Vec::new();
    for (key, seen) in &survivors {
        match allowlist.get(key) {
            None => problems.push(format!("surviving mutant not in allowlist: {key}")),
            Some(allowed) if allowed != seen => problems.push(format!(
                "count mismatch: {key} outcomes={seen} allowlist={allowed}"
            )),
            _ => {}
        }
    }
    for key in allowlist.keys() {
        if !survivors.contains_key(key) {
            problems.push(format!(
                "stale allowlist entry with no surviving outcome: {key}"
            ));
        }
    }
    if !problems.is_empty() {
        return Err(GateError::Violation(problems.join("\n")));
    }
    let gated: u64 = survivors.values().sum();
    Ok(format!("survivors gated: {gated}, all covered"))
}

fn survivor_multiset(json: &str) -> Result<BTreeMap<GateKey, u64>, GateError> {
    let root: Value = serde_json::from_str(json)
        .map_err(|e| GateError::Usage(format!("invalid outcomes JSON: {e}")))?;
    let outcomes = root
        .get("outcomes")
        .and_then(Value::as_array)
        .ok_or_else(|| malformed_file("missing outcomes array"))?;
    let missed_counter = uint_counter(&root, "missed")?;
    let timeout_counter = uint_counter(&root, "timeout")?;
    let caught_counter = uint_counter(&root, "caught")?;
    let unviable_counter = uint_counter(&root, "unviable")?;
    let total_counter = uint_counter(&root, "total_mutants")?;
    let mut survivors = BTreeMap::new();
    let mut missed_seen = 0u64;
    let mut timeout_seen = 0u64;
    let mut caught_seen = 0u64;
    let mut unviable_seen = 0u64;
    let mut mutant_seen = 0u64;
    for outcome in outcomes {
        let scenario = outcome
            .get("scenario")
            .ok_or_else(|| malformed_outcome("missing scenario"))?;
        let summary = outcome
            .get("summary")
            .and_then(Value::as_str)
            .ok_or_else(|| malformed_outcome("missing summary string"))?;
        if scenario.as_str() == Some("Baseline") {
            if summary != "Success" {
                return Err(GateError::Violation(format!(
                    "baseline summary is '{summary}', not Success"
                )));
            }
            continue;
        }
        if let Some(label) = scenario.as_str() {
            return Err(malformed_outcome(format!(
                "scenario string '{label}' is not Baseline"
            )));
        }
        let mutant = scenario
            .get("Mutant")
            .and_then(Value::as_object)
            .ok_or_else(|| malformed_outcome("scenario object without Mutant"))?;
        let key = parse_key(mutant)?;
        mutant_seen += 1;
        match summary {
            "CaughtMutant" => caught_seen += 1,
            "Unviable" => unviable_seen += 1,
            "MissedMutant" => {
                *survivors.entry(key).or_insert(0) += 1;
                missed_seen += 1;
            }
            "Timeout" => {
                *survivors.entry(key).or_insert(0) += 1;
                timeout_seen += 1;
            }
            other => {
                return Err(GateError::Violation(format!(
                    "unknown summary '{other}' for {key} (allowed: CaughtMutant, MissedMutant, Timeout, Unviable)"
                )));
            }
        }
    }
    if mutant_seen == 0 {
        return Err(GateError::Violation(
            "no mutant outcomes present, refusing an empty run".to_string(),
        ));
    }
    if missed_seen != missed_counter {
        return Err(GateError::Violation(format!(
            "missed counter mismatch: outcomes count {missed_seen} != file counter {missed_counter}"
        )));
    }
    if timeout_seen != timeout_counter {
        return Err(GateError::Violation(format!(
            "timeout counter mismatch: outcomes count {timeout_seen} != file counter {timeout_counter}"
        )));
    }
    if caught_seen != caught_counter {
        return Err(GateError::Violation(format!(
            "caught counter mismatch: outcomes count {caught_seen} != file counter {caught_counter}"
        )));
    }
    if unviable_seen != unviable_counter {
        return Err(GateError::Violation(format!(
            "unviable counter mismatch: outcomes count {unviable_seen} != file counter {unviable_counter}"
        )));
    }
    if mutant_seen != total_counter {
        return Err(GateError::Violation(format!(
            "total_mutants counter mismatch: outcomes count {mutant_seen} != file counter {total_counter}"
        )));
    }
    Ok(survivors)
}

fn parse_key(mutant: &serde_json::Map<String, Value>) -> Result<GateKey, GateError> {
    let file = string_field(mutant, "file")?;
    let function_name = match mutant.get("function") {
        Some(Value::Object(function)) => string_field(function, "function_name")?,
        Some(Value::Null) => String::new(),
        _ => return Err(malformed_outcome("mutant without function object")),
    };
    let genre = string_field(mutant, "genre")?;
    let replacement = string_field(mutant, "replacement")?;
    Ok(GateKey {
        file,
        function: function_name,
        genre,
        replacement,
    })
}

fn string_field(map: &serde_json::Map<String, Value>, name: &str) -> Result<String, GateError> {
    map.get(name)
        .and_then(Value::as_str)
        .map(str::to_owned)
        .ok_or_else(|| malformed_outcome(format!("missing string field '{name}'")))
}

fn uint_counter(root: &Value, name: &str) -> Result<u64, GateError> {
    root.get(name)
        .and_then(Value::as_u64)
        .ok_or_else(|| malformed_file(format!("missing integer counter '{name}'")))
}

fn parse_allowlist(text: &str) -> Result<BTreeMap<GateKey, u64>, GateError> {
    let mut entries = BTreeMap::new();
    for (idx, line) in text.lines().enumerate() {
        let row = idx + 1;
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let cols: Vec<&str> = line.split('\t').collect();
        if cols.len() != 6 {
            return Err(row_error(
                row,
                format!("expected 6 tab-separated columns, found {}", cols.len()),
            ));
        }
        if cols[0].is_empty() || cols[2].is_empty() {
            return Err(row_error(row, "file and genre must be non-empty"));
        }
        if cols[1].is_empty() {
            return Err(row_error(
                row,
                "function must be non-empty, write (empty) for a module-level mutant",
            ));
        }
        let function = if cols[1] == EMPTY_SENTINEL {
            String::new()
        } else {
            cols[1].to_owned()
        };
        let replacement = if cols[3] == EMPTY_SENTINEL {
            String::new()
        } else if cols[3].contains("->") {
            cols[3]
                .rsplit("->")
                .next()
                .filter(|token| !token.is_empty())
                .ok_or_else(|| {
                    row_error(row, "replacement must have a non-empty token after '->'")
                })?
                .to_owned()
        } else if cols[3].is_empty() {
            return Err(row_error(
                row,
                "replacement must be non-empty, write (empty) for a deleted token",
            ));
        } else {
            cols[3].to_owned()
        };
        let count: u64 = cols[4]
            .trim()
            .parse()
            .map_err(|_| row_error(row, format!("count '{}' is not an integer", cols[4])))?;
        if count < 1 {
            return Err(row_error(row, "count must be >= 1"));
        }
        if cols[5].is_empty() {
            return Err(row_error(row, "justification must be non-empty"));
        }
        let key = GateKey {
            file: cols[0].to_owned(),
            function,
            genre: cols[2].to_owned(),
            replacement,
        };
        if entries.contains_key(&key) {
            return Err(row_error(row, format!("duplicate entry for {key}")));
        }
        entries.insert(key, count);
    }
    Ok(entries)
}

fn row_error(row: usize, detail: impl Into<String>) -> GateError {
    GateError::Violation(format!("allowlist row {row}: {}", detail.into()))
}

#[derive(Debug)]
pub struct MergedOutcome {
    pub json: String,
    pub total_mutants: u64,
    pub inputs: usize,
}

pub fn merge_outcomes(inputs: &[&str]) -> Result<MergedOutcome, GateError> {
    if inputs.len() < 2 {
        return Err(GateError::Usage(
            "merge needs at least two input files".to_owned(),
        ));
    }
    let mut roots = Vec::with_capacity(inputs.len());
    for (idx, text) in inputs.iter().enumerate() {
        let root: Value = serde_json::from_str(text).map_err(|e| {
            GateError::Usage(format!("invalid outcomes JSON in input {}: {e}", idx + 1))
        })?;
        check_complete_run(&root, idx + 1)?;
        roots.push(root);
    }
    let versions: Vec<&str> = roots
        .iter()
        .filter_map(|root| root.get("cargo_mutants_version").and_then(Value::as_str))
        .collect();
    if let Some(first) = versions.first()
        && versions.iter().any(|version| version != first)
    {
        return Err(GateError::Violation(format!(
            "cargo_mutants_version disagreement across inputs: {versions:?}"
        )));
    }
    let sums = [
        "total_mutants",
        "caught",
        "missed",
        "timeout",
        "unviable",
        "success",
    ];
    let mut merged = serde_json::Map::new();
    for name in sums {
        let total: u64 = roots
            .iter()
            .map(|root| {
                root.get(name)
                    .and_then(Value::as_u64)
                    .ok_or_else(|| malformed_file(format!("missing integer counter '{name}'")))
            })
            .collect::<Result<Vec<u64>, GateError>>()?
            .into_iter()
            .sum();
        merged.insert(name.to_owned(), Value::from(total));
    }
    if let Some(first) = versions.first() {
        merged.insert(
            "cargo_mutants_version".to_owned(),
            Value::String((*first).to_owned()),
        );
    }
    if let Some(earliest) = roots
        .iter()
        .filter_map(|root| root.get("start_time").and_then(Value::as_str))
        .min()
    {
        merged.insert("start_time".to_owned(), Value::String(earliest.to_owned()));
    }
    if let Some(latest) = roots
        .iter()
        .filter_map(|root| root.get("end_time").and_then(Value::as_str))
        .max()
    {
        merged.insert("end_time".to_owned(), Value::String(latest.to_owned()));
    }
    let mut outcomes = Vec::new();
    for root in &roots {
        outcomes.extend(
            root.get("outcomes")
                .and_then(Value::as_array)
                .ok_or_else(|| malformed_file("missing outcomes array"))?
                .iter()
                .cloned(),
        );
    }
    merged.insert("outcomes".to_owned(), Value::Array(outcomes));
    let total_mutants = merged
        .get("total_mutants")
        .and_then(Value::as_u64)
        .unwrap_or(0);
    let json = serde_json::to_string_pretty(&Value::Object(merged))
        .map_err(|e| GateError::Usage(format!("cannot serialize merged outcomes: {e}")))?;
    Ok(MergedOutcome {
        json,
        total_mutants,
        inputs: inputs.len(),
    })
}

fn check_complete_run(root: &Value, input: usize) -> Result<(), GateError> {
    let end_time = root
        .get("end_time")
        .and_then(Value::as_str)
        .ok_or_else(|| {
            GateError::Violation(format!(
                "input {input} has no end_time string, refusing a killed or partial run"
            ))
        })?;
    if end_time.is_empty() {
        return Err(GateError::Violation(format!(
            "input {input} has an empty end_time, refusing a killed or partial run"
        )));
    }
    let outcomes = root
        .get("outcomes")
        .and_then(Value::as_array)
        .ok_or_else(|| malformed_file(format!("input {input} is missing the outcomes array")))?;
    let mut counts = [
        ("CaughtMutant", "caught", 0u64),
        ("Unviable", "unviable", 0u64),
        ("MissedMutant", "missed", 0u64),
        ("Timeout", "timeout", 0u64),
    ];
    let mut mutants = 0u64;
    for outcome in outcomes {
        let summary = outcome
            .get("summary")
            .and_then(Value::as_str)
            .ok_or_else(|| malformed_outcome("missing summary string"))?;
        if outcome.get("scenario").and_then(Value::as_str) == Some("Baseline") {
            if summary != "Success" {
                return Err(GateError::Violation(format!(
                    "input {input} baseline summary is '{summary}', not Success"
                )));
            }
            continue;
        }
        if !outcome
            .get("scenario")
            .and_then(Value::as_object)
            .is_some_and(|scenario| scenario.contains_key("Mutant"))
        {
            return Err(malformed_outcome("scenario object without Mutant"));
        }
        mutants += 1;
        match counts
            .iter_mut()
            .find(|(summary_name, _, _)| *summary_name == summary)
        {
            Some((_, _, seen)) => *seen += 1,
            None => {
                return Err(GateError::Violation(format!(
                    "unknown summary '{summary}' in input {input}"
                )));
            }
        }
    }
    if mutants == 0 {
        return Err(GateError::Violation(format!(
            "input {input} has no mutant outcomes, refusing an empty run"
        )));
    }
    for (summary_name, counter, seen) in counts {
        let declared = root
            .get(counter)
            .and_then(Value::as_u64)
            .ok_or_else(|| malformed_file(format!("missing integer counter '{counter}'")))?;
        if seen != declared {
            return Err(GateError::Violation(format!(
                "input {input} counter mismatch for {summary_name}: outcomes count {seen} != file counter {declared}"
            )));
        }
    }
    let declared_total = root
        .get("total_mutants")
        .and_then(Value::as_u64)
        .ok_or_else(|| malformed_file("missing integer counter 'total_mutants'"))?;
    if mutants != declared_total {
        return Err(GateError::Violation(format!(
            "input {input} total_mutants mismatch: outcomes count {mutants} != file counter {declared_total}"
        )));
    }
    Ok(())
}

fn malformed_outcome(detail: impl Into<String>) -> GateError {
    GateError::Violation(format!("malformed outcome: {}", detail.into()))
}

fn malformed_file(detail: impl Into<String>) -> GateError {
    GateError::Violation(format!("malformed outcomes file: {}", detail.into()))
}

#[cfg(test)]
mod tests;
