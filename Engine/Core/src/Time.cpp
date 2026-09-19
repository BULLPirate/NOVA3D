#include <Nova/Core/Time.h>

namespace Nova {

void Clock::Reset() {
    m_Start = {};
    m_Last = {};
    m_HasLast = false;
    m_DeltaSeconds = 0.0f;
    m_TotalSeconds = 0.0f;
}

void Clock::Tick() {
    const TimePoint now = std::chrono::steady_clock::now();
    if (!m_HasLast) {
        m_Start = now;
        m_Last = now;
        m_HasLast = true;
        m_DeltaSeconds = 0.0f;
        m_TotalSeconds = 0.0f;
        return;
    }

    m_DeltaSeconds = std::chrono::duration<float>(now - m_Last).count();
    if (m_DeltaSeconds > 0.05f) {
        m_DeltaSeconds = 0.05f;
    }
    m_TotalSeconds = std::chrono::duration<float>(now - m_Start).count();
    m_Last = now;
}

} // namespace Nova
