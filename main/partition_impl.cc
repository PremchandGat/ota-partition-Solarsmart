#include <stdio.h>
// #include "esp_spi_flash.h"
// #include "esp_ota_ops.h"
// #include "esp32/rom/md5_hash.h"
#include <freertos/FreeRTOS.h>
#include "freertos/task.h"
#include <esp_ota_ops.h>
#include <esp_spi_flash.h>
#include <esp_system.h>

#include "new_partition.h"
#include <string.h>

/*
Given a partition table as follows:

secure,   0x42, 0x00,    0x9000,   0x4000,
otadata,  data, ota,     0xd000,   0x2000,
ota_0,    app,  ota_0,   0x10000,  0x150000,
ota_1,    app,  ota_1,   0x160000, 0x150000,
ota_0_cfg,0x41, 0x00,    0x2B0000, 0x4000,
ota_1_cfg,0x41, 0x01,    0x2B4000, 0x4000,
nvs,      data, nvs,     0x2B8000, 0x4000,
coredump, data, coredump,0x2BC000, 0x10000,
data,     0x40, 0x00,    0x2CC000, 0x134000, encrypted

We want to change the partition table to have size 0x1a0000
and reduce the size of the data partition.
*/

extern "C" void partition_change();

const int RETRY_LIMIT = 10;
const int RETRY_WAIT = 100;

const size_t PARTITION_TABLE_ADDRESS = 0x8000;
const size_t PARTITION_TABLE_SIZE = 0xC00;
const size_t PARTITION_TABLE_ALIGNED_SIZE = 0x1000; // Must be divisible by 4k.

// const size_t OTA_1_ADDRESS = 0x160000;
const size_t OTA_1_ADDRESS = 0x1b0000;

void sleep(int ms)
{
  vTaskDelay(ms / portTICK_PERIOD_MS);
}

esp_err_t IRAM_ATTR replace_partition_table()
{
  if (PARTITION_TABLE_SIZE != NEW_PARTITION_LEN)
  {
    printf("Unexpected partition table size.\n");
    esp_ota_mark_app_invalid_rollback_and_reboot();
    return ESP_FAIL;
  }

  uint8_t *new_partition_table = NEW_PARTITION;
  if (new_partition_table[0] != 0xAA || new_partition_table[1] != 0x50)
  {
    printf("Incorrect magic number: 0x%x%x\n", new_partition_table[0], new_partition_table[1]);
    esp_ota_mark_app_invalid_rollback_and_reboot();
    return ESP_FAIL;
  }

  esp_err_t err = ESP_FAIL;
  for (int attempts = 1; attempts <= RETRY_LIMIT; attempts++)
  {
    printf("Attempt %d of %d:\n", attempts, RETRY_LIMIT);
    printf("Erasing partition table...\n");
    err = spi_flash_erase_range(PARTITION_TABLE_ADDRESS, PARTITION_TABLE_ALIGNED_SIZE);
    if (err != ESP_OK)
    {
      printf("Failed to erase partition table: 0x%x\n", err);
      sleep(RETRY_WAIT);
      continue;
    }

    printf("Writing partition table...\n");
    err = spi_flash_write(PARTITION_TABLE_ADDRESS, new_partition_table, PARTITION_TABLE_ALIGNED_SIZE);
    if (err != ESP_OK)
    {
      printf("Failed to write partition table: 0x%x\n", err);
      sleep(RETRY_WAIT);
      continue;
    }
    break;
  }
  return err;
}

bool IRAM_ATTR verify_partition_setup()
{
  const esp_partition_t *running_partition = esp_ota_get_running_partition();
  if (running_partition->address != OTA_1_ADDRESS)
  {
    printf("Failed, running partition should be OTA_1.\n");
    return true;
  }
  return true;
}

void IRAM_ATTR partition_change(void)
{
  printf("Hello world!\n");
  printf("Running from partition %s\n", esp_ota_get_running_partition()->label);
  if (!verify_partition_setup())
  {
    printf("Failed to verify partition setup.\n");
    esp_ota_mark_app_invalid_rollback_and_reboot();
    return;
  }
  else
  {
    replace_partition_table();
  }
  fflush(stdout);
}

