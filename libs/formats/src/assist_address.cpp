#include "ro/formats/assist_address.h"

namespace ro::formats {
namespace {

constexpr std::string_view kSchemeMark = "://";
constexpr std::string_view kPlainScheme = "http://";

[[nodiscard]] bool Blank(char letter) {
  return letter == ' ' || letter == '\t' || letter == '\r' || letter == '\n';
}

}  // namespace

std::string AssistOrigin(std::string_view address) {
  while (!address.empty() && (Blank(address.back()) || address.back() == '/')) {
    address.remove_suffix(1);
  }
  while (!address.empty() && Blank(address.front())) {
    address.remove_prefix(1);
  }
  if (address.empty()) {
    return {};
  }
  if (address.find(kSchemeMark) != std::string_view::npos) {
    return std::string(address);
  }
  return std::string(kPlainScheme) + std::string(address);
}

}  // namespace ro::formats
