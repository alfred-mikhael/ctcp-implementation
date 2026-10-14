#ifndef CTCP_EVENTS_H
#define CTCP_EVENTS_H

#include "ns3/mt-event.h"

class CTCP_CONNECT : public ns3::MTEvent {
public:
    CTCP_CONNECT(long time, flow_id fid)
        : ns3::MTEvent(ns3::EventType::INCOMING, ns3::EventSubtype::APP_EVENT, time, fid) {}
    void getType() override {}
};

class CTCP_SEND : public ns3::MTEvent {
public:
    int data_size;
    CTCP_SEND(long time, flow_id fid)
        : ns3::MTEvent(ns3::EventType::INCOMING, ns3::EventSubtype::APP_EVENT, time, fid) {}
    void getType() override {}
};
class CTCP_ACK : public ns3::MTEvent {
public:
    // CTCP_ACK for the sequence number
    int ack_seq;
    // ACKs for the current block and DOF
    int ack_currblock;
    int ack_currdof;
    // number of blocks which receiver can accept at once
    int num_blocks;
    int blksize;
    CTCP_ACK(long time, flow_id fid)
        : ns3::MTEvent(ns3::EventType::INCOMING, ns3::EventSubtype::NET_EVENT, time, fid) {}
    void getType() override {}
};
class CTCP_DATA : public ns3::MTEvent {
public:
    int data_len;
    int seq_num;
    // seed to be used for PRNG
    int seed;
    // Which block this is a part of
    int blknum;
    int uncoded;
    uint8_t *payload;
    CTCP_DATA(long time, flow_id fid)
        : ns3::MTEvent(ns3::EventType::INCOMING, ns3::EventSubtype::NET_EVENT, time, fid) {}
    ~CTCP_DATA() {}
    void getType() override {}
};
class CTCP_MISS_ACK : public ns3::MTEvent {
public:
    int seq_num;
    CTCP_MISS_ACK(long time, flow_id fid)
        : ns3::MTEvent(ns3::EventType::NONE, ns3::EventSubtype::TIMER_EVENT, time, fid) {}
    void getType() override {}
};

class CTCP_SYN : public ns3::MTEvent {
public:
    int seq_num;
    int blknum;
    int numblks;
    int blksize;
    CTCP_SYN(long time, flow_id fid)
        : ns3::MTEvent(ns3::EventType::INCOMING, ns3::EventSubtype::NET_EVENT, time, fid) {}
    void getType() override {}
};
class CTCP_SYN_ACK : public ns3::MTEvent {
public:
    int seq_num;
    int ack_seq;
    int numblks;
    int blksize;

    int ack_currblock;
    int send_currblk;
    CTCP_SYN_ACK(long time, flow_id fid)
        : ns3::MTEvent(ns3::EventType::INCOMING, ns3::EventSubtype::NET_EVENT, time, fid) {}
    void getType() override {}
};

#endif