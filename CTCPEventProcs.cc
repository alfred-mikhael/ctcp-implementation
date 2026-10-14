#include "CTCPEventProcs.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>

namespace {

std::random_device rd;
std::mt19937 seed_rd(rd());
std::uniform_int_distribution<uint32_t> seed_gen(0, UINT32_MAX);

void AttachDefaultCtcpPktGenRules(PKT_GEN_INSTR *instr) {
    if (!instr) {
        return;
    }
    if (instr->smss <= 0) {
        instr->smss = 1024;
    }

    instr->segmentation_rule = [](const ns3::PktGenSegmentContext &ctx,
                                  const PKT_GEN_INSTR &current) {
        if (ctx.remaining_units <= 0) {
            if (current.header_only_packet) {
                return ns3::PktGenSegmentDecision::HeaderOnly(true, 0);
            } else {
                return ns3::PktGenSegmentDecision::NoEmit(true);
            }
        }
        return ns3::PktGenSegmentDecision::EmitData(1);
    };

    instr->header_rule = [](CTCPheader &hdr, const ns3::PktGenSegmentContext &ctx,
                            const ns3::PktGenSegmentDecision &decision,
                            const PKT_GEN_INSTR &current) {
        (void)decision;
        hdr.src_port = current.bp.src_port;
        hdr.dest_port = current.bp.dest_port;
        hdr.seq = current.bp.seq_num + ctx.emitted_units;
        hdr.ack = current.bp.ack ? 1 : 0;
        hdr.syn = current.bp.syn ? 1 : 0;
        hdr.ack_seq = current.bp.ack_seq;
        hdr.ack_blk = current.bp.ack_blk;
        hdr.ack_currdof = current.bp.ack_currdof;
        hdr.blknum = current.bp.blknum;
        hdr.numblks = current.bp.numblks;
        hdr.blksize = current.bp.blksize;
        hdr.seed = current.bp.seed;
        hdr.uncoded = current.bp.uncoded;
    };
    // instr->packet_finalizer_hook = [](Packet &p, const PktGenSegmentContext &ctx,
    //                                   const PktGenSegmentDecision &decision,
    //                                   const PKT_GEN_INSTR &current) {};
}

// Emit CTCP_DATA segments for bytes already enqueued in tx_buf (ESTABLISHED path).
// Used by ctcp_send_ep and by handshake completion when draining pre-CTCP_CONNECT pending
// CTCP_DATA.
void CtcpEmitSendableSegments(flow_id fid, ctcp_context &ctx, EPResult &result) {
    const int SMSS = 1024;
    int remaining_blocks = ctx.tx_block_buf.unsent();
    std::cout << "CtcpEmitSendableSegments: Computed " << remaining_blocks
              << " unsent blocks. ctx.currblk=" << ctx.currblk << std::endl;
    // Prune inflight_info of old packets
    int64_t now = Simulator::Now().GetMilliSeconds();
    const int old_size = ctx.inflight_info.size();
    ctx.inflight_info.erase(remove_if(ctx.inflight_info.begin(), ctx.inflight_info.end(),
                                      [&ctx, now](const ctcp_packet_info p) {
                                          return now - p.sent_time > 1.5 * ctx.RTT;
                                      }),
                            ctx.inflight_info.end());
    const int pruned = old_size - ctx.inflight_info.size();
    if (pruned > 5) {
        std::cout << "CtcpEmitSendableSegments: Pruned " << pruned
                  << " packets because they exceeded lifetime of 1.5 RTT" << std::endl;
    }
    int tokens = floor(ctx.tokens) - ctx.inflight_info.size();
    if (tokens <= 0) {
        return;
    }
    if (remaining_blocks > 0) {
        int *arr = new int[ctx.numblks];
        for (int i = 0; i < ctx.numblks; i++)
            arr[i] = 0;
        for (ctcp_packet_info p_info : ctx.inflight_info) {
            if (p_info.block_num < ctx.currblk)
                continue;
            arr[p_info.block_num - ctx.currblk]++;
        }
        bool is_currblk = true;
        int curr_unsaturated = 0;
        while (tokens > 0) {
            // 1. find first block which isn't saturated
            if (is_currblk &&
                (1 - ctx.loss_rate) * arr[curr_unsaturated] >= ctx.blksize - ctx.currdof) {
                curr_unsaturated++;
                is_currblk = false;
            }
            while (curr_unsaturated < ctx.numblks &&
                   (1 - ctx.loss_rate) * arr[curr_unsaturated] >= ctx.blksize) {
                curr_unsaturated++;
                // Should not exceed receive window
                if (curr_unsaturated >= ctx.numblks)
                    return;
            }
            // 2. generate pkt_gen_instr
            PKT_GEN_INSTR *data_instr = new PKT_GEN_INSTR(fid);
            data_instr->packets_to_send = 1;
            data_instr->smss = SMSS;

            data_instr->bp.seq_num = ctx.seqno_next++;
            data_instr->bp.blknum = curr_unsaturated + ctx.currblk;
            data_instr->bp.ack = false;
            data_instr->bp.syn = false;
            data_instr->bp.ack_seq = ctx.ackseq_next;
            data_instr->bp.ack_blk = ctx.decoding_currblk;
            data_instr->bp.ack_currdof = ctx.ack_currdof;
            data_instr->bp.numblks = ctx.numblks;
            data_instr->bp.blksize = ctx.blksize;
            data_instr->bp.field_size = 256;
            data_instr->bp.uncoded = -1;
            if (curr_unsaturated == 0) {
                if (ctx.uncoded_sent_so_far < ctx.blksize) {
                    data_instr->bp.uncoded = ctx.uncoded_sent_so_far++;
                }
            }
            data_instr->bp.seed = seed_gen(seed_rd);

            AttachDefaultCtcpPktGenRules(data_instr);
            result.instrs.emplace_back(data_instr);

            ctx.inflight_info.push_back({data_instr->bp.seq_num, data_instr->bp.blknum,
                                         Simulator::Now().GetMicroSeconds()});
            std::cout << "CtcpEmitSendableSegments: sent packet for blknum="
                      << curr_unsaturated + ctx.currblk << " at t=" << now
                      << " packets in-flight for blknum=" << arr[curr_unsaturated] + 1
                      << " uncoded=" << data_instr->bp.uncoded << " seed=" << data_instr->bp.seed
                      << " remaining tokens=" << tokens - 1 << std::endl;
            // 3. Increase arr[blknum] and decrease ctx.tokens
            arr[curr_unsaturated]++;
            tokens--;
        }
        delete[] arr;
    }
    // Arm timer if new data is being sent and there's not already a timer active.
    if (remaining_blocks > 0 && !ctx.ack_timeout.isActive()) {
        if (!ctx.ack_timeout_event) {
            ctx.ack_timeout_event = new CTCP_MISS_ACK(10, fid);
        }
        CTCP_MISS_ACK *time_ev = (CTCP_MISS_ACK *)ctx.ack_timeout_event;
        time_ev->flowId = fid;
        time_ev->time = 10;
        time_ev->seq_num = ctx.seqno_next - 1;
        ctx.ack_timeout.set_duration(ctx.RTO);
        ctx.ack_timeout.start(time_ev);

        std::cout << "CtcpEmitSendableSegments: Arming timer since none is active "
                     "set ctx.ack_timeout.isActive()="
                  << ctx.ack_timeout.isActive() << std::endl;
    }
}

/**
 * Inserts vector c into matrix C such that C remains upper triangular. Performs the
 * same operations on vector p and inserts into P. Assumes that the input matrices
 * are given in ROW MAJOR ORDER.
 */
bool OnlineGaussianEliminationInsert(std::vector<gf256::gf> &C, std::vector<gf256::gf> &P,
                                     std::vector<uint8_t> &c, std::vector<uint8_t> &p, int blksize,
                                     int SMSS, int next_row_index) {
    int index = 0;

    // find first nonzero
    while (index < blksize && c[index] == 0)
        index++;

    while (index < blksize) {
        // Case 1: insert pivot row
        if (index == next_row_index) {
            uint8_t pivot = c[index];
            uint8_t inv = gf256::inv(pivot);

            // normalize c (only from pivot onward)
            for (int j = index; j < blksize; j++) {
                gf256::mul_inplace(c[j], inv);
            }

            // normalize p
            for (int j = 0; j < SMSS; j++) {
                gf256::mul_inplace(p[j], inv);
            }

            // write row into C
            gf256::gf *Crow = &C[next_row_index * blksize];
            for (int j = index; j < blksize; j++) {
                Crow[j] = c[j];
            }

            // write row into P
            gf256::gf *Prow = &P[next_row_index * SMSS];
            for (int j = 0; j < SMSS; j++) {
                Prow[j] = p[j];
            }

            return true;
        }

        // Case: cannot use this packet
        if (index > next_row_index) {
            return false;
        }

        // Case 2: eliminate
        gf256::gf factor = c[index];

        if (factor != 0) {
            gf256::gf *Crow = &C[index * blksize];
            gf256::gf *Prow = &P[index * SMSS];

            // eliminate c
            for (int j = index; j < blksize; j++) {
                gf256::add_inplace(c[j], gf256::mul(factor, Crow[j]));
            }

            // eliminate p
            for (int j = 0; j < SMSS; j++) {
                gf256::add_inplace(p[j], gf256::mul(factor, Prow[j]));
            }
        }

        // advance pivot search
        index++;
        while (index < blksize && c[index] == 0)
            index++;
    }

    return false;
}
/** Generated by ChatGPT */
void solve_upper_triangular(std::vector<gf256::gf> &C, std::vector<gf256::gf> &P, int blksize,
                            int SMSS) {
    for (int i = blksize - 1; i >= 0; --i) {
        gf256::gf *row_i_C = &C[i * blksize];
        gf256::gf *row_i_P = &P[i * SMSS];

        // eliminate rows above
        for (int j = 0; j < i; ++j) {
            gf256::gf *row_j_C = &C[j * blksize];
            gf256::gf *row_j_P = &P[j * SMSS];

            gf256::gf factor = row_j_C[i];
            if (factor == 0)
                continue;

            // eliminate C[j][i]
            for (int col = i; col < blksize; col++) {
                gf256::add_inplace(row_j_C[col], gf256::mul(factor, row_i_C[col]));
            }

            // apply to payload
            for (int col = 0; col < SMSS; col++) {
                gf256::add_inplace(row_j_P[col], gf256::mul(factor, row_i_P[col]));
            }
        }
    }
}
} // namespace

