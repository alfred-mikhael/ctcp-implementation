#include "CTCPStructs.h"

namespace ns3 {
namespace gf256 {

uint8_t mul_table[256][256];
uint8_t inv_table[256];

static uint8_t exp_table[512];
static uint8_t log_table[256];

bool initialized = 0;

void init() {
    if (initialized)
        return;
    // --- Build log/exp tables (once) ---
    uint16_t x = 1;

    for (int i = 0; i < 255; i++) {
        exp_table[i] = (uint8_t)x;
        log_table[x] = i;

        x <<= 1;
        if (x & 0x100)
            x ^= 0x11d;
    }

    for (int i = 255; i < 512; i++) {
        exp_table[i] = exp_table[i - 255];
    }

    log_table[0] = 0;

    // --- Build multiplication table ---
    for (int a = 0; a < 256; a++) {
        for (int b = 0; b < 256; b++) {
            if (a == 0 || b == 0) {
                mul_table[a][b] = 0;
            } else {
                int sum = log_table[a] + log_table[b];
                mul_table[a][b] = exp_table[sum];
            }
        }
    }

    // --- Build inverse table ---
    inv_table[0] = 0; // undefined, but safe sentinel

    for (int a = 1; a < 256; a++) {
        inv_table[a] = exp_table[255 - log_table[a]];
    }
}

} // namespace gf256
} // namespace ns3