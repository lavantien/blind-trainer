use super::drill_view::{
    CardPanel, DrillCardRoot, RatePrompt, RepsField, RepsTimer, refresh, spawn_card,
    spawn_rate_panel, spawn_reps_panel,
};
use crate::logic::{Fsrs, NetState, SaveStores, VendorCorpus};
use crate::persist::Stores;
use crate::present::FontBook;
use crate::states::DrillPhase;
use crate::views::{PendingDrill, SyllableBook};
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bt_core::Cube;
use bt_data::corpus::{Corpus, SheetClass};
use bt_data::srs::{Card, Rating};
use bt_scheme::{
    CORNER_BUFFER_STICKER, CaseId, EDGE_BUFFER_STICKER, Orbit, Rng, enumerate_cases, name_of,
};

pub use super::drill_card::{
    CardContent, DrillSide, RowStyle, build_sides, card_content, card_lines, mechanic_spans,
    memo_token, rate_lines, reps_lines,
};
pub use super::drill_view::spawn;

#[derive(Clone, Copy, Debug, Resource, Default)]
pub struct DrillSeed(pub u64);

#[derive(Clone, Debug, Resource)]
pub struct DrillSession {
    pub side_a: Option<DrillSide>,
    pub side_b: Option<DrillSide>,
    pub pin: Option<CaseId>,
    pub reps_done: u32,
    pub reps_target: u32,
    pub reps_side_a: bool,
    pub reps_clock: f32,
    pub last_split_at: f32,
    pub splits: Vec<(bool, f32)>,
    pub rate_step: u8,
    pub prepared: Cube,
}

impl Default for DrillSession {
    fn default() -> DrillSession {
        DrillSession {
            side_a: None,
            side_b: None,
            pin: None,
            reps_done: 0,
            reps_target: 0,
            reps_side_a: true,
            reps_clock: 0.0,
            last_split_at: 0.0,
            splits: Vec::new(),
            rate_step: 0,
            prepared: Cube::solved(),
        }
    }
}

impl DrillSession {
    pub fn sides(&self) -> Option<(&DrillSide, &DrillSide)> {
        Some((self.side_a.as_ref()?, self.side_b.as_ref()?))
    }

