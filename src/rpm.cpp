#include "rpm.hpp"
#include <cstdint>

rpm_collector::rpm_collector() = default;

auto rpm_collector::get_rpm(int64_t timestamp, uint8_t pin_value) -> double {
    if (timestamp == 0) { return 0.0; }

    if (m_last_time_ != 0 && (timestamp - m_last_time_) > 2000000LL) {
        m_rpm_      = 0.0;
        m_was_rpm_   = false;
        m_last_time_ = 0;
    } else if (pin_value == LO && !m_was_rpm_) {
        if (m_last_time_ != 0) {
            const auto elapsed_us = timestamp - m_last_time_;
            if (elapsed_us > 1) {
                m_rpm_    = 60000000.0 / static_cast<double>(elapsed_us);
                m_was_rpm_ = true;
            }
        }
        m_last_time_ = timestamp;
        m_was_rpm_   = true; // suppress second LO until a HI resets it
    } else if (pin_value == HI) {
        m_was_rpm_ = false;
    }

    return m_rpm_;
}
