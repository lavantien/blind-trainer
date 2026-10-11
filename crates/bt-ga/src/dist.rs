use serde::{Deserialize, Serialize};
use std::path::Path;

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct CaseCount {
    pub orbit: String,
    pub letters: String,
    pub count: u64,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct PairCount {
    pub from: u32,
    pub to: u32,
    pub count: u64,
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct FreqData {
    pub cases: Vec<CaseCount>,
    pub adjacent: Vec<PairCount>,
    pub cooccurring: Vec<PairCount>,
    pub total_draws: u64,
    pub seed: u64,
}

pub fn load_freq_data(path: &Path) -> Result<FreqData, String> {
    let bytes = std::fs::read(path).map_err(|e| format!("read {}: {e}", path.display()))?;
    serde_json::from_slice(&bytes).map_err(|e| format!("parse {}: {e}", path.display()))
}

pub fn config_memofreq(config: &serde_json::Value) -> Result<(u64, u64), String> {
    let samples = config
        .pointer("/memofreq/samples")
        .and_then(|v| v.as_u64())
        .ok_or_else(|| "config section missing: memofreq.samples".to_string())?;
    let seed = config
        .pointer("/memofreq/seed")
        .and_then(|v| v.as_u64())
        .ok_or_else(|| "config section missing: memofreq.seed".to_string())?;
    Ok((samples, seed))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn freq_data_round_trips_through_json() {
        let data = FreqData {
            cases: vec![
                CaseCount {
                    orbit: "edge".to_string(),
                    letters: "AB".to_string(),
                    count: 120,
                },
                CaseCount {
                    orbit: "corner".to_string(),
                    letters: "QT".to_string(),
                    count: 7,
                },
            ],
            adjacent: vec![PairCount {
                from: 0,
                to: 17,
                count: 3,
            }],
            cooccurring: vec![PairCount {
                from: 5,
                to: 6,
                count: 9,
            }],
            total_draws: 1000,
            seed: 20261012,
        };
        let bytes = serde_json::to_vec(&data).unwrap();
        let back: FreqData = serde_json::from_slice(&bytes).unwrap();
        assert_eq!(back, data);
        let dir = std::env::temp_dir().join("bt-ga-dist-test");
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("freq.json");
        std::fs::write(&path, &bytes).unwrap();
        assert_eq!(load_freq_data(&path).unwrap(), data);
    }

    #[test]
    fn config_memofreq_reads_the_hub() {
        let config = bt_data::paths::load_config().expect("config loads");
        let (samples, seed) = config_memofreq(&config).expect("memofreq section");
        assert_eq!(samples, 200000);
        assert_eq!(seed, 20261012);
    }
}
