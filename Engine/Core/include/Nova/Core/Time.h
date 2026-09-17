#pragma once

#include <chrono>

namespace Nova {

/// Frame clock. Call Tick once per frame; DeltaSeconds is time since the previous Tick.
class Clock {
public:
    void Reset();
    void Tick();

    float DeltaSeconds() const { return m_DeltaSeconds; }
    float TotalSeconds() const { return m_TotalSeconds; }

private:
    using TimePoint = std::chrono::steady_clock::time_point;

    TimePoint m_Start{};
    TimePoint m_Last{};
    bool m_HasLast = false;
    float m_DeltaSeconds = 0.0f;
    float m_TotalSeconds = 0.0f;
};

} // namespace Nova
