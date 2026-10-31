PLAN_EDGE = list("UB UR UF UL LU LF LD LB FU FR FD FL RU RB RD RF BU BL BD BR DF DR DB DL".split())
PLAN_CORNER = list("UBL UBR UFR UFL LUB LUF LDF LDB FUL FUR FDR FDL RUF RUB RDB RDF BUR BUL BDL BDR DFL DFR DBR DBL".split())

NAMES = PLAN_CORNER + PLAN_EDGE


def key_clean(name):
    out = [ord(name[i]) if i < len(name) else 0 for i in range(3)]
    if out[0] > out[1]:
        out[0], out[1] = out[1], out[0]
    if out[1] > out[2]:
        out[1], out[2] = out[2], out[1]
    if out[0] > out[1]:
        out[0], out[1] = out[1], out[0]
    return tuple(out)


def key_no_third_pass(name):
    out = [ord(name[i]) if i < len(name) else 0 for i in range(3)]
    if out[0] > out[1]:
        out[0], out[1] = out[1], out[0]
    if out[1] > out[2]:
        out[1], out[2] = out[2], out[1]
    return tuple(out)


def key_negated_third_pass(name):
    out = [ord(name[i]) if i < len(name) else 0 for i in range(3)]
    if out[0] > out[1]:
        out[0], out[1] = out[1], out[0]
    if out[1] > out[2]:
        out[1], out[2] = out[2], out[1]
    if out[0] <= out[1]:
        out[0], out[1] = out[1], out[0]
    return tuple(out)


def key_init_tweak(name, index, value):
    out = [value, value, value]
    for i in range(min(3, len(name))):
        out[i] = ord(name[i])
    if out[0] > out[1]:
        out[0], out[1] = out[1], out[0]
    if out[1] > out[2]:
        out[1], out[2] = out[2], out[1]
    if out[0] > out[1]:
        out[0], out[1] = out[1], out[0]
    return tuple(out)


def oracle(a, b):
    return sorted(NAMES[a]) == sorted(NAMES[b])


def main():
    print("ENTER face_key_probe")
    keys_clean = [key_clean(n) for n in NAMES]
    mismatches = 0
    for a in range(48):
        for b in range(48):
            same = keys_clean[a] == keys_clean[b]
            if same != oracle(a, b):
                mismatches += 1
    print("clean key vs oracle mismatches: %d" % mismatches)

    variants = {
        "deleted_third_pass": [key_no_third_pass(n) for n in NAMES],
        "negated_third_pass": [key_negated_third_pass(n) for n in NAMES],
        "out0_255": [key_init_tweak(n, 0, 255) for n in NAMES],
        "out1_255": [key_init_tweak(n, 1, 255) for n in NAMES],
        "out2_255": [key_init_tweak(n, 2, 255) for n in NAMES],
        "out0_1": [key_init_tweak(n, 0, 1) for n in NAMES],
        "out1_1": [key_init_tweak(n, 1, 1) for n in NAMES],
        "out2_1": [key_init_tweak(n, 2, 1) for n in NAMES],
    }
    for label, keys in variants.items():
        caught = 0
        for a in range(48):
            for b in range(48):
                if (keys[a] == keys[b]) != oracle(a, b):
                    caught += 1
        print("%s: %d oracle disagreements" % (label, caught))
    print("EXIT face_key_probe")


if __name__ == "__main__":
    main()
