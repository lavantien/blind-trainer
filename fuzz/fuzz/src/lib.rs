use std::path::PathBuf;
use std::sync::OnceLock;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{Duration, Instant};

use bt_core::{Alg, Cube, corner_stickers, edge_stickers};
use bt_data::commutator::{decompose, decompose_verified};
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
    let mut corners: Vec<u8> = (0..8u8).collect();
    let mut edges: Vec<u8> = (0..12u8).collect();
    shuffle(&mut rng, &mut corners);
    shuffle(&mut rng, &mut edges);
    let mut occ = identity_occ();
    for slot in 0..8usize {
        let piece = corner_stickers(corners[slot]);
        let positions = corner_stickers(slot as u8);
        let twist = rng.below(3) as usize;
        for j in 0..3 {
            occ[positions[(j + twist) % 3] as usize] = piece[j];
        }
    }
    for slot in 0..12usize {
        let [p0, p1] = edge_stickers(edges[slot]);
        let [s0, s1] = edge_stickers(slot as u8);
        if rng.below(2) == 1 {
            occ[s0 as usize] = p1;
            occ[s1 as usize] = p0;
        } else {
            occ[s0 as usize] = p0;
            occ[s1 as usize] = p1;
        }
    }
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
            if let Some(found) = first_existing([
                path.clone(),
                PathBuf::from("..").join(&raw),
                PathBuf::from("../..").join(&raw),
            ]) {
                return found;
            }
        }
    }
    let mut dir = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
    for _ in 0..5 {
        dir.push("config");
        dir.push("config.json");
        if dir.is_file() {
            return dir;
        }
        dir.pop();
        dir.pop();
        if !dir.pop() {
            break;
        }
    }
    PathBuf::from("../../config/config.json")
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
    assert_eq!(
        reparsed, alg,
        "round trip changed alg: {text:?} -> {formatted}"
    );
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

fn unit_expansion(unit: &bt_data::commutator::Expanded) -> Alg {
    let mut moves = Vec::new();
    if let Some(setup) = &unit.setup {
        moves.extend_from_slice(setup.moves());
    }
    let (first, second) = if unit.b_first {
        (&unit.b, &unit.a)
    } else {
        (&unit.a, &unit.b)
    };
    moves.extend_from_slice(first.moves());
    moves.extend_from_slice(second.moves());
    moves.extend_from_slice(first.inverse().moves());
    moves.extend_from_slice(second.inverse().moves());
    if let Some(setup) = &unit.setup {
        moves.extend_from_slice(setup.inverse().moves());
    }
    Alg::new(moves)
}

pub fn commutator_once(data: &[u8]) {
    let text = String::from_utf8_lossy(data);
    let units = decompose(&text);
    assert_eq!(decompose(&text), units, "decompose must be deterministic");
    let Some(units) = units else {
        return;
    };
    let mut derived = Vec::new();
    for unit in &units {
        let expansion = unit_expansion(unit);
        let mut cube = Cube::solved();
        cube.apply_alg(&expansion);
        let mut restored = cube;
        restored.apply_alg(&expansion.inverse());
        assert!(restored.is_solved(), "expansion not invertible: {text:?}");
        derived.extend_from_slice(expansion.moves());
    }
    let derived_alg = Alg::new(derived);
    let verified = decompose_verified(&text, &derived_alg)
        .expect("derived expansion must verify against itself");
    let mut candidate = Cube::solved();
    for unit in &verified {
        candidate.apply_alg(&unit_expansion(unit));
    }
    let mut reference = Cube::solved();
    reference.apply_alg(&derived_alg);
    assert_eq!(candidate, reference, "verified order mismatch: {text:?}");
    if let Ok(alg) = Alg::parse(&text) {
        if let Some(verified) = decompose_verified(&text, &alg) {
            let mut candidate = Cube::solved();
            for unit in &verified {
                candidate.apply_alg(&unit_expansion(unit));
            }
            let mut reference = Cube::solved();
            reference.apply_alg(&alg);
            assert_eq!(candidate, reference, "verified order mismatch: {text:?}");
        }
    }
}

