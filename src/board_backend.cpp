#include "board_backend.hpp"
#include "canbus_backend.hpp"
#include <Arduino.h>

int64_t board_backend::m_last_send    = 0;
int64_t board_backend::m_last_cleanup = 0;

board_backend::board_backend(const char* ssid, const char* password)
    : m_ssid_(ssid), m_password_(password), m_wifi_(ssid, password) {};

void board_backend::initialize() {
    m_wifi_.start();
    cleanup_clients();
    if (!m_sd_.init_sd()) {
        Serial.println("SD init fail");
        return;
    }
    m_canbus_.start_can();
}

// Runs the backend
void board_backend::run() {
    int64_t now = esp_timer_get_time();
    if (now - m_last_cleanup > 10000000LL) {
        cleanup_clients();
        m_last_cleanup = now;
    }

    // collect data here, will be changed with canbus implementation
    if (auto incoming = m_canbus_.receive_can()){
        m_incoming_frame_ = incoming.value();
        if (!parse_frame(m_incoming_frame_)){
            Serial.println("Frame Parse Failed");
        }
    }

    wheel_rpm  = m_wheel_rc_.get_rpm(esp_timer_get_time(), digitalRead(32)) / 2;
    engine_rpm = m_engine_rc_.get_rpm(esp_timer_get_time(), digitalRead(33));

    snprintf(m_msg_, sizeof(m_msg_), "T %llu W %f E %f\n", get_real_time(), wheel_rpm, engine_rpm);

    if ((now - m_last_send > 50000LL) && m_is_time_synced_) {
        send_data(m_msg_);
        if (m_sd_.is_open && m_sd_.is_write) { m_sd_.write_sd(m_msg_); }
        m_last_send = now;
    }

    if (m_wifi_.new_command /*add for LoRa behavior*/) { receive_data(); }
}

bool board_backend::parse_frame(mbr_can_message& frame){
    switch (frame.id) {
            case 0x100: {
                mbr_dbc_rpm_data_t rpm{};
                if (mbr_dbc_rpm_data_unpack(&rpm, frame.data, frame.length) != 0){ return false; }
                engine_rpm = mbr_dbc_rpm_data_engine_rpm_decode(rpm.engine_rpm);
                wheel_rpm  = mbr_dbc_rpm_data_wheel_rpm_decode(rpm.wheel_rpm);
                return true;
            }
            case 0x200: {
                mbr_dbc_f_shock_data_t front{};
                if (mbr_dbc_f_shock_data_unpack(&front, frame.data, frame.length) != 0){ return false; }
                fr_shock = mbr_dbc_f_shock_data_fr_shock_decode(front.fr_shock);
                fl_shock = mbr_dbc_f_shock_data_fl_shock_decode(front.fl_shock);
                return true;
            }
            case 0x201: {
                mbr_dbc_r_shock_data_t rear{};
                if (mbr_dbc_r_shock_data_unpack(&rear, frame.data, frame.length) != 0){ return false; }
                rr_shock = mbr_dbc_r_shock_data_rr_shock_decode(rear.rr_shock);
                rl_shock = mbr_dbc_r_shock_data_rl_shock_decode(rear.rl_shock);
                return true;
            }
            default:
                return false;
        }
}

uint64_t board_backend::get_real_time() const {
    if (!m_is_time_synced_) {
        return 0;
    }

    int64_t current_time = esp_timer_get_time();
    auto elapsed_micros = static_cast<uint64_t>(current_time - m_local_sync_micros_);
    return static_cast<uint64_t>(m_base_time_micros_) + elapsed_micros;
}

void board_backend::send_data(const char* msg) { m_wifi_.send_data(msg); }

void board_backend::receive_data() {
    m_wifi_.new_command = false;
    char res[64];
    if (m_wifi_on_) {
        if (strncmp(m_wifi_.command_value, "SYNC", 4) == 0) {
            const char* time_str = m_wifi_.command_value + 4;
            m_local_sync_micros_   = esp_timer_get_time();
            m_base_time_micros_    = static_cast<int64_t>(strtoull(time_str, nullptr, 10));
            m_is_time_synced_      = true;
            if (m_base_time_micros_ != 0) {
                snprintf(res, sizeof(res), "RES 0 SYNC %lld\n", m_base_time_micros_);
                Serial.println(res);
                send_data(res);
            }
        } else if (strncmp(m_wifi_.command_value, "SD_START", 8) == 0) {
            const char* name_str = m_wifi_.command_value + 9;
            if (*name_str == '\0') { name_str = "/data.txt"; }
            if (m_file_count_ < MAX_FILES) {
                bool exists = false;
                for (int i = 0; i < m_file_count_; i++) {
                    if (strncmp(m_file_index_[i], name_str, MAX_NAME_LEN) == 0) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) { strncpy(m_file_index_[m_file_count_++], name_str, MAX_NAME_LEN - 1); }
            } else {
                send_data("RES 0 SD_START deadbeef\n");
                return;
            }
            if (m_sd_.is_open) {
                if (m_sd_.close_sd()) {
                    send_data("RES 0 SD_WRITE 0\n");
                    send_data("RES 0 SD_CLOSE 1\n");
                } else {
                    send_data("RES 0 SD_CLOSE 0\n");
                }
            }
            if (!m_sd_.open_sd(name_str)) {
                send_data("RES 0 SD_START deadbeef\n");
                return;
            }
            m_sd_.name = name_str;
            snprintf(res, sizeof(res), "RES 0 SD_START %s\n", name_str);
            send_data(res);
        } else if (strncmp(m_wifi_.command_value, "SD_WRITE", 8) == 0) {
            const char* value_str = m_wifi_.command_value + 9;
            m_sd_.is_write         = (*value_str == '1');
            snprintf(res, sizeof(res), "RES 0 SD_WRITE %d\n", m_sd_.is_write);
            send_data(res);
        } else if (strncmp(m_wifi_.command_value, "SD_CLOSE", 8) == 0) {
            if (m_sd_.close_sd()) {
                send_data("RES 0 SD_WRITE 0\n");
                send_data("RES 0 SD_CLOSE 1\n");
            } else {
                send_data("RES 0 SD_CLOSE 0\n");
            }
        } else if (strncmp(m_wifi_.command_value, "STATUS", 6) == 0) {
            for (size_t i = 0; i < MAX_FILES; i++) {
                // snprintf(res, sizeof(res), "RES 0 SD_OPEN %d\n", m_FileNames[i]);
                // SendData(res);
            }
        } else {
            /*currently does nothing, need to work on other command implementation*/
            send_data("RES 1\n");
        }
    }

    if (m_lora_on_ /*currently its never on */) {}
}