    pub fn current_side(&self) -> Option<&DrillSide> {
        if self.reps_side_a {
            self.side_a.as_ref()
        } else {
            self.side_b.as_ref()
        }
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

pub fn flip_case(case: &CaseId) -> CaseId {
    CaseId {
        orbit: case.orbit,
        t1: case.t2,
        t2: case.t1,
    }
}

pub fn card_key(sheet: SheetClass, position: &str) -> String {
    format!("{}/{}", sheet.file(), position)
}

pub fn even_reps(n: u32) -> u32 {
    (n.saturating_add(n % 2).max(2)) & !1
}

pub fn has_commutator(text: &str) -> bool {
    !text.is_empty() && text != "Not found."
}

fn reset_counters(session: &mut DrillSession) {
    session.reps_done = 0;
    session.reps_side_a = true;
    session.reps_clock = 0.0;
    session.last_split_at = 0.0;
    session.splits.clear();
    session.rate_step = 0;
}

pub(crate) fn install_pair(
    session: &mut DrillSession,
    corpus: &Corpus,
    stores: &Stores,
    case: CaseId,
) {
    session.pin = Some(case);
    match build_sides(corpus, &case) {
        Some((a, b)) => {
            let mut prepared = Cube::solved();
            prepared.apply_alg(&b.alg);
            session.prepared = prepared;
            session.side_a = Some(a);
            session.side_b = Some(b);
            session.reps_target = even_reps(stores.settings.drill_reps);
        }
        None => {
            session.prepared = Cube::solved();
            session.side_a = None;
            session.side_b = None;
            session.reps_target = 0;
        }
    }
    reset_counters(session);
}

fn clear_pair(session: &mut DrillSession) {
    session.pin = None;
    session.side_a = None;
    session.side_b = None;
    session.prepared = Cube::solved();
    session.reps_target = 0;
    reset_counters(session);
}

pub fn pair_is_due(stores: &Stores, now_day: i64, case: &CaseId) -> bool {
    let sheet = sheet_of(case.orbit);
    let positions = [case_position(case), case_position(&flip_case(case))];
    let mut all_carded = true;
    let mut any_due = false;
    for position in &positions {
        match stores.srs.cards.get(&card_key(sheet, position)) {
            None => all_carded = false,
            Some(card) => any_due |= card.is_due(now_day),
        }
    }
    !all_carded || any_due
}

pub fn pick_pair(corpus: &Corpus, stores: &Stores, now_day: i64, rng: &mut Rng) -> Option<CaseId> {
    let mut tier_due: Vec<CaseId> = Vec::new();
    let mut tier_seen: Vec<CaseId> = Vec::new();
    for orbit in [Orbit::Edge, Orbit::Corner] {
        for case in enumerate_cases(orbit) {
            if corpus
                .rows_for_position(sheet_of(orbit), &case_position(&case))
                .is_empty()
            {
                continue;
            }
            if pair_is_due(stores, now_day, &case) {
                tier_due.push(case);
            } else {
                tier_seen.push(case);
            }
        }
    }
    let bucket = if tier_due.is_empty() {
        &tier_seen
    } else {
        &tier_due
    };
    if bucket.is_empty() {
        return None;
    }
    Some(bucket[rng.below(bucket.len() as u64) as usize])
}

pub fn write_review(
    stores: &mut Stores,
    fsrs: &bt_data::fsrs::FsrsConfig,
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
    card.review(rating, now_day, fsrs);
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

fn set_net(net: &mut NetState, cube: Cube, side: &DrillSide) {
    net.cube = cube;
    net.buffer_sticker = Some(buffer_of(side.case.orbit));
    net.targets = [Some(side.case.t1), Some(side.case.t2)];
}

fn clear_net(net: &mut NetState) {
    net.cube = Cube::solved();
    net.buffer_sticker = None;
    net.targets = [None, None];
}

pub fn sync_net(session: Res<DrillSession>, mut net: ResMut<NetState>) {
    match session.sides() {
        Some((a, _)) => set_net(&mut net, Cube::solved(), a),
        None => clear_net(&mut net),
    }
}

pub fn enter(
    mut pending: ResMut<PendingDrill>,
    mut session: ResMut<DrillSession>,
    corpus: Res<VendorCorpus>,
    stores: Res<Stores>,
    seed: Res<DrillSeed>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    let case = pending.0.take().or_else(|| {
        let mut rng = Rng::new(seed.0);
        pick_pair(&corpus.0, &stores, crate::views::now_day(), &mut rng)
    });
    match case {
        Some(case) => install_pair(&mut session, &corpus.0, &stores, case),
        None => clear_pair(&mut session),
    }
    next.set(DrillPhase::Card);
}

pub fn enter_card(
    mut messages: ResMut<Messages<KeyboardInput>>,
    session: Res<DrillSession>,
    mut net: ResMut<NetState>,
) {
    messages.clear();
    match session.sides() {
        Some((a, _)) => set_net(&mut net, Cube::solved(), a),
        None => clear_net(&mut net),
    }
}

pub fn enter_reps(
    mut messages: ResMut<Messages<KeyboardInput>>,
    mut commands: Commands,
    mut session: ResMut<DrillSession>,
    fonts: Option<Res<FontBook>>,
    roots: Query<Entity, With<DrillCardRoot>>,
    mut net: ResMut<NetState>,
) {
    messages.clear();
    session.reps_clock = 0.0;
    session.last_split_at = 0.0;
    if let Some(side) = session.current_side() {
        set_net(&mut net, session.prepared, side);
    }
    if let (Some(fonts), Some(root)) = (fonts.as_deref(), roots.iter().next()) {
        spawn_reps_panel(&mut commands, root, fonts, &session);
    }
}

pub fn enter_rate(
    mut messages: ResMut<Messages<KeyboardInput>>,
    mut commands: Commands,
    session: Res<DrillSession>,
    fonts: Option<Res<FontBook>>,
    roots: Query<Entity, With<DrillCardRoot>>,
) {
    messages.clear();
    if let (Some(fonts), Some(root)) = (fonts.as_deref(), roots.iter().next()) {
        spawn_rate_panel(&mut commands, root, fonts, &session);
    }
}

pub fn card_keys(
    mut inputs: MessageReader<KeyboardInput>,
    session: Res<DrillSession>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    if session.sides().is_none() {
        return;
    }
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && !input.repeat && input.key_code == KeyCode::Space
        {
            next.set(DrillPhase::Reps);
            return;
        }
    }
}

pub fn reps_tick(
    time: Res<Time<Virtual>>,
    mut session: ResMut<DrillSession>,
    mut timers: Query<&mut Text, With<RepsTimer>>,
) {
    session.reps_clock += time.delta_secs();
    refresh(
        timers.iter_mut(),
        &format!("{}s", session.reps_clock as u32),
    );
}

pub fn reps_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<DrillSession>,
    mut net: ResMut<NetState>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && !input.repeat && input.key_code == KeyCode::Space
        {
            if session.current_side().is_none() {
                return;
            }
            let split = (session.reps_clock - session.last_split_at).max(0.0);
            session.last_split_at = session.reps_clock;
            let executing_a = session.reps_side_a;
            session.splits.push((executing_a, split));
            if let Some(side) = session.current_side() {
                net.cube.apply_alg(&side.alg);
            }
            session.reps_side_a = !session.reps_side_a;
            session.reps_done += 1;
            if let Some(side) = session.current_side() {
                net.buffer_sticker = Some(buffer_of(side.case.orbit));
                net.targets = [Some(side.case.t1), Some(side.case.t2)];
            }
            if session.reps_done >= session.reps_target {
                next.set(DrillPhase::Rate);
            }
            return;
        }
    }
}

