#ifndef CTCPRXAPP_H
#define CTCPRXAPP_H

#include "ns3/mt-rxapp.h"

/**
 * @brief Converts app CTCP_SEND requests into CTCP CTCP_SEND events.
 */
class CTCPRXAppParser : public ns3::MTRXAppParser {
public:
    int id = 0;
    CTCPRXAppParser() {}
    ~CTCPRXAppParser() {}
    /**
     * @brief Parse one app message to an initial CTCP event.
     */
    ns3::MTEvent *request_parser(ns3::app_msg_t request);
};

#endif