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
    match app.run() {
        AppExit::Success => {}
        AppExit::Error(code) => std::process::exit(i32::from(code.get())),
    }
}

enum ShotMode {
    Smoke(PathBuf),
    Usage(PathBuf),
    FatalShot(PathBuf),
}

const SHOT_FLAGS: [&str; 3] = ["--smoke", "--usage", "--fatal-shot"];

fn flag_value<'a>(args: &'a [String], flag: &str) -> Option<&'a str> {
    let index = args.iter().position(|arg| arg == flag)?;
    args.get(index + 1).map(String::as_str)
}

fn parse_shot_mode(args: &[String]) -> Result<Option<ShotMode>, String> {
    let present: Vec<&str> = SHOT_FLAGS
        .into_iter()
        .filter(|flag| args.iter().any(|arg| arg == flag))
        .collect();
    let Some(&flag) = present.first() else {
        return Ok(None);
    };
    if present.len() > 1 {
        return Err("only one of --smoke, --usage, --fatal-shot may be set".to_string());
    }
    let dir = match flag_value(args, flag) {
        Some(value) if value.starts_with("--") => {
            return Err(format!("{flag} needs an output directory, got {value}"));
        }
        Some(value) => PathBuf::from(value),
        None => return Err(format!("{flag} needs an output directory")),
    };
    Ok(Some(match flag {
        "--smoke" => ShotMode::Smoke(dir),
        "--usage" => ShotMode::Usage(dir),
        _ => ShotMode::FatalShot(dir),
    }))
}

#[cfg(test)]
mod tests {
    use super::parse_shot_mode;

    fn args(items: &[&str]) -> Vec<String> {
        items.iter().map(|item| item.to_string()).collect()
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
    fn parse_shot_mode_rejects_missing_values() {
        assert!(parse_shot_mode(&args(&["bt", "--usage"])).is_err());
        assert!(parse_shot_mode(&args(&["bt", "--fatal-shot"])).is_err());
        assert!(parse_shot_mode(&args(&["bt", "--smoke"])).is_err());
    }

    #[test]
    fn parse_shot_mode_rejects_flag_combinations_and_flag_like_values() {
        for combo in [
            vec!["bt", "--usage", "a", "--fatal-shot", "b"],
            vec!["bt", "--smoke", "a", "--usage", "b"],
            vec!["bt", "--smoke", "--usage", "docs/x"],
            vec!["bt", "--usage", "--fatal-shot"],
        ] {
            assert!(parse_shot_mode(&args(&combo)).is_err(), "args {combo:?}");
        }
    }
}
