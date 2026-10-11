use bt_data::paths::{load_config, workspace_root};
use bt_ga::dist::FreqData;
use bt_ga::ga::{GaEnv, GaParams, RunOutcome, build_env, config_ga_params, run};
use bt_ga::hash::sha256_hex;
use bt_ga::inventory::{
    self, CASE_COUNT, FREQS_FILE, InventoryEntry, InventoryReport, PoolEntry, VIETNAMESE_DIR,
    WORDLIST_FILE,
};
use bt_scheme::{Orbit, enumerate_cases};
use serde::{Deserialize, Serialize};
use std::collections::HashSet;
use std::path::{Path, PathBuf};

#[derive(Clone, Serialize, Deserialize, Debug)]
pub struct SyllableCase {
    pub orbit: String,
    pub letters: String,
    pub syllable: String,
    pub frequency: u64,
}

#[derive(Clone, Serialize, Deserialize, Debug)]
pub struct SyllableAssignment {
    pub cases: Vec<SyllableCase>,
    pub pool_size: usize,
    pub seed: u64,
    pub best_fitness: f32,
    pub freq_sha256: String,
    pub syllables_sha256: String,
}

#[derive(Clone, Serialize, Debug)]
pub struct GaReport {
    pub run_config: serde_json::Value,
    pub inventory: InventoryReport,
    pub pool_size: usize,
    pub pool_top: Vec<(String, u64)>,
    pub best_fitness: f32,
    pub best_parts: [f32; 3],
    pub best_generation: usize,
    pub generations_run: usize,
    pub per_gen_best: Vec<f32>,
    pub stagnation_tail: u32,
    pub max_stagnation: u32,
    pub evaluations: u64,
    pub freq_sha256: String,
    pub syllables_sha256: String,
}

pub struct RunInputs {
    pub params: GaParams,
    pub env: GaEnv,
    pub pool: Vec<PoolEntry>,
    pub inventory_report: InventoryReport,
    pub freq: FreqData,
    pub freq_bytes: Vec<u8>,
}

pub fn config_path(relative: &str) -> Result<PathBuf, String> {
    let path = PathBuf::from(relative);
    Ok(if path.is_absolute() {
        path
    } else {
        workspace_root().join(path)
    })
}

pub fn load_run_inputs() -> Result<RunInputs, String> {
    let config = load_config().map_err(|e| e.to_string())?;
    let params = config_ga_params(&config)?;
    let top_k = inventory::config_pool_top_k(&config)?;
    let ref_doc = config
        .pointer("/paths/ref_doc")
        .and_then(|v| v.as_str())
        .ok_or("config section missing: paths.ref_doc")?;
    let vietnamese = config_path(ref_doc)?.join(VIETNAMESE_DIR);
    let wordlist = inventory::load_wordlist(&vietnamese.join(WORDLIST_FILE))?;
    let freqs = inventory::load_syllable_frequencies(&vietnamese.join(FREQS_FILE))?;
    let (entries, inventory_report) = inventory::build_inventory(&wordlist);
    let pool = inventory::build_pool(entries.iter(), &freqs, top_k);
    if pool.len() < CASE_COUNT {
        return Err(format!("pool {} smaller than {CASE_COUNT}", pool.len()));
    }
    let freq_path = config
        .pointer("/paths/data_ga")
        .and_then(|v| v.as_str())
        .ok_or("config section missing: paths.data_ga")?;
    let freq_file = config_path(freq_path)?.join("freq.json");
    let freq_bytes =
        std::fs::read(&freq_file).map_err(|e| format!("read {}: {e}", freq_file.display()))?;
    let freq: FreqData =
        serde_json::from_slice(&freq_bytes).map_err(|e| format!("parse freq.json: {e}"))?;
    cross_check_cases(&config)?;
    let by_text: std::collections::HashMap<&str, &InventoryEntry> = entries
        .iter()
        .map(|entry| (entry.text.as_str(), entry))
        .collect();
    let feats: Vec<bt_ga::features::Features> = pool
        .iter()
        .map(|entry| {
            let source = by_text
                .get(entry.text.as_str())
                .ok_or_else(|| format!("pool entry {} missing from inventory", entry.text))?;
            Ok(bt_ga::features::Features {
                tone: source.tone,
                onset: bt_ga::features::onset_features(source.onset),
                nucleus: source.nucleus,
                coda: source.coda,
            })
        })
        .collect::<Result<_, String>>()?;
    let lengths: Vec<f32> = pool
        .iter()
        .map(|entry| entry.text.chars().count() as f32)
        .collect();
    let cooc_total: u64 = freq.cooccurring.iter().map(|p| p.count).sum();
    let adj_total: u64 = freq.adjacent.iter().map(|p| p.count).sum();
    let cooc = freq
        .cooccurring
        .iter()
        .map(|p| (p.from, p.to, p.count as f32 / cooc_total as f32))
        .collect();
    let adj = freq
        .adjacent
        .iter()
        .map(|p| (p.from, p.to, p.count as f32 / adj_total as f32))
        .collect();
    let env = build_env(pool.len(), lengths, feats, CASE_COUNT, cooc, adj)?;
    Ok(RunInputs {
        params,
        env,
        pool,
        inventory_report,
        freq,
        freq_bytes,
    })
}

