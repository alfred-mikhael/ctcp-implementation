#pragma once
#include "ns3/mt-header.h"

class CTCPheader : public ns3::MTHeader {
public:
    static ns3::TypeId GetTypeId();
    int src_port;
    int dest_port;
    int seq;
    int blknum;
    int ack_seq;
    int ack_blk;
    int ack_currdof;
    int ack;
    int syn;
    int numblks;
    int blksize;
    int seed;
    int uncoded;
    uint32_t GetSerializedSize() const;
    void Serialize(ns3::Buffer::Iterator start) const;
    uint32_t Deserialize(ns3::Buffer::Iterator start);
    void Print(std::ostream &os) const;
    ns3::TypeId GetInstanceTypeId() const;
};