/*

#include <stdio.h>
#include <cstring>
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_ota_ops.h"
#include "esp32/rom/md5_hash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


const size_t PARTITION_TABLE_ADDRESS = 0x8000;
const size_t PARTITION_TABLE_SIZE = 0xC00;
const size_t PARTITION_TABLE_ALIGNED_SIZE = 0x1000;  // Must be divisible by 4k.

const size_t OTA_DATA_ADDRESS = 0xD000;
const size_t OTA_DATA_SIZE = 0x2000;

const int RETRY_LIMIT = 10;
const int RETRY_WAIT = 100;

void sleep(int ms) {
  vTaskDelay(ms / portTICK_PERIOD_MS);
}

esp_err_t replace_partition_table(uint8_t* new_partition_table) {
  if (new_partition_table[0] != 0xAA || new_partition_table[1] != 0x50) {
    printf("Incorrect magic number: 0x%x%x\n", new_partition_table[0], new_partition_table[1]);
    esp_ota_mark_app_invalid_rollback_and_reboot();
    return ESP_FAIL;
  }


  esp_err_t err = ESP_FAIL;
  for (int attempts = 1; attempts <= RETRY_LIMIT; attempts++) {
    printf("Attempt %d of %d:\n", attempts, RETRY_LIMIT);
    printf("Erasing partition table...\n");
    err = spi_flash_erase_range(PARTITION_TABLE_ADDRESS, PARTITION_TABLE_ALIGNED_SIZE);
    if (err != ESP_OK) {
      printf("Failed to erase partition table: 0x%x\n", err);
      sleep(RETRY_WAIT);
      continue;
    }

    printf("Writing partition table...\n");
    err = spi_flash_write(PARTITION_TABLE_ADDRESS, new_partition_table, PARTITION_TABLE_ALIGNED_SIZE);
    if (err != ESP_OK) {
      printf("Failed to write partition table: 0x%x\n", err);
      sleep(RETRY_WAIT);
      continue;
    }
    break;
  }
  return err;
}


// Partition table entry format: 32 byte in little-endian
// -  2b magic number (string) (0xAA50)
// -  1b type
// -  1b subtype
// -  4b offset (long)
// -  4b size (long)
// - 16b name (string)
// -  4b flags (long)
// Table ends with 32 byte checksum (first 2 bytes magic number (0xEBEB) and the last 16 bytes are a MD5 checksum).
//   Followed by a line with an end maker (0xFF).

const uint8_t TYPE_OFFSET = 2;
const uint8_t SUBTYPE_OFFSET = 3;
const uint8_t LABEL_OFFSET = 12;

const uint8_t TABLE_ENTRY_LENGTH = 32;
const uint8_t LABEL_LENGTH = 16;
const uint8_t CHECKSUM_LENGTH = 16;

const uint8_t CHECKSUM_MAGIC_NUMBER = 0xEB;
const uint8_t END_MARKER = 0xFF;
const uint8_t FIRST_ENTRY_MAGIC_NUMBER = 0xAA;
const uint8_t SECOND_ENTRY_MAGIC_NUMBER = 0x50;

bool prepare_new_partition_table(uint8_t* partition_table) {
  esp_err_t err = spi_flash_read(PARTITION_TABLE_ADDRESS, partition_table, PARTITION_TABLE_ALIGNED_SIZE);
  if (err != ESP_OK) {
    printf("Failed to read partition table: 0x%x\n", err);
    return false;
  }
  const esp_partition_t* running_partition = esp_ota_get_running_partition();

  uint32_t factory_offset = 0;
  uint8_t* factory_label = new uint8_t[16];
  memset(factory_label, 0x00, 16);

  uint32_t other_ota_offset = 0;
  uint8_t other_ota_subtype = 0;
  uint8_t* other_ota_label = new uint8_t[16];
  memset(other_ota_label, 0x00, 16);
  uint32_t ota_count = 0;

  uint32_t checksum_offset = 0;
  uint32_t content_length = 0;
  bool checksum_found = false;
  bool end_marker_found = false;

  for (uint32_t i = 0; i + TABLE_ENTRY_LENGTH <= PARTITION_TABLE_SIZE; i += TABLE_ENTRY_LENGTH) {
    // Read partition table line.
    uint8_t first_char = partition_table[i];
    uint8_t second_char = partition_table[i + 1];
    if (first_char == END_MARKER && second_char == END_MARKER) {
      // End marker.
      if (!checksum_found) {
        printf("No checksum.\n");
        return false;
      }
      end_marker_found = true;
      break;
    } else if (checksum_found) {
      printf("Checksum not followed by end marker.\n");
      return false;
    } else if (first_char == CHECKSUM_MAGIC_NUMBER && second_char == CHECKSUM_MAGIC_NUMBER) {
      // Checksum marker.
      checksum_offset = i + 16;
      content_length = i;
      checksum_found = true;
    } else if (first_char == FIRST_ENTRY_MAGIC_NUMBER && second_char == SECOND_ENTRY_MAGIC_NUMBER) {
      // Partition table entry.
      uint8_t type = partition_table[i + TYPE_OFFSET];
      if (type == ESP_PARTITION_TYPE_APP) {
        uint8_t subtype = partition_table[i + SUBTYPE_OFFSET];
        if (subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
          // We found the factory partition entry. Record the offset and labels.
          factory_offset = i;
          memcpy(factory_label, partition_table + factory_offset + LABEL_OFFSET, LABEL_LENGTH);
        } else if (ESP_PARTITION_SUBTYPE_APP_OTA_MIN <= subtype && subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX) {
          // We found one of the OTA partitions.
          ota_count++;
          if (subtype != running_partition->subtype) {
            // This is "the other" OTA partition. Record offset, subtype and label.
            other_ota_offset = i;
            other_ota_subtype = subtype;
            memcpy(other_ota_label, partition_table + other_ota_offset + LABEL_OFFSET, LABEL_LENGTH);
          }
        }
      }
    } else {
      printf("Illegal partition table line %d: 0x%02x%02x (i: %d)\n", i / TABLE_ENTRY_LENGTH, first_char, second_char, i);
      return false;
    }
  }

  // Verify that the swap can happen.
  if (!end_marker_found){
    printf("No end marker.\n");
    return false;
  }
  if (ota_count != 2) {
    printf("Incorrect amount of OTAs: %d\n", ota_count);
    return false;
  }
  if (factory_offset == 0) {
    printf("No factory partition found.\n");
    return false;
  }
  if (other_ota_offset == 0 || other_ota_subtype == 0) {
    printf("No other OTA partition found.\n");
    return false;
  }
  if (!checksum_found) {
    printf("No checksum found.\n");
    return false;
  }

  // Update the partition table.
  // Swap the subtypes and labels, respectively, of the factory and "other OTA" partition.
  partition_table[other_ota_offset + SUBTYPE_OFFSET] = ESP_PARTITION_SUBTYPE_APP_FACTORY;
  memcpy(partition_table + other_ota_offset + LABEL_OFFSET, factory_label, LABEL_LENGTH);
  partition_table[factory_offset + SUBTYPE_OFFSET] = other_ota_subtype;
  memcpy(partition_table + factory_offset + LABEL_OFFSET, other_ota_label, LABEL_LENGTH);

  // Compute the checksum for the modified partition table.
  MD5Context* context = new MD5Context;
  MD5Init(context);
  MD5Update(context, partition_table, content_length);
  uint8_t* digest = new uint8_t[CHECKSUM_LENGTH];
  MD5Final(digest, context);
  memcpy(partition_table + checksum_offset, digest, CHECKSUM_LENGTH);

  return true;
}

bool verify_partition_setup() {
  const esp_partition_t* running_partition = esp_ota_get_running_partition();
  if (running_partition->type == ESP_PARTITION_TYPE_APP && running_partition->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
    printf("Failed, running partition should be OTA.\n");
    return false;
  }

  // TODO(Lau): If possible, check whether the fallback partition is an OTA partition (this would mean that there is an ESP app in the fallback partition).
  // The above may not be necessary (and impossible due to a lag in the OTA API). Currently, we perform a "factory promote" when we run from the factory partition.
  // This means that there will always be something in the other OTA partition when factory swap runs.

  return true;
}

extern "C" void factory_swap() {
    printf("Swap factory partition!\n");
    if (!verify_partition_setup()) {
      printf("Failed to verify partition setup.\n");
      esp_ota_mark_app_invalid_rollback_and_reboot();
      return;
    }
    printf("Preparing new partition table.\n");
    uint8_t* new_partition_table = new uint8_t[PARTITION_TABLE_ALIGNED_SIZE];
    if (!prepare_new_partition_table(new_partition_table)) {
      printf("Failed to prepare new partition table.\n");
      esp_ota_mark_app_invalid_rollback_and_reboot();
      return;
    }

    printf("Erasing OTA data.\n");
    esp_err_t err = spi_flash_erase_range(OTA_DATA_ADDRESS, OTA_DATA_SIZE);
    if (err != ESP_OK) {
      esp_ota_mark_app_invalid_rollback_and_reboot();
      return;
    }

    printf("Entering critical zone!\n");
    err = replace_partition_table(new_partition_table);
    if (err != ESP_OK) {
      esp_restart();
      return;
    }
    printf("Leaving critical zone!\n");
    printf("Restarting...\n");
    fflush(stdout);
    esp_restart();
}
*/
