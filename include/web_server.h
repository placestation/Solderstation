/* #include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

void web_server_init();
void onWifiConnectedHandler(WiFiEvent_t e, WiFiEventInfo_t info);
void onWifiIpAddressAssigned(WiFiEvent_t e, WiFiEventInfo_t info);
// void onWifiDisconnectedHandler(WiFiEvent_t e, WiFiEventInfo_t info);
IPAddress getWifiIPAddress();
bool isWifiConnected();
void web_server_loop();

#endif // WEB_SERVER_H

*/

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>

enum WifiState {
    WIFI_STATE_CONNECTING,
    WIFI_STATE_AP_MODE,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED
};

void web_server_init();
void web_server_loop();
void web_server_reset_wifi();
IPAddress getWifiIPAddress();
bool isWifiConnected();
WifiState getWifiState();
String getWifiAPName();

#endif // WEB_SERVER_H