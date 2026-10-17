#include "ro/ui/triple_clipboard.h"

#include <charconv>
#include <cstddef>
#include <format>
#include <system_error>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"

#include "imgui.h"

namespace ro::ui {
namespace {

// Ce qui sépare deux nombres.
[[nodiscard]] bool Separates(char letter) {
  return letter == ' ' || letter == '\t' || letter == ',' || letter == ';' || letter == '\n' ||
         letter == '\r';
}

}  // namespace

std::string TripleText(const std::array<float, 3>& values) {
  return std::format("{:.2f} {:.2f} {:.2f}", values[0], values[1], values[2]);
}

std::optional<std::array<float, 3>> ParseTriple(std::string_view text) {
  std::array<float, 3> values{};
  std::size_t count = 0;
  std::size_t at = 0;
  while (at < text.size()) {
    if (Separates(text[at])) {
      ++at;
      continue;
    }
    if (count == values.size()) {
      return std::nullopt;
    }
    float value = 0.0F;
    const auto [end, error] = std::from_chars(text.data() + at, text.data() + text.size(), value);
    if (error != std::errc() || (end != text.data() + text.size() && !Separates(*end))) {
      return std::nullopt;
    }
    values[count++] = value;
    at = static_cast<std::size_t>(end - text.data());
  }
  if (count != values.size()) {
    return std::nullopt;
  }
  return values;
}

bool CopyPasteTriple(const char* id, std::array<float, 3>& values, const SkinArt& art) {
  ImGui::PushID(id);
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Copier"), art)) {
    ImGui::SetClipboardText(TripleText(values).c_str());
  }
  ImGui::SameLine();
  bool pasted = false;
  if (SkinSmallButton(core::Tr("Coller"), art)) {
    const char* clipboard = ImGui::GetClipboardText();
    if (clipboard != nullptr) {
      if (const std::optional<std::array<float, 3>> read = ParseTriple(clipboard);
          read.has_value()) {
        values = *read;
        pasted = true;
      }
    }
  }
  ImGui::PopID();
  return pasted;
}

}  // namespace ro::ui
