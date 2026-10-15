pub mod comments;
pub mod constants;
pub mod lexer;

use std::path::{Path, PathBuf};

pub const SCAN_PATTERNS: [&str; 4] = [
    "crates/**/*.rs",
    "tools/**/*.rs",
    "fuzz/fuzz/**/*.rs",
    "tests/e2e/src/**/*.rs",
];

pub const SKIP_DIRS: [&str; 1] = ["target"];

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct Finding {
    pub path: String,
    pub line: usize,
    pub kind: &'static str,
    pub detail: String,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum CheckMode {
    Comments,
    Constants,
    Both,
}

#[derive(Clone, PartialEq, Eq, Debug, Default)]
pub struct CheckOutcome {
    pub findings: Vec<Finding>,
    pub stale_allowlist: Vec<String>,
}

#[derive(Clone, PartialEq, Eq, Debug)]
pub struct AllowEntry {
    pub path: String,
    pub core: String,
    pub justification: String,
}

#[derive(Clone, PartialEq, Eq, Debug, Default)]
pub struct Allowlist {
    pub entries: Vec<AllowEntry>,
}

pub fn workspace_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .and_then(|path| path.parent())
        .expect("workspace root from manifest dir")
        .to_path_buf()
}

pub fn config_path_from(env: Option<&str>) -> PathBuf {
    match env.filter(|value| !value.is_empty()) {
        Some(raw) => {
            let path = PathBuf::from(raw);
            if path.is_absolute() {
                path
            } else {
                workspace_root().join(path)
            }
        }
        None => workspace_root().join("config").join("config.json"),
    }
}

pub fn load_config() -> Result<serde_json::Value, String> {
    let path = config_path_from(std::env::var("BT_CONFIG").ok().as_deref());
    let text = std::fs::read_to_string(&path)
        .map_err(|source| format!("read {}: {source}", path.display()))?;
    serde_json::from_str(&text).map_err(|source| format!("parse {}: {source}", path.display()))
}

pub fn parse_allowlist(text: &str, source: &Path) -> Result<Allowlist, String> {
    let mut entries = Vec::new();
    for (index, raw) in text.lines().enumerate() {
        let line = raw.trim_end_matches('\r');
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let fields: Vec<&str> = line.split('\t').collect();
        if fields.len() < 3 {
            return Err(format!(
                "allowlist {} line {} needs path TAB core TAB justification",
                source.display(),
                index + 1
            ));
        }
        entries.push(AllowEntry {
            path: fields[0].to_string(),
            core: fields[1].to_string(),
            justification: fields[2..].join("\t"),
        });
    }
    Ok(Allowlist { entries })
}

impl Allowlist {
    pub fn covers(&self, path: &str, core: &str) -> bool {
        self.entries
            .iter()
            .any(|entry| entry.path == path && entry.core == core)
    }
}

pub fn collect_sources(root: &Path) -> Vec<PathBuf> {
    let mut files = Vec::new();
    for pattern in SCAN_PATTERNS {
        let segs: Vec<&str> = pattern.split('/').collect();
        walk(root, &segs, &mut files);
    }
    files.sort();
    files.dedup();
    files
}

pub fn run_checks(
    root: &Path,
    mode: CheckMode,
    config: &serde_json::Value,
    allowlist_path: &Path,
) -> Result<CheckOutcome, String> {
    let files = collect_sources(root);
    if files.is_empty() {
        return Err(format!("no rust sources under {}", root.display()));
    }
    let mut outcome = CheckOutcome::default();
    if matches!(mode, CheckMode::Comments | CheckMode::Both) {
        for path in &files {
            let text = read_source(path)?;
            let rel = rel_path(path, root);
            outcome.findings.extend(comments::findings(&text, &rel));
        }
    }
    if matches!(mode, CheckMode::Constants | CheckMode::Both) {
        let policy = constants::policy_from_config(config)?;
        let allow_text = std::fs::read_to_string(allowlist_path)
            .map_err(|source| format!("read {}: {source}", allowlist_path.display()))?;
        let allowlist = parse_allowlist(&allow_text, allowlist_path)?;
        let mut covered_pairs: Vec<(String, String)> = Vec::new();
        for path in &files {
            let text = read_source(path)?;
            let masked = lexer::mask_source(&text);
            let rel = rel_path(path, root);
            for hit in constants::recurring_literals(&masked.text, policy.literal_recurrence) {
                if policy
                    .small_literal_allow
                    .iter()
                    .any(|allowed| allowed == &hit.core)
                {
                    continue;
                }
                covered_pairs.push((rel.clone(), hit.core.clone()));
                if !allowlist.covers(&rel, &hit.core) {
                    outcome.findings.push(Finding {
                        path: rel.clone(),
                        line: hit.line,
                        kind: "constant",
                        detail: format!("literal {} x{}", hit.core, hit.count),
                    });
                }
            }
        }
        for entry in &allowlist.entries {
            if !covered_pairs
                .iter()
                .any(|(path, core)| path == &entry.path && core == &entry.core)
            {
                outcome
                    .stale_allowlist
                    .push(format!("{} TAB {}", entry.path, entry.core));
            }
        }
    }
    Ok(outcome)
}

