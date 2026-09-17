#include <Nova/Core/Events.h>

namespace Nova {

void EventBus::Unsubscribe(Token token) {
    for (auto& [type, slots] : m_Handlers) {
        (void)type;
        for (auto it = slots.begin(); it != slots.end(); ++it) {
            if (it->Id == token) {
                slots.erase(it);
                return;
            }
        }
    }
}

} // namespace Nova
