use crate::logic::{AppConfig, BootWarnings, FatalReason, VendorCorpus};
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
                    (views::algs::read_query, views::algs::bind_detail).chain(),
                    IntoScheduleConfigs::run_if(views::memo::bind_text, in_state(AppState::Memo)),
                    IntoScheduleConfigs::run_if(views::solve::bind_text, in_state(AppState::Solve)),
                    views::stats::bind_text,
                    views::settings::bind_text,
                )
                    .after(views::ViewsLogic)
                    .run_if(resource_exists::<VendorCorpus>),
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
            .add_systems(
                OnEnter(AppState::Algs),
                |mut commands: Commands, fonts: Res<FontBook>, config: Res<AppConfig>| {
                    views::algs::spawn(&mut commands, &fonts, &config);
                },
            )
            .add_systems(
                OnEnter(AppState::Stats),
                |mut commands: Commands, fonts: Res<FontBook>| {
                    views::stats::spawn(&mut commands, &fonts);
                },
            )
            .add_systems(
                OnEnter(AppState::Settings),
                |mut commands: Commands,
                 fonts: Res<FontBook>,
                 stores: Res<crate::persist::Stores>| {
                    views::settings::spawn(&mut commands, &fonts, &stores);
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
    let reserve = theme::ROOT_REM * (theme::NET_VIEW_RESERVE_REM + theme::DYNAMIC_BAND_REM);
    NetGeometry::for_window(
        config.window.width as f32,
        (config.window.height as f32 - reserve).max(theme::ROOT_REM),
    )
}

pub fn drill_net_geometry(config: &AppConfig) -> NetGeometry {
    let frame = 2.0 * theme::ROOT_REM * theme::PANEL_PADDING_REM;
    let width = (config.window.width as f32 - frame) * theme::DRILL_NET_WIDTH_PCT;
    let chrome = theme::ROOT_REM * (theme::HEADER_HEIGHT_REM + 2.0 * theme::PANEL_PADDING_REM);
    NetGeometry::for_region(
        width,
        (config.window.height as f32 - chrome).max(theme::ROOT_REM),
    )
}

fn content_node() -> Node {
    Node {
        width: Val::Percent(100.0),
        flex_grow: 1.0,
        flex_direction: FlexDirection::Column,
        ..default()
    }
}

pub fn spawn_view_frame(
    commands: &mut Commands,
    fonts: &FontBook,
    state: AppState,
    title: &str,
    context: &str,
) -> Entity {
    let root = commands
        .spawn((
            DespawnOnExit(state),
            Node {
                width: Val::Percent(100.0),
                height: Val::Percent(100.0),
                flex_direction: FlexDirection::Column,
                padding: UiRect::all(theme::rem(theme::PANEL_PADDING_REM)),
                ..default()
            },
            BackgroundColor(theme::background()),
        ))
        .id();
    commands.entity(root).with_children(|parent| {
        parent
            .spawn((
                Node {
                    width: Val::Percent(100.0),
                    height: theme::rem(theme::HEADER_HEIGHT_REM),
                    flex_direction: FlexDirection::Row,
                    align_items: AlignItems::Center,
                    justify_content: JustifyContent::SpaceBetween,
                    column_gap: theme::rem(theme::SPACING_REM),
                    border: UiRect::bottom(theme::rem(theme::HAIRLINE_REM)),
                    ..default()
                },
                BorderColor::all(theme::hairline()),
            ))
            .with_children(|header| {
                header.spawn((
                    Text::new(title),
                    TextFont {
                        font: FontSource::Handle(fonts.bold.clone()),
                        font_size: FontSize::Rem(theme::FONT_TITLE),
                        ..default()
                    },
                    TextColor(theme::text_primary()),
                ));
                header.spawn((
                    Text::new(context),
                    TextFont {
                        font: FontSource::Handle(fonts.regular.clone()),
                        font_size: FontSize::Rem(theme::FONT_SMALL),
                        ..default()
                    },
                    TextColor(theme::text_muted()),
                ));
            });
    });
    commands.spawn((content_node(), ChildOf(root))).id()
}

pub fn spawn_text_view(
    commands: &mut Commands,
    fonts: &FontBook,
    state: AppState,
    title: &str,
    lines: &[&str],
    geometry: Option<NetGeometry>,
) -> Entity {
    let content = spawn_view_frame(commands, fonts, state, title, "");
    commands.entity(content).insert(Node {
        align_items: AlignItems::Center,
        justify_content: JustifyContent::Center,
        row_gap: theme::rem(theme::SPACING_REM),
        ..content_node()
    });
    commands.entity(content).with_children(|parent| {
        for line in lines {
            spawn_hint(parent, fonts, line);
        }
        if let Some(geometry) = geometry {
            net::spawn_net(parent, geometry, fonts.bold.clone());
        }
    });
    content
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

pub fn spawn_line<M: Component>(
    parent: &mut ChildSpawnerCommands,
    fonts: &FontBook,
    text: &str,
    size_rem: f32,
    color: Color,
    marker: M,
) -> Entity {
    parent
        .spawn((
            marker,
            Text::new(text),
            TextFont {
                font: FontSource::Handle(fonts.regular.clone()),
                font_size: FontSize::Rem(size_rem),
                ..default()
            },
            TextColor(color),
        ))
        .id()
}

pub fn spawn_dynamic<M: Component>(
    parent: &mut ChildSpawnerCommands,
    fonts: &FontBook,
    text: &str,
    marker: M,
) {
    spawn_line(
        parent,
        fonts,
        text,
        theme::FONT_BODY,
        theme::text_primary(),
        marker,
    );
}

pub fn spawn_spans(
    parent: &mut ChildSpawnerCommands,
    fonts: &FontBook,
    spans: &[(&str, f32, Color)],
) -> Entity {
    let mut root = parent.spawn((
        Text::new(""),
        TextFont {
            font: FontSource::Handle(fonts.regular.clone()),
            font_size: FontSize::Rem(theme::FONT_BODY),
            ..default()
        },
        TextColor(theme::text_primary()),
    ));
    root.with_children(|text| {
        for (chunk, size, color) in spans {
            text.spawn((
                TextSpan::new(*chunk),
                TextFont {
                    font: FontSource::Handle(fonts.regular.clone()),
                    font_size: FontSize::Rem(*size),
                    ..default()
                },
                TextColor(*color),
            ));
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
    spawn_text_view(&mut commands, &fonts, AppState::Fatal, "fatal", &refs, None);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Component)]
    struct FocalLine;

    #[test]
    fn net_leaves_room_for_header_and_dynamic_text() {
        let config = AppConfig::default();
        let geo = net_geometry(&config);
        let header = theme::ROOT_REM * theme::NET_VIEW_RESERVE_REM;
        let band = theme::ROOT_REM * theme::DYNAMIC_BAND_REM;
        let net = geo.net_height();
        assert!(
            header + net + band <= config.window.height as f32,
            "header {header} + net {net} + band {band} exceeds {}",
            config.window.height
        );
        assert!(geo.quad >= 16.0);
    }

    #[test]
    fn dynamic_band_covers_eight_lines_plus_the_input_box() {
        let line = theme::ROOT_REM * theme::FONT_BODY * 1.5;
        let box_rows = theme::ROOT_REM * 3.0;
        let spacing = theme::ROOT_REM * theme::SPACING_REM * 3.0;
        assert!(
            theme::ROOT_REM * theme::DYNAMIC_BAND_REM >= line * 8.0 + box_rows + spacing,
            "band too small for eight body lines and the input box"
        );
    }

    #[test]
    fn drill_net_fills_the_left_region() {
        let config = AppConfig::default();
        let geo = drill_net_geometry(&config);
        assert_eq!(geo.quad, 37.0);
        let frame = 2.0 * theme::ROOT_REM * theme::PANEL_PADDING_REM;
        let width = (config.window.width as f32 - frame) * theme::DRILL_NET_WIDTH_PCT;
        let chrome = theme::ROOT_REM * (theme::HEADER_HEIGHT_REM + 2.0 * theme::PANEL_PADDING_REM);
        let height = config.window.height as f32 - chrome;
        assert!(geo.net_width() <= width);
        assert!(geo.net_height() <= height);
    }

    #[test]
    fn view_frame_builds_header_and_content() {
        let mut world = World::new();
        let fonts = FontBook {
            regular: Handle::default(),
            bold: Handle::default(),
        };
        let content = {
            let mut commands = world.commands();
            spawn_view_frame(&mut commands, &fonts, AppState::Menu, "menu", "3 due")
        };
        world.flush();
        let root = world
            .get::<ChildOf>(content)
            .expect("content parented to the frame")
            .0;
        let root_node = world.get::<Node>(root).expect("root node");
        assert_eq!(root_node.height, Val::Percent(100.0));
        let root_children = world.get::<Children>(root).expect("root children");
        assert_eq!(root_children.len(), 2);
        assert_eq!(root_children[1], content);
        let header = root_children[0];
        let header_node = world.get::<Node>(header).expect("header node");
        assert_eq!(header_node.height, theme::rem(theme::HEADER_HEIGHT_REM));
        assert_eq!(header_node.border.bottom, theme::rem(theme::HAIRLINE_REM));
        assert!(world.get::<BorderColor>(header).is_some());
        let header_children = world.get::<Children>(header).expect("header children");
        assert_eq!(header_children.len(), 2);
        assert!(world.get::<Text>(header_children[0]).is_some());
        assert!(world.get::<Text>(header_children[1]).is_some());
        let node = world.get::<Node>(content).expect("content node");
        assert_eq!(node.flex_grow, 1.0);
    }

    #[test]
    fn text_view_centers_its_content_region() {
        let mut world = World::new();
        let fonts = FontBook {
            regular: Handle::default(),
            bold: Handle::default(),
        };
        let content = {
            let mut commands = world.commands();
            spawn_text_view(
                &mut commands,
                &fonts,
                AppState::Menu,
                "menu",
                &["hint"],
                None,
            )
        };
        world.flush();
        let node = world.get::<Node>(content).expect("content node");
        assert_eq!(node.justify_content, JustifyContent::Center);
        assert_eq!(node.align_items, AlignItems::Center);
        assert_eq!(
            world.get::<Children>(content).expect("hint spawned").len(),
            1
        );
    }

    #[test]
    fn spans_build_one_root_with_styled_children() {
        let mut world = World::new();
        let fonts = FontBook {
            regular: Handle::default(),
            bold: Handle::default(),
        };
        let holder = {
            let mut commands = world.commands();
            let holder = commands.spawn(()).id();
            commands.entity(holder).with_children(|parent| {
                spawn_line(
                    parent,
                    &fonts,
                    "pair",
                    theme::FONT_FOCAL,
                    theme::ok(),
                    FocalLine,
                );
                spawn_spans(
                    parent,
                    &fonts,
                    &[
                        ("R2 U' ", theme::FONT_ALG, theme::setup()),
                        ("[S,R2]", theme::FONT_ALG, theme::teal()),
                    ],
                );
            });
            holder
        };
        world.flush();
        let children = world.get::<Children>(holder).expect("holder children");
        assert_eq!(children.len(), 2);
        let line = children[0];
        assert!(world.get::<FocalLine>(line).is_some());
        let line_font = world.get::<TextFont>(line).expect("line font");
        assert!(
            matches!(line_font.font_size, FontSize::Rem(size) if size == theme::FONT_FOCAL),
            "line scale applied"
        );
        assert_eq!(
            world.get::<TextColor>(line).expect("line color").0,
            theme::ok()
        );
        let root = children[1];
        assert!(world.get::<Text>(root).is_some());
        let span_children = world.get::<Children>(root).expect("span children");
        assert_eq!(span_children.len(), 2);
        let expected = [theme::setup(), theme::teal()];
        for (index, color) in expected.into_iter().enumerate() {
            let span = span_children[index];
            assert!(world.get::<TextSpan>(span).is_some());
            let font = world.get::<TextFont>(span).expect("span font");
            assert!(
                matches!(font.font_size, FontSize::Rem(size) if size == theme::FONT_ALG),
                "span scale applied"
            );
            assert_eq!(world.get::<TextColor>(span).expect("span color").0, color);
        }
    }
}