pub fn selftest(workdir: &Path) -> Result<bool, String> {
    let config_path = workdir.join("config.json");
    let allow_path = workdir.join("allow.txt");
    write_file(&workdir.join("crates").join("dirty.rs"), DIRTY_FIXTURE)?;
    write_file(&workdir.join("crates").join("clean.rs"), CLEAN_FIXTURE)?;
    write_file(
        &workdir.join("clean_only").join("crates").join("clean.rs"),
        CLEAN_FIXTURE,
    )?;
    write_file(&config_path, SELFTEST_CONFIG)?;
    write_file(&allow_path, "# selftest allowlist\n")?;
    let config: serde_json::Value = serde_json::from_str(SELFTEST_CONFIG)
        .map_err(|source| format!("parse {}: {source}", config_path.display()))?;
    let dirty_run = run_checks(workdir, CheckMode::Both, &config, &allow_path)?;
    let dirty_findings: Vec<&Finding> = dirty_run
        .findings
        .iter()
        .filter(|finding| finding.path == "crates/dirty.rs")
        .collect();
    let line_hits = dirty_findings
        .iter()
        .filter(|finding| finding.detail == "line comment")
        .count();
    let block_hits = dirty_findings
        .iter()
        .filter(|finding| finding.detail == "block comment")
        .count();
    let literal_hits = dirty_findings
        .iter()
        .filter(|finding| finding.detail == "literal 7 x2")
        .count();
    let clean_silent = dirty_run
        .findings
        .iter()
        .all(|finding| finding.path == "crates/dirty.rs");
    let clean_run = run_checks(
        &workdir.join("clean_only"),
        CheckMode::Both,
        &config,
        &allow_path,
    )?;
    Ok(line_hits == 1
        && block_hits == 1
        && literal_hits == 1
        && clean_silent
        && dirty_run.stale_allowlist.is_empty()
        && clean_run.findings.is_empty()
        && clean_run.stale_allowlist.is_empty())
}

const DIRTY_FIXTURE: &str = "fn dirty() -> u32 {\n    let host = \"https://example.org/a//b\";\n    let scale = 7;\n    let shift = 7;\n    scale + shift\n}\n\n// selftest line marker\n/* nested /* marker */ tail */\n";

const CLEAN_FIXTURE: &str =
    "fn clean() -> u32 {\n    let one = 1;\n    let two = 2;\n    one + two\n}\n";

const SELFTEST_CONFIG: &str = "{\"policy\":{\"literal_recurrence\":2,\"small_literal_allow\":[]}}";

fn write_file(path: &Path, text: &str) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)
            .map_err(|source| format!("create {}: {source}", parent.display()))?;
    }
    std::fs::write(path, text).map_err(|source| format!("write {}: {source}", path.display()))
}

fn read_source(path: &Path) -> Result<String, String> {
    std::fs::read_to_string(path).map_err(|source| format!("read {}: {source}", path.display()))
}

fn glob_match(pattern: &[char], name: &[char]) -> bool {
    match pattern.split_first() {
        None => name.is_empty(),
        Some((first, rest)) if *first == '*' => {
            glob_match(rest, name) || (!name.is_empty() && glob_match(pattern, &name[1..]))
        }
        Some((first, rest)) => {
            !name.is_empty() && *first == name[0] && glob_match(rest, &name[1..])
        }
    }
}

fn walk(dir: &Path, segs: &[&str], out: &mut Vec<PathBuf>) {
    let Some((first, rest)) = segs.split_first() else {
        if dir.is_file() {
            out.push(dir.to_path_buf());
        }
        return;
    };
    if *first == "**" {
        walk(dir, rest, out);
        let Ok(entries) = std::fs::read_dir(dir) else {
            return;
        };
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() && !SKIP_DIRS.contains(&entry.file_name().to_string_lossy().as_ref()) {
                walk(&path, segs, out);
            }
        }
    } else {
        let Ok(entries) = std::fs::read_dir(dir) else {
            return;
        };
        for entry in entries.flatten() {
            let path = entry.path();
            let name = entry.file_name().to_string_lossy().to_string();
            let name_chars: Vec<char> = name.chars().collect();
            let pattern_chars: Vec<char> = first.chars().collect();
            if glob_match(&pattern_chars, &name_chars) {
                walk(&path, rest, out);
            }
        }
    }
}

