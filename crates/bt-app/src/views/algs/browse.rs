use crate::persist::Stores;
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bt_data::seedpack::{NSpec, PackEntry, SeedRow};
use std::collections::BTreeSet;

pub const LETTERS_LABEL: &str = "letters";

#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub enum RepoKind {
    #[default]
    Letters,
    Event,
    Size,
    Method,
    Stage,
    Case,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct RepoCase {
    pub alg: String,
    pub source_url: String,
    pub source_at: String,
}

#[derive(Clone, Debug, PartialEq, Eq, Default)]
pub struct RepoNode {
    pub label: String,
    pub kind: RepoKind,
    pub children: Vec<RepoNode>,
    pub case: Option<RepoCase>,
}

#[derive(Clone, Debug, Resource)]
pub struct RepoBrowser {
    pub arms: Vec<RepoNode>,
    pub arm: usize,
    pub depth: usize,
    pub path: Vec<usize>,
    pub repo_focused: bool,
}

impl Default for RepoBrowser {
    fn default() -> RepoBrowser {
        RepoBrowser {
            arms: vec![RepoNode {
                label: LETTERS_LABEL.to_string(),
                kind: RepoKind::Letters,
                children: Vec::new(),
                case: None,
            }],
            arm: 0,
            depth: 0,
            path: Vec::new(),
            repo_focused: false,
        }
    }
}

impl RepoBrowser {
    pub fn letters_focused(&self) -> bool {
        !self.repo_focused
    }

    pub fn has_seed_arms(&self) -> bool {
        self.arms.len() > 1
    }

    pub fn node(&self) -> Option<&RepoNode> {
        node_at(&self.arms, self.arm, &self.path)
    }
}

pub fn node_at<'a>(arms: &'a [RepoNode], arm: usize, path: &[usize]) -> Option<&'a RepoNode> {
    let mut node = arms.get(arm)?;
    for step in path {
        node = node.children.get(*step)?;
    }
    Some(node)
}

fn size_covers(n: &NSpec, size: i64) -> bool {
    match n {
        NSpec::Unset => false,
        NSpec::Fixed(fixed) => *fixed == size,
        NSpec::Sizes(sizes) => sizes.contains(&size),
    }
}

fn event_sizes(packs: &[&PackEntry]) -> Vec<i64> {
    let mut sizes: BTreeSet<i64> = BTreeSet::new();
    for pack in packs {
        match &pack.n {
            NSpec::Fixed(size) => {
                sizes.insert(*size);
            }
            NSpec::Sizes(list) => {
                for size in list {
                    sizes.insert(*size);
                }
            }
            NSpec::Unset => {}
        }
    }
    sizes.into_iter().collect()
}

fn stage_node(pack: &PackEntry, rows: &[SeedRow]) -> RepoNode {
    let cases = rows
        .iter()
        .filter(|row| {
            row.event == pack.event && row.method == pack.method && row.stage == pack.stage
        })
        .map(|row| RepoNode {
            label: row.case_key.clone(),
            kind: RepoKind::Case,
            children: Vec::new(),
            case: Some(RepoCase {
                alg: row.alg.clone(),
                source_url: row.source_url.clone(),
                source_at: row.source_at.clone(),
            }),
        })
        .collect();
    RepoNode {
        label: pack.stage.clone(),
        kind: RepoKind::Stage,
        children: cases,
        case: None,
    }
}

fn method_node(pack: &PackEntry, rows: &[SeedRow]) -> RepoNode {
    RepoNode {
        label: pack.method.clone(),
        kind: RepoKind::Method,
        children: vec![stage_node(pack, rows)],
        case: None,
    }
}

fn merge_method(target: &mut RepoNode, pack: &PackEntry, rows: &[SeedRow]) {
    if let Some(existing) = target
        .children
        .iter_mut()
        .find(|child| child.label == pack.method)
    {
        existing.children.push(stage_node(pack, rows));
    } else {
        target.children.push(method_node(pack, rows));
    }
}

