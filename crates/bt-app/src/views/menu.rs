use crate::present::{FontBook, spawn_line, spawn_spans, spawn_view_frame};
use crate::states::AppState;
use crate::theme;
use bevy::prelude::*;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct MenuRow {
    pub number: usize,
    pub label: &'static str,
    pub description: &'static str,
}

impl MenuRow {
    pub fn title(&self) -> String {
        format!("{} {}", self.number, self.label)
    }
}

pub fn state_description(state: AppState) -> &'static str {
    match state {
        AppState::Drill => "physical 3-style pair drilling with spaced repetition",
        AppState::Memo => "streaming memo on seeded scrambles",
        AppState::Solve => "the timed full cycle",
        AppState::Replay => "orbit the 3d solve replay",
        AppState::Algs => "browse and filter all 818 cases",
        AppState::Stats => "solve history and review load",
        AppState::Settings => "cadence, memo mode, reps",
        AppState::Bootstrap | AppState::Fatal | AppState::Menu => "",
    }
}

pub fn menu_rows() -> Vec<MenuRow> {
    AppState::MENU_KEYS
        .iter()
        .enumerate()
        .map(|(index, (_, state))| MenuRow {
            number: index + 1,
            label: state.label(),
            description: state_description(*state),
        })
        .collect()
}

pub fn warning_context(warnings: &[String]) -> String {
    warnings.join(" | ")
}

#[derive(Component, Default)]
pub struct MenuRowText;

pub fn spawn(commands: &mut Commands, fonts: &FontBook, warnings: &[String]) -> Entity {
    let context = warning_context(warnings);
    let content = spawn_view_frame(commands, fonts, AppState::Menu, "menu", &context);
    commands.entity(content).insert(Node {
        width: Val::Percent(100.0),
        flex_grow: 1.0,
        flex_direction: FlexDirection::Column,
        justify_content: JustifyContent::Center,
        align_items: AlignItems::FlexStart,
        row_gap: theme::rem(theme::SPACING_REM * 2.0),
        ..default()
    });
    commands.entity(content).with_children(|parent| {
        for row in menu_rows() {
            parent
                .spawn(Node {
                    flex_direction: FlexDirection::Column,
                    row_gap: theme::rem(theme::SPACING_REM * 0.5),
                    ..default()
                })
                .with_children(|item| {
                    let number = row.number.to_string();
                    spawn_spans(
                        item,
                        fonts,
                        &[
                            (number.as_str(), theme::FONT_HEADING, theme::accent()),
                            (" ", theme::FONT_HEADING, theme::text_primary()),
                            (row.label, theme::FONT_HEADING, theme::text_primary()),
                        ],
                    );
                    spawn_line(
                        item,
                        fonts,
                        row.description,
                        theme::FONT_SMALL,
                        theme::text_muted(),
                        MenuRowText,
                    );
                });
        }
    });
    content
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn menu_rows_list_every_key_with_its_description() {
        let rows = menu_rows();
        let titles: Vec<String> = rows.iter().map(MenuRow::title).collect();
        assert_eq!(
            titles,
            vec![
                "1 drill",
                "2 memo training",
                "3 solve",
                "4 algs",
                "5 stats",
                "6 settings",
            ]
        );
        assert_eq!(
            rows[0].description,
            "physical 3-style pair drilling with spaced repetition"
        );
        assert_eq!(rows[1].description, "streaming memo on seeded scrambles");
        assert_eq!(rows[2].description, "the timed full cycle");
        assert_eq!(rows[3].description, "browse and filter all 818 cases");
        assert_eq!(rows[4].description, "solve history and review load");
        assert_eq!(rows[5].description, "cadence, memo mode, reps");
    }

    #[test]
    fn warning_context_joins_boot_warnings() {
        assert_eq!(warning_context(&[]), "");
        assert_eq!(
            warning_context(&["vendor data missing".to_string()]),
            "vendor data missing"
        );
        assert_eq!(
            warning_context(&["a".to_string(), "b".to_string()]),
            "a | b"
        );
    }
}