const DB_STAGES: [&str; 6] = ["edge", "corner", "flips", "twists", "parity", "ltct"];
const DB_ALPHABET: &[u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -'!/\\\"#$%&()[]{}<>?,.;:=+*~^@|_";

fn db_string(rng: &mut Rng) -> String {
    let len = rng.below(24) as usize;
    (0..len)
        .map(|_| DB_ALPHABET[rng.below(DB_ALPHABET.len() as u64) as usize] as char)
        .collect()
}

fn db_i64(rng: &mut Rng) -> i64 {
    (rng.below(1u64 << 62)) as i64 - (1i64 << 61)
}

pub fn db_once(data: &[u8]) {
    use bt_data::db::Db;
    use bt_data::paths::{UserAlg, UserAlgs};
    use bt_data::repo_algs;
    use bt_data::repo_settings;
    use bt_data::repo_solves;
    use bt_data::repo_solves::{SolveLog, SolveRecord};
    use bt_data::repo_srs;
    use bt_data::repo_srs::SrsStore;
    use bt_data::srs::{Card, ReviewLogEntry};
    use std::collections::BTreeMap;

    let mut rng = Rng::new(seed_from(data));
    let mut db = Db::open_in_memory().expect("open");
    assert_eq!(
        db.user_version().expect("version"),
        bt_data::db::LATEST_USER_VERSION
    );

    let mut algs = UserAlgs::new();
    let mut cards = BTreeMap::new();
    for _ in 0..rng.below(32) {
        let stage = DB_STAGES[rng.below(DB_STAGES.len() as u64) as usize];
        let case_key = db_string(&mut rng);
        if case_key.is_empty() {
            continue;
        }
        let key = format!("{stage}/{case_key}");
        algs.entries.insert(
            key.clone(),
            UserAlg {
                alg: db_string(&mut rng),
            },
        );
        let reviews = (0..rng.below(20))
            .map(|_| ReviewLogEntry {
                ts_days: db_i64(&mut rng),
                quality: rng.below(6) as u32,
                elapsed_days: db_i64(&mut rng),
                scheduled_days: db_i64(&mut rng),
            })
            .collect::<Vec<_>>();
        cards.insert(
            key,
            Card {
                ease: 1.0 + rng.below(400) as f32 / 100.0,
                interval_days: db_i64(&mut rng),
                due_day: db_i64(&mut rng),
                reps: rng.below(1u64 << 30) as u32,
                lapses: rng.below(1u64 << 30) as u32,
                reviews,
            },
        );
    }
    let store = SrsStore { cards };
    let log = SolveLog {
        solves: (0..rng.below(32))
            .map(|_| SolveRecord {
                started_ms: db_i64(&mut rng),
                memo_ms: db_i64(&mut rng),
                exec_ms: db_i64(&mut rng),
                success: rng.below(2) == 1,
            })
            .collect(),
    };
    let settings_text = db_string(&mut rng);

    repo_algs::save(&mut db, &algs).expect("save algs");
    repo_srs::save(&mut db, &store).expect("save srs");
    repo_solves::save(&mut db, &log).expect("save solves");
    repo_settings::save(&mut db, repo_settings::APP_KEY, &settings_text).expect("save settings");

    assert_eq!(repo_algs::load(&db).expect("load algs"), algs);
    assert_eq!(repo_srs::load(&db).expect("load srs"), store);
    assert_eq!(repo_solves::load(&db).expect("load solves"), log);
    assert_eq!(
        repo_settings::load_text(&db, repo_settings::APP_KEY).expect("load settings"),
        Some(settings_text)
    );

    repo_algs::save(&mut db, &UserAlgs::new()).expect("clear algs");
    repo_srs::save(&mut db, &SrsStore::default()).expect("clear srs");
    repo_solves::save(&mut db, &SolveLog::default()).expect("clear solves");
    assert_eq!(repo_algs::load(&db).expect("reload algs"), UserAlgs::new());
    assert_eq!(
        repo_srs::load(&db).expect("reload srs"),
        SrsStore::default()
    );
    assert_eq!(
        repo_solves::load(&db).expect("reload solves"),
        SolveLog::default()
    );
    assert_eq!(db.is_fresh().expect("fresh"), false);
}

fn fsrs_grade(rng: &mut Rng) -> bt_data::fsrs::Grade {
    bt_data::fsrs::Grade::ALL[rng.below(4) as usize]
}

fn fsrs_bounded_elapsed(rng: &mut Rng) -> f64 {
    match rng.below(4) {
        0 => rng.below(10) as f64 / 10.0,
        1 => rng.below(3660) as f64,
        2 => rng.below(365_000) as f64,
        _ => rng.below(1_000_000_000) as f64,
    }
}

pub fn fsrs_once(data: &[u8]) {
    fsrs_default_walk(seed_from(data));
    fsrs_hostile_walk(seed_from(data));
    fsrs_replay_walk(seed_from(data));
    fsrs_config_parse(seed_from(data));
}

fn fsrs_default_walk(seed: u64) {
    use bt_data::fsrs::{FsrsConfig, MemoryState, next_interval_days};

    let cfg = FsrsConfig::defaults();
    let mut rng = Rng::new(seed ^ 0x5f5f_4445_4641_5531);
    let mut memory = MemoryState::initial(fsrs_grade(&mut rng), &cfg);
    assert!(memory.is_valid());
    for _ in 0..rng.below(600) {
        let grade = fsrs_grade(&mut rng);
        let elapsed = fsrs_bounded_elapsed(&mut rng);
        memory = memory.review(grade, elapsed, &cfg);
        assert!(
            memory.is_valid(),
            "default weights must keep the state valid, elapsed {elapsed}"
        );
        let interval = next_interval_days(memory.stability, &cfg);
        assert!(
            (1..=36500).contains(&interval),
            "interval {interval} left the clamps"
        );
        let retrievability = memory.retrievability(elapsed);
        assert!(
            retrievability.is_finite() && retrievability > 0.0 && retrievability <= 1.0,
            "retrievability {retrievability} left (0, 1]"
        );
    }
}

fn fsrs_hostile_walk(seed: u64) {
    use bt_data::fsrs::{FsrsConfig, MemoryState, next_interval_days};

    let mut rng = Rng::new(seed ^ 0x5f5f_484f_5354_4931);
    let mut cfg = FsrsConfig::defaults();
    for weight in &mut cfg.weights {
        match rng.below(3) {
            0 => {}
            1 => *weight = rng.below(10_000) as f64 / 7.0,
            _ => *weight = f64::from_bits(rng.below(u64::MAX)),
        }
    }
    cfg.request_retention = match rng.below(3) {
        0 => 0.9,
        1 => rng.below(1500) as f64 / 1000.0,
        _ => f64::from_bits(rng.below(u64::MAX)),
    };
    cfg.interval_clamp_days = [rng.below(100) as i64 - 50, rng.below(100) as i64 - 50];
    let mut memory = MemoryState::initial(fsrs_grade(&mut rng), &cfg);
    for _ in 0..rng.below(300) {
        let elapsed = match rng.below(3) {
            0 => fsrs_bounded_elapsed(&mut rng),
            _ => f64::from_bits(rng.below(u64::MAX)),
        };
        memory = memory.review(fsrs_grade(&mut rng), elapsed, &cfg);
        let _ = next_interval_days(memory.stability, &cfg);
        let _ = memory.retrievability(elapsed);
    }
}

fn fsrs_replay_walk(seed: u64) {
    use bt_data::fsrs::FsrsConfig;
    use bt_data::srs::{ReplayError, ReviewLogEntry, replay_sm2};

    let cfg = FsrsConfig::defaults();
    let mut rng = Rng::new(seed ^ 0x5f5f_5245_5041_5931);
    let mut history: Vec<ReviewLogEntry> = Vec::new();
    for _ in 0..rng.below(64) {
        let quality = rng.below(8) as u32;
        history.push(ReviewLogEntry {
            ts_days: history.len() as i64,
            quality,
            elapsed_days: rng.below(4000) as i64 - 100,
            scheduled_days: 0,
        });
        match replay_sm2(&history, &cfg) {
            Ok(Some(state)) => assert!(
                state.is_valid(),
                "qualities 2..=5 must replay to a valid state, history {history:?}"
            ),
            Ok(None) => assert!(history.is_empty(), "nonempty history replayed to none"),
            Err(ReplayError::UnknownQuality(found)) => {
                assert!(found < 2 || found > 5, "quality {found} rejected");
            }
        }
    }
}

fn fsrs_config_parse(seed: u64) {
    use bt_data::fsrs::FsrsConfig;

    let mut rng = Rng::new(seed ^ 0x5f5f_4346_4746_5a31);
    for _ in 0..rng.below(32) {
        let weights: Vec<f64> = (0..bt_data::fsrs::WEIGHT_COUNT)
            .map(|_| match rng.below(3) {
                0 => 0.5,
                1 => rng.below(2000) as f64,
                _ => f64::from_bits(rng.below(u64::MAX)),
            })
            .collect();
        let retention = match rng.below(3) {
            0 => 0.9,
            1 => rng.below(1500) as f64 / 1000.0,
            _ => f64::from_bits(rng.below(u64::MAX)),
        };
        let lo = rng.below(10) as i64 - 3;
        let hi = rng.below(40_000) as i64;
        let value = serde_json::json!({
            "srs": {"fsrs": {"weights": weights, "request_retention": retention, "interval_clamp_days": [lo, hi]}}
        });
        if let Ok(cfg) = FsrsConfig::from_value(&value) {
            assert!(cfg.weights.iter().all(|weight| weight.is_finite()));
            assert!(
                cfg.request_retention > 0.0 && cfg.request_retention <= 1.0,
                "retention {retention} accepted"
            );
            assert!(
                cfg.interval_clamp_days[0] >= 1
                    && cfg.interval_clamp_days[1] >= cfg.interval_clamp_days[0],
                "clamps {:?} accepted",
                cfg.interval_clamp_days
            );
        }
    }
}
