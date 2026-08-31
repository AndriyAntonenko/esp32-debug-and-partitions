#include <Arduino.h>
#include "zero_division.h"
#include "nullptr.h"

// Set CRASH_SCENARIO to the scenario you want to reproduce, then rebuild.
#define CRASH_NONE 0
#define CRASH_NULL_POINTER 1
#define CRASH_DIVIDE_BY_ZERO 2

#define CRASH_SCENARIO CRASH_NULL_POINTER

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("Hello! I am useless program, that will crash. Pick a scenario via CRASH_SCENARIO to read that trace, and know something about debugging.");

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
