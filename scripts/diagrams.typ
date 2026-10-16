#set page(width: auto, height: auto, margin: 12pt, fill: white)
#set text(font: ("Segoe UI", "Arial"), size: 9.5pt, lang: "en", region: "US")

#let node-fill = rgb("#2f3439")
#let ink = white
#let line-col = rgb("#8a9099")
#let node-w = 104pt
#let node-h = 36pt
#let arrow-w = 26pt
#let row-gap = 16pt

#let node(label) = rect(
  width: node-w,
  height: node-h,
  radius: 5pt,
  fill: node-fill,
  stroke: none,
  inset: 0pt,
  align(center + horizon, text(fill: ink, label)),
)

#let arrow = {
  line(length: arrow-w - 8pt, stroke: 0.9pt + line-col)
  polygon(fill: line-col, (0pt, -3.2pt), (6.5pt, 0pt), (0pt, 3.2pt))
}

#let fan(labels) = box({
  let h = labels.len() * node-h + (labels.len() - 1) * 9pt
  place(dx: -2pt, line(start: (0pt, 0pt), end: (0pt, h), stroke: 0.9pt + line-col))
  stack(spacing: 9pt, ..labels.map(node))
})

#let flow-row(items) = {
  let cells = ()
  for (i, item) in items.enumerate() {
    if i > 0 { cells.push(arrow) }
    if type(item) == array {
      cells.push(fan(item))
    } else {
      cells.push(node(item))
    }
  }
  grid(columns: (auto,) * cells.len(), align: center + horizon, ..cells)
}

#let diagram(rows) = {
  align(left, stack(spacing: row-gap, ..rows.map(flow-row)))
}

#let chain(labels, per-row: 6) = {
  let rows = ()
  let i = 0
  while i < labels.len() {
    let end = calc.min(i + per-row, labels.len())
    rows.push(labels.slice(i, end))
    i = end
  }
  diagram(rows)
}

#diagram((
  ("bt-core", "bt-scheme", "bt-data", ("bt-app", "bt-gendata", "bt-verify")),
  ("bt-scheme", "bt-ga", ("bt-ga-run", "bt-memofreq")),
  ("bt-puzzles", ("bt-app", "bt-verify")),
  ("bt-visual-diff",),
))
#pagebreak()

#chain((
  "notation parse",
  "move table",
  "48-sticker permutation",
  "flip twist analysis",
  "center-fixed predicate",
))
#pagebreak()

#chain((
  "Speffz map",
  "floating pair space",
  "classifier",
  "memo generator",
  "seeded scramble",
))
#pagebreak()

#chain((
  "ref_ground tsv",
  "corpus load",
  "user overlay",
  "verify vs simcheck",
  "srs sm-2",
))
#pagebreak()

#chain((
  "syllable inventory",
  "perceptual features",
  "frequency weights",
  "evolve",
  "syllables.json",
))
#pagebreak()

#chain((
  "config hub",
  "stores load",
  "corpus",
  "state machine",
  "views",
))
#pagebreak()

#chain((
  "pick due pair",
  "prepare card",
  "reps alternating A/B",
  "rate 1..4",
  "sm-2 write",
  "next pair",
))
#pagebreak()

#chain((
  "seeded scramble",
  "memo pairs",
  "per-orbit stream",
  "recall",
  "rate",
))
#pagebreak()

#chain((
  "scramble",
  "memo clock",
  "exec clock",
  "corpus replay verify",
  "solve log",
))
#pagebreak()

#chain((
  "test",
  "fmt",
  "lint",
  "check",
  "build",
  "fuzz",
  "mutation",
  "e2e",
  "visual",
))