fn rel_path(path: &Path, root: &Path) -> String {
    path.strip_prefix(root)
        .map(|rel| {
            rel.components()
                .map(|part| part.as_os_str().to_string_lossy())
                .collect::<Vec<_>>()
                .join("/")
        })
        .unwrap_or_else(|_| path.to_string_lossy().to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU64, Ordering};

    static TEMP_COUNTER: AtomicU64 = AtomicU64::new(0);

    fn temp_root(tag: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!(
            "bt-policy-{}-{}-{}",
            tag,
            std::process::id(),
            TEMP_COUNTER.fetch_add(1, Ordering::Relaxed)
        ));
        std::fs::create_dir_all(&dir).expect("temp root");
        dir
    }

    fn write(path: &Path, text: &str) {
        std::fs::create_dir_all(path.parent().expect("parent")).expect("parent dir");
        std::fs::write(path, text).expect("write");
    }

    const DIRTY: &str = DIRTY_FIXTURE;

    const CLEAN: &str = CLEAN_FIXTURE;

    const CONFIG: &str = SELFTEST_CONFIG;

    #[test]
    fn workspace_root_is_the_repo_root() {
        let root = workspace_root();
        assert!(root.join("config").join("config.json").is_file());
        assert!(root.join("makefile").is_file());
    }

    #[test]
    fn config_path_honors_env_and_default() {
        assert_eq!(
            config_path_from(None),
            workspace_root().join("config").join("config.json")
        );
        assert_eq!(
            config_path_from(Some("other/conf.json")),
            workspace_root().join("other").join("conf.json")
        );
        let absolute = std::env::temp_dir().join("x.json");
        assert_eq!(config_path_from(Some(absolute.to_str().unwrap())), absolute);
    }

    #[test]
    fn glob_match_handles_star_names() {
        let pat: Vec<char> = "*.rs".chars().collect();
        assert!(glob_match(&pat, &"main.rs".chars().collect::<Vec<_>>()));
        assert!(!glob_match(&pat, &"main.txt".chars().collect::<Vec<_>>()));
        assert!(!glob_match(&pat, &Vec::new()));
    }

    #[test]
    fn collect_sources_covers_every_scan_root() {
        let files = collect_sources(&workspace_root());
        assert!(
            files.contains(
                &workspace_root()
                    .join("crates")
                    .join("bt-app")
                    .join("src")
                    .join("views")
                    .join("images.rs")
            )
        );
        assert!(
            files.contains(
                &workspace_root()
                    .join("tools")
                    .join("bt-gendata")
                    .join("src")
                    .join("model.rs")
            )
        );
        assert!(
            files
                .iter()
                .any(|path| path.starts_with(workspace_root().join("fuzz").join("fuzz")))
        );
        assert!(
            files
                .iter()
                .any(|path| path.starts_with(workspace_root().join("tests").join("e2e")))
        );
    }

    #[test]
    fn real_repo_sources_have_no_comment_hits() {
        let outcome = run_checks(
            &workspace_root(),
            CheckMode::Comments,
            &serde_json::Value::Null,
            Path::new("unused-in-comments-mode"),
        )
        .expect("comments pass over the repo");
        assert!(
            outcome.findings.is_empty(),
            "unexpected comment hits: {:?}",
            outcome.findings
        );
    }

    #[test]
    fn parse_allowlist_skips_headers_and_reads_fields() {
        let text = "# header\n\nsrc/a.rs\t42\tlegacy table\nsrc/b.rs\t7\twhy\tdetail continues\n";
        let allow = parse_allowlist(text, Path::new("allow.txt")).expect("parses");
        assert_eq!(allow.entries.len(), 2);
        assert_eq!(
            allow.entries[0],
            AllowEntry {
                path: "src/a.rs".to_string(),
                core: "42".to_string(),
                justification: "legacy table".to_string(),
            }
        );
        assert_eq!(allow.entries[1].justification, "why\tdetail continues");
        assert!(allow.covers("src/a.rs", "42"));
        assert!(!allow.covers("src/a.rs", "7"));
        assert!(!allow.covers("src/b.rs", "42"));
    }

    #[test]
    fn parse_allowlist_rejects_short_lines_naming_the_line() {
        let err = parse_allowlist("src/a.rs\t42\n", Path::new("allow.txt")).unwrap_err();
        assert_eq!(
            err,
            "allowlist allow.txt line 1 needs path TAB core TAB justification"
        );
    }

    #[test]
    fn run_checks_reports_relative_paths_and_both_kinds() {
        let root = temp_root("run-basic");
        write(&root.join("crates").join("dirty.rs"), DIRTY);
        write(&root.join("crates").join("clean.rs"), CLEAN);
        write(&root.join("allow.txt"), "# allow\n");
        let config: serde_json::Value = serde_json::from_str(CONFIG).expect("config json");
        let outcome = run_checks(&root, CheckMode::Both, &config, &root.join("allow.txt"))
            .expect("checks run");
        assert_eq!(outcome.stale_allowlist, Vec::<String>::new());
        assert_eq!(outcome.findings.len(), 3);
        assert_eq!(outcome.findings[0].path, "crates/dirty.rs");
        assert_eq!(outcome.findings[0].detail, "line comment");
        assert_eq!(outcome.findings[1].detail, "block comment");
        assert_eq!(outcome.findings[2].detail, "literal 7 x2");
        assert_eq!(outcome.findings[2].kind, "constant");
        assert!(
            outcome
                .findings
                .iter()
                .all(|finding| finding.path == "crates/dirty.rs"),
            "clean.rs must stay silent"
        );
    }

    #[test]
    fn allowlist_covers_recurring_pairs_and_flags_stale_entries() {
        let root = temp_root("run-allow");
        write(&root.join("crates").join("dirty.rs"), DIRTY);
        write(
            &root.join("allow.txt"),
            "crates/dirty.rs\t7\tlegacy\nsrc/gone.rs\t9\tnone\n",
        );
        let config: serde_json::Value = serde_json::from_str(CONFIG).expect("config json");
        let outcome = run_checks(
            &root,
            CheckMode::Constants,
            &config,
            &root.join("allow.txt"),
        )
        .expect("checks run");
        assert!(outcome.findings.is_empty(), "{:?}", outcome.findings);
        assert_eq!(
            outcome.stale_allowlist,
            vec!["src/gone.rs TAB 9".to_string()]
        );
    }

    #[test]
    fn small_literal_allow_silences_a_core_everywhere() {
        let root = temp_root("run-small");
        write(&root.join("crates").join("dirty.rs"), DIRTY);
        write(&root.join("allow.txt"), "# allow\n");
        let config: serde_json::Value = serde_json::from_str(
            "{\"policy\":{\"literal_recurrence\":2,\"small_literal_allow\":[\"7\"]}}",
        )
        .expect("config json");
        let outcome = run_checks(
            &root,
            CheckMode::Constants,
            &config,
            &root.join("allow.txt"),
        )
        .expect("checks run");
        assert!(outcome.findings.is_empty(), "{:?}", outcome.findings);
    }

    #[test]
    fn missing_allowlist_file_is_a_harness_error() {
        let root = temp_root("run-miss");
        write(&root.join("crates").join("clean.rs"), CLEAN);
        let config: serde_json::Value = serde_json::from_str(CONFIG).expect("config json");
        let err = run_checks(
            &root,
            CheckMode::Constants,
            &config,
            &root.join("absent.txt"),
        )
        .unwrap_err();
        assert!(err.starts_with("read "), "{err}");
        assert!(err.contains("absent.txt"), "{err}");
    }

    #[test]
    fn comments_mode_ignores_the_allowlist_file() {
        let root = temp_root("run-comments");
        write(&root.join("crates").join("clean.rs"), CLEAN);
        let outcome = run_checks(
            &root,
            CheckMode::Comments,
            &serde_json::Value::Null,
            &root.join("absent.txt"),
        )
        .expect("comments mode runs");
        assert!(outcome.findings.is_empty());
    }

    #[test]
    fn empty_scan_root_is_a_harness_error() {
        let root = temp_root("run-empty");
        let err = run_checks(
            &root,
            CheckMode::Both,
            &serde_json::Value::Null,
            &root.join("allow.txt"),
        )
        .unwrap_err();
        assert!(err.starts_with("no rust sources under"), "{err}");
    }

    #[test]
    fn walk_skips_target_directories() {
        let root = temp_root("walk-target");
        write(&root.join("crates").join("real.rs"), CLEAN);
        write(
            &root
                .join("crates")
                .join("target")
                .join("debug")
                .join("gen.rs"),
            DIRTY,
        );
        let files = collect_sources(&root);
        assert_eq!(files, vec![root.join("crates").join("real.rs")]);
    }

    #[test]
    fn selftest_proves_both_detection_directions() {
        let workdir = temp_root("selftest");
        assert!(selftest(&workdir).expect("selftest runs"));
        let probe = workdir.join("crates").join("dirty.rs");
        assert!(probe.is_file(), "selftest must plant the dirty fixture");
    }
}
