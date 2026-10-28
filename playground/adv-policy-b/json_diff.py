import json
import os

here = os.path.dirname(__file__)


def flat(obj, prefix=""):
    out = {}
    if isinstance(obj, dict):
        for k, v in obj.items():
            out.update(flat(v, prefix + "/" + str(k)))
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            out.update(flat(v, prefix + "/" + str(i)))
    else:
        out[prefix] = obj
    return out


with open(os.path.join(here, "report_a.json"), encoding="utf-8") as fh:
    a = flat(json.load(fh))
with open(os.path.join(here, "report_b.json"), encoding="utf-8") as fh:
    b = flat(json.load(fh))
keys_a = set(a)
keys_b = set(b)
print("ONLY_A", sorted(keys_a - keys_b)[:10])
print("ONLY_B", sorted(keys_b - keys_a)[:10])
diff = [k for k in sorted(keys_a & keys_b) if a[k] != b[k]]
print("DIFFERING_KEYS", len(diff))
for k in diff[:10]:
    print(k, repr(a[k]), repr(b[k]))
print("TOTAL_A", len(keys_a), "TOTAL_B", len(keys_b))
