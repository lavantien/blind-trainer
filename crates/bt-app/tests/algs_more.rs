#[allow(dead_code)]
mod views_common;

use bevy::prelude::*;
use bt_app::logic::AppConfig;
use bt_app::present::FontBook;
use bt_app::states::AppState;
use bt_app::theme;
use bt_app::views::PendingDrill;
use bt_app::views::algs::{
    self, AlgsBrowser, AlgsDetail, AlgsDetailLine, AlgsListRow, AlgsQuery, AlgsQueryBox,
    list_capacity, meta_line,
};
use bt_app::views::drill::{CardContent, DrillSession, RowStyle, build_sides, case_position};
use bt_app::views::drill_view;
use bt_app::views::drill_view::{
    CardPanel, DrillCardRoot, RatePrompt, RepsField, RepsPanel, RepsTimer,
};
use bt_core::Alg;
use bt_data::corpus::{Corpus, Row, SheetClass, Solver};
use bt_scheme::{CaseId, Orbit, enumerate_cases, sticker_of};
use views_common::*;

fn edge_case(first: char, second: char) -> CaseId {
    CaseId {
        orbit: Orbit::Edge,
        t1: sticker_of(Orbit::Edge, first),
        t2: sticker_of(Orbit::Edge, second),
    }
}

fn corner_case(first: char, second: char) -> CaseId {
    CaseId {
        orbit: Orbit::Corner,
        t1: sticker_of(Orbit::Corner, first),
        t2: sticker_of(Orbit::Corner, second),
    }
}

fn temp_paths(fixture: &Fixture) -> RuntimePaths {
    RuntimePaths {
        config_path: fixture.paths.config_path.clone(),
        vendor_root: fixture.paths.vendor_root.clone(),
        user_root: fixture.paths.user_root.clone(),
        assets_root: fixture.paths.assets_root.clone(),
    }
}

fn edge_row(position: &str, alg: &str, commutator: &str) -> Row {
    Row {
        solver: Solver::Cherry,
        sheet: SheetClass::Edge,
        rowid: 1,
        position: position.to_string(),
        alg: Alg::parse(alg).expect("fixture alg parses"),
        alg_text: alg.to_string(),
        commutator: commutator.to_string(),
        recon_link: String::new(),
        youtube_link: String::new(),
    }
}

fn fonts() -> FontBook {
    FontBook {
        regular: Handle::default(),
        bold: Handle::default(),
    }
}

fn algs_app(fixture: &Fixture, query_text: &str) -> (App, Entity) {
    let mut app = headless_app(temp_paths(fixture));
    app.insert_resource(fonts())
        .insert_resource(bevy::input_focus::InputFocus::default())
        .add_systems(Update, (algs::read_query, algs::bind_detail).chain());
    boot_to_menu(&mut app);
    let editable = app
        .world_mut()
        .spawn(bevy::text::EditableText::new(query_text))
        .id();
    let query_box = app.world_mut().spawn(AlgsQueryBox).id();
    app.world_mut().entity_mut(query_box).add_child(editable);
    let capacity = list_capacity(app.world().resource::<AppConfig>());
    for index in 0..capacity {
        app.world_mut().spawn((
            Text::new(""),
            TextColor(theme::text_muted()),
            AlgsListRow(index),
        ));
    }
    app.world_mut().spawn(AlgsDetail);
    open(&mut app, AppState::Algs);
    pump(&mut app, 2);
    (app, editable)
}

fn rail_entity(app: &mut App) -> Entity {
    let mut query = app.world_mut().query_filtered::<Entity, With<AlgsDetail>>();
    query.iter(app.world()).next().expect("detail rail spawned")
}

fn detail_texts(app: &mut App) -> Vec<String> {
    let rail = rail_entity(app);
    let children = app.world().get::<Children>(rail).expect("rail children");
    children
        .iter()
        .filter(|child| app.world().get::<AlgsDetailLine>(*child).is_some())
        .filter_map(|child| app.world().get::<Text>(child).map(|text| text.0.clone()))
        .collect()
}

