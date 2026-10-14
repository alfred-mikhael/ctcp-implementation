#include "CTCPTXNet.h"
#include "CTCPHeaders.h"
#include "ns3/mt-pkt-gen.h"
#include "ns3/ptr.h"

#include <iostream>

using namespace ns3;

namespace {

PktGenPayloadResult BuildCtcpPayload(const PktGenSegmentContext &ctx,
                                     const PktGenSegmentDecision &decision,
                                     const PKT_GEN_INSTR &instr) {
    int SMSS = 1024;
    if (instr.payload_builder_hook) {
        return instr.payload_builder_hook(ctx, decision, instr);
    }
    if (decision.units <= 0) {
        return MakeNoPayload();
    }
    return MakePaddingPayload(decision.units * SMSS);
}

} // namespace

std::vector<Ptr<Packet>> CTCPTXNet::get_next_packets(MTInstruction *instr) {
    auto *ctcp_instr = dynamic_cast<PKT_GEN_INSTR *>(instr);
    if (!ctcp_instr) {
        std::cout << "CTCPTXNet: unsupported instruction type" << std::endl;
        return {};
    }

    return RealizePktGenInstruction<PKT_GEN_INSTR, CTCPheader>(
        *ctcp_instr, 1024, ctcp_instr->segmentation_rule, ctcp_instr->header_rule, BuildCtcpPayload,
        ctcp_instr->packet_finalizer_hook);
}
