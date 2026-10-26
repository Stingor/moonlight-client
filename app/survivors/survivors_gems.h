#pragma once

#include <array>
#include <cstdint>

namespace moonclient {

// LES GEMMES D'EXPÉRIENCE (U26) : la valeur se lit à la couleur. Une gemme
// d'une unité est bleue, puis jaune, rouge, et l'Emperium porte les gemmes
// fusionnées les plus lourdes. Constantes d'`app` et non règles : la couleur
// ne change rien à la partie, et le Codex la lit sans l'arène.
inline constexpr std::uint16_t kBlueGemstoneItem = 717;
inline constexpr std::uint16_t kYellowGemstoneItem = 715;
inline constexpr std::uint16_t kRedGemstoneItem = 716;
inline constexpr std::uint16_t kEmperiumItem = 714;
// Le premier nombre d'XP de chaque couleur au-delà du bleu.
inline constexpr std::uint32_t kBlueGemFromExp = 1;
inline constexpr std::uint32_t kYellowGemFromExp = 2;
inline constexpr std::uint32_t kRedGemFromExp = 10;
inline constexpr std::uint32_t kEmperiumFromExp = 50;

// L'objet qui montre une gemme de `exp` points.
[[nodiscard]] constexpr std::uint16_t GemItemOf(std::uint32_t exp) noexcept {
  if (exp >= kEmperiumFromExp) return kEmperiumItem;
  if (exp >= kRedGemFromExp) return kRedGemstoneItem;
  if (exp >= kYellowGemFromExp) return kYellowGemstoneItem;
  return kBlueGemstoneItem;
}

// Une couleur de gemme : son objet, et le premier nombre d'XP qui la montre.
struct GemColour {
  std::uint16_t item_id = 0;
  std::uint32_t from_exp = 0;
};
// Les quatre couleurs, par seuil croissant.
inline constexpr std::array<GemColour, 4> kGemColours{{
    {.item_id = kBlueGemstoneItem, .from_exp = kBlueGemFromExp},
    {.item_id = kYellowGemstoneItem, .from_exp = kYellowGemFromExp},
    {.item_id = kRedGemstoneItem, .from_exp = kRedGemFromExp},
    {.item_id = kEmperiumItem, .from_exp = kEmperiumFromExp},
}};

}  // namespace moonclient
