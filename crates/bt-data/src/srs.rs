use crate::fsrs::{FsrsConfig, Grade, MemoryState, next_interval_days};
use serde::{Deserialize, Serialize};
use std::fmt;

pub use crate::fsrs::Grade as Rating;

#[derive(Debug)]
pub enum ConfigError {
    Missing(&'static str),
    Malformed(serde_json::Error),
    Invalid(&'static str),
}

impl fmt::Display for ConfigError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ConfigError::Missing(path) => write!(f, "config section missing: {path}"),
            ConfigError::Malformed(source) => write!(f, "config section malformed: {source}"),
            ConfigError::Invalid(why) => write!(f, "config section invalid: {why}"),
        }
    }
}

impl std::error::Error for ConfigError {}

#[derive(Clone, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct ReviewLogEntry {
    pub ts_days: i64,
    pub quality: u32,
    pub elapsed_days: i64,
    pub scheduled_days: i64,
}

#[derive(Clone, PartialEq, Debug, Serialize, Deserialize)]
pub struct Card {
    #[serde(default)]
    pub memory: Option<MemoryState>,
    pub interval_days: i64,
    pub due_day: i64,
    pub reps: u32,
    pub lapses: u32,
    pub reviews: Vec<ReviewLogEntry>,
}

impl Card {
    pub fn new(now_days: i64) -> Card {
        Card {
            memory: None,
            interval_days: 0,
            due_day: now_days,
            reps: 0,
            lapses: 0,
            reviews: Vec::new(),
        }
    }

    pub fn review(&mut self, rating: Rating, now_days: i64, cfg: &FsrsConfig) {
        let elapsed_days = match self.reviews.last() {
            Some(entry) => (now_days - entry.ts_days).max(0),
            None => 0,
        };
        let memory = if self.reviews.is_empty() {
            MemoryState::initial(rating, cfg)
        } else {
            match self
                .memory
                .or_else(|| replay_sm2(&self.reviews, cfg).ok().flatten())
            {
                Some(prior) => prior.review(rating, elapsed_days as f64, cfg),
                None => MemoryState::initial(rating, cfg),
            }
        };
        self.memory = Some(memory);
        self.reps += 1;
        if rating == Grade::Again {
            self.lapses += 1;
        }
        self.interval_days = next_interval_days(memory.stability, cfg);
        self.due_day = now_days + self.interval_days;
        self.reviews.push(ReviewLogEntry {
            ts_days: now_days,
            quality: rating.to_code() as u32 + 1,
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

#[derive(Debug, PartialEq, Eq)]
pub enum ReplayError {
    UnknownQuality(u32),
}

impl fmt::Display for ReplayError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ReplayError::UnknownQuality(quality) => {
                write!(f, "review quality {quality} has no sm-2 grade mapping")
            }
        }
    }
}

impl std::error::Error for ReplayError {}

pub fn grade_from_quality(quality: u32) -> Result<Grade, ReplayError> {
    match quality {
        2 => Ok(Grade::Again),
        3 => Ok(Grade::Hard),
        4 => Ok(Grade::Good),
        5 => Ok(Grade::Easy),
        other => Err(ReplayError::UnknownQuality(other)),
    }
}

pub fn replay_sm2(
    reviews: &[ReviewLogEntry],
    cfg: &crate::fsrs::FsrsConfig,
) -> Result<Option<crate::fsrs::MemoryState>, ReplayError> {
    let Some(first) = reviews.first() else {
        return Ok(None);
    };
    let mut memory = crate::fsrs::MemoryState::initial(grade_from_quality(first.quality)?, cfg);
    for entry in reviews.iter().skip(1) {
        let grade = grade_from_quality(entry.quality)?;
        memory = memory.review(grade, entry.elapsed_days as f64, cfg);
    }
    Ok(Some(memory))
}
