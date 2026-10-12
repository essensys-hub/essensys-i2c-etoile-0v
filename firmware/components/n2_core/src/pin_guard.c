#include "pin_guard.h"

int n2_pin_guard_check(uint64_t mask, n2_pin_is_output_fn is_output, void *ctx)
{
    for (int pin = 0; pin < 64; pin++) {
        if ((mask >> pin) & 1u) {
            if (is_output == 0 || is_output(pin, ctx)) {
                return pin;
            }
        }
    }
    return -1;
}
