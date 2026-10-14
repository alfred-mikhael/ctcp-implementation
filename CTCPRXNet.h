#ifndef CTCP_RX_NET_H
#define CTCP_RX_NET_H

#include "ns3/mt-rxnet.h"

#include <vector>

/**
 * @brief Parses inbound CTCP packets into internal transport events.
 */
class CTCPRXNetParser : public ns3::MTRXNetParser {
public:
    CTCPRXNetParser() {}

    /**
     * @brief Decode packet/header fields and emit CTCP events.
     */
    std::vector<ns3::MTEvent *> packet_parser(ns3::Ipv4Header iphdr, ns3::Ptr<ns3::Packet> pkt);
};

#endif