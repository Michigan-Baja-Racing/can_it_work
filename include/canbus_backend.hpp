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
struct MbrCanMessage {
    uint32_t Id;
    bool Extended;
    uint8_t Length;
    uint8_t Data[8];
};

// canbus_backend.hpp
class CanBusBackend {
public:
    bool StartCAN();
    bool SendCAN(const MbrCanMessage& msg);
    std::optional<MbrCanMessage> ReceiveCAN();
    [[nodiscard]]bool IsRunning() const { return m_Running; }

private:
    bool m_Running = false;
    bool m_Operational = false;

    #if defined(ARDUINO_ARCH_ESP32)
        CanFrame MBRtoTWAI(const MbrCanMessage& msg);
        MbrCanMessage TWAItoMBR(const CanFrame& frame);
    #elif defined(ARDUINO_ARCH_STM32)
        STM32_CAN_Frame MBRtoSTM(const MbrCanMessage& msg);
        MbrCanMessage STMtoMBR(const STM32_CAN_Frame& frame);
    #endif
};


#endif
