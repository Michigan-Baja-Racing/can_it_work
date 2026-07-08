#ifndef MBR_CAN_H
#define MBR_CAN_H

#include <Arduino.h>
#include <optional>

#if defined(ARDUINO_ARCH_ESP32)
    // Use the native high-performance ESP32 library
    #include <ESP32-TWAI-CAN.hpp>
    #define CAN_RX_PIN 4
    #define CAN_TX_PIN 5
#elif defined(ARDUINO_ARCH_STM32)
    // Use a native STM32 CAN library (like STM32_CAN)
    #include <STM32_CAN.h>
#endif

// Struct to keep CAN messages unified across platforms
struct mbr_can_message {
    uint32_t id;
    bool extended;
    uint8_t length;
    uint8_t data[8];
};

// canbus_backend.hpp
class can_bus_backend {
public:
    bool start_can();
    bool send_can(const mbr_can_message& msg);
    [[nodiscard]] std::optional<mbr_can_message> receive_can();
    [[nodiscard]] bool is_running() const { return m_running_; }

private:
    bool m_running_ = false;
    bool m_operational_ = false;

    #if defined(ARDUINO_ARCH_ESP32)
        CanFrame mbr_to_twai(const mbr_can_message& msg);
        mbr_can_message twai_to_mbr(const CanFrame& frame);
    #elif defined(ARDUINO_ARCH_STM32)
        STM32_CAN_Frame MBRtoSTM(const MbrCanMessage& msg);
        MbrCanMessage STMtoMBR(const STM32_CAN_Frame& frame);
    #endif
};


#endif
