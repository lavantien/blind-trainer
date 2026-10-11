use crate::logic::{AppConfig, BootWarnings, FatalReason};
use crate::net::{self, NetGeometry};
use crate::states::AppState;
use crate::theme;
use crate::views;
use bevy::feathers::FeathersPlugins;
use bevy::feathers::dark_theme::create_dark_theme;
use bevy::feathers::theme::UiTheme;
use bevy::prelude::*;
use bevy::state::state_scoped::DespawnOnExit;

const FONT_REGULAR: &str = "fonts/NotoSans-Regular.ttf";
const FONT_BOLD: &str = "fonts/NotoSans-Bold.ttf";

#[derive(Clone, Debug, Resource)]
pub struct FontBook {
    pub regular: Handle<Font>,
    pub bold: Handle<Font>,
}

pub struct PresentationPlugin;

impl Plugin for PresentationPlugin {
    fn build(&self, app: &mut App) {
        app.add_plugins(views::ViewsLogicPlugin)
            .add_plugins(FeathersPlugins)
            .insert_resource(UiTheme(create_dark_theme()))
            .insert_resource(RemSize(theme::ROOT_REM))
            .add_systems(Startup, (load_fonts, spawn_camera))
            .add_systems(Update, net::update_net_colors)
            .add_systems(
                Update,
                (
                    (
                        views::drill::read_input,
                        views::drill::sync_input,
                        views::drill::bind_text,
                        views::drill::focus_input_on_memo,
                    )
                        .chain(),
                    views::memo::bind_text,
                    views::solve::bind_text,
                )
                    .after(views::ViewsLogic),
            )
            .add_systems(
                OnEnter(AppState::Menu),
                |mut commands: Commands, fonts: Res<FontBook>, warnings: Res<BootWarnings>| {
                    views::menu::spawn(&mut commands, &fonts, &warnings.0);
                },
            )
            .add_systems(
                OnEnter(AppState::Drill),
                |mut commands: Commands, fonts: Res<FontBook>, config: Res<AppConfig>| {
                    views::drill::spawn(&mut commands, &fonts, &config);
                },
            )
            .add_systems(
                OnEnter(AppState::Memo),
                |mut commands: Commands, fonts: Res<FontBook>| {
                    views::memo::spawn(&mut commands, &fonts);
                },
            )
            .add_systems(
                OnEnter(AppState::Solve),
                |mut commands: Commands, fonts: Res<FontBook>, config: Res<AppConfig>| {
                    views::solve::spawn(&mut commands, &fonts, &config);
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

pub fn spawn_text_view(
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
        spawn_title(parent, fonts, title);
        for line in lines {
            spawn_hint(parent, fonts, line);
        }
        if let Some(geometry) = geometry {
            net::spawn_net(parent, geometry);
        }
    });
    root.id()
}

pub fn spawn_title(parent: &mut ChildSpawnerCommands, fonts: &FontBook, title: &str) {
    parent.spawn((
        Text::new(title),
        TextFont {
            font: FontSource::Handle(fonts.bold.clone()),
            font_size: FontSize::Rem(theme::FONT_TITLE),
            ..default()
        },
        TextColor(theme::text_primary()),
    ));
}

pub fn spawn_hint(parent: &mut ChildSpawnerCommands, fonts: &FontBook, line: &str) {
    parent.spawn((
        Text::new(line),
        TextFont {
            font: FontSource::Handle(fonts.regular.clone()),
            font_size: FontSize::Rem(theme::FONT_SMALL),
            ..default()
        },
        TextColor(theme::text_muted()),
    ));
}

pub fn spawn_dynamic<M: Component>(
    parent: &mut ChildSpawnerCommands,
    fonts: &FontBook,
    text: &str,
    marker: M,
) {
    parent.spawn((
        marker,
        Text::new(text),
        TextFont {
            font: FontSource::Handle(fonts.regular.clone()),
            font_size: FontSize::Rem(theme::FONT_BODY),
            ..default()
        },
        TextColor(theme::text_primary()),
    ));
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
    spawn_text_view(&mut commands, &fonts, AppState::Fatal, "fatal", &refs, None);
}
