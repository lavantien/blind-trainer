use crate::logic::{Fsrs, NavIntent};
use crate::persist::PersistenceRoot;
use crate::states::{AppState, CaseDrillPhase};
use crate::views::algs::browse::{RepoBrowser, RepoKind};
use crate::views::algs::detail::OverrideStore;
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bt_data::db::{Db, EVENT_3BLD};
use bt_data::repo_cards::{self, CaseCards};
use bt_data::repo_user::CaseRef;
use bt_data::srs::Card;
use bt_puzzles::{FaceletFace, FaceletGrid, Geometry};

const CUBE_FACES: [&str; 6] = ["U", "L", "F", "R", "B", "D"];
const SKEWB_FACES: [&str; 6] = ["U", "F", "R", "L", "B", "D"];
const FTO_FACES: [&str; 8] = ["U", "F", "R", "BR", "B", "BL", "L", "D"];
const FTO_DISPLAY: [&str; 8] = ["U", "L", "F", "R", "B", "BR", "BL", "D"];
const PYRA_FACES: [&str; 4] = ["F", "L", "R", "D"];
const MEGA_FACES: [&str; 12] = [
    "U", "F", "R", "BR", "BL", "L", "D", "B", "UR", "DR", "DL", "UL",
];
const SQ1_FACES: [&str; 2] = ["U", "D"];
const FACE_COLOR_INDEX: [(&str, usize); 12] = [
    ("U", 0),
    ("L", 1),
    ("F", 2),
    ("R", 3),
    ("B", 4),
    ("D", 5),
    ("BR", 6),
    ("BL", 7),
    ("UR", 8),
    ("DR", 9),
    ("DL", 10),
    ("UL", 11),
];

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct DrillCase {
    pub case: CaseRef,
    pub size: Option<i64>,
    pub seed_alg: String,
    pub alg: String,
}

#[derive(Clone, Debug, Default, Resource)]
pub struct PendingCaseDrill(pub Vec<DrillCase>);

#[derive(Clone, Debug, Default, Resource)]
pub struct CaseCardStore(pub CaseCards, pub Option<String>);

#[derive(Clone, Debug, Default, Resource)]
pub struct CaseDrillSession {
    pub queue: Vec<DrillCase>,
    pub current: Option<DrillCase>,
    pub done: usize,
    pub total: usize,
    pub grid: Option<FaceletGrid>,
    pub grid_error: Option<String>,
    pub save_error: Option<String>,
}

pub fn face_color_index(face: &str) -> usize {
    FACE_COLOR_INDEX
        .iter()
        .find(|(name, _)| *name == face)
        .map(|(_, index)| *index)
        .unwrap_or(0)
}

fn engine_faces(event: &str) -> Option<&'static [&'static str]> {
    match event {
        "2x2" | "3x3" | "oh" | "bigcube" => Some(&CUBE_FACES),
        "skewb" => Some(&SKEWB_FACES),
        "fto" => Some(&FTO_FACES),
        "pyraminx" => Some(&PYRA_FACES),
        "megaminx" => Some(&MEGA_FACES),
        "sq1" => Some(&SQ1_FACES),
        _ => None,
    }
}

fn display_faces(event: &str) -> Option<&'static [&'static str]> {
    match event {
        "skewb" => Some(&CUBE_FACES),
        "fto" => Some(&FTO_DISPLAY),
        other => engine_faces(other),
    }
}

pub fn face_permutation(event: &str) -> Option<Vec<usize>> {
    let engine = engine_faces(event)?;
    let display = display_faces(event)?;
    let mut order = Vec::with_capacity(display.len());
    for name in display {
        order.push(engine.iter().position(|candidate| candidate == name)?);
    }
    Some(order)
}

pub fn color_remap(event: &str) -> Option<Vec<usize>> {
    if event == "sq1" {
        return Some([0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 5, 5, 5, 5, 5, 5].to_vec());
    }
    let engine = engine_faces(event)?;
    Some(engine.iter().map(|name| face_color_index(name)).collect())
}