fn detail_color(app: &mut App, line: &str) -> Color {
    let rail = rail_entity(app);
    let children = app.world().get::<Children>(rail).expect("rail children");
    for child in children.iter() {
        if let Some(text) = app.world().get::<Text>(child)
            && text.0 == line
        {
            return app.world().get::<TextColor>(child).expect("line color").0;
        }
    }
    panic!("detail line {line} missing");
}

fn span_color(app: &mut App, chunk: &str) -> Color {
    let mut query = app.world_mut().query::<(&TextSpan, &TextColor)>();
    for (span, color) in query.iter(app.world()) {
        if span.0 == chunk {
            return color.0;
        }
    }
    panic!("span {chunk} missing");
}

fn list_row(app: &mut App, index: usize) -> Entity {
    let mut query = app.world_mut().query::<(Entity, &AlgsListRow)>();
    query
        .iter(app.world())
        .find(|(_, row)| row.0 == index)
        .map(|(entity, _)| entity)
        .unwrap_or_else(|| panic!("list row {index} missing"))
}

fn row_text(app: &mut App, index: usize) -> String {
    let entity = list_row(app, index);
    app.world().get::<Text>(entity).expect("row text").0.clone()
}

fn row_color(app: &mut App, index: usize) -> Color {
    let entity = list_row(app, index);
    app.world().get::<TextColor>(entity).expect("row color").0
}

fn set_editable(app: &mut App, editable: Entity, text: &str) {
    app.world_mut()
        .get_mut::<bevy::text::EditableText>(editable)
        .expect("editable alive")
        .editor
        .set_text(text);
}

fn focus_editable(app: &mut App, editable: Entity) {
    app.world_mut()
        .resource_mut::<bevy::input_focus::InputFocus>()
        .set(editable, bevy::input_focus::FocusCause::Navigated);
}

#[test]
fn slash_focuses_the_filter_and_plain_keys_never_grab_it() {
    let _guard = guard();
    let fixture = fixture("moreslash");
    let (mut app, editable) = algs_app(&fixture, "");
    assert_eq!(
        app.world()
            .resource::<bevy::input_focus::InputFocus>()
            .get(),
        None
    );
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    assert_eq!(
        app.world()
            .resource::<bevy::input_focus::InputFocus>()
            .get(),
        None,
        "navigation keys must not focus the filter"
    );
    press(&mut app, KeyCode::Slash);
    pump(&mut app, 2);
    assert_eq!(
        app.world()
            .resource::<bevy::input_focus::InputFocus>()
            .get(),
        Some(editable),
        "slash focuses the editable child of the query box"
    );
}

#[test]
fn typed_prefix_flows_from_the_input_into_the_filter() {
    let _guard = guard();
    let fixture = fixture("moretype");
    let (app, _) = algs_app(&fixture, " ab ");
    assert_eq!(app.world().resource::<AlgsQuery>().0, " ab ");
    let browser = app.world().resource::<AlgsBrowser>();
    assert_eq!(browser.filtered.len(), 2);
    assert_eq!(browser.selected, 0);
    assert_eq!(browser.filtered[0].orbit, Orbit::Edge);
    assert_eq!(browser.filtered[1].orbit, Orbit::Corner);
}

#[test]
fn the_list_bind_renders_caret_rows_and_blanks_the_overflow() {
    let _guard = guard();
    let fixture = fixture("morecaret");
    let (mut app, _) = algs_app(&fixture, "ab");
    assert_eq!(row_text(&mut app, 0), "> AB edge");
    assert_eq!(row_color(&mut app, 0), theme::text_primary());
    assert_eq!(row_text(&mut app, 1), "  AB corner");
    assert_eq!(row_color(&mut app, 1), theme::text_muted());
    assert_eq!(
        row_text(&mut app, 2),
        "",
        "rows past the match set blank out"
    );
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    assert_eq!(row_text(&mut app, 0), "  AB edge");
    assert_eq!(row_color(&mut app, 0), theme::text_muted());
    assert_eq!(row_text(&mut app, 1), "> AB corner");
    assert_eq!(row_color(&mut app, 1), theme::text_primary());
}

