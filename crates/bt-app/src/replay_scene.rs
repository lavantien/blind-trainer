use bt_core::{Axis, Cube, Move, Slice};
use serde::Deserialize;

pub const DEFAULT_SECS_PER_MOVE: f64 = 0.5;
pub const DEFAULT_YAW_DEG: f32 = 45.0;
pub const DEFAULT_PITCH_DEG: f32 = 30.0;
pub const DEFAULT_DISTANCE: f32 = 6.0;
pub const DEFAULT_ZOOM_MIN: f32 = 3.5;
pub const DEFAULT_ZOOM_MAX: f32 = 12.0;
pub const DEFAULT_PITCH_CLAMP_DEG: [f32; 2] = [-80.0, 80.0];
pub const DRAG_DEG_PER_PX: f32 = 0.4;
pub const ZOOM_UNITS_PER_NOTCH: f32 = 0.75;
pub const CUBIE_SPAN: f32 = 0.94;
pub const STICKER_SPAN: f32 = 0.82;
pub const STICKER_LIFT: f32 = 0.475;

pub type I3 = (i8, i8, i8);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Cell {
    pub slot: u8,
    pub face: usize,
    pub row: usize,
    pub col: usize,
    pub pos: I3,
    pub normal: I3,
}

pub const FACE_NORMALS: [I3; 6] = [
    (0, 1, 0),
    (-1, 0, 0),
    (0, 0, 1),
    (1, 0, 0),
    (0, 0, -1),
    (0, -1, 0),
];
const FACE_AXIS: [usize; 6] = [1, 0, 2, 0, 2, 1];
const FACE_SIGN: [i8; 6] = [1, -1, 1, 1, -1, -1];
const ROT_REF_FACE: [usize; 3] = [3, 0, 2];

const fn corner_pos(row: usize, col: usize) -> usize {
    match (row, col) {
        (0, 0) => 0,
        (0, 2) => 1,
        (2, 2) => 2,
        (2, 0) => 3,
        _ => 0,
    }
}

const fn edge_pos(row: usize, col: usize) -> usize {
    match (row, col) {
        (0, 1) => 0,
        (1, 2) => 1,
        (2, 1) => 2,
        (1, 0) => 3,
        _ => 0,
    }
}

const fn slot_of(face: usize, row: usize, col: usize) -> u8 {
    if row == 1 && col == 1 {
        48 + face as u8
    } else if row.is_multiple_of(2) && col.is_multiple_of(2) {
        (face * 4 + corner_pos(row, col)) as u8
    } else {
        (24 + face * 4 + edge_pos(row, col)) as u8
    }
}

const fn pos_of(face: usize, row: usize, col: usize) -> I3 {
    let (r, c) = (row as i8, col as i8);
    match face {
        0 => (c - 1, 1, r - 1),
        1 => (-1, 1 - r, c - 1),
        2 => (c - 1, 1 - r, 1),
        3 => (1, 1 - r, 1 - c),
        4 => (1 - c, 1 - r, -1),
        _ => (c - 1, -1, 1 - r),
    }
}

const fn build_cells() -> [Cell; 54] {
    let mut cells = [Cell {
        slot: 0,
        face: 0,
        row: 0,
        col: 0,
        pos: (0, 0, 0),
        normal: (0, 0, 0),
    }; 54];
    let mut next = 0usize;
    let mut face = 0usize;
    while face < 6 {
        let mut row = 0usize;
        while row < 3 {
            let mut col = 0usize;
            while col < 3 {
                cells[next] = Cell {
                    slot: slot_of(face, row, col),
                    face,
                    row,
                    col,
                    pos: pos_of(face, row, col),
                    normal: FACE_NORMALS[face],
                };
                next += 1;
                col += 1;
            }
            row += 1;
        }
        face += 1;
    }
    cells
}

pub const CELLS: [Cell; 54] = build_cells();

pub fn cell_at(pos: I3, normal: I3) -> Option<&'static Cell> {
    CELLS
        .iter()
        .find(|cell| cell.pos == pos && cell.normal == normal)
}

