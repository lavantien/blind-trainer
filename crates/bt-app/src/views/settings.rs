use crate::logic::SettingsChanged;
use crate::persist::{MemoMode, Settings, Stores};
use crate::states::AppState;
use bevy::feathers::controls::{FeathersNumberInput, FeathersSelect, list_rows_from_strings};
use bevy::input::ButtonState;
use bevy::input::keyboard::KeyboardInput;
use bevy::prelude::*;

#[derive(Clone, Copy, PartialEq, Eq, Debug, Resource, Default)]
pub struct SettingsDraft {
    pub memo_cadence_ms: u32,
    pub memo_mode: MemoMode,
    pub drill_reps: u32,
}

impl SettingsDraft {
    pub fn from_settings(settings: &Settings) -> SettingsDraft {
        SettingsDraft {
            memo_cadence_ms: settings.memo_cadence_ms,
            memo_mode: settings.memo_mode,
            drill_reps: settings.drill_reps,
        }
    }

    pub fn to_settings(self) -> Settings {
        Settings {
            memo_cadence_ms: self.memo_cadence_ms,
            memo_mode: self.memo_mode,
            drill_reps: self.drill_reps,
        }
    }
}

pub fn clamp_cadence(value: i64) -> u32 {
    value.clamp(1, 10_000) as u32
}

pub fn clamp_reps(value: i64) -> u32 {
    value.clamp(1, 100) as u32
}

pub fn enter(
    stores: Res<Stores>,
    mut draft: ResMut<SettingsDraft>,
    mut messages: ResMut<Messages<KeyboardInput>>,
) {
    *draft = SettingsDraft::from_settings(&stores.settings);
    messages.clear();
}

pub fn apply_draft(draft: Res<SettingsDraft>, stores: Res<Stores>, mut commands: Commands) {
    if !draft.is_changed() {
        return;
    }
    let next = draft.to_settings();
    if next != stores.settings {
        commands.write_message(SettingsChanged(next));
    }
}

pub fn mode_keys(
    mut inputs: MessageReader<KeyboardInput>,
    focus: Option<Res<bevy::input_focus::InputFocus>>,
    editables: Query<&bevy::text::EditableText>,
    mut draft: ResMut<SettingsDraft>,
) {
    let typing = focus
        .as_deref()
        .and_then(|focus| focus.get())
        .is_some_and(|entity| editables.get(entity).is_ok());
    if typing {
        return;
    }
    for input in inputs.read() {
        if input.state != ButtonState::Pressed {
            continue;
        }
        let mode = match input.key_code {
            KeyCode::Digit1 => Some(MemoMode::Syllables),
            KeyCode::Digit2 => Some(MemoMode::Images),
            KeyCode::Digit3 => Some(MemoMode::PerOrbit),
            _ => None,
        };
        if let Some(mode) = mode {
            draft.memo_mode = mode;
            return;
        }
    }
}

pub fn lines(draft: &SettingsDraft) -> Vec<String> {
    vec![
        format!("memo cadence {} ms", draft.memo_cadence_ms),
        format!("memo mode {}", draft.memo_mode.label()),
        format!("drill reps {}", draft.drill_reps),
        "1 syllables  2 images  3 per-orbit  edits save immediately".to_string(),
    ]
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn draft_round_trips_settings() {
        let settings = Settings {
            memo_cadence_ms: 650,
            memo_mode: MemoMode::Images,
            drill_reps: 8,
        };
        let draft = SettingsDraft::from_settings(&settings);
        assert_eq!(draft.to_settings(), settings);
    }

    #[test]
    fn clamps_keep_values_usable() {
        assert_eq!(clamp_cadence(0), 1);
        assert_eq!(clamp_cadence(500), 500);
        assert_eq!(clamp_cadence(100_000), 10_000);
        assert_eq!(clamp_reps(-3), 1);
        assert_eq!(clamp_reps(999), 100);
    }

    #[test]
    fn mode_rows_cover_every_mode_in_label_order() {
        for (row, (label, mode)) in MemoMode::LABELS.iter().enumerate() {
            assert_eq!(mode.index(), row);
            assert_eq!(MemoMode::from_index(row), Some(*mode));
            assert_eq!(mode.label(), *label);
        }
        assert_eq!(MemoMode::from_index(MemoMode::LABELS.len()), None);
        assert_eq!(MemoMode::default(), MemoMode::PerOrbit);
    }

    #[test]
    fn hint_line_lists_every_mode() {
        let draft = SettingsDraft::default();
        let rendered = lines(&draft);
        let hint = rendered.last().expect("hint line");
        for (label, _) in MemoMode::LABELS {
            assert!(hint.contains(label), "hint missing {label}: {hint}");
        }
    }
}

