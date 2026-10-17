use crate::logic::{AppConfig, NavIntent, NetState, VendorCorpus};
use crate::states::AppState;
use crate::theme;
use crate::views::PendingDrill;
use crate::views::drill::{buffer_of, case_position, expand_alg_line, has_commutator};
use bevy::feathers::controls::{FeathersTextInput, FeathersTextInputContainer};
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bt_core::Cube;
use bt_data::commutator::{Expanded, decompose_verified};
use bt_data::corpus::{Corpus, Row, SheetClass};
use bt_scheme::{CaseId, Orbit, enumerate_cases};

pub mod browse;
pub mod detail;
pub mod display;

#[derive(Clone, Debug, Default, Resource)]
pub struct AlgsQuery(pub String);

#[derive(Clone, Debug, Resource)]
pub struct AlgsBrowser {
    pub all: Vec<CaseId>,
    pub filtered: Vec<CaseId>,
    pub selected: usize,
}

impl Default for AlgsBrowser {
    fn default() -> AlgsBrowser {
        AlgsBrowser {
            all: all_cases(),
            filtered: Vec::new(),
            selected: 0,
        }
    }
}

pub fn all_cases() -> Vec<CaseId> {
    let mut cases: Vec<CaseId> = enumerate_cases(Orbit::Edge);
    cases.extend(enumerate_cases(Orbit::Corner));
    cases
}

pub fn letters_of(case: &CaseId) -> String {
    let (first, second) = case.letters();
    format!("{first}{second}")
}

pub fn orbit_word(orbit: Orbit) -> &'static str {
    match orbit {
        Orbit::Edge => "edge",
        Orbit::Corner => "corner",
    }
}

pub fn filter_cases(all: &[CaseId], query: &str) -> Vec<CaseId> {
    let trimmed = query.trim().to_ascii_uppercase();
    if trimmed.is_empty() {
        return all.to_vec();
    }
    all.iter()
        .filter(|case| letters_of(case).starts_with(&trimmed))
        .copied()
        .collect()
}

pub fn case_rows<'a>(corpus: &'a Corpus, case: &CaseId) -> Vec<&'a Row> {
    corpus.rows_for_position(sheet(case), &case_position(case))
}

pub fn sheet(case: &CaseId) -> SheetClass {
    crate::views::drill::sheet_of(case.orbit)
}

pub fn covered(corpus: &Corpus, case: &CaseId) -> bool {
    !case_rows(corpus, case).is_empty()
}

pub fn enter(
    query: Res<AlgsQuery>,
    mut browser: ResMut<AlgsBrowser>,
    mut net: ResMut<NetState>,
    mut messages: ResMut<Messages<KeyboardInput>>,
) {
    browser.filtered = filter_cases(&browser.all, &query.0);
    browser.selected = 0;
    messages.clear();
    if let Some(case) = browser.filtered.first() {
        highlight(&mut net, *case);
    }
}

pub fn refresh_list(
    query: Res<AlgsQuery>,
    mut browser: ResMut<AlgsBrowser>,
    mut net: ResMut<NetState>,
) {
    if !query.is_changed() {
        return;
    }
    browser.filtered = filter_cases(&browser.all, &query.0);
    browser.selected = browser
        .selected
        .min(browser.filtered.len().saturating_sub(1));
    if let Some(case) = browser.filtered.get(browser.selected) {
        highlight(&mut net, *case);
    }
}

fn highlight(net: &mut NetState, case: CaseId) {
    net.cube = Cube::solved();
    net.buffer_sticker = Some(buffer_of(case.orbit));
    net.targets = [Some(case.t1), Some(case.t2)];
}

pub fn focus_query_keys(
    mut inputs: MessageReader<KeyboardInput>,
    focus: Option<ResMut<bevy::input_focus::InputFocus>>,
    boxes: Query<&Children, With<AlgsQueryBox>>,
    editables: Query<&bevy::text::EditableText>,
) {
    let Some(mut focus) = focus else {
        return;
    };
    for input in inputs.read() {
        if input.state != ButtonState::Pressed || input.key_code != KeyCode::Slash {
            continue;
        }
        for children in &boxes {
            for child in children.iter() {
                if editables.get(child).is_ok() && focus.get() != Some(child) {
                    focus.set(child, bevy::input_focus::FocusCause::Navigated);
                }
            }
        }
        return;
    }
}

