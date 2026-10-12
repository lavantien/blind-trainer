use crate::logic::{NetState, SaveStores, Sm2, VendorCorpus};
use crate::persist::Stores;
use crate::states::{AppState, DrillPhase};
use crate::views::{NowDay, PendingDrill};
use bevy::feathers::controls::{FeathersTextInput, FeathersTextInputContainer};
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bt_core::Cube;
use bt_data::corpus::{Corpus, SheetClass};
use bt_data::srs::{Card, Rating};
use bt_scheme::{
    CORNER_BUFFER_STICKER, CaseId, EDGE_BUFFER_STICKER, Orbit, Rng, enumerate_cases, name_of,
};

const TARGET_SWAP_SECS: f32 = 0.6;

#[derive(Clone, Copy, Debug, Resource, Default)]
pub struct DrillSeed(pub u64);

#[derive(Clone, Debug, Default, Resource)]
pub struct DrillDraft(pub String);

#[derive(Clone, PartialEq, Eq, Debug)]
pub enum DrillVerdict {
    ParseFailed(String),
    Problems(Vec<String>),
    Ok,
}

impl DrillVerdict {
    pub fn is_ok(&self) -> bool {
        matches!(self, DrillVerdict::Ok)
    }

    pub fn lines(&self) -> Vec<String> {
        match self {
            DrillVerdict::Ok => vec!["correct".to_string()],
            DrillVerdict::ParseFailed(detail) => vec![format!("unparsable alg: {detail}")],
            DrillVerdict::Problems(problems) => problems.clone(),
        }
    }
}

#[derive(Clone, Debug, Resource)]
pub struct DrillSession {
    pub case: Option<CaseId>,
    pub position: String,
    pub buffer_sticker: u8,
    pub verdict: Option<DrillVerdict>,
    pub reps_done: u32,
    pub target_pair: Option<(u8, u8)>,
    pub target_swap_clock: f32,
}

impl Default for DrillSession {
    fn default() -> DrillSession {
        DrillSession {
            case: None,
            position: String::new(),
            buffer_sticker: EDGE_BUFFER_STICKER,
            verdict: None,
            reps_done: 0,
            target_pair: None,
            target_swap_clock: 0.0,
        }
    }
}

impl DrillSession {
    pub fn letters(&self) -> String {
        self.case
            .map(|case| {
                let (first, second) = case.letters();
                format!("{first}{second}")
            })
            .unwrap_or_default()
    }

    pub fn covered(&self, corpus: &Corpus) -> bool {
        self.case
            .map(|case| {
                !corpus
                    .rows_for_position(sheet_of(case.orbit), &self.position)
                    .is_empty()
            })
            .unwrap_or(false)
    }
}

pub fn sheet_of(orbit: Orbit) -> SheetClass {
    match orbit {
        Orbit::Edge => SheetClass::Edge,
        Orbit::Corner => SheetClass::Corner,
    }
}

pub fn buffer_of(orbit: Orbit) -> u8 {
    match orbit {
        Orbit::Edge => EDGE_BUFFER_STICKER,
        Orbit::Corner => CORNER_BUFFER_STICKER,
    }
}

pub fn case_position(case: &CaseId) -> String {
    format!(
        "{}-{}-{}",
        name_of(buffer_of(case.orbit)),
        name_of(case.t1),
        name_of(case.t2)
    )
}

pub fn card_key(sheet: SheetClass, position: &str) -> String {
    format!("{}/{}", sheet.file(), position)
}

pub fn evaluate(alg_text: &str, case: &CaseId) -> DrillVerdict {
    match bt_core::Alg::parse(alg_text.trim()) {
        Err(err) => DrillVerdict::ParseFailed(err.to_string()),
        Ok(alg) => {
            let verdict = bt_data::verify::verify(&alg, &case_position(case), sheet_of(case.orbit));
            if verdict.ok() {
                DrillVerdict::Ok
            } else {
                DrillVerdict::Problems(verdict.problems.iter().map(|p| p.to_string()).collect())
            }
        }
    }
}

pub fn selection_rank(corpus: &Corpus, stores: &Stores, now_day: i64, case: &CaseId) -> usize {
    let sheet = sheet_of(case.orbit);
    let position = case_position(case);
    let covered = !corpus.rows_for_position(sheet, &position).is_empty();
    let due = stores
        .srs
        .cards
        .get(&card_key(sheet, &position))
        .map(|card| card.is_due(now_day))
        .unwrap_or(true);
    match (due, covered) {
        (true, false) => 0,
        (true, true) => 1,
        (false, false) => 2,
        (false, true) => 3,
    }
}

