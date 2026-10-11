use crate::persist::Stores;
use crate::states::AppState;
use crate::views::NowDay;
use bevy::prelude::*;

#[derive(Clone, Copy, PartialEq, Debug, Default, Resource)]
pub struct StatsSnapshot {
    pub solves: usize,
    pub memo_mean_ms: f64,
    pub memo_median_ms: f64,
    pub exec_mean_ms: f64,
    pub exec_median_ms: f64,
    pub due_edge: usize,
    pub due_corner: usize,
    pub streak: usize,
}

pub fn median(values: &mut [i64]) -> f64 {
    if values.is_empty() {
        return 0.0;
    }
    values.sort_unstable();
    let mid = values.len() / 2;
    if values.len() % 2 == 1 {
        values[mid] as f64
    } else {
        (values[mid - 1] + values[mid]) as f64 / 2.0
    }
}

pub fn mean(values: &[i64]) -> f64 {
    if values.is_empty() {
        return 0.0;
    }
    values.iter().map(|&value| value as f64).sum::<f64>() / values.len() as f64
}

pub fn streak_of(stores: &Stores) -> usize {
    let mut reviews: Vec<(i64, u32)> = Vec::new();
    for card in stores.srs.cards.values() {
        for entry in &card.reviews {
            reviews.push((entry.ts_days, entry.quality));
        }
    }
    reviews.sort_by_key(|(day, _)| *day);
    let mut streak = 0;
    for (_, quality) in reviews.iter().rev() {
        if *quality >= 3 {
            streak += 1;
        } else {
            break;
        }
    }
    streak
}

pub fn snapshot(stores: &Stores, now_day: i64) -> StatsSnapshot {
    let memo: Vec<i64> = stores.solves.solves.iter().map(|s| s.memo_ms).collect();
    let exec: Vec<i64> = stores.solves.solves.iter().map(|s| s.exec_ms).collect();
    let mut due_edge = 0;
    let mut due_corner = 0;
    for (key, card) in &stores.srs.cards {
        if !card.is_due(now_day) {
            continue;
        }
        if key.starts_with("edge/") {
            due_edge += 1;
        } else if key.starts_with("corner/") {
            due_corner += 1;
        }
    }
    StatsSnapshot {
        solves: stores.solves.solves.len(),
        memo_mean_ms: mean(&memo),
        memo_median_ms: median(&mut memo.clone()),
        exec_mean_ms: mean(&exec),
        exec_median_ms: median(&mut exec.clone()),
        due_edge,
        due_corner,
        streak: streak_of(stores),
    }
}

pub fn enter(stores: Res<Stores>, now_day: Res<NowDay>, mut snap: ResMut<StatsSnapshot>) {
    *snap = snapshot(&stores, now_day.0);
}

pub fn lines(snap: &StatsSnapshot) -> Vec<String> {
    vec![
        format!("solves {}", snap.solves),
        format!(
            "memo mean {:.1}s median {:.1}s",
            snap.memo_mean_ms / 1000.0,
            snap.memo_median_ms / 1000.0
        ),
        format!(
            "exec mean {:.1}s median {:.1}s",
            snap.exec_mean_ms / 1000.0,
            snap.exec_median_ms / 1000.0
        ),
        format!("srs due edge {} corner {}", snap.due_edge, snap.due_corner),
        format!("verified drill streak {}", snap.streak),
    ]
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::persist::{SolveRecord, SrsStore};
    use bt_data::srs::{Card, ReviewLogEntry};
    use std::collections::BTreeMap;

    fn card_with_reviews(qualities: &[u32]) -> Card {
        Card {
            ease: 2.5,
            interval_days: 1,
            due_day: 0,
            reps: qualities.len() as u32,
            lapses: 0,
            reviews: qualities
                .iter()
                .enumerate()
                .map(|(index, quality)| ReviewLogEntry {
                    ts_days: 100 + index as i64,
                    quality: *quality,
                    elapsed_days: 0,
                    scheduled_days: 1,
                })
                .collect(),
        }
    }

    fn stores_with(cards: Vec<(&str, Card)>, solves: Vec<SolveRecord>) -> Stores {
        let mut srs = SrsStore {
            cards: BTreeMap::new(),
        };
        for (key, card) in cards {
            srs.cards.insert(key.to_string(), card);
        }
        Stores {
            settings: crate::persist::Settings {
                memo_cadence_ms: 800,
                memo_mode: crate::persist::MemoMode::Syllables,
                drill_reps: 5,
            },
            srs,
            solves: crate::persist::SolveLog { solves },
            user_algs: Default::default(),
        }
    }

    #[test]
    fn snapshot_aggregates_splits_and_due_counts() {
        let stores = stores_with(
            vec![
                ("edge/UF-UB-UR", card_with_reviews(&[4, 5])),
                ("corner/UFR-UBL-UBR", card_with_reviews(&[2])),
            ],
            vec![
                SolveRecord {
                    started_ms: 1,
                    memo_ms: 20_000,
                    exec_ms: 10_000,
                    success: true,
                },
                SolveRecord {
                    started_ms: 2,
                    memo_ms: 30_000,
                    exec_ms: 20_000,
                    success: false,
                },
            ],
        );
        let snap = snapshot(&stores, 0);
        assert_eq!(snap.solves, 2);
        assert_eq!(snap.memo_mean_ms, 25_000.0);
        assert_eq!(snap.memo_median_ms, 25_000.0);
        assert_eq!(snap.exec_mean_ms, 15_000.0);
        assert_eq!(snap.exec_median_ms, 15_000.0);
        assert_eq!(snap.due_edge, 1);
        assert_eq!(snap.due_corner, 1);
        assert_eq!(snap.streak, 2);
    }

    #[test]
    fn streak_counts_trailing_successful_reviews_across_cards() {
        let stores = stores_with(
            vec![
                ("edge/A", card_with_reviews(&[5])),
                ("corner/B", card_with_reviews(&[4, 4])),
            ],
            vec![],
        );
        assert_eq!(streak_of(&stores), 3);
        let broken = stores_with(vec![("edge/A", card_with_reviews(&[4, 2, 5]))], vec![]);
        assert_eq!(streak_of(&broken), 1);
    }

    #[test]
    fn median_handles_even_counts() {
        assert_eq!(median(&mut [3, 1, 2, 4]), 2.5);
        assert_eq!(median(&mut [5, 1, 3]), 3.0);
        assert_eq!(median(&mut []), 0.0);
    }
}

#[derive(Component, Default)]
pub struct StatsText;

pub fn spawn(commands: &mut Commands, fonts: &crate::present::FontBook) -> Entity {
    let entity = crate::present::spawn_text_view(
        commands,
        fonts,
        AppState::Stats,
        "stats",
        &["esc menu"],
        None,
    );
    commands.entity(entity).with_children(|parent| {
        crate::present::spawn_dynamic(parent, fonts, "", StatsText);
    });
    entity
}

pub fn bind_text(snap: Res<StatsSnapshot>, mut texts: Query<&mut Text, With<StatsText>>) {
    if !snap.is_changed() {
        return;
    }
    let body = lines(&snap).join("\n");
    for mut text in &mut texts {
        *text = Text::new(body.clone());
    }
}
