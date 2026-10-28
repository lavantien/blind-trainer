import hashlib

GENERATOR = "first-setup-plateau-1"

for canonical in (
    '{"penalty_other":1e+16}',
    '{"penalty_other":1e-07}',
    '{"penalty_other":1000000000000000.0}',
    '{"penalty_other":0.00001}',
    '{"penalty_other":2.0}',
):
    preimage = canonical.encode() + b"\0" + GENERATOR.encode()
    print(f"{canonical} => {hashlib.sha256(preimage).hexdigest()}")
