#pragma once

#include <cstdint>
#include <functional>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace Nova {

/// Typed pub/sub. Core never knows about Editor or plugin event structs.
class EventBus {
public:
    using Token = uint32_t;

    template <typename TEvent>
    Token Subscribe(std::function<void(const TEvent&)> handler) {
        const Token token = ++m_Next;
        Slot slot;
        slot.Id = token;
        slot.Fn = [handler](const void* payload) {
            handler(*static_cast<const TEvent*>(payload));
        };
        m_Handlers[std::type_index(typeid(TEvent))].push_back(std::move(slot));
        return token;
    }

    void Unsubscribe(Token token);

    template <typename TEvent>
    void Publish(const TEvent& event) {
        const auto it = m_Handlers.find(std::type_index(typeid(TEvent)));
        if (it == m_Handlers.end()) {
            return;
        }
        const std::vector<Slot> snapshot = it->second;
        for (const Slot& slot : snapshot) {
            slot.Fn(&event);
        }
    }

private:
    struct Slot {
        Token Id = 0;
        std::function<void(const void*)> Fn;
    };

    std::unordered_map<std::type_index, std::vector<Slot>> m_Handlers;
    Token m_Next = 0;
};

} // namespace Nova