fn sized_event_node(event: &str, packs: &[&PackEntry], rows: &[SeedRow]) -> RepoNode {
    let mut node = RepoNode {
        label: event.to_string(),
        kind: RepoKind::Event,
        children: Vec::new(),
        case: None,
    };
    for size in event_sizes(packs) {
        let mut size_node = RepoNode {
            label: format!("{size}x{size}"),
            kind: RepoKind::Size,
            children: Vec::new(),
            case: None,
        };
        for pack in packs.iter().filter(|pack| size_covers(&pack.n, size)) {
            merge_method(&mut size_node, pack, rows);
        }
        node.children.push(size_node);
    }
    node
}

pub fn repo_arms(
    packs: &[PackEntry],
    hidden: &BTreeSet<String>,
    rows: &[SeedRow],
) -> Vec<RepoNode> {
    let mut arms = RepoBrowser::default().arms;
    let mut order: Vec<String> = Vec::new();
    let mut grouped: Vec<Vec<&PackEntry>> = Vec::new();
    for pack in packs {
        if hidden.contains(&pack.event) {
            continue;
        }
        match order.iter().position(|event| event == &pack.event) {
            Some(index) => grouped[index].push(pack),
            None => {
                order.push(pack.event.clone());
                grouped.push(vec![pack]);
            }
        }
    }
    for (event, packs) in order.iter().zip(grouped) {
        let sized = packs.iter().any(|pack| !matches!(pack.n, NSpec::Unset));
        let node = if sized {
            sized_event_node(event, &packs, rows)
        } else {
            let mut node = RepoNode {
                label: event.clone(),
                kind: RepoKind::Event,
                children: Vec::new(),
                case: None,
            };
            for pack in packs {
                merge_method(&mut node, pack, rows);
            }
            node
        };
        arms.push(node);
    }
    arms
}

pub fn list_at(browser: &RepoBrowser) -> Vec<&RepoNode> {
    if browser.depth == 0 {
        return browser.arms.iter().collect();
    }
    match node_at(
        &browser.arms,
        browser.arm,
        &browser.path[..browser.depth - 1],
    ) {
        Some(node) => node.children.iter().collect(),
        None => Vec::new(),
    }
}

pub fn selection(browser: &RepoBrowser) -> Option<usize> {
    if browser.depth == 0 {
        Some(browser.arm)
    } else {
        browser.path.last().copied()
    }
}

pub fn repo_move(browser: &mut RepoBrowser, step: i32) {
    let len = list_at(browser).len() as i32;
    if len == 0 {
        return;
    }
    let current = selection(browser).unwrap_or(0) as i32;
    let next = ((current + step).rem_euclid(len)) as usize;
    if browser.depth == 0 {
        browser.arm = next;
    } else {
        *browser
            .path
            .last_mut()
            .expect("depth above zero holds a path") = next;
    }
}

pub fn repo_deeper(browser: &mut RepoBrowser) {
    if !browser.repo_focused {
        if browser.has_seed_arms() {
            browser.repo_focused = true;
            browser.arm = 0;
            browser.depth = 0;
            browser.path.clear();
        }
        return;
    }
    if browser.depth == 0
        && browser
            .arms
            .get(browser.arm)
            .is_some_and(|arm| arm.kind == RepoKind::Letters || arm.children.is_empty())
    {
        browser.repo_focused = false;
        browser.path.clear();
        return;
    }
    let has_children = browser.node().is_some_and(|node| !node.children.is_empty());
    if has_children {
        browser.depth += 1;
        browser.path.push(0);
    }
}

pub fn repo_shallower(browser: &mut RepoBrowser) {
    if !browser.repo_focused {
        return;
    }
    if browser.depth == 0 {
        browser.repo_focused = false;
        browser.path.clear();
        return;
    }
    browser.depth -= 1;
    browser.path.pop();
}

pub fn repo_list_rows(browser: &RepoBrowser, capacity: usize) -> Vec<(String, bool)> {
    let entries = list_at(browser);
    if entries.is_empty() {
        return vec![(String::new(), false)];
    }
    let selected = selection(browser).unwrap_or(0);
    let window = super::list_window(entries.len(), selected, capacity);
    window
        .map(|index| {
            let caret = if index == selected { ">" } else { " " };
            (
                format!("{caret} {}", entries[index].label),
                index == selected,
            )
        })
        .collect()
}

