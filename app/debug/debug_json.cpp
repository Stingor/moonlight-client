#include "debug/debug_json.h"

#include "ro/core/printed.h"

namespace moonclient {
namespace {

constexpr unsigned char kFirstPrintable = 0x20;

}  // namespace

void JsonWriter::Separate() {
  if (after_key_) {
    after_key_ = false;
    return;
  }
  if (!filled_.empty()) {
    if (filled_.back()) {
      text_ += ',';
    }
    filled_.back() = true;
  }
}

JsonWriter& JsonWriter::BeginObject() {
  Separate();
  text_ += '{';
  filled_.push_back(false);
  return *this;
}

JsonWriter& JsonWriter::EndObject() {
  text_ += '}';
  filled_.pop_back();
  return *this;
}

JsonWriter& JsonWriter::BeginArray() {
  Separate();
  text_ += '[';
  filled_.push_back(false);
  return *this;
}

JsonWriter& JsonWriter::EndArray() {
  text_ += ']';
  filled_.pop_back();
  return *this;
}

JsonWriter& JsonWriter::Key(std::string_view key) {
  String(key);
  text_ += ':';
  after_key_ = true;
  return *this;
}

JsonWriter& JsonWriter::String(std::string_view text) {
  Separate();
  text_ += '"';
  for (const char each : text) {
    const auto byte = static_cast<unsigned char>(each);
    switch (each) {
      case '"':
        text_ += "\\\"";
        break;
      case '\\':
        text_ += "\\\\";
        break;
      case '\n':
        text_ += "\\n";
        break;
      case '\r':
        text_ += "\\r";
        break;
      case '\t':
        text_ += "\\t";
        break;
      default:
        if (byte < kFirstPrintable) {
          text_ += ro::core::Printed("\\u%04x", static_cast<unsigned>(byte));
        } else {
          // L'UTF-8 passe tel quel : JSON l'accepte, et les accents restent lisibles.
          text_ += each;
        }
        break;
    }
  }
  text_ += '"';
  return *this;
}

JsonWriter& JsonWriter::Int(std::int64_t number) {
  Separate();
  text_ += std::to_string(number);
  return *this;
}

JsonWriter& JsonWriter::Bool(bool value) {
  Separate();
  text_ += value ? "true" : "false";
  return *this;
}

JsonWriter& JsonWriter::Null() {
  Separate();
  text_ += "null";
  return *this;
}

}  // namespace moonclient