#[derive(Component, Default)]
pub struct SettingsText;

#[derive(Component, Default)]
pub struct CadenceInput;

#[derive(Component, Default)]
pub struct RepsInput;

#[derive(Component, Default)]
pub struct ModeSelect;

pub fn on_cadence_change(
    change: On<bevy::ui_widgets::ValueChange<i64>>,
    mut draft: ResMut<SettingsDraft>,
) {
    draft.memo_cadence_ms = clamp_cadence(change.value);
}

pub fn on_reps_change(
    change: On<bevy::ui_widgets::ValueChange<i64>>,
    mut draft: ResMut<SettingsDraft>,
) {
    draft.drill_reps = clamp_reps(change.value);
}

pub fn on_mode_change(
    change: On<bevy::ui_widgets::ValueChange<Entity>>,
    rows: Query<&bevy::feathers::controls::OptionIndex>,
    mut draft: ResMut<SettingsDraft>,
) {
    if let Ok(index) = rows.get(change.value)
        && let Some(mode) = MemoMode::from_index(index.0)
    {
        draft.memo_mode = mode;
    }
}

pub fn spawn(commands: &mut Commands, fonts: &crate::present::FontBook, stores: &Stores) -> Entity {
    let draft = SettingsDraft::from_settings(&stores.settings);
    let selected = Some(draft.memo_mode.index());
    let entity = crate::present::spawn_text_view(
        commands,
        fonts,
        AppState::Settings,
        "settings",
        &["edits save immediately, esc menu"],
        None,
    );
    commands.entity(entity).with_children(|parent| {
        let body = lines(&draft).join("\n");
        crate::present::spawn_dynamic(parent, fonts, &body, SettingsText);
    });
    let cadence = commands
        .spawn_scene(bsn! {
            @FeathersNumberInput {
                @label_text: {Some("cadence ms")},
            }
            Node { width: px(160.0), flex_grow: 0.0, flex_shrink: 0.0 }
            on(on_cadence_change)
        })
        .insert(CadenceInput)
        .insert(ChildOf(entity))
        .id();
    commands
        .entity(cadence)
        .insert(bevy::feathers::controls::NumberInputValue::I64(
            draft.memo_cadence_ms as i64,
        ));
    let reps = commands
        .spawn_scene(bsn! {
            @FeathersNumberInput {
                @label_text: {Some("drill reps")},
            }
            Node { width: px(160.0), flex_grow: 0.0, flex_shrink: 0.0 }
            on(on_reps_change)
        })
        .insert(RepsInput)
        .insert(ChildOf(entity))
        .id();
    commands
        .entity(reps)
        .insert(bevy::feathers::controls::NumberInputValue::I64(
            draft.drill_reps as i64,
        ));
    commands
        .spawn_scene(bsn! {
            @FeathersSelect {
                @options: {list_rows_from_strings(MemoMode::LABELS.map(|(label, _)| label), selected)},
                @max_visible: 3,
            }
            on(on_mode_change)
        })
        .insert(ModeSelect)
        .insert(ChildOf(entity));
    entity
}

pub fn bind_text(draft: Res<SettingsDraft>, mut texts: Query<&mut Text, With<SettingsText>>) {
    if !draft.is_changed() {
        return;
    }
    let body = lines(&draft).join("\n");
    for mut text in &mut texts {
        *text = Text::new(body.clone());
    }
}
