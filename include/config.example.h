#pragma once

// Copy this file to include/config.h and edit the values.

#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-password"

// RemoteTerm host only -- do not include http:// or https://
#define REMOTETERM_HOST "192.168.1.50"
#define REMOTETERM_PORT 8000
#define REMOTETERM_TLS false

// Optional HTTP Basic Auth. Leave both empty if RemoteTerm has no basic auth.
#define REMOTETERM_USERNAME ""
#define REMOTETERM_PASSWORD ""

// UI / network behaviour
#define DEVICE_NAME "RemoteTerm Display"
#define INITIAL_MESSAGE_LIMIT 40
#define MAX_CHANNELS 64
#define MAX_MESSAGES 80
#define POLL_FALLBACK_MS 15000UL
#define CHANNEL_REFRESH_MS 300000UL
#define WIFI_RETRY_MS 10000UL
#define WS_RETRY_MS 5000UL

// If true, HTTPS/WSS accepts any server certificate. Useful for local/self-signed
// RemoteTerm instances. For exposed/public systems, use proper certificate checking.
#define TLS_INSECURE true

// If saved Wi-Fi cannot connect for 30 seconds, the display starts a setup AP.
// Connect to RemoteTerm-XXXX with password "configure", then open 192.168.4.1.
#define WIFI_CONNECT_TIMEOUT_MS 30000UL
#define SETUP_AP_PASSWORD "configure"
#define DEFAULT_DISPLAY_ROTATION 2
#define OTA_CHECK_INTERVAL_MS 21600000UL