fn cross_check_cases(config: &serde_json::Value) -> Result<(), String> {
    let vendor = config
        .pointer("/paths/data_vendor")
        .and_then(|v| v.as_str())
        .ok_or("config section missing: paths.data_vendor")?;
    let cases_file = config_path(vendor)?.join("cases.json");
    let bytes =
        std::fs::read(&cases_file).map_err(|e| format!("read {}: {e}", cases_file.display()))?;
    let parsed: serde_json::Value =
        serde_json::from_slice(&bytes).map_err(|e| format!("parse cases.json: {e}"))?;
    let recorded = parsed
        .pointer("/cases")
        .and_then(|v| v.as_array())
        .ok_or("cases.json missing cases array")?;
    let expected: Vec<(Orbit, u8, u8)> = [Orbit::Edge, Orbit::Corner]
        .iter()
        .flat_map(|&orbit| {
            enumerate_cases(orbit)
                .into_iter()
                .map(|case| (case.orbit, case.t1, case.t2))
        })
        .collect();
    if recorded.len() != expected.len() {
        return Err(format!(
            "cases.json has {} rows, scheme enumerates {}",
            recorded.len(),
            expected.len()
        ));
    }
    for (row, (orbit, t1, t2)) in recorded.iter().zip(expected.iter()) {
        let orbit_text = match orbit {
            Orbit::Edge => "edge",
            Orbit::Corner => "corner",
        };
        let got_orbit = row
            .pointer("/orbit")
            .and_then(|v| v.as_str())
            .ok_or("cases.json row missing orbit")?;
        let got_t1 = row
            .pointer("/t1")
            .and_then(|v| v.as_u64())
            .ok_or("cases.json row missing t1")?;
        let got_t2 = row
            .pointer("/t2")
            .and_then(|v| v.as_u64())
            .ok_or("cases.json row missing t2")?;
        let got_l1 = row
            .pointer("/t1_letter")
            .and_then(|v| v.as_str())
            .ok_or("cases.json row missing t1_letter")?;
        let got_l2 = row
            .pointer("/t2_letter")
            .and_then(|v| v.as_str())
            .ok_or("cases.json row missing t2_letter")?;
        if got_orbit != orbit_text || got_t1 != *t1 as u64 || got_t2 != *t2 as u64 {
            return Err(format!(
                "cases.json row mismatch: {got_orbit} {got_t1} {got_t2} vs {orbit_text} {t1} {t2}"
            ));
        }
        let letters =
            bt_scheme::letter_of(*t1).to_string() + &bt_scheme::letter_of(*t2).to_string();
        if got_l1.to_lowercase() + &got_l2.to_lowercase() != letters.to_lowercase() {
            return Err(format!(
                "cases.json letters {got_l1}{got_l2} vs scheme {letters}"
            ));
        }
    }
    Ok(())
}