pub fn list_keys(
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    focus: Option<Res<bevy::input_focus::InputFocus>>,
    editables: Query<&bevy::text::EditableText>,
    repo: Res<browse::RepoBrowser>,
    mut browser: ResMut<AlgsBrowser>,
    mut net: ResMut<NetState>,
) {
    let typing = focus
        .as_deref()
        .and_then(|focus| focus.get())
        .is_some_and(|entity| editables.get(entity).is_ok());
    let mut step = 0i32;
    let mut drill = false;
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        match input.key_code {
            KeyCode::ArrowDown => step += 1,
            KeyCode::ArrowUp => step -= 1,
            KeyCode::KeyN if !typing => step += 1,
            KeyCode::KeyP if !typing => step -= 1,
            KeyCode::Enter | KeyCode::KeyD if !typing => drill = true,
            _ => {}
        }
    }
    if !repo.letters_focused() {
        return;
    }
    if step != 0 && !browser.filtered.is_empty() {
        let len = browser.filtered.len() as i32;
        let current = browser.selected as i32;
        browser.selected = ((current + step).rem_euclid(len)) as usize;
    }
    if let Some(case) = browser.filtered.get(browser.selected) {
        highlight(&mut net, *case);
    }
    if drill && let Some(selected) = browser.filtered.get(browser.selected).copied() {
        commands.write_message(NavIntent::Open(AppState::Drill));
        commands.queue(move |world: &mut World| {
            world.resource_mut::<PendingDrill>().0 = Some(selected);
        });
    }
}

pub fn list_capacity(config: &AppConfig) -> usize {
    let chrome = theme::ROOT_REM * (theme::HEADER_HEIGHT_REM + 2.0 * theme::PANEL_PADDING_REM);
    let query_box = theme::ROOT_REM * 3.0;
    let line = theme::ROOT_REM * theme::FONT_BODY * 1.5;
    (((config.window.height as f32 - chrome - query_box) / line).floor() as usize).max(1)
}

pub fn list_window(
    filtered_len: usize,
    selected: usize,
    capacity: usize,
) -> std::ops::Range<usize> {
    let capacity = capacity.min(filtered_len).max(1);
    let start = selected
        .saturating_sub(capacity / 2)
        .min(filtered_len.saturating_sub(capacity));
    start..start + capacity
}

pub fn list_rows(browser: &AlgsBrowser, capacity: usize) -> Vec<(String, bool)> {
    if browser.filtered.is_empty() {
        return vec![("no case matches the query".to_string(), false)];
    }
    list_window(browser.filtered.len(), browser.selected, capacity)
        .map(|index| {
            let case = browser.filtered[index];
            let selected = index == browser.selected;
            let caret = if selected { ">" } else { " " };
            (
                format!("{caret} {} {}", letters_of(&case), orbit_word(case.orbit)),
                selected,
            )
        })
        .collect()
}

pub fn meta_line(row: &Row) -> String {
    let verdict = bt_data::verify::verify(&row.alg, &row.position, row.sheet);
    let badge = if verdict.ok() {
        "verified".to_string()
    } else {
        format!("{} problem(s)", verdict.problems.len())
    };
    format!(
        "{} qtm {} htm {} [{}]",
        row.solver,
        row.qtm(),
        row.htm(),
        badge
    )
}

pub fn pair_expand_rows(row: &Row) -> Vec<String> {
    let displayed = if decompose_verified(&row.commutator, &row.alg).is_some() {
        ""
    } else {
        row.alg_text.as_str()
    };
    expand_alg_line(&row.commutator, &row.alg, &row.alg_text, displayed)
        .map(|line| vec![line])
        .unwrap_or_default()
}

