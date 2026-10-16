use crate::logic::{Fsrs, SaveStores};
use crate::persist::{MemoMode, Stores};
use crate::states::{AppState, MemoPhase};
use crate::views::SyllableBook;
use crate::views::drill::{case_position, rating_of, sheet_of, write_review};
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bt_core::Cube;
use bt_data::srs::Rating;
use bt_scheme::memo::memo as memo_of;
use bt_scheme::{Memo, Orbit, name_of, random_state};

#[derive(Clone, Copy, Debug, Resource, Default)]
pub struct MemoSeed(pub u64);

#[derive(Clone, Debug)]
pub struct StreamToken {
    pub orbit: Orbit,
    pub letters: [char; 2],
    pub syllable: Option<String>,
    pub image: Option<&'static str>,
}

pub fn orbit_name(orbit: Orbit) -> &'static str {
    match orbit {
        Orbit::Edge => "edge",
        Orbit::Corner => "corner",
    }
}

pub fn build_tokens(memo: &Memo, book: &SyllableBook) -> Vec<StreamToken> {
    memo.pairs
        .0
        .iter()
        .chain(memo.pairs.1.iter())
        .map(|case| {
            let (first, second) = case.letters();
            let letters = [first, second];
            StreamToken {
                orbit: case.orbit,
                letters,
                syllable: book
                    .syllable_for(orbit_name(case.orbit), letters)
                    .map(str::to_string),
                image: super::images::image_for(letters),
            }
        })
        .collect()
}

pub fn token_text(token: &StreamToken, mode: MemoMode) -> String {
    let letters: String = token.letters.iter().collect();
    match mode {
        MemoMode::Syllables => token.syllable.clone().unwrap_or_else(|| letters.clone()),
        MemoMode::Images => token
            .image
            .map(str::to_string)
            .unwrap_or_else(|| letters.clone()),
        MemoMode::PerOrbit => match token.orbit {
            Orbit::Edge => token.syllable.clone().unwrap_or_else(|| letters.clone()),
            Orbit::Corner => token
                .image
                .map(str::to_string)
                .unwrap_or_else(|| letters.clone()),
        },
    }
}

#[derive(Clone, Debug, Resource)]
pub struct MemoSession {
    pub seed: u64,
    pub scramble: Cube,
    pub memo: Option<Memo>,
    pub tokens: Vec<StreamToken>,
    pub shown: usize,
    pub clock: f32,
}

impl Default for MemoSession {
    fn default() -> MemoSession {
        MemoSession {
            seed: 0,
            scramble: Cube::solved(),
            memo: None,
            tokens: Vec::new(),
            shown: 0,
            clock: 0.0,
        }
    }
}

impl MemoSession {
    pub fn cadence_secs(&self, stores: &Stores) -> f32 {
        stores.settings.memo_cadence_ms as f32 / 1000.0
    }

    pub fn reset_stream(&mut self) {
        self.shown = 0;
        self.clock = 0.0;
    }
}

pub fn rebuild(session: &mut MemoSession, seed: u64, book: &SyllableBook) {
    let scramble = random_state(seed);
    let memo = memo_of(&scramble);
    session.tokens = build_tokens(&memo, book);
    session.memo = Some(memo);
    session.scramble = scramble;
    session.seed = seed;
    session.reset_stream();
}

pub fn enter(
    mut session: ResMut<MemoSession>,
    seed: Res<MemoSeed>,
    book: Res<SyllableBook>,
    mut next: ResMut<NextState<MemoPhase>>,
) {
    rebuild(&mut session, seed.0, &book);
    next.set(MemoPhase::Intro);
}

pub fn enter_intro(mut messages: ResMut<Messages<KeyboardInput>>) {
    messages.clear();
}

pub fn enter_stream(
    mut messages: ResMut<Messages<KeyboardInput>>,
    mut session: ResMut<MemoSession>,
) {
    messages.clear();
    session.reset_stream();
}

pub fn enter_recall(mut messages: ResMut<Messages<KeyboardInput>>) {
    messages.clear();
}

pub fn enter_review(mut messages: ResMut<Messages<KeyboardInput>>) {
    messages.clear();
}

pub fn intro_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<MemoSession>,
    book: Res<SyllableBook>,
    mut next: ResMut<NextState<MemoPhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            let seed = session.seed.wrapping_add(1);
            session.seed = seed;
            rebuild(&mut session, seed, &book);
            next.set(MemoPhase::Stream);
            return;
        }
    }
}

