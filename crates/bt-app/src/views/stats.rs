use crate::logic::Fsrs;
use crate::persist::{PersistenceRoot, Stores};
use crate::states::AppState;
use crate::views::NowDay;
use bevy::prelude::*;
use bt_data::db::Db;
use bt_data::repo_cards::{self, CaseCards};

#[derive(Clone, PartialEq, Debug, Default, Resource)]
pub struct StatsSnapshot {
    pub solves: usize,
    pub memo_mean_ms: f64,
    pub memo_median_ms: f64,
    pub exec_mean_ms: f64,
    pub exec_median_ms: f64,
    pub due_edge: usize,
    pub due_corner: usize,
    pub streak: usize,
    pub due_events: Vec<(String, usize)>,
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

pub fn due_events(cards: &CaseCards, now_day: i64) -> Vec<(String, usize)> {
    let mut out: Vec<(String, usize)> = Vec::new();
    for (key, card) in cards {
        let slot = match out.iter().position(|(event, _)| *event == key.event) {
            Some(slot) => slot,
            None => {
                out.push((key.event.clone(), 0));
                out.len() - 1
            }
        };
        if card.is_due(now_day) {
            out[slot].1 += 1;
        }
    }
    out
}

pub fn snapshot(stores: &Stores, cards: &CaseCards, now_day: i64) -> StatsSnapshot {
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
        due_events: due_events(cards, now_day),
    }
}

pub fn enter(
    stores: Res<Stores>,
    now_day: Res<NowDay>,
    root: Res<PersistenceRoot>,
    fsrs: Res<Fsrs>,
    mut snap: ResMut<StatsSnapshot>,
) {
    let cards = match Db::open_root(&root.0) {
        Ok(db) => repo_cards::load(&db, &fsrs.0).unwrap_or_default(),
        Err(_) => CaseCards::default(),
    };
    *snap = snapshot(&stores, &cards, now_day.0);
}

fn seconds(ms: f64) -> String {
    format!("{:.1}s", ms / 1000.0)
}

pub struct StatBlock {
    pub label: String,
    pub value: String,
    pub size_rem: f32,
}

