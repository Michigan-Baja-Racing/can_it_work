#include "board_wifi.hpp"
#include "sd.hpp"
#include "rpm.hpp"
#include "canbus_backend.hpp"
#include "mbr_dbc.h"
#include <array>
#include <string_view>

static constexpr int MAX_FILES    = 16;
static constexpr int MAX_NAME_LEN = 32;

class board_backend {
  public:
    explicit board_backend(const char* ssid, const char* password);
    ~board_backend() = default;

    void     initialize();
    void     run();
    uint64_t get_real_time() const;
    void     cleanup_clients() { m_wifi_.cleanup_clients(); }

  public:
    // all data to be sent, this will likely get redone with canbus implementation
    volatile double wheel_rpm  = 0;
    volatile double engine_rpm = 0;
    volatile double fr_shock = 0;
    volatile double fl_shock = 0;
    volatile double rr_shock = 0;
    volatile double rl_shock = 0;

  private:
    // This is where all command behavior belongs, always add your command function declarations here
    using command_handler = void (board_backend::*)(std::string_view payload);

    struct command_mapping {
        std::string_view command_name;
        command_handler   handler;
    };

    void handle_sync_cmd(std::string_view payload);
    void handle_sd_start_cmd(std::string_view payload);
    void handle_sd_write_cmd(std::string_view payload);
    void handle_sd_close_cmd(std::string_view payload);
    void handle_status_cmd(std::string_view payload);
    void handle_control_all_cmd(std::string_view payload);

    static const std::array<command_mapping, 6> m_command_table;

    void handle_command(std::string_view incoming);

  private:
    void                     send_data(const char* msg);
    void                     send_data(std::string_view);
    void                     receive_data();
    bool                     parse_frame(mbr_can_message& frame);
    const char*              m_ssid_;
    const char*              m_password_;
    char                     m_msg_[200];
    int64_t                  m_base_time_micros_{0};
    int64_t                  m_local_sync_micros_{0};
    volatile bool            m_is_time_synced_{false};
    volatile bool            m_wifi_on_{true};
    volatile bool            m_lora_on_{false};
    static int64_t           m_last_send;
    static int64_t           m_last_cleanup;
    board_wifi                m_wifi_;
    sd_card                   m_sd_;
    rpm_collector             m_wheel_rc_;
    rpm_collector             m_engine_rc_;
    can_bus_backend            m_canbus_;
    mbr_can_message            m_incoming_frame_;
    std::array<std::array<char, MAX_NAME_LEN>, MAX_FILES> m_file_index_{};
    int                      m_file_count_ = 0;
    std::vector<std::string> m_file_names_;
};
