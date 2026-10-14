#ifndef MTCTCP_H
#define MTCTCP_H

#include "ns3/modular-transport.h"
#include "ns3/mt-context.h"

class CTCPTXApp;

/**
 *  @brief TCP protocol binding built on top of ModularTransport.
 *
 * Owns TCP-specific component wiring and exposes helpers used by tests/apps.
 */
class MTCTCP : public ns3::ModularTransport {
private:
public:
    MTCTCP();
    ~MTCTCP();
    /**
     * @brief Create per-flow CTCP connection state.
     */
    ns3::MTContext *InitContext(flow_id fid) override;
    void print_debugging_info(flow_id fid) override;
    /**
     * @brief Export CTCP fields into the generic per-flow stats sample.
     */
    void ExtractContextStats(flow_id fid, ns3::MTStatsSample &s) override;
    /**
     * @brief Access the CTCP app-facing transmitter component.
     */
    CTCPTXApp *GetTxApp() const;

    /// Active open: enqueue a CTCP_CONNECT event so the stack sends CTCP_SYN (call before CTCP_SEND
    /// / CTCP_DATA).
    void ReceiveConnectRequest(const ns3::Ipv4Address &saddr,
                               const ns3::Ipv4Address &daddr) override;
};

#endif // MTTCP_H
