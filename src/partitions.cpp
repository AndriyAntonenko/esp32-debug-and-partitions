#include <Arduino.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_flash.h>

#include "partitions.h"

namespace
{
  const size_t PARTITION_NAME_LENGTH = 16;
  const uint32_t PARTITION_TABLE_OFFSET = 0x8000;
  const uint32_t PARTITION_TABLE_SIZE = 0xC00;
  const uint32_t PARTITION_MAGIC_MD5 = 0xEBEB;
  const uint32_t PARTITION_MAGIC = 0x50AA;

  struct __attribute__((packed)) RawPartitionEntry
  {
    uint16_t magic;
    uint8_t type;
    uint8_t subtype;
    uint32_t offset;
    uint32_t size;
    char label[PARTITION_NAME_LENGTH];
    uint32_t flags;
  };

  static_assert(sizeof(RawPartitionEntry) == 32, "a partition table entry must stay 32 bytes");

  void formatType(uint8_t type, char *out, size_t length)
  {
    switch (type)
    {
    case ESP_PARTITION_TYPE_APP:
      snprintf(out, length, "app");
      break;
    case ESP_PARTITION_TYPE_DATA:
      snprintf(out, length, "data");
      break;
    default:
      snprintf(out, length, "0x%02x", type);
      break;
    }
  }

  void formatSubtype(uint8_t type, uint8_t subtype, char *out, size_t length)
  {
    if (type == ESP_PARTITION_TYPE_APP)
    {
      if (subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_0 && subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_15)
      {
        snprintf(out, length, "ota_%u", subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0);
        return;
      }

      switch (subtype)
      {
      case ESP_PARTITION_SUBTYPE_APP_FACTORY:
        snprintf(out, length, "factory");
        return;
      case ESP_PARTITION_SUBTYPE_APP_TEST:
        snprintf(out, length, "test");
        return;
      }
    }

    if (type == ESP_PARTITION_TYPE_DATA)
    {
      switch (subtype)
      {
      case ESP_PARTITION_SUBTYPE_DATA_OTA:
        snprintf(out, length, "ota");
        return;
      case ESP_PARTITION_SUBTYPE_DATA_PHY:
        snprintf(out, length, "phy");
        return;
      case ESP_PARTITION_SUBTYPE_DATA_NVS:
        snprintf(out, length, "nvs");
        return;
      case ESP_PARTITION_SUBTYPE_DATA_COREDUMP:
        snprintf(out, length, "coredump");
        return;
      case ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS:
        snprintf(out, length, "nvs_keys");
        return;
      case ESP_PARTITION_SUBTYPE_DATA_FAT:
        snprintf(out, length, "fat");
        return;
      case ESP_PARTITION_SUBTYPE_DATA_SPIFFS:
        snprintf(out, length, "spiffs");
        return;
      }
    }

    snprintf(out, length, "0x%02x", subtype);
  }

  void printHeader()
  {
    Serial.println("Label            Type  SubType     Address           Size");
    Serial.println("------------------------------------------------------------");
  }

  void printFooter()
  {
    Serial.println("------------------------------------------------------------");
  }

  void printRow(const char *label, uint8_t type, uint8_t subtype, uint32_t address, uint32_t size, bool isRunning)
  {
    char typeText[8];
    char subtypeText[12];

    formatType(type, typeText, sizeof(typeText));
    formatSubtype(type, subtype, subtypeText, sizeof(subtypeText));

    Serial.printf("%-17s%-6s%-12s0x%08x%7u KB%s\n",
                  label,
                  typeText,
                  subtypeText,
                  address,
                  size / 1024,
                  isRunning ? "  <-- running" : "");
  }
}

// Walks the partition table through the esp_partition API. The table is already
// parsed and cached by the IDF, so every entry arrives as a ready esp_partition_t.
void partitions::readPartitionsTableHighLevel()
{
  Serial.println("[INFO]: Searching for partitions...");

  const esp_partition_t *running = esp_ota_get_running_partition();

  printHeader();

  esp_partition_iterator_t iterator = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  while (iterator != NULL)
  {
    const esp_partition_t *partition = esp_partition_get(iterator);

    printRow(partition->label,
             partition->type,
             partition->subtype,
             partition->address,
             partition->size,
             running != NULL && partition->address == running->address);

    iterator = esp_partition_next(iterator);
  }
  esp_partition_iterator_release(iterator);

  printFooter();
}

// Reads the same table straight from flash at 0x8000 with esp_flash_read()
// and parses the 32-byte entries by hand.
void partitions::readPartitionsTableLowLevel()
{

  printHeader();
  size_t entry_size = sizeof(RawPartitionEntry);
  size_t partitions_count = PARTITION_TABLE_SIZE / entry_size;

  for (uint32_t slot = 0; slot < partitions_count; slot++)
  {
    const uint32_t address = PARTITION_TABLE_OFFSET + slot * entry_size;
    RawPartitionEntry entry;
    const esp_err_t error = esp_flash_read(NULL, &entry, address, entry_size);

    if (error != ESP_OK)
    {
      Serial.printf("[ERROR]: esp_flash_read failed at 0x%08x: %s\n", address, esp_err_to_name(error));
      break;
    }

    if (entry.magic == PARTITION_MAGIC_MD5 || entry.magic != PARTITION_MAGIC)
    {
      break;
    }

    printRow(entry.label, entry.type, entry.subtype, entry.offset, entry.size, false);
  }

  printFooter();
}
