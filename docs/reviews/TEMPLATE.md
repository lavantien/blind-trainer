# adversarial review record

Copy this file to docs/reviews/<scope-or-date>.md and fill every field. Do not leave placeholder rows in a finished record.

## header

| field | value |
| --- | --- |
| wave or scope | <wave id, branch, or change under review> |
| reviewer 1 | <name or agent id> |
| reviewer 2 | <name or agent id> |
| date range | <yyyy-mm-dd to yyyy-mm-dd> |
| final verdict | <clean or findings fixed at commit <hash>> |

The two reviewers work independently and never see each other's findings before adjudication. Each hunts counterexamples, breaks edge cases, and challenges assumptions in the change under review.

## findings

| id | reviewer | severity | location | finding | resolution |
| --- | --- | --- | --- | --- | --- |
| F1 | <1 or 2> | <high, medium, low> | <file:line or symbol> | <what breaks and how> | <fix commit hash, or `none` plus a refutation note> |

## adjudication

Refuted findings stay in the table with resolution `none` and a refutation note, then get their reasoning recorded here so the same claim is not re-litigated later.

| id | outcome | reasoning |
| --- | --- | --- |
| F1 | <refuted or confirmed> | <why the finding does or does not hold> |

## verified-clean inventory

Everything the reviewers checked and found no defect in. One line per area, so coverage gaps stay visible.

- <area or behavior checked>: clean
