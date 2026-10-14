use super::SyllableBook;
use super::drill::{DrillSession, card_key, case_position, flip_case, has_commutator, sheet_of};
use crate::persist::Stores;
use crate::theme;
use crate::theme::FONT_ALG;
use bevy::prelude::*;
use bt_core::{Alg, Cube};
use bt_data::commutator::decompose_verified;
use bt_data::corpus::{Corpus, Row, SheetClass, Solver};
use bt_scheme::{CaseId, Orbit};

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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::views::drill::install_pair;
    use bt_data::srs::Card;

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

    fn seen_card(due_day: i64) -> Card {
        Card {
            ease: 2.5,
            interval_days: 6,
            due_day,
            reps: 2,
            lapses: 0,
            reviews: Vec::new(),
        }
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
    fn card_falls_back_to_the_raw_commutator_row_when_present_but_undecomposable() {
        let corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2", "junk text");
        let stores = stores_with_reps(4);
        let mut session = DrillSession::default();
        install_pair(&mut session, &corpus, &stores, edge_case('A', 'B'));
        let content = card_content(&session, &corpus, &book_with(None), &stores, 0);
        assert!(content.mechanics.is_none());
        assert_eq!(
            content
                .rows
                .iter()
                .find(|(text, _)| text.starts_with("commutator "))
                .expect("raw commutator row renders"),
            &("commutator junk text".to_string(), RowStyle::Small)
        );
    }

    #[test]
    fn card_srs_line_reads_each_side_card_state() {
        let corpus = single_pair_corpus("R2 U' S R2 S' R2 U R2", "");
        let mut stores = stores_with_reps(4);
        stores
            .srs
            .cards
            .insert(card_key(SheetClass::Edge, "UF-UB-UR"), seen_card(0));
        stores
            .srs
            .cards
            .insert(card_key(SheetClass::Edge, "UF-UR-UB"), seen_card(10));
        let mut session = DrillSession::default();
        install_pair(&mut session, &corpus, &stores, edge_case('A', 'B'));
        let content = card_content(&session, &corpus, &book_with(None), &stores, 0);
        assert_eq!(
            content
                .rows
                .iter()
                .find(|(text, _)| text.starts_with("srs "))
                .expect("srs row renders")
                .0,
            "srs AB due, BA ok"
        );
    }

    #[test]
    fn card_prepare_line_prefers_the_vendor_flip_alg() {
        let alg = "R2 U' S R2 S' R2 U R2";
        let inverse = Alg::parse(alg).expect("alg parses").inverse().format();
        let corpus = Corpus::from_rows(vec![
            edge_row("UF-UB-UR", alg, ""),
            row(Solver::Eggins, SheetClass::Edge, "UF-UR-UB", &inverse, ""),
        ]);
        let stores = stores_with_reps(4);
        let mut session = DrillSession::default();
        install_pair(&mut session, &corpus, &stores, edge_case('A', 'B'));
        let content = card_content(&session, &corpus, &book_with(None), &stores, 0);
        assert_eq!(
            content
                .rows
                .iter()
                .find(|(text, _)| text.starts_with("prepare "))
                .expect("prepare row renders"),
            &(format!("prepare BA eggins: {inverse}"), RowStyle::Body)
        );
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
    fn mechanic_spans_join_multiple_units_with_a_separator() {
        let alg = Alg::parse("S R2 S' R2 U R2 U' R2").expect("alg parses");
        let spans = mechanic_spans("[S,R2] + [U,R2]", &alg).expect("two units verify");
        let joined: String = spans.iter().map(|(text, _, _)| text.as_str()).collect();
        assert_eq!(joined, "[S, R2] + [U, R2]");
        assert_eq!(
            spans[5],
            (" + ".to_string(), FONT_ALG, theme::text_primary())
        );
        assert_eq!(spans[7], ("U".to_string(), FONT_ALG, theme::teal()));
        assert_eq!(spans[9], ("R2".to_string(), FONT_ALG, theme::interchange()));
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

    #[test]
    fn reps_and_rate_lines_report_no_pair_without_sides() {
        let session = DrillSession::default();
        assert_eq!(reps_lines(&session), vec!["no pair".to_string()]);
        assert_eq!(rate_lines(&session), vec!["no pair".to_string()]);
    }
}
