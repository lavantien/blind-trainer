use serde::{Deserialize, Serialize};
use std::fmt;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Rating {
    Again,
    Hard,
    Good,
    Easy,
}

impl Rating {
    pub const ALL: [Rating; 4] = [Rating::Again, Rating::Hard, Rating::Good, Rating::Easy];
}

#[derive(Debug)]
pub enum ConfigError {
    Missing(&'static str),
    Malformed(serde_json::Error),
}

impl fmt::Display for ConfigError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ConfigError::Missing(path) => write!(f, "config section missing: {path}"),
            ConfigError::Malformed(source) => write!(f, "config section malformed: {source}"),
        }
    }
}

impl std::error::Error for ConfigError {}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct Sm2Config {
    qualities: [u32; 4],
    interval_clamp_days: [i64; 2],
}

#[derive(Deserialize)]
struct Sm2Section {
    again: u32,
    hard: u32,
    good: u32,
    easy: u32,
    interval_clamp_days: [i64; 2],
}

impl Sm2Config {
    pub fn from_value(config: &serde_json::Value) -> Result<Sm2Config, ConfigError> {
        let section = config
            .pointer("/srs/sm2")
            .ok_or(ConfigError::Missing("srs.sm2"))?;
        let parsed: Sm2Section =
            serde_json::from_value(section.clone()).map_err(ConfigError::Malformed)?;
        Ok(Sm2Config {
            qualities: [parsed.again, parsed.hard, parsed.good, parsed.easy],
            interval_clamp_days: parsed.interval_clamp_days,
        })
    }

    pub fn quality(&self, rating: Rating) -> u32 {
        self.qualities[rating as usize]
    }

    pub fn interval_min_days(&self) -> i64 {
        self.interval_clamp_days[0]
    }

    pub fn interval_max_days(&self) -> i64 {
        self.interval_clamp_days[1]
    }
}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct ReviewLogEntry {
    pub ts_days: i64,
    pub quality: u32,
    pub elapsed_days: i64,
    pub scheduled_days: i64,
}

#[derive(Clone, PartialEq, Debug, Serialize, Deserialize)]
pub struct Card {
    pub ease: f32,
    pub interval_days: i64,
    pub due_day: i64,
    pub reps: u32,
    pub lapses: u32,
    pub reviews: Vec<ReviewLogEntry>,
}

const DEFAULT_EASE: f32 = 2.5;
const EASE_FLOOR: f32 = 1.3;
const EASE_DELTA_BASE: f32 = 0.1;
const EASE_DELTA_LINEAR: f32 = 0.08;
const EASE_DELTA_QUADRATIC: f32 = 0.02;
const PASS_THRESHOLD: u32 = 3;
const FIRST_INTERVAL: i64 = 1;
const SECOND_INTERVAL: i64 = 6;

impl Card {
    pub fn new(now_days: i64) -> Card {
        Card {
            ease: DEFAULT_EASE,
            interval_days: 0,
            due_day: now_days,
            reps: 0,
            lapses: 0,
            reviews: Vec::new(),
        }
    }

    pub fn review(&mut self, rating: Rating, now_days: i64, cfg: &Sm2Config) {
        let quality = cfg.quality(rating);
        let elapsed_days = match self.reviews.last() {
            Some(entry) => (now_days - entry.ts_days).max(0),
            None => 0,
        };
        if quality < PASS_THRESHOLD {
            self.reps = 0;
            self.lapses += 1;
            self.interval_days = FIRST_INTERVAL;
        } else {
            self.interval_days = match self.reps {
                0 => FIRST_INTERVAL,
                1 => SECOND_INTERVAL,
                _ => (self.interval_days as f32 * self.ease).round() as i64,
            };
            self.reps += 1;
            self.ease = (self.ease + ease_delta(quality)).max(EASE_FLOOR);
        }
        self.interval_days = self
            .interval_days
            .clamp(cfg.interval_min_days(), cfg.interval_max_days());
        self.due_day = now_days + self.interval_days;
        self.reviews.push(ReviewLogEntry {
            ts_days: now_days,
            quality,
            elapsed_days,
            scheduled_days: self.interval_days,
        });
    }

    pub fn last_review_day(&self) -> Option<i64> {
        self.reviews.last().map(|entry| entry.ts_days)
    }

    pub fn is_due(&self, now_days: i64) -> bool {
        now_days >= self.due_day
    }
}

fn ease_delta(quality: u32) -> f32 {
    let gap = 5f32 - quality.min(5) as f32;
    EASE_DELTA_BASE - gap * (EASE_DELTA_LINEAR + gap * EASE_DELTA_QUADRATIC)
}
