#ifndef CTCP_INSTRUCTIONS_H
#define CTCP_INSTRUCTIONS_H

#include "CTCPHeaders.h"
#include "ns3/mt-instruction.h"
#include "ns3/mt-pkt-gen.h"

#include <functional>

/**
 *  @brief CTCP packet-generation instruction (pkt_gen, §5.2).
 *
 * A single PKT_GEN_INSTR represents one pkt_gen call from a CTCP event
 * processor. CTCPTXNet::get_next_packets() realises it into one or more
 * concrete ns3::Packet objects, segmenting data bursts at smss boundaries.
 */
class PKT_GEN_INSTR : public ns3::MTInstruction {
public:
    struct CTCP_BLUEPRINT {
        int src_port{1};
        int dest_port{2};
        int seq_num{0};
        int blknum{0};
        bool ack{false};
        bool syn{false};
        int ack_seq{0};
        int ack_blk{0};
        int ack_currdof{0};
        int numblks{0};
        int blksize{0};
        uint32_t seed{0};
        int field_size{256};
        int uncoded{-1};
    };

    int packets_to_send{0}; ///< total units for pkt-gen (TCP uses bytes)
    bool header_only_packet{
        false};     ///< explicit request to emit a header-only packet when bytes_to_send==0
    int smss{1024}; ///< max segment size (in bytes); used by the TCP segmentation rule
    CTCP_BLUEPRINT bp;

    using SegmentationRule = ns3::PktGenSegmentPlanner<PKT_GEN_INSTR>;
    using HeaderRule = ns3::PktGenHeaderWriter<PKT_GEN_INSTR, CTCPheader>;
    using PayloadBuilderHook = ns3::PktGenPayloadBuilder<PKT_GEN_INSTR>;
    using PacketFinalizerHook = ns3::PktGenPacketFinalizer<PKT_GEN_INSTR>;

    SegmentationRule segmentation_rule;
    HeaderRule header_rule;
    PayloadBuilderHook payload_builder_hook;
    PacketFinalizerHook packet_finalizer_hook;

    explicit PKT_GEN_INSTR(flow_id f) : ns3::MTInstruction(f, ns3::TXNET_INSTR) {}
};

/**
 *  @brief TCP app-feedback instruction delivered through TXAPP_INSTR.
 */
class FB_INSTR : public ns3::MTInstruction {
public:
    int seq_num{0};
    int blknum{0};
    int data_len{0};

    explicit FB_INSTR(flow_id f) : ns3::MTInstruction(f, ns3::TXAPP_INSTR) {}
};

#endif /* CTCP_INSTRUCTIONS_H */