#[test]
fn the_detail_shows_coverage_mechanics_meta_and_links() {
    let _guard = guard();
    let fixture = fixture("moredetail");
    let (mut app, _) = algs_app(&fixture, "ab");
    let parsed = Alg::parse(COVERED_ALG).expect("fixture alg parses");
    let lines = detail_texts(&mut app);
    assert!(lines.contains(&"2 of 818 cases match".to_string()));
    assert!(lines.contains(&"AB".to_string()));
    assert!(lines.contains(&"edge UF-UB-UR covered yes".to_string()));
    assert!(lines.contains(&format!(
        "cherry qtm {} htm {} [verified]",
        parsed.qtm(),
        parsed.htm()
    )));
    assert!(lines.contains(&"recon  video https://example.test/recon".to_string()));
    assert_eq!(span_color(&mut app, "R2 U': "), theme::setup());
    assert_eq!(span_color(&mut app, "S"), theme::teal());
    assert_eq!(span_color(&mut app, "R2"), theme::interchange());
    assert_eq!(
        detail_color(&mut app, "edge UF-UB-UR covered yes"),
        theme::ok()
    );

    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    let corner_line = format!(
        "corner {} covered no",
        case_position(&corner_case('A', 'B'))
    );
    let lines = detail_texts(&mut app);
    assert!(lines.contains(&corner_line));
    assert!(lines.contains(&"no vendor algs".to_string()));
    assert_eq!(detail_color(&mut app, &corner_line), theme::danger());
}

#[test]
fn a_raw_commutator_line_appears_when_decomposition_fails() {
    let _guard = guard();
    let fixture = bare_fixture("morerawcomm");
    add_edge_row(&fixture.base, COVERED_POSITION, COVERED_ALG, "junk");
    let (mut app, _) = algs_app(&fixture, "ab");
    let lines = detail_texts(&mut app);
    assert!(lines.contains(&COVERED_ALG.to_string()));
    assert!(lines.contains(&"commutator junk".to_string()));
    let mut spans = app.world_mut().query::<&TextSpan>();
    assert_eq!(
        spans.iter(app.world()).count(),
        0,
        "no mechanic spans without a decomposition"
    );
}

#[test]
fn extra_vendor_rows_render_as_alternatives_with_their_own_meta() {
    let _guard = guard();
    let fixture = fixture("morealt");
    add_edge_row(&fixture.base, COVERED_POSITION, "M2 U M2 U2 M U M2", "");
    let (mut app, _) = algs_app(&fixture, "ab");
    let alt = Alg::parse("M2 U M2 U2 M U M2").expect("alt alg parses");
    let verdict = bt_data::verify::verify(&alt, COVERED_POSITION, SheetClass::Edge);
    let badge = if verdict.ok() {
        "verified".to_string()
    } else {
        format!("{} problem(s)", verdict.problems.len())
    };
    let lines = detail_texts(&mut app);
    assert!(lines.contains(&"M2 U M2 U2 M U M2".to_string()));
    assert!(lines.contains(&format!(
        "cherry qtm {} htm {} [{badge}]",
        alt.qtm(),
        alt.htm()
    )));
    assert!(
        !lines.iter().any(|line| line == COVERED_ALG),
        "the decomposed first row renders as spans, not raw alg text"
    );
    assert!(
        lines
            .iter()
            .filter(|line| line.starts_with("recon"))
            .count()
            >= 2,
        "every vendor row carries its links"
    );
}