pub fn mechanic_spans(units: &[Expanded]) -> Vec<(String, Color)> {
    let mut spans = Vec::new();
    for (index, unit) in units.iter().enumerate() {
        if index > 0 {
            spans.push((" + ".to_string(), theme::text_muted()));
        }
        if let Some(setup) = &unit.setup {
            spans.push((format!("{}: ", setup.format()), theme::setup()));
        }
        spans.push(("[".to_string(), theme::text_muted()));
        if unit.b_first {
            spans.push((unit.b.format(), theme::interchange()));
            spans.push((",".to_string(), theme::text_muted()));
            spans.push((unit.a.format(), theme::teal()));
        } else {
            spans.push((unit.a.format(), theme::teal()));
            spans.push((",".to_string(), theme::text_muted()));
            spans.push((unit.b.format(), theme::interchange()));
        }
        spans.push(("]".to_string(), theme::text_muted()));
    }
    spans
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn all_cases_cover_the_full_pair_space() {
        let cases = all_cases();
        assert_eq!(cases.len(), 818);
        assert!(cases.iter().all(|case| letters_of(case).len() == 2));
    }

    #[test]
    fn filter_matches_prefix_case_insensitively() {
        let all = all_cases();
        let ab = filter_cases(&all, "ab");
        assert_eq!(ab.len(), 2);
        assert_eq!(letters_of(&ab[0]), "AB");
        assert_eq!(ab[0].orbit, Orbit::Edge);
        assert_eq!(ab[1].orbit, Orbit::Corner);
        assert_eq!(filter_cases(&all, "").len(), 818);
        assert!(filter_cases(&all, "z").is_empty());
    }

    #[test]
    fn list_window_keeps_the_selection_centered_and_clamped() {
        assert_eq!(list_window(818, 0, 10), 0..10);
        assert_eq!(list_window(818, 5, 10), 0..10);
        assert_eq!(list_window(818, 6, 10), 1..11);
        assert_eq!(list_window(818, 817, 10), 808..818);
        assert_eq!(list_window(3, 2, 10), 0..3);
    }

    #[test]
    fn list_capacity_fits_the_window_and_floors_at_one() {
        let mut config = AppConfig::default();
        config.window.height = 800;
        assert_eq!(list_capacity(&config), 21);
        config.window.height = 0;
        assert_eq!(list_capacity(&config), 1);
    }

    #[test]
    fn list_rows_mark_the_selected_row_with_a_caret() {
        let mut browser = AlgsBrowser::default();
        browser.filtered = browser.all.clone();
        browser.selected = 0;
        let rows = list_rows(&browser, 6);
        assert_eq!(rows.len(), 6);
        assert_eq!(rows[0].0, "> AB edge");
        assert!(rows[0].1);
        assert!(rows[1].0.starts_with("  ") && rows[1].0.ends_with(" edge"));
        assert!(!rows[1].1);
        browser.selected = 817;
        let tail = list_rows(&browser, 6);
        assert!(
            tail.last()
                .is_some_and(|(label, selected)| { label.starts_with('>') && *selected })
        );
    }

    #[test]
    fn list_rows_report_an_empty_match_set_once() {
        let browser = AlgsBrowser::default();
        assert_eq!(
            list_rows(&browser, 10),
            vec![("no case matches the query".to_string(), false)]
        );
    }

    #[test]
    fn pair_expand_rows_carry_the_cancelled_flat_form() {
        let cancelling = Row {
            solver: bt_data::corpus::Solver::Cherry,
            sheet: SheetClass::Edge,
            rowid: 1,
            position: "UF-UB-UR".to_string(),
            alg: bt_core::Alg::parse("U' R U R' U' U S U' U R U' R' U S'").unwrap(),
            alg_text: "U' R U R' U' U S U' U R U' R' U S'".to_string(),
            commutator: "[U':[R,U],S]".to_string(),
            recon_link: String::new(),
            youtube_link: String::new(),
        };
        assert_eq!(
            pair_expand_rows(&cancelling),
            vec!["expand U' R U R' S R U' R' U S'".to_string()]
        );
        let mut aligned = cancelling.clone();
        aligned.commutator = "R2 U':[S,R2]".to_string();
        aligned.alg = bt_core::Alg::parse("R2 U' S R2 S' R2 U R2").unwrap();
        aligned.alg_text = "R2 U' S R2 S' R2 U R2".to_string();
        assert_eq!(
            pair_expand_rows(&aligned),
            vec!["expand R2 U' S R2 S' R2 U R2".to_string()],
            "the mechanics branch shows no other flat form"
        );
        let mut placeholder = aligned.clone();
        placeholder.commutator = "Not found.".to_string();
        assert!(pair_expand_rows(&placeholder).is_empty());
        let mut junk = aligned.clone();
        junk.commutator = "junk text".to_string();
        assert!(pair_expand_rows(&junk).is_empty());
    }

    #[test]
    fn mechanic_spans_color_setup_insertion_and_interchange() {
        let alg = bt_core::Alg::parse("R2 U' S R2 S' R2 U R2").expect("fixture alg parses");
        let units = decompose_verified("R2 U':[S,R2]", &alg).expect("verified decomposition");
        assert_eq!(units.len(), 1);
        let spans = mechanic_spans(&units);
        assert_eq!(spans[0], ("R2 U': ".to_string(), theme::setup()));
        assert_eq!(spans[1], ("[".to_string(), theme::text_muted()));
        assert_eq!(spans[2], ("S".to_string(), theme::teal()));
        assert_eq!(spans[3], (",".to_string(), theme::text_muted()));
        assert_eq!(spans[4], ("R2".to_string(), theme::interchange()));
        assert_eq!(spans[5], ("]".to_string(), theme::text_muted()));
        let mut joined = units.clone();
        joined.push(units[0].clone());
        assert_eq!(
            mechanic_spans(&joined)[6],
            (" + ".to_string(), theme::text_muted())
        );
    }
}

