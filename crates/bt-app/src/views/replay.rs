use crate::logic::{RuntimePaths, VendorCorpus, load_config_value};
use crate::replay_scene::{
    CELLS, CUBIE_SPAN, I3, Orbit, Playback, ReplayCfg, STICKER_LIFT, STICKER_SPAN,
    apply_drag_delta, apply_zoom, camera_offset, default_orbit, replay_cfg_from,
};
use crate::states::AppState;
use crate::theme;
use crate::views::solve::{SolveSession, executed_moves};
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::input::mouse::{AccumulatedMouseMotion, MouseWheel};
use bevy::prelude::*;
use bevy::render::view::Tonemapping;
use bevy::state::state_scoped::DespawnOnExit;

#[derive(Clone, Debug, Resource)]
pub struct ReplayScene {
    pub cfg: ReplayCfg,
    pub orbit: Orbit,
    pub playback: Playback,
}

impl Default for ReplayScene {
    fn default() -> ReplayScene {
        let cfg = ReplayCfg::default();
        ReplayScene {
            orbit: default_orbit(&cfg),
            cfg,
            playback: Playback::new(bt_core::Cube::solved(), Vec::new()),
        }
    }
}

#[derive(Component)]
pub struct ReplayCubie {
    pub pos: I3,
}

#[derive(Component)]
pub struct ReplaySticker {
    pub cell: usize,
}

pub fn enter(
    mut commands: Commands,
    session: Res<SolveSession>,
    corpus: Res<VendorCorpus>,
    paths: Res<RuntimePaths>,
    mut scene: ResMut<ReplayScene>,
) {
    let cfg = replay_cfg_from(&load_config_value(&paths.config_path).unwrap_or_default());
    let moves = session
        .memo
        .as_ref()
        .map(|memo| {
            let mut broken = Vec::new();
            executed_moves(&corpus.0, memo, &mut broken)
        })
        .unwrap_or_default();
    scene.cfg = cfg;
    scene.orbit = default_orbit(&scene.cfg);
    scene.playback = Playback::new(session.scramble, moves);
    commands.insert_resource(ClearColor(theme::background()));
}

pub fn replay_open_key(
    mut inputs: MessageReader<KeyboardInput>,
    mut next: ResMut<NextState<AppState>>,
) {
    for input in inputs.read() {
        if input.state == ButtonState::Pressed && input.key_code == KeyCode::KeyV {
            next.set(AppState::Replay);
            return;
        }
    }
}

pub fn keys(
    mut inputs: MessageReader<KeyboardInput>,
    mut scene: ResMut<ReplayScene>,
    mut next: ResMut<NextState<AppState>>,
) {
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        match input.key_code {
            KeyCode::Space => scene.playback.toggle(),
            KeyCode::KeyR => scene.orbit = default_orbit(&scene.cfg),
            KeyCode::KeyV => next.set(AppState::Solve),
            _ => {}
        }
    }
}

pub fn drag(
    buttons: Res<ButtonInput<MouseButton>>,
    motion: Res<AccumulatedMouseMotion>,
    mut scene: ResMut<ReplayScene>,
) {
    let held = buttons.any_pressed([MouseButton::Left, MouseButton::Right, MouseButton::Middle]);
    if held && (motion.delta.x != 0.0 || motion.delta.y != 0.0) {
        scene.orbit = apply_drag_delta(scene.orbit, &scene.cfg, motion.delta.x, motion.delta.y);
    }
}

pub fn zoom(mut wheel: MessageReader<MouseWheel>, mut scene: ResMut<ReplayScene>) {
    let notches: f32 = wheel.read().map(|event| event.y).sum();
    if notches != 0.0 {
        scene.orbit = apply_zoom(scene.orbit, &scene.cfg, notches);
    }
}

pub fn tick(time: Res<Time<Virtual>>, mut scene: ResMut<ReplayScene>) {
    let dt = time.delta_secs() as f64;
    let secs = scene.cfg.secs_per_move;
    scene.playback.tick(dt, secs);
}