pub fn breadcrumb(browser: &RepoBrowser) -> String {
    let mut parts = vec!["repo".to_string()];
    if browser.depth > 0 {
        if let Some(arm) = browser.arms.get(browser.arm) {
            parts.push(arm.label.clone());
        }
        for depth in 1..browser.depth {
            if let Some(node) = node_at(&browser.arms, browser.arm, &browser.path[..depth]) {
                parts.push(node.label.clone());
            }
        }
    }
    parts.join(" > ")
}

pub fn count_kind(node: &RepoNode, kind: RepoKind) -> usize {
    let self_hit = usize::from(node.kind == kind);
    node.children
        .iter()
        .map(|child| count_kind(child, kind))
        .sum::<usize>()
        + self_hit
}

pub fn repo_enter(stores: Res<Stores>, mut browser: ResMut<RepoBrowser>) {
    browser.arms = repo_arms(
        &stores.seeds.packs,
        &stores.seeds.hidden,
        &stores.seeds.rows,
    );
    browser.arm = 0;
    browser.depth = 0;
    browser.path.clear();
    browser.repo_focused = false;
}

pub fn repo_keys(
    mut inputs: MessageReader<KeyboardInput>,
    focus: Option<Res<bevy::input_focus::InputFocus>>,
    editables: Query<&bevy::text::EditableText>,
    mut browser: ResMut<RepoBrowser>,
) {
    let typing = focus
        .as_deref()
        .and_then(|focus| focus.get())
        .is_some_and(|entity| editables.get(entity).is_ok());
    let mut step = 0i32;
    let mut deeper = false;
    let mut shallower = false;
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        match input.key_code {
            KeyCode::ArrowDown => step += 1,
            KeyCode::ArrowUp => step -= 1,
            KeyCode::KeyN if !typing => step += 1,
            KeyCode::KeyP if !typing => step -= 1,
            KeyCode::ArrowRight => deeper = true,
            KeyCode::ArrowLeft => shallower = true,
            _ => {}
        }
    }
    if step != 0 && browser.repo_focused {
        repo_move(&mut browser, step);
    }
    if deeper {
        repo_deeper(&mut browser);
    }
    if shallower {
        repo_shallower(&mut browser);
    }
}

#[cfg(test)]
pub(crate) mod testkit {
    use super::*;
    use std::collections::BTreeSet;

    pub fn pack(event: &str, method: &str, stage: &str, n: NSpec) -> PackEntry {
        PackEntry {
            file: format!("{event}_{method}_{stage}.tsv"),
            event: event.to_string(),
            method: method.to_string(),
            stage: stage.to_string(),
            n,
            expected_rows: 1,
        }
    }

    pub fn row(event: &str, method: &str, stage: &str, case_key: &str) -> SeedRow {
        SeedRow {
            event: event.to_string(),
            method: method.to_string(),
            stage: stage.to_string(),
            case_key: case_key.to_string(),
            alg: format!("alg {case_key}"),
            front_json: "{}".to_string(),
            source_url: "https://example.test".to_string(),
            source_at: "2026-01-01".to_string(),
            content_hash: String::new(),
        }
    }

    pub fn hidden(keys: &[&str]) -> BTreeSet<String> {
        keys.iter().map(|key| key.to_string()).collect()
    }

    pub fn fixture_packs() -> Vec<PackEntry> {
        vec![
            pack("2x2", "cll", "cll", NSpec::Unset),
            pack("bigcube", "yau", "parity", NSpec::Sizes(vec![4, 5, 6, 7])),
            pack(
                "bigcube",
                "reduction",
                "parity",
                NSpec::Sizes(vec![5, 6, 7]),
            ),
            pack("sq1", "lbl", "cubeshape", NSpec::Unset),
        ]
    }

    pub fn fixture_rows() -> Vec<SeedRow> {
        vec![
            row("2x2", "cll", "cll", "cll.1"),
            row("2x2", "cll", "cll", "cll.2"),
            row("bigcube", "yau", "parity", "par.1"),
            row("bigcube", "reduction", "parity", "red.1"),
        ]
    }