ctcp_connect_ep::ctcp_connect_ep() {}
ctcp_connect_ep::~ctcp_connect_ep() {}
EventProcessorOutput *ctcp_connect_ep::process(MTEvent *e, EventProcessorOutput *epOut) {
    std::cout << "ctcp_connect_ep::process starting" << std::endl;
    CTCP_CONNECT ev = *(CTCP_CONNECT *)e;
    ctcp_context &ctx = *(ctcp_context *)epOut->ctx;
    interm_out int_out = *(interm_out *)epOut->intermOutput;
    EventProcessorOutput *epOutput = new EventProcessorOutput();
    EPResult r = own_Process(ev, ctx, int_out);
    epOutput->instructions = epOut->instructions;
    epOutput->instructions.insert(epOutput->instructions.end(), r.instrs.begin(), r.instrs.end());
    epOutput->ctx = &ctx;
    epOutput->intermOutput = new interm_out(int_out);
    return epOutput;
}
bool ctcp_connect_ep::isValidEvent(MTEvent *e) {
    return true;
}
EPResult ctcp_connect_ep::own_Process(CTCP_CONNECT &ev, ctcp_context &ctx, interm_out &int_out) {
    EPResult result;
    if (ctx.conn_state == CTCP_CONN_ESTABLISHED) {
        return result;
    }
    if (ctx.conn_state != CTCP_CONN_CLOSED) {
        return result;
    }
    int iss = ctx.tx_byte_buf.send_next();
    if (iss == 0) {
        iss = 1;
    }
    int ibs = ctx.tx_block_buf.send_next();
    if (ibs == 0) {
        ibs = 1;
    }
    ctx.tx_byte_buf.init_seq(iss);
    ctx.tx_block_buf.init_seq(ibs);
    ctx.last_unacked_seq = ctx.seqno_next + 1;
    ctx.currblk = ibs;
    ctx.conn_state = CTCP_CONN_SYN_SENT;

    cout << "ctcp_connect_ep::own_process building syn_instr" << endl;
    PKT_GEN_INSTR *syn_instr = new PKT_GEN_INSTR(ev.flowId);
    syn_instr->bp.seq_num = ctx.seqno_next++;
    syn_instr->bp.blknum = ibs;
    syn_instr->bp.ack = false;
    syn_instr->bp.syn = true;
    syn_instr->bp.numblks = ctx.numblks;
    syn_instr->bp.blksize = ctx.blksize;
    syn_instr->bp.field_size = 256;
    syn_instr->bp.ack_seq = 0;
    syn_instr->bp.ack_blk = 0;
    syn_instr->bp.ack_currdof = 0;
    syn_instr->bp.seed = 0;

    syn_instr->packets_to_send = 0;
    syn_instr->header_only_packet = true;

    cout << "ctcp_connect_ep::own_process calling AttachDefaultCtcpPktGenRules(syn_instr)" << endl;
    AttachDefaultCtcpPktGenRules(syn_instr);

    result.instrs.emplace_back(syn_instr);
    return result;
}