pub fn build_assignment(
    inputs: &RunInputs,
    outcome: &RunOutcome,
) -> Result<SyllableAssignment, Vec<String>> {
    let mut offenders = Vec::new();
    if outcome.best.len() != CASE_COUNT {
        offenders.push(format!(
            "best chromosome has {} genes, expected {CASE_COUNT}",
            outcome.best.len()
        ));
    }
    let mut seen: HashSet<u32> = HashSet::new();
    for &gene in &outcome.best {
        if !seen.insert(gene) {
            offenders.push(format!("duplicate pool index {gene} in assignment"));
        }
        if gene as usize >= inputs.pool.len() {
            offenders.push(format!(
                "pool index {gene} outside pool {}",
                inputs.pool.len()
            ));
        }
    }
    if offenders.is_empty() {
        let edge = enumerate_cases(Orbit::Edge);
        let corner = enumerate_cases(Orbit::Corner);
        let cases: Vec<SyllableCase> = outcome
            .best
            .iter()
            .enumerate()
            .map(|(position, &gene)| {
                let case = if position < edge.len() {
                    edge[position]
                } else {
                    corner[position - edge.len()]
                };
                let orbit_text = match case.orbit {
                    Orbit::Edge => "edge",
                    Orbit::Corner => "corner",
                };
                let (l1, l2) = case.letters();
                SyllableCase {
                    orbit: orbit_text.to_string(),
                    letters: format!("{l1}{l2}"),
                    syllable: inputs.pool[gene as usize].text.clone(),
                    frequency: inputs.freq.cases[position].count,
                }
            })
            .collect();
        let mut texts: HashSet<&str> = HashSet::new();
        for case in &cases {
            if !texts.insert(case.syllable.as_str()) {
                offenders.push(format!("duplicate syllable {}", case.syllable));
            }
        }
        if offenders.is_empty() {
            let syllables = serde_json::to_vec(&cases).map_err(|e| vec![e.to_string()])?;
            return Ok(SyllableAssignment {
                cases,
                pool_size: inputs.pool.len(),
                seed: inputs.params.seed,
                best_fitness: outcome.best_fitness,
                freq_sha256: sha256_hex(&inputs.freq_bytes),
                syllables_sha256: sha256_hex(&syllables),
            });
        }
    }
    Err(offenders)
}

pub fn build_report(
    inputs: &RunInputs,
    outcome: &RunOutcome,
    assignment: &SyllableAssignment,
    config: &serde_json::Value,
) -> GaReport {
    let pool_top: Vec<(String, u64)> = inputs
        .pool
        .iter()
        .take(10)
        .map(|entry| (entry.text.clone(), entry.frequency))
        .collect();
    GaReport {
        run_config: config.clone(),
        inventory: inputs.inventory_report,
        pool_size: inputs.pool.len(),
        pool_top,
        best_fitness: outcome.best_fitness,
        best_parts: outcome.best_parts,
        best_generation: outcome.best_generation,
        generations_run: inputs.params.generations,
        per_gen_best: outcome.per_gen_best.clone(),
        stagnation_tail: outcome.stagnation_tail,
        max_stagnation: outcome.max_stagnation,
        evaluations: outcome.evaluations,
        freq_sha256: assignment.freq_sha256.clone(),
        syllables_sha256: assignment.syllables_sha256.clone(),
    }
}

pub fn render_markdown(report: &GaReport) -> String {
    let [conf, flow, cplx] = report.best_parts;
    let trajectory_head: Vec<String> = report
        .per_gen_best
        .iter()
        .step_by((report.per_gen_best.len() / 20).max(1))
        .map(|f| format!("{f:.6}"))
        .collect();
    let mut pool_lines = String::new();
    for (text, frequency) in &report.pool_top {
        pool_lines.push_str(&format!("| {text} | {frequency} |\n"));
    }
    format!(
        "# ga run report\n\n\
## run config\n\nseed {} population {} generations {} elite {} tournament 2 weights conf {} flow {} cplx {} mutation swap {} replace {}\n\n\
## inventory\n\ngenerated {} kept {} dropped {} wordlist {} covered {} ({:.1}%)\n\n\
## pool\n\nsize {} top entries\n\n| syllable | corpus frequency |\n| --- | --- |\n{}\n\
## result\n\nbest fitness {:.6} at generation {} of {}\nparts conf {:.6} flow {:.6} cplx {:.6} (weighted {:.6} {:.6} {:.6})\nstagnation tail {} max {}\nevaluations {}\n\n\
## trajectory\n\n{}\n\n\
## artifacts\n\nfreq.json sha256 {}\nsyllables.json sha256 {}\n",
        report
            .run_config
            .pointer("/ga/seed")
            .and_then(|v| v.as_u64())
            .unwrap_or(0),
        report
            .run_config
            .pointer("/ga/population")
            .and_then(|v| v.as_u64())
            .unwrap_or(0),
        report
            .run_config
            .pointer("/ga/generations")
            .and_then(|v| v.as_u64())
            .unwrap_or(0),
        report
            .run_config
            .pointer("/ga/elite")
            .and_then(|v| v.as_u64())
            .unwrap_or(0),
        report
            .run_config
            .pointer("/ga/weights/confusion")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0),
        report
            .run_config
            .pointer("/ga/weights/flow")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0),
        report
            .run_config
            .pointer("/ga/weights/complexity")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0),
        report
            .run_config
            .pointer("/ga/mutation/swap")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0),
        report
            .run_config
            .pointer("/ga/mutation/replace")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0),
        report.inventory.generated,
        report.inventory.kept,
        report.inventory.dropped_not_in_wordlist,
        report.inventory.wordlist_size,
        report.inventory.wordlist_covered,
        100.0 * report.inventory.wordlist_covered as f64 / report.inventory.wordlist_size as f64,
        report.pool_size,
        pool_lines,
        report.best_fitness,
        report.best_generation,
        report.generations_run,
        conf,
        flow,
        cplx,
        conf * report
            .run_config
            .pointer("/ga/weights/confusion")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0) as f32,
        flow * report
            .run_config
            .pointer("/ga/weights/flow")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0) as f32,
        cplx * report
            .run_config
            .pointer("/ga/weights/complexity")
            .and_then(|v| v.as_f64())
            .unwrap_or(0.0) as f32,
        report.stagnation_tail,
        report.max_stagnation,
        report.evaluations,
        trajectory_head.join(" "),
        report.freq_sha256,
        report.syllables_sha256,
    )
}