pub fn blocks(snap: &StatsSnapshot) -> Vec<StatBlock> {
    let mut rendered = vec![
        StatBlock {
            label: "solves".to_string(),
            value: snap.solves.to_string(),
            size_rem: crate::theme::FONT_TITLE,
        },
        StatBlock {
            label: "verified drill streak".to_string(),
            value: snap.streak.to_string(),
            size_rem: crate::theme::FONT_TITLE,
        },
        StatBlock {
            label: "memo mean".to_string(),
            value: seconds(snap.memo_mean_ms),
            size_rem: crate::theme::FONT_TITLE,
        },
        StatBlock {
            label: "memo median".to_string(),
            value: seconds(snap.memo_median_ms),
            size_rem: crate::theme::FONT_HEADING,
        },
        StatBlock {
            label: "exec mean".to_string(),
            value: seconds(snap.exec_mean_ms),
            size_rem: crate::theme::FONT_TITLE,
        },
        StatBlock {
            label: "exec median".to_string(),
            value: seconds(snap.exec_median_ms),
            size_rem: crate::theme::FONT_HEADING,
        },
        StatBlock {
            label: "due edge".to_string(),
            value: snap.due_edge.to_string(),
            size_rem: crate::theme::FONT_TITLE,
        },
        StatBlock {
            label: "due corner".to_string(),
            value: snap.due_corner.to_string(),
            size_rem: crate::theme::FONT_TITLE,
        },
    ];
    for (event, count) in &snap.due_events {
        rendered.push(StatBlock {
            label: format!("due {event}"),
            value: count.to_string(),
            size_rem: crate::theme::FONT_TITLE,
        });
    }
    rendered
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::persist::{SolveRecord, SrsStore};
    use bt_data::repo_user::CaseRef;
    use bt_data::srs::{Card, ReviewLogEntry};
    use std::collections::BTreeMap;

    fn card_with_reviews(qualities: &[u32]) -> Card {
        Card {
            memory: None,
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
            seeds: Default::default(),
        }
    }

    fn case_ref(event: &str, method: &str, stage: &str, case_key: &str) -> CaseRef {
        CaseRef {
            event: event.to_string(),
            method: method.to_string(),
            stage: stage.to_string(),
            case_key: case_key.to_string(),
        }
    }

    fn case_cards(entries: Vec<(CaseRef, Card)>) -> CaseCards {
        entries.into_iter().collect()
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
        let mut future = card_with_reviews(&[4]);
        future.due_day = 900;
        let cards = case_cards(vec![
            (
                case_ref("2x2", "cll", "cll", "cll.1"),
                card_with_reviews(&[4]),
            ),
            (case_ref("2x2", "cll", "cll", "cll.2"), future),
            (
                case_ref("skewb", "sarah", "sarah", "s.1"),
                card_with_reviews(&[2]),
            ),
        ]);
        let snap = snapshot(&stores, &cards, 0);
        assert_eq!(snap.solves, 2);
        assert_eq!(snap.memo_mean_ms, 25_000.0);
        assert_eq!(snap.memo_median_ms, 25_000.0);
        assert_eq!(snap.exec_mean_ms, 15_000.0);
        assert_eq!(snap.exec_median_ms, 15_000.0);
        assert_eq!(snap.due_edge, 1);
        assert_eq!(snap.due_corner, 1);
        assert_eq!(snap.streak, 2);
        assert_eq!(
            snap.due_events,
            vec![("2x2".to_string(), 1), ("skewb".to_string(), 1),]
        );
    }

    #[test]
    fn due_events_count_per_event_in_card_order() {
        let mut future = card_with_reviews(&[4]);
        future.due_day = 900;
        let cards = case_cards(vec![
            (
                case_ref("skewb", "sarah", "sarah", "s.1"),
                card_with_reviews(&[4]),
            ),
            (
                case_ref("2x2", "cll", "cll", "cll.1"),
                card_with_reviews(&[4]),
            ),
            (case_ref("2x2", "cll", "cll", "cll.2"), future),
        ]);
        assert_eq!(
            due_events(&cards, 0),
            vec![("2x2".to_string(), 1), ("skewb".to_string(), 1),],
            "card order groups events and only due cards count"
        );
        assert_eq!(
            due_events(&CaseCards::new(), 5),
            Vec::<(String, usize)>::new()
        );
        assert_eq!(
            due_events(&cards, 900),
            vec![("2x2".to_string(), 2), ("skewb".to_string(), 1),],
            "later days make scheduled cards due too"
        );
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

    #[test]
    fn blocks_pair_values_with_labels_in_a_fixed_order() {
        let snap = StatsSnapshot {
            solves: 2,
            memo_mean_ms: 25_000.0,
            memo_median_ms: 25_000.0,
            exec_mean_ms: 15_000.0,
            exec_median_ms: 15_000.0,
            due_edge: 1,
            due_corner: 0,
            streak: 2,
            due_events: vec![("2x2".to_string(), 3)],
        };
        let rendered = blocks(&snap);
        let labels: Vec<&str> = rendered.iter().map(|block| block.label.as_str()).collect();
        assert_eq!(
            labels,
            vec![
                "solves",
                "verified drill streak",
                "memo mean",
                "memo median",
                "exec mean",
                "exec median",
                "due edge",
                "due corner",
                "due 2x2",
            ]
        );
        let values: Vec<&str> = rendered.iter().map(|block| block.value.as_str()).collect();
        assert_eq!(
            values,
            vec!["2", "2", "25.0s", "25.0s", "15.0s", "15.0s", "1", "0", "3"]
        );
        assert_eq!(rendered[0].size_rem, crate::theme::FONT_TITLE);
        assert_eq!(rendered[3].size_rem, crate::theme::FONT_HEADING);
    }
}

#[derive(Component, Default)]
pub struct StatsValue(pub usize);

#[derive(Component, Default)]
pub struct StatsLabel;

pub fn spawn(commands: &mut Commands, fonts: &crate::present::FontBook) -> Entity {
    let content =
        crate::present::spawn_view_frame(commands, fonts, AppState::Stats, "stats", "esc menu");
    commands.entity(content).insert(Node {
        width: Val::Percent(100.0),
        flex_grow: 1.0,
        flex_direction: FlexDirection::Column,
        justify_content: JustifyContent::Center,
        align_items: AlignItems::Center,
        row_gap: crate::theme::rem(crate::theme::SPACING_REM * 2.0),
        ..default()
    });
    commands.entity(content).with_children(|parent| {
        let layout: Vec<(usize, String, f32)> = blocks(&StatsSnapshot::default())
            .iter()
            .enumerate()
            .map(|(index, block)| (index, block.label.clone(), block.size_rem))
            .collect();
        for row in layout.chunks(2) {
            parent
                .spawn(Node {
                    flex_direction: FlexDirection::Row,
                    column_gap: crate::theme::rem(crate::theme::SPACING_REM * 6.0),
                    ..default()
                })
                .with_children(|row_parent| {
                    for (index, label, size_rem) in row {
                        row_parent
                            .spawn(Node {
                                flex_direction: FlexDirection::Column,
                                row_gap: crate::theme::rem(crate::theme::SPACING_REM * 0.5),
                                ..default()
                            })
                            .with_children(|block| {
                                crate::present::spawn_line(
                                    block,
                                    fonts,
                                    "",
                                    *size_rem,
                                    crate::theme::text_primary(),
                                    StatsValue(*index),
                                );
                                crate::present::spawn_line(
                                    block,
                                    fonts,
                                    label,
                                    crate::theme::FONT_MICRO,
                                    crate::theme::text_muted(),
                                    StatsLabel,
                                );
                            });
                    }
                });
        }
    });
    content
}

pub fn bind_text(snap: Res<StatsSnapshot>, mut values: Query<(&mut Text, &StatsValue)>) {
    if !snap.is_changed() {
        return;
    }
    let rendered = blocks(&snap);
    for (mut text, value) in &mut values {
        if let Some(block) = rendered.get(value.0) {
            *text = Text::new(block.value.clone());
        }
    }
}
