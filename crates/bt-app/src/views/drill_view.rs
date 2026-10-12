use super::drill::{CardContent, DrillSession, RowStyle};
use crate::present::{FontBook, spawn_line, spawn_spans};
use crate::states::{AppState, DrillPhase};
use crate::theme;
use crate::theme::{
    FONT_ALG, FONT_BODY, FONT_FOCAL, FONT_HEADING, FONT_MICRO, FONT_SMALL, SPACING_REM,
};
use bevy::prelude::*;
use bevy::state::state_scoped::DespawnOnExit;

impl RowStyle {
    pub fn metrics(self) -> (f32, Color) {
        match self {
            RowStyle::Focal => (FONT_FOCAL, theme::text_primary()),
            RowStyle::Muted => (FONT_BODY, theme::text_muted()),
            RowStyle::Heading => (FONT_HEADING, theme::text_primary()),
            RowStyle::Alg => (FONT_ALG, theme::text_primary()),
            RowStyle::Body => (FONT_BODY, theme::text_primary()),
            RowStyle::Small => (FONT_SMALL, theme::text_muted()),
            RowStyle::Micro => (FONT_MICRO, theme::text_muted()),
        }
    }
}

#[derive(Component)]
pub struct DrillCardRoot;

#[derive(Component)]
pub struct CardPanel;

#[derive(Component)]
pub struct RepsPanel;

#[derive(Component)]
pub struct RatePanel;

#[derive(Component)]
pub enum RepsField {
    Letters,
    Alg,
    Count,
}

#[derive(Component)]
pub struct RepsTimer;

#[derive(Component)]
pub struct RatePrompt;

#[derive(Component)]
pub struct DrillLine;

pub fn refresh<'a>(texts: impl Iterator<Item = Mut<'a, Text>>, shown: &str) {
    for mut text in texts {
        if text.0 != shown {
            *text = Text::new(shown.to_string());
        }
    }
}

fn panel_node() -> Node {
    Node {
        flex_direction: FlexDirection::Column,
        row_gap: theme::rem(SPACING_REM),
        ..default()
    }
}

fn spawn_panel<M: Component, B: FnOnce(&mut ChildSpawnerCommands)>(
    commands: &mut Commands,
    root: Entity,
    phase: DrillPhase,
    marker: M,
    build: B,
) {
    commands.entity(root).with_children(|parent| {
        parent
            .spawn((DespawnOnExit(phase), marker, panel_node()))
            .with_children(build);
    });
}

pub fn spawn_card(commands: &mut Commands, root: Entity, fonts: &FontBook, content: &CardContent) {
    spawn_panel(commands, root, DrillPhase::Card, CardPanel, |panel| {
        for (text, style) in &content.rows {
            if *style == RowStyle::Alg && content.mechanics.is_some() {
                continue;
            }
            let (size, color) = style.metrics();
            spawn_line(panel, fonts, text, size, color, DrillLine);
        }
        if let Some(spans) = &content.mechanics {
            let refs: Vec<(&str, f32, Color)> = spans
                .iter()
                .map(|(text, size, color)| (text.as_str(), *size, *color))
                .collect();
            panel
                .spawn((
                    DrillLine,
                    Node {
                        flex_direction: FlexDirection::Column,
                        ..default()
                    },
                ))
                .with_children(|mechanics| {
                    spawn_spans(mechanics, fonts, &refs);
                });
        }
    });
}

pub fn spawn_reps_panel(
    commands: &mut Commands,
    root: Entity,
    fonts: &FontBook,
    session: &DrillSession,
) {
    let (letters, alg) = match session.current_side() {
        Some(side) => (side.letters(), side.alg_text.clone()),
        None => (String::new(), String::new()),
    };
    let count = format!("rep {} of {}", session.reps_done, session.reps_target);
    spawn_panel(commands, root, DrillPhase::Reps, RepsPanel, |panel| {
        spawn_line(
            panel,
            fonts,
            &letters,
            FONT_FOCAL,
            theme::text_primary(),
            RepsField::Letters,
        );
        spawn_line(panel, fonts, &alg, FONT_ALG, theme::teal(), RepsField::Alg);
        spawn_line(
            panel,
            fonts,
            &count,
            FONT_HEADING,
            theme::text_primary(),
            RepsField::Count,
        );
        spawn_line(
            panel,
            fonts,
            "0s",
            FONT_HEADING,
            theme::text_muted(),
            RepsTimer,
        );
        spawn_line(
            panel,
            fonts,
            "space each rep",
            FONT_SMALL,
            theme::text_muted(),
            DrillLine,
        );
    });
}

pub fn spawn_rate_panel(
    commands: &mut Commands,
    root: Entity,
    fonts: &FontBook,
    session: &DrillSession,
) {
    let prompt = match session.sides() {
        Some((a, b)) => {
            let side = if session.rate_step == 0 { a } else { b };
            format!("rate {}", side.letters())
        }
        None => "no pair".to_string(),
    };
    let total: f32 = session.splits.iter().map(|(_, secs)| secs).sum();
    let avg = if session.splits.is_empty() {
        0.0
    } else {
        total / session.splits.len() as f32
    };
    let summary = format!("{} reps avg {:.1}s", session.splits.len(), avg);
    spawn_panel(commands, root, DrillPhase::Rate, RatePanel, |panel| {
        spawn_line(
            panel,
            fonts,
            &prompt,
            FONT_FOCAL,
            theme::text_primary(),
            RatePrompt,
        );
        spawn_line(
            panel,
            fonts,
            &summary,
            FONT_HEADING,
            theme::text_primary(),
            DrillLine,
        );
        spawn_line(
            panel,
            fonts,
            "1 again 2 hard 3 good 4 easy",
            FONT_SMALL,
            theme::text_muted(),
            DrillLine,
        );
    });
}

pub fn spawn(
    commands: &mut Commands,
    fonts: &FontBook,
    config: &crate::logic::AppConfig,
) -> Entity {
    let content = crate::present::spawn_view_frame(
        commands,
        fonts,
        AppState::Drill,
        "drill",
        "space reps, 1..4 rate, esc menu",
    );
    commands.entity(content).insert(Node {
        width: Val::Percent(100.0),
        flex_grow: 1.0,
        flex_direction: FlexDirection::Row,
        column_gap: theme::rem(SPACING_REM),
        ..default()
    });
    commands.entity(content).with_children(|parent| {
        parent
            .spawn(Node {
                width: Val::Percent(theme::DRILL_NET_WIDTH_PCT * 100.0),
                align_items: AlignItems::Center,
                justify_content: JustifyContent::Center,
                ..default()
            })
            .with_children(|region| {
                crate::net::spawn_net(region, crate::present::drill_net_geometry(config));
            });
        parent.spawn((
            DrillCardRoot,
            Node {
                flex_grow: 1.0,
                flex_direction: FlexDirection::Column,
                justify_content: JustifyContent::Center,
                row_gap: theme::rem(SPACING_REM),
                ..default()
            },
        ));
    });
    content
}
