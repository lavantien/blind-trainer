use bevy::asset::AssetPlugin;
use bevy::prelude::*;
use bevy::window::WindowResolution;
use bt_app::logic::{AppConfig, AppLogicPlugin, RuntimePaths, app_config_from, load_config_value};
use bt_app::present::PresentationPlugin;
use bt_app::smoke::SmokePlugin;
use std::path::PathBuf;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let smoke_dir = parse_smoke(&args);
    let paths = RuntimePaths::from_env();
    let asset_root = paths.assets_root.clone();
    let window = load_config_value(&paths.config_path)
        .ok()
        .and_then(|value| app_config_from(&value).ok())
        .map(|config| config.window)
        .unwrap_or_else(|| AppConfig::default().window);
    let mut app = App::new();
    app.add_plugins(
        DefaultPlugins
            .set(AssetPlugin {
                file_path: asset_root.to_string_lossy().to_string(),
                ..default()
            })
            .set(WindowPlugin {
                primary_window: Some(Window {
                    resolution: WindowResolution::new(window.width, window.height),
                    title: window.title.clone(),
                    ..default()
                }),
                ..default()
            }),
    )
    .add_plugins(AppLogicPlugin::with_paths(paths))
    .add_plugins(PresentationPlugin);
    if let Some(dir) = smoke_dir {
        app.add_plugins(SmokePlugin { dir });
    }
    app.run();
}

fn parse_smoke(args: &[String]) -> Option<PathBuf> {
    let index = args.iter().position(|arg| arg == "--smoke")?;
    args.get(index + 1).map(PathBuf::from)
}

#[cfg(test)]
mod tests {
    use super::parse_smoke;

    fn args(items: &[&str]) -> Vec<String> {
        items.iter().map(|item| item.to_string()).collect()
    }

    #[test]
    fn parse_smoke_reads_dir_after_flag() {
        let parsed = parse_smoke(&args(&["blind-trainer", "--smoke", "artifacts/e2e"]));
        assert_eq!(
            parsed.map(|path| path.to_string_lossy().to_string()),
            Some("artifacts/e2e".to_string())
        );
    }

    #[test]
    fn parse_smoke_rejects_missing_value_and_absent_flag() {
        assert!(parse_smoke(&args(&["blind-trainer", "--smoke"])).is_none());
        assert!(parse_smoke(&args(&["blind-trainer"])).is_none());
    }
}
