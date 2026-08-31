#include "nullptr.h"
#include <Arduino.h>

namespace
{
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

}

void __attribute__((noinline)) nullptr_issue::triggerNullPointerIssue()
{
  Serial.flush(); // <-- ensuring that serial is passed
  triggerNullPointerIssue_level1();
}