pub fn pick_case(corpus: &Corpus, stores: &Stores, now_day: i64, rng: &mut Rng) -> CaseId {
    let mut buckets: [Vec<CaseId>; 4] = [Vec::new(), Vec::new(), Vec::new(), Vec::new()];
    for orbit in [Orbit::Edge, Orbit::Corner] {
        for case in enumerate_cases(orbit) {
            buckets[selection_rank(corpus, stores, now_day, &case)].push(case);
        }
    }
    let bucket = buckets
        .into_iter()
        .find(|bucket| !bucket.is_empty())
        .expect("pair space is never empty");
    bucket[rng.below(bucket.len() as u64) as usize]
}

pub fn write_review(
    stores: &mut Stores,
    sm2: &bt_data::srs::Sm2Config,
    sheet: SheetClass,
    position: &str,
    rating: Rating,
    now_day: i64,
) {
    let key = card_key(sheet, position);
    let card = stores
        .srs
        .cards
        .entry(key)
        .or_insert_with(|| Card::new(now_day));
    card.review(rating, now_day, sm2);
}

fn install_case(session: &mut DrillSession, case: CaseId) {
    session.case = Some(case);
    session.position = case_position(&case);
    session.buffer_sticker = buffer_of(case.orbit);
    session.verdict = None;
    session.target_pair = Some((case.t1, case.t2));
    session.target_swap_clock = 0.0;
}

fn apply_net(session: &DrillSession, net: &mut NetState) {
    let Some(case) = session.case else {
        return;
    };
    net.cube = Cube::solved();
    net.buffer_sticker = Some(session.buffer_sticker);
    net.targets = [Some(case.t1), Some(case.t2)];
}

pub fn sync_net(session: Res<DrillSession>, mut net: ResMut<NetState>) {
    apply_net(&session, &mut net);
}

pub fn enter(
    mut pending: ResMut<PendingDrill>,
    mut session: ResMut<DrillSession>,
    corpus: Res<VendorCorpus>,
    stores: Res<Stores>,
    now_day: Res<NowDay>,
    seed: Res<DrillSeed>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    let case = pending.0.take().unwrap_or_else(|| {
        let mut rng = Rng::new(seed.0);
        pick_case(&corpus.0, &stores, now_day.0, &mut rng)
    });
    install_case(&mut session, case);
    next.set(DrillPhase::Ready);
}

pub fn enter_ready(
    mut messages: ResMut<Messages<KeyboardInput>>,
    mut draft: ResMut<DrillDraft>,
    boxes: Query<&Children, With<DrillInputBox>>,
    mut inputs: Query<&mut bevy::text::EditableText, Without<DrillInputBox>>,
    mut session: ResMut<DrillSession>,
    mut net: ResMut<NetState>,
) {
    messages.clear();
    set_box_text(&boxes, &mut inputs, "");
    draft.0.clear();
    session.verdict = None;
    session.target_swap_clock = 0.0;
    apply_net(&session, &mut net);
}

pub fn enter_memo(mut messages: ResMut<Messages<KeyboardInput>>) {
    messages.clear();
}

pub fn enter_exec(mut messages: ResMut<Messages<KeyboardInput>>) {
    messages.clear();
}

pub fn enter_review(mut messages: ResMut<Messages<KeyboardInput>>) {
    messages.clear();
}

pub fn ready_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            next.set(DrillPhase::Memo);
            return;
        }
    }
}

pub fn input_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut draft: ResMut<DrillDraft>,
    boxes: Query<&Children, With<DrillInputBox>>,
    editables: Query<&bevy::text::EditableText, Without<DrillInputBox>>,
    mut session: ResMut<DrillSession>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Enter {
            for children in &boxes {
                for child in children.iter() {
                    if let Ok(editable) = editables.get(child) {
                        draft.0 = editable.value().to_string();
                    }
                }
            }
            if let Some(case) = session.case {
                session.verdict = Some(evaluate(&draft.0, &case));
            }
            next.set(DrillPhase::Exec);
            return;
        }
    }
}

