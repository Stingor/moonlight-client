#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ro::ui {

// LE CODE D'UNE COULEUR, tel qu'un joueur le copie et le colle : `0xRRGGBB`.
//
// On l'ÉCRIT toujours ainsi, chiffres en majuscules. On le LIT sous toutes les
// formes qu'un joueur a sous la main : `0xRRGGBB`, `#RRGGBB`, `^RRGGBB` — la
// couleur d'un dialogue de PNJ — ou les six chiffres seuls, casse et espaces
// autour indifférents.

// La place qu'un code écrit occupe, zéro final compris.
inline constexpr std::size_t kColorCodeSize = 9;

[[nodiscard]] std::array<char, kColorCodeSize> FormatColorCode(std::array<std::uint8_t, 3> rgb);

// 🔴 SIX CHIFFRES, NI PLUS NI MOINS : un code à moitié tapé ne se lit pas, et
// ne repeint donc rien en chemin. La forme courte à trois chiffres n'est pas
// admise pour la même raison : elle est une étape de la frappe des six.
[[nodiscard]] std::optional<std::array<std::uint8_t, 3>> ParseColorCode(std::string_view text);

}  // namespace ro::ui
