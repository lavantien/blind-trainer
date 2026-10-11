use std::path::PathBuf;
use std::sync::OnceLock;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{Duration, Instant};

use bt_core::{Alg, Cube};
use bt_scheme::{
    Classification, Orbit, Rng, apply_memo, classify, memo_from_array, random_state, state_array,
    trace,
};

fn identity_occ() -> [u8; 54] {
    std::array::from_fn(|i| i as u8)
}

pub fn seed_from(data: &[u8]) -> u64 {
    let mut bytes = [0u8; 8];
    let take = data.len().min(8);
    bytes[..take].copy_from_slice(&data[..take]);
    u64::from_le_bytes(bytes) ^ (data.len() as u64).wrapping_mul(0x9E37_79B9_7F4A_7C15)
}

fn shuffle(rng: &mut Rng, values: &mut [u8]) {
    for i in (1..values.len()).rev() {
        let j = rng.below(i as u64 + 1) as usize;
        values.swap(i, j);
    }
}

pub fn repaired_occ(seed: u64) -> [u8; 54] {
    let mut rng = Rng::new(seed);
    let mut corners: Vec<u8> = (0..24u8).collect();
    let mut edges: Vec<u8> = (24..48u8).collect();
    shuffle(&mut rng, &mut corners);
    shuffle(&mut rng, &mut edges);
    let mut occ = identity_occ();
    occ[..24].copy_from_slice(&corners);
    occ[24..48].copy_from_slice(&edges);
    occ
}

fn first_existing(candidates: impl IntoIterator<Item = PathBuf>) -> Option<PathBuf> {
    candidates.into_iter().find(|path| path.is_file())
}

fn config_path() -> PathBuf {
    if let Ok(raw) = std::env::var("BT_CONFIG") {
        if !raw.is_empty() {
            let path = PathBuf::from(&raw);
            if path.is_absolute() {
                return path;
            }
            if let Some(found) = first_existing([path.clone(), PathBuf::from("..").join(&raw)]) {
                return found;
            }
        }
    }
    first_existing([
        PathBuf::from("config").join("config.json"),
        PathBuf::from("..").join("config").join("config.json"),
    ])
    .unwrap_or_else(|| PathBuf::from("..").join("config").join("config.json"))
}

pub fn fuzz_limits() -> (Option<u64>, Option<u64>) {
    let parsed = std::fs::read_to_string(config_path())
        .ok()
        .and_then(|text| serde_json::from_str::<serde_json::Value>(&text).ok());
    match parsed {
        Some(value) => (
            value
                .pointer("/fuzz/runs")
                .and_then(|v| v.as_u64())
                .filter(|&n| n > 0),
            value
                .pointer("/fuzz/secs")
                .and_then(|v| v.as_u64())
                .filter(|&n| n > 0),
        ),
        None => (None, None),
    }
}

struct Bound {
    start: Instant,
    runs: Option<u64>,
    secs: Option<u64>,
}

static BOUND: OnceLock<Bound> = OnceLock::new();
static BOUND_COUNT: AtomicU64 = AtomicU64::new(0);

pub fn within_bounds() -> bool {
    let bound = BOUND.get_or_init(|| {
        let (runs, secs) = fuzz_limits();
        Bound {
            start: Instant::now(),
            runs,
            secs,
        }
    });
    let count = BOUND_COUNT.fetch_add(1, Ordering::Relaxed) + 1;
    if bound.runs.is_some_and(|runs| count > runs) {
        return false;
    }
    if bound
        .secs
        .is_some_and(|secs| bound.start.elapsed() >= Duration::from_secs(secs))
    {
        return false;
    }
    true
}

pub fn notation_once(data: &[u8]) {
    let text = String::from_utf8_lossy(data);
    let parsed = Alg::parse(&text);
    if text.contains('\u{FFFD}') {
        assert!(parsed.is_err(), "replacement char must not parse: {text:?}");
    }
    let Ok(alg) = parsed else {
        return;
    };
    let formatted = alg.format();
    let reparsed = Alg::parse(&formatted).expect("formatted alg must reparse");
    assert_eq!(reparsed, alg, "round trip changed alg: {text:?} -> {formatted}");
    assert_eq!(reparsed.qtm(), alg.qtm());
    assert_eq!(reparsed.htm(), alg.htm());
    let mut cube = Cube::solved();
    cube.apply_alg(&alg);
    let mut restored = cube.clone();
    restored.apply_alg(&alg.inverse());
    assert!(restored.is_solved(), "inverse failed: {text:?}");
    assert!(restored.centers_fixed(), "centers moved: {text:?}");
}

pub fn classify_once(data: &[u8]) {
    let solved = classify(&Cube::solved());
    assert!(matches!(solved.class, Classification::Solved));
    assert!(solved.edge.is_solved());
    assert!(solved.corner.is_solved());
    assert_eq!(solved.position(), None);
    let cube = Cube::from_occ(repaired_occ(seed_from(data)));
    let report = classify(&cube);
    let _ = report.position();
    let edge_trace = trace(&cube, Orbit::Edge);
    let corner_trace = trace(&cube, Orbit::Corner);
    for case in edge_trace.cases() {
        assert!(Orbit::Edge.stickers().contains(&case.t1));
        assert!(Orbit::Edge.stickers().contains(&case.t2));
    }
    for case in corner_trace.cases() {
        assert!(Orbit::Corner.stickers().contains(&case.t1));
        assert!(Orbit::Corner.stickers().contains(&case.t2));
    }
    for case in report.edge.cases() {
        assert!(Orbit::Edge.stickers().contains(&case.t1));
        assert!(Orbit::Edge.stickers().contains(&case.t2));
    }
    for case in report.corner.cases() {
        assert!(Orbit::Corner.stickers().contains(&case.t1));
        assert!(Orbit::Corner.stickers().contains(&case.t2));
    }
}

pub fn memo_once(data: &[u8]) {
    let cube = random_state(seed_from(data));
    let occ = state_array(&cube);
    let memo = memo_from_array(&occ);
    let mut work = occ;
    apply_memo(&mut work, &memo);
    assert_eq!(work, identity_occ(), "memo round trip failed");
}
