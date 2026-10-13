use super::drill_view::{
    CardPanel, DrillCardRoot, RatePrompt, RepsField, RepsTimer, refresh, spawn_card,
    spawn_rate_panel, spawn_reps_panel,
};
use crate::logic::{NetState, SaveStores, Sm2, VendorCorpus};
use crate::persist::Stores;
use crate::present::FontBook;
use crate::states::DrillPhase;
use crate::theme;
use crate::theme::FONT_ALG;
use crate::views::{PendingDrill, SyllableBook};
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bt_core::{Alg, Cube};
use bt_data::commutator::decompose_verified;
use bt_data::corpus::{Corpus, Row, SheetClass, Solver};
use bt_data::srs::{Card, Rating};
use bt_scheme::{
    CORNER_BUFFER_STICKER, CaseId, EDGE_BUFFER_STICKER, Orbit, Rng, enumerate_cases, name_of,
};

pub use super::drill_view::spawn;

#[derive(Clone, Copy, Debug, Resource, Default)]
pub struct DrillSeed(pub u64);

#[derive(Clone, Debug)]
pub struct DrillSide {
    pub case: CaseId,
    pub position: String,
    pub alg: Alg,
    pub alg_text: String,
    pub commutator: String,
    pub solver_label: String,
    pub verified: bool,
    pub vendor: bool,
}

impl DrillSide {
    pub fn letters(&self) -> String {
        let (first, second) = self.case.letters();
        format!("{first}{second}")
    }
}

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

fn row_to_side(case: &CaseId, row: &Row) -> DrillSide {
    DrillSide {
        case: *case,
        position: case_position(case),
        alg: row.alg.clone(),
        alg_text: row.alg_text.clone(),
        commutator: row.commutator.clone(),
        solver_label: row.solver.dir().to_string(),
        verified: bt_data::verify::verify(&row.alg, &row.position, row.sheet).ok(),
        vendor: row.solver != Solver::User,
    }
}

fn inverse_side(a: &DrillSide) -> DrillSide {
    let flipped = flip_case(&a.case);
    let alg = a.alg.inverse();
    DrillSide {
        position: case_position(&flipped),
        alg_text: alg.format(),
        alg,
        case: flipped,
        commutator: String::new(),
        solver_label: "inverse".to_string(),
        verified: true,
        vendor: false,
    }
}

pub fn build_sides(corpus: &Corpus, case: &CaseId) -> Option<(DrillSide, DrillSide)> {
    let sheet = sheet_of(case.orbit);
    let position = case_position(case);
    let rows = corpus.rows_for_position(sheet, &position);
    let row = rows.first()?;
    let a = row_to_side(case, row);
    let flipped = flip_case(case);
    let flip_rows = corpus.rows_for_position(sheet, &case_position(&flipped));
    let mut b = match flip_rows.first() {
        Some(flip_row) => row_to_side(&flipped, flip_row),
        None => inverse_side(&a),
    };
    let mut check = Cube::solved();
    check.apply_alg(&a.alg);
    check.apply_alg(&b.alg);
    if !check.is_solved() {
        b = inverse_side(&a);
    }
    Some((a, b))
}

fn reset_counters(session: &mut DrillSession) {
    session.reps_done = 0;
    session.reps_side_a = true;
    session.reps_clock = 0.0;
    session.last_split_at = 0.0;
    session.splits.clear();
    session.rate_step = 0;
}

fn install_pair(session: &mut DrillSession, corpus: &Corpus, stores: &Stores, case: CaseId) {
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

fn due_label(stores: &Stores, sheet: SheetClass, position: &str, now_day: i64) -> &'static str {
    match stores.srs.cards.get(&card_key(sheet, position)) {
        None => "new",
        Some(card) if card.is_due(now_day) => "due",
        Some(_) => "ok",
    }
}

pub fn memo_token(book: &SyllableBook, case: &CaseId) -> String {
    let (first, second) = case.letters();
    let pair = [first, second];
    let letters: String = pair.iter().collect();
    match case.orbit {
        Orbit::Edge => book
            .syllable_for(crate::views::memo::orbit_name(Orbit::Edge), pair)
            .map(str::to_string)
            .unwrap_or(letters),
        Orbit::Corner => crate::views::images::image_for(pair)
            .map(str::to_string)
            .unwrap_or(letters),
    }
}

