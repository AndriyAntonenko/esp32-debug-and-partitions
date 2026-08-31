#include <Arduino.h>

// Set CRASH_SCENARIO to the scenario you want to reproduce, then rebuild.
#define CRASH_NONE 0
#define CRASH_NULL_POINTER 1
#define CRASH_DIVIDE_BY_ZERO 2

#define CRASH_SCENARIO CRASH_DIVIDE_BY_ZERO

void __attribute__((noinline)) triggerNullPointerIssue_level3()
{
  int *p = nullptr;
  *p = 42;
}

void __attribute__((noinline)) triggerNullPointerIssue_level2()
{
  triggerNullPointerIssue_level3();
}

void __attribute__((noinline)) triggerNullPointerIssue_level1()
{
  triggerNullPointerIssue_level2();
}

void __attribute__((noinline)) triggerNullPointerIssue()
{
  Serial.flush(); // <-- ensuring that serial is passed
  triggerNullPointerIssue_level1();
}

const int TRIGGER_DIVISION_BY_ZERO_AT = 10;
const int DIVISION_ITERATION_DELAY = 500;
volatile int divisor = 0;
int divisionIteration = 0;

void __attribute__((noinline)) triggerZeroDivisionIssue()
{
  if (divisionIteration >= TRIGGER_DIVISION_BY_ZERO_AT)
  {
    Serial.flush(); // <-- ensuring that serial is passed
    Serial.println(divisionIteration / divisor);
  }
  delay(DIVISION_ITERATION_DELAY);
  divisionIteration++;
}

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("Hello! I am useless program, that will crash. Pick a scenario via CRASH_SCENARIO to read that trace, and know something about debugging.");

#if CRASH_SCENARIO == CRASH_NULL_POINTER
  Serial.println("Scenario: null pointer write (StoreProhibited)");
  triggerNullPointerIssue();
#elif CRASH_SCENARIO == CRASH_DIVIDE_BY_ZERO
  Serial.println("Scenario: integer division by zero (IntegerDivideByZero)");
#else
  Serial.println("Scenario: none, nothing will crash");
#endif
}

void loop()
{
#if CRASH_SCENARIO == CRASH_DIVIDE_BY_ZERO
  triggerZeroDivisionIssue();
#endif
}
