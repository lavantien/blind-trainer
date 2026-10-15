import json
from datetime import datetime, timedelta, timezone

from fsrs import Card, Rating, Scheduler

CORPUS = [
    ("first_again", [(1, None)]),
    ("first_hard", [(2, None)]),
    ("first_good", [(3, None)]),
    ("first_easy", [(4, None)]),
    ("good_ladder", [(3, None), (3, 1), (3, 3), (3, 7), (3, 16), (3, 35)]),
    ("hard_ladder", [(2, None), (2, 1), (2, 1), (2, 1), (2, 1)]),
    ("easy_ladder", [(4, None), (4, 1), (4, 3), (4, 10), (4, 30)]),
    ("lapse_streak", [(3, None), (1, 30), (1, 1), (1, 1), (1, 1), (1, 1)]),
    ("long_gap", [(3, None), (3, 1), (3, 100), (3, 365)]),
    ("same_day_mix", [(3, None), (3, 0), (4, 0), (2, 0), (1, 0), (3, 1)]),
    ("same_day_from_again", [(1, None), (1, 0), (2, 0), (3, 0)]),
    ("reversion_alternating", [(2, None), (2, 5), (4, 5), (2, 5), (4, 5), (2, 5), (4, 5)]),
    ("mixed_walk", [(3, None), (1, 7), (2, 1), (4, 4), (3, 2), (3, 20)]),
    ("huge_gap_after_easy", [(4, None), (3, 10000)]),
]

BASE = datetime(2026, 1, 1, tzinfo=timezone.utc)

scheduler = Scheduler(
    desired_retention=0.9,
    learning_steps=(),
    relearning_steps=(),
    maximum_interval=36500,
    enable_fuzzing=False,
)

rows = ["case\tstep\trating\telapsed_days\tretrievability\tstability\tdifficulty\tinterval_days"]
for case, steps in CORPUS:
    card = Card(card_id=1, due=BASE)
    now = BASE
    for step, (rating, gap) in enumerate(steps):
        if gap is not None:
            now = now + timedelta(days=gap)
        r = card.get_retrievability(current_datetime=now) if card.last_review else None
        card, _ = scheduler.review_card(card, Rating(rating), now)
        interval_days = (card.due - now).days
        rows.append(
            "\t".join(
                [
                    case,
                    str(step),
                    str(rating),
                    "" if gap is None else str(gap),
                    "" if r is None else repr(r),
                    repr(card.stability),
                    repr(card.difficulty),
                    str(interval_days),
                ]
            )
        )

with open("reference_vectors.tsv", "w", encoding="utf-8", newline="\n") as f:
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
    "base_datetime": BASE.isoformat(),
    "cases": {case: steps for case, steps in CORPUS},
}
with open("reference_meta.json", "w", encoding="utf-8", newline="\n") as f:
    json.dump(meta, f, indent=2)
    f.write("\n")
