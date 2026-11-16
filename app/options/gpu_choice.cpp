#include "options/gpu_choice.h"

#include <charconv>
#include <cstddef>
#include <string>
#include <system_error>

namespace moonclient {
namespace {

// Le champ que Windows réserve à la carte, dans une chaîne de champs « clé=valeur; ».
constexpr std::string_view kPreferenceField = "GpuPreference";
constexpr char kFieldEnd = ';';
constexpr char kValueMark = '=';

[[nodiscard]] bool IsKnown(int value) {
  return value == static_cast<int>(GpuPreference::kAutomatic) ||
         value == static_cast<int>(GpuPreference::kPowerSaving) ||
         value == static_cast<int>(GpuPreference::kHighPerformance);
}

// Appelle `visit(champ, nom, valeur)` pour chaque champ non vide de la chaîne,
// dans l'ordre. `champ` est le texte entier, tel qu'écrit.
template <typename Visit>
void ForEachField(std::string_view windows_value, Visit&& visit) {
  while (!windows_value.empty()) {
    const std::size_t end = windows_value.find(kFieldEnd);
    const std::string_view field = windows_value.substr(0, end);
    windows_value =
        end == std::string_view::npos ? std::string_view{} : windows_value.substr(end + 1);
    if (field.empty()) {
      continue;
    }
    const std::size_t mark = field.find(kValueMark);
    if (mark == std::string_view::npos) {
      visit(field, field, std::string_view{});
    } else {
      visit(field, field.substr(0, mark), field.substr(mark + 1));
    }
  }
}

}  // namespace

GpuPreference GpuPreferenceIn(std::string_view windows_value) {
  GpuPreference found = GpuPreference::kAutomatic;
  ForEachField(windows_value, [&found](std::string_view /*field*/, std::string_view name,
                                       std::string_view value) {
    if (name != kPreferenceField) {
      return;
    }
    int number = 0;
    const auto [end, failed] = std::from_chars(value.data(), value.data() + value.size(), number);
    if (failed == std::errc{} && end == value.data() + value.size() && IsKnown(number)) {
      found = static_cast<GpuPreference>(number);
    }
  });
  return found;
}

std::string WithGpuPreference(std::string_view windows_value, GpuPreference wanted) {
  std::string rewritten;
  ForEachField(windows_value, [&rewritten](std::string_view field, std::string_view name,
                                           std::string_view /*value*/) {
    if (name == kPreferenceField) {
      return;
    }
    rewritten += field;
    rewritten += kFieldEnd;
  });
  if (wanted != GpuPreference::kAutomatic) {
    rewritten += kPreferenceField;
    rewritten += kValueMark;
    rewritten += std::to_string(static_cast<int>(wanted));
    rewritten += kFieldEnd;
  }
  return rewritten;
}

GpuPreference GpuPreferenceAt(int rank, GpuPreference kept) {
  return IsKnown(rank) ? static_cast<GpuPreference>(rank) : kept;
}

}  // namespace moonclient
