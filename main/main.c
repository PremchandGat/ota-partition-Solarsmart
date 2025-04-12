#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"

#include "wifi_manager.h"

#include "sdkconfig.h"

#include <stdint.h>
#include <stddef.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
// #include "protocol_examples_common.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"
/* For OTA */
#include "ota.h"

#include "esp_spi_flash.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include <stdlib.h>

#define TAG "FW_COPY"
#define BUF_SIZE 4096

void copy_firmware_to_raw_offsets()
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running)
    {
        ESP_LOGE(TAG, "Failed to get running partition");
        return;
    }

    ESP_LOGI(TAG, "Running firmware at: 0x%X (%s), size: %d", running->address, running->label, running->size);

    // Define target flash offsets and size
    const uint32_t target_offsets[] = {0x10000, 0x190000};
    const size_t target_size = running->size;

    // Allocate buffer
    uint8_t *buf = malloc(BUF_SIZE);
    if (!buf)
    {
        ESP_LOGE(TAG, "Buffer allocation failed");
        return;
    }

    for (int i = 0; i < 2; ++i)
    {
        uint32_t target_addr = target_offsets[i];

        // Skip if target is same as currently running partition
        if (target_addr == running->address)
        {
            ESP_LOGW(TAG, "Skipping copy to 0x%X (same as running)", target_addr);
            continue;
        }

        ESP_LOGI(TAG, "Erasing flash at 0x%X (size: %d)", target_addr, target_size);
        esp_err_t err = spi_flash_erase_range(target_addr, target_size);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Erase failed at 0x%X: %s", target_addr, esp_err_to_name(err));
            continue;
        }

        ESP_LOGI(TAG, "Copying firmware to 0x%X", target_addr);
        for (size_t offset = 0; offset < target_size; offset += BUF_SIZE)
        {
            size_t len = BUF_SIZE;
            if (offset + len > target_size)
            {
                len = target_size - offset;
            }

            err = esp_partition_read(running, offset, buf, len);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Read failed at offset 0x%X: %s", offset, esp_err_to_name(err));
                break;
            }

            err = spi_flash_write(target_addr + offset, buf, len);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Write failed at offset 0x%X: %s", offset, esp_err_to_name(err));
                break;
            }
        }
        ESP_LOGI(TAG, "✅ Finished copying to 0x%X", target_addr);
    }
    free(buf);
}

extern void partition_change();

/**
 * @brief this is an exemple of a callback that you can setup in your own app to get notified of wifi manager event.
 */
void cb_connection_ok(void *pvParameter)
{
    ip_event_got_ip_t *param = (ip_event_got_ip_t *)pvParameter;

    /* transform IP to human readable string */
    char str_ip[16];
    esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);

    ESP_LOGI("WIFI:", "I have a connection and my IP is %s!", str_ip);
    printf("OTA starts in 5 seconds");
    sleep(5);
    ota_update_from_url("http://178.128.194.58/xhmodv4.bin", "V1.0");
}

void app_main(void)
{
    ESP_LOGI("APP:", "[APP] Startup..");
    partition_change();
    const esp_partition_t *updateParititon = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    printf("Running from partition: %s at offset 0x%X\n",
           running_partition->label,
           running_partition->address);

    // uint32_t part_offset = 0x10000; // Example for ota_1
    // esp_partition_subtype_t part_subType = ESP_PARTITION_SUBTYPE_APP_OTA_0;
    // uint32_t part_size = 0x110000;
    // esp_partition_t partition;
    // partition.flash_chip = running_partition->flash_chip;
    // partition.type = ESP_PARTITION_TYPE_APP;
    // partition.subtype = part_subType;
    // partition.size = part_size;
    // partition.encrypted = false;
    // partition.address = 0x10000;
    // strcpy((char *)partition.label, "ota_0");

    // You cannot assign to `char label[16]` directly; use strcpy instead
    // strcpy((char *)partition.label, "ota_0"); // cast to non-const for strcpy safety
    // if (strcmp(running_partition->label, "ota_0") == 0)
    // {
    //   part_offset = 0x10000; // Example: factory
    //   strcpy((char *)partition.label, "ota_0");
    // }
    // else if (strcmp(running_partition->label, "ota_1") == 0)
    // {
    //   part_offset = 0x190000; // Example: ota_1
    //   strcpy((char *)partition.label, "ota_1");
    // }
    // partition.address = part_offset;
    bool flagRestart = true;
    if (updateParititon->address == 0x10000)
    {
        flagRestart = false;
    }
    if (updateParititon->address == 0x190000)
    {
        flagRestart = false;
    }
    if (flagRestart)
    {
        printf("Copy the current fw to all the partitions");
        copy_firmware_to_raw_offsets();
        printf("System isn't rebooted after the Partition update\n");
        // esp_err_t error = esp_ota_set_boot_partition(&partition);
        // printf("esp_ota_set_boot_partition Status: %d\n", error);
        // if (error == ESP_OK)
        // {
        //   printf("Boot from same partition\n");
        // }
        // else
        // {
        //   printf("Failed Updated current partition table\n");
        // }
        // printf("Rebooting........\n");
        // sleep(1);
        esp_restart();
    }
    /* start the wifi manager */
    wifi_manager_start();
    // /* register a callback as an example to how you can integrate your code with the wifi manager */
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);
}