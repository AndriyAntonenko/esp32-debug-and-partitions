#include "zero_division.h"
#include <Arduino.h>

namespace
{
  const int TRIGGER_DIVISION_BY_ZERO_AT = 10;
  const int DIVISION_ITERATION_DELAY = 500;
  volatile int divisor = 0;
  int divisionIteration = 0;

}

void __attribute__((noinline)) zero_division::triggerZeroDivisionIssue()
{
  if (divisionIteration >= TRIGGER_DIVISION_BY_ZERO_AT)
  {
    Serial.flush(); // <-- ensuring that serial is passed
    Serial.println(divisionIteration / divisor);
  }
  delay(DIVISION_ITERATION_DELAY);
  divisionIteration++;
}