pub fn verdict_keys(
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<DrillSession>,
    mut stores: ResMut<Stores>,
    sm2: Res<Sm2>,
    now_day: Res<NowDay>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    let Some(case) = session.case else {
        return;
    };
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        let Some(rating) = rating_of(input.key_code) else {
            continue;
        };
        write_review(
            &mut stores,
            &sm2.0,
            sheet_of(case.orbit),
            &session.position,
            rating,
            now_day.0,
        );
        commands.write_message(SaveStores);
        session.reps_done += 1;
        next.set(DrillPhase::Review);
        return;
    }
}

pub fn review_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<DrillSession>,
    corpus: Res<VendorCorpus>,
    stores: Res<Stores>,
    now_day: Res<NowDay>,
    mut seed: ResMut<DrillSeed>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            let mut rng = Rng::new(seed.0);
            let case = pick_case(&corpus.0, &stores, now_day.0, &mut rng);
            seed.0 = seed.0.wrapping_add(1);
            install_case(&mut session, case);
            next.set(DrillPhase::Ready);
            return;
        }
    }
}

pub fn swap_target(
    time: Res<Time<Virtual>>,
    mut session: ResMut<DrillSession>,
    mut net: ResMut<NetState>,
) {
    session.target_swap_clock += time.delta_secs();
    if session.target_swap_clock < TARGET_SWAP_SECS {
        return;
    }
    session.target_swap_clock = 0.0;
    if let Some((first, second)) = session.target_pair {
        let next = match net.targets[0] {
            Some(sticker) if sticker == first => second,
            _ => first,
        };
        net.targets = [Some(next), net.targets[1]];
    }
}

pub fn rating_of(key: KeyCode) -> Option<Rating> {
    match key {
        KeyCode::Digit1 => Some(Rating::Again),
        KeyCode::Digit2 => Some(Rating::Hard),
        KeyCode::Digit3 => Some(Rating::Good),
        KeyCode::Digit4 => Some(Rating::Easy),
        _ => None,
    }
}

pub fn session_lines(session: &DrillSession, corpus: &Corpus) -> Vec<String> {
    let mut lines = Vec::new();
    if let Some(case) = session.case {
        let (first, second) = case.letters();
        lines.push(format!(
            "pair {}{} {} {}",
            first,
            second,
            if case.orbit == Orbit::Edge {
                "edge"
            } else {
                "corner"
            },
            session.position
        ));
        lines.push(format!(
            "vendor coverage: {}",
            if session.covered(corpus) {
                "yes"
            } else {
                "none, bring your own alg"
            }
        ));
    }
    lines.push(format!("reps this session: {}", session.reps_done));
    if let Some(verdict) = &session.verdict {
        lines.push(String::new());
        lines.extend(verdict.lines());
    }
    lines
}

#[cfg(test)]
mod tests {
    use super::*;

    fn edge_case(first: char, second: char) -> CaseId {
        CaseId {
            orbit: Orbit::Edge,
            t1: bt_scheme::sticker_of(Orbit::Edge, first),
            t2: bt_scheme::sticker_of(Orbit::Edge, second),
        }
    }

    #[test]
    fn case_position_spells_buffer_t1_t2() {
        assert_eq!(case_position(&edge_case('A', 'B')), "UF-UB-UR");
        assert_eq!(
            case_position(&CaseId {
                orbit: Orbit::Corner,
                t1: bt_scheme::sticker_of(Orbit::Corner, 'A'),
                t2: bt_scheme::sticker_of(Orbit::Corner, 'B'),
            }),
            "UFR-UBL-UBR"
        );
    }

    #[test]
    fn evaluate_rejects_wrong_alg_with_named_problems() {
        let case = edge_case('A', 'B');
        let verdict = evaluate("R U", &case);
        assert!(!verdict.is_ok());
        assert!(!verdict.lines().is_empty());
        assert!(matches!(
            evaluate("qq", &case),
            DrillVerdict::ParseFailed(_)
        ));
    }

    fn empty_stores() -> Stores {
        Stores {
            settings: crate::persist::Settings {
                memo_cadence_ms: 800,
                memo_mode: crate::persist::MemoMode::Syllables,
                drill_reps: 5,
            },
            srs: crate::persist::SrsStore::default(),
            solves: crate::persist::SolveLog::default(),
            user_algs: bt_data::paths::UserAlgs::default(),
        }
    }