    pub fn armed_browser() -> RepoBrowser {
        let mut browser = RepoBrowser::default();
        browser.arms = repo_arms(&fixture_packs(), &hidden(&["sq1"]), &fixture_rows());
        browser
    }
}

#[cfg(test)]
mod tests {
    use super::testkit::{armed_browser, fixture_packs, fixture_rows, hidden, pack};
    use super::*;

    #[test]
    fn arms_catalog_letters_first_then_registry_events_excluding_hidden() {
        let arms = repo_arms(&fixture_packs(), &hidden(&["sq1"]), &fixture_rows());
        assert_eq!(arms.len(), 3);
        assert_eq!(arms[0].label, "letters");
        assert_eq!(arms[0].kind, RepoKind::Letters);
        assert_eq!(arms[1].label, "2x2");
        assert_eq!(arms[2].label, "bigcube");
        assert!(arms.iter().all(|arm| arm.label != "sq1"));
    }

    #[test]
    fn plain_events_hold_methods_then_stages_then_cases() {
        let arms = repo_arms(&fixture_packs(), &hidden(&["sq1"]), &fixture_rows());
        let two_by = &arms[1];
        assert_eq!(two_by.children.len(), 1);
        assert_eq!(two_by.children[0].label, "cll");
        assert_eq!(two_by.children[0].kind, RepoKind::Method);
        let stage = &two_by.children[0].children[0];
        assert_eq!(stage.label, "cll");
        assert_eq!(stage.kind, RepoKind::Stage);
        assert_eq!(
            stage
                .children
                .iter()
                .map(|case| case.label.as_str())
                .collect::<Vec<_>>(),
            vec!["cll.1", "cll.2"]
        );
        assert_eq!(stage.children[0].kind, RepoKind::Case);
        assert_eq!(
            stage.children[0].case.as_ref().expect("case payload").alg,
            "alg cll.1"
        );
    }

    #[test]
    fn bigcube_expands_sorted_sizes_with_covering_methods_in_registry_order() {
        let arms = repo_arms(&fixture_packs(), &hidden(&["sq1"]), &fixture_rows());
        let bigcube = &arms[2];
        let labels: Vec<&str> = bigcube
            .children
            .iter()
            .map(|size| size.label.as_str())
            .collect();
        assert_eq!(labels, vec!["4x4", "5x5", "6x6", "7x7"]);
        assert!(
            bigcube
                .children
                .iter()
                .all(|size| size.kind == RepoKind::Size)
        );
        let four = &bigcube.children[0];
        let four_methods: Vec<&str> = four
            .children
            .iter()
            .map(|method| method.label.as_str())
            .collect();
        assert_eq!(four_methods, vec!["yau"]);
        let five = &bigcube.children[1];
        let five_methods: Vec<&str> = five
            .children
            .iter()
            .map(|method| method.label.as_str())
            .collect();
        assert_eq!(five_methods, vec!["yau", "reduction"]);
        let five_reduction_case = &five.children[1].children[0].children[0];
        assert_eq!(five_reduction_case.label, "red.1");
    }

    #[test]
    fn fixed_sizes_join_the_size_level_of_a_sized_event() {
        let packs = vec![
            pack("bigcube", "yau", "parity", NSpec::Sizes(vec![4, 5])),
            pack("bigcube", "free", "parity", NSpec::Fixed(4)),
        ];
        let arms = repo_arms(&packs, &hidden(&[]), &[]);
        let bigcube = &arms[1];
        let labels: Vec<&str> = bigcube
            .children
            .iter()
            .map(|size| size.label.as_str())
            .collect();
        assert_eq!(labels, vec!["4x4", "5x5"]);
        let four: Vec<&str> = bigcube.children[0]
            .children
            .iter()
            .map(|method| method.label.as_str())
            .collect();
        assert_eq!(four, vec!["yau", "free"]);
        let five: Vec<&str> = bigcube.children[1]
            .children
            .iter()
            .map(|method| method.label.as_str())
            .collect();
        assert_eq!(five, vec!["yau"]);
    }

    #[test]
    fn empty_registry_leaves_the_letters_arm_alone() {
        let arms = repo_arms(&[], &hidden(&["2x2"]), &[]);
        assert_eq!(arms.len(), 1);
        assert_eq!(arms[0].label, "letters");
    }