pub fn spawn(
    mut commands: Commands,
    scene: Res<ReplayScene>,
    mut meshes: ResMut<Assets<Mesh>>,
    mut materials: ResMut<Assets<StandardMaterial>>,
    mut ui_cameras: Query<&mut Camera, (With<Camera2d>, Without<Camera3d>)>,
) {
    if let Ok(mut camera) = ui_cameras.single_mut() {
        camera.is_active = false;
    }
    let body_mesh = meshes.add(Cuboid::new(CUBIE_SPAN, CUBIE_SPAN, CUBIE_SPAN));
    let sticker_mesh = meshes.add(Rectangle::new(STICKER_SPAN, STICKER_SPAN));
    let body_material = materials.add(StandardMaterial {
        base_color: Color::srgb(0.04, 0.04, 0.05),
        ..default()
    });
    let sticker_materials: Vec<Handle<StandardMaterial>> = CELLS
        .iter()
        .map(|cell| {
            materials.add(StandardMaterial {
                base_color: theme::face_color(scene.playback.sticker_face(cell)),
                ..default()
            })
        })
        .collect();
    commands
        .spawn((
            DespawnOnExit(AppState::Replay),
            AmbientLight::default(),
            Transform::default(),
            Visibility::default(),
        ))
        .with_children(|parent| {
            parent.spawn((
                DirectionalLight::default(),
                Transform::from_xyz(4.0, 8.0, 5.0).looking_at(Vec3::ZERO, Vec3::Y),
            ));
            parent.spawn((
                Camera3d::default(),
                Tonemapping::SomewhatBoringDisplayTransform,
                Transform::default(),
            ));
            for x in -1..=1i8 {
                for y in -1..=1i8 {
                    for z in -1..=1i8 {
                        spawn_cubie(
                            parent,
                            (x, y, z),
                            &body_mesh,
                            &body_material,
                            &sticker_mesh,
                            &sticker_materials,
                        );
                    }
                }
            }
        });
}

fn spawn_cubie(
    parent: &mut ChildSpawnerCommands,
    pos: I3,
    body_mesh: &Handle<Mesh>,
    body_material: &Handle<StandardMaterial>,
    sticker_mesh: &Handle<Mesh>,
    sticker_materials: &[Handle<StandardMaterial>],
) {
    parent
        .spawn((
            ReplayCubie { pos },
            Mesh3d(body_mesh.clone()),
            MeshMaterial3d(body_material.clone()),
            Transform::from_translation(vec_of(pos)),
        ))
        .with_children(|cubie| {
            for (index, cell) in CELLS.iter().enumerate() {
                if cell.pos != pos {
                    continue;
                }
                let normal = vec_of(cell.normal);
                cubie.spawn((
                    ReplaySticker { cell: index },
                    Mesh3d(sticker_mesh.clone()),
                    MeshMaterial3d(sticker_materials[index].clone()),
                    Transform::from_translation(normal * STICKER_LIFT)
                        .with_rotation(Quat::from_rotation_arc(Vec3::Z, normal)),
                ));
            }
        });
}

pub fn restore_ui_camera(mut ui_cameras: Query<&mut Camera, (With<Camera2d>, Without<Camera3d>)>) {
    if let Ok(mut camera) = ui_cameras.single_mut() {
        camera.is_active = true;
    }
}

pub fn bind_cubies(scene: Res<ReplayScene>, mut cubies: Query<(&ReplayCubie, &mut Transform)>) {
    let turn = scene.playback.current_turn();
    let progress = scene.playback.current_progress(scene.cfg.secs_per_move);
    for (cubie, mut transform) in cubies.iter_mut() {
        let base = vec_of(cubie.pos);
        match turn {
            Some(turn) if turn.rotates(cubie.pos) => {
                let quat = Quat::from_axis_angle(
                    axis_vec(turn.axis),
                    turn.angle_deg(progress).to_radians(),
                );
                *transform = Transform::from_translation(quat * base).with_rotation(quat);
            }
            _ => *transform = Transform::from_translation(base),
        }
    }
}

