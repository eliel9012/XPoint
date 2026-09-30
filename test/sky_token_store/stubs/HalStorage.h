#pragma once

#include "TestStorage.h"

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }
  bool exists(const char*) { return fake::sdExists; }
  bool remove(const char*) {
    if (fake::sdRemoveFails) return false;
    fake::sdExists = false;
    return true;
  }
};

#define Storage HalStorage::getInstance()