    #[test]
    fn right_from_letters_enters_the_repo_only_when_seed_arms_exist() {
        let mut browser = RepoBrowser::default();
        repo_deeper(&mut browser);
        assert!(!browser.repo_focused);

        let mut armed = armed_browser();
        repo_deeper(&mut armed);
        assert!(armed.repo_focused);
        assert_eq!(armed.arm, 0);
        assert_eq!(armed.depth, 0);
    }

    #[test]
    fn navigation_walks_the_full_bigcube_cascade() {
        let mut browser = armed_browser();
        repo_deeper(&mut browser);
        repo_move(&mut browser, 2);
        assert_eq!(browser.arms[browser.arm].label, "bigcube");
        repo_deeper(&mut browser);
        assert_eq!(browser.depth, 1);
        assert_eq!(browser.node().expect("size node").label, "4x4");
        repo_move(&mut browser, 1);
        assert_eq!(browser.node().expect("size node").label, "5x5");
        repo_deeper(&mut browser);
        assert_eq!(browser.depth, 2);
        assert_eq!(browser.node().expect("method node").label, "yau");
        repo_deeper(&mut browser);
        assert_eq!(browser.depth, 3);
        assert_eq!(browser.node().expect("stage node").label, "parity");
        repo_deeper(&mut browser);
        assert_eq!(browser.depth, 4);
        assert_eq!(browser.node().expect("case node").kind, RepoKind::Case);
        repo_deeper(&mut browser);
        assert_eq!(browser.depth, 4, "cases are the deepest level");
        repo_shallower(&mut browser);
        repo_shallower(&mut browser);
        repo_shallower(&mut browser);
        repo_shallower(&mut browser);
        assert_eq!(browser.depth, 0);
        repo_shallower(&mut browser);
        assert!(!browser.repo_focused);
    }

    #[test]
    fn arm_selection_wraps_and_right_on_letters_returns_letters_focus() {
        let mut browser = armed_browser();
        repo_deeper(&mut browser);
        for _ in 0..3 {
            repo_move(&mut browser, 1);
        }
        assert_eq!(browser.arm, 0);
        repo_deeper(&mut browser);
        assert!(!browser.repo_focused);
        assert!(browser.path.is_empty());
    }

    #[test]
    fn list_rows_mark_the_current_selection_with_a_caret() {
        let mut browser = armed_browser();
        repo_deeper(&mut browser);
        let rows = repo_list_rows(&browser, 10);
        assert_eq!(
            rows.iter()
                .map(|(label, _)| label.as_str())
                .collect::<Vec<_>>(),
            vec!["> letters", "  2x2", "  bigcube"]
        );
        repo_move(&mut browser, 2);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        let cases = repo_list_rows(&browser, 10);
        assert_eq!(cases.len(), 1);
        assert_eq!(cases[0], ("> par.1".to_string(), true));
    }

    #[test]
    fn breadcrumb_trails_the_walked_path() {
        let mut browser = armed_browser();
        repo_deeper(&mut browser);
        assert_eq!(breadcrumb(&browser), "repo");
        repo_move(&mut browser, 2);
        repo_deeper(&mut browser);
        repo_move(&mut browser, 1);
        repo_deeper(&mut browser);
        assert_eq!(breadcrumb(&browser), "repo > bigcube > 5x5");
    }

    #[test]
    fn subtree_counts_feed_the_detail_summary() {
        let arms = repo_arms(&fixture_packs(), &hidden(&["sq1"]), &fixture_rows());
        let two_by = &arms[1];
        assert_eq!(count_kind(two_by, RepoKind::Method), 1);
        assert_eq!(count_kind(two_by, RepoKind::Stage), 1);
        assert_eq!(count_kind(two_by, RepoKind::Case), 2);
        let bigcube = &arms[2];
        assert_eq!(count_kind(bigcube, RepoKind::Size), 4);
        assert_eq!(count_kind(bigcube, RepoKind::Method), 7);
        assert_eq!(count_kind(bigcube, RepoKind::Case), 7);
    }
}