pub fn bind_camera(
    scene: Res<ReplayScene>,
    mut cameras: Query<&mut Transform, (With<Camera3d>, Without<Camera2d>)>,
) {
    let (x, y, z) = camera_offset(&scene.orbit);
    for mut transform in cameras.iter_mut() {
        *transform = Transform::from_xyz(x, y, z).looking_at(Vec3::ZERO, Vec3::Y);
    }
}

pub fn bind_stickers(
    scene: Res<ReplayScene>,
    mut applied: Local<u64>,
    mut stickers: Query<(&ReplaySticker, &MeshMaterial3d<StandardMaterial>)>,
    mut materials: ResMut<Assets<StandardMaterial>>,
) {
    if scene.playback.settle_gen == *applied {
        return;
    }
    *applied = scene.playback.settle_gen;
    for (sticker, material) in stickers.iter_mut() {
        let face = scene.playback.sticker_face(&CELLS[sticker.cell]);
        if let Some(mut standard) = materials.get_mut(material.id()) {
            standard.base_color = theme::face_color(face);
        }
    }
}

fn vec_of(pos: I3) -> Vec3 {
    Vec3::new(pos.0 as f32, pos.1 as f32, pos.2 as f32)
}

fn axis_vec(axis: bt_core::Axis) -> Vec3 {
    match axis {
        bt_core::Axis::X => Vec3::X,
        bt_core::Axis::Y => Vec3::Y,
        bt_core::Axis::Z => Vec3::Z,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::replay_scene::{cell_at, turn_of, turned_cell};
    use bt_core::{Alg, Axis, Cube, Face, Move};

    #[test]
    fn every_cubie_carries_its_own_stickers() {
        let mut cubies = 0;
        for x in -1..=1i8 {
            for y in -1..=1i8 {
                for z in -1..=1i8 {
                    let stickers = CELLS.iter().filter(|cell| cell.pos == (x, y, z)).count();
                    let outward = [x == -1, x == 1, y == -1, y == 1, z == -1, z == 1]
                        .iter()
                        .filter(|edge| **edge)
                        .count();
                    assert_eq!(stickers, outward, "cubie ({x},{y},{z})");
                    cubies += 1;
                }
            }
        }
        assert_eq!(cubies, 27);
        assert_eq!(CELLS.len(), 54);
    }

    #[test]
    fn turned_cell_is_identity_outside_the_layer() {
        let turn = turn_of(Move::Face {
            face: Face::R,
            quarter: 1,
        });
        let settled = cell_at((-1, 0, 0), (-1, 0, 0)).expect("L center edge cell");
        let turned = turned_cell(settled, &turn);
        assert_eq!(turned.slot, settled.slot);
        let moving = cell_at((1, 1, 0), (1, 0, 0)).expect("R face upper edge cell");
        let moved = turned_cell(moving, &turn);
        assert_ne!(moved.slot, moving.slot);
    }

    #[test]
    fn axis_vec_matches_the_engine_axes() {
        assert_eq!(axis_vec(Axis::X), Vec3::X);
        assert_eq!(axis_vec(Axis::Y), Vec3::Y);
        assert_eq!(axis_vec(Axis::Z), Vec3::Z);
    }

    #[test]
    fn scene_default_holds_a_still_solved_cube() {
        let scene = ReplayScene::default();
        assert_eq!(scene.playback.cube, Cube::solved());
        assert!(scene.playback.finished());
        assert_eq!(scene.orbit, default_orbit(&scene.cfg));
        assert_eq!(scene.cfg, ReplayCfg::default());
    }

    #[test]
    fn played_scene_replays_the_collected_moves() {
        let alg = Alg::parse("R U R' U'").expect("fixture alg");
        let mut start = Cube::solved();
        start.apply_alg(&alg);
        let mut playback = Playback::new(start, alg.inverse().moves().to_vec());
        playback.toggle();
        for _ in 0..16 {
            playback.tick(0.5, 0.5);
        }
        assert!(playback.cube.is_solved());
    }
}