ctcp_send_ep::ctcp_send_ep() {}
ctcp_send_ep::~ctcp_send_ep() {}
EventProcessorOutput *ctcp_send_ep::process(MTEvent *e, EventProcessorOutput *epOut) {
    CTCP_SEND ev = *(CTCP_SEND *)e;
    ctcp_context &ctx = *(ctcp_context *)epOut->ctx;
    interm_out int_out = *(interm_out *)epOut->intermOutput;
    EventProcessorOutput *epOutput = new EventProcessorOutput();
    EPResult r = own_Process(ev, ctx, int_out);
    epOutput->instructions = epOut->instructions;
    epOutput->instructions.insert(epOutput->instructions.end(), r.instrs.begin(), r.instrs.end());
    epOutput->ctx = &ctx;
    epOutput->intermOutput = new interm_out(int_out);
    return epOutput;
}
bool ctcp_send_ep::isValidEvent(MTEvent *e) {
    return true;
}
EPResult ctcp_send_ep::own_Process(CTCP_SEND &ev, ctcp_context &ctx, interm_out &int_out) {
    EPResult result;
    (void)int_out;
    int smss = 1024;

    // Active open: CTCP_CONNECT moves CLOSED -> SYN_SENT. Passive: first CTCP_SYN moves CLOSED ->
    // SYN_RCVD. CTCP_SEND is only valid once that handshake has started.
    if (ctx.conn_state == CTCP_CONN_CLOSED) {
        std::cerr << "CTCP_SEND rejected on flow " << ev.flowId
                  << ": start the connection first (call ReceiveConnectRequest before "
                     "ReceiveAppMessage.)\n";
        return result;
    }

    if (ctx.conn_state != CTCP_CONN_ESTABLISHED) {
        ctx.tx_byte_buf.enqueue_pending(ev.data_size);
        int total_num_packets = std::ceil((float)ev.data_size / smss);
        int total_num_blocks = std::ceil((float)total_num_packets / ctx.blksize);
        ctx.tx_block_buf.enqueue_pending(total_num_blocks);
        std::cout << "ctcp_send_ep::own_Process: total_num_packets=" << total_num_packets
                  << " total_num_blocks=" << total_num_blocks << std::endl;

        return result;
    }

    ctx.tx_byte_buf.enqueue(ev.data_size); // mark data as sendable
    int total_num_packets = std::ceil((float)ev.data_size / smss);
    int total_num_blocks = std::ceil((float)total_num_packets / ctx.blksize);
    std::cout << "ctcp_send_ep::own_Process: total_num_packets=" << total_num_packets
              << " total_num_blocks=" << total_num_blocks << std::endl;
    ctx.tx_block_buf.enqueue(total_num_blocks);

    CtcpEmitSendableSegments(ev.flowId, ctx, result);
    return result;
}

estimate_net_params_ep::estimate_net_params_ep() {}
estimate_net_params_ep::~estimate_net_params_ep() {}
EventProcessorOutput *estimate_net_params_ep::process(MTEvent *e, EventProcessorOutput *epOut) {
    CTCP_ACK ev = *(CTCP_ACK *)e;
    ctcp_context &ctx = *(ctcp_context *)epOut->ctx;
    interm_out int_out = *(interm_out *)epOut->intermOutput;
    EventProcessorOutput *epOutput = new EventProcessorOutput();
    EPResult r = own_Process(ev, ctx, int_out);
    epOutput->instructions = epOut->instructions;
    epOutput->instructions.insert(epOutput->instructions.end(), r.instrs.begin(), r.instrs.end());
    epOutput->ctx = &ctx;
    epOutput->intermOutput = new interm_out(int_out);
    return epOutput;
}
bool estimate_net_params_ep::isValidEvent(MTEvent *e) {
    return true;
}
EPResult estimate_net_params_ep::own_Process(CTCP_ACK &ev, ctcp_context &ctx, interm_out &int_out) {
    if (ctx.conn_state != CTCP_CONN_ESTABLISHED) {
        return {};
    }
    int_out.skip_ack_eps = 0;
    ctx.tlast_ack = (Simulator::Now().GetMicroSeconds());

    // if the packet is old (>1.5RTT) or out of order, we have already assumed
    // that it was lost, so we should not update any parameters using it.
    if (ev.ack_seq < ctx.last_unacked_seq) {
        return {};
    }

    // find time that this packet was sent
    int64_t time_packet_sent = -1;
    for (ctcp_packet_info p_info : ctx.inflight_info) {
        if (p_info.seq_num == ev.ack_seq - 1) {
            time_packet_sent = p_info.sent_time;
            break;
        }
    }
    if (time_packet_sent == -1)
        return {};
    // update RTT
    double rtt = (ctx.tlast_ack - time_packet_sent) / 1000;
    ctx.RTT = rtt;
    if (rtt < ctx.RTT_min) {
        ctx.RTT_min = rtt;
    }

    ctx.currdof = max(ev.ack_currdof, ctx.currdof);
    // update loss rate
    if (ev.ack_seq >= ctx.last_unacked_seq) {
        int losses = ev.ack_seq - ctx.last_unacked_seq;
        ctx.loss_rate = ctx.loss_rate * pow(1 - ctx.smoothing_factor, losses + 1) +
                        (1 - pow(1 - ctx.smoothing_factor, losses));

        if (ctx.loss_rate < 0.001)
            ctx.loss_rate = 0;
    }
    std::cout << "estimate_net_params_ep::own_Process rtt=" << ctx.RTT << " rtt_min=" << ctx.RTT_min
              << " loss_rate=" << ctx.loss_rate << " last_unacked_seq=" << ev.ack_seq + 1
              << " previous last_unacked_seq=" << ctx.last_unacked_seq
              << " tlast_ack=" << ctx.tlast_ack << std::endl;
    // setting     ctx.last_unacked_seq = ev.ack_seq++; happens in the ctcp_ack_net_ep
    return {};
}

