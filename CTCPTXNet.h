#ifndef CTCP_TX_NET_H
#define CTCP_TX_NET_H

#include "CTCPInstructions.h"
#include "ns3/mt-txnet.h"
#include "ns3/ptr.h"
#include <vector>

/**
 * @brief Realizes CTCP packet-generation instructions into ns-3 packets.
 */
class CTCPTXNet : public ns3::MTTXNetScheduler {
public:
    CTCPTXNet() {}
    /**
     * @brief Build one or more packets for a CTCP transmit instruction.
     */
    std::vector<ns3::Ptr<ns3::Packet>> get_next_packets(ns3::MTInstruction *instr) override;
};

#endif