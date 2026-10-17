use crate::persist::PersistenceRoot;
use crate::present::FontBook;
use crate::theme;
use crate::views::algs::AlgsDetailLine;
use crate::views::algs::browse::{RepoBrowser, RepoKind, breadcrumb, count_kind, node_at};
use bevy::input::ButtonState;
use bevy::input::keyboard::Key;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bt_data::db::Db;
use bt_data::repo_user::{CaseRef, Overrides};
use bt_puzzles::verify_tier1;
use std::path::Path;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum DetailLine {
    Title(String),
    Alg(String),
    Meta(String),
    Warn(String),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CaseSite {
    pub case: CaseRef,
    pub size: Option<i64>,
}

#[derive(Clone, Debug, Default, Resource)]
pub struct OverrideStore(pub Overrides);

#[derive(Clone, Debug, Default, Resource)]
pub struct OverrideDraft {
    pub case: Option<CaseSite>,
    pub text: String,
    pub error: Option<String>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct EffectiveAlg {
    pub alg: String,
    pub overridden: bool,
}

pub fn effective_alg(case: &CaseRef, seed_alg: &str, store: &OverrideStore) -> EffectiveAlg {
    match store.0.get(case) {
        Some(alg) => EffectiveAlg {
            alg: alg.clone(),
            overridden: true,
        },
        None => EffectiveAlg {
            alg: seed_alg.to_string(),
            overridden: false,
        },
    }
}

pub fn case_expand_line(event: &str, alg: &str) -> Option<String> {
    (event == bt_data::db::EVENT_3BLD)
        .then(|| crate::views::drill::expand_flat_line(alg))
        .flatten()
}

pub fn case_site_of(browser: &RepoBrowser) -> Option<CaseSite> {
    let node = browser.node()?;
    if node.kind != RepoKind::Case {
        return None;
    }
    let arm = browser.arms.get(browser.arm)?;
    let mut size = None;
    let mut method = None;
    let mut stage = None;
    for depth in 0..browser.path.len() {
        let walked = node_at(&browser.arms, browser.arm, &browser.path[..=depth])?;
        match walked.kind {
            RepoKind::Size => {
                size = walked
                    .label
                    .split('x')
                    .next()
                    .and_then(|digits| digits.parse::<i64>().ok());
            }
            RepoKind::Method => method = Some(walked.label.clone()),
            RepoKind::Stage => stage = Some(walked.label.clone()),
            _ => {}
        }
    }
    Some(CaseSite {
        case: CaseRef {
            event: arm.label.clone(),
            method: method?,
            stage: stage?,
            case_key: node.label.clone(),
        },
        size,
    })
}

fn nxn_check(n: usize, text: &str) -> Result<(), String> {
    let geometry = bt_puzzles::nxn::Nxn::new(n)
        .ok_or_else(|| format!("no tier 1 geometry for a {n}x{n} cube"))?;
    verify_tier1(&geometry, text).map_err(|err| err.to_string())
}

fn skewb_stripped(alg: &str) -> Result<String, String> {
    let stripped = bt_data::notation::strip_rotations(alg);
    if stripped.is_empty() {
        Err("alg is empty after rotation stripping".to_string())
    } else {
        Ok(stripped)
    }
}

fn geometry_check(event: &str, size: Option<i64>, text: &str) -> Result<(), String> {
    match event {
        "2x2" => nxn_check(2, text),
        "3x3" | "oh" | "3bld" => nxn_check(3, text),
        "bigcube" => nxn_check(size.unwrap_or(7).clamp(2, 7) as usize, text),
        "skewb" => skewb_stripped(text).and_then(|stripped| {
            verify_tier1(&bt_puzzles::skewb::Skewb::new(), &stripped).map_err(|err| err.to_string())
        }),
        "pyraminx" => {
            verify_tier1(&bt_puzzles::pyra::Pyraminx, text).map_err(|err| err.to_string())
        }
        "megaminx" => verify_tier1(&bt_puzzles::mega::Mega, text).map_err(|err| err.to_string()),
        "fto" => verify_tier1(&bt_puzzles::fto::Fto::new(), text).map_err(|err| err.to_string()),
        "sq1" => verify_tier1(&bt_puzzles::sq1::Sq1, text).map_err(|err| err.to_string()),
        other => Err(format!("no tier 1 geometry for event {other}")),
    }
}

fn write_override(root: &Path, case: &CaseRef, alg: &str) -> Result<(), String> {
    let mut db = Db::open_root(root).map_err(|err| err.to_string())?;
    bt_data::repo_user::upsert(&mut db, case, alg).map_err(|err| err.to_string())
}

fn remove_override(root: &Path, case: &CaseRef) -> Result<bool, String> {
    let mut db = Db::open_root(root).map_err(|err| err.to_string())?;
    bt_data::repo_user::remove(&mut db, case).map_err(|err| err.to_string())
}

pub fn override_enter(root: Res<PersistenceRoot>, mut store: ResMut<OverrideStore>) {
    store.0 = Db::open_root(&root.0)
        .and_then(|db| bt_data::repo_user::load_all(&db))
        .unwrap_or_default();
}

pub fn override_keys(
    mut inputs: MessageReader<KeyboardInput>,
    focus: Option<Res<bevy::input_focus::InputFocus>>,
    editables: Query<&bevy::text::EditableText>,
    root: Res<PersistenceRoot>,
    repo: Res<RepoBrowser>,
    mut store: ResMut<OverrideStore>,
    mut draft: ResMut<OverrideDraft>,
) {
    let typing = focus
        .as_deref()
        .and_then(|focus| focus.get())
        .is_some_and(|entity| editables.get(entity).is_ok());
    let editing = draft.case.is_some();
    let mut submit = false;
    let mut cancel = false;
    let mut open = false;
    let mut delete = false;
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        match input.key_code {
            KeyCode::Enter if editing => submit = true,
            KeyCode::Tab if editing => cancel = true,
            KeyCode::Backspace if editing => {
                draft.text.pop();
            }
            KeyCode::KeyO if !editing && !typing => open = true,
            KeyCode::KeyX if !editing && !typing => delete = true,
            _ => {
                if editing && let Key::Character(text) = &input.logical_key {
                    draft.text.push_str(text);
                }
            }
        }
    }
    if editing {
        if cancel {
            *draft = OverrideDraft::default();
            return;
        }
        if !submit {
            return;
        }
        let Some(site) = draft.case.clone() else {
            return;
        };
        let text = draft.text.trim().to_string();
        if text.is_empty() {
            *draft = OverrideDraft::default();
            return;
        }
        if let Err(reason) = geometry_check(&site.case.event, site.size, &text) {
            draft.error = Some(reason);
            return;
        }
        match write_override(&root.0, &site.case, &text) {
            Ok(()) => {
                store.0.insert(site.case, text);
                *draft = OverrideDraft::default();
            }
            Err(reason) => draft.error = Some(reason),
        }
        return;
    }
    let Some(site) = case_site_of(&repo) else {
        return;
    };
    if open {
        let seed_alg = repo
            .node()
            .and_then(|node| node.case.as_ref())
            .map(|case| case.alg.clone())
            .unwrap_or_default();
        draft.case = Some(site.clone());
        draft.text = store.0.get(&site.case).cloned().unwrap_or(seed_alg);
        draft.error = None;
    }
    if delete && store.0.contains_key(&site.case) {
        match remove_override(&root.0, &site.case) {
            Ok(_) => {
                store.0.remove(&site.case);
            }
            Err(reason) => draft.error = Some(reason),
        }
    }
}

pub fn repo_detail_lines(
    browser: &RepoBrowser,
    store: &OverrideStore,
    draft: &OverrideDraft,
) -> Vec<DetailLine> {
    let site = case_site_of(browser);
    let mut lines = vec![DetailLine::Meta(breadcrumb(browser))];
    let Some(node) = browser.node() else {
        lines.push(DetailLine::Meta("no repository".to_string()));
        return lines;
    };
    lines.push(DetailLine::Title(node.label.clone()));
    match node.kind {
        RepoKind::Letters => lines.push(DetailLine::Meta("the 3bld pair browser".to_string())),
        RepoKind::Case => {
            let Some(case) = &node.case else {
                return lines;
            };
            let Some(site) = site else {
                lines.push(DetailLine::Alg(case.alg.clone()));
                lines.push(DetailLine::Meta(format!(
                    "source {} {}",
                    case.source_url, case.source_at
                )));
                return lines;
            };
            let effective = effective_alg(&site.case, &case.alg, store);
            lines.push(DetailLine::Alg(effective.alg.clone()));
            lines.push(DetailLine::Meta(if effective.overridden {
                "override".to_string()
            } else {
                "seed".to_string()
            }));
            if let Some(line) = case_expand_line(&site.case.event, &effective.alg) {
                lines.push(DetailLine::Meta(line));
            }
            if let Err(reason) = geometry_check(&site.case.event, site.size, &effective.alg) {
                lines.push(DetailLine::Warn(reason));
            }
            if draft
                .case
                .as_ref()
                .is_some_and(|drafted| drafted.case == site.case)
            {
                lines.push(DetailLine::Meta("editing".to_string()));
                lines.push(DetailLine::Alg(draft.text.clone()));
            }
            lines.push(DetailLine::Meta(format!(
                "source {} {}",
                case.source_url, case.source_at
            )));
        }
        RepoKind::Method => lines.push(DetailLine::Meta(format!(
            "stages {} cases {}",
            count_kind(node, RepoKind::Stage),
            count_kind(node, RepoKind::Case)
        ))),
        RepoKind::Stage => lines.push(DetailLine::Meta(format!(
            "cases {}",
            count_kind(node, RepoKind::Case)
        ))),
        RepoKind::Event | RepoKind::Size => lines.push(DetailLine::Meta(format!(
            "methods {} stages {} cases {}",
            count_kind(node, RepoKind::Method),
            count_kind(node, RepoKind::Stage),
            count_kind(node, RepoKind::Case)
        ))),
    }
    if let Some(reason) = &draft.error {
        lines.push(DetailLine::Warn(reason.clone()));
    }
    lines
}

pub fn spawn_seed_detail(
    detail: &mut ChildSpawnerCommands,
    fonts: &FontBook,
    browser: &RepoBrowser,
    store: &OverrideStore,
    draft: &OverrideDraft,
) {
    for line in repo_detail_lines(browser, store, draft) {
        match line {
            DetailLine::Title(text) => {
                crate::present::spawn_line(
                    detail,
                    fonts,
                    &text,
                    theme::FONT_TITLE,
                    theme::text_primary(),
                    AlgsDetailLine,
                );
            }
            DetailLine::Alg(text) => {
                crate::present::spawn_line(
                    detail,
                    fonts,
                    &text,
                    theme::FONT_ALG,
                    theme::text_primary(),
                    AlgsDetailLine,
                );
            }
            DetailLine::Meta(text) => {
                crate::present::spawn_line(
                    detail,
                    fonts,
                    &text,
                    theme::FONT_SMALL,
                    theme::text_muted(),
                    AlgsDetailLine,
                );
            }
            DetailLine::Warn(text) => {
                crate::present::spawn_line(
                    detail,
                    fonts,
                    &text,
                    theme::FONT_SMALL,
                    theme::danger(),
                    AlgsDetailLine,
                );
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::views::algs::browse::RepoBrowser;
    use crate::views::algs::browse::testkit::{armed_browser, skewb_sarah_rows};
    use crate::views::algs::browse::{repo_deeper, repo_move};

    fn lines_of(browser: &RepoBrowser) -> Vec<String> {
        repo_detail_lines(
            browser,
            &OverrideStore::default(),
            &OverrideDraft::default(),
        )
        .into_iter()
        .map(|line| match line {
            DetailLine::Title(text)
            | DetailLine::Alg(text)
            | DetailLine::Meta(text)
            | DetailLine::Warn(text) => text,
        })
        .collect()
    }

    fn bigcube_case(site_case: &str) -> RepoBrowser {
        let mut browser = armed_browser();
        repo_deeper(&mut browser);
        repo_move(&mut browser, 2);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        assert_eq!(browser.node().expect("case node").label, site_case);
        browser
    }

    #[test]
    fn letters_arm_detail_names_the_pair_browser() {
        let mut browser = armed_browser();
        repo_deeper(&mut browser);
        assert!(browser.repo_focused);
        assert_eq!(
            lines_of(&browser),
            vec![
                "repo".to_string(),
                "letters".to_string(),
                "the 3bld pair browser".to_string(),
            ]
        );
    }

    #[test]
    fn event_arm_detail_summarizes_its_subtree() {
        let mut browser = armed_browser();
        repo_deeper(&mut browser);
        repo_move(&mut browser, 1);
        assert_eq!(
            lines_of(&browser),
            vec![
                "repo".to_string(),
                "2x2".to_string(),
                "methods 1 stages 1 cases 2".to_string(),
            ]
        );
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        assert_eq!(
            lines_of(&browser).last(),
            Some(&"cases 2".to_string()),
            "stage detail reports its case count"
        );
    }

    #[test]
    fn sized_event_summarizes_across_covering_packs() {
        let mut browser = armed_browser();
        repo_deeper(&mut browser);
        repo_move(&mut browser, 2);
        repo_deeper(&mut browser);
        repo_move(&mut browser, 1);
        assert_eq!(
            lines_of(&browser),
            vec![
                "repo > bigcube".to_string(),
                "5x5".to_string(),
                "methods 2 stages 2 cases 2".to_string(),
            ]
        );
    }

    #[test]
    fn case_detail_shows_alg_seed_badge_and_source() {
        let browser = bigcube_case("par.1");
        let lines = repo_detail_lines(
            &browser,
            &OverrideStore::default(),
            &OverrideDraft::default(),
        );
        assert_eq!(
            lines[0],
            DetailLine::Meta("repo > bigcube > 4x4 > yau > parity".to_string())
        );
        assert_eq!(lines[1], DetailLine::Title("par.1".to_string()));
        assert_eq!(lines[2], DetailLine::Alg("alg par.1".to_string()));
        assert_eq!(lines[3], DetailLine::Meta("seed".to_string()));
        assert!(
            matches!(lines[4], DetailLine::Warn(_)),
            "a seed alg that fails tier 1 shows a warning: {lines:?}"
        );
        assert_eq!(
            lines[5],
            DetailLine::Meta("source https://example.test 2026-01-01".to_string())
        );
    }

    #[test]
    fn letters_browser_keeps_no_repository_lines() {
        let browser = RepoBrowser::default();
        assert_eq!(
            lines_of(&browser),
            vec![
                "repo".to_string(),
                "letters".to_string(),
                "the 3bld pair browser".to_string(),
            ]
        );
    }

    #[test]
    fn case_site_walks_the_cascade_into_a_full_reference() {
        let browser = bigcube_case("par.1");
        assert_eq!(
            case_site_of(&browser),
            Some(CaseSite {
                case: CaseRef {
                    event: "bigcube".to_string(),
                    method: "yau".to_string(),
                    stage: "parity".to_string(),
                    case_key: "par.1".to_string(),
                },
                size: Some(4),
            })
        );
        let mut browser = armed_browser();
        repo_deeper(&mut browser);
        repo_move(&mut browser, 1);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        assert_eq!(
            case_site_of(&browser),
            Some(CaseSite {
                case: CaseRef {
                    event: "2x2".to_string(),
                    method: "cll".to_string(),
                    stage: "cll".to_string(),
                    case_key: "cll.1".to_string(),
                },
                size: None,
            })
        );
    }

    #[test]
    fn case_site_is_none_above_the_case_level() {
        let mut browser = armed_browser();
        repo_deeper(&mut browser);
        assert_eq!(case_site_of(&browser), None);
    }

    #[test]
    fn bld_case_expand_line_gates_on_event_and_cancellation() {
        assert_eq!(
            case_expand_line("3bld", "R R U").as_deref(),
            Some("expand R2 U")
        );
        assert_eq!(case_expand_line("3bld", "R U"), None);
        assert_eq!(case_expand_line("2x2", "R R U"), None);
        assert_eq!(case_expand_line("3bld", "junk"), None);
    }

    #[test]
    fn tier1_gate_accepts_clean_cube_algs_and_rejects_bad_ones() {
        assert_eq!(geometry_check("2x2", None, "R U R'"), Ok(()));
        assert_eq!(geometry_check("bigcube", Some(5), "Rw U Rw'"), Ok(()));
        assert!(geometry_check("3x3", None, "QQ").is_err());
        assert!(geometry_check("bigcube", None, "R U R'").is_ok());
        assert_eq!(
            geometry_check("clock", None, "R U R'"),
            Err("no tier 1 geometry for event clock".to_string())
        );
    }

    #[test]
    fn every_skewb_sarah_seed_row_passes_the_tier1_gate() {
        let rows = skewb_sarah_rows();
        assert_eq!(rows.len(), 15);
        for row in &rows {
            assert_eq!(
                geometry_check("skewb", None, &row.alg),
                Ok(()),
                "{} holds {}",
                row.case_key,
                row.alg
            );
        }
    }

    #[test]
    fn a_skewb_override_with_only_rotations_is_rejected_as_empty() {
        assert_eq!(
            geometry_check("skewb", None, "y2 z x'"),
            Err("alg is empty after rotation stripping".to_string())
        );
    }

    #[test]
    fn effective_alg_prefers_the_override_over_the_seed_row() {
        let case = CaseRef {
            event: "2x2".to_string(),
            method: "cll".to_string(),
            stage: "cll".to_string(),
            case_key: "cll.1".to_string(),
        };
        let mut store = OverrideStore::default();
        assert_eq!(
            effective_alg(&case, "R U R'", &store),
            EffectiveAlg {
                alg: "R U R'".to_string(),
                overridden: false,
            }
        );
        store.0.insert(case.clone(), "F R F'".to_string());
        assert_eq!(
            effective_alg(&case, "R U R'", &store),
            EffectiveAlg {
                alg: "F R F'".to_string(),
                overridden: true,
            }
        );
    }

    #[test]
    fn case_detail_marks_the_override_and_warns_on_a_broken_effective_alg() {
        let browser = bigcube_case("par.1");
        let case = CaseRef {
            event: "bigcube".to_string(),
            method: "yau".to_string(),
            stage: "parity".to_string(),
            case_key: "par.1".to_string(),
        };
        let mut store = OverrideStore::default();
        store.0.insert(case.clone(), "R U R'".to_string());
        let lines = repo_detail_lines(&browser, &store, &OverrideDraft::default());
        assert_eq!(lines[2], DetailLine::Alg("R U R'".to_string()));
        assert_eq!(lines[3], DetailLine::Meta("override".to_string()));
        assert_eq!(
            lines[4],
            DetailLine::Meta("source https://example.test 2026-01-01".to_string())
        );

        store.0.insert(case, "QQ".to_string());
        let lines = repo_detail_lines(&browser, &store, &OverrideDraft::default());
        assert!(matches!(lines[4], DetailLine::Warn(_)));

        let draft = OverrideDraft {
            case: case_site_of(&browser),
            text: "F R F'".to_string(),
            ..OverrideDraft::default()
        };
        let lines = repo_detail_lines(&browser, &OverrideStore::default(), &draft);
        assert!(lines.contains(&DetailLine::Meta("editing".to_string())));
        assert!(lines.contains(&DetailLine::Alg("F R F'".to_string())));
    }
}