congestion_control_ep::congestion_control_ep() {}
congestion_control_ep::~congestion_control_ep() {}
EventProcessorOutput *congestion_control_ep::process(MTEvent *e, EventProcessorOutput *epOut) {
    CTCP_ACK ev = *(CTCP_ACK *)e;
    ctcp_context &ctx = *(ctcp_context *)epOut->ctx;
    interm_out int_out = *(interm_out *)epOut->intermOutput;
    EventProcessorOutput *epOutput = new EventProcessorOutput();
    EPResult r = own_Process(ev, ctx, int_out);
    epOutput->instructions = epOut->instructions;
    epOutput->instructions.insert(epOutput->instructions.end(), r.instrs.begin(), r.instrs.end());
    epOutput->ctx = &ctx;
    epOutput->intermOutput = new interm_out(int_out);
    return epOutput;
}
bool congestion_control_ep::isValidEvent(MTEvent *e) {
    return true;
}
EPResult congestion_control_ep::own_Process(CTCP_ACK &ev, ctcp_context &ctx, interm_out &int_out) {
    if (ctx.conn_state != CTCP_CONN_ESTABLISHED) {
        return {};
    }
    if (int_out.skip_ack_eps) {
        return {};
    }
    if (ev.ack_seq < ctx.ackseq_timeout)
        return {};
    // Should happen in ctcp_ack_timeout_ep?
    // if (Simulator::Now().GetMilliSeconds() - ctx.tlast_ack > ctx.RTO) {
    //      Put into slow start mode
    // }
    if (ctx.tokens < ctx.ssthresh && ctx.stage == 0) {
        // Slow start.
        cout << "Congestion State: In Slow Start" << endl;
        ctx.tokens = ctx.tokens + 1;
        cout << "[TOKENS t=" << Simulator::Now().GetSeconds() << "s flow=" << ev.flowId
             << "] tokens=" << ctx.tokens << " ssthresh=" << ctx.ssthresh << " event=slow_start"
             << endl;
    } else {
        ctx.stage = 1;
        cout << "Congestion State: In Additive Increase" << endl;
        if (ev.ack_seq > ctx.last_unacked_seq) {
            cout << "Out of order CTCP_ACK received. RTT_min=" << ctx.RTT_min << " RTT=" << ctx.RTT
                 << " decrease tokens by factor " << (float)ctx.RTT_min / ctx.RTT << endl;
            ctx.tokens = max((float)(ctx.RTT_min / ctx.RTT * ctx.tokens), (float)1);
        } else {
            // don't increase by more than 1
            ctx.tokens = ctx.tokens + std::min(1 / ctx.tokens, (float)1.0);
        }
        cout << "[TOKENS t=" << Simulator::Now().GetSeconds() << "s flow=" << ev.flowId
             << "] tokens=" << ctx.tokens << " ssthresh=" << ctx.ssthresh << " event=cong_avoidance"
             << endl;
    }
    return {};
}

