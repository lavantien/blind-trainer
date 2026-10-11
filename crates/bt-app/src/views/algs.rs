use crate::logic::{NavIntent, NetState, VendorCorpus};
use crate::states::AppState;
use crate::views::PendingDrill;
use crate::views::drill::{buffer_of, case_position};
use bevy::feathers::controls::{FeathersTextInput, FeathersTextInputContainer};
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bt_core::Cube;
use bt_data::corpus::{Corpus, Row, SheetClass};
use bt_scheme::{CaseId, Orbit, enumerate_cases};

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

pub fn row_lines(row: &Row) -> Vec<String> {
    let verdict = bt_data::verify::verify(&row.alg, &row.position, row.sheet);
    let badge = if verdict.ok() {
        "verified".to_string()
    } else {
        format!("{} problem(s)", verdict.problems.len())
    };
    vec![
        format!(
            "{} {} qtm {} htm {} [{}]",
            row.solver,
            row.alg_text,
            row.qtm(),
            row.htm(),
            badge
        ),
        format!("commutator {}", row.commutator),
        format!("recon {} video {}", row.recon_link, row.youtube_link),
    ]
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
    net.target_sticker = Some(case.t1);
}

pub fn list_keys(
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    focus: Option<Res<bevy::input_focus::InputFocus>>,
    mut browser: ResMut<AlgsBrowser>,
    mut net: ResMut<NetState>,
) {
    if focus.is_some_and(|focus| focus.get().is_some()) {
        return;
    }
    let mut step = 0i32;
    let mut drill = false;
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        match input.key_code {
            KeyCode::ArrowDown | KeyCode::KeyN => step += 1,
            KeyCode::ArrowUp | KeyCode::KeyP => step -= 1,
            KeyCode::Enter | KeyCode::KeyD => drill = true,
            _ => {}
        }
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

pub fn detail_lines(corpus: &Corpus, browser: &AlgsBrowser) -> Vec<String> {
    let mut lines = vec![format!(
        "{} of {} cases match",
        browser.filtered.len(),
        browser.all.len()
    )];
    match browser.filtered.get(browser.selected) {
        None => lines.push("no case matches the query".to_string()),
        Some(case) => {
            lines.push(format!(
                "selected {} {} {} covered {}",
                letters_of(case),
                if case.orbit == Orbit::Edge {
                    "edge"
                } else {
                    "corner"
                },
                case_position(case),
                if covered(corpus, case) { "yes" } else { "no" }
            ));
            let rows = case_rows(corpus, case);
            if rows.is_empty() {
                lines.push("no vendor algs".to_string());
            }
            for row in rows {
                lines.extend(row_lines(row));
            }
        }
    }
    lines
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
}

#[derive(Component, Default)]
pub struct AlgsDetail;

#[derive(Component, Default)]
pub struct AlgsQueryBox;

pub fn spawn(
    commands: &mut Commands,
    fonts: &crate::present::FontBook,
    config: &crate::logic::AppConfig,
) -> Entity {
    let entity = crate::present::spawn_text_view(
        commands,
        fonts,
        AppState::Algs,
        "algs",
        &["type letters to search, up down select, d drills this case, esc menu"],
        Some(crate::present::net_geometry(config)),
    );
    commands.entity(entity).with_children(|parent| {
        crate::present::spawn_dynamic(parent, fonts, "", AlgsDetail);
    });
    commands
        .spawn_scene(bsn! {
            @FeathersTextInputContainer
            Node { width: px(160.0), flex_grow: 0.0 }
            Children [
                @FeathersTextInput
            ]
        })
        .insert(AlgsQueryBox)
        .insert(ChildOf(entity));
    entity
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

pub fn sync_query(
    query: Res<AlgsQuery>,
    boxes: Query<&Children, With<AlgsQueryBox>>,
    mut inputs: Query<&mut bevy::text::EditableText, Without<AlgsQueryBox>>,
) {
    if !query.is_changed() {
        return;
    }
    for children in &boxes {
        for child in children.iter() {
            if let Ok(mut editable) = inputs.get_mut(child)
                && editable.value().to_string() != query.0
            {
                editable.editor_mut().set_text(&query.0);
            }
        }
    }
}

pub fn bind_detail(
    corpus: Res<VendorCorpus>,
    browser: Res<AlgsBrowser>,
    mut texts: Query<&mut Text, With<AlgsDetail>>,
) {
    if !browser.is_changed() {
        return;
    }
    let body = detail_lines(&corpus.0, &browser).join("\n");
    for mut text in &mut texts {
        *text = Text::new(body.clone());
    }
}