#[test]
fn movement_keys_step_wrap_and_respect_the_typing_guard() {
    let _guard = guard();
    let fixture = fixture("moremove");
    let (mut app, editable) = algs_app(&fixture, "");
    let total = app.world().resource::<AlgsBrowser>().all.len();

    press(&mut app, KeyCode::ArrowUp);
    pump(&mut app, 2);
    assert_eq!(app.world().resource::<AlgsBrowser>().selected, total - 1);

    press(&mut app, KeyCode::KeyN);
    pump(&mut app, 2);
    assert_eq!(
        app.world().resource::<AlgsBrowser>().selected,
        0,
        "n wraps down from the tail"
    );

    press(&mut app, KeyCode::KeyP);
    pump(&mut app, 2);
    assert_eq!(
        app.world().resource::<AlgsBrowser>().selected,
        total - 1,
        "p wraps up from the head"
    );

    focus_editable(&mut app, editable);
    press(&mut app, KeyCode::KeyN);
    press(&mut app, KeyCode::KeyP);
    pump(&mut app, 2);
    assert_eq!(
        app.world().resource::<AlgsBrowser>().selected,
        total - 1,
        "n and p stay with the filter while typing"
    );
    press(&mut app, KeyCode::Enter);
    pump(&mut app, 2);
    assert_eq!(
        current_state(&app),
        AppState::Algs,
        "enter stays with the filter while typing"
    );
}

#[test]
fn movement_wraps_inside_a_small_match_set() {
    let _guard = guard();
    let fixture = fixture("morewrap");
    let (mut app, _) = algs_app(&fixture, "ab");
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    assert_eq!(app.world().resource::<AlgsBrowser>().selected, 1);
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    assert_eq!(
        app.world().resource::<AlgsBrowser>().selected,
        0,
        "down wraps within the match set"
    );
}

#[test]
fn enter_sends_the_selected_case_to_the_drill() {
    let _guard = guard();
    let fixture = fixture("moreenter");
    let (mut app, _) = algs_app(&fixture, "ab");
    press(&mut app, KeyCode::Enter);
    pump(&mut app, 3);
    assert_eq!(current_state(&app), AppState::Drill);
    assert_eq!(
        app.world().resource::<PendingDrill>().0,
        None,
        "drill enter consumed the pending case"
    );
    let session = app.world().resource::<DrillSession>();
    assert_eq!(
        session.side_a.as_ref().map(|side| side.case),
        Some(edge_case('A', 'B'))
    );
}

#[test]
fn a_shrunk_match_set_clamps_the_selection_and_blocks_the_handoff() {
    let _guard = guard();
    let fixture = fixture("moreshrink");
    let (mut app, editable) = algs_app(&fixture, "ab");
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    assert_eq!(app.world().resource::<AlgsBrowser>().selected, 1);
    set_editable(&mut app, editable, "abc");
    pump(&mut app, 3);
    let browser = app.world().resource::<AlgsBrowser>();
    assert_eq!(browser.filtered.len(), 0);
    assert_eq!(
        browser.selected, 0,
        "the selection clamps to the shrunken set"
    );

    press(&mut app, KeyCode::ArrowDown);
    press(&mut app, KeyCode::ArrowDown);
    pump(&mut app, 2);
    assert_eq!(
        app.world().resource::<AlgsBrowser>().selected,
        0,
        "movement has nothing to move inside an empty set"
    );
    press(&mut app, KeyCode::Enter);
    press(&mut app, KeyCode::KeyD);
    pump(&mut app, 3);
    assert_eq!(
        current_state(&app),
        AppState::Algs,
        "handoff needs a selected case"
    );
    assert_eq!(app.world().resource::<PendingDrill>().0, None);
    assert_eq!(row_text(&mut app, 0), "no case matches the query");
    assert_eq!(row_text(&mut app, 1), "");
    assert!(detail_texts(&mut app).contains(&"0 of 818 cases match".to_string()));
    assert!(detail_texts(&mut app).contains(&"no case matches the query".to_string()));
}

