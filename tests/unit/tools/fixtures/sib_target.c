#include "sib_helper.h"

int sib_probe(void) {
    return sib_answer() == 42;
}