pub fn stream(
    time: Res<Time<Virtual>>,
    stores: Res<Stores>,
    mut session: ResMut<MemoSession>,
    mut next: ResMut<NextState<MemoPhase>>,
) {
    let cadence = session.cadence_secs(&stores);
    if cadence <= 0.0 {
        session.shown = session.tokens.len();
    } else {
        session.clock += time.delta_secs();
        while session.clock >= cadence && session.shown < session.tokens.len() {
            session.clock -= cadence;
            session.shown += 1;
        }
    }
    if session.shown >= session.tokens.len() && !session.tokens.is_empty() {
        next.set(MemoPhase::Recall);
    }
}

pub fn recall_keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut next: ResMut<NextState<MemoPhase>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::Space {
            next.set(MemoPhase::Review);
            return;
        }
    }
}

pub fn review_keys(
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    session: Res<MemoSession>,
    mut stores: ResMut<Stores>,
    fsrs: Res<Fsrs>,
    mut next: ResMut<NextState<MemoPhase>>,
) {
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        match input.key_code {
            KeyCode::KeyR => {
                next.set(MemoPhase::Stream);
                return;
            }
            KeyCode::Space => {
                next.set(MemoPhase::Intro);
                return;
            }
            key => {
                let Some(rating) = rating_of(key) else {
                    continue;
                };
                rate_pairs(
                    &mut stores,
                    &fsrs.0,
                    &session,
                    rating,
                    crate::views::now_day(),
                );
                commands.write_message(SaveStores);
                next.set(MemoPhase::Intro);
                return;
            }
        }
    }
}

pub fn rate_pairs(
    stores: &mut Stores,
    fsrs: &bt_data::fsrs::FsrsConfig,
    session: &MemoSession,
    rating: Rating,
    now_day: i64,
) {
    let Some(memo) = &session.memo else {
        return;
    };
    for case in memo.pairs.0.iter().chain(memo.pairs.1.iter()) {
        let position = case_position(case);
        write_review(
            stores,
            fsrs,
            sheet_of(case.orbit),
            &position,
            rating,
            now_day,
        );
    }
}

