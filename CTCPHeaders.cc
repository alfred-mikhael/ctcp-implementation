#include "CTCPHeaders.h"

using namespace ns3;

TypeId CTCPheader::GetTypeId() {
    static TypeId tid = TypeId("ns3::CTCPheader")
                            .SetParent<MTHeader>()
                            .SetGroupName("ModularTransport")
                            .AddConstructor<CTCPheader>();
    return tid;
}

uint32_t CTCPheader::GetSerializedSize() const {
    // Size is in bytes (12 32-bit words = 28 bytes).
    return 13 * 4;
}
void CTCPheader::Serialize(Buffer::Iterator start) const {
    Buffer::Iterator i = start;
    i.WriteHtonU32(src_port);
    i.WriteHtonU32(dest_port);
    i.WriteHtonU32(seq);
    i.WriteHtonU32(blknum);
    i.WriteHtonU32(ack_seq);
    i.WriteHtonU32(ack_blk);
    i.WriteHtonU32(ack_currdof);
    i.WriteHtonU32(ack);
    i.WriteHtonU32(syn);
    i.WriteHtonU32(numblks);
    i.WriteHtonU32(blksize);
    i.WriteHtonU32(seed);
    i.WriteHtonU32(uncoded);
}
uint32_t CTCPheader::Deserialize(Buffer::Iterator start) {
    Buffer::Iterator i = start;
    src_port = i.ReadNtohU32();
    dest_port = i.ReadNtohU32();
    seq = i.ReadNtohU32();
    blknum = i.ReadNtohU32();
    ack_seq = i.ReadNtohU32();
    ack_blk = i.ReadNtohU32();
    ack_currdof = i.ReadNtohU32();
    ack = i.ReadNtohU32();
    syn = i.ReadNtohU32();
    numblks = i.ReadNtohU32();
    blksize = i.ReadNtohU32();
    seed = i.ReadNtohU32();
    uncoded = i.ReadNtohU32();

    return GetSerializedSize();
}
void CTCPheader::Print(std::ostream &os) const {}
TypeId CTCPheader::GetInstanceTypeId() const {
    return GetTypeId();
}