fn push_span(spans: &mut Vec<(String, f32, Color)>, text: impl Into<String>, color: Color) {
    spans.push((text.into(), FONT_ALG, color));
}

pub fn mechanic_spans(commutator: &str, alg: &Alg) -> Option<Vec<(String, f32, Color)>> {
    let units = decompose_verified(commutator, alg)?;
    let mut spans = Vec::new();
    for (index, unit) in units.iter().enumerate() {
        if index > 0 {
            push_span(&mut spans, " + ", theme::text_primary());
        }
        if let Some(setup) = &unit.setup {
            push_span(&mut spans, format!("{}: ", setup.format()), theme::setup());
        }
        push_span(&mut spans, "[", theme::text_primary());
        let a_text = unit.a.format();
        let b_text = unit.b.format();
        if unit.b_first {
            push_span(&mut spans, b_text, theme::interchange());
            push_span(&mut spans, ", ", theme::text_primary());
            push_span(&mut spans, a_text, theme::teal());
        } else {
            push_span(&mut spans, a_text, theme::teal());
            push_span(&mut spans, ", ", theme::text_primary());
            push_span(&mut spans, b_text, theme::interchange());
        }
        push_span(&mut spans, "]", theme::text_primary());
    }
    Some(spans)
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum RowStyle {
    Focal,
    Muted,
    Heading,
    Alg,
    Body,
    Small,
    Micro,
}

pub struct CardContent {
    pub rows: Vec<(String, RowStyle)>,
    pub mechanics: Option<Vec<(String, f32, Color)>>,
}

pub fn card_content(
    session: &DrillSession,
    corpus: &Corpus,
    book: &SyllableBook,
    stores: &Stores,
    now_day: i64,
) -> CardContent {
    let Some((a, b)) = session.sides() else {
        let hint = match session.pin {
            Some(pin) => {
                let (first, second) = pin.letters();
                format!("pair {first}{second} uncovered, esc menu")
            }
            None => "no covered pairs, esc menu".to_string(),
        };
        return CardContent {
            rows: vec![(hint, RowStyle::Muted)],
            mechanics: None,
        };
    };
    let sheet = sheet_of(a.case.orbit);
    let mechanics = mechanic_spans(&a.commutator, &a.alg);
    let mut rows = vec![
        (a.letters(), RowStyle::Focal),
        (
            format!(
                "{} {}",
                crate::views::memo::orbit_name(a.case.orbit),
                a.position
            ),
            RowStyle::Muted,
        ),
        (
            format!("memo {}", memo_token(book, &a.case)),
            RowStyle::Heading,
        ),
        (a.alg_text.clone(), RowStyle::Alg),
    ];
    if mechanics.is_none() && has_commutator(&a.commutator) {
        rows.push((format!("commutator {}", a.commutator), RowStyle::Small));
    }
    rows.push((
        format!("prepare {} {}: {}", b.letters(), b.solver_label, b.alg_text),
        RowStyle::Body,
    ));
    rows.push((
        format!(
            "{} qtm {} htm {} {}",
            a.alg.qtm(),
            a.alg.htm(),
            a.solver_label,
            if a.verified { "verified" } else { "unverified" }
        ),
        RowStyle::Small,
    ));
    rows.push((
        format!(
            "srs {} {}, {} {}",
            a.letters(),
            due_label(stores, sheet, &a.position, now_day),
            b.letters(),
            due_label(stores, sheet, &b.position, now_day)
        ),
        RowStyle::Small,
    ));
    for row in corpus.rows_for_position(sheet, &a.position).iter().skip(1) {
        rows.push((
            format!("alt {} {}", row.solver, row.alg_text),
            RowStyle::Micro,
        ));
    }
    CardContent { rows, mechanics }
}

pub fn card_lines(
    session: &DrillSession,
    corpus: &Corpus,
    book: &SyllableBook,
    stores: &Stores,
    now_day: i64,
) -> Vec<String> {
    card_content(session, corpus, book, stores, now_day)
        .rows
        .into_iter()
        .map(|(text, _)| text)
        .collect()
}

pub fn reps_lines(session: &DrillSession) -> Vec<String> {
    match session.current_side() {
        Some(side) => vec![
            format!("rep {} of {}", session.reps_done, session.reps_target),
            format!("{} {}", side.letters(), side.alg_text),
            format!("{}s", session.reps_clock as u32),
        ],
        None => vec!["no pair".to_string()],
    }
}

pub fn rate_lines(session: &DrillSession) -> Vec<String> {
    let Some((a, b)) = session.sides() else {
        return vec!["no pair".to_string()];
    };
    let side = if session.rate_step == 0 { a } else { b };
    let total: f32 = session.splits.iter().map(|(_, secs)| secs).sum();
    let avg = if session.splits.is_empty() {
        0.0
    } else {
        total / session.splits.len() as f32
    };
    vec![
        format!("rate {}", side.letters()),
        format!("{} reps avg {:.1}s", session.splits.len(), avg),
        "1 again 2 hard 3 good 4 easy".to_string(),
    ]
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
    sm2: Res<Sm2>,
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
        write_review(&mut stores, &sm2.0, sheet_of(orbit), position, rating, day);
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

    fn book_with(entry: Option<(&str, &str, &str)>) -> SyllableBook {
        let mut book = SyllableBook::default();
        if let Some((orbit, letters, syllable)) = entry {
            book.entries.insert(
                (orbit.to_string(), letters.to_string()),
                syllable.to_string(),
            );
        }
        book
    }

    fn single_pair_corpus(alg: &str, commutator: &str) -> Corpus {
        Corpus::from_rows(vec![edge_row("UF-UB-UR", alg, commutator)])
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
    fn side_b_uses_the_vendor_flip_when_it_alternates() {
        let alg = "R2 U' S R2 S' R2 U R2";
        let inverse = Alg::parse(alg).expect("alg parses").inverse().format();
        let corpus = Corpus::from_rows(vec![
            edge_row("UF-UB-UR", alg, ""),
            row(Solver::Eggins, SheetClass::Edge, "UF-UR-UB", &inverse, ""),
        ]);
        let (a, b) = build_sides(&corpus, &edge_case('A', 'B')).expect("sides built");
        assert_eq!(a.solver_label, "cherry");
        assert!(a.vendor && b.vendor);
        assert_eq!(b.solver_label, "eggins");
        assert_eq!(b.letters(), "BA");
        assert_eq!(b.position, "UF-UR-UB");
    }

    #[test]
    fn side_b_falls_back_to_the_inverse_without_flip_coverage() {
        let (a, b) = build_sides(
            &single_pair_corpus("R2 U' S R2 S' R2 U R2", ""),
            &edge_case('A', 'B'),
        )
        .expect("sides built");
        assert!(!b.vendor);
        assert_eq!(b.solver_label, "inverse");
        assert_eq!(b.alg, a.alg.inverse());
        assert_eq!(b.alg_text, a.alg.inverse().format());
        assert!(b.verified);
    }

    #[test]
    fn install_guard_replaces_a_flip_that_breaks_alternation() {
        let corpus = Corpus::from_rows(vec![
            edge_row("UF-UB-UR", "R2 U' S R2 S' R2 U R2", ""),
            edge_row("UF-UR-UB", "R U R' U'", ""),
        ]);
        let (a, b) = build_sides(&corpus, &edge_case('A', 'B')).expect("sides built");
        assert!(!b.vendor, "broken flip must be replaced");
        assert_eq!(b.solver_label, "inverse");
        let mut cube = Cube::solved();
        cube.apply_alg(&a.alg);
        cube.apply_alg(&b.alg);
        assert!(cube.is_solved(), "replacement must restore alternation");
    }

    #[test]
    fn install_pair_prepares_the_drilling_state() {
        let corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2", "");
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

    #[test]
    fn card_content_carries_the_full_pair_card() {
        let alg = "R2 U' S R2 S' R2 U R2";
        let parsed = Alg::parse(alg).expect("alg parses");
        let corpus = Corpus::from_rows(vec![
            edge_row("UF-UB-UR", alg, "R2 U':[S,R2]"),
            row(
                Solver::Eggins,
                SheetClass::Edge,
                "UF-UB-UR",
                "M2 U M2 U2 M U M2",
                "",
            ),
        ]);
        let stores = stores_with_reps(4);
        let mut session = DrillSession::default();
        install_pair(&mut session, &corpus, &stores, edge_case('A', 'B'));
        let content = card_content(
            &session,
            &corpus,
            &book_with(Some(("edge", "AB", "tả"))),
            &stores,
            0,
        );
        let inverse = parsed.inverse().format();
        let lines: Vec<String> = content.rows.iter().map(|(text, _)| text.clone()).collect();
        assert_eq!(
            lines,
            vec![
                "AB".to_string(),
                "edge UF-UB-UR".to_string(),
                "memo tả".to_string(),
                alg.to_string(),
                format!("prepare BA inverse: {inverse}"),
                format!("{} qtm {} htm cherry verified", parsed.qtm(), parsed.htm()),
                "srs AB new, BA new".to_string(),
                "alt eggins M2 U M2 U2 M U M2".to_string(),
            ]
        );
        let spans = content.mechanics.expect("verified commutator yields spans");
        assert_eq!(spans[0], ("R2 U': ".to_string(), FONT_ALG, theme::setup()));
        assert_eq!(spans[2], ("S".to_string(), FONT_ALG, theme::teal()));
        assert_eq!(spans[4], ("R2".to_string(), FONT_ALG, theme::interchange()));
    }

    #[test]
    fn card_skips_the_raw_commutator_row_when_undecomposable() {
        let stores = stores_with_reps(4);
        for commutator in ["Not found.", ""] {
            let corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2", commutator);
            let mut session = DrillSession::default();
            install_pair(&mut session, &corpus, &stores, edge_case('A', 'B'));
            let content = card_content(&session, &corpus, &book_with(None), &stores, 0);
            assert!(content.mechanics.is_none(), "commutator {commutator}");
            assert!(
                !content
                    .rows
                    .iter()
                    .any(|(text, _)| text.starts_with("commutator")),
                "placeholder must not render: {commutator}"
            );
            assert!(
                content
                    .rows
                    .iter()
                    .any(|(text, style)| *style == RowStyle::Alg && !text.is_empty()),
                "alg row stays: {commutator}"
            );
        }
    }

    #[test]
    fn card_lines_pick_tokens_per_orbit_with_letter_fallback() {
        let stores = stores_with_reps(4);
        let corner_corpus = Corpus::from_rows(vec![row(
            Solver::Cherry,
            SheetClass::Corner,
            "UFR-UBL-UBR",
            "R U' L' U",
            "",
        )]);
        let mut corner_session = DrillSession::default();
        install_pair(
            &mut corner_session,
            &corner_corpus,
            &stores,
            corner_case('A', 'B'),
        );
        let corner_lines = card_lines(
            &corner_session,
            &corner_corpus,
            &book_with(None),
            &stores,
            0,
        );
        assert_eq!(corner_lines[0], "AB");
        assert_eq!(corner_lines[1], "corner UFR-UBL-UBR");
        let word = crate::views::images::image_for(['A', 'B']).unwrap_or("AB");
        assert_eq!(corner_lines[2], format!("memo {word}"));
        let edge_corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2", "");
        let mut edge_session = DrillSession::default();
        install_pair(
            &mut edge_session,
            &edge_corpus,
            &stores,
            edge_case('A', 'B'),
        );
        let edge_lines = card_lines(&edge_session, &edge_corpus, &book_with(None), &stores, 0);
        assert_eq!(
            edge_lines[2], "memo AB",
            "letters fallback without syllable"
        );
    }

    #[test]
    fn reps_lines_show_the_executing_side_in_whole_seconds() {
        let mut session = DrillSession::default();
        install_pair(
            &mut session,
            &single_pair_corpus("R2 U' S R2 S' R2 U R2", ""),
            &stores_with_reps(4),
            edge_case('A', 'B'),
        );
        session.reps_done = 2;
        session.reps_side_a = false;
        session.reps_clock = 3.7;
        let inverse = session.side_b.as_ref().expect("side b").alg_text.clone();
        assert_eq!(
            reps_lines(&session),
            vec![
                "rep 2 of 4".to_string(),
                format!("BA {inverse}"),
                "3s".to_string(),
            ]
        );
    }

    #[test]
    fn rate_lines_prompt_each_side_in_order() {
        let mut session = DrillSession::default();
        install_pair(
            &mut session,
            &single_pair_corpus("R2 U' S R2 S' R2 U R2", ""),
            &stores_with_reps(2),
            edge_case('A', 'B'),
        );
        session.splits = vec![(true, 1.0), (false, 2.0)];
        assert_eq!(rate_lines(&session)[0], "rate AB");
        assert_eq!(rate_lines(&session)[1], "2 reps avg 1.5s");
        session.rate_step = 1;
        assert_eq!(rate_lines(&session)[0], "rate BA");
    }
}
