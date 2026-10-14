#include "CTCPContext.h"
#include "CTCPEvents.h"
#include "CTCPInstructions.h"
#include "ns3/mt-context.h"
#include "ns3/mt-event.h"
#include "ns3/mt-eventprocessor.h"
#include "ns3/mtp-types.h"

#pragma once
using namespace ns3;
using namespace std;

struct EPResult {
    vector<MTInstruction *> instrs;
};

/**
 * @brief Handles app CTCP_CONNECT events: connection establishment entry-point.
 */
class ctcp_connect_ep : public MTEventProcessor {
public:
    ctcp_connect_ep();
    ~ctcp_connect_ep();
    EventProcessorOutput *process(MTEvent *e, EventProcessorOutput *epOut);
    bool isValidEvent(MTEvent *e);
    EPResult own_Process(CTCP_CONNECT &ev, ctcp_context &ctx, interm_out &int_out);
};

class ctcp_send_ep : public MTEventProcessor {
public:
    ctcp_send_ep();
    ~ctcp_send_ep();
    EventProcessorOutput *process(MTEvent *e, EventProcessorOutput *epOut);
    bool isValidEvent(MTEvent *e);
    EPResult own_Process(CTCP_SEND &ev, ctcp_context &ctx, interm_out &int_out);
};

class estimate_net_params_ep : public MTEventProcessor {
public:
    estimate_net_params_ep();
    ~estimate_net_params_ep();
    EventProcessorOutput *process(MTEvent *e, EventProcessorOutput *epOut);
    bool isValidEvent(MTEvent *e);
    EPResult own_Process(CTCP_ACK &ev, ctcp_context &ctx, interm_out &int_out);
};

class congestion_control_ep : public MTEventProcessor {
public:
    congestion_control_ep();
    ~congestion_control_ep();
    EventProcessorOutput *process(MTEvent *e, EventProcessorOutput *epOut);
    bool isValidEvent(MTEvent *e);
    EPResult own_Process(CTCP_ACK &ev, ctcp_context &ctx, interm_out &int_out);
};
class ctcp_ack_net_ep : public MTEventProcessor {
public:
    ctcp_ack_net_ep();
    ~ctcp_ack_net_ep();
    EventProcessorOutput *process(MTEvent *e, EventProcessorOutput *epOut);
    bool isValidEvent(MTEvent *e);
    EPResult own_Process(CTCP_ACK &ev, ctcp_context &ctx, interm_out &int_out);
};
class ctcp_data_net_ep : public MTEventProcessor {
public:
    ctcp_data_net_ep();
    ~ctcp_data_net_ep();
    EventProcessorOutput *process(MTEvent *e, EventProcessorOutput *epOut);
    bool isValidEvent(MTEvent *e);
    EPResult own_Process(CTCP_DATA &ev, ctcp_context &ctx, interm_out &int_out);
};
class ctcp_syn_net_ep : public MTEventProcessor {
public:
    ctcp_syn_net_ep();
    ~ctcp_syn_net_ep();
    EventProcessorOutput *process(MTEvent *e, EventProcessorOutput *epOut);
    bool isValidEvent(MTEvent *e);
    EPResult own_Process(CTCP_SYN &ev, ctcp_context &ctx, interm_out &int_out);
};
class ctcp_syn_ack_net_ep : public MTEventProcessor {
public:
    ctcp_syn_ack_net_ep();
    ~ctcp_syn_ack_net_ep();
    EventProcessorOutput *process(MTEvent *e, EventProcessorOutput *epOut);
    bool isValidEvent(MTEvent *e);
    EPResult own_Process(CTCP_SYN_ACK &ev, ctcp_context &ctx, interm_out &int_out);
};
class ctcp_send_ack_ep : public MTEventProcessor {
public:
    ctcp_send_ack_ep();
    ~ctcp_send_ack_ep();
    EventProcessorOutput *process(MTEvent *e, EventProcessorOutput *epOut);
    bool isValidEvent(MTEvent *e);
    EPResult own_Process(CTCP_DATA &ev, ctcp_context &ctx, interm_out &int_out);
};
class ctcp_app_feedback_ep : public MTEventProcessor {
public:
    ctcp_app_feedback_ep();
    ~ctcp_app_feedback_ep();
    EventProcessorOutput *process(MTEvent *e, EventProcessorOutput *epOut);
    bool isValidEvent(MTEvent *e);
    EPResult own_Process(CTCP_DATA &ev, ctcp_context &ctx, interm_out &int_out);
};
class ctcp_ack_timeout_ep : public MTEventProcessor {
public:
    ctcp_ack_timeout_ep();
    ~ctcp_ack_timeout_ep();
    EventProcessorOutput *process(MTEvent *e, EventProcessorOutput *epOut);
    bool isValidEvent(MTEvent *e);
    EPResult own_Process(CTCP_MISS_ACK &ev, ctcp_context &ctx, interm_out &int_out);
};
