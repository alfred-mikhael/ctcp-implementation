#include "CTCPRXApp.h"
#include "CTCPEvents.h"

using namespace ns3;

MTEvent *CTCPRXAppParser::request_parser(ns3::app_msg_t request) {
    // Every app request becomes one CTCP_SEND event for the flow owned by this parser.
    CTCP_SEND *ev = new CTCP_SEND(10, id);
    ev->data_size =
        (request.size > 0) ? request.size : 100000; // default to 100KB if size not specified
    return ev;
}