    #[test]
    fn pick_case_prefers_due_then_uncovered() {
        let corpus = Corpus::from_rows(Vec::new());
        let stores = empty_stores();
        let mut rng = Rng::new(7);
        for _ in 0..20 {
            let case = pick_case(&corpus, &stores, 0, &mut rng);
            assert_eq!(selection_rank(&corpus, &stores, 0, &case), 0);
        }
    }

    #[test]
    fn rating_keys_map_1_to_4() {
        assert_eq!(rating_of(KeyCode::Digit1), Some(Rating::Again));
        assert_eq!(rating_of(KeyCode::Digit2), Some(Rating::Hard));
        assert_eq!(rating_of(KeyCode::Digit3), Some(Rating::Good));
        assert_eq!(rating_of(KeyCode::Digit4), Some(Rating::Easy));
        assert_eq!(rating_of(KeyCode::KeyR), None);
    }
}

#[derive(Component, Default)]
pub struct DrillText;

#[derive(Component, Default)]
pub struct DrillInputBox;

pub fn spawn(
    commands: &mut Commands,
    fonts: &crate::present::FontBook,
    config: &crate::logic::AppConfig,
) -> Entity {
    let entity = crate::present::spawn_text_view(
        commands,
        fonts,
        AppState::Drill,
        "drill",
        &["space reveal, type alg, enter check, 1..4 rate, esc menu"],
        Some(crate::present::net_geometry(config)),
    );
    commands.entity(entity).with_children(|parent| {
        crate::present::spawn_dynamic(parent, fonts, "", DrillText);
    });
    commands
        .spawn_scene(bsn! {
            @FeathersTextInputContainer
            Node { width: px(460.0), flex_grow: 0.0 }
            Children [
                @FeathersTextInput
            ]
        })
        .insert(DrillInputBox)
        .insert(ChildOf(entity));
    entity
}

pub fn read_input(
    mut draft: ResMut<DrillDraft>,
    boxes: Query<&Children, With<DrillInputBox>>,
    mut inputs: Query<&mut bevy::text::EditableText, Without<DrillInputBox>>,
) {
    for children in &boxes {
        for child in children.iter() {
            if let Ok(editable) = inputs.get_mut(child) {
                let text = editable.value().to_string();
                if text != draft.0 {
                    draft.0 = text;
                }
            }
        }
    }
}

fn set_box_text(
    boxes: &Query<&Children, With<DrillInputBox>>,
    inputs: &mut Query<&mut bevy::text::EditableText, Without<DrillInputBox>>,
    text: &str,
) {
    for children in boxes {
        for child in children.iter() {
            if let Ok(mut editable) = inputs.get_mut(child)
                && editable.value() != text
            {
                editable.editor_mut().set_text(text);
            }
        }
    }
}

pub fn set_draft_and_box(world: &mut World, text: &str) {
    world.resource_mut::<DrillDraft>().0 = text.to_string();
    let mut boxes: Vec<Entity> = Vec::new();
    {
        let mut state = world.query_filtered::<&Children, With<DrillInputBox>>();
        for children in state.iter(world) {
            for child in children.iter() {
                boxes.push(child);
            }
        }
    }
    let mut state = world.query_filtered::<&mut bevy::text::EditableText, ()>();
    for child in boxes {
        if let Ok(mut editable) = state.get_mut(world, child)
            && editable.value() != text
        {
            editable.editor_mut().set_text(text);
        }
    }
}

pub fn focus_input_on_memo(
    phase: Res<State<DrillPhase>>,
    mut focus: ResMut<bevy::input_focus::InputFocus>,
    boxes: Query<&Children, With<DrillInputBox>>,
) {
    let in_memo = *phase.get() == DrillPhase::Memo;
    for children in &boxes {
        for child in children.iter() {
            let focused = focus.get() == Some(child);
            if in_memo && !focused {
                focus.set(child, bevy::input_focus::FocusCause::Navigated);
            } else if !in_memo && focused {
                focus.clear();
            }
        }
    }
}

pub fn bind_text(
    session: Res<DrillSession>,
    corpus: Res<VendorCorpus>,
    mut texts: Query<&mut Text, With<DrillText>>,
) {
    if !session.is_changed() {
        return;
    }
    let body = session_lines(&session, &corpus.0).join("\n");
    for mut text in &mut texts {
        *text = Text::new(body.clone());
    }
}
