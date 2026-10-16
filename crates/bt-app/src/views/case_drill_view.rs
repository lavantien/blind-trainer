use crate::logic::AppConfig;
use crate::present::FontBook;
use crate::states::{AppState, CaseDrillPhase};
use crate::theme;
use crate::views::algs::display::{
    NetBandSlot, NetFace, NetLayout, NetSlot, cell_color, net_layout,
};
use crate::views::case_drill::{CaseDrillSession, case_site_line};
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;
use bevy::state::state_scoped::DespawnOnExit;

const NET_REGION_WIDTH_PCT: f32 = 0.45;
const NET_REGION_HEIGHT_PCT: f32 = 0.72;
const NET_CELL_GAP_RATIO: f32 = 0.12;
const NET_QUAD_MIN: f32 = 8.0;
const NET_QUAD_MAX: f32 = 56.0;

#[derive(Component)]
pub struct CaseDrillRoot;

#[derive(Component)]
pub struct ShowPanel;

#[derive(Component)]
pub struct GradePanel;

#[derive(Component, Default)]
pub struct CaseDrillLine;

pub fn layout_cells(layout: &NetLayout) -> (usize, usize) {
    let cols = layout
        .bands
        .iter()
        .map(|band| {
            band.iter()
                .map(|slot| match slot {
                    NetBandSlot::Face(face) => face.width,
                    NetBandSlot::Spacer(width) => *width,
                })
                .sum::<usize>()
        })
        .max()
        .unwrap_or(0);
    let rows = layout
        .bands
        .iter()
        .map(|band| {
            band.iter()
                .map(|slot| match slot {
                    NetBandSlot::Face(face) => face.rows.len(),
                    NetBandSlot::Spacer(_) => 0,
                })
                .max()
                .unwrap_or(0)
        })
        .sum();
    (cols, rows)
}

pub fn quad_for(cols: usize, rows: usize, region_w: f32, region_h: f32) -> f32 {
    let span = |cells: usize| cells.max(1) as f32 * (1.0 + NET_CELL_GAP_RATIO) - NET_CELL_GAP_RATIO;
    (region_w / span(cols))
        .min(region_h / span(rows))
        .floor()
        .clamp(NET_QUAD_MIN, NET_QUAD_MAX)
}

pub fn spawn_ui(mut commands: Commands, fonts: Option<Res<FontBook>>) {
    let Some(fonts) = fonts.as_deref() else {
        return;
    };
    let content = crate::present::spawn_view_frame(
        &mut commands,
        fonts,
        AppState::CaseDrill,
        "case drill",
        "space reveal, 1..4 grade, esc menu",
    );
    commands.entity(content).insert(Node {
        width: Val::Percent(100.0),
        flex_grow: 1.0,
        flex_direction: FlexDirection::Column,
        row_gap: theme::rem(theme::SPACING_REM),
        ..default()
    });
    commands.entity(content).with_children(|parent| {
        parent.spawn((
            CaseDrillRoot,
            Node {
                flex_grow: 1.0,
                flex_direction: FlexDirection::Column,
                justify_content: JustifyContent::Center,
                row_gap: theme::rem(theme::SPACING_REM),
                ..default()
            },
        ));
    });
}

pub fn enter_show(
    mut messages: ResMut<Messages<KeyboardInput>>,
    mut commands: Commands,
    session: Res<CaseDrillSession>,
    config: Res<AppConfig>,
    fonts: Option<Res<FontBook>>,
    roots: Query<Entity, With<CaseDrillRoot>>,
) {
    messages.clear();
    let (Some(fonts), Some(root)) = (fonts.as_deref(), roots.iter().next()) else {
        return;
    };
    commands.entity(root).with_children(|parent| {
        parent
            .spawn((
                DespawnOnExit(CaseDrillPhase::Show),
                ShowPanel,
                Node {
                    flex_direction: FlexDirection::Column,
                    row_gap: theme::rem(theme::SPACING_REM),
                    align_items: AlignItems::Center,
                    ..default()
                },
            ))
            .with_children(|panel| spawn_case_lines(panel, fonts, &session, &config, false));
    });
}

