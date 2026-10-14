#include "CTCPDispatcher.h"
#include "CTCPEventProcs.h"
#include "CTCPEvents.h"
#include <iostream>

CTCPDispatcher::CTCPDispatcher() {}

std::vector<MTEventProcessor *> CTCPDispatcher::dispatch(MTEvent *event) {
    // Build the processor chain for this event type in execution order.
    std::vector<MTEventProcessor *> ChosenProcessors;
    if (typeid(*event) == typeid(CTCP_ACK)) {
        ChosenProcessors.push_back(new estimate_net_params_ep());
        ChosenProcessors.push_back(new congestion_control_ep());
        ChosenProcessors.push_back(new ctcp_ack_net_ep());
    }
    if (typeid(*event) == typeid(CTCP_MISS_ACK)) {
        ChosenProcessors.push_back(new ctcp_ack_timeout_ep());
    }
    if (typeid(*event) == typeid(CTCP_DATA)) {
        ChosenProcessors.push_back(new ctcp_data_net_ep());
        ChosenProcessors.push_back(new ctcp_send_ack_ep());
        ChosenProcessors.push_back(new ctcp_app_feedback_ep());
    }
    if (typeid(*event) == typeid(CTCP_SYN)) {
        ChosenProcessors.push_back(new ctcp_syn_net_ep());
    }
    if (typeid(*event) == typeid(CTCP_SYN_ACK)) {
        ChosenProcessors.push_back(new ctcp_syn_ack_net_ep());
    }
    if (typeid(*event) == typeid(CTCP_CONNECT)) {
        std::cout << "CTCPDispatcher: creating ctcp_connect_ep for CTCP_CONNECT event with typeid "
                  << typeid(*event).name() << std::endl;
        ChosenProcessors.push_back(new ctcp_connect_ep());
    }
    if (typeid(*event) == typeid(CTCP_SEND)) {
        ChosenProcessors.push_back(new ctcp_send_ep());
    }
    return ChosenProcessors;
}
