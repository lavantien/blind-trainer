import json
from datetime import datetime, timedelta, timezone

from fsrs import Card, Rating, Scheduler

SM2_QUALITY_TO_GRADE = {2: Rating.Again, 3: Rating.Hard, 4: Rating.Good, 5: Rating.Easy}

HISTORIES = [
    ("single_good", [(4, 0)]),
    ("single_again", [(2, 0)]),
    ("good_ladder", [(4, 0), (4, 1), (4, 6)]),
    ("lapse_then_recover", [(4, 0), (4, 1), (2, 6), (4, 1), (4, 3)]),
    ("hard_chain", [(3, 0), (3, 1), (3, 1)]),
    ("easy_chain", [(5, 0), (5, 1), (5, 6)]),
    ("mixed", [(4, 0), (5, 1), (3, 10), (2, 20), (4, 1), (5, 30)]),
    ("same_day_pair", [(4, 0), (4, 0)]),
    ("fixture_shape", [(4, 0), (3, 6), (5, 6)]),
    ("long_gap", [(4, 0), (4, 100)]),
]

BASE = datetime(2026, 1, 1, tzinfo=timezone.utc)

scheduler = Scheduler(
    desired_retention=0.9,
    learning_steps=(),
    relearning_steps=(),
    maximum_interval=36500,
    enable_fuzzing=False,
)

rows = ["case\tstep\tquality\telapsed_days\tstability\tdifficulty\tinterval_days"]
for case, history in HISTORIES:
    card = Card(card_id=1, due=BASE)
    now = BASE
    for step, (quality, elapsed) in enumerate(history):
        now = now + timedelta(days=elapsed)
        card, _ = scheduler.review_card(card, SM2_QUALITY_TO_GRADE[quality], now)
        interval_days = (card.due - now).days
        rows.append(
            "\t".join(
                [
                    case,
                    str(step),
                    str(quality),
                    str(elapsed),
                    repr(card.stability),
                    repr(card.difficulty),
                    str(interval_days),
                ]
            )
        )

with open("sm2_migration.tsv", "w", encoding="utf-8", newline="\n") as f:
    f.write("\n".join(rows) + "\n")

meta = {
    "generator": "py-fsrs fsrs.Scheduler.review_card",
    "package": "fsrs 5.1.3 (PyPI)",
    "source_tag": "github.com/open-spaced-repetition/py-fsrs v5.1.3, fsrs/fsrs.py sha256 9f9d6eeb7fd6ccfd3c644991b7e304553fbec51fd5f42421c328b706c32d0620",
    "scheduler": {
        "parameters": list(scheduler.parameters),
        "desired_retention": scheduler.desired_retention,
        "learning_steps": [],
        "relearning_steps": [],
        "maximum_interval": scheduler.maximum_interval,
        "enable_fuzzing": scheduler.enable_fuzzing,
    },
    "quality_to_grade": {str(k): int(v) for k, v in SM2_QUALITY_TO_GRADE.items()},
    "histories": {case: history for case, history in HISTORIES},
}
with open("sm2_migration_meta.json", "w", encoding="utf-8", newline="\n") as f:
    json.dump(meta, f, indent=2)
    f.write("\n")