// Event processor for receiving an CTCP_ACK event from the network layer.
ctcp_ack_net_ep::ctcp_ack_net_ep() {}
ctcp_ack_net_ep::~ctcp_ack_net_ep() {}
EventProcessorOutput *ctcp_ack_net_ep::process(MTEvent *e, EventProcessorOutput *epOut) {
    CTCP_ACK ev = *(CTCP_ACK *)e;
    ctcp_context &ctx = *(ctcp_context *)epOut->ctx;
    interm_out int_out = *(interm_out *)epOut->intermOutput;
    EventProcessorOutput *epOutput = new EventProcessorOutput();
    EPResult r = own_Process(ev, ctx, int_out);
    epOutput->instructions = epOut->instructions;
    epOutput->instructions.insert(epOutput->instructions.end(), r.instrs.begin(), r.instrs.end());
    epOutput->ctx = &ctx;
    epOutput->intermOutput = new interm_out(int_out);
    return epOutput;
}
bool ctcp_ack_net_ep::isValidEvent(MTEvent *e) {
    return true;
}
EPResult ctcp_ack_net_ep::own_Process(CTCP_ACK &ev, ctcp_context &ctx, interm_out &int_out) {
    EPResult result;
    int SMSS = 1024;
    // Three-way handshake's final CTCP_ACK case
    if (ctx.conn_state == CTCP_CONN_SYN_RCVD) {
        // Final CTCP_ACK of the 3-way handshake should acknowledge our CTCP_SYN (ISS+1).
        if (ev.ack_seq == ctx.seqno_next) {
            ctx.numblks = ev.num_blocks;
            ctx.blksize = ev.blksize;
            ctx.decoding_offset = 0;
            ctx.conn_state = CTCP_CONN_ESTABLISHED;

            ctx.tlast_ack = Simulator::Now().GetMilliSeconds();
            // CTCP_SEND any pending CTCP_DATA that the application already had buffered while the
            // connection was establishing. This case could emerge if we are the passive open side
            // and the app called CTCP_SEND before the handshake completed.
            if (ctx.tx_block_buf.has_pending()) {
                const int pending_blocks = ctx.tx_block_buf.drain_pending();
                ctx.tx_block_buf.enqueue(pending_blocks);
                const int pending_bytes = ctx.tx_byte_buf.drain_pending();
                ctx.tx_byte_buf.enqueue(pending_bytes);
                CtcpEmitSendableSegments(ev.flowId, ctx, result);
            }
        }
        return result;
    }
    if (ctx.conn_state != CTCP_CONN_ESTABLISHED) {
        return result;
    }
    if (int_out.skip_ack_eps) {
        return result;
    }
    if (ev.ack_seq < ctx.ackseq_timeout)
        return {};

    // update buffers
    if (ev.ack_currblock > ctx.currblk) {
        int blocks_acked = ev.ack_currblock - ctx.currblk;
        ctx.tx_block_buf.advance_seq(ev.ack_currblock - ctx.currblk);
        ctx.tx_block_buf.ack(ev.ack_currblock);
        ctx.tx_byte_buf.advance_seq(blocks_acked * ctx.blksize * SMSS);
        ctx.tx_byte_buf.ack(ctx.tx_byte_buf.send_una() + blocks_acked * ctx.blksize * SMSS);
        std::cout << "ctcp_ack_net_ep::own_Process: tx_block_buffer._send_next="
                  << ctx.tx_block_buf.send_next()
                  << " tx_block_buffer._data_end=" << ctx.tx_block_buf.data_end() << std::endl;
        ctx.currblk = ev.ack_currblock;
        ctx.currdof = ev.ack_currdof;
        ctx.uncoded_sent_so_far = 0;
    }
    if (ctx.last_unacked_seq <= ev.ack_seq) {
        ctx.last_unacked_seq = ev.ack_seq + 1;
        // prune inflight_info buffer
        const int old_size = ctx.inflight_info.size();
        ctx.inflight_info.erase(remove_if(ctx.inflight_info.begin(), ctx.inflight_info.end(),
                                          [&ctx](const ctcp_packet_info c) {
                                              return c.seq_num < ctx.last_unacked_seq - 1;
                                          }),
                                ctx.inflight_info.end());
        std::cout << "ctcp_ack_net_ep::own_Process: ack advanced! Pruned "
                  << old_size - ctx.inflight_info.size() << " packets from inflight_info."
                  << std::endl;
    }

    // Retransmission timer handling:
    // - Stop if nothing is outstanding.
    // - Restart when block advances (many new cumulative acks), or if no timer is active.
    if (ctx.tx_block_buf.all_acked()) {
        std::cout << "ctcp_ack_net_ep::own_Process: stopping timer, nothing outstanding."
                  << std::endl;
        ctx.ack_timeout.stop();
        return result;
    }

    if (ev.ack_seq >= ((CTCP_MISS_ACK *)ctx.ack_timeout_event)->seq_num &&
        ctx.ack_timeout.isActive()) {
        std::cout << "ctcp_ack_net_ep::own_Process: ACK advanced past timeout, resetting timer."
                  << std::endl;
        if (!ctx.ack_timeout_event) {
            ctx.ack_timeout_event = new CTCP_MISS_ACK(10, ev.flowId);
        }
        CTCP_MISS_ACK *time_ev = (CTCP_MISS_ACK *)ctx.ack_timeout_event;
        time_ev->flowId = ev.flowId;
        time_ev->time = 10;
        time_ev->seq_num = ctx.seqno_next;
        ctx.ack_timeout.set_duration(ctx.RTO);
        ctx.ack_timeout.start(time_ev);
    }
    // Try to send data packets since we should have more tokens available to us. Also starts a new
    // timer
    CtcpEmitSendableSegments(ev.flowId, ctx, result);

    return result;
}

ctcp_data_net_ep::ctcp_data_net_ep() {}
ctcp_data_net_ep::~ctcp_data_net_ep() {}
EventProcessorOutput *ctcp_data_net_ep::process(MTEvent *e, EventProcessorOutput *epOut) {
    CTCP_DATA ev = *(CTCP_DATA *)e;
    ctcp_context &ctx = *(ctcp_context *)epOut->ctx;
    interm_out int_out = *(interm_out *)epOut->intermOutput;
    EventProcessorOutput *epOutput = new EventProcessorOutput();
    EPResult r = own_Process(ev, ctx, int_out);
    epOutput->instructions = epOut->instructions;
    epOutput->instructions.insert(epOutput->instructions.end(), r.instrs.begin(), r.instrs.end());
    epOutput->ctx = new ctcp_context(ctx);
    epOutput->intermOutput = new interm_out(int_out);
    return epOutput;
}
bool ctcp_data_net_ep::isValidEvent(MTEvent *e) {
    return true;
}
EPResult ctcp_data_net_ep::own_Process(CTCP_DATA &ev, ctcp_context &ctx, interm_out &int_out) {
    if (ctx.conn_state != CTCP_CONN_ESTABLISHED) {
        return {};
    }
    int SMSS = 1024;
    int currblk = ev.blknum;
    if (currblk < ctx.decoding_currblk) {
        // The block for this packet has already been decoded. It is stale.
        return {};
    }
    // Online Gaussian Elimination.
    // 1. Generate coding coefficients
    auto before = std::chrono::high_resolution_clock::now();

    std::vector<gf256::gf> c;
    if (ev.uncoded >= 0) {
        for (int i = 0; i < ctx.blksize; i++) {
            c.push_back(0);
        }
        c[ev.uncoded] = 1;
    } else {
        std::srand(ev.seed);
        for (int i = 0; i < ctx.blksize; i++) {
            // Do not allow for zero coefficients.
            c.push_back(std::rand() % 255 + 1);
        }
    }
    // 2. Get payload (and convert to GF256)
    std::vector<gf256::gf> p;
    int j = 0;
    for (int i = 0; i < min((int)p.size(), SMSS); i++, j++) {
        p.push_back(ev.payload[i]);
    }
    for (; j < SMSS; j++) {
        p.push_back((uint8_t)0);
    }
    auto after = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = after - before;
    ctx.coding_delay += duration.count();

    int blk_index = (currblk - ctx.decoding_currblk + ctx.decoding_offset) % ctx.numblks;
    assert(0 <= blk_index && blk_index < ctx.numblks);
    before = std::chrono::high_resolution_clock::now();
    bool is_independent =
        OnlineGaussianEliminationInsert(ctx.coding_arr[blk_index], ctx.payload_arr[blk_index], c, p,
                                        ctx.blksize, SMSS, ctx.rank_arr[blk_index]);
    after = std::chrono::high_resolution_clock::now();
    duration = after - before;
    std::cout << "ctcp_data_net_ep::own_Process: OnlineGaussianEliminationInsert consumed "
              << duration.count() << "ms." << std::endl;
    ctx.coding_delay += duration.count();
    // Update context if needed.
    ctx.rank_arr[blk_index] += is_independent;
    ctx.ack_currdof += is_independent;
    return {};
}

