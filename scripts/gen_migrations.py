import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
MIGRATIONS = REPO / "db" / "migrations"
OUT_C = REPO / "src" / "data" / "migrations_embedded.c"
OUT_H = REPO / "src" / "data" / "migrations_embedded.h"


def c_escape(text):
    out = []
    for ch in text:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif ord(ch) < 32:
            out.append(f"\\{ord(ch):03o}")
        else:
            out.append(ch)
    return "".join(out)


def main(argv):
    names = sorted(p.stem for p in MIGRATIONS.glob("*.sql"))
    if not names:
        print(f"no migrations under {MIGRATIONS}", file=sys.stderr)
        return 1
    header = [
        "#ifndef BT_DATA_MIGRATIONS_EMBEDDED_H",
        "#define BT_DATA_MIGRATIONS_EMBEDDED_H",
        "",
        "#include <stddef.h>",
        "",
        "size_t bt_db_embedded_migration_count(void);",
        "const char *bt_db_embedded_migration_name(size_t index);",
        "const char *bt_db_embedded_migration_sql(size_t index);",
        "",
        "#endif",
        "",
    ]
    OUT_H.write_text("\n".join(header), encoding="utf-8", newline="\n")
    body = ["#include \"data/migrations_embedded.h\"", ""]
    for name in names:
        sql = (MIGRATIONS / f"{name}.sql").read_text(encoding="utf-8")
        body.append(f"static const char MIGRATION_{name.upper()}[] = \"{c_escape(sql)}\";")
    body += [
        "",
        f"static const char *const MIGRATION_NAMES[] = {{",
    ]
    for name in names:
        body.append(f"    \"{name}\",")
    body += [
        "};",
        "",
        "static const char *const MIGRATION_SQL[] = {",
    ]
    for name in names:
        body.append(f"    MIGRATION_{name.upper()},")
    body += [
        "};",
        "",
        "#define EMBEDDED_COUNT ((size_t)(sizeof MIGRATION_SQL / sizeof MIGRATION_SQL[0]))",
        "",
        "size_t bt_db_embedded_migration_count(void) {",
        "    return EMBEDDED_COUNT;",
        "}",
        "",
        "const char *bt_db_embedded_migration_name(size_t index) {",
        "    if (index >= EMBEDDED_COUNT) {",
        "        return NULL;",
        "    }",
        "    return MIGRATION_NAMES[index];",
        "}",
        "",
        "const char *bt_db_embedded_migration_sql(size_t index) {",
        "    if (index >= EMBEDDED_COUNT) {",
        "        return NULL;",
        "    }",
        "    return MIGRATION_SQL[index];",
        "}",
        "",
    ]
    OUT_C.write_text("\n".join(body), encoding="utf-8", newline="\n")
    print(f"embedded {len(names)} migrations into {OUT_C}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