pub fn themed_grid(grid: &FaceletGrid, event: &str) -> Option<FaceletGrid> {
    let remap = color_remap(event)?;
    let order = face_permutation(event)?;
    let mut faces = Vec::with_capacity(order.len());
    for engine_index in order {
        let source = grid.faces.get(engine_index)?;
        let mut rows = Vec::with_capacity(source.rows.len());
        for row in &source.rows {
            rows.push(
                row.iter()
                    .map(|&color| {
                        remap.get(color as usize).copied().unwrap_or(color as usize) as u8
                    })
                    .collect(),
            );
        }
        faces.push(FaceletFace { rows });
    }
    Some(FaceletGrid { faces })
}

fn geometry_grid<G: Geometry>(
    geometry: &G,
    text: &str,
    event: &str,
) -> Result<FaceletGrid, String> {
    let alg = geometry.parse_alg(text).map_err(|err| err.to_string())?;
    let inverse = geometry.invert_alg(&alg);
    let mut state = geometry.solved_state();
    geometry
        .apply_alg(&mut state, &inverse)
        .map_err(|err| err.to_string())?;
    themed_grid(&geometry.facelets(&state), event)
        .ok_or_else(|| format!("no face layout for event {event}"))
}

fn nxn_grid(n: usize, text: &str) -> Result<FaceletGrid, String> {
    let geometry = bt_puzzles::nxn::Nxn::new(n)
        .ok_or_else(|| format!("no case geometry for a {n}x{n} cube"))?;
    geometry_grid(&geometry, text, "bigcube")
}

pub fn case_facelets(event: &str, size: Option<i64>, alg: &str) -> Result<FaceletGrid, String> {
    match event {
        "2x2" => {
            let geometry = bt_puzzles::nxn::Nxn::new(2).expect("2x2 geometry");
            geometry_grid(&geometry, alg, event)
        }
        "3x3" | "oh" => {
            let geometry = bt_puzzles::nxn::Nxn::new(3).expect("3x3 geometry");
            geometry_grid(&geometry, alg, event)
        }
        "bigcube" => nxn_grid(size.unwrap_or(7).clamp(2, 7) as usize, alg),
        "skewb" => geometry_grid(&bt_puzzles::skewb::Skewb::new(), alg, event),
        "pyraminx" => geometry_grid(&bt_puzzles::pyra::Pyraminx, alg, event),
        "megaminx" => geometry_grid(&bt_puzzles::mega::Mega, alg, event),
        "fto" => geometry_grid(&bt_puzzles::fto::Fto::new(), alg, event),
        "sq1" => geometry_grid(&bt_puzzles::sq1::Sq1, alg, event),
        other => Err(format!("no case geometry for event {other}")),
    }
}

fn collect_node(
    node: &crate::views::algs::browse::RepoNode,
    event: &str,
    method: Option<&str>,
    stage: Option<&str>,
    size: Option<i64>,
    store: &OverrideStore,
    out: &mut Vec<DrillCase>,
) {
    match node.kind {
        RepoKind::Letters => {}
        RepoKind::Event | RepoKind::Method | RepoKind::Stage => {
            let method = match node.kind {
                RepoKind::Method => Some(node.label.as_str()),
                _ => method,
            };
            let stage = match node.kind {
                RepoKind::Stage => Some(node.label.as_str()),
                _ => stage,
            };
            for child in &node.children {
                collect_node(child, event, method, stage, size, store, out);
            }
        }
        RepoKind::Size => {
            let parsed = node
                .label
                .split('x')
                .next()
                .and_then(|digits| digits.parse::<i64>().ok());
            for child in &node.children {
                collect_node(child, event, method, stage, parsed, store, out);
            }
        }
        RepoKind::Case => {
            if let (Some(method), Some(stage), Some(case)) = (method, stage, &node.case) {
                let reference = CaseRef {
                    event: event.to_string(),
                    method: method.to_string(),
                    stage: stage.to_string(),
                    case_key: node.label.clone(),
                };
                let effective =
                    crate::views::algs::detail::effective_alg(&reference, &case.alg, store);
                out.push(DrillCase {
                    case: reference,
                    size,
                    seed_alg: case.alg.clone(),
                    alg: effective.alg,
                });
            }
        }
    }
}

