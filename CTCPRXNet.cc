#include "CTCPRXNet.h"
#include "CTCPEvents.h"
#include "CTCPHeaders.h"
#include <random>
#include <vector>

using namespace std;
using namespace ns3;

std::vector<ns3::MTEvent *> CTCPRXNetParser::packet_parser(ns3::Ipv4Header iphdr,
                                                           ns3::Ptr<ns3::Packet> pkt) {
    vector<MTEvent *> events;
    CTCPheader ctcphdr;
    GetPointer(pkt)->PeekHeader(ctcphdr);

    // Derive a canonical flow_id from the IP-address pair.
    // The flow_id constructor normalizes the ordering so that both
    // directions of a connection produce the same identifier.
    flow_id fid(iphdr.GetSource().Get(), iphdr.GetDestination().Get());

    // SYN / SYN-CTCP_ACK handling (TCP Handshake).
    if (ctcphdr.syn == 1) {
        if (ctcphdr.ack == 1) {
            std::cout << "RX Parser: Parsing SYN-ACK packet: seq=" << ctcphdr.seq
                      << " ack_seq=" << ctcphdr.ack_seq << " ack_blk=" << ctcphdr.ack_blk
                      << " blknum=" << ctcphdr.blknum << " syn=" << ctcphdr.syn
                      << " ack_flag=" << ctcphdr.ack << " blksize=" << ctcphdr.blksize
                      << " numblks=" << ctcphdr.numblks << " (flowId=" << fid << ")" << std::endl;
            CTCP_SYN_ACK *syn_ack_ev = new CTCP_SYN_ACK(10, fid);
            syn_ack_ev->seq_num = ctcphdr.seq;
            syn_ack_ev->ack_seq = ctcphdr.ack_seq;
            syn_ack_ev->numblks = ctcphdr.numblks;
            syn_ack_ev->blksize = ctcphdr.blksize;
            syn_ack_ev->send_currblk = ctcphdr.blknum;
            syn_ack_ev->ack_currblock = ctcphdr.ack_blk;
            events.push_back(syn_ack_ev);
            return events;
        }

        std::cout << "RX Parser: Parsing CTCP_SYN packet: seq=" << ctcphdr.seq
                  << " blknum=" << ctcphdr.blknum << " syn=" << ctcphdr.syn
                  << " numblks=" << ctcphdr.numblks << " blksize=" << ctcphdr.blksize
                  << " (flowId=" << fid << ")" << std::endl;
        CTCP_SYN *syn_ev = new CTCP_SYN(10, fid);
        syn_ev->seq_num = ctcphdr.seq;
        syn_ev->blksize = ctcphdr.blksize;
        syn_ev->numblks = ctcphdr.numblks;
        syn_ev->blknum = ctcphdr.blknum;
        events.push_back(syn_ev);
        return events;
    }

    if (ctcphdr.ack == 0) {
        std::cout << "RX Parser: Parsing CTCP_DATA Packet of Sequence " << ctcphdr.seq
                  << " blknum=" << ctcphdr.blknum << " seed=" << ctcphdr.seed
                  << " uncoded=" << ctcphdr.uncoded << " (flowId=" << fid << ")" << std::endl;
        CTCP_DATA *data_ev = new CTCP_DATA(10, fid);
        data_ev->seq_num = ctcphdr.seq;
        data_ev->blknum = ctcphdr.blknum;
        data_ev->seed = ctcphdr.seed;
        data_ev->uncoded = ctcphdr.uncoded;
        data_ev->data_len = GetPointer(pkt)->GetSize() - ctcphdr.GetSerializedSize();
        data_ev->payload = new uint8_t[data_ev->data_len];
        GetPointer(pkt)->CopyData(data_ev->payload, data_ev->data_len);
        events.push_back(data_ev);
    }

    if (ctcphdr.ack == 1) {
        std::cout << "RX Parser: Parsing CTCP_ACK packet: seq=" << ctcphdr.seq
                  << " ack_seq=" << ctcphdr.ack_seq << " ack_blk=" << ctcphdr.ack_blk
                  << " ack_flag=" << ctcphdr.ack << " ack_dof=" << ctcphdr.ack_currdof
                  << " numblks=" << ctcphdr.numblks << " blksize=" << ctcphdr.blksize
                  << " (flowId=" << fid << ")" << std::endl;
        CTCP_ACK *ack_ev = new CTCP_ACK(10, fid);
        ack_ev->ack_seq = ctcphdr.ack_seq;
        ack_ev->ack_currblock = ctcphdr.ack_blk;
        ack_ev->ack_currdof = ctcphdr.ack_currdof;
        ack_ev->blksize = ctcphdr.blksize;
        ack_ev->num_blocks = ctcphdr.numblks;
        events.push_back(ack_ev);
    }
    return events;
}
