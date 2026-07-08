#include <cstdint>

class rpm_collector {
  public:
    explicit rpm_collector();

    auto               get_rpm(int64_t timestamp, uint8_t pin_value) -> double; // in the cpp
    [[nodiscard]] auto thresh() const -> int64_t {
        return m_last_time_ + 2000000LL /* Needs to be converted to us */;
    }

  private:
    int64_t m_last_time_ = 0; // This is in microseconds
    double  m_rpm_      = 0;
    bool    m_was_rpm_   = false;
};

#define LO 0
#define HI 1

/*

// In RPM.cpp

auto GetRPM(uint32_t timestamp, uint8_t pin_value) -> double {
    if (timestamp == 0) { return 0.0; }
    if (pin_value == LO && m_LastTime > Thresh()) {
        m_RPM = 0.0;
        // Do something meaningful maybe
    } else if (pin_value == HI) {
        const auto elapsed_us = timestamp - m_LastTime;
        m_RPM = elapsed_us * 1.67e-8;
        m_LastTime = timestamp;
    }

    return m_RPM;
}

Using the class:

in setup:

pinMode(67, INPUT);
pinMode(69, INPUT);
pinMode(420, INPUT);

RPMCollector wheel_rc;
RPMCollector engine_rc;

in loop:

wheel_rc.GetRPM(current_time(), digitalRead(67));
engine_rc.GetRPM(current_time(), digitalRead(69));

*/
