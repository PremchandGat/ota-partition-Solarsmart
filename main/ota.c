#include "ota.h"
#include <esp_ota_ops.h>
#include <esp_spi_flash.h>
#include <esp_system.h>

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(OTA_TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(OTA_TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(OTA_TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(OTA_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(OTA_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(OTA_TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(OTA_TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    }
    return ESP_OK;
}

void ota_update_from_url(const char *OTA_URL, const char *FW_VERSION)
{
    sleep(3);

    if (strcmp(FW_VERSION, FIRMWARE_VERSION) == 0)
    {
        ESP_LOGI(OTA_TAG, "Firmware version is same so, OTA aborted %s", OTA_URL);
        return;
    }
    esp_http_client_config_t config = {
        .url = OTA_URL,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    ESP_LOGI(OTA_TAG, "Starting OTA update from %s", OTA_URL);
    esp_err_t ret = esp_https_ota(&config);

    if (ret == ESP_OK)
    {
        ESP_LOGE(OTA_TAG, "OTA Update successful, restarting...");
        esp_restart();
    }
    else
    {
        esp_restart();
        ESP_LOGE(OTA_TAG, "OTA Update failed");
    }
}
