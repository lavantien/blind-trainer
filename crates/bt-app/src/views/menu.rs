use crate::states::AppState;
use bevy::prelude::*;

pub fn menu_lines(warnings: &[String]) -> Vec<String> {
    let mut lines: Vec<String> = AppState::MENU_KEYS
        .iter()
        .enumerate()
        .map(|(index, (_, state))| format!("{} {}", index + 1, state.label()))
        .collect();
    lines.push("esc back to menu".to_string());
    lines.extend(warnings.iter().map(|warning| format!("! {warning}")));
    lines
}

pub fn spawn(
    commands: &mut Commands,
    fonts: &crate::present::FontBook,
    warnings: &[String],
) -> Entity {
    let lines = menu_lines(warnings);
    let refs: Vec<&str> = lines.iter().map(String::as_str).collect();
    crate::present::spawn_text_view(commands, fonts, AppState::Menu, "menu", &refs, None)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn menu_lines_list_every_key_hint() {
        let lines = menu_lines(&[]);
        assert_eq!(lines.len(), 7);
        assert_eq!(lines[0], "1 drill");
        assert_eq!(lines[1], "2 memo training");
        assert_eq!(lines[2], "3 solve");
        assert_eq!(lines[3], "4 algs");
        assert_eq!(lines[4], "5 stats");
        assert_eq!(lines[5], "6 settings");
        assert_eq!(lines[6], "esc back to menu");
    }

    #[test]
    fn menu_lines_append_warnings() {
        let lines = menu_lines(&["vendor data missing".to_string()]);
        assert_eq!(lines.len(), 8);
        assert_eq!(lines[7], "! vendor data missing");
    }
}
