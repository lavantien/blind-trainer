use bevy::asset::AssetPlugin;
use bevy::prelude::*;
use bevy::window::WindowResolution;
use bt_app::logic::{AppLogicPlugin, RuntimePaths, app_config_from, load_config_value};
use bt_app::present::PresentationPlugin;
use bt_app::smoke::SmokePlugin;
use bt_app::usage::UsagePlugin;
use std::path::PathBuf;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let shot_mode = match parse_shot_mode(&args) {
        Ok(mode) => mode,
        Err(err) => {
            eprintln!("{err}");
            std::process::exit(2);
        }
    };
    let paths = RuntimePaths::from_env();
    let asset_root = paths.assets_root.clone();
    let config = load_config_value(&paths.config_path)
        .ok()
        .and_then(|value| app_config_from(&value).ok())
        .unwrap_or_default();
    let window = config.window.clone();
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
    match shot_mode {
        Some(ShotMode::Smoke(dir)) => {
            app.add_plugins(SmokePlugin {
                dir,
                seed: config.smoke_seed,
            });
        }
        Some(ShotMode::Usage(dir)) => {
            app.add_plugins(UsagePlugin::tour(
                dir,
                config.smoke_seed,
                config.memo.cadence_ms,
            ));
        }
        Some(ShotMode::FatalShot(dir)) => {
            app.add_plugins(UsagePlugin::fatal_shot(dir));
        }
        None => {}
    }
    app.run();
}

enum ShotMode {
    Smoke(PathBuf),
    Usage(PathBuf),
    FatalShot(PathBuf),
}

fn flag_value(args: &[String], flag: &str) -> Option<Option<PathBuf>> {
    let index = args.iter().position(|arg| arg == flag)?;
    Some(args.get(index + 1).map(PathBuf::from))
}

fn parse_shot_mode(args: &[String]) -> Result<Option<ShotMode>, String> {
    if let Some(dir) = parse_smoke(args) {
        return Ok(Some(ShotMode::Smoke(dir)));
    }
    let usage = flag_value(args, "--usage");
    let fatal = flag_value(args, "--fatal-shot");
    if usage.is_some() && fatal.is_some() {
        return Err("--usage and --fatal-shot are mutually exclusive".to_string());
    }
    let missing = |flag: &str| format!("{flag} needs an output directory");
    Ok(match (usage, fatal) {
        (Some(Some(dir)), _) => Some(ShotMode::Usage(dir)),
        (Some(None), _) => return Err(missing("--usage")),
        (None, Some(Some(dir))) => Some(ShotMode::FatalShot(dir)),
        (None, Some(None)) => return Err(missing("--fatal-shot")),
        (None, None) => None,
    })
}

fn parse_smoke(args: &[String]) -> Option<PathBuf> {
    let index = args.iter().position(|arg| arg == "--smoke")?;
    args.get(index + 1).map(PathBuf::from)
}

#[cfg(test)]
mod tests {
    use super::parse_shot_mode;

    fn args(items: &[&str]) -> Vec<String> {
        items.iter().map(|item| item.to_string()).collect()
    }

    #[test]
    fn parse_smoke_reads_dir_after_flag() {
        let parsed = super::parse_smoke(&args(&["blind-trainer", "--smoke", "artifacts/e2e"]));
        assert_eq!(
            parsed.map(|path| path.to_string_lossy().to_string()),
            Some("artifacts/e2e".to_string())
        );
    }

    #[test]
    fn parse_smoke_rejects_missing_value_and_absent_flag() {
        assert!(super::parse_smoke(&args(&["blind-trainer", "--smoke"])).is_none());
        assert!(super::parse_smoke(&args(&["blind-trainer"])).is_none());
    }

    #[test]
    fn parse_shot_mode_routes_each_flag() {
        let usage =
            parse_shot_mode(&args(&["bt", "--usage", "docs/screenshots"])).expect("usage parses");
        assert!(matches!(usage, Some(super::ShotMode::Usage(_))));
        let fatal = parse_shot_mode(&args(&["bt", "--fatal-shot", "docs/screenshots"]))
            .expect("fatal parses");
        assert!(matches!(fatal, Some(super::ShotMode::FatalShot(_))));
        let smoke = parse_shot_mode(&args(&["bt", "--smoke", "artifacts/e2e"])).expect("smoke");
        assert!(matches!(smoke, Some(super::ShotMode::Smoke(_))));
        assert!(parse_shot_mode(&args(&["bt"])).expect("none").is_none());
    }

    #[test]
    fn parse_shot_mode_rejects_missing_values_and_double_modes() {
        assert!(parse_shot_mode(&args(&["bt", "--usage"])).is_err());
        assert!(parse_shot_mode(&args(&["bt", "--fatal-shot"])).is_err());
        assert!(parse_shot_mode(&args(&["bt", "--usage", "a", "--fatal-shot", "b"])).is_err());
    }
}