pub fn residue_lines(memo: &Memo) -> Vec<String> {
    let mut lines = Vec::new();
    if let Some(parity) = &memo.residue.parity {
        lines.push(format!(
            "parity {}-{}, {}-{}",
            name_of(parity.edge.0),
            name_of(parity.edge.1),
            name_of(parity.corner.0),
            name_of(parity.corner.1)
        ));
    }
    for chunk in memo.residue.flips.chunks(2) {
        if let [first, second] = chunk {
            lines.push(format!("flip {} & {}", name_of(*first), name_of(*second)));
        }
    }
    for twist in &memo.residue.twists {
        lines.push(format!(
            "twist {} x{}",
            name_of(twist.corner),
            twist.quarter
        ));
    }
    if lines.is_empty() {
        lines.push("residue none".to_string());
    }
    lines
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::views::SyllableBook;

    fn empty_stores() -> Stores {
        Stores {
            settings: crate::persist::Settings {
                memo_cadence_ms: 800,
                memo_mode: MemoMode::PerOrbit,
                drill_reps: 6,
            },
            srs: crate::persist::SrsStore::default(),
            solves: crate::persist::SolveLog::default(),
            user_algs: bt_data::paths::UserAlgs::default(),
            seeds: Default::default(),
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

    fn sample_memo() -> Memo {
        memo_of(&random_state(12_345u64))
    }

    fn memo_with_edge_pair(first: char, second: char) -> Memo {
        for seed in 0..5_000u64 {
            let memo = memo_of(&random_state(seed));
            let hit = memo.pairs.0.iter().any(|case| {
                let (a, b) = case.letters();
                (a, b) == (first, second)
            });
            if hit {
                return memo;
            }
        }
        panic!("no seed produced edge pair {first}{second}");
    }

    #[test]
    fn tokens_fall_back_to_letters_without_syllables() {
        let memo = sample_memo();
        let tokens = build_tokens(&memo, &book_with(None));
        assert_eq!(tokens.len(), memo.pairs.0.len() + memo.pairs.1.len());
        for (case, token) in memo
            .pairs
            .0
            .iter()
            .chain(memo.pairs.1.iter())
            .zip(tokens.iter())
        {
            assert_eq!(token.orbit, case.orbit);
            let (first, second) = case.letters();
            let letters = format!("{first}{second}");
            assert_eq!(
                token_text(token, MemoMode::Syllables),
                letters,
                "syllable fallback must be the pair letters"
            );
        }
    }

    #[test]
    fn syllable_book_overrides_pair_text_when_present() {
        let mut book = book_with(None);
        book.entries
            .insert(("edge".to_string(), "AB".to_string()), "tả".to_string());
        let memo = memo_with_edge_pair('A', 'B');
        let tokens = build_tokens(&memo, &book);
        let ab = tokens
            .iter()
            .find(|token| token.letters == ['A', 'B'] && token.orbit == Orbit::Edge)
            .expect("edge AB in memo");
        assert_eq!(token_text(ab, MemoMode::Syllables), "tả");
    }

    #[test]
    fn image_mode_uses_words_when_available() {
        let memo = memo_with_edge_pair('A', 'B');
        let tokens = build_tokens(&memo, &book_with(None));
        let ab = tokens
            .iter()
            .find(|token| token.letters == ['A', 'B'] && token.orbit == Orbit::Edge)
            .expect("edge AB in memo");
        assert_eq!(token_text(ab, MemoMode::Images), "Anthony Brooks");
    }

    #[test]
    fn per_orbit_streams_edge_syllables_and_corner_images() {
        let edge = StreamToken {
            orbit: Orbit::Edge,
            letters: ['A', 'B'],
            syllable: Some("tả".to_string()),
            image: Some("Anthony Brooks"),
        };
        let corner = StreamToken {
            orbit: Orbit::Corner,
            letters: ['A', 'B'],
            syllable: Some("tả".to_string()),
            image: Some("Anthony Brooks"),
        };
        assert_eq!(token_text(&edge, MemoMode::PerOrbit), "tả");
        assert_eq!(token_text(&corner, MemoMode::PerOrbit), "Anthony Brooks");
    }

    #[test]
    fn per_orbit_falls_back_to_letters_when_the_orbit_book_lacks_the_pair() {
        let edge = StreamToken {
            orbit: Orbit::Edge,
            letters: ['C', 'D'],
            syllable: None,
            image: None,
        };
        let corner = StreamToken {
            orbit: Orbit::Corner,
            letters: ['E', 'F'],
            syllable: Some("bo".to_string()),
            image: None,
        };
        assert_eq!(token_text(&edge, MemoMode::PerOrbit), "CD");
        assert_eq!(token_text(&corner, MemoMode::PerOrbit), "EF");
    }

    #[test]
    fn stream_rows_center_the_current_token_with_trailing_recents() {
        let mut session = MemoSession {
            tokens: build_tokens(&sample_memo(), &book_with(None)),
            ..Default::default()
        };
        let stores = empty_stores();
        let mode = stores.settings.memo_mode;
        let shown = 3.min(session.tokens.len());
        session.shown = shown;
        let rows = view_rows(MemoPhase::Stream, &session, &stores);
        assert_eq!(rows.focal, token_text(&session.tokens[shown - 1], mode));
        let recents: Vec<String> = session.tokens[..shown - 1]
            .iter()
            .map(|token| token_text(token, mode))
            .collect();
        assert_eq!(rows.recent, recents.join(" "));
        assert!(
            rows.status.contains(&format!("{shown}/")),
            "status: {}",
            rows.status
        );
    }

    #[test]
    fn review_rows_list_every_token_with_residue_status() {
        let memo = sample_memo();
        let shown = memo.pairs.0.len() + memo.pairs.1.len();
        let session = MemoSession {
            tokens: build_tokens(&memo, &book_with(None)),
            memo: Some(memo),
            shown,
            ..Default::default()
        };
        let stores = empty_stores();
        let mode = stores.settings.memo_mode;
        let rows = view_rows(MemoPhase::Review, &session, &stores);
        let all: Vec<String> = session
            .tokens
            .iter()
            .map(|token| token_text(token, mode))
            .collect();
        assert_eq!(rows.recent, all.join(" "));
        assert!(
            rows.status
                .contains(&residue_lines(session.memo.as_ref().expect("memo")).join("; ")),
            "status: {}",
            rows.status
        );
        assert!(rows.status.contains("rate"), "status: {}", rows.status);
    }

    #[test]
    fn stream_advances_only_at_cadence() {
        let mut session = MemoSession::default();
        let stores = empty_stores();
        session.tokens = build_tokens(&sample_memo(), &book_with(None));
        assert!(!session.tokens.is_empty());
        session.clock = 0.0;
        session.shown = 0;
        session.clock += 0.5 * session.cadence_secs(&stores);
        assert!(session.clock < session.cadence_secs(&stores));
    }
}

pub const RECENT_TOKENS: usize = 4;

#[derive(Clone, Copy, Component, Debug, PartialEq, Eq)]
pub struct MemoLine(pub MemoLineKind);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MemoLineKind {
    Focal,
    Recent,
    Status,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MemoRows {
    pub focal: String,
    pub recent: String,
    pub status: String,
}

fn joined_texts(tokens: &[StreamToken], mode: MemoMode) -> String {
    tokens
        .iter()
        .map(|token| token_text(token, mode))
        .collect::<Vec<_>>()
        .join(" ")
}

fn trailing_before_current(session: &MemoSession, mode: MemoMode) -> String {
    let end = session.shown.saturating_sub(1);
    let start = end.saturating_sub(RECENT_TOKENS);
    joined_texts(&session.tokens[start..end], mode)
}

fn review_status(session: &MemoSession) -> String {
    let mut parts = Vec::new();
    if let Some(memo) = &session.memo {
        parts.extend(residue_lines(memo));
    }
    parts.push("r replay  space next  1..4 rate pairs".to_string());
    parts.join("; ")
}

pub fn view_rows(phase: MemoPhase, session: &MemoSession, stores: &Stores) -> MemoRows {
    let mode = stores.settings.memo_mode;
    let cadence = stores.settings.memo_cadence_ms;
    match phase {
        MemoPhase::Intro => MemoRows {
            focal: "space to stream".to_string(),
            recent: String::new(),
            status: format!(
                "seed {}  {} pairs  mode {}  cadence {} ms",
                session.seed,
                session.tokens.len(),
                mode.label(),
                cadence
            ),
        },
        MemoPhase::Stream => MemoRows {
            focal: if session.shown > 0 {
                token_text(&session.tokens[session.shown - 1], mode)
            } else {
                "...".to_string()
            },
            recent: trailing_before_current(session, mode),
            status: format!(
                "stream {}/{}  cadence {} ms",
                session.shown,
                session.tokens.len(),
                cadence
            ),
        },
        MemoPhase::Recall => MemoRows {
            focal: "recall".to_string(),
            recent: String::new(),
            status: format!(
                "{}/{} streamed  space to reveal",
                session.shown,
                session.tokens.len()
            ),
        },
        MemoPhase::Review => MemoRows {
            focal: "rate 1..4".to_string(),
            recent: joined_texts(&session.tokens, mode),
            status: review_status(session),
        },
    }
}

pub fn spawn(commands: &mut Commands, fonts: &crate::present::FontBook) -> Entity {
    let entity = crate::present::spawn_text_view(
        commands,
        fonts,
        AppState::Memo,
        "memo training",
        &["esc menu"],
        None,
    );
    commands.entity(entity).with_children(|parent| {
        parent.spawn((
            MemoLine(MemoLineKind::Focal),
            Text::new(""),
            TextFont {
                font: FontSource::Handle(fonts.bold.clone()),
                font_size: FontSize::Rem(crate::theme::FONT_FOCAL),
                ..default()
            },
            TextColor(crate::theme::text_primary()),
        ));
        crate::present::spawn_line(
            parent,
            fonts,
            "",
            crate::theme::FONT_SMALL,
            crate::theme::text_muted(),
            MemoLine(MemoLineKind::Recent),
        );
        crate::present::spawn_line(
            parent,
            fonts,
            "",
            crate::theme::FONT_BODY,
            crate::theme::text_primary(),
            MemoLine(MemoLineKind::Status),
        );
    });
    entity
}

pub fn bind_text(
    phase: Res<State<MemoPhase>>,
    session: Res<MemoSession>,
    stores: Res<Stores>,
    mut lines: Query<(&MemoLine, &mut Text)>,
) {
    if !session.is_changed() && !phase.is_changed() {
        return;
    }
    let rows = view_rows(*phase.get(), &session, &stores);
    for (line, mut text) in &mut lines {
        let value = match line.0 {
            MemoLineKind::Focal => &rows.focal,
            MemoLineKind::Recent => &rows.recent,
            MemoLineKind::Status => &rows.status,
        };
        *text = Text::new(value.clone());
    }
}
