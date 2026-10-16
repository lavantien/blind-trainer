use crate::present::FontBook;
use crate::theme;
use crate::views::algs::AlgsDetailLine;
use crate::views::algs::browse::{RepoBrowser, RepoKind, breadcrumb, count_kind};
use bevy::prelude::*;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum DetailLine {
    Title(String),
    Alg(String),
    Meta(String),
}

pub fn repo_detail_lines(browser: &RepoBrowser) -> Vec<DetailLine> {
    let mut lines = vec![DetailLine::Meta(breadcrumb(browser))];
    let Some(node) = browser.node() else {
        lines.push(DetailLine::Meta("no repository".to_string()));
        return lines;
    };
    lines.push(DetailLine::Title(node.label.clone()));
    match node.kind {
        RepoKind::Letters => lines.push(DetailLine::Meta("the 3bld pair browser".to_string())),
        RepoKind::Case => {
            if let Some(case) = &node.case {
                lines.push(DetailLine::Alg(case.alg.clone()));
                lines.push(DetailLine::Meta(format!(
                    "source {} {}",
                    case.source_url, case.source_at
                )));
            }
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
    lines
}

pub fn spawn_seed_detail(
    detail: &mut ChildSpawnerCommands,
    fonts: &FontBook,
    browser: &RepoBrowser,
) {
    for line in repo_detail_lines(browser) {
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
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::views::algs::browse::RepoBrowser;
    use crate::views::algs::browse::testkit::armed_browser;
    use crate::views::algs::browse::{repo_deeper, repo_move, repo_shallower};

    fn lines_of(browser: &RepoBrowser) -> Vec<String> {
        repo_detail_lines(browser)
            .into_iter()
            .map(|line| match line {
                DetailLine::Title(text) | DetailLine::Alg(text) | DetailLine::Meta(text) => text,
            })
            .collect()
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
    fn case_detail_shows_alg_and_source() {
        let mut browser = armed_browser();
        repo_deeper(&mut browser);
        repo_move(&mut browser, 2);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        repo_deeper(&mut browser);
        assert_eq!(browser.depth, 4);
        let lines = repo_detail_lines(&browser);
        assert_eq!(
            lines[0],
            DetailLine::Meta("repo > bigcube > 4x4 > yau > parity".to_string())
        );
        assert_eq!(lines[1], DetailLine::Title("par.1".to_string()));
        assert_eq!(lines[2], DetailLine::Alg("alg par.1".to_string()));
        assert_eq!(
            lines[3],
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
        let mut armed = armed_browser();
        repo_deeper(&mut armed);
        repo_shallower(&mut armed);
        assert!(!armed.repo_focused);
    }
}
