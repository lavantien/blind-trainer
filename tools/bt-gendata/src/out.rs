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

    #[test]
    fn write_atomic_fails_and_cleans_up_when_the_target_is_a_directory() {
        let dir = std::env::temp_dir().join(format!("bt-gendata-out-dir-{}", std::process::id()));
        let target = dir.join("file.json");
        std::fs::create_dir_all(&target).unwrap();
        let err = write_atomic(&target, b"{}").unwrap_err();
        assert!(err.starts_with("write "), "{err}");
        let leftovers: Vec<_> = std::fs::read_dir(&dir).unwrap().collect();
        assert_eq!(leftovers.len(), 1);
        assert!(leftovers[0].as_ref().unwrap().path().is_dir());
        std::fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn write_atomic_fails_when_the_parent_exists_as_a_file() {
        let dir = std::env::temp_dir().join(format!("bt-gendata-out-file-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let blocker = dir.join("blocker");
        std::fs::write(&blocker, b"x").unwrap();
        let err = write_atomic(&blocker.join("child.json"), b"{}").unwrap_err();
        assert!(err.starts_with("write "), "{err}");
        assert!(!blocker.join("child.json").exists());
        std::fs::remove_dir_all(&dir).ok();
    }

    #[test]
    fn write_atomic_writes_into_an_existing_directory_without_temp_leftovers() {
        let dir = std::env::temp_dir().join(format!("bt-gendata-out-flat-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("flat.json");
        write_atomic(&path, b"payload").expect("flat write");
        assert_eq!(std::fs::read(&path).unwrap(), b"payload");
        let names: Vec<String> = std::fs::read_dir(&dir)
            .unwrap()
            .map(|entry| entry.unwrap().file_name().to_string_lossy().to_string())
            .collect();
        assert_eq!(names, vec!["flat.json".to_string()]);
        std::fs::remove_dir_all(&dir).ok();
    }
}
