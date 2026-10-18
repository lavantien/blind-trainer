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

pub fn ink_for(color: Color) -> Color {
    let linear = color.to_linear();
    let luminance = 0.2126 * linear.red + 0.7152 * linear.green + 0.0722 * linear.blue;
    if luminance >= 0.5 {
        Color::srgb(0.08, 0.08, 0.08)
    } else {
        Color::srgb(0.96, 0.96, 0.96)
    }
}

pub fn letter_color(face: usize) -> Color {
    ink_for(theme::face_color(face))
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

pub fn cell_stroke(quad: f32) -> f32 {
    (quad * 0.06).clamp(1.5, 4.0)
}

pub fn face_stroke(quad: f32) -> f32 {
    (quad * 0.08).clamp(2.0, 5.0)
}

impl NetGeometry {
    pub fn for_window(width: f32, height: f32) -> NetGeometry {
        NetGeometry::for_region(width, height)
    }

    pub fn for_region(width: f32, height: f32) -> NetGeometry {
        let quad = (width / 14.7).min(height / 11.0).floor().clamp(16.0, 96.0);
        NetGeometry {
            quad,
            gap: (quad * 0.12).max(2.0),
        }
    }

    pub fn net_width(&self) -> f32 {
        face_plate(*self) * 4.0 + self.gap * 3.0
    }

    pub fn net_height(&self) -> f32 {
        face_plate(*self) * 3.0 + self.gap * 2.0
    }
}

#[derive(Component)]
pub struct NetCell {
    pub grid: usize,
}

#[derive(Component)]
pub struct NetFace {
    pub face: usize,
}

#[derive(Component, Default)]
pub struct NetPulse(pub f32);

#[derive(Component)]
pub struct NetView;

const NET_BANDS: [&[Option<usize>]; 3] = [
    &[None, Some(0), None, None],
    &[Some(1), Some(2), Some(3), Some(4)],
    &[None, Some(5), None, None],
];

pub fn face_plate(geo: NetGeometry) -> f32 {
    geo.quad * 3.0 + geo.gap * 2.0 + face_stroke(geo.quad) * 4.0
}

pub fn spawn_net(
    parent: &mut ChildSpawnerCommands,
    geo: NetGeometry,
    font: Handle<Font>,
) -> Entity {
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
                        for &slot in band {
                            match slot {
                                Some(face) => spawn_face(row_parent, face, geo, &font),
                                None => {
                                    row_parent.spawn(Node {
                                        width: Val::Px(face_plate(geo)),
                                        ..default()
                                    });
                                }
                            }
                        }
                    });
            }
        })
        .id()
}

fn spawn_face(
    parent: &mut ChildSpawnerCommands,
    face: usize,
    geo: NetGeometry,
    font: &Handle<Font>,
) {
    let stroke = Val::Px(face_stroke(geo.quad));
    parent
        .spawn((
            NetFace { face },
            Node {
                flex_direction: FlexDirection::Column,
                row_gap: Val::Px(geo.gap),
                border: UiRect::all(stroke),
                padding: UiRect::all(stroke),
                ..default()
            },
            BorderColor::all(theme::text_primary()),
        ))
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
                            let mut cell = row_parent.spawn((
                                NetCell { grid },
                                NetPulse(0.0),
                                Node {
                                    width: Val::Px(geo.quad),
                                    height: Val::Px(geo.quad),
                                    border: UiRect::all(Val::Px(cell_stroke(geo.quad))),
                                    border_radius: BorderRadius::all(Val::Px(geo.quad * 0.15)),
                                    ..default()
                                },
                                BackgroundColor(theme::face_color(face)),
                                BorderColor::all(ink_for(theme::face_color(face))),
                            ));
                            if grid % 9 == 4 {
                                cell.with_children(|cell_parent| {
                                    cell_parent.spawn((
                                        Node {
                                            width: Val::Percent(100.0),
                                            height: Val::Percent(100.0),
                                            align_items: AlignItems::Center,
                                            justify_content: JustifyContent::Center,
                                            ..default()
                                        },
                                        Text::new(theme::face_name(face)),
                                        TextFont {
                                            font: FontSource::Handle(font.clone()),
                                            font_size: FontSize::Px(geo.quad * 0.45),
                                            ..default()
                                        },
                                        TextColor(letter_color(face)),
                                    ));
                                });
                            }
                        }
                    });
            }
        });
}

