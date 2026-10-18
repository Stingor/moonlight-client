#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_tooltip.h"

#include "ui_text.h"

// CE QUE LES TROIS FENÊTRES DU SALON PARTAGENT. En-tête privé : il vit dans
// `src/`, rien hors de cette couche n'en a besoin.
namespace ro::ui::chat_room {

// Un mot du client, terminé par son zéro : ImGui veut une chaîne C.
[[nodiscard]] inline std::string Owned(std::string_view text) { return std::string(text); }

// UN TEXTE VENU D'UN AUTRE JOUEUR, montré sans que ses « ## » deviennent des
// identifiants d'ImGui : ils couperaient le libellé, et « ### » changerait même
// l'identité d'une fenêtre.
[[nodiscard]] inline std::string Shown(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char letter : text) {
    if (letter == '#' && !out.empty() && out.back() == '#') {
      out.push_back(' ');
    }
    out.push_back(letter);
  }
  return out;
}

// La place d'un compteur « 12/36 », et d'une ligne composée.
inline constexpr std::size_t kCounterBytes = 32;
inline constexpr std::size_t kLineBytes = 192;

// Le fond d'un champ qu'une erreur désigne : un rouge pâle, lisible sous le
// texte sombre du corps clair.
inline constexpr ImU32 kFaultyFieldColor = IM_COL32(236, 178, 178, 255);

// Les octets de suite d'un caractère UTF-8 : 10xxxxxx.
inline constexpr unsigned char kUtf8ContinuationMask = 0xC0;
inline constexpr unsigned char kUtf8ContinuationBits = 0x80;
// Le premier octet qui n'est plus de l'ASCII.
inline constexpr unsigned char kFirstNonAscii = 0x80;

// La saisie d'un tampon, jusqu'à son zéro.
[[nodiscard]] inline std::string_view Typed(std::span<const char> buffer) {
  const auto end = std::ranges::find(buffer, '\0');
  return std::string_view(buffer.data(), static_cast<std::size_t>(end - buffer.begin()));
}

// LA COLONNE DES LIBELLÉS : le plus long, DANS LA LANGUE COURANTE, et un
// cadratin d'air. Mesurée sur un seul, un libellé plus long chevaucherait son
// champ.
[[nodiscard]] inline float LabelColumn(std::initializer_list<std::string_view> labels) {
  float widest = 0.0F;
  for (const std::string_view label : labels) {
    widest = std::max(widest, TextWidth(label));
  }
  return widest + ImGui::GetFontSize();
}

[[nodiscard]] inline bool IsAscii(std::string_view text) {
  return std::ranges::none_of(
      text, [](char byte) { return static_cast<unsigned char>(byte) >= kFirstNonAscii; });
}

// CE QU'UN MOT DE PASSE PÈSE SUR LE FIL : un octet par caractère, dans
// l'encodage du serveur. On compte donc les caractères, pas les octets UTF-8.
[[nodiscard]] inline std::size_t PasswordBytes(std::string_view utf8) {
  return static_cast<std::size_t>(std::ranges::count_if(utf8, [](char byte) {
    return (static_cast<unsigned char>(byte) & kUtf8ContinuationMask) != kUtf8ContinuationBits;
  }));
}

// « 12/36 » : les octets du fil saisis sur leur plafond, gris tant que ça
// tient, rouge au-delà.
inline void DrawByteCounter(std::size_t used, std::size_t most) {
  std::array<char, kCounterBytes> text{};
  std::snprintf(text.data(), text.size(), "%zu/%zu", used, most);
  ImGui::PushStyleColor(ImGuiCol_Text, used > most
                                           ? ImGui::ColorConvertU32ToFloat4(PaintingLook().bad_text)
                                           : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted(text.data());
  ImGui::PopStyleColor();
}

}  // namespace ro::ui::chat_room
