#ifndef CTCPSTRUCTS_H
#define CTCPSTRUCTS_H

#include "ns3/mtp-types.h"
#include <Eigen/Core>
#include <cstdint>

namespace ns3 {

enum ctcp_conn_state {
    CTCP_CONN_CLOSED = 0,
    CTCP_CONN_SYN_SENT,
    CTCP_CONN_SYN_RCVD,
    CTCP_CONN_ESTABLISHED
};

class interm_out : public MTIntermediateOutput {
public:
    bool change_cwnd{};
    bool skip_ack_eps{};
};

struct ctcp_packet_info {
    int seq_num;
    int block_num;
    int64_t sent_time;
};

#pragma once
#include <cstdint>

namespace gf256 {

using gf = uint8_t;

// --- Tables ---
extern uint8_t mul_table[256][256];
extern uint8_t inv_table[256];

// --- Init ---
void init();

// --- Arithmetic (ALL INLINE) ---

inline gf add(gf a, gf b) {
    return a ^ b;
}

inline gf sub(gf a, gf b) {
    return a ^ b;
}

inline gf mul(gf a, gf b) {
    return mul_table[a][b];
}

inline gf inv(gf a) {
    // assume a != 0
    return inv_table[a];
}

inline gf div(gf a, gf b) {
    // assume b != 0
    return mul_table[a][inv_table[b]];
}

// --- In-place ops (important for performance) ---

inline void add_inplace(gf &a, gf b) {
    a ^= b;
}

inline void mul_inplace(gf &a, gf b) {
    a = mul_table[a][b];
}

} // namespace gf256
} // namespace ns3

#endif