use crate::logic::{AppConfig, BootWarnings, FatalReason};
use crate::net::{self, NetGeometry};
use crate::states::AppState;
use crate::theme;
use bevy::feathers::FeathersPlugins;
use bevy::feathers::controls::{FeathersTextInput, FeathersTextInputContainer};
use bevy::feathers::dark_theme::create_dark_theme;
use bevy::feathers::theme::UiTheme;
use bevy::prelude::*;
use bevy::state::state_scoped::DespawnOnExit;

const FONT_REGULAR: &str = "fonts/NotoSans-Regular.ttf";
const FONT_BOLD: &str = "fonts/NotoSans-Bold.ttf";

const MENU_HINTS: [&str; 3] = [
    "1 drill   2 memo   3 solve",
    "4 algs   5 stats   6 settings",
    "esc back to menu",
];

#[derive(Clone, Debug, Resource)]
pub struct FontBook {
    pub regular: Handle<Font>,
    pub bold: Handle<Font>,
}

pub struct PresentationPlugin;

impl Plugin for PresentationPlugin {
    fn build(&self, app: &mut App) {
        app.add_plugins(FeathersPlugins)
            .insert_resource(UiTheme(create_dark_theme()))
            .insert_resource(RemSize(theme::ROOT_REM))
            .add_systems(Startup, (load_fonts, spawn_camera))
            .add_systems(Update, net::update_net_colors)
            .add_systems(
                OnEnter(AppState::Menu),
                |mut commands: Commands, fonts: Res<FontBook>| {
                    spawn_view(
                        &mut commands,
                        &fonts,
                        AppState::Menu,
                        "menu",
                        &MENU_HINTS,
                        None,
                    );
                },
            )
            .add_systems(
                OnEnter(AppState::Drill),
                |mut commands: Commands, fonts: Res<FontBook>, config: Res<AppConfig>| {
                    spawn_view(
                        &mut commands,
                        &fonts,
                        AppState::Drill,
                        "drill",
                        &["buffer sticker pulses, target sticker highlights"],
                        Some(net_geometry(&config)),
                    );
                },
            )
            .add_systems(
                OnEnter(AppState::Memo),
                |mut commands: Commands, fonts: Res<FontBook>| {
                    spawn_view(
                        &mut commands,
                        &fonts,
                        AppState::Memo,
                        "memo training",
                        &["syllables stream at the configured cadence"],
                        None,
                    );
                },
            )
            .add_systems(
                OnEnter(AppState::Solve),
                |mut commands: Commands, fonts: Res<FontBook>, config: Res<AppConfig>| {
                    spawn_view(
                        &mut commands,
                        &fonts,
                        AppState::Solve,
                        "solve",
                        &["ready, memo, exec, review"],
                        Some(net_geometry(&config)),
                    );
                },
            )
            .add_systems(
                OnEnter(AppState::Algs),
                |mut commands: Commands, fonts: Res<FontBook>| {
                    spawn_view(
                        &mut commands,
                        &fonts,
                        AppState::Algs,
                        "algs",
                        &["browse and edit case algorithms"],
                        None,
                    );
                },
            )
            .add_systems(
                OnEnter(AppState::Stats),
                |mut commands: Commands, fonts: Res<FontBook>| {
                    spawn_view(
                        &mut commands,
                        &fonts,
                        AppState::Stats,
                        "stats",
                        &["solve history and splits"],
                        None,
                    );
                },
            )
            .add_systems(
                OnEnter(AppState::Settings),
                |mut commands: Commands, fonts: Res<FontBook>| {
                    let entity = spawn_view(
                        &mut commands,
                        &fonts,
                        AppState::Settings,
                        "settings",
                        &["memo cadence, memo mode, drill reps"],
                        None,
                    );
                    commands
                        .spawn_scene(bsn! {
                            @FeathersTextInputContainer
                            Node { width: px(400.0), flex_grow: 0.0 }
                            Children [
                                @FeathersTextInput
                            ]
                        })
                        .insert(ChildOf(entity));
                },
            )
            .add_systems(OnEnter(AppState::Fatal), enter_fatal);
    }
}

fn load_fonts(mut commands: Commands, server: Res<AssetServer>) {
    commands.insert_resource(FontBook {
        regular: server.load(FONT_REGULAR),
        bold: server.load(FONT_BOLD),
    });
}

fn spawn_camera(mut commands: Commands) {
    commands.spawn(Camera2d);
}

pub fn net_geometry(config: &AppConfig) -> NetGeometry {
    let reserve = theme::ROOT_REM * theme::NET_VIEW_RESERVE_REM;
    NetGeometry::for_window(
        config.window.width as f32,
        (config.window.height as f32 - reserve).max(theme::ROOT_REM),
    )
}

fn spawn_view(
    commands: &mut Commands,
    fonts: &FontBook,
    state: AppState,
    title: &str,
    lines: &[&str],
    geometry: Option<NetGeometry>,
) -> Entity {
    let mut root = commands.spawn((
        DespawnOnExit(state),
        Node {
            width: Val::Percent(100.0),
            height: Val::Percent(100.0),
            flex_direction: FlexDirection::Column,
            align_items: AlignItems::Center,
            justify_content: JustifyContent::Center,
            row_gap: theme::rem(theme::SPACING_REM),
            padding: UiRect::all(theme::rem(theme::PANEL_PADDING_REM)),
            ..default()
        },
        BackgroundColor(theme::background()),
    ));
    root.with_children(|parent| {
        parent.spawn((
            Text::new(title),
            TextFont {
                font: FontSource::Handle(fonts.bold.clone()),
                font_size: FontSize::Rem(theme::FONT_TITLE),
                ..default()
            },
            TextColor(theme::text_primary()),
        ));
        for line in lines {
            parent.spawn((
                Text::new(*line),
                TextFont {
                    font: FontSource::Handle(fonts.regular.clone()),
                    font_size: FontSize::Rem(theme::FONT_SMALL),
                    ..default()
                },
                TextColor(theme::text_muted()),
            ));
        }
        if let Some(geometry) = geometry {
            net::spawn_net(parent, geometry);
        }
    });
    root.id()
}

fn enter_fatal(
    mut commands: Commands,
    fonts: Res<FontBook>,
    reason: Res<FatalReason>,
    warnings: Res<BootWarnings>,
) {
    let mut lines: Vec<String> = vec![reason.0.clone()];
    lines.extend(warnings.0.iter().cloned());
    let refs: Vec<&str> = lines.iter().map(String::as_str).collect();
    spawn_view(&mut commands, &fonts, AppState::Fatal, "fatal", &refs, None);
}