pub fn enter_grade(
    mut messages: ResMut<Messages<KeyboardInput>>,
    mut commands: Commands,
    session: Res<CaseDrillSession>,
    config: Res<AppConfig>,
    fonts: Option<Res<FontBook>>,
    roots: Query<Entity, With<CaseDrillRoot>>,
) {
    messages.clear();
    let (Some(fonts), Some(root)) = (fonts.as_deref(), roots.iter().next()) else {
        return;
    };
    commands.entity(root).with_children(|parent| {
        parent
            .spawn((
                DespawnOnExit(CaseDrillPhase::Grade),
                GradePanel,
                Node {
                    flex_direction: FlexDirection::Column,
                    row_gap: theme::rem(theme::SPACING_REM),
                    align_items: AlignItems::Center,
                    ..default()
                },
            ))
            .with_children(|panel| spawn_case_lines(panel, fonts, &session, &config, true));
    });
}

fn spawn_case_lines(
    panel: &mut ChildSpawnerCommands,
    fonts: &FontBook,
    session: &CaseDrillSession,
    config: &AppConfig,
    revealed: bool,
) {
    let Some(case) = session.current.as_ref() else {
        crate::present::spawn_line(
            panel,
            fonts,
            "no case",
            theme::FONT_TITLE,
            theme::text_muted(),
            CaseDrillLine,
        );
        return;
    };
    crate::present::spawn_line(
        panel,
        fonts,
        &case.case.case_key,
        theme::FONT_TITLE,
        theme::text_primary(),
        CaseDrillLine,
    );
    crate::present::spawn_line(
        panel,
        fonts,
        &case_site_line(case),
        theme::FONT_SMALL,
        theme::text_muted(),
        CaseDrillLine,
    );
    crate::present::spawn_line(
        panel,
        fonts,
        &format!("case {} of {}", session.done + 1, session.total),
        theme::FONT_SMALL,
        theme::text_muted(),
        CaseDrillLine,
    );
    if revealed {
        crate::present::spawn_line(
            panel,
            fonts,
            &case.alg,
            theme::FONT_ALG,
            theme::teal(),
            CaseDrillLine,
        );
        crate::present::spawn_line(
            panel,
            fonts,
            "1 again 2 hard 3 good 4 easy",
            theme::FONT_SMALL,
            theme::text_muted(),
            CaseDrillLine,
        );
    } else {
        crate::present::spawn_line(
            panel,
            fonts,
            "space to reveal",
            theme::FONT_SMALL,
            theme::text_muted(),
            CaseDrillLine,
        );
    }
    if let Some(reason) = &session.save_error {
        crate::present::spawn_line(
            panel,
            fonts,
            reason,
            theme::FONT_SMALL,
            theme::danger(),
            CaseDrillLine,
        );
    }
    if let Some(reason) = &session.grid_error {
        crate::present::spawn_line(
            panel,
            fonts,
            reason,
            theme::FONT_SMALL,
            theme::danger(),
            CaseDrillLine,
        );
    } else if let Some(grid) = &session.grid {
        let layout = net_layout(grid);
        let (cols, rows) = layout_cells(&layout);
        let quad = quad_for(
            cols,
            rows,
            config.window.width as f32 * NET_REGION_WIDTH_PCT,
            config.window.height as f32 * NET_REGION_HEIGHT_PCT,
        );
        spawn_net(panel, &layout, quad);
    }
}

fn face_plate_width(cells: usize, quad: f32, gap: f32) -> f32 {
    cells as f32 * quad + (cells.saturating_sub(1)) as f32 * gap
}

pub fn spawn_net(parent: &mut ChildSpawnerCommands, layout: &NetLayout, quad: f32) {
    let gap = (quad * NET_CELL_GAP_RATIO).max(1.5);
    parent
        .spawn(Node {
            flex_direction: FlexDirection::Column,
            row_gap: Val::Px(gap),
            align_items: AlignItems::Center,
            ..default()
        })
        .with_children(|bands| {
            for band in &layout.bands {
                bands
                    .spawn(Node {
                        flex_direction: FlexDirection::Row,
                        column_gap: Val::Px(gap),
                        ..default()
                    })
                    .with_children(|row| {
                        for slot in band {
                            match slot {
                                NetBandSlot::Face(face) => {
                                    spawn_face_plate(row, face, quad, gap);
                                }
                                NetBandSlot::Spacer(width) => {
                                    row.spawn(Node {
                                        width: Val::Px(face_plate_width(*width, quad, gap)),
                                        ..default()
                                    });
                                }
                            }
                        }
                    });
            }
        });
}