#[derive(Component, Default)]
pub struct AlgsDetailLine;

#[derive(Component, Default)]
pub struct AlgsDetail;

#[derive(Component, Default)]
pub struct AlgsListRow(pub usize);

#[derive(Component, Default)]
pub struct AlgsQueryBox;

pub fn spawn(
    commands: &mut Commands,
    fonts: &crate::present::FontBook,
    config: &AppConfig,
) -> Entity {
    let content = crate::present::spawn_view_frame(
        commands,
        fonts,
        AppState::Algs,
        "algs",
        "type letters to search, arrows select, left right browse packs, o edits a case, enter saves, tab cancels, x deletes the override, d drills, esc menu",
    );
    commands.entity(content).insert(Node {
        width: Val::Percent(100.0),
        flex_grow: 1.0,
        flex_direction: FlexDirection::Row,
        column_gap: theme::rem(theme::SPACING_REM * 2.0),
        ..default()
    });
    let left = commands
        .spawn((
            Node {
                width: Val::Percent(40.0),
                flex_direction: FlexDirection::Column,
                row_gap: theme::rem(theme::SPACING_REM),
                ..default()
            },
            ChildOf(content),
        ))
        .id();
    commands.spawn((
        AlgsDetail,
        Node {
            flex_grow: 1.0,
            flex_direction: FlexDirection::Column,
            row_gap: theme::rem(theme::SPACING_REM * 0.5),
            ..default()
        },
        ChildOf(content),
    ));
    commands
        .spawn_scene(bsn! {
            @FeathersTextInputContainer
            Node { width: px(160.0), flex_grow: 0.0 }
            Children [
                @FeathersTextInput
            ]
        })
        .insert(AlgsQueryBox)
        .insert(ChildOf(left));
    commands.entity(left).with_children(|column| {
        for index in 0..list_capacity(config) {
            crate::present::spawn_line(
                column,
                fonts,
                "",
                theme::FONT_BODY,
                theme::text_muted(),
                AlgsListRow(index),
            );
        }
    });
    content
}

