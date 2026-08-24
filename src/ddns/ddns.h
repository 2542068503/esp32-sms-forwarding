#pragma once
#include <Arduino.h>

class Ddns {
public:
  static void init();
  static void tick();
  static void forceUpdate();
  static String lastIpv6();
  static bool lastUpdateSuccess();
  static unsigned long lastUpdateTime();
};
