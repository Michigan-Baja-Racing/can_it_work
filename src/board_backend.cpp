#include "board_backend.hpp"
#include "canbus_backend.hpp"
#include <Arduino.h>
#include <string>
#include <charconv>

int64_t board_backend::m_last_send    = 0;
int64_t board_backend::m_last_cleanup = 0;

board_backend::board_backend(const char* ssid, const char* password)
    : m_ssid_(ssid), m_password_(password), m_wifi_(ssid, password) {};

const std::array<board_backend::command_mapping, 5> board_backend::m_command_table{{
    {"SYNC",     &board_backend::handle_sync_cmd},
    {"SD_START", &board_backend::handle_sd_start_cmd},
    {"SD_WRITE", &board_backend::handle_sd_write_cmd},
    {"SD_CLOSE", &board_backend::handle_sd_close_cmd},
    {"STATUS",   &board_backend::handle_status_cmd}
}};

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

    snprintf(m_msg_, sizeof(m_msg_), "T %llu W %f E %f FL %f FR %f RL %f RR %f\n", get_real_time(), wheel_rpm, engine_rpm, fl_shock, fr_shock, rl_shock, rr_shock);

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

void board_backend::send_data(std::string_view msg) { m_wifi_.send_data(msg); }

void board_backend::receive_data() {
    m_wifi_.new_command = false;
    if (!m_wifi_on_) { return ;}

    std::string_view incoming = m_wifi_.command_value;

    if (incoming.rfind("CMD ", 0) == 0) {
            handle_command(incoming);
        }
}

void board_backend::handle_command(std::string_view incoming) {
    incoming.remove_prefix(4);

    bool command_found = false;

    for (const auto& cmd : m_command_table) {
        if (incoming.rfind(cmd.command_name, 0) == 0){
            std::string_view payload;
            if (incoming.size() > cmd.command_name.size() + 1) {
                payload = incoming.substr(cmd.command_name.size() + 1);
            }
            (this->*(cmd.handler))(payload);
            command_found = true;
            break;
        }
    }
    if (!command_found) {
        send_data("RES CMD_FAIL 0\n");
    }
}

// This is the collection of every current Wifi command and its implementation.
// To add more, simply add the command function def in the .hpp
// then increase the size of the command table and finally
// write the implementation of the command

void board_backend::handle_sync_cmd(std::string_view payload) {
    std::array<char, 64> res;
    m_local_sync_micros_   = esp_timer_get_time();
    std::from_chars(payload.begin(), payload.end(), m_base_time_micros_, 10);
    m_is_time_synced_      = true;
    if (m_base_time_micros_ != 0) {
        int size = snprintf(res.data(), res.size(), "RES SYNC %lld\n", m_base_time_micros_);
        std::string_view res_payload(res.data(), size);
        Serial.println(res_payload.data()); //NOLINT
        send_data(res_payload);
    }
}

void board_backend::handle_sd_start_cmd(std::string_view payload) {
    std::array<char, 64> res;
    std::array<char, MAX_NAME_LEN> name_array;
    if (payload.empty()) {
        constexpr std::string_view default_name = "/data.txt";
        std::copy(default_name.begin(), default_name.end(), name_array.begin());
    } else {
        size_t len = std::min(payload.size(), name_array.size() - 1);
        std::copy(payload.begin(), payload.begin() + len, name_array.begin());
    }
    if (m_file_count_ < MAX_FILES) {
        bool exists = false;
        for (int i = 0; i < m_file_count_; i++) {
            if (m_file_index_[i] == name_array) {
                exists = true;
                break;
            }
        }
        if (!exists) { m_file_index_[m_file_count_++] = name_array; }
    } else {
        send_data("RES SD_START deadbeef\n");
        return;
    }
    if (m_sd_.is_open) {
        if (m_sd_.close_sd()) {
            send_data("RES SD_WRITE 0\n");
            send_data("RES SD_CLOSE 1\n");
        } else {
            send_data("RES SD_CLOSE 0\n");
        }
    }
    if (!m_sd_.open_sd(name_array.data())) {
        send_data("RES SD_START deadbeef\n");
        return;
    }
    m_sd_.name = name_array.data();
    int size = snprintf(res.data(), res.size(), "RES SD_START %s\n", name_array.data());
    std::string_view res_payload(res.data(), size);
    Serial.println(res_payload.data()); //NOLINT
    send_data(res_payload);
}

void board_backend::handle_sd_write_cmd(std::string_view payload) {
    std::array<char, 64> res;
    std::string payload_str = std::string(payload);
    const char* value_str = payload_str.c_str();
    m_sd_.is_write        = (*value_str == '1');
    int size = snprintf(res.data(), res.size(), "RES SD_WRITE %d\n", m_sd_.is_write);
    std::string_view res_payload(res.data(), size);
    send_data(res_payload);
}

void board_backend::handle_sd_close_cmd(std::string_view payload) {
    if (m_sd_.close_sd()) {
        send_data("RES SD_WRITE 0\n");
        send_data("RES SD_CLOSE 1\n");
    } else {
        send_data("RES SD_CLOSE 0\n");
    }
}

void board_backend::handle_status_cmd(std::string_view payload){
    for (size_t i = 0; i < MAX_FILES; i++) {
        // snprintf(res, sizeof(res), "RES 0 SD_OPEN %d\n", m_FileNames[i]);
        // SendData(res);
    }
}

/*
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
                send_data("RES SD_START deadbeef\n");
                return;
            }
            if (m_sd_.is_open) {
                if (m_sd_.close_sd()) {
                    send_data("RES SD_WRITE 0\n");
                    send_data("RES SD_CLOSE 1\n");
                } else {
                    send_data("RES SD_CLOSE 0\n");
                }
            }
            if (!m_sd_.open_sd(name_str)) {
                send_data("RES SD_START deadbeef\n");
                return;
            }
            m_sd_.name = name_str;
            snprintf(res, sizeof(res), "RES SD_START %s\n", name_str);
            send_data(res);
        } else if (strncmp(m_wifi_.command_value, "SD_WRITE", 8) == 0) {
            const char* value_str = m_wifi_.command_value + 9;
            m_sd_.is_write         = (*value_str == '1');
            snprintf(res, sizeof(res), "RES SD_WRITE %d\n", m_sd_.is_write);
            send_data(res);
        } else if (strncmp(m_wifi_.command_value, "SD_CLOSE", 8) == 0) {
            if (m_sd_.close_sd()) {
                send_data("RES SD_WRITE 0\n");
                send_data("RES SD_CLOSE 1\n");
            } else {
                send_data("RES SD_CLOSE 0\n");
            }
        } else if (strncmp(m_wifi_.command_value, "STATUS", 6) == 0) {
            for (size_t i = 0; i < MAX_FILES; i++) {
                // snprintf(res, sizeof(res), "RES 0 SD_OPEN %d\n", m_FileNames[i]);
                // SendData(res);
            }
        } else {
            /*currently does nothing, need to work on other command implementation
            send_data("RES 1\n");
        }
    }

    if (m_lora_on_ /*currently its never on ) {}
}
*/
