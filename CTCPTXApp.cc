/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "CTCPTXApp.h"

#include <iostream>

void CTCPTXApp::execute_instr(ns3::MTInstruction *instr) {
    if (!instr) {
        std::cout << "CTCPTXApp: null TXAPP instruction" << std::endl;
        return;
    }

    if (auto *fb = dynamic_cast<FB_INSTR *>(instr)) {
        // Keep feedback ordered per flow so app polling observes transport order.
        feedback_[fb->flowId].push(*fb);
        return;
    }

    std::cout << "CTCPTXApp: unsupported TXAPP instruction for flow=" << instr->flowId << std::endl;
}

bool CTCPTXApp::HasFeedback(flow_id fid) const {
    auto it = feedback_.find(fid);
    return it != feedback_.end() && !it->second.empty();
}

bool CTCPTXApp::PopFeedback(flow_id fid, FB_INSTR &out) {
    auto it = feedback_.find(fid);
    if (it == feedback_.end() || it->second.empty()) {
        return false;
    }
    out = it->second.front();
    it->second.pop();
    return true;
}

size_t CTCPTXApp::FeedbackCount(flow_id fid) const {
    auto it = feedback_.find(fid);
    return (it == feedback_.end()) ? 0u : it->second.size();
}
