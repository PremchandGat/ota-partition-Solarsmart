#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
static const char *FIRMWARE_VERSION = "V0.0";
static const char *OTA_TAG = "OTA";

void ota_update_from_url(const char *OTA_URL, const char *FW_VERSION);
esp_err_t _http_event_handler(esp_http_client_event_t *evt);