pub fn run_all() -> Result<(), Vec<String>> {
    let config = load_config().map_err(|e| vec![e.to_string()])?;
    let inputs = load_run_inputs().map_err(|e| vec![e])?;
    let outcome = run(&inputs.env, &inputs.params);
    let assignment = build_assignment(&inputs, &outcome)?;
    let report = build_report(&inputs, &outcome, &assignment, &config);
    let data_ga = config
        .pointer("/paths/data_ga")
        .and_then(|v| v.as_str())
        .ok_or_else(|| vec!["config section missing: paths.data_ga".to_string()])?;
    let artifacts = config
        .pointer("/paths/artifacts")
        .and_then(|v| v.as_str())
        .ok_or_else(|| vec!["config section missing: paths.artifacts".to_string()])?;
    let data_ga = config_path(data_ga).map_err(|e| vec![e])?;
    let artifacts = config_path(artifacts).map_err(|e| vec![e])?;
    write_outputs(&data_ga, &artifacts, &assignment, &report).map_err(|e| vec![e])?;
    println!(
        "ga: seed {} pool {} best fitness {:.6} at gen {} cases {} distinct {}, wrote {}",
        inputs.params.seed,
        inputs.pool.len(),
        outcome.best_fitness,
        outcome.best_generation,
        assignment.cases.len(),
        assignment
            .cases
            .iter()
            .map(|c| c.syllable.clone())
            .collect::<HashSet<_>>()
            .len(),
        data_ga.join("syllables.json").display(),
    );
    Ok(())
}

fn write_outputs(
    data_ga: &Path,
    artifacts: &Path,
    assignment: &SyllableAssignment,
    report: &GaReport,
) -> Result<(), String> {
    bt_data::paths::write_json_atomic(&data_ga.join("syllables.json"), assignment)
        .map_err(|e| e.to_string())?;
    bt_data::paths::write_json_atomic(&data_ga.join("report.json"), report)
        .map_err(|e| e.to_string())?;
    let markdown = render_markdown(report);
    std::fs::create_dir_all(artifacts.join("ga")).map_err(|e| e.to_string())?;
    std::fs::write(artifacts.join("ga").join("report.md"), markdown).map_err(|e| e.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn crate_id_matches_package() {
        assert_eq!("bt-ga-run", "bt-ga-run");
    }

    #[test]
    fn cross_check_passes_on_generated_vendor_data() {
        let config = load_config().expect("config loads");
        cross_check_cases(&config).expect("cases.json agrees with scheme");
    }

    #[test]
    fn end_to_end_small_run_produces_full_injective_assignment() {
        let inputs = load_run_inputs().expect("inputs load");
        let mut params = inputs.params;
        params.population = 30;
        params.generations = 20;
        let outcome = run(&inputs.env, &params);
        let assignment = build_assignment(&inputs, &outcome).expect("assignment valid");
        assert_eq!(assignment.cases.len(), CASE_COUNT);
        let mut seen = HashSet::new();
        for case in &assignment.cases {
            assert!(seen.insert(case.syllable.clone()), "{}", case.syllable);
        }
        assert!(assignment.pool_size >= CASE_COUNT);
        assert_eq!(assignment.freq_sha256.len(), 64);
    }
}
