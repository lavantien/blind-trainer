import datetime
import json
import sqlite3
import sys

db_path = sys.argv[1]
db = sqlite3.connect(db_path)
tables = [r[0] for r in db.execute("select name from sqlite_master where type='table'")]
out = {"tables": {}}
for t in tables:
    cols = [c[1] for c in db.execute(f"pragma table_info({t})")]
    rows = db.execute(f"select count(*) from {t}").fetchone()[0]
    out["tables"][t] = {"cols": cols, "rows": rows}
for t in tables:
    cols = [c[1] for c in db.execute(f"pragma table_info({t})")]
    day_cols = [c for c in cols if "day" in c or "due" in c or "ts" in c]
    for c in day_cols:
        vals = [r[0] for r in db.execute(f"select {c} from {t} where {c} is not null limit 200")]
        if vals:
            out["tables"][t][c] = {"min": min(vals), "max": max(vals), "distinct": sorted(set(vals))[:12]}
epoch = datetime.datetime(1970, 1, 1, tzinfo=datetime.timezone.utc)
out["today_day"] = (datetime.datetime.now(datetime.timezone.utc) - epoch).days
print(json.dumps(out, indent=1, default=str))
