#include "ns3/mt-dispatcher.h"
#include "ns3/mt-event.h"
#include "ns3/mt-eventprocessor.h"

#include <vector>

using namespace ns3;
using namespace std;

/**
 * @brief Selects which CTCP event processors run for each incoming event type.
 */
class CTCPDispatcher : public MTDispatcher {
public:
    CTCPDispatcher();
    ~CTCPDispatcher() {}
    /**
     * @brief Return processors to execute for this event in pipeline order.
     */
    std::vector<MTEventProcessor *> dispatch(MTEvent *);
};
