#include "canbus_backend.hpp"



bool CanBusBackend::StartCAN() {
    #if defined(ARDUINO_ARCH_ESP32)
        // Setup native ESP32 TWAI hardware
        if (ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN) && ESP32Can.begin(TWAI_SPEED_1000KBPS)){
            m_Running = true;
            Serial.println("CAN Start Success");
            return true;
        }
        Serial.println("CAN Start Fail");
        return false;
    #elif defined(ARDUINO_ARCH_STM32)
        // Setup native STM32 hardware
        STM32Can.begin(1000000);
    #endif
}

bool CanBusBackend::SendCAN(const MbrCanMessage& msg) {
    #if defined(ARDUINO_ARCH_ESP32)
        // Convert MbrCanMessage to ESP32 TWAI format and send
        CanFrame esp32frame = DBCtoCAN(msg);
        if (ESP32Can.writeFrame(esp32frame)){
            m_Operational = true;
            return true;
        }
        m_Operational = false;
        return false;
    #elif defined(ARDUINO_ARCH_STM32)
        // Convert MbrCanMessage to STM32 format and send

        return true;
    #endif
}

std::optional<MbrCanMessage> CanBusBackend::ReceiveCAN(){
    #if defined(ARDUINO_ARCH_ESP32)
        // Interpret CAN message and translate to ESP32 format
        CanFrame incoming_msg;
        if (ESP32Can.readFrame(incoming_msg, 0)){
            return DBCtoMBR(incoming_msg);
        }
        return std::nullopt;
    #elif defined(ARDUINO_ARCH_STM32)
        // Interpret CAN message and translate to STM format
        //CanFrame incoming_msg;
        return std::nullopt;
    #endif
}

#if defined (ARDUINO_ARCH_ESP32)
CanFrame CanBusBackend::DBCtoCAN(const MbrCanMessage& msg){
    CanFrame translated;
    //input dbc cpp code

    return translated;
}

MbrCanMessage CanBusBackend::DBCtoMBR(const CanFrame& frame){
    MbrCanMessage translated;
    //dbc cpp again

    return translated;
}
#elif defined (ARDUINO_ARCH_STM32)

#endif
