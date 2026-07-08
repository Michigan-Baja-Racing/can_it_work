#pragma once

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
// #include <DNSServer.h>

class board_wifi {
  public:
    explicit board_wifi(const char* ssid, const char* password);
    ~board_wifi() = default;

    void          cleanup_clients() { m_web_sock_.cleanupClients(); }
    void          start();
    void          send_data(const char* msg);
    char          command_value[128]{""};
    volatile bool new_command{false};

  private:
    const char* m_ssid_;
    const char* m_password_;

    AsyncWebServer m_async_server_;
    AsyncWebSocket m_web_sock_;
};