pub fn collect_cases(browser: &RepoBrowser, store: &OverrideStore) -> Vec<DrillCase> {
    let mut cases = Vec::new();
    let Some(arm) = browser.arms.get(browser.arm) else {
        return cases;
    };
    if arm.kind == RepoKind::Letters || arm.label == EVENT_3BLD {
        return cases;
    }
    let mut node = arm;
    let mut method: Option<String> = None;
    let mut stage: Option<String> = None;
    let mut size = None;
    for step in &browser.path {
        let Some(child) = node.children.get(*step) else {
            return cases;
        };
        node = child;
        match node.kind {
            RepoKind::Size => {
                size = node
                    .label
                    .split('x')
                    .next()
                    .and_then(|digits| digits.parse::<i64>().ok());
            }
            RepoKind::Method => method = Some(node.label.clone()),
            RepoKind::Stage => stage = Some(node.label.clone()),
            _ => {}
        }
    }
    collect_node(
        node,
        &arm.label,
        method.as_deref(),
        stage.as_deref(),
        size,
        store,
        &mut cases,
    );
    cases
}

pub fn due_session(cases: Vec<DrillCase>, cards: &CaseCards, now_day: i64) -> Vec<DrillCase> {
    let mut due = Vec::new();
    let mut seen = Vec::new();
    for case in cases {
        match cards.get(&case.case) {
            Some(card) if !card.is_due(now_day) => seen.push(case),
            _ => due.push(case),
        }
    }
    if due.is_empty() { seen } else { due }
}

pub fn drill_request(
    browser: &RepoBrowser,
    store: &OverrideStore,
    cards: &CaseCardStore,
    now_day: i64,
) -> Option<Vec<DrillCase>> {
    let node = browser.node()?;
    let all = collect_cases(browser, store);
    if all.is_empty() {
        return None;
    }
    if node.kind == RepoKind::Case {
        return Some(all);
    }
    Some(due_session(all, &cards.0, now_day))
}

pub fn install(session: &mut CaseDrillSession, queue: Vec<DrillCase>) {
    session.total = queue.len();
    session.done = 0;
    session.queue = queue;
    session.save_error = None;
    advance(session);
}

pub fn advance(session: &mut CaseDrillSession) {
    session.current = if session.queue.is_empty() {
        None
    } else {
        Some(session.queue.remove(0))
    };
    session.grid = None;
    session.grid_error = None;
    if let Some(case) = &session.current {
        match case_facelets(&case.case.event, case.size, &case.alg) {
            Ok(grid) => session.grid = Some(grid),
            Err(reason) => session.grid_error = Some(reason),
        }
    }
}

pub fn case_site_line(case: &DrillCase) -> String {
    let mut line = format!(
        "{} > {} > {}",
        case.case.event, case.case.method, case.case.stage
    );
    if let Some(size) = case.size {
        line.push_str(&format!(" {size}x{size}"));
    }
    line
}

pub fn enter(
    mut commands: Commands,
    mut pending: ResMut<PendingCaseDrill>,
    cards: Res<CaseCardStore>,
    mut session: ResMut<CaseDrillSession>,
    mut next: ResMut<NextState<CaseDrillPhase>>,
) {
    let queue = std::mem::take(&mut pending.0);
    if queue.is_empty() {
        commands.write_message(NavIntent::Open(AppState::Algs));
        return;
    }
    install(&mut session, queue);
    if let Some(reason) = &cards.1 {
        session.save_error = Some(reason.clone());
    }
    next.set(CaseDrillPhase::Show);
}

pub fn show_keys(
    mut inputs: MessageReader<KeyboardInput>,
    session: Res<CaseDrillSession>,
    mut next: ResMut<NextState<CaseDrillPhase>>,
) {
    if session.current.is_none() {
        return;
    }
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && !input.repeat && input.key_code == KeyCode::Space
        {
            next.set(CaseDrillPhase::Grade);
            return;
        }
    }
}