ctcp_syn_net_ep::ctcp_syn_net_ep() {}
ctcp_syn_net_ep::~ctcp_syn_net_ep() {}
EventProcessorOutput *ctcp_syn_net_ep::process(MTEvent *e, EventProcessorOutput *epOut) {
    CTCP_SYN ev = *(CTCP_SYN *)e;
    ctcp_context &ctx = *(ctcp_context *)epOut->ctx;
    interm_out int_out = *(interm_out *)epOut->intermOutput;
    EventProcessorOutput *epOutput = new EventProcessorOutput();
    EPResult r = own_Process(ev, ctx, int_out);
    epOutput->instructions = epOut->instructions;
    epOutput->instructions.insert(epOutput->instructions.end(), r.instrs.begin(), r.instrs.end());
    epOutput->ctx = &ctx;
    epOutput->intermOutput = new interm_out(int_out);
    return epOutput;
}
bool ctcp_syn_net_ep::isValidEvent(MTEvent *e) {
    return true;
}
EPResult ctcp_syn_net_ep::own_Process(CTCP_SYN &ev, ctcp_context &ctx, interm_out &int_out) {
    EPResult result;
    if (ctx.conn_state == CTCP_CONN_ESTABLISHED) {
        return result;
    }

    // Record the peer's ISN and advertised receive window.
    ctx.decoding_currblk = ev.blknum;
    ctx.numblks = ev.numblks;
    ctx.blksize = ev.blksize;
    ctx.ackseq_next = ++ev.seq_num;
    std::cout << "ctcp_syn_net_ep::own_Process decoding_currblk=" << ctx.decoding_currblk
              << " numblks=" << ctx.numblks << " blksize=" << ctx.blksize
              << " received blknum=" << ev.blknum << std::endl;

    // If we're already in SYN_SENT / SYN_RCVD, preserve our existing ISS to avoid corrupting the
    // local sequence space on duplicate SYNs or simultaneous open.
    int iss = 0;
    if (ctx.conn_state == CTCP_CONN_SYN_SENT || ctx.conn_state == CTCP_CONN_SYN_RCVD) {
        iss = ctx.seqno_next;
    }
    if (iss == 0) {
        iss = ctx.seqno_next;
        if (iss == 0) {
            iss = 1;
        }
    }
    ctx.seqno_next = iss;
    ctx.last_unacked_seq = iss + 1;
    ctx.conn_state = CTCP_CONN_SYN_RCVD;

    PKT_GEN_INSTR *syn_ack_instr = new PKT_GEN_INSTR(ev.flowId);
    syn_ack_instr->bp.seq_num = ctx.seqno_next++;
    syn_ack_instr->bp.blknum = ctx.currblk;
    syn_ack_instr->bp.blksize = ctx.blksize;
    syn_ack_instr->bp.numblks = ctx.numblks;
    syn_ack_instr->packets_to_send = 0;
    syn_ack_instr->header_only_packet = true;
    syn_ack_instr->bp.ack = true;
    syn_ack_instr->bp.syn = true;
    syn_ack_instr->bp.ack_seq = ctx.ackseq_next++;
    syn_ack_instr->bp.ack_blk = ctx.decoding_currblk;
    AttachDefaultCtcpPktGenRules(syn_ack_instr);
    result.instrs.emplace_back(syn_ack_instr);
    return result;
}

ctcp_syn_ack_net_ep::ctcp_syn_ack_net_ep() {}
ctcp_syn_ack_net_ep::~ctcp_syn_ack_net_ep() {}
EventProcessorOutput *ctcp_syn_ack_net_ep::process(MTEvent *e, EventProcessorOutput *epOut) {
    CTCP_SYN_ACK ev = *(CTCP_SYN_ACK *)e;
    ctcp_context &ctx = *(ctcp_context *)epOut->ctx;
    interm_out int_out = *(interm_out *)epOut->intermOutput;
    EventProcessorOutput *epOutput = new EventProcessorOutput();
    EPResult r = own_Process(ev, ctx, int_out);
    epOutput->instructions = epOut->instructions;
    epOutput->instructions.insert(epOutput->instructions.end(), r.instrs.begin(), r.instrs.end());
    epOutput->ctx = &ctx;
    epOutput->intermOutput = new interm_out(int_out);
    return epOutput;
}
bool ctcp_syn_ack_net_ep::isValidEvent(MTEvent *e) {
    return true;
}
EPResult ctcp_syn_ack_net_ep::own_Process(CTCP_SYN_ACK &ev, ctcp_context &ctx,
                                          interm_out &int_out) {
    EPResult result;
    // Only active openers (SYN_SENT) and simultaneous-open peers (SYN_RCVD) process
    // CTCP_SYN-CTCP_ACK. Reject in ESTABLISHED and all other states; the expected_ack check below
    // also guards this.
    if (ctx.conn_state != CTCP_CONN_SYN_SENT && ctx.conn_state != CTCP_CONN_SYN_RCVD) {
        return result;
    }

    // Validate that the CTCP_SYN-CTCP_ACK acknowledges our CTCP_SYN (ISS+1).
    if (ctx.seqno_next > 0 && ev.ack_seq != ctx.seqno_next) {
        return result;
    }
    ctx.decoding_currblk = ev.send_currblk;
    ctx.ackseq_next = ++ev.seq_num;
    ctx.conn_state = CTCP_CONN_ESTABLISHED;

    PKT_GEN_INSTR *ack_instr = new PKT_GEN_INSTR(ev.flowId);
    ack_instr->bp.seq_num = ctx.seqno_next;
    ack_instr->packets_to_send = 0;
    ack_instr->header_only_packet = true;
    ack_instr->bp.ack = true;
    ack_instr->bp.syn = false;
    ack_instr->bp.ack_seq = ctx.ackseq_next++;
    ack_instr->bp.numblks = ctx.numblks;
    ack_instr->bp.blksize = ctx.blksize;

    ctx.last_unacked_seq = ctx.seqno_next + 1;

    AttachDefaultCtcpPktGenRules(ack_instr);
    result.instrs.emplace_back(ack_instr);

    // If the application already had CTCP_DATA pending from a CTCP_SEND event while the connection
    // was being established, mark it as sendable now that the connection is established, and
    // CTCP_SEND it. Note: This does NOT piggyback the CTCP_DATA on the CTCP_SYN-CTCP_ACK CTCP_ACK
    // PKT_GEN_INSTR above.
    if (ctx.tx_block_buf.has_pending()) {
        int pending = ctx.tx_block_buf.drain_pending();
        ctx.tx_block_buf.enqueue(pending);
        pending = ctx.tx_byte_buf.drain_pending();
        ctx.tx_byte_buf.enqueue(pending);
        CtcpEmitSendableSegments(ev.flowId, ctx, result);
    }
    return result;
}

