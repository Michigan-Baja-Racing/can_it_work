#include "board_wifi.hpp"
#include "WiFi.h"

board_wifi::board_wifi(const char* ssid, const char* password)
    : m_ssid_(ssid), m_password_(password), m_async_server_(80), m_web_sock_("/ws") {}

void board_wifi::start() {
    WiFi.softAPdisconnect(true);
    WiFiClass::mode(WIFI_AP);
    delay(100);

    IPAddress local_ip(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_ip, gateway, subnet);

    if (WiFi.softAP(m_ssid_, m_password_, 1, 0, 4)) {
        Serial.println("SoftAP Started Successfully");
    } else {
        Serial.println("SoftAP Failed to Start");
    }
    Serial.print("WiFi IP address: ");
    Serial.println(WiFi.softAPIP());

    if (MDNS.begin("telemetry")) { Serial.println("mDNS responder started"); }

    m_web_sock_.onEvent([this](AsyncWebSocket*       server,
                             AsyncWebSocketClient* /*client*/,
                             AwsEventType          type,
                             void*                 /*arg*/,
                             uint8_t*              data,
                             size_t                len) {
        if (type == WS_EVT_DATA) {
            size_t copy_len = min(len, sizeof(command_value) - 1);
            memcpy(command_value, data, copy_len);
            command_value[copy_len] = '\0';
            new_command            = true;

            Serial.print("Received Command: ");
            Serial.println(command_value);
        } else if (type == WS_EVT_CONNECT) {
            server->cleanupClients();
            Serial.println("Client connected");
        } else if (type == WS_EVT_DISCONNECT) {
            Serial.println("Client disconnected");
        }
    });

    m_async_server_.addHandler(&m_web_sock_);

    m_async_server_.on("/connecttest.txt", [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "Microsoft NCSI");
    });

    m_async_server_.on("/generate_204", [](AsyncWebServerRequest* request) { request->send(204); });
    m_async_server_.begin();
    Serial.println("HTTP Server started");
}

void board_wifi::send_data(const char* msg) {
    if (m_web_sock_.count() > 0) { m_web_sock_.textAll(msg); }
}

void board_wifi::send_data(std::string_view msg) {
    if (m_web_sock_.count() > 0) { m_web_sock_.textAll(msg.data(), msg.size()); }
}