#[test]
fn meta_line_badges_verification_and_problem_counts() {
    let verified = edge_row(COVERED_POSITION, COVERED_ALG, "");
    let parsed = Alg::parse(COVERED_ALG).expect("fixture alg parses");
    assert_eq!(
        meta_line(&verified),
        format!(
            "cherry qtm {} htm {} [verified]",
            parsed.qtm(),
            parsed.htm()
        )
    );
    let mut broken = edge_row(COVERED_POSITION, "R U R' U'", "");
    broken.solver = Solver::Eggins;
    let broken_alg = Alg::parse("R U R' U'").expect("broken alg parses");
    let problems = bt_data::verify::verify(&broken_alg, COVERED_POSITION, SheetClass::Edge)
        .problems
        .len();
    assert!(problems > 0, "the fixture alg must fail verification");
    assert_eq!(
        meta_line(&broken),
        format!(
            "eggins qtm {} htm {} [{problems} problem(s)]",
            broken_alg.qtm(),
            broken_alg.htm()
        )
    );
}

#[test]
fn row_styles_map_to_their_font_and_color() {
    assert_eq!(
        RowStyle::Focal.metrics(),
        (theme::FONT_FOCAL, theme::text_primary())
    );
    assert_eq!(
        RowStyle::Muted.metrics(),
        (theme::FONT_BODY, theme::text_muted())
    );
    assert_eq!(
        RowStyle::Heading.metrics(),
        (theme::FONT_HEADING, theme::text_primary())
    );
    assert_eq!(
        RowStyle::Alg.metrics(),
        (theme::FONT_ALG, theme::text_primary())
    );
    assert_eq!(
        RowStyle::Body.metrics(),
        (theme::FONT_BODY, theme::text_primary())
    );
    assert_eq!(
        RowStyle::Small.metrics(),
        (theme::FONT_SMALL, theme::text_muted())
    );
    assert_eq!(
        RowStyle::Micro.metrics(),
        (theme::FONT_MICRO, theme::text_muted())
    );
}

fn refresh_shown(mut texts: Query<&mut Text>) {
    drill_view::refresh(texts.iter_mut(), "shown");
}

#[test]
fn refresh_rewrites_only_differing_texts() {
    let mut world = World::new();
    let stale = world.spawn(Text::new("stale")).id();
    let same = world.spawn(Text::new("shown")).id();
    let mut schedule = Schedule::default();
    schedule.add_systems(refresh_shown);
    schedule.run(&mut world);
    assert_eq!(world.get::<Text>(stale).expect("stale text").0, "shown");
    assert_eq!(world.get::<Text>(same).expect("same text").0, "shown");
    world.clear_trackers();
    schedule.run(&mut world);
    let mut changed = world.query_filtered::<(), Changed<Text>>();
    assert_eq!(
        changed.iter(&world).count(),
        0,
        "an equal string must not be rewritten"
    );
}

fn panel_texts(world: &mut World, marker: &str) -> Vec<String> {
    let panel = world
        .query_filtered::<Entity, With<CardPanel>>()
        .iter(world)
        .find(|entity| {
            world
                .get::<Node>(*entity)
                .is_some_and(|node| node.flex_direction == FlexDirection::Column)
                && world.get::<Children>(*entity).is_some_and(|children| {
                    children.iter().any(|child| {
                        world
                            .get::<Text>(child)
                            .is_some_and(|text| text.0 == marker)
                    })
                })
        })
        .expect("panel holding the marker line");
    let children = world.get::<Children>(panel).expect("panel children");
    children
        .iter()
        .filter_map(|child| world.get::<Text>(child).map(|text| text.0.clone()))
        .collect()
}