ctcp_send_ack_ep::ctcp_send_ack_ep() {}
ctcp_send_ack_ep::~ctcp_send_ack_ep() {}
EventProcessorOutput *ctcp_send_ack_ep::process(MTEvent *e, EventProcessorOutput *epOut) {
    CTCP_DATA ev = *(CTCP_DATA *)e;
    ctcp_context &ctx = *(ctcp_context *)epOut->ctx;
    interm_out int_out = *(interm_out *)epOut->intermOutput;
    EventProcessorOutput *epOutput = new EventProcessorOutput();
    EPResult r = own_Process(ev, ctx, int_out);
    epOutput->instructions = epOut->instructions;
    epOutput->instructions.insert(epOutput->instructions.end(), r.instrs.begin(), r.instrs.end());
    epOutput->ctx = &ctx;
    epOutput->intermOutput = new interm_out(int_out);
    return epOutput;
}
bool ctcp_send_ack_ep::isValidEvent(MTEvent *e) {
    return true;
}
EPResult ctcp_send_ack_ep::own_Process(CTCP_DATA &ev, ctcp_context &ctx, interm_out &int_out) {
    EPResult result;
    if (ctx.conn_state != CTCP_CONN_ESTABLISHED) {
        return result;
    }
    PKT_GEN_INSTR *ack_instr = new PKT_GEN_INSTR(ev.flowId);
    // Pure CTCP_ACK by deafault; no CTCP_DATA piggbacking.
    ack_instr->bp.seq_num = ctx.seqno_next;
    ack_instr->bp.blknum = ctx.currblk;
    ack_instr->bp.numblks = ctx.numblks;
    ack_instr->bp.blksize = ctx.blksize;
    ack_instr->bp.ack = 1;
    ack_instr->bp.syn = 0;
    ack_instr->bp.ack_seq = ev.seq_num + 1;

    ctx.ackseq_next = max(ev.seq_num + 1, ctx.ackseq_next);

    ack_instr->header_only_packet = true;
    ack_instr->packets_to_send = 1;
    // Find first in rank array which isn't max rank
    int blocks_advanced = 0;
    for (int i = 0; i < ctx.numblks; blocks_advanced++, i++) {
        int idx = (ctx.decoding_offset + i) % ctx.numblks;
        assert(0 <= idx && idx < ctx.numblks);
        if (ctx.rank_arr[idx] < ctx.blksize) {
            break;
        }
    }
    ack_instr->bp.ack_blk = ctx.decoding_currblk + blocks_advanced;
    std::cout << "ctcp_send_ack_ep::own_Process ctx.decoding_currblk=" << ctx.decoding_currblk
              << " blocks_advanced=" << blocks_advanced
              << " ack_instr->bp.ack_blk=" << ctx.decoding_currblk + blocks_advanced << std::endl;
    int idx = (ctx.decoding_offset + blocks_advanced) % ctx.numblks;
    assert(0 <= idx && idx < ctx.numblks);
    ack_instr->bp.ack_currdof = ctx.rank_arr[idx];
    AttachDefaultCtcpPktGenRules(ack_instr);
    result.instrs.emplace_back(ack_instr);
    return result;
}

