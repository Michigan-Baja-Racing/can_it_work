#include "sd.hpp"
#include "SD.h"

sd_card::sd_card() = default;

bool sd_card::open_sd(const char* name) {

    File check  = SD.open(name, FILE_READ);
    bool exists = static_cast<bool>(check);
    if (check) { check.close(); }

    if (exists) {
        m_log_file_ = SD.open(name, FILE_APPEND);
    } else {
        m_log_file_ = SD.open(name, FILE_WRITE);
    }

    if (m_log_file_) {
        is_open = true;
        if (exists) {
            Serial.println("SD File re-opened successfully. Streaming active.");
        } else {
            Serial.println("SD File opened successfully. Streaming active.");
        }
        return true;
    }
    Serial.println("Failed to open file for appending.");
    return false;
}

void sd_card::write_sd(const char* msg) {
    if (!is_open || !m_log_file_) { return; }

    m_log_file_.print(msg);

    if (millis() - m_last_flush_ > 5000) {
        m_log_file_.flush();
        m_last_flush_ = millis();
    }
}

bool sd_card::close_sd() {
    if (is_open && m_log_file_) {
        m_log_file_.close();
        is_open  = false;
        is_write = false;
        Serial.println("SD File cleanly closed.");
        return true;
    }
    return false;
}

bool sd_card::init_sd() const {
    if (!SD.begin(m_chip_select_)) {
        Serial.println("SD mount fail");
        return false;
    }
    Serial.println("SD mounted");
    return true;
}