#[test]
fn the_card_renders_mechanics_and_plain_alg_rows() {
    let mut world = World::new();
    let book = fonts();
    let root = world.spawn_empty().id();
    let content = CardContent {
        rows: vec![
            ("AB".to_string(), RowStyle::Focal),
            (COVERED_ALG.to_string(), RowStyle::Alg),
        ],
        mechanics: Some(vec![
            ("R2 U': ".to_string(), theme::FONT_ALG, theme::setup()),
            ("[S,R2]".to_string(), theme::FONT_ALG, theme::teal()),
        ]),
    };
    {
        let mut commands = world.commands();
        drill_view::spawn_card(&mut commands, root, &book, &content);
    }
    world.flush();
    let lines = panel_texts(&mut world, "AB");
    assert_eq!(
        lines
            .iter()
            .filter(|line| line.as_str() == COVERED_ALG)
            .count(),
        0,
        "the alg row becomes spans, not a plain line"
    );
    let mut spans = world.query::<(&TextSpan, &TextColor)>();
    let mut seen = Vec::new();
    for (span, color) in spans.iter(&world) {
        if span.0 == "R2 U': " {
            seen.push((theme::setup(), color.0));
        }
        if span.0 == "[S,R2]" {
            seen.push((theme::teal(), color.0));
        }
    }
    assert_eq!(seen.len(), 2, "both mechanic chunks render");
    assert!(seen.iter().all(|(expected, actual)| expected == actual));

    let plain = CardContent {
        rows: vec![("M2 U M2 U2 M U M2".to_string(), RowStyle::Alg)],
        mechanics: None,
    };
    {
        let mut commands = world.commands();
        drill_view::spawn_card(&mut commands, root, &book, &plain);
    }
    world.flush();
    let lines = panel_texts(&mut world, "M2 U M2 U2 M U M2");
    assert_eq!(lines, vec!["M2 U M2 U2 M U M2".to_string()]);
    let mut rows = world.query::<(&Text, &TextFont, &TextColor)>();
    let (font, color) = rows
        .iter(&world)
        .find(|(text, _, _)| text.0 == "M2 U M2 U2 M U M2")
        .map(|(_, font, color)| (font.clone(), color.0))
        .expect("plain alg line spawned");
    assert!(matches!(font.font_size, FontSize::Rem(size) if size == theme::FONT_ALG));
    assert_eq!(color, theme::text_primary());
}

#[test]
fn the_reps_panel_binds_letters_alg_and_count_fields() {
    let corpus = Corpus::from_rows(vec![edge_row(COVERED_POSITION, COVERED_ALG, "")]);
    let (a, b) = build_sides(&corpus, &edge_case('A', 'B')).expect("sides build");
    let session = DrillSession {
        side_a: Some(a),
        side_b: Some(b),
        reps_done: 1,
        reps_target: 4,
        reps_side_a: false,
        ..DrillSession::default()
    };
    let mut world = World::new();
    let book = fonts();
    let root = world.spawn_empty().id();
    {
        let mut commands = world.commands();
        drill_view::spawn_reps_panel(&mut commands, root, &book, &session);
    }
    world.flush();
    let inverse = Alg::parse(COVERED_ALG)
        .expect("fixture alg parses")
        .inverse()
        .format();
    let mut fields = world.query::<(&Text, &RepsField)>();
    let (mut letters, mut algs, mut counts) = (Vec::new(), Vec::new(), Vec::new());
    for (text, field) in fields.iter(&world) {
        match field {
            RepsField::Letters => letters.push(text.0.clone()),
            RepsField::Alg => algs.push(text.0.clone()),
            RepsField::Count => counts.push(text.0.clone()),
        }
    }
    assert_eq!(letters, vec!["BA".to_string()]);
    assert_eq!(algs, vec![inverse]);
    assert_eq!(counts, vec!["rep 1 of 4".to_string()]);
    let mut timers = world.query::<(&Text, &RepsTimer)>();
    assert_eq!(
        timers
            .iter(&world)
            .map(|(text, _)| text.0.clone())
            .collect::<Vec<_>>(),
        vec!["0s".to_string()]
    );

    let empty = DrillSession::default();
    let root = world.spawn_empty().id();
    {
        let mut commands = world.commands();
        drill_view::spawn_reps_panel(&mut commands, root, &book, &empty);
    }
    world.flush();
    let mut fields = world.query::<(&Text, &RepsField)>();
    let (mut letters, mut counts) = (Vec::new(), Vec::new());
    for (text, field) in fields.iter(&world) {
        match field {
            RepsField::Letters => letters.push(text.0.clone()),
            RepsField::Count => counts.push(text.0.clone()),
            RepsField::Alg => {}
        }
    }
    assert!(
        letters.contains(&String::new()),
        "no side clears the letters"
    );
    assert!(
        counts.contains(&"rep 0 of 0".to_string()),
        "no pair reports zero progress"
    );
    let mut panels = world.query_filtered::<Entity, With<RepsPanel>>();
    assert_eq!(panels.iter(&world).count(), 2, "each spawn owns a panel");
}

