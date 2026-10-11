use std::path::Path;

pub fn write_atomic(path: &Path, bytes: &[u8]) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent).map_err(|e| format!("write {}: {e}", parent.display()))?;
    }
    let name = path
        .file_name()
        .map(|n| n.to_string_lossy().to_string())
        .unwrap_or_else(|| "data".to_string());
    let temp = path.with_file_name(format!("{name}.tmp-{}", std::process::id()));
    std::fs::write(&temp, bytes).map_err(|e| format!("write {}: {e}", temp.display()))?;
    match std::fs::rename(&temp, path) {
        Ok(()) => Ok(()),
        Err(e) => {
            std::fs::remove_file(&temp).ok();
            Err(format!("write {}: {e}", path.display()))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn write_atomic_creates_parents_and_overwrites() {
        let dir = std::env::temp_dir().join(format!("bt-gendata-out-{}", std::process::id()));
        let path = dir.join("nested").join("file.json");
        write_atomic(&path, b"{\"a\":1}").expect("first write");
        write_atomic(&path, b"{\"a\":2}").expect("second write");
        assert_eq!(std::fs::read(&path).unwrap(), b"{\"a\":2}");
        let entries = std::fs::read_dir(dir.join("nested")).unwrap().count();
        assert_eq!(entries, 1);
        std::fs::remove_dir_all(&dir).ok();
    }
}
