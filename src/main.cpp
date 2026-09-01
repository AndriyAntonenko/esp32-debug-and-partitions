#include <Arduino.h>
#include "zero_division.h"
#include "nullptr.h"
#include "partitions.h"
#include "esp_partition.h"
#include <esp_ota_ops.h>

// Set CRASH_SCENARIO to the scenario you want to reproduce, then rebuild.
#define CRASH_NONE 0
#define CRASH_NULL_POINTER 1
#define CRASH_DIVIDE_BY_ZERO 2

#define CRASH_SCENARIO CRASH_NULL_POINTER

void printCurrentPartitionTable();

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("Hello! I am useless program, that will crash. Pick a scenario via CRASH_SCENARIO to read that trace, and know something about debugging.");

  partitions::readPartitionsTableHighLevel();
  partitions::readPartitionsTableLowLevel();

#if CRASH_SCENARIO == CRASH_NULL_POINTER
  Serial.println("Scenario: null pointer write (StoreProhibited)");
  nullptr_issue::triggerNullPointerIssue();
#elif CRASH_SCENARIO == CRASH_DIVIDE_BY_ZERO
  Serial.println("Scenario: integer division by zero (IntegerDivideByZero)");
#else
  Serial.println("Scenario: none, nothing will crash");
#endif
}

void loop()
{
#if CRASH_SCENARIO == CRASH_DIVIDE_BY_ZERO
  zero_division::triggerZeroDivisionIssue();
#endif
}

const char *typeName(uint8_t type)
{
  switch (type)
  {
  case ESP_PARTITION_TYPE_APP:
    return "app";
  case ESP_PARTITION_TYPE_DATA:
    return "data";
  default:
    return "?";
  }
}

const char *subtypeName(uint8_t type, uint8_t subtype)
{
  if (type == ESP_PARTITION_TYPE_APP)
  {
    if (subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY)
      return "factory";
    if (subtype == ESP_PARTITION_SUBTYPE_APP_TEST)
      return "test";
    if (subtype >= 0x10 && subtype <= 0x1f)
    {
      static char buf[8];
      snprintf(buf, sizeof(buf), "ota_%u", subtype - 0x10);
      return buf;
    }
    return "?";
  }

  switch (subtype)
  {
  case ESP_PARTITION_SUBTYPE_DATA_OTA:
    return "ota";
  case ESP_PARTITION_SUBTYPE_DATA_PHY:
    return "phy";
  case ESP_PARTITION_SUBTYPE_DATA_NVS:
    return "nvs";
  case ESP_PARTITION_SUBTYPE_DATA_COREDUMP:
    return "coredump";
  case ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS:
    return "nvs_keys";
  case ESP_PARTITION_SUBTYPE_DATA_FAT:
    return "fat";
  case ESP_PARTITION_SUBTYPE_DATA_SPIFFS:
    return "spiffs";
  default:
    return "?";
  }
}

void printCurrentPartitionTable()
{
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);

  if (it == NULL)
  {
    Serial.println("[WARN]: no partitions found");
    return;
  }

  const esp_partition_t *running = esp_ota_get_running_partition();

  Serial.println("[INFO]: Searching for partitions...");
  Serial.printf("%-16s %-5s %-11s %-12s %9s\n",
                "Label", "Type", "SubType", "Address", "Size");
  for (int i = 0; i < 60; i++)
    Serial.print('-');
  Serial.println();

  while (it != NULL)
  {
    const esp_partition_t *partition = esp_partition_get(it);

    Serial.printf("%-16s %-5s %-11s 0x%08x %6u KB%s\n",
                  partition->label,
                  typeName(partition->type),
                  subtypeName(partition->type, partition->subtype),
                  partition->address,
                  partition->size / 1024,
                  (partition == running) ? "  <-- running" : "");

    it = esp_partition_next(it);
  }

  esp_partition_iterator_release(it);
  for (int i = 0; i < 60; i++)
    Serial.print('-');
  Serial.print("\n");
}