#[test]
fn the_rate_panel_prompts_the_unrated_side_and_averages_splits() {
    let corpus = Corpus::from_rows(vec![edge_row(COVERED_POSITION, COVERED_ALG, "")]);
    let (a, b) = build_sides(&corpus, &edge_case('A', 'B')).expect("sides build");
    let session = DrillSession {
        side_a: Some(a),
        side_b: Some(b),
        rate_step: 1,
        splits: vec![(true, 1.5), (false, 2.5)],
        ..DrillSession::default()
    };
    let mut world = World::new();
    let book = fonts();
    let root = world.spawn_empty().id();
    {
        let mut commands = world.commands();
        drill_view::spawn_rate_panel(&mut commands, root, &book, &session);
    }
    world.flush();
    let mut prompts = world.query_filtered::<&Text, With<RatePrompt>>();
    assert_eq!(
        prompts
            .iter(&world)
            .map(|text| text.0.clone())
            .collect::<Vec<_>>(),
        vec!["rate BA".to_string()],
        "step 1 prompts side b"
    );
    let mut lines = world.query_filtered::<&Text, With<drill_view::DrillLine>>();
    let texts: Vec<String> = lines.iter(&world).map(|text| text.0.clone()).collect();
    assert!(texts.contains(&"2 reps avg 2.0s".to_string()));
    assert!(texts.contains(&"1 again 2 hard 3 good 4 easy".to_string()));

    let empty = DrillSession::default();
    let root = world.spawn_empty().id();
    {
        let mut commands = world.commands();
        drill_view::spawn_rate_panel(&mut commands, root, &book, &empty);
    }
    world.flush();
    let mut prompts = world.query_filtered::<&Text, With<RatePrompt>>();
    let texts: Vec<String> = prompts.iter(&world).map(|text| text.0.clone()).collect();
    assert!(texts.contains(&"no pair".to_string()));
    let mut lines = world.query_filtered::<&Text, With<drill_view::DrillLine>>();
    let texts: Vec<String> = lines.iter(&world).map(|text| text.0.clone()).collect();
    assert!(texts.contains(&"0 reps avg 0.0s".to_string()));
}

#[test]
fn drill_spawn_builds_the_net_region_and_the_card_root() {
    let mut world = World::new();
    let book = fonts();
    let config = AppConfig::default();
    let content = {
        let mut commands = world.commands();
        drill_view::spawn(&mut commands, &book, &config)
    };
    world.flush();
    let node = world.get::<Node>(content).expect("content node");
    assert_eq!(node.flex_direction, FlexDirection::Row);
    assert_eq!(node.width, Val::Percent(100.0));
    let mut roots = world.query_filtered::<Entity, With<DrillCardRoot>>();
    assert_eq!(roots.iter(&world).count(), 1);
    let mut nets = world.query_filtered::<Entity, With<bt_app::net::NetView>>();
    assert!(
        nets.iter(&world).count() > 0,
        "the frame hosts the cube net"
    );
}

#[test]
fn the_browser_lists_the_full_edge_then_corner_space() {
    let _guard = guard();
    let fixture = fixture("morespace");
    let (app, _) = algs_app(&fixture, "");
    let browser = app.world().resource::<AlgsBrowser>().clone();
    assert_eq!(browser.filtered.len(), 818);
    let expected: Vec<CaseId> = enumerate_cases(Orbit::Edge)
        .into_iter()
        .chain(enumerate_cases(Orbit::Corner))
        .collect();
    assert_eq!(browser.filtered, expected);
}
