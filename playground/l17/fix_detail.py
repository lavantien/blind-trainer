from pathlib import Path

p = Path("src/app/views/algs/detail.c")
text = p.read_text(encoding="utf-8")

old = """static long long size_label_value(const char *label) {
    char digits[BT_REPO_LABEL_MAX];
    size_t at = 0;
    while (label[at] != '\\0' && label[at] != 'x' && at + 1 < sizeof digits) {
        digits[at] = label[at];
        at++;
    }
    digits[at] = '\\0';
    return strtoll(digits, NULL, 10);
}

"""
count = text.count(old)
assert count == 2, f"expected 2 helper copies, found {count}"
text = text.replace(old, "", 2)

old = """        if (walked->kind == BT_REPO_SIZE) {
            out->has_size = 1;
            out->size = size_label_value(walked->label);
        } else if (walked->kind == BT_REPO_METHOD) {"""
new = """        if (walked->kind == BT_REPO_SIZE) {
            out->has_size = bt_repo_size_of_label(walked->label, &out->size);
        } else if (walked->kind == BT_REPO_METHOD) {"""
assert old in text, "site_of size walk"
text = text.replace(old, new, 1)

p.write_text(text, encoding="utf-8")
print("detail.c fixed")