pub fn home_face(sticker: u8) -> usize {
    if sticker < 24 {
        (sticker / 4) as usize
    } else if sticker < 48 {
        ((sticker - 24) / 4) as usize
    } else {
        (sticker - 48) as usize
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Turn {
    pub axis: Axis,
    pub applied: u8,
    pub mask: [bool; 3],
}

impl Turn {
    pub fn rotates(&self, pos: I3) -> bool {
        self.mask[(coord_along(pos, self.axis) + 1) as usize]
    }

    pub fn angle_deg(&self, progress: f32) -> f32 {
        self.applied as f32 * 90.0 * progress
    }
}

fn coord_along(pos: I3, axis: Axis) -> i8 {
    match axis {
        Axis::X => pos.0,
        Axis::Y => pos.1,
        Axis::Z => pos.2,
    }
}

fn axis_index(axis: Axis) -> usize {
    match axis {
        Axis::X => 0,
        Axis::Y => 1,
        Axis::Z => 2,
    }
}

fn applied_quarters(face: usize, quarter: u8) -> u8 {
    if FACE_SIGN[face] < 0 {
        quarter
    } else {
        4 - quarter
    }
}

fn face_axis(face: usize) -> Axis {
    [Axis::X, Axis::Y, Axis::Z][FACE_AXIS[face]]
}

fn single_layer(face: usize) -> [bool; 3] {
    let normal = FACE_NORMALS[face];
    let axis = FACE_AXIS[face];
    let coord = match axis {
        0 => normal.0,
        1 => normal.1,
        _ => normal.2,
    };
    let mut mask = [false; 3];
    mask[(coord + 1) as usize] = true;
    mask
}

fn slice_face(slice: Slice) -> usize {
    match slice {
        Slice::M => 1,
        Slice::E => 5,
        Slice::S => 2,
    }
}

pub fn turn_of(mv: Move) -> Turn {
    match mv {
        Move::Face { face, quarter } => Turn {
            axis: face_axis(face.index()),
            applied: applied_quarters(face.index(), quarter),
            mask: single_layer(face.index()),
        },
        Move::Inner { face, quarter } => Turn {
            axis: face_axis(face.index()),
            applied: applied_quarters(face.index(), quarter),
            mask: [false, true, false],
        },
        Move::Wide { face, quarter } => {
            let mut mask = single_layer(face.index());
            mask[1] = true;
            Turn {
                axis: face_axis(face.index()),
                applied: applied_quarters(face.index(), quarter),
                mask,
            }
        }
        Move::Slice { slice, quarter } => Turn {
            axis: face_axis(slice_face(slice)),
            applied: applied_quarters(slice_face(slice), quarter),
            mask: [false, true, false],
        },
        Move::Rotation { axis, quarter } => Turn {
            axis,
            applied: applied_quarters(ROT_REF_FACE[axis_index(axis)], quarter),
            mask: [true; 3],
        },
    }
}

fn rot_once(v: I3, axis: usize) -> I3 {
    let (x, y, z) = (v.0, v.1, v.2);
    match axis {
        0 => (x, -z, y),
        1 => (z, y, -x),
        _ => (-y, x, z),
    }
}

pub fn rotate(v: I3, axis: Axis, times: u8) -> I3 {
    let mut out = v;
    let mut i = 0;
    while i < times {
        out = rot_once(out, axis_index(axis));
        i += 1;
    }
    out
}

pub fn turned_cell(cell: &Cell, turn: &Turn) -> &'static Cell {
    if !turn.rotates(cell.pos) {
        cell_at(cell.pos, cell.normal).expect("identity cell lookup")
    } else {
        cell_at(
            rotate(cell.pos, turn.axis, turn.applied),
            rotate(cell.normal, turn.axis, turn.applied),
        )
        .expect("turned cell lands on a face")
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Deserialize)]
#[serde(default)]
pub struct ReplayCfg {
    pub secs_per_move: f64,
    pub yaw_deg: f32,
    pub pitch_deg: f32,
    pub distance: f32,
    pub zoom_min: f32,
    pub zoom_max: f32,
    pub pitch_clamp_deg: [f32; 2],
}

impl Default for ReplayCfg {
    fn default() -> ReplayCfg {
        ReplayCfg {
            secs_per_move: DEFAULT_SECS_PER_MOVE,
            yaw_deg: DEFAULT_YAW_DEG,
            pitch_deg: DEFAULT_PITCH_DEG,
            distance: DEFAULT_DISTANCE,
            zoom_min: DEFAULT_ZOOM_MIN,
            zoom_max: DEFAULT_ZOOM_MAX,
            pitch_clamp_deg: DEFAULT_PITCH_CLAMP_DEG,
        }
    }
}

impl ReplayCfg {
    pub fn sanitized(self) -> ReplayCfg {
        let (mut lo, mut hi) = (self.zoom_min, self.zoom_max);
        if lo > hi {
            std::mem::swap(&mut lo, &mut hi);
        }
        let (mut pitch_lo, mut pitch_hi) = (self.pitch_clamp_deg[0], self.pitch_clamp_deg[1]);
        if pitch_lo > pitch_hi {
            std::mem::swap(&mut pitch_lo, &mut pitch_hi);
        }
        ReplayCfg {
            secs_per_move: self.secs_per_move.max(0.0),
            yaw_deg: wrap_yaw(self.yaw_deg),
            pitch_deg: self.pitch_deg.clamp(pitch_lo, pitch_hi),
            distance: self.distance.clamp(lo, hi),
            zoom_min: lo,
            zoom_max: hi,
            pitch_clamp_deg: [pitch_lo, pitch_hi],
        }
    }
}

pub fn replay_cfg_from(value: &serde_json::Value) -> ReplayCfg {
    value
        .pointer("/app/replay")
        .and_then(|section| serde_json::from_value::<ReplayCfg>(section.clone()).ok())
        .unwrap_or_default()
        .sanitized()
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Orbit {
    pub yaw_deg: f32,
    pub pitch_deg: f32,
    pub distance: f32,
}

pub fn default_orbit(cfg: &ReplayCfg) -> Orbit {
    Orbit {
        yaw_deg: wrap_yaw(cfg.yaw_deg),
        pitch_deg: cfg.pitch_deg,
        distance: cfg.distance,
    }
}

pub fn wrap_yaw(deg: f32) -> f32 {
    let mut out = deg % 360.0;
    if out <= -180.0 {
        out += 360.0;
    }
    if out > 180.0 {
        out -= 360.0;
    }
    out
}

pub fn apply_drag_delta(orbit: Orbit, cfg: &ReplayCfg, dx_px: f32, dy_px: f32) -> Orbit {
    Orbit {
        yaw_deg: wrap_yaw(orbit.yaw_deg + dx_px * DRAG_DEG_PER_PX),
        pitch_deg: (orbit.pitch_deg + dy_px * DRAG_DEG_PER_PX)
            .clamp(cfg.pitch_clamp_deg[0], cfg.pitch_clamp_deg[1]),
        distance: orbit.distance.clamp(cfg.zoom_min, cfg.zoom_max),
    }
}

pub fn apply_zoom(orbit: Orbit, cfg: &ReplayCfg, notches: f32) -> Orbit {
    Orbit {
        yaw_deg: orbit.yaw_deg,
        pitch_deg: orbit.pitch_deg,
        distance: (orbit.distance - notches * ZOOM_UNITS_PER_NOTCH)
            .clamp(cfg.zoom_min, cfg.zoom_max),
    }
}

pub fn camera_offset(orbit: &Orbit) -> (f32, f32, f32) {
    let yaw = orbit.yaw_deg.to_radians();
    let pitch = orbit.pitch_deg.to_radians();
    let (sin_yaw, cos_yaw) = yaw.sin_cos();
    let (sin_pitch, cos_pitch) = pitch.sin_cos();
    (
        orbit.distance * cos_pitch * sin_yaw,
        orbit.distance * sin_pitch,
        orbit.distance * cos_pitch * cos_yaw,
    )
}

pub fn turn_progress(elapsed_secs: f64, secs_per_move: f64) -> f32 {
    if secs_per_move <= 0.0 {
        return 1.0;
    }
    let t = (elapsed_secs / secs_per_move).clamp(0.0, 1.0) as f32;
    t * t * (3.0 - 2.0 * t)
}

#[derive(Clone, Debug, PartialEq)]
pub struct Playback {
    pub cube: Cube,
    pub moves: Vec<Move>,
    pub index: usize,
    pub elapsed: f64,
    pub playing: bool,
    pub settle_gen: u64,
}

impl Playback {
    pub fn new(start: Cube, moves: Vec<Move>) -> Playback {
        Playback {
            cube: start,
            moves,
            index: 0,
            elapsed: 0.0,
            playing: false,
            settle_gen: 1,
        }
    }

    pub fn finished(&self) -> bool {
        self.index >= self.moves.len()
    }

    pub fn toggle(&mut self) {
        if !self.finished() {
            self.playing = !self.playing;
        }
    }

    pub fn current_turn(&self) -> Option<Turn> {
        if self.finished() {
            None
        } else {
            Some(turn_of(self.moves[self.index]))
        }
    }

    pub fn current_progress(&self, secs_per_move: f64) -> f32 {
        if self.finished() {
            0.0
        } else {
            turn_progress(self.elapsed, secs_per_move)
        }
    }

    pub fn sticker_face(&self, cell: &Cell) -> usize {
        home_face(self.cube.sticker_at(cell.slot))
    }

    pub fn tick(&mut self, dt: f64, secs_per_move: f64) {
        if !self.playing {
            return;
        }
        self.elapsed += dt.max(0.0);
        while self.elapsed >= secs_per_move && self.index < self.moves.len() {
            self.cube.apply(self.moves[self.index]);
            self.index += 1;
            self.elapsed -= secs_per_move.max(0.0);
            self.settle_gen += 1;
        }
        if self.finished() {
            self.playing = false;
            self.elapsed = 0.0;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use bt_core::{Alg, Face};

    fn cfg() -> ReplayCfg {
        ReplayCfg::default()
    }

    fn orbit() -> Orbit {
        default_orbit(&cfg())
    }

    #[test]
    fn default_orbit_matches_config_defaults() {
        let orbit = orbit();
        assert_eq!(orbit.yaw_deg, 45.0);
        assert_eq!(orbit.pitch_deg, 30.0);
        assert_eq!(orbit.distance, 6.0);
    }

    #[test]
    fn drag_clamps_pitch_at_both_bounds() {
        let far_up = apply_drag_delta(orbit(), &cfg(), 0.0, 5000.0);
        assert_eq!(far_up.pitch_deg, cfg().pitch_clamp_deg[1]);
        let far_down = apply_drag_delta(orbit(), &cfg(), 0.0, -5000.0);
        assert_eq!(far_down.pitch_deg, cfg().pitch_clamp_deg[0]);
        let small = apply_drag_delta(orbit(), &cfg(), 0.0, 10.0);
        assert!((small.pitch_deg - 34.0).abs() < 1e-5);
    }

    #[test]
    fn drag_wraps_yaw_around() {
        let spun = apply_drag_delta(
            Orbit {
                yaw_deg: 170.0,
                ..orbit()
            },
            &cfg(),
            100.0,
            0.0,
        );
        assert!((spun.yaw_deg - (-150.0)).abs() < 1e-5);
        let spun_back = apply_drag_delta(
            Orbit {
                yaw_deg: -170.0,
                ..orbit()
            },
            &cfg(),
            -100.0,
            0.0,
        );
        assert!((spun_back.yaw_deg - 150.0).abs() < 1e-5);
        assert!((wrap_yaw(180.0) - 180.0).abs() < 1e-5);
        assert!((wrap_yaw(-180.0) - 180.0).abs() < 1e-5);
        assert!((wrap_yaw(0.0)).abs() < 1e-5);
    }

    #[test]
    fn drag_keeps_yaw_free_and_distance_untouched() {
        let moved = apply_drag_delta(orbit(), &cfg(), 900.0, 0.0);
        assert_eq!(moved.distance, orbit().distance);
        assert!((-180.0..=180.0).contains(&moved.yaw_deg));
        let centered = apply_drag_delta(orbit(), &cfg(), 3600.0, 0.0);
        assert!((centered.yaw_deg - 45.0).abs() < 1e-5);
    }

    #[test]
    fn zoom_clamps_at_both_bounds() {
        let in_all_the_way = apply_zoom(orbit(), &cfg(), 100.0);
        assert_eq!(in_all_the_way.distance, cfg().zoom_min);
        let out_all_the_way = apply_zoom(orbit(), &cfg(), -100.0);
        assert_eq!(out_all_the_way.distance, cfg().zoom_max);
        let one_notch = apply_zoom(orbit(), &cfg(), 1.0);
        assert!((one_notch.distance - (6.0 - ZOOM_UNITS_PER_NOTCH)).abs() < 1e-5);
        assert_eq!(one_notch.yaw_deg, orbit().yaw_deg);
        assert_eq!(one_notch.pitch_deg, orbit().pitch_deg);
    }

    #[test]
    fn camera_offset_places_the_eye_on_the_sphere() {
        let front = camera_offset(&Orbit {
            yaw_deg: 0.0,
            pitch_deg: 0.0,
            distance: 6.0,
        });
        assert_eq!(front, (0.0, 0.0, 6.0));
        let right = camera_offset(&Orbit {
            yaw_deg: 90.0,
            pitch_deg: 0.0,
            distance: 6.0,
        });
        assert!((right.0 - 6.0).abs() < 1e-5);
        assert!(right.1.abs() < 1e-5);
        assert!(right.2.abs() < 1e-5);
        let above = camera_offset(&Orbit {
            yaw_deg: 0.0,
            pitch_deg: 90.0,
            distance: 6.0,
        });
        assert!((above.1 - 6.0).abs() < 1e-5);
        let (x, y, z) = camera_offset(&orbit());
        let radius = (x * x + y * y + z * z).sqrt();
        assert!((radius - 6.0).abs() < 1e-4);
    }

    #[test]
    fn progress_is_eased_monotonic_and_bounded() {
        assert_eq!(turn_progress(-1.0, 0.5), 0.0);
        assert_eq!(turn_progress(0.0, 0.5), 0.0);
        assert_eq!(turn_progress(0.5, 0.5), 1.0);
        assert_eq!(turn_progress(9.0, 0.5), 1.0);
        assert_eq!(turn_progress(1.0, 0.0), 1.0);
        assert_eq!(turn_progress(0.0, 0.0), 1.0);
        let mid = turn_progress(0.25, 0.5);
        assert!((mid - 0.5).abs() < 1e-5);
        let mut last = 0.0f32;
        for step in 0..=40 {
            let t = turn_progress(step as f64 * 0.02, 0.5);
            assert!(t >= last, "progress must not decrease at step {step}");
            last = t;
        }
        assert!((last - 1.0).abs() < 1e-6);
    }

    #[test]
    fn cells_cover_every_slot_once_and_match_engine_ids() {
        let mut seen = [false; 54];
        for cell in CELLS.iter() {
            let slot = cell.slot as usize;
            assert!(!seen[slot], "slot {slot} claimed twice");
            seen[slot] = true;
            if cell.row == 1 && cell.col == 1 {
                assert_eq!(cell.slot, bt_core::center_id(Face::ALL[cell.face]));
            } else if cell.row.is_multiple_of(2) && cell.col.is_multiple_of(2) {
                assert_eq!(
                    cell.slot,
                    bt_core::corner_id(Face::ALL[cell.face], corner_pos(cell.row, cell.col) as u8)
                );
            } else {
                assert_eq!(
                    cell.slot,
                    bt_core::edge_id(Face::ALL[cell.face], edge_pos(cell.row, cell.col) as u8)
                );
            }
        }
        assert!(seen.iter().all(|&hit| hit));
        let mut spots = CELLS.iter().map(|cell| cell.pos).collect::<Vec<I3>>();
        spots.sort_unstable();
        spots.dedup();
        assert_eq!(spots.len(), 26);
        assert!(!spots.contains(&(0, 0, 0)));
    }

    #[test]
    fn face_order_matches_the_theme_face_table() {
        let names = ["U", "L", "F", "R", "B", "D"];
        for (face, name) in names.iter().enumerate() {
            let normal = FACE_NORMALS[face];
            let spelled = match normal {
                (0, 1, 0) => "U",
                (-1, 0, 0) => "L",
                (0, 0, 1) => "F",
                (1, 0, 0) => "R",
                (0, 0, -1) => "B",
                (0, -1, 0) => "D",
                _ => panic!("face {face} normal {normal:?} off axis"),
            };
            assert_eq!(spelled, *name);
        }
    }

    #[test]
    fn solved_cube_paints_every_cell_with_its_own_face() {
        let cube = Cube::solved();
        for cell in CELLS.iter() {
            assert_eq!(home_face(cube.sticker_at(cell.slot)), cell.face);
        }
        assert_eq!(home_face(0), 0);
        assert_eq!(home_face(23), 5);
        assert_eq!(home_face(24), 0);
        assert_eq!(home_face(47), 5);
        assert_eq!(home_face(48), 0);
        assert_eq!(home_face(53), 5);
    }

    fn all_moves() -> Vec<Move> {
        let mut out = Vec::new();
        for face in Face::ALL {
            for quarter in 1..=3u8 {
                out.push(Move::Face { face, quarter });
                out.push(Move::Inner { face, quarter });
                out.push(Move::Wide { face, quarter });
            }
        }
        for slice in [Slice::M, Slice::E, Slice::S] {
            for quarter in 1..=3u8 {
                out.push(Move::Slice { slice, quarter });
            }
        }
        for axis in [Axis::X, Axis::Y, Axis::Z] {
            for quarter in 1..=3u8 {
                out.push(Move::Rotation { axis, quarter });
            }
        }
        out
    }

    #[test]
    fn every_move_family_turns_exactly_like_the_engine_permutes() {
        for mv in all_moves() {
            let turn = turn_of(mv);
            let mut cube = Cube::solved();
            cube.apply(mv);
            for cell in CELLS.iter() {
                let target = turned_cell(cell, &turn);
                assert_eq!(
                    target.slot,
                    cube.position_of(cell.slot),
                    "move {mv:?} sticker {} expected slot {} got {}",
                    cell.slot,
                    cube.position_of(cell.slot),
                    target.slot
                );
            }
        }
    }

    #[test]
    fn turn_of_picks_the_physical_layer_for_each_family() {
        let r = turn_of(Move::Face {
            face: Face::R,
            quarter: 1,
        });
        assert!(matches!(r.axis, Axis::X));
        assert_eq!(r.mask, [false, false, true]);
        assert!(r.rotates((1, 0, 0)));
        assert!(!r.rotates((0, 0, 0)));
        assert!(!r.rotates((-1, 0, 0)));
        let u = turn_of(Move::Face {
            face: Face::U,
            quarter: 1,
        });
        assert!(matches!(u.axis, Axis::Y));
        assert_eq!(u.mask, [false, false, true]);
        assert!(u.rotates((0, 1, 0)));
        assert!(!u.rotates((0, 0, 0)));
        let inner_r = turn_of(Move::Inner {
            face: Face::R,
            quarter: 1,
        });
        assert_eq!(inner_r.mask, [false, true, false]);
        assert!(inner_r.rotates((0, 0, 0)));
        assert!(!inner_r.rotates((1, 0, 0)));
        let wide_r = turn_of(Move::Wide {
            face: Face::R,
            quarter: 1,
        });
        assert_eq!(wide_r.mask, [false, true, true]);
        let m = turn_of(Move::Slice {
            slice: Slice::M,
            quarter: 1,
        });
        assert!(matches!(m.axis, Axis::X));
        assert_eq!(m.mask, [false, true, false]);
        let x = turn_of(Move::Rotation {
            axis: Axis::X,
            quarter: 1,
        });
        assert!(matches!(x.axis, Axis::X));
        assert_eq!(x.mask, [true; 3]);
        assert!(x.rotates((-1, -1, -1)));
    }

    #[test]
    fn angle_scales_with_applied_quarters_and_progress() {
        let half = Turn {
            axis: Axis::Y,
            applied: 2,
            mask: [true; 3],
        };
        assert!((half.angle_deg(1.0) - 180.0).abs() < 1e-5);
        assert!((half.angle_deg(0.5) - 90.0).abs() < 1e-5);
        let prime = Turn {
            axis: Axis::Y,
            applied: 3,
            mask: [true; 3],
        };
        assert!((prime.angle_deg(1.0) - 270.0).abs() < 1e-5);
        assert!((prime.angle_deg(0.0)).abs() < 1e-6);
    }

    #[test]
    fn playback_settles_each_move_and_freezes_at_the_end() {
        let alg = Alg::parse("R U r' 2F M2 x S'").expect("fixture alg");
        let mut start = Cube::solved();
        start.apply_alg(&alg);
        let mut playback = Playback::new(start, alg.inverse().moves().to_vec());
        assert!(!playback.finished());
        assert_eq!(playback.current_turn().map(|t| t.axis), Some(Axis::Z));
        playback.toggle();
        assert!(playback.playing);
        let start_gen = playback.settle_gen;
        playback.tick(0.4, 0.5);
        assert_eq!(playback.index, 0);
        assert!((playback.current_progress(0.5) - turn_progress(0.4, 0.5)).abs() < 1e-6);
        playback.tick(0.2, 0.5);
        assert_eq!(playback.index, 1);
        assert_eq!(playback.settle_gen, start_gen + 1);
        for _ in 0..60 {
            playback.tick(0.5, 0.5);
        }
        assert!(playback.finished());
        assert!(!playback.playing);
        assert!(playback.cube.is_solved());
        assert_eq!(playback.current_turn(), None);
        playback.toggle();
        assert!(!playback.playing);
    }

    #[test]
    fn playback_catches_up_when_a_frame_spans_several_moves() {
        let alg = Alg::parse("R U F2").expect("fixture alg");
        let mut playback = Playback::new(Cube::solved(), alg.moves().to_vec());
        playback.toggle();
        playback.tick(1.6, 0.5);
        assert_eq!(playback.index, 3);
        assert!(playback.finished());
        let mut reference = Cube::solved();
        reference.apply_alg(&alg);
        assert_eq!(playback.cube, reference);
    }

    #[test]
    fn paused_playback_holds_mid_turn_state() {
        let alg = Alg::parse("U").expect("fixture alg");
        let mut playback = Playback::new(Cube::solved(), alg.moves().to_vec());
        playback.toggle();
        playback.tick(0.25, 0.5);
        playback.toggle();
        assert!(!playback.playing);
        let before = playback.cube;
        playback.tick(1.0, 0.5);
        assert_eq!(playback.cube, before);
        assert_eq!(playback.index, 0);
        assert!((playback.current_progress(0.5) - 0.5).abs() < 1e-5);
    }

    #[test]
    fn zero_move_replay_shows_a_still_cube() {
        let mut playback = Playback::new(Cube::solved(), Vec::new());
        assert!(playback.finished());
        assert_eq!(playback.current_turn(), None);
        playback.toggle();
        assert!(!playback.playing);
        playback.tick(1.0, 0.5);
        assert!(playback.cube.is_solved());
    }

    #[test]
    fn cfg_sanitizes_reversed_and_out_of_range_values() {
        let raw = ReplayCfg {
            secs_per_move: -1.0,
            yaw_deg: 405.0,
            pitch_deg: 90.0,
            distance: 99.0,
            zoom_min: 12.0,
            zoom_max: 3.5,
            pitch_clamp_deg: [80.0, -80.0],
        };
        let clean = raw.sanitized();
        assert_eq!(clean.secs_per_move, 0.0);
        assert!((clean.yaw_deg - 45.0).abs() < 1e-5);
        assert_eq!(clean.pitch_deg, 80.0);
        assert_eq!(clean.distance, 12.0);
        assert_eq!(clean.zoom_min, 3.5);
        assert_eq!(clean.zoom_max, 12.0);
        assert_eq!(clean.pitch_clamp_deg, [-80.0, 80.0]);
    }

    #[test]
    fn cfg_reads_the_hub_section_and_falls_back_to_defaults() {
        let hub = serde_json::json!({
            "app": {
                "replay": {
                    "secs_per_move": 0.25,
                    "yaw_deg": 30.0,
                    "pitch_deg": 20.0,
                    "distance": 8.0,
                    "zoom_min": 4.0,
                    "zoom_max": 10.0,
                    "pitch_clamp_deg": [-70.0, 70.0]
                }
            }
        });
        let parsed = replay_cfg_from(&hub);
        assert_eq!(parsed.secs_per_move, 0.25);
        assert_eq!(parsed.yaw_deg, 30.0);
        assert_eq!(parsed.pitch_deg, 20.0);
        assert_eq!(parsed.distance, 8.0);
        assert_eq!(parsed.zoom_min, 4.0);
        assert_eq!(parsed.zoom_max, 10.0);
        assert_eq!(parsed.pitch_clamp_deg, [-70.0, 70.0]);
        assert_eq!(replay_cfg_from(&serde_json::json!({})), cfg());
        assert_eq!(
            replay_cfg_from(&serde_json::json!({"app": {"replay": "broken"}})),
            cfg()
        );
        let partial = replay_cfg_from(&serde_json::json!({"app": {"replay": {"distance": 7.0}}}));
        assert_eq!(partial.distance, 7.0);
        assert_eq!(partial.secs_per_move, cfg().secs_per_move);
    }
}
