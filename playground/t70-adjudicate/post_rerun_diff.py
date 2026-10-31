import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))

MANIFESTS = ["core_scheme", "puzzles", "ga", "common", "data"]


def main():
    print("ENTER post_rerun_diff")
    minted = {}
    for name in MANIFESTS:
        with open(os.path.join(HERE, "manifest_%s.json" % name), encoding="utf-8") as handle:
            for row in json.load(handle)["rows"]:
                key = (row["file"], row["function"], row["op"], row["to"])
                minted[key] = minted.get(key, 0) + row["count"]
    print("minted rows total: %d spanning %d sites" % (len(minted), sum(minted.values())))
    with open(os.path.join(HERE, "families_core_scheme_data_puzzles_ga_common.json"),
              encoding="utf-8") as handle:
        families = json.load(handle)["families"]
    current = {}
    for family in families:
        key = (family["file"], family["function"], family["op"], family["to"])
        current[key] = current.get(key, 0) + family["count"]
    minted_sites = {k: {s for _ in range(0)} for k in minted}
    current_sites = {}
    for family in families:
        key = (family["file"], family["function"], family["op"], family["to"])
        current_sites.setdefault(key, set()).update(
            s.split(" ")[0] for s in family["sites"])
    with open(os.path.join(HERE, "manifest_core_scheme.json"), encoding="utf-8") as handle:
        disposed = json.load(handle)["test_disposed"]
    for name in MANIFESTS[1:]:
        with open(os.path.join(HERE, "manifest_%s.json" % name), encoding="utf-8") as handle:
            disposed += json.load(handle)["test_disposed"]
    for entry in disposed:
        key = (entry["file"], entry["function"], entry["op"], entry["to"])
        minted[key] = minted.get(key, 0)
        for _ in entry["killed"]:
            current_sites.setdefault(key, set())
    changed = []
    for key in sorted(set(minted) | set(current)):
        was = minted.get(key, 0)
        now = current.get(key, 0)
        if was != now:
            changed.append((key, was, now))
    print("families with changed presence: %d" % len(changed))
    for key, was, now in changed:
        delta = now - was
        print("  %-+4d was=%-3d now=%-3d %s %s %s to=%s" % (delta, was, now, key[0],
                                                            key[1] or "(empty)", key[2],
                                                            repr(key[3])))
    print("EXIT post_rerun_diff")


if __name__ == "__main__":
    main()
