#include "ro/ui/triple_clipboard.h"

#include <charconv>
#include <cstddef>
#include <format>
#include <system_error>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"

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

std::optional<std::array<float, 3>> ClipboardTriple() {
  const char* clipboard = ImGui::GetClipboardText();
  if (clipboard == nullptr) {
    return std::nullopt;
  }
  return ParseTriple(clipboard);
}

bool TripleClipboardMenu(const char* id, std::array<float, 3>& values) {
  // Le champ est un groupe, sans identifiant : le menu prend un nom, sous `id`.
  ImGui::PushID(id);
  bool pasted = false;
  if (BeginSkinPopupContextItem("##presse_papiers")) {
    if (ImGui::MenuItem(core::Tr("Copier"))) {
      ImGui::SetClipboardText(TripleText(values).c_str());
    }
    const std::optional<std::array<float, 3>> read = ClipboardTriple();
    if (ImGui::MenuItem(core::Tr("Coller"), nullptr, false, read.has_value())) {
      values = *read;
      pasted = true;
    }
    EndSkinPopup();
  }
  ImGui::PopID();
  return pasted;
}

bool PositionClipboardMenu(const char* id, std::array<float, 3>& native, MapCubes cubes) {
  std::array<float, 3> cells = CellsOfNative(native, cubes);
  if (!TripleClipboardMenu(id, cells)) {
    return false;
  }
  native = NativeOfCells(cells, cubes);
  return true;
}

}  // namespace ro::ui
