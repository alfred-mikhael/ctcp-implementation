#ifndef CTCP_CONTEXT_H
#define CTCP_CONTEXT_H

#include "CTCPStructs.h"
#include "ns3/mt-context.h"
#include "ns3/mt-event.h"
#include "ns3/mt-rx-buffer.h"
#include "ns3/mt-timer.h"
#include "ns3/mt-tx-buffer.h"

#include <iostream>
#include <vector>

namespace ns3 {

class ctcp_context : public MTContext {
public:
    int seqno_next = 1;
    int ackseq_next = 0;
    int ssthresh = 999999;
    // equivalent to cwnd in CTCP
    float tokens = 3;
    // RTO is interpreted as milliseconds by MTTimer.
    int RTO = 500;
    int max_rto = 60000; // RFC 6298 §2.5 upper bound (ms)
    // seqno of the last packet sent before timeout. Packets sent prior to this
    // should not impact congestion control.
    int ackseq_timeout = -1;

    double RTT = 999999999;
    double RTT_min = 99999999;

    // time of the last CTCP_ACK recieved
    int tlast_ack = 0;
    int last_unacked_seq = 0;

    bool first_rto = true;
    // not keeping an estimate of RTT so don't need this
    // int rtt_sample_seq = -1;
    // double rtt_sample_time = 0.0;

    // Equivalent of rcwnd for CTCP
    // number of blocks of packets which are in flight
    int numblks = 100;
    // size of each block of packets
    int blksize = 16;

    // Indicates which block is the first in line.
    // currblk + numblks is the number of the last in-flight block
    int currblk = 0;
    // Number of degrees of freedom for the current sending block
    int currdof = 0;
    int uncoded_sent_so_far = 0;
    // current degrees of freedom for current receiving block
    int ack_currdof = 0;
    // number between 0 and numblks
    int decoding_offset = 0;
    int decoding_currblk = 0;

    // short term estimate of packet loss rate
    float loss_rate = 0;
    float smoothing_factor = 0.1;

    std::vector<ctcp_packet_info> inflight_info;

    // matrices of size blksize x blksize, stored as a flat matrix in ROW MAJOR ORDER
    std::vector<std::vector<gf256::gf>> coding_arr;
    std::vector<int> rank_arr;
    // matrices of size blksize x smss (usually 1024), stored as a flat matrix in ROW MAJOR ORDER
    std::vector<std::vector<gf256::gf>> payload_arr;

    ctcp_conn_state conn_state = CTCP_CONN_CLOSED;
    int stage{};

    MTTimer ack_timeout;
    // Reused timeout event to avoid allocating (and leaking) a new MTEvent on every timer reset.
    MTEvent *ack_timeout_event = nullptr;

    MTTxBuffer tx_block_buf;
    MTTxBuffer tx_byte_buf;

    char read_from_addr{};
    char write_to_addr{};
    double coding_delay = 0;

    ctcp_context() {
        std::cout << "ctcp_conext: initializing" << std::endl;
        gf256::init();
        const int SMSS = 1024;
        coding_arr =
            std::vector<std::vector<gf256::gf>>(numblks, std::vector<gf256::gf>(blksize * blksize));
        payload_arr =
            std::vector<std::vector<gf256::gf>>(numblks, std::vector<gf256::gf>(blksize * SMSS));
        rank_arr = std::vector<int>(numblks, 0);
    }
    ~ctcp_context() {
        std::cout << "ctcp_context: destructing ctcp_context!" << std::endl;
    }
};

} // namespace ns3

#endif
