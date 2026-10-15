use serde::{Deserialize, Serialize};

pub const WEIGHT_COUNT: usize = 19;

pub const DEFAULT_WEIGHTS: [f64; WEIGHT_COUNT] = [
    0.40255, 1.18385, 3.173, 15.69105, 7.1949, 0.5345, 1.4604, 0.0046, 1.54575, 0.1192, 1.01925,
    1.9395, 0.11, 0.29605, 2.2698, 0.2315, 2.9898, 0.51655, 0.6621,
];

pub const DECAY: f64 = -0.5;

pub const MIN_INITIAL_STABILITY: f64 = 0.1;
pub const MIN_DIFFICULTY: f64 = 1.0;
pub const MAX_DIFFICULTY: f64 = 10.0;

pub fn factor() -> f64 {
    0.9f64.powf(1.0 / DECAY) - 1.0
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Grade {
    Again = 1,
    Hard = 2,
    Good = 3,
    Easy = 4,
}

#[derive(Clone, PartialEq, Debug, Serialize, Deserialize)]
pub struct FsrsConfig {
    pub weights: [f64; WEIGHT_COUNT],
    pub request_retention: f64,
    pub interval_clamp_days: [i64; 2],
}

#[derive(Clone, Copy, PartialEq, Debug, Serialize, Deserialize)]
pub struct MemoryState {
    pub stability: f64,
    pub difficulty: f64,
}

impl FsrsConfig {
    pub fn defaults() -> FsrsConfig {
        FsrsConfig {
            weights: DEFAULT_WEIGHTS,
            request_retention: 0.9,
            interval_clamp_days: [1, 36500],
        }
    }
}

fn clamp_difficulty(difficulty: f64) -> f64 {
    difficulty.clamp(MIN_DIFFICULTY, MAX_DIFFICULTY)
}

fn initial_difficulty(grade: Grade, w: &[f64; WEIGHT_COUNT]) -> f64 {
    clamp_difficulty(w[4] - (w[5] * (grade as i32 as f64 - 1.0)).exp() + 1.0)
}

fn next_difficulty(difficulty: f64, grade: Grade, w: &[f64; WEIGHT_COUNT]) -> f64 {
    let delta = -(w[6] * (grade as i32 as f64 - 3.0));
    let damped = difficulty + (MAX_DIFFICULTY - difficulty) * delta / 9.0;
    let anchor = initial_difficulty(Grade::Easy, w);
    clamp_difficulty(w[7] * anchor + (1.0 - w[7]) * damped)
}

fn short_term_stability(stability: f64, grade: Grade, w: &[f64; WEIGHT_COUNT]) -> f64 {
    stability * (w[17] * (grade as i32 as f64 - 3.0 + w[18])).exp()
}

fn recall_stability(
    stability: f64,
    difficulty: f64,
    retrievability: f64,
    grade: Grade,
    w: &[f64; WEIGHT_COUNT],
) -> f64 {
    let hard_penalty = if grade == Grade::Hard { w[15] } else { 1.0 };
    let easy_bonus = if grade == Grade::Easy { w[16] } else { 1.0 };
    stability
        * (1.0
            + w[8].exp()
                * (11.0 - difficulty)
                * stability.powf(-w[9])
                * (((1.0 - retrievability) * w[10]).exp() - 1.0)
                * hard_penalty
                * easy_bonus)
}

fn forget_stability(
    stability: f64,
    difficulty: f64,
    retrievability: f64,
    w: &[f64; WEIGHT_COUNT],
) -> f64 {
    w[11]
        * difficulty.powf(-w[12])
        * ((stability + 1.0).powf(w[13]) - 1.0)
        * ((1.0 - retrievability) * w[14]).exp()
}

impl MemoryState {
    pub fn initial(grade: Grade, cfg: &FsrsConfig) -> MemoryState {
        MemoryState {
            stability: cfg.weights[grade as usize - 1].max(MIN_INITIAL_STABILITY),
            difficulty: initial_difficulty(grade, &cfg.weights),
        }
    }

    pub fn retrievability(&self, elapsed_days: f64) -> f64 {
        let elapsed = elapsed_days.max(0.0);
        (1.0 + factor() * elapsed / self.stability).powf(DECAY)
    }

    pub fn review(&self, grade: Grade, elapsed_days: f64, cfg: &FsrsConfig) -> MemoryState {
        let w = &cfg.weights;
        let difficulty = next_difficulty(self.difficulty, grade, w);
        let stability = if elapsed_days < 1.0 {
            short_term_stability(self.stability, grade, w)
        } else {
            let retrievability = self.retrievability(elapsed_days);
            if grade == Grade::Again {
                let lapsed = forget_stability(self.stability, self.difficulty, retrievability, w);
                let ceiling = self.stability / (w[17] * w[18]).exp();
                lapsed.min(ceiling)
            } else {
                recall_stability(self.stability, self.difficulty, retrievability, grade, w)
            }
        };
        MemoryState {
            stability,
            difficulty,
        }
    }
}

pub fn next_interval_days(stability: f64, cfg: &FsrsConfig) -> i64 {
    let raw = (stability / factor()) * (cfg.request_retention.powf(1.0 / DECAY) - 1.0);
    let rounded = raw.round_ties_even() as i64;
    rounded
        .max(cfg.interval_clamp_days[0].max(1))
        .min(cfg.interval_clamp_days[1])
}