fn spawn_detail(
    detail: &mut ChildSpawnerCommands,
    fonts: &crate::present::FontBook,
    corpus: &Corpus,
    browser: &AlgsBrowser,
) {
    crate::present::spawn_line(
        detail,
        fonts,
        &format!(
            "{} of {} cases match",
            browser.filtered.len(),
            browser.all.len()
        ),
        theme::FONT_MICRO,
        theme::text_muted(),
        AlgsDetailLine,
    );
    let Some(case) = browser.filtered.get(browser.selected) else {
        crate::present::spawn_line(
            detail,
            fonts,
            "no case matches the query",
            theme::FONT_SMALL,
            theme::text_muted(),
            AlgsDetailLine,
        );
        return;
    };
    let case_covered = covered(corpus, case);
    let rows = case_rows(corpus, case);
    crate::present::spawn_line(
        detail,
        fonts,
        &letters_of(case),
        theme::FONT_TITLE,
        theme::text_primary(),
        AlgsDetailLine,
    );
    crate::present::spawn_line(
        detail,
        fonts,
        &format!(
            "{} {} {}",
            orbit_word(case.orbit),
            case_position(case),
            if case_covered {
                "covered yes"
            } else {
                "covered no"
            }
        ),
        theme::FONT_SMALL,
        if case_covered {
            theme::ok()
        } else {
            theme::danger()
        },
        AlgsDetailLine,
    );
    if rows.is_empty() {
        crate::present::spawn_line(
            detail,
            fonts,
            "no vendor algs",
            theme::FONT_SMALL,
            theme::text_muted(),
            AlgsDetailLine,
        );
    }
    for (index, row) in rows.iter().enumerate() {
        if index == 0 {
            match decompose_verified(&row.commutator, &row.alg) {
                Some(units) => {
                    let spans = mechanic_spans(&units);
                    let refs: Vec<(&str, f32, Color)> = spans
                        .iter()
                        .map(|(text, color)| (text.as_str(), theme::FONT_ALG, *color))
                        .collect();
                    crate::present::spawn_spans(detail, fonts, &refs);
                    for line in pair_expand_rows(row) {
                        crate::present::spawn_line(
                            detail,
                            fonts,
                            &line,
                            theme::FONT_SMALL,
                            theme::text_muted(),
                            AlgsDetailLine,
                        );
                    }
                }
                None => {
                    crate::present::spawn_line(
                        detail,
                        fonts,
                        &row.alg_text,
                        theme::FONT_ALG,
                        theme::text_primary(),
                        AlgsDetailLine,
                    );
                    if has_commutator(&row.commutator) {
                        crate::present::spawn_line(
                            detail,
                            fonts,
                            &format!("commutator {}", row.commutator),
                            theme::FONT_SMALL,
                            theme::text_muted(),
                            AlgsDetailLine,
                        );
                    }
                    for line in pair_expand_rows(row) {
                        crate::present::spawn_line(
                            detail,
                            fonts,
                            &line,
                            theme::FONT_SMALL,
                            theme::text_muted(),
                            AlgsDetailLine,
                        );
                    }
                }
            }
        } else {
            crate::present::spawn_line(
                detail,
                fonts,
                &row.alg_text,
                theme::FONT_BODY,
                theme::text_muted(),
                AlgsDetailLine,
            );
        }
        crate::present::spawn_line(
            detail,
            fonts,
            &meta_line(row),
            theme::FONT_SMALL,
            theme::text_muted(),
            AlgsDetailLine,
        );
        crate::present::spawn_line(
            detail,
            fonts,
            &format!("recon {} video {}", row.recon_link, row.youtube_link),
            theme::FONT_MICRO,
            theme::text_muted(),
            AlgsDetailLine,
        );
    }
}

pub fn read_query(
    mut query: ResMut<AlgsQuery>,
    boxes: Query<&Children, With<AlgsQueryBox>>,
    inputs: Query<&bevy::text::EditableText, Without<AlgsQueryBox>>,
) {
    for children in &boxes {
        for child in children.iter() {
            if let Ok(editable) = inputs.get(child) {
                let text = editable.value().to_string();
                if text != query.0 {
                    query.0 = text;
                }
            }
        }
    }
}

#[allow(clippy::too_many_arguments)]
pub fn bind_detail(
    mut commands: Commands,
    corpus: Res<VendorCorpus>,
    repo: Res<browse::RepoBrowser>,
    store: Res<detail::OverrideStore>,
    draft: Res<detail::OverrideDraft>,
    browser: Res<AlgsBrowser>,
    config: Res<AppConfig>,
    fonts: Res<crate::present::FontBook>,
    rails: Query<(Entity, Option<&Children>), With<AlgsDetail>>,
    mut rows: Query<(&mut Text, &mut TextColor, &AlgsListRow)>,
) {
    if !browser.is_changed() && !repo.is_changed() && !store.is_changed() && !draft.is_changed() {
        return;
    }
    let rendered = if repo.repo_focused {
        browse::repo_list_rows(&repo, list_capacity(&config))
    } else {
        list_rows(&browser, list_capacity(&config))
    };
    for (mut text, mut color, row) in &mut rows {
        match rendered.get(row.0) {
            Some((label, selected)) => {
                *text = Text::new(label.clone());
                color.0 = if *selected {
                    theme::text_primary()
                } else {
                    theme::text_muted()
                };
            }
            None => *text = Text::new(String::new()),
        }
    }
    for (rail, children) in &rails {
        if let Some(children) = children {
            for child in children.iter() {
                commands.entity(child).despawn();
            }
        }
        commands.entity(rail).with_children(|parent| {
            if repo.repo_focused {
                detail::spawn_seed_detail(parent, &fonts, &repo, &store, &draft);
            } else {
                spawn_detail(parent, &fonts, &corpus.0, &browser);
            }
        });
    }
}