fn spawn_face_plate(parent: &mut ChildSpawnerCommands, face: &NetFace, quad: f32, gap: f32) {
    let stroke = Val::Px(crate::net::cell_stroke(quad));
    parent
        .spawn((
            Node {
                flex_direction: FlexDirection::Column,
                row_gap: Val::Px(gap),
                border: UiRect::all(stroke),
                padding: UiRect::all(stroke),
                ..default()
            },
            BorderColor::all(theme::text_primary()),
        ))
        .with_children(|plate| {
            for row in &face.rows {
                plate
                    .spawn(Node {
                        flex_direction: FlexDirection::Row,
                        column_gap: Val::Px(gap),
                        ..default()
                    })
                    .with_children(|row_parent| {
                        for slot in row {
                            let mut cell = row_parent.spawn(Node {
                                width: Val::Px(quad),
                                height: Val::Px(quad),
                                border: UiRect::all(Val::Px(crate::net::cell_stroke(quad))),
                                border_radius: BorderRadius::all(Val::Px(quad * 0.15)),
                                ..default()
                            });
                            if let NetSlot::Cell(cell_data) = slot {
                                cell.insert((
                                    BackgroundColor(cell_color(cell_data)),
                                    BorderColor::all(crate::net::ink_for(cell_color(cell_data))),
                                ));
                            }
                        }
                    });
            }
        });
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::views::case_drill::case_facelets;

    #[test]
    fn layout_cells_counts_columns_and_rows_of_any_net() {
        let nxn = case_facelets("3x3", None, "R").expect("grid");
        let layout = net_layout(&nxn);
        assert_eq!(layout_cells(&layout), (12, 9));
        let skewb = case_facelets("skewb", None, "R").expect("grid");
        assert_eq!(layout_cells(&net_layout(&skewb)), (12, 9));
        let mega = case_facelets("megaminx", None, "R").expect("grid");
        assert_eq!(layout_cells(&net_layout(&mega)), (12, 15));
        let sq1 = case_facelets("sq1", None, "(1,0)/").expect("grid");
        assert_eq!(layout_cells(&net_layout(&sq1)), (12, 2));
    }

    #[test]
    fn quad_for_fits_the_region_and_clamps() {
        assert!((quad_for(12, 9, 576.0, 576.0) - 43.0).abs() < 1.0);
        assert_eq!(quad_for(12, 9, 48.0, 48.0), NET_QUAD_MIN);
        assert_eq!(quad_for(2, 2, 4000.0, 4000.0), NET_QUAD_MAX);
        assert!(quad_for(12, 9, 576.0, 576.0) <= 576.0 / 12.0);
    }

    #[test]
    fn spawn_net_builds_one_bordered_cell_per_layout_cell() {
        let grid = case_facelets("3x3", None, "R").expect("grid");
        let layout = net_layout(&grid);
        let mut world = World::new();
        {
            let mut commands = world.commands();
            let holder = commands.spawn(()).id();
            commands.entity(holder).with_children(|parent| {
                spawn_net(parent, &layout, 24.0);
            });
        }
        world.flush();
        let mut nodes = world.query::<&Node>();
        let cells = nodes
            .iter(&world)
            .filter(|node| node.width == Val::Px(24.0) && node.height == Val::Px(24.0))
            .count();
        assert_eq!(cells, 54, "a 3x3 net spawns all 54 stickers");
        let bands = layout.bands.len();
        let depth = nodes.iter(&world).count();
        assert_eq!(
            depth,
            54 + 18 + 6 + bands + 2 + 1,
            "cells, face rows, plates, bands, spacers, and the root"
        );
    }
}
