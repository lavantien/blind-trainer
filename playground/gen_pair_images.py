import re
SRC = "ref_doc/bld/speedsolving-letter-pairs.wiki"
OUT = "crates/bt-app/src/views/images.rs"
HEADER = """use std::collections::BTreeMap;
use std::sync::LazyLock;

static PAIR_IMAGES: [(&str, &str); {count}] = [
{rows}];

static INDEX: LazyLock<BTreeMap<&'static str, &'static str>> = LazyLock::new(|| {
    PAIR_IMAGES.iter().copied().collect()
});

pub fn image_for(letters: [char; 2]) -> Option<&'static str> {
    let key: String = letters.iter().collect();
    INDEX.get(key.as_str()).copied()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn table_is_sorted_and_unique() {
        for window in PAIR_IMAGES.windows(2) {
            assert!(window[0].0 < window[1].0, "{} >= {}", window[0].0, window[1].0);
        }
    }

    #[test]
    fn known_pairs_resolve() {
        assert_eq!(image_for(['A', 'B']), Some("Anthony Brooks"));
        assert_eq!(image_for(['Q', 'M']), Some("Quail Man"));
        assert_eq!(image_for(['J', 'X']), None);
        assert_eq!(image_for(['X', 'X']), None);
    }
}
"""

OVERRIDES = {
    "PH": "Pharaoh",
    "QE": "Queue",
    "RQ": "Racquetball",
    "TV": "Television",
    "UE": "Ukulele instrument",
}

def parse(path):
    pairs = {}
    cur = None
    for line in open(path, encoding="utf-8"):
        m = re.match(r"^==([A-Z]{2})==\s*$", line)
        if m:
            cur = m.group(1)
            continue
        if cur and line.startswith("*"):
            word = line[1:].strip().strip("[]").split("(")[0].strip()
            if word and cur not in pairs:
                pairs[cur] = OVERRIDES.get(cur, word)
    return pairs

def main():
    pairs = parse(SRC)
    rows = "".join(
        '    ("{}", "{}"),\n'.format(k, pairs[k].replace('"', "'")) for k in sorted(pairs)
    )
    open(OUT, "w", encoding="utf-8", newline="\n").write(HEADER.replace("{count}", str(len(pairs))).replace("{rows}", rows))
    print("wrote", OUT, "pairs:", len(pairs))

if __name__ == "__main__":
    main()
