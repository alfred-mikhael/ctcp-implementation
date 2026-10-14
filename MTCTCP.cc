#include "MTCTCP.h"
#include "CTCPContext.h"
#include "CTCPDispatcher.h"
#include "CTCPEvents.h"
#include "CTCPRXApp.h"
#include "CTCPRXNet.h"
#include "CTCPStructs.h"
#include "CTCPTXApp.h"
#include "CTCPTXNet.h"
#include <fstream>

using namespace ns3;

MTCTCP::MTCTCP() {
    // Wire CTCP-specific parser/scheduler/dispatcher components into the base engine.
    this->rxapp = new CTCPRXAppParser();
    this->txnet = new CTCPTXNet();
    this->rxnet = new CTCPRXNetParser();
    this->txapp = new CTCPTXApp();
    this->dispatcher = new CTCPDispatcher();
    this->interm_output = new interm_out();
}

MTCTCP::~MTCTCP() {}

MTContext *MTCTCP::InitContext(flow_id fid) {
    // Create and register one TCP context per flow.
    MTContext *mtctx = new ctcp_context();
    ctx_table[fid] = mtctx;
    return mtctx;
}

void MTCTCP::print_debugging_info(flow_id fid) {
    ofstream cwndFile("data.csv", std::ios_base::app);
    ctcp_context *ctx = (ctcp_context *)ctx_table[fid];
    cwndFile << Simulator::Now().GetSeconds() << " " << ctx->tokens << " " << ctx->stage << endl;
}

void MTCTCP::ExtractContextStats(flow_id fid, ns3::MTStatsSample &s) {
    // Copy TCP-only fields into the generic per-flow stats sample.
    ctcp_context *ctx = dynamic_cast<ctcp_context *>(ctx_table[fid]);
    if (!ctx) {
        return;
    }
    s.tokens = ctx->tokens;
    s.ssthresh = ctx->ssthresh;
    s.blksize = ctx->blksize;
    s.numblks = ctx->numblks;
    s.rtt = ctx->RTT;
    s.rtt_min = ctx->RTT_min;
    s.rto_ms = ctx->RTO;
    s.conn_stage = ctx->stage;
}

CTCPTXApp *MTCTCP::GetTxApp() const {
    return dynamic_cast<CTCPTXApp *>(this->txapp);
}

void MTCTCP::ReceiveConnectRequest(const Ipv4Address &saddr, const Ipv4Address &daddr) {
    SetPendingAddresses(saddr, daddr);
    flow_id fid(saddr.Get(), daddr.Get());
    CTCP_CONNECT *ev = new CTCP_CONNECT(10, fid);
    ev->flowId = fid;
    scheduler->enqueue_event(ev->flowId, ev);
    Mainloop();
}
