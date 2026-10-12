use crate::logic::{HighlightKind, NetState};
use crate::theme;
use bevy::prelude::*;
use bt_scheme::name_of;
use std::sync::LazyLock;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
struct SideRule {
    letter: u8,
    is_row: bool,
    pos: u8,
}

const SIDE_RULES: [[SideRule; 4]; theme::FACE_COUNT] = [
    [
        SideRule {
            letter: b'B',
            is_row: true,
            pos: 0,
        },
        SideRule {
            letter: b'F',
            is_row: true,
            pos: 2,
        },
        SideRule {
            letter: b'L',
            is_row: false,
            pos: 0,
        },
        SideRule {
            letter: b'R',
            is_row: false,
            pos: 2,
        },
    ],
    [
        SideRule {
            letter: b'U',
            is_row: true,
            pos: 0,
        },
        SideRule {
            letter: b'D',
            is_row: true,
            pos: 2,
        },
        SideRule {
            letter: b'B',
            is_row: false,
            pos: 0,
        },
        SideRule {
            letter: b'F',
            is_row: false,
            pos: 2,
        },
    ],
    [
        SideRule {
            letter: b'U',
            is_row: true,
            pos: 0,
        },
        SideRule {
            letter: b'D',
            is_row: true,
            pos: 2,
        },
        SideRule {
            letter: b'L',
            is_row: false,
            pos: 0,
        },
        SideRule {
            letter: b'R',
            is_row: false,
            pos: 2,
        },
    ],
    [
        SideRule {
            letter: b'U',
            is_row: true,
            pos: 0,
        },
        SideRule {
            letter: b'D',
            is_row: true,
            pos: 2,
        },
        SideRule {
            letter: b'F',
            is_row: false,
            pos: 0,
        },
        SideRule {
            letter: b'B',
            is_row: false,
            pos: 2,
        },
    ],
    [
        SideRule {
            letter: b'U',
            is_row: true,
            pos: 0,
        },
        SideRule {
            letter: b'D',
            is_row: true,
            pos: 2,
        },
        SideRule {
            letter: b'R',
            is_row: false,
            pos: 0,
        },
        SideRule {
            letter: b'L',
            is_row: false,
            pos: 2,
        },
    ],
    [
        SideRule {
            letter: b'F',
            is_row: true,
            pos: 0,
        },
        SideRule {
            letter: b'B',
            is_row: true,
            pos: 2,
        },
        SideRule {
            letter: b'L',
            is_row: false,
            pos: 0,
        },
        SideRule {
            letter: b'R',
            is_row: false,
            pos: 2,
        },
    ],
];

pub fn face_of(sticker: u8) -> usize {
    match name_of(sticker).as_bytes()[0] {
        b'U' => 0,
        b'L' => 1,
        b'F' => 2,
        b'R' => 3,
        b'B' => 4,
        b'D' => 5,
        other => panic!("sticker face byte unexpected: {other}"),
    }
}

pub fn grid_of(sticker: u8) -> (usize, usize, usize) {
    let face = face_of(sticker);
    let bytes = name_of(sticker).as_bytes();
    let mut row = 1usize;
    let mut col = 1usize;
    for &byte in &bytes[1..] {
        for rule in &SIDE_RULES[face] {
            if rule.letter == byte {
                if rule.is_row {
                    row = rule.pos as usize;
                } else {
                    col = rule.pos as usize;
                }
            }
        }
    }
    (face, row, col)
}

pub fn grid_index(sticker: u8) -> usize {
    let (face, row, col) = grid_of(sticker);
    face * 9 + row * 3 + col
}

static GRID_TO_STICKER: LazyLock<[Option<u8>; 54]> = LazyLock::new(|| {
    let mut map = [None; 54];
    for sticker in 0u8..48 {
        map[grid_index(sticker)] = Some(sticker);
    }
    map
});

pub fn sticker_at_grid(grid: usize) -> Option<u8> {
    GRID_TO_STICKER[grid]
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub struct NetGeometry {
    pub quad: f32,
    pub gap: f32,
}

impl NetGeometry {
    pub fn for_window(width: f32, height: f32) -> NetGeometry {
        NetGeometry::for_region(width, height)
    }

    pub fn for_region(width: f32, height: f32) -> NetGeometry {
        let quad = (width / 13.5).min(height / 10.2).floor().clamp(16.0, 96.0);
        NetGeometry {
            quad,
            gap: (quad * 0.12).max(2.0),
        }
    }
}

#[derive(Component)]
pub struct NetCell {
    pub grid: usize,
}

#[derive(Component, Default)]
pub struct NetPulse(pub f32);

#[derive(Component)]
pub struct NetView;

const NET_BANDS: [&[usize]; 3] = [&[0], &[1, 2, 3, 4], &[5]];

pub fn spawn_net(parent: &mut ChildSpawnerCommands, geo: NetGeometry) -> Entity {
    parent
        .spawn((
            NetView,
            Node {
                flex_direction: FlexDirection::Column,
                row_gap: Val::Px(geo.gap),
                align_items: AlignItems::Center,
                ..default()
            },
        ))
        .with_children(|parent| {
            for band in NET_BANDS {
                parent
                    .spawn(Node {
                        flex_direction: FlexDirection::Row,
                        column_gap: Val::Px(geo.gap),
                        ..default()
                    })
                    .with_children(|row_parent| {
                        for &face in band {
                            spawn_face(row_parent, face, geo);
                        }
                    });
            }
        })
        .id()
}

fn spawn_face(parent: &mut ChildSpawnerCommands, face: usize, geo: NetGeometry) {
    parent
        .spawn(Node {
            flex_direction: FlexDirection::Column,
            row_gap: Val::Px(geo.gap),
            ..default()
        })
        .with_children(|face_parent| {
            for row in 0..3usize {
                face_parent
                    .spawn(Node {
                        flex_direction: FlexDirection::Row,
                        column_gap: Val::Px(geo.gap),
                        ..default()
                    })
                    .with_children(|row_parent| {
                        for col in 0..3usize {
                            let grid = face * 9 + row * 3 + col;
                            row_parent.spawn((
                                NetCell { grid },
                                NetPulse(0.0),
                                Node {
                                    width: Val::Px(geo.quad),
                                    height: Val::Px(geo.quad),
                                    border_radius: BorderRadius::all(Val::Px(geo.quad * 0.15)),
                                    ..default()
                                },
                                BackgroundColor(theme::face_color(face)),
                            ));
                        }
                    });
            }
        });
}

pub fn update_net_colors(
    net: Res<NetState>,
    time: Res<Time<Virtual>>,
    mut cells: Query<(&NetCell, &mut BackgroundColor, &mut NetPulse)>,
) {
    let dt = time.delta_secs();
    let wave = 0.6 + 0.4 * (time.elapsed_secs() * std::f32::consts::TAU * theme::PULSE_HZ).sin();
    for (cell, mut background, mut pulse) in cells.iter_mut() {
        let face = cell.grid / 9;
        let mut base = theme::face_color(face);
        let mut want_pulse = 0.0f32;
        let mut highlight = None;
        if let Some(sticker) = sticker_at_grid(cell.grid) {
            let occupant = net.cube.sticker_at(sticker);
            base = theme::face_color(face_of(occupant));
            match net.highlight_kind(occupant) {
                Some(HighlightKind::Buffer) => {
                    want_pulse = 1.0;
                    highlight = Some(theme::teal());
                }
                Some(HighlightKind::Target) => {
                    want_pulse = 1.0;
                    highlight = Some(theme::interchange());
                }
                None => {}
            }
        }
        let delta = (want_pulse - pulse.0)
            .clamp(-dt * theme::PULSE_LERP_PER_S, dt * theme::PULSE_LERP_PER_S);
        pulse.0 += delta;
        background.0 = match highlight {
            Some(color) => base.mix(&color, pulse.0 * wave),
            None => base,
        };
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const EXPECTED: [(u8, usize, usize, usize); 48] = [
        (0, 0, 0, 0),
        (1, 0, 0, 2),
        (2, 0, 2, 2),
        (3, 0, 2, 0),
        (4, 1, 0, 0),
        (5, 1, 0, 2),
        (6, 1, 2, 2),
        (7, 1, 2, 0),
        (8, 2, 0, 0),
        (9, 2, 0, 2),
        (10, 2, 2, 2),
        (11, 2, 2, 0),
        (12, 3, 0, 0),
        (13, 3, 0, 2),
        (14, 3, 2, 2),
        (15, 3, 2, 0),
        (16, 4, 0, 0),
        (17, 4, 0, 2),
        (18, 4, 2, 2),
        (19, 4, 2, 0),
        (20, 5, 0, 0),
        (21, 5, 0, 2),
        (22, 5, 2, 2),
        (23, 5, 2, 0),
        (24, 0, 0, 1),
        (25, 0, 1, 2),
        (26, 0, 2, 1),
        (27, 0, 1, 0),
        (28, 1, 0, 1),
        (29, 1, 1, 2),
        (30, 1, 2, 1),
        (31, 1, 1, 0),
        (32, 2, 0, 1),
        (33, 2, 1, 2),
        (34, 2, 2, 1),
        (35, 2, 1, 0),
        (36, 3, 0, 1),
        (37, 3, 1, 2),
        (38, 3, 2, 1),
        (39, 3, 1, 0),
        (40, 4, 0, 1),
        (41, 4, 1, 2),
        (42, 4, 2, 1),
        (43, 4, 1, 0),
        (44, 5, 0, 1),
        (45, 5, 1, 2),
        (46, 5, 2, 1),
        (47, 5, 1, 0),
    ];

    #[test]
    fn grid_matches_position_map() {
        for (sticker, face, row, col) in EXPECTED {
            assert_eq!(grid_of(sticker), (face, row, col), "sticker {sticker}");
            assert_eq!(grid_index(sticker), face * 9 + row * 3 + col);
        }
    }

    #[test]
    fn grid_covers_all_cells_once() {
        let mut seen = [false; 54];
        for sticker in 0u8..48 {
            let grid = grid_index(sticker);
            assert!(!seen[grid], "grid cell {grid} claimed twice");
            seen[grid] = true;
            assert_eq!(sticker_at_grid(grid), Some(sticker));
        }
        for face in 0..6usize {
            assert_eq!(sticker_at_grid(face * 9 + 4), None, "center must be empty");
        }
    }

    #[test]
    fn face_of_follows_sticker_name() {
        assert_eq!(face_of(0), 0);
        assert_eq!(face_of(26), 0);
        assert_eq!(face_of(29), 1);
        assert_eq!(face_of(33), 2);
        assert_eq!(face_of(36), 3);
        assert_eq!(face_of(41), 4);
        assert_eq!(face_of(44), 5);
    }

    #[test]
    fn geometry_fits_configured_window() {
        let geo = NetGeometry::for_window(1280.0, 800.0);
        assert!(geo.quad >= 16.0 && geo.quad <= 96.0);
        assert!(geo.quad * 12.0 + geo.gap * 11.0 <= 1280.0);
        assert!(geo.quad * 9.0 + geo.gap * 8.0 <= 800.0);
    }

    #[test]
    fn geometry_fits_window_with_view_reserve() {
        let reserve = crate::theme::ROOT_REM * crate::theme::NET_VIEW_RESERVE_REM;
        let geo = NetGeometry::for_window(1280.0, 800.0 - reserve);
        assert!(geo.quad * 9.0 + geo.gap * 8.0 + reserve <= 800.0);
        assert!(geo.quad * 12.0 + geo.gap * 11.0 <= 1280.0);
    }

    #[test]
    fn geometry_clamps_tiny_windows() {
        let geo = NetGeometry::for_window(100.0, 60.0);
        assert_eq!(geo.quad, 16.0);
    }

    #[test]
    fn for_region_sizes_the_drill_left_region() {
        let width = 1280.0 * crate::theme::DRILL_NET_WIDTH_PCT;
        let chrome = crate::theme::ROOT_REM
            * (crate::theme::HEADER_HEIGHT_REM + 2.0 * crate::theme::PANEL_PADDING_REM);
        let height = 800.0 - chrome;
        let geo = NetGeometry::for_region(width, height);
        assert_eq!(geo.quad, 41.0);
        assert!(geo.quad * 12.0 + geo.gap * 11.0 <= width);
        assert!(geo.quad * 9.0 + geo.gap * 8.0 <= height);
    }
}