pub fn grade_keys(
    mut commands: Commands,
    mut inputs: MessageReader<KeyboardInput>,
    mut session: ResMut<CaseDrillSession>,
    fsrs: Res<Fsrs>,
    mut cards: ResMut<CaseCardStore>,
    root: Res<PersistenceRoot>,
    mut next: ResMut<NextState<CaseDrillPhase>>,
) {
    let Some(case) = session.current.clone() else {
        return;
    };
    for input in inputs.read() {
        if input.state != ButtonState::Pressed || input.repeat {
            continue;
        }
        let Some(rating) = crate::views::drill::rating_of(input.key_code) else {
            continue;
        };
        if let Some(reason) = &cards.1 {
            session.save_error = Some(reason.clone());
            return;
        }
        let day = crate::views::now_day();
        let card = cards
            .0
            .entry(case.case.clone())
            .or_insert_with(|| Card::new(day));
        card.review(rating, day, &fsrs.0);
        session.save_error = Db::open_root(&root.0)
            .and_then(|mut db| repo_cards::save_card(&mut db, &case.case, card))
            .err()
            .map(|err| err.to_string());
        session.done += 1;
        advance(&mut session);
        if session.current.is_some() {
            next.set(CaseDrillPhase::Show);
        } else {
            commands.write_message(NavIntent::Open(AppState::Algs));
        }
        return;
    }
}

