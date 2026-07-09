#include "canbus_backend.hpp"

// This file serves as the translation layer between either the ESP32 or STM platform into
// MBR's CAN struct
// This is mostly just to allow for 1 file to manage both types of logic

bool can_bus_backend::start_can() {
    #if defined(ARDUINO_ARCH_ESP32)
        // Setup native ESP32 TWAI hardware
        if (ESP32Can.setPins(CAN_TX_PIN, CAN_RX_PIN) && ESP32Can.begin(TWAI_SPEED_1000KBPS)){
            m_running_ = true;
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

bool can_bus_backend::send_can(const mbr_can_message& msg) {
    #if defined(ARDUINO_ARCH_ESP32)
        // Convert MbrCanMessage to ESP32 TWAI format and send
        CanFrame esp32frame = mbr_to_twai(msg);
        if (ESP32Can.writeFrame(esp32frame)){
            m_operational_ = true;
            return true;
        }
        m_operational_ = false;
        return false;
    #elif defined(ARDUINO_ARCH_STM32)
        // Convert MbrCanMessage to STM32 format and send

        return true;
    #endif
}

std::optional<mbr_can_message> can_bus_backend::receive_can(){
    #if defined(ARDUINO_ARCH_ESP32)
        // Interpret CAN message and translate to ESP32 format
        CanFrame incoming_msg;
        if (ESP32Can.readFrame(incoming_msg, 0)){
            return twai_to_mbr(incoming_msg);
        }
        return std::nullopt;
    #elif defined(ARDUINO_ARCH_STM32)
        // Interpret CAN message and translate to STM format
        //CanFrame incoming_msg;
        return std::nullopt;
    #endif
}

#if defined (ARDUINO_ARCH_ESP32)
CanFrame can_bus_backend::mbr_to_twai(const mbr_can_message& msg){
    CanFrame twai_translated{};
    twai_translated.identifier = msg.id;
    twai_translated.extd = msg.extended;
    twai_translated.data_length_code = msg.length;
    memcpy(twai_translated.data, msg.data, msg.length);

    return twai_translated;
}

mbr_can_message can_bus_backend::twai_to_mbr(const CanFrame& frame){
    mbr_can_message mbr_translated{};
    mbr_translated.id = frame.identifier;
    mbr_translated.extended = frame.extd;
    mbr_translated.length = frame.data_length_code;
    memcpy(mbr_translated.data, frame.data, frame.data_length_code);
    return mbr_translated;
}
#elif defined (ARDUINO_ARCH_STM32)

#endif