ctcp_app_feedback_ep::ctcp_app_feedback_ep() {}
ctcp_app_feedback_ep::~ctcp_app_feedback_ep() {}
EventProcessorOutput *ctcp_app_feedback_ep::process(MTEvent *e, EventProcessorOutput *epOut) {
    CTCP_DATA ev = *(CTCP_DATA *)e;
    ctcp_context &ctx = *(ctcp_context *)epOut->ctx;
    interm_out int_out = *(interm_out *)epOut->intermOutput;
    EventProcessorOutput *epOutput = new EventProcessorOutput();
    EPResult r = own_Process(ev, ctx, int_out);
    epOutput->instructions = epOut->instructions;
    epOutput->instructions.insert(epOutput->instructions.end(), r.instrs.begin(), r.instrs.end());
    epOutput->ctx = &ctx;
    epOutput->intermOutput = new interm_out(int_out);
    return epOutput;
}
bool ctcp_app_feedback_ep::isValidEvent(MTEvent *e) {
    return true;
}
EPResult ctcp_app_feedback_ep::own_Process(CTCP_DATA &ev, ctcp_context &ctx, interm_out &int_out) {
    EPResult result;
    if (ctx.conn_state != CTCP_CONN_ESTABLISHED) {
        return result;
    }
    // Find how many blocks were advanced
    int SMSS = 1024;
    int blocks_advanced = 0;
    for (int i = 0; i < ctx.numblks; blocks_advanced++, i++) {
        int idx = (ctx.decoding_offset + i) % ctx.numblks;
        assert(0 <= idx && idx < ctx.numblks);
        if (ctx.rank_arr[idx] < ctx.blksize) {
            break;
        }
        std::cout << "ctcp_app_feedback_ep::own_Process: block number " << ctx.decoding_currblk + i
                  << " advanced with rank " << ctx.rank_arr[idx] << std::endl;
        ctx.rank_arr[idx] = 0;
    }
    // Decode payload array!
    double total_time = 0.0;
    for (int i = 0; i < blocks_advanced; i++) {
        int idx = (ctx.decoding_offset + i) % ctx.numblks;
        assert(0 <= idx && idx < ctx.numblks);
        auto before = std::chrono::high_resolution_clock::now();
        solve_upper_triangular(ctx.coding_arr[idx], ctx.payload_arr[idx], ctx.blksize, SMSS);
        auto after = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = after - before;
        total_time += duration.count();
        // Since this is a simulation, I can immediately reset the arrays and update context params.
        // Otherwise, here you would extract the original packets and CTCP_SEND to the app.
        for (int r = 0; r < ctx.blksize; r++) {
            for (int c = 0; c < ctx.blksize; c++) {
                ctx.coding_arr[idx][ctx.blksize * r + c] = 0;
            }
        }
        // Don't need to clear payload array because it just gets overwritten anyway
    }
    ctx.coding_delay += total_time;

    std::cout << "ctcp_app_feedback_ep::own_Process: solve_upper_triangular consumed " << total_time
              << " ms. Total coding delay so far=" << ctx.coding_delay << " ms." << std::endl;
    // update context params
    ctx.decoding_offset = (ctx.decoding_offset + blocks_advanced) % ctx.numblks;
    assert(0 <= ctx.decoding_offset && ctx.decoding_offset < ctx.numblks);
    ctx.ack_currdof = ctx.rank_arr[ctx.decoding_offset];
    ctx.decoding_currblk = ctx.decoding_currblk + blocks_advanced;

    // CTCP_SEND feedback instruction if a block was advanced.
    if (blocks_advanced > 0) {
        FB_INSTR *fb_instr = new FB_INSTR(ev.flowId);
        fb_instr->seq_num = ev.seq_num;
        fb_instr->blknum = ctx.decoding_currblk;
        fb_instr->data_len = blocks_advanced * ctx.blksize * SMSS;
        result.instrs.emplace_back(fb_instr);
        return result;
    }
    return {};
}

ctcp_ack_timeout_ep::ctcp_ack_timeout_ep() {}
ctcp_ack_timeout_ep::~ctcp_ack_timeout_ep() {}
EventProcessorOutput *ctcp_ack_timeout_ep::process(MTEvent *e, EventProcessorOutput *epOut) {
    CTCP_MISS_ACK ev = *(CTCP_MISS_ACK *)e;
    ctcp_context &ctx = *(ctcp_context *)epOut->ctx;
    interm_out int_out = *(interm_out *)epOut->intermOutput;
    EventProcessorOutput *epOutput = new EventProcessorOutput();
    EPResult r = own_Process(ev, ctx, int_out);
    epOutput->instructions = epOut->instructions;
    epOutput->instructions.insert(epOutput->instructions.end(), r.instrs.begin(), r.instrs.end());
    epOutput->ctx = &ctx;
    epOutput->intermOutput = new interm_out(int_out);
    return epOutput;
}
bool ctcp_ack_timeout_ep::isValidEvent(MTEvent *e) {
    return true;
}
EPResult ctcp_ack_timeout_ep::own_Process(CTCP_MISS_ACK &ev, ctcp_context &ctx,
                                          interm_out &int_out) {
    EPResult result;
    ctx.stage = 0;
    // Nothing outstanding: don't retransmit and don't keep re-arming the timer.
    if (ctx.seqno_next == ctx.last_unacked_seq &&
        ctx.tx_block_buf.data_end() <= ctx.tx_block_buf.send_una()) {
        cout << "ctcp_ack_timeout_ep: t=" << Simulator::Now().GetSeconds() << "s"
             << " flow=" << ev.flowId << " no-outstanding-CTCP_DATA"
             << " (send_una=" << ctx.tx_block_buf.send_una()
             << " send_next=" << ctx.tx_block_buf.send_next()
             << " data_end=" << ctx.tx_block_buf.data_end() << ") -> stop timer" << endl;
        ctx.ack_timeout.stop();
        return result;
    }

    // RFC 5681: ssthresh = max(FlightSize/2, 2*SMSS) measured BEFORE cwnd is reset.
    // Use max(cwnd, in_flight) — in_flight() can be small at timeout time if
    // many ACKs arrived before the timer fired, which would give a wrong ssthresh.
    int flightsize = floor(max(ctx.tokens, (float)ctx.inflight_info.size()));
    int opt1 = flightsize / 2;
    int opt2 = 2;
    ctx.ssthresh = (opt1 > opt2) ? opt1 : opt2;
    ctx.tokens = 3;
    cout << "[TOKENS t=" << Simulator::Now().GetSeconds() << "s flow=" << ev.flowId
         << "] tokens=" << ctx.tokens << " ssthresh=" << ctx.ssthresh << " event=timeout" << endl;

    ctx.ackseq_timeout = ctx.seqno_next;
    ctx.inflight_info.clear();
    int packets_to_send = ctx.tokens - ctx.inflight_info.size();
    if (packets_to_send <= 0) {
        cout << "ctcp_ack_timeout_ep: t=" << Simulator::Now().GetSeconds() << "s"
             << " flow=" << ev.flowId << " computed-packets_to_send=0"
             << " (send_una=" << ctx.tx_block_buf.send_una()
             << " send_next=" << ctx.tx_block_buf.send_next()
             << " data_end=" << ctx.tx_block_buf.data_end() << ") -> stop timer" << endl;
        ctx.ack_timeout.stop();
        return result;
    }

    cout << "ctcp_ack_timeout_ep: t=" << Simulator::Now().GetSeconds() << "s"
         << " flow=" << ev.flowId << " retransmit seq=" << ctx.tx_block_buf.send_una()
         << " len=" << packets_to_send << " RTO(ms)=" << ctx.RTO << endl;

    // Exponential backoff; cap at max_rto per RFC 6298 §2.5.
    ctx.RTO = (int)(1.25 * ctx.RTT);
    if (ctx.RTO > ctx.max_rto) {
        ctx.RTO = ctx.max_rto;
    }
    ctx.ack_timeout.restart(ctx.RTO);
    CtcpEmitSendableSegments(ev.flowId, ctx, result);
    return result;
}
