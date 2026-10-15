import json
import re

import fitz

PDF = "bestf2l.pdf"
ROT_COLOR = 0x3366FF
PINK = (0.9569, 0.8, 0.8)
COL_SPLIT = 250


def close(a, b):
    return all(abs(x - y) < 0.01 for x, y in zip(a, b))


def reconstruct():
    doc = fitz.open(PDF)
    pages = []
    for pno in range(len(doc)):
        page = doc[pno]
        images = [im["bbox"] for im in page.get_image_info()]
        cases = [b for b in images if 40 < b[2] - b[0] < 80 and 40 < b[3] - b[1] < 90 and b[0] < 400]
        pinks = []
        for dr in page.get_drawings():
            r = dr["rect"]
            if dr.get("fill") and close(dr["fill"], PINK) and r.width > 60 and 10 < r.height < 30:
                pinks.append(r)
        alg_spans = []
        headers = []
        for block in page.get_text("dict")["blocks"]:
            if block["type"] != 0:
                continue
            for line in block["lines"]:
                for span in line["spans"]:
                    t = span["text"]
                    if not t.strip():
                        continue
                    x0, y0 = span["bbox"][0], span["bbox"][1]
                    if round(span["size"]) == 14:
                        alg_spans.append({"x": x0, "y": y0, "rot": span["color"] == ROT_COLOR, "text": t})
                    else:
                        headers.append({"x": x0, "y": y0, "size": round(span["size"]), "text": t.strip()})
        rows = {}
        for s in alg_spans:
            key = round(s["y"] / 4) * 4
            rows.setdefault(key, []).append(s)
        lines = []
        for key in sorted(rows):
            group = sorted(rows[key], key=lambda s: s["x"])
            for col_name, lo, hi in (("left", 0, COL_SPLIT), ("right", COL_SPLIT, 10000)):
                col = [s for s in group if lo <= s["x"] < hi]
                if not col:
                    continue
                text = "".join(s["text"] for s in col)
                text = re.sub(r"\s+", " ", text).strip()
                y = key
                hl = any(
                    r.y0 - 2 <= y <= r.y1 + 6 and r.x0 < hi and r.x1 > lo and r.x0 >= lo - 60 and r.x1 <= hi + 60
                    for r in pinks
                )
                lines.append({"y": y, "col": col_name, "x0": col[0]["x"], "alg": text, "multi_slot": hl})
        pages.append({"page": pno + 1, "case_images": cases, "lines": lines, "headers": headers})
    return pages


def assign(pages):
    cases = []
    for p in pages:
        imgs = sorted(p["case_images"], key=lambda b: (b[1], b[0]))
        for idx, img in enumerate(imgs):
            col = "left" if img[0] < COL_SPLIT else "right"
            lo, hi = (0, COL_SPLIT) if col == "left" else (COL_SPLIT, 10000)
            below = [
                ln
                for ln in p["lines"]
                if ln["col"] == col
                and ln["y"] >= img[1] - 2
                and (idx + 1 >= len(imgs) or ln["y"] < imgs[idx + 1][1] - 2 or imgs[idx + 1][0] >= COL_SPLIT or True)
            ]
            nxt = [b for b in imgs if b[1] > img[1] + 2 and ((b[0] < COL_SPLIT) == (img[0] < COL_SPLIT))]
            limit = min((b[1] for b in nxt), default=10000)
            below = [ln for ln in below if ln["y"] < limit - 2]
            hdrs = [h for h in p["headers"] if h["y"] < img[1] and h["size"] >= 20]
            cases.append(
                {
                    "page": p["page"],
                    "y": round(img[1], 1),
                    "col": col,
                    "order": (p["page"], round(img[1], 1), 0 if col == "left" else 1),
                    "algs": [ln["alg"] for ln in below],
                    "multi": [ln["multi_slot"] for ln in below],
                    "section": hdrs[-1]["text"] if hdrs else "",
                }
            )
    cases.sort(key=lambda c: c["order"])
    return cases


def main():
    pages = reconstruct()
    cases = assign(pages)
    with open("f2l_cases.json", "w", encoding="utf-8") as fh:
        json.dump(cases, fh, indent=1)
    print("total case images:", len(cases))
    sec1 = []
    sec = ""
    for c in cases:
        pass
    for c in cases:
        print(
            c["order"],
            c["section"][:30],
            len(c["algs"]),
            c["algs"][0][:48] if c["algs"] else "NONE",
            "MULTI" if c["multi"] and c["multi"][0] else "",
        )


if __name__ == "__main__":
    main()
