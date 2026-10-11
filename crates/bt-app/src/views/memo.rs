use crate::logic::{SaveStores, Sm2};
use crate::persist::{MemoMode, Stores};
use crate::states::{AppState, MemoPhase};
use crate::views::drill::{case_position, rating_of, sheet_of, write_review};
use crate::views::{NowDay, SyllableBook};
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
        MemoMode::Images => token.image.map(str::to_string).unwrap_or(letters),
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
    sm2: Res<Sm2>,
    now_day: Res<NowDay>,
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
                rate_pairs(&mut stores, &sm2.0, &session, rating, now_day.0);
                commands.write_message(SaveStores);
                next.set(MemoPhase::Intro);
                return;
            }
        }
    }
}

pub fn rate_pairs(
    stores: &mut Stores,
    sm2: &bt_data::srs::Sm2Config,
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
            sm2,
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
                memo_mode: MemoMode::Syllables,
                drill_reps: 5,
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

#[derive(Component, Default)]
pub struct MemoText;

pub fn view_lines(phase: MemoPhase, session: &MemoSession, stores: &Stores) -> Vec<String> {
    let mode = stores.settings.memo_mode;
    let mut lines = vec![format!("seed {}", session.seed)];
    match phase {
        MemoPhase::Intro => {
            lines.push(format!("{} pairs to stream", session.tokens.len()));
            lines.push("space to stream".to_string());
        }
        MemoPhase::Stream | MemoPhase::Recall => {
            let revealed: Vec<String> = session.tokens[..session.shown]
                .iter()
                .map(|token| token_text(token, mode))
                .collect();
            lines.push(revealed.join(" "));
            lines.push(format!(
                "{}/{} streamed",
                session.shown,
                session.tokens.len()
            ));
            if phase == MemoPhase::Recall {
                lines.push("recall now, space to reveal".to_string());
            }
        }
        MemoPhase::Review => {
            let all: Vec<String> = session
                .tokens
                .iter()
                .map(|token| token_text(token, mode))
                .collect();
            lines.push(all.join(" "));
            if let Some(memo) = &session.memo {
                lines.extend(residue_lines(memo));
            }
            lines.push("r replay, space next, 1..4 rate pairs".to_string());
        }
    }
    lines
}

pub fn spawn(commands: &mut Commands, fonts: &crate::present::FontBook) -> Entity {
    let entity = crate::present::spawn_text_view(
        commands,
        fonts,
        AppState::Memo,
        "memo training",
        &["syllables stream at the configured cadence, esc menu"],
        None,
    );
    commands.entity(entity).with_children(|parent| {
        crate::present::spawn_dynamic(parent, fonts, "", MemoText);
    });
    entity
}

pub fn bind_text(
    phase: Res<State<MemoPhase>>,
    session: Res<MemoSession>,
    stores: Res<Stores>,
    mut texts: Query<&mut Text, With<MemoText>>,
) {
    if !session.is_changed() && !phase.is_changed() {
        return;
    }
    let body = view_lines(*phase.get(), &session, &stores).join("\n");
    for mut text in &mut texts {
        *text = Text::new(body.clone());
    }
}