pub fn reps_bind(session: Res<DrillSession>, mut fields: Query<(&mut Text, &RepsField)>) {
    let Some(side) = session.current_side() else {
        return;
    };
    let shown_count = format!("rep {} of {}", session.reps_done, session.reps_target);
    for (mut text, field) in &mut fields {
        let shown = match field {
            RepsField::Letters => side.letters(),
            RepsField::Alg => side.alg_text.clone(),
            RepsField::Count => shown_count.clone(),
        };
        if text.0 != shown {
            *text = Text::new(shown);
        }
    }
}

#[allow(clippy::too_many_arguments)]
pub fn rate_keys(
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<DrillSession>,
    corpus: Res<VendorCorpus>,
    mut stores: ResMut<Stores>,
    fsrs: Res<Fsrs>,
    mut seed: ResMut<DrillSeed>,
    mut next: ResMut<NextState<DrillPhase>>,
) {
    let day = crate::views::now_day();
    let sides = session
        .sides()
        .map(|(a, b)| (a.case.orbit, a.position.clone(), b.position.clone()));
    let Some((orbit, position_a, position_b)) = sides else {
        return;
    };
    for input in inputs.read() {
        if input.state != ButtonState::Pressed || input.repeat {
            continue;
        }
        let Some(rating) = rating_of(input.key_code) else {
            continue;
        };
        let position = if session.rate_step == 0 {
            &position_a
        } else {
            &position_b
        };
        write_review(&mut stores, &fsrs.0, sheet_of(orbit), position, rating, day);
        commands.write_message(SaveStores);
        session.rate_step += 1;
        if session.rate_step > 1 {
            let mut rng = Rng::new(seed.0);
            let picked = pick_pair(&corpus.0, &stores, day, &mut rng);
            seed.0 = seed.0.wrapping_add(1);
            match picked {
                Some(case) => install_pair(&mut session, &corpus.0, &stores, case),
                None => clear_pair(&mut session),
            }
            next.set(DrillPhase::Card);
        }
        return;
    }
}

pub fn rate_bind(session: Res<DrillSession>, mut prompts: Query<&mut Text, With<RatePrompt>>) {
    let Some((a, b)) = session.sides() else {
        return;
    };
    let side = if session.rate_step == 0 { a } else { b };
    refresh(prompts.iter_mut(), &format!("rate {}", side.letters()));
}

#[allow(clippy::too_many_arguments)]
pub fn ensure_card(
    mut commands: Commands,
    session: Res<DrillSession>,
    corpus: Res<VendorCorpus>,
    stores: Res<Stores>,
    book: Res<SyllableBook>,
    fonts: Option<Res<FontBook>>,
    roots: Query<Entity, With<DrillCardRoot>>,
    panels: Query<(), With<CardPanel>>,
) {
    if panels.iter().next().is_some() {
        return;
    }
    let (Some(fonts), Some(root)) = (fonts.as_deref(), roots.iter().next()) else {
        return;
    };
    let content = card_content(&session, &corpus.0, &book, &stores, crate::views::now_day());
    spawn_card(&mut commands, root, fonts, &content);
}

