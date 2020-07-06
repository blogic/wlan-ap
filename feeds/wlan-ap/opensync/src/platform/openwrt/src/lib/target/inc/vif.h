#ifndef _VIF_H__
#define _VIF_H__

#define OVSDB_SECURITY_KEY                  "key"
#define OVSDB_SECURITY_OFTAG                "oftag"
#define OVSDB_SECURITY_MODE                 "mode"
#define OVSDB_SECURITY_MODE_WEP64           "64"
#define OVSDB_SECURITY_MODE_WEP128          "128"
#define OVSDB_SECURITY_MODE_WPA1            "1"
#define OVSDB_SECURITY_MODE_WPA2            "2"
#define OVSDB_SECURITY_MODE_MIXED           "mixed"
#define OVSDB_SECURITY_ENCRYPTION           "encryption"
#define OVSDB_SECURITY_ENCRYPTION_OPEN      "OPEN"
#define OVSDB_SECURITY_ENCRYPTION_WEP       "WEP"
#define OVSDB_SECURITY_ENCRYPTION_WPA_PSK   "WPA-PSK"
#define OVSDB_SECURITY_ENCRYPTION_WPA_EAP   "WPA-EAP"
#define OVSDB_SECURITY_RADIUS_SERVER_IP     "radius_server_ip"
#define OVSDB_SECURITY_RADIUS_SERVER_PORT   "radius_server_port"
#define OVSDB_SECURITY_RADIUS_SERVER_SECRET "radius_server_secret"

extern bool vif_state_update(struct uci_section *s, struct schema_Wifi_VIF_Config *vconf);

#endif