pub fn cards_enter(root: Res<PersistenceRoot>, fsrs: Res<Fsrs>, mut cards: ResMut<CaseCardStore>) {
    *cards = CaseCardStore::default();
    match Db::open_root(&root.0) {
        Ok(db) => match repo_cards::load(&db, &fsrs.0) {
            Ok(loaded) => cards.0 = loaded,
            Err(err) => cards.1 = Some(err.to_string()),
        },
        Err(err) => cards.1 = Some(err.to_string()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::views::algs::browse::testkit::{fixture_packs, fixture_rows, hidden, pack, row};
    use crate::views::algs::browse::{repo_arms, repo_deeper, repo_move};
    use bt_data::seedpack::NSpec;

    fn solved_grid(names: &[&str]) -> FaceletGrid {
        FaceletGrid {
            faces: names
                .iter()
                .map(|_| FaceletFace {
                    rows: vec![vec![0, 1, 2], vec![1, 0, 1], vec![2, 1, 0]],
                })
                .collect(),
        }
    }

    fn uniform_grid(names: &[&str]) -> FaceletGrid {
        FaceletGrid {
            faces: names
                .iter()
                .enumerate()
                .map(|(index, _)| FaceletFace {
                    rows: vec![
                        vec![index as u8, index as u8],
                        vec![index as u8, index as u8],
                    ],
                })
                .collect(),
        }
    }

    fn armed() -> RepoBrowser {
        RepoBrowser {
            arms: repo_arms(&fixture_packs(), &hidden(&["sq1"]), &fixture_rows()),
            ..RepoBrowser::default()
        }
    }

    fn at_bigcube_case(case_key: &str) -> RepoBrowser {
        let mut browser = armed();
        repo_deeper(&mut browser);
        repo_move(&mut browser, 2);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        assert_eq!(browser.node().expect("case node").label, case_key);
        browser
    }

    fn at_2x2_event() -> RepoBrowser {
        let mut browser = armed();
        repo_deeper(&mut browser);
        repo_move(&mut browser, 1);
        repo_deeper(&mut browser);
        browser
    }

    fn with_3bld_arm() -> RepoBrowser {
        let mut browser = RepoBrowser::default();
        let packs = vec![
            pack("3bld", "3style", "parity", NSpec::Unset),
            pack("2x2", "cll", "cll", NSpec::Unset),
        ];
        let rows = vec![
            row("3bld", "3style", "parity", "par.1"),
            row("2x2", "cll", "cll", "cll.1"),
        ];
        browser.arms = repo_arms(&packs, &hidden(&[]), &rows);
        repo_deeper(&mut browser);
        repo_move(&mut browser, 1);
        browser
    }

    #[test]
    fn face_permutations_follow_the_ledger_orders() {
        assert_eq!(
            face_permutation("3x3"),
            Some((0..6).collect::<Vec<_>>()),
            "nxn engine order already matches the theme order"
        );
        assert_eq!(face_permutation("skewb"), Some(vec![0, 3, 1, 2, 4, 5]));
        assert_eq!(face_permutation("fto"), Some(vec![0, 6, 1, 2, 4, 3, 5, 7]));
        assert_eq!(
            face_permutation("pyraminx"),
            Some((0..4).collect::<Vec<_>>())
        );
        assert_eq!(
            face_permutation("megaminx"),
            Some((0..12).collect::<Vec<_>>())
        );
        assert_eq!(face_permutation("sq1"), Some(vec![0, 1]));
        assert_eq!(face_permutation("3bld"), None);
        assert_eq!(face_permutation("clock"), None);
    }

    #[test]
    fn color_remaps_send_each_face_name_to_its_palette_slot() {
        assert_eq!(color_remap("3x3"), Some(vec![0, 1, 2, 3, 4, 5]));
        assert_eq!(color_remap("skewb"), Some(vec![0, 2, 3, 1, 4, 5]));
        assert_eq!(color_remap("fto"), Some(vec![0, 2, 3, 6, 4, 7, 1, 5]));
        assert_eq!(color_remap("pyraminx"), Some(vec![2, 1, 3, 5]));
        assert_eq!(
            color_remap("megaminx"),
            Some(vec![0, 2, 3, 6, 7, 1, 5, 4, 8, 9, 10, 11])
        );
        assert_eq!(
            color_remap("sq1"),
            Some(vec![0, 0, 0, 0, 0, 0, 0, 0, 5, 5, 5, 5, 5, 5, 5, 5])
        );
        assert_eq!(color_remap("clock"), None);
    }

    #[test]
    fn pyraminx_faces_are_opposite_their_vertices() {
        assert_eq!(PYRA_FACES, ["F", "L", "R", "D"]);
    }

    #[test]
    fn sq1_piece_ids_render_in_layer_face_colors() {
        let solved = case_facelets("sq1", None, "(6,0)").expect("grid");
        assert!(
            solved.faces[0].rows[0].iter().all(|&color| color == 0),
            "a layer-bound twist keeps the U color: {:?}",
            solved.faces[0].rows[0]
        );
        assert!(
            solved.faces[1].rows[0].iter().all(|&color| color == 5),
            "the untouched bottom layer keeps the D color: {:?}",
            solved.faces[1].rows[0]
        );
        let scrambled = case_facelets("sq1", None, "(1,0)/").expect("grid");
        for face in &scrambled.faces {
            assert!(
                face.rows[0].iter().all(|&color| color == 0 || color == 5),
                "piece ids never leak as palette slots: {:?}",
                face.rows[0]
            );
        }
    }

    #[test]
    fn themed_grid_identity_colors_a_solved_skewb() {
        let grid = uniform_grid(&SKEWB_FACES);
        let themed = themed_grid(&grid, "skewb").expect("skewb theme");
        assert_eq!(themed.faces.len(), 6);
        for (slot, name) in CUBE_FACES.iter().enumerate() {
            let wanted = face_color_index(name) as u8;
            assert!(
                themed.faces[slot]
                    .rows
                    .iter()
                    .all(|row| row.iter().all(|&color| color == wanted)),
                "slot {name} must be uniform"
            );
        }
        assert_eq!(
            themed.faces[1].rows,
            vec![vec![1, 1], vec![1, 1]],
            "the L slot shows the engine L face recolored to the L palette slot"
        );
    }

    #[test]
    fn themed_grid_keeps_sticker_payload_under_remap() {
        let grid = solved_grid(&CUBE_FACES);
        let themed = themed_grid(&grid, "3x3").expect("cube theme");
        assert_eq!(themed.faces, grid.faces, "identity event changes nothing");
        let skewb = themed_grid(&grid, "skewb").expect("skewb theme");
        assert_eq!(
            skewb.faces[0].rows[0],
            vec![0, 2, 3],
            "the U slot keeps the engine U face with F and R stickers recolored"
        );
        assert_eq!(
            skewb.faces[2].rows[0],
            vec![0, 2, 3],
            "the engine F face lands in the F slot recolored"
        );
    }

    #[test]
    fn case_facelets_applies_the_inverse_alg_to_solved() {
        let shown = case_facelets("3x3", None, "R U R'").expect("grid");
        let nxn = bt_puzzles::nxn::Nxn::new(3).expect("geometry");
        let alg = nxn.parse_alg("R U R'").expect("alg");
        let mut expected = nxn.solved_state();
        nxn.apply_alg(&mut expected, &nxn.invert_alg(&alg))
            .expect("apply");
        assert_eq!(
            shown,
            themed_grid(&nxn.facelets(&expected), "3x3").expect("theme")
        );
        let mut forward = nxn.solved_state();
        nxn.apply_alg(&mut forward, &alg).expect("apply");
        assert_ne!(
            shown,
            themed_grid(&nxn.facelets(&forward), "3x3").expect("theme"),
            "display must not show the forward state"
        );
    }

    #[test]
    fn case_facelets_covers_every_pack_event_and_rejects_the_rest() {
        for (event, size, alg) in [
            ("2x2", None, "R U R'"),
            ("bigcube", Some(5), "Rw U Rw'"),
            ("skewb", None, "R F' L"),
            ("pyraminx", None, "R U"),
            ("fto", None, "R U R'"),
            ("sq1", None, "(1,0)/"),
            ("megaminx", None, "R U'"),
        ] {
            let grid = case_facelets(event, size, alg)
                .unwrap_or_else(|err| panic!("event {event} alg {alg} must render: {err}"));
            assert_eq!(
                grid.faces.len(),
                engine_faces(event).expect("known event").len(),
                "event {event}"
            );
        }
        assert_eq!(
            case_facelets("clock", None, "R"),
            Err("no case geometry for event clock".to_string())
        );
        assert_eq!(
            case_facelets("3bld", None, "R"),
            Err("no case geometry for event 3bld".to_string())
        );
        assert!(case_facelets("3x3", None, "QQ").is_err());
    }

    #[test]
    fn collect_cases_walks_the_subtree_with_the_site_reference() {
        let browser = at_bigcube_case("par.1");
        let cases = collect_cases(&browser, &OverrideStore::default());
        assert_eq!(cases.len(), 1);
        assert_eq!(
            cases[0].case,
            CaseRef {
                event: "bigcube".to_string(),
                method: "yau".to_string(),
                stage: "parity".to_string(),
                case_key: "par.1".to_string(),
            }
        );
        assert_eq!(cases[0].size, Some(4));
        assert_eq!(cases[0].alg, "alg par.1");
        assert_eq!(cases[0].seed_alg, "alg par.1");

        let event_browser = at_2x2_event();
        let cases = collect_cases(&event_browser, &OverrideStore::default());
        assert_eq!(
            cases
                .iter()
                .map(|case| case.case.case_key.as_str())
                .collect::<Vec<_>>(),
            vec!["cll.1", "cll.2"],
            "document order is preserved"
        );
        assert!(cases.iter().all(|case| case.size.is_none()));
    }

    #[test]
    fn collect_cases_uses_the_override_as_the_effective_alg() {
        let browser = at_bigcube_case("par.1");
        let case = CaseRef {
            event: "bigcube".to_string(),
            method: "yau".to_string(),
            stage: "parity".to_string(),
            case_key: "par.1".to_string(),
        };
        let mut store = OverrideStore::default();
        store.0.insert(case, "R U R'".to_string());
        let cases = collect_cases(&browser, &store);
        assert_eq!(cases[0].seed_alg, "alg par.1");
        assert_eq!(cases[0].alg, "R U R'");
    }

    #[test]
    fn collect_cases_ignores_the_letters_and_3bld_arms() {
        let mut browser = armed();
        repo_deeper(&mut browser);
        assert!(collect_cases(&browser, &OverrideStore::default()).is_empty());
        let bld = with_3bld_arm();
        assert!(
            collect_cases(&bld, &OverrideStore::default()).is_empty(),
            "the 3bld arm keeps its own drill"
        );
    }

    #[test]
    fn due_session_prioritizes_missing_and_due_cards() {
        let mut cards = CaseCards::new();
        let build = |key: &str| DrillCase {
            case: CaseRef {
                event: "2x2".to_string(),
                method: "cll".to_string(),
                stage: "cll".to_string(),
                case_key: key.to_string(),
            },
            size: None,
            seed_alg: String::new(),
            alg: String::new(),
        };
        let fresh = build("cll.1");
        let future = build("cll.2");
        let overdue = build("cll.3");
        let mut scheduled = Card::new(0);
        scheduled.due_day = 900;
        cards.insert(future.case.clone(), scheduled);
        cards.insert(overdue.case.clone(), Card::new(0));
        let queue = due_session(
            vec![fresh.clone(), future.clone(), overdue.clone()],
            &cards,
            1,
        );
        assert_eq!(
            queue,
            vec![fresh, overdue],
            "missing and due cards come first in document order"
        );
        let fallback = due_session(vec![future.clone()], &cards, 1);
        assert_eq!(fallback, vec![future], "with nothing due every case drills");
    }

    #[test]
    fn drill_request_drills_one_case_and_due_sessions_below_it() {
        let cards = CaseCardStore::default();
        let case_browser = at_bigcube_case("par.1");
        assert_eq!(
            drill_request(&case_browser, &OverrideStore::default(), &cards, 0)
                .expect("single case")
                .len(),
            1
        );
        let event_browser = at_2x2_event();
        assert_eq!(
            drill_request(&event_browser, &OverrideStore::default(), &cards, 0)
                .expect("due session")
                .len(),
            2,
            "nothing carded yet so every case drills"
        );
        let mut browser = armed();
        repo_deeper(&mut browser);
        assert!(
            drill_request(&browser, &OverrideStore::default(), &cards, 0).is_none(),
            "the letters arm offers no case drill"
        );
    }

    #[test]
    fn install_and_advance_run_the_session_queue() {
        let mut session = CaseDrillSession::default();
        let first = DrillCase {
            case: CaseRef {
                event: "3x3".to_string(),
                method: "cfop".to_string(),
                stage: "pll".to_string(),
                case_key: "pll.1".to_string(),
            },
            size: None,
            seed_alg: "R U R'".to_string(),
            alg: "R U R'".to_string(),
        };
        let broken = DrillCase {
            case: CaseRef {
                event: "3x3".to_string(),
                method: "cfop".to_string(),
                stage: "pll".to_string(),
                case_key: "pll.2".to_string(),
            },
            size: None,
            seed_alg: "QQ".to_string(),
            alg: "QQ".to_string(),
        };
        install(&mut session, vec![first.clone(), broken.clone()]);
        assert_eq!(session.total, 2);
        assert_eq!(session.done, 0);
        assert_eq!(session.current, Some(first.clone()));
        assert!(session.grid.is_some());
        assert!(session.grid_error.is_none());
        advance(&mut session);
        assert_eq!(session.current, Some(broken.clone()));
        assert!(session.grid.is_none());
        assert_eq!(
            session.grid_error,
            case_facelets("3x3", None, "QQ").err(),
            "a broken alg reports the exact engine error"
        );
        advance(&mut session);
        assert!(session.current.is_none());
        assert!(session.grid.is_none());
        assert!(session.grid_error.is_none());
    }

    #[test]
    fn case_site_line_spells_the_site_and_size() {
        let case = DrillCase {
            case: CaseRef {
                event: "bigcube".to_string(),
                method: "yau".to_string(),
                stage: "parity".to_string(),
                case_key: "par.1".to_string(),
            },
            size: Some(5),
            seed_alg: String::new(),
            alg: String::new(),
        };
        assert_eq!(case_site_line(&case), "bigcube > yau > parity 5x5");
    }
}
