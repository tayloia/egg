#include "yolk.h"

std::string egg::yolk::String::fromEnum(int value, const StringFromEnum* tableBegin, const StringFromEnum* tableEnd) {
  for (auto* entry = tableBegin; entry != tableEnd; ++entry) {
    // First scan the entire table for an exact match
    if (entry->value == value) {
      return entry->text;
    }
  }
  std::string result;
  for (auto* entry = tableBegin; entry != tableEnd; ++entry) {
    // Now scan for bit fields
    if ((entry->value != 0) && ((entry->value & value) == entry->value)) {
      if (!result.empty()) {
        result += '|';
      }
      result += entry->text;
      value ^= entry->value;
      if (value == 0) {
        return result;
      }
    }
  }
  // Append the remaining numeric value
  if (!result.empty()) {
    result += '|';
  }
  result += std::to_string(value);
  return result;
}