#[cfg(test)]
mod tests {
    use super::*;
    use bt_core::Alg;
    use bt_data::corpus::{Row, Solver};

    fn edge_case(first: char, second: char) -> CaseId {
        CaseId {
            orbit: Orbit::Edge,
            t1: bt_scheme::sticker_of(Orbit::Edge, first),
            t2: bt_scheme::sticker_of(Orbit::Edge, second),
        }
    }

    fn corner_case(first: char, second: char) -> CaseId {
        CaseId {
            orbit: Orbit::Corner,
            t1: bt_scheme::sticker_of(Orbit::Corner, first),
            t2: bt_scheme::sticker_of(Orbit::Corner, second),
        }
    }

    fn row(solver: Solver, sheet: SheetClass, position: &str, alg: &str, commutator: &str) -> Row {
        Row {
            solver,
            sheet,
            rowid: 1,
            position: position.to_string(),
            alg: Alg::parse(alg).expect("fixture alg parses"),
            alg_text: alg.to_string(),
            commutator: commutator.to_string(),
            recon_link: String::new(),
            youtube_link: String::new(),
        }
    }

    fn edge_row(position: &str, alg: &str, commutator: &str) -> Row {
        row(Solver::Cherry, SheetClass::Edge, position, alg, commutator)
    }

    fn stores_with_reps(reps: u32) -> Stores {
        Stores {
            settings: crate::persist::Settings {
                memo_cadence_ms: 800,
                memo_mode: crate::persist::MemoMode::Syllables,
                drill_reps: reps,
            },
            srs: crate::persist::SrsStore::default(),
            solves: crate::persist::SolveLog::default(),
            user_algs: bt_data::paths::UserAlgs::default(),
        }
    }

    fn single_pair_corpus(alg: &str) -> Corpus {
        Corpus::from_rows(vec![edge_row("UF-UB-UR", alg, "")])
    }

    #[test]
    fn case_position_spells_buffer_t1_t2() {
        assert_eq!(case_position(&edge_case('A', 'B')), "UF-UB-UR");
        assert_eq!(case_position(&corner_case('A', 'B')), "UFR-UBL-UBR");
        assert_eq!(case_position(&flip_case(&edge_case('A', 'B'))), "UF-UR-UB");
    }

    #[test]
    fn rating_keys_map_1_to_4() {
        assert_eq!(rating_of(KeyCode::Digit1), Some(Rating::Again));
        assert_eq!(rating_of(KeyCode::Digit2), Some(Rating::Hard));
        assert_eq!(rating_of(KeyCode::Digit3), Some(Rating::Good));
        assert_eq!(rating_of(KeyCode::Digit4), Some(Rating::Easy));
        assert_eq!(rating_of(KeyCode::KeyR), None);
    }

    #[test]
    fn even_reps_rounds_up_and_floors_at_two() {
        assert_eq!([0, 1, 2, 3, 4, 5, 6].map(even_reps), [2, 2, 2, 4, 4, 6, 6]);
    }

    #[test]
    fn even_reps_survives_the_u32_maximum() {
        let reps = even_reps(u32::MAX);
        assert_eq!(reps % 2, 0);
        assert!(reps >= 2);
    }

    #[test]
    fn install_pair_prepares_the_drilling_state() {
        let corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2");
        let stores = stores_with_reps(5);
        let mut session = DrillSession::default();
        install_pair(&mut session, &corpus, &stores, edge_case('A', 'B'));
        assert_eq!(session.reps_target, 6);
        assert_eq!(session.reps_target, even_reps(5));
        let mut expected = Cube::solved();
        expected.apply_alg(&session.side_b.as_ref().expect("side b").alg);
        assert_eq!(session.prepared, expected);
        assert!(!session.prepared.is_solved());
        let mut solved_again = session.prepared;
        solved_again.apply_alg(&session.side_a.as_ref().expect("side a").alg);
        assert!(solved_again.is_solved());
        assert!(session.reps_side_a);
        assert_eq!(session.reps_done, 0);
        assert!(session.splits.is_empty());
        install_pair(
            &mut session,
            &Corpus::from_rows(Vec::new()),
            &stores,
            edge_case('A', 'B'),
        );
        assert!(session.sides().is_none());
        assert_eq!(session.pin, Some(edge_case('A', 'B')));
        assert!(session.prepared.is_solved());
        assert_eq!(session.reps_target, 0);
    }
}