#[derive(Resource)]
pub struct PulseFrozen;

pub const SMOKE_PULSE_FACTOR: f32 = 1.0;

pub fn update_net_colors(
    net: Res<NetState>,
    time: Res<Time<Virtual>>,
    frozen: Option<Res<PulseFrozen>>,
    mut cells: Query<(
        &NetCell,
        &mut BackgroundColor,
        &mut BorderColor,
        &mut NetPulse,
    )>,
) {
    let dt = time.delta_secs();
    let wave = 0.6 + 0.4 * (time.elapsed_secs() * std::f32::consts::TAU * theme::PULSE_HZ).sin();
    for (cell, mut background, mut border, mut pulse) in cells.iter_mut() {
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
        let factor = if frozen.is_some() {
            SMOKE_PULSE_FACTOR
        } else {
            pulse.0 * wave
        };
        background.0 = match highlight {
            Some(color) => base.mix(&color, factor),
            None => base,
        };
        *border = BorderColor::all(ink_for(background.0));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn axis_unit(byte: u8) -> (i8, i8, i8) {
        match byte {
            b'U' => (0, 1, 0),
            b'D' => (0, -1, 0),
            b'R' => (1, 0, 0),
            b'L' => (-1, 0, 0),
            b'F' => (0, 0, -1),
            b'B' => (0, 0, 1),
            other => panic!("axis byte unexpected: {other}"),
        }
    }

    fn sticker_placement(sticker: u8) -> ((i8, i8, i8), (i8, i8, i8)) {
        let bytes = name_of(sticker).as_bytes();
        let mut pos = (0, 0, 0);
        for &byte in bytes {
            let unit = axis_unit(byte);
            pos = (pos.0 + unit.0, pos.1 + unit.1, pos.2 + unit.2);
        }
        (pos, axis_unit(bytes[0]))
    }

    fn cell_placement(face: usize, row: usize, col: usize) -> ((i8, i8, i8), (i8, i8, i8)) {
        let (r, c) = (row as i8, col as i8);
        match face {
            0 => ((c - 1, 1, 1 - r), (0, 1, 0)),
            1 => ((-1, 1 - r, 1 - c), (-1, 0, 0)),
            2 => ((c - 1, 1 - r, -1), (0, 0, -1)),
            3 => ((1, 1 - r, c - 1), (1, 0, 0)),
            4 => ((1 - c, 1 - r, 1), (0, 0, 1)),
            5 => ((c - 1, -1, r - 1), (0, -1, 0)),
            other => panic!("face index unexpected: {other}"),
        }
    }

    #[test]
    fn grid_matches_physical_unfold() {
        for sticker in 0u8..48 {
            let placement = sticker_placement(sticker);
            let mut hits = Vec::new();
            for face in 0..6usize {
                for row in 0..3usize {
                    for col in 0..3usize {
                        if cell_placement(face, row, col) == placement {
                            hits.push((face, row, col));
                        }
                    }
                }
            }
            assert_eq!(hits.len(), 1, "sticker {sticker} at {placement:?}");
            assert_eq!(grid_of(sticker), hits[0], "sticker {sticker}");
            assert_eq!(
                grid_index(sticker),
                hits[0].0 * 9 + hits[0].1 * 3 + hits[0].2
            );
        }
    }

    #[test]
    fn letter_ink_contrasts_face_luminance() {
        let dark = Color::srgb(0.08, 0.08, 0.08);
        let light = Color::srgb(0.96, 0.96, 0.96);
        assert_eq!(letter_color(0), dark);
        assert_eq!(letter_color(5), dark);
        assert_eq!(letter_color(3), light);
        assert_eq!(letter_color(4), light);
    }

    fn luminance(color: Color) -> f32 {
        let linear = color.to_linear();
        0.2126 * linear.red + 0.7152 * linear.green + 0.0722 * linear.blue
    }

    #[test]
    fn ink_for_contrasts_faces_highlights_and_mixes() {
        let mut colors = Vec::new();
        for face in 0..theme::FACE_COUNT {
            colors.push(theme::face_color(face));
            colors.push(theme::face_color(face).mix(&theme::teal(), 0.5));
            colors.push(theme::face_color(face).mix(&theme::interchange(), 0.5));
        }
        for color in colors {
            let ink = ink_for(color);
            assert!(
                (luminance(ink) - luminance(color)).abs() >= 0.35,
                "ink {ink:?} too close to {color:?}"
            );
        }
    }

    #[test]
    fn stroke_widths_track_quad_within_clamps() {
        assert_eq!(cell_stroke(16.0), 1.5);
        assert!((cell_stroke(41.0) - 2.46).abs() < 1e-5);
        assert_eq!(cell_stroke(96.0), 4.0);
        assert_eq!(face_stroke(16.0), 2.0);
        assert!((face_stroke(41.0) - 3.28).abs() < 1e-5);
        assert_eq!(face_stroke(96.0), 5.0);
    }

    #[test]
    fn net_footprint_sums_cells_gaps_and_face_strokes() {
        let geo = NetGeometry::for_region(563.2, 690.0);
        assert_eq!(geo.quad, 38.0);
        assert!((geo.net_width() - 554.8).abs() < 1e-4);
        assert!((geo.net_height() - 414.96).abs() < 1e-4);
    }

    #[test]
    fn u_and_d_bands_seat_faces_over_the_f_slot() {
        let mut world = World::new();
        let geo = NetGeometry::for_region(563.2, 690.0);
        {
            let mut commands = world.commands();
            let holder = commands.spawn(()).id();
            commands.entity(holder).with_children(|parent| {
                spawn_net(parent, geo, Handle::default());
            });
        }
        world.flush();
        let view = world
            .query_filtered::<Entity, With<NetView>>()
            .single(&world)
            .expect("one net view");
        let bands = world.get::<Children>(view).expect("three bands");
        assert_eq!(bands.len(), 3);
        let expected = [
            vec![None, Some(0usize), None, None],
            vec![Some(1usize), Some(2), Some(3), Some(4)],
            vec![None, Some(5usize), None, None],
        ];
        for (band, wanted) in bands.iter().zip(expected) {
            let children = world.get::<Children>(band).expect("band children");
            assert_eq!(children.len(), wanted.len());
            for (slot, face) in children.iter().zip(wanted) {
                match face {
                    Some(index) => {
                        assert_eq!(world.get::<NetFace>(slot).expect("face marker").face, index)
                    }
                    None => {
                        assert!(world.get::<NetFace>(slot).is_none());
                        let node = world.get::<Node>(slot).expect("spacer node");
                        assert_eq!(node.width, Val::Px(face_plate(geo)));
                    }
                }
            }
        }
    }

    #[test]
    fn spawn_net_borders_every_cell_and_face() {
        let mut world = World::new();
        let geo = NetGeometry::for_region(563.2, 690.0);
        {
            let mut commands = world.commands();
            let holder = commands.spawn(()).id();
            commands.entity(holder).with_children(|parent| {
                spawn_net(parent, geo, Handle::default());
            });
        }
        world.flush();
        let cell_stroke = Val::Px(cell_stroke(geo.quad));
        let mut cells = world.query::<(&NetCell, &Node, &BorderColor)>();
        assert_eq!(cells.iter(&world).count(), 54);
        for (cell, node, border) in cells.iter(&world) {
            assert_eq!(node.border, UiRect::all(cell_stroke));
            assert_eq!(
                *border,
                BorderColor::all(ink_for(theme::face_color(cell.grid / 9)))
            );
        }
        let face_stroke = Val::Px(face_stroke(geo.quad));
        let mut faces = world.query::<(&NetFace, &Node, &BorderColor)>();
        let mut seen = Vec::new();
        for (marker, node, border) in faces.iter(&world) {
            seen.push(marker.face);
            assert_eq!(node.border, UiRect::all(face_stroke));
            assert_eq!(node.padding, UiRect::all(face_stroke));
            assert_eq!(*border, BorderColor::all(theme::text_primary()));
        }
        seen.sort_unstable();
        assert_eq!(seen, vec![0, 1, 2, 3, 4, 5]);
    }

    fn highlight_colors(net: &NetState, elapsed: f32, frozen: bool) -> Vec<(usize, Color)> {
        let mut world = World::new();
        world.insert_resource(net.clone());
        if frozen {
            world.insert_resource(PulseFrozen);
        }
        let mut time = Time::<Virtual>::default();
        time.advance_to(std::time::Duration::from_secs_f32(elapsed));
        world.insert_resource(time);
        let geo = NetGeometry::for_region(563.2, 690.0);
        {
            let mut commands = world.commands();
            let holder = commands.spawn(()).id();
            commands.entity(holder).with_children(|parent| {
                spawn_net(parent, geo, Handle::default());
            });
        }
        world.flush();
        let mut schedule = Schedule::default();
        schedule.add_systems(update_net_colors);
        schedule.run(&mut world);
        let mut cells = world.query::<(&NetCell, &BackgroundColor)>();
        let mut out: Vec<(usize, Color)> = cells
            .iter(&world)
            .map(|(cell, background)| (cell.grid, background.0))
            .collect();
        out.sort_by_key(|(grid, _)| *grid);
        out
    }

    fn targeted_net() -> NetState {
        NetState {
            targets: [
                Some(sticker_at_grid(1).expect("grid 1 holds a sticker")),
                Some(sticker_at_grid(3).expect("grid 3 holds a sticker")),
            ],
            ..NetState::default()
        }
    }

    #[test]
    fn frozen_pulse_renders_the_same_highlights_whatever_the_elapsed_time() {
        let net = targeted_net();
        let early = highlight_colors(&net, 0.3, true);
        let late = highlight_colors(&net, 3.7, true);
        assert_eq!(early, late);
    }

    #[test]
    fn live_pulse_keeps_varying_with_the_elapsed_time() {
        let net = targeted_net();
        let early = highlight_colors(&net, 0.3, false);
        let late = highlight_colors(&net, 3.7, false);
        assert_ne!(early, late);
    }

    #[test]
    fn update_net_colors_keeps_border_ink_matched() {
        let mut world = World::new();
        world.insert_resource(NetState::default());
        world.insert_resource(Time::<Virtual>::default());
        let geo = NetGeometry::for_region(563.2, 690.0);
        {
            let mut commands = world.commands();
            let holder = commands.spawn(()).id();
            commands.entity(holder).with_children(|parent| {
                spawn_net(parent, geo, Handle::default());
            });
        }
        world.flush();
        let mut schedule = Schedule::default();
        schedule.add_systems(update_net_colors);
        schedule.run(&mut world);
        let mut cells = world.query_filtered::<(&BackgroundColor, &BorderColor), With<NetCell>>();
        assert_eq!(cells.iter(&world).count(), 54);
        for (background, border) in cells.iter(&world) {
            assert_eq!(*border, BorderColor::all(ink_for(background.0)));
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
        assert!(geo.net_width() <= 1280.0);
        assert!(geo.net_height() <= 800.0);
    }

    #[test]
    fn geometry_fits_window_with_view_reserve() {
        let reserve = crate::theme::ROOT_REM * crate::theme::NET_VIEW_RESERVE_REM;
        let geo = NetGeometry::for_window(1280.0, 800.0 - reserve);
        assert!(geo.net_height() + reserve <= 800.0);
        assert!(geo.net_width() <= 1280.0);
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
        assert_eq!(geo.quad, 38.0);
        assert!(geo.net_width() <= width);
        assert!(geo.net_height() <= height);
    }
}
