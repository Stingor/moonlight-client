// LE CODE D'UN THÈME : copié par un joueur, collé par un autre, il rend le même
// habillage ; une ligne qui n'en est pas un ne pose rien.

#include "options/skin_look_code.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "options/skin_look_colors.h"
#include "options/skin_look_options.h"

using moonclient::ReadSkinLookCode;
using moonclient::SkinLookCode;
using moonclient::SkinLookPresets;
using ro::ui::SkinLook;

namespace {

constexpr int kSombre = 1;
// « moonlook5: », puis les deux chiffres du thème.
constexpr std::size_t kThemeAt = 10;
constexpr std::size_t kThemeDigits = 2;

// Huit chiffres hexadécimaux.
[[nodiscard]] std::string Hex(std::uint32_t value) {
  std::array<char, 9> text{};
  std::snprintf(text.data(), text.size(), "%08X", value);
  return text.data();
}

}  // namespace

TEST_CASE("Code de thème : copié puis collé, l'habillage revient à l'identique", "[app][skin]") {
  SkinLook look = SkinLookPresets()[kSombre];
  look.paper = IM_COL32(12, 34, 56, 200);
  look.opacity = 0.85F;
  const std::string code = SkinLookCode(look, kSombre);
  CHECK(code.starts_with("moonlook5:"));
  // Les blancs qu'un copier-coller ajoute ne comptent pas.
  const auto shared = ReadSkinLookCode("  " + code + "\n");
  REQUIRE(shared.has_value());
  CHECK(shared->theme_rank == kSombre);
  CHECK(shared->look == look);
  // Un thème collé tel quel se reconnaît encore : la luminosité est exacte.
  const auto plain = ReadSkinLookCode(SkinLookCode(SkinLookPresets()[kSombre], kSombre));
  REQUIRE(plain.has_value());
  CHECK(plain->look == SkinLookPresets()[kSombre]);
}

TEST_CASE("Code de thème : une ligne qui n'en est pas un ne pose rien", "[app][skin]") {
  const std::string code = SkinLookCode(SkinLook{}, 0);
  CHECK_FALSE(ReadSkinLookCode("").has_value());
  CHECK_FALSE(ReadSkinLookCode("bonjour").has_value());
  CHECK_FALSE(ReadSkinLookCode(code.substr(0, code.size() - 1)).has_value());
  CHECK_FALSE(ReadSkinLookCode("moonlook9:" + code.substr(kThemeAt)).has_value());
  std::string unknown_theme = code;
  unknown_theme.replace(kThemeAt, kThemeDigits, "07");
  CHECK_FALSE(ReadSkinLookCode(unknown_theme).has_value());
  std::string not_hex = code;
  not_hex.back() = 'G';
  CHECK_FALSE(ReadSkinLookCode(not_hex).has_value());
}

TEST_CASE("Code de thème : un code ancien prend de son thème les couleurs venues depuis",
          "[app][skin]") {
  // UN CODE DE LA PREMIÈRE VERSION : la palette d'origine seule, par clé.
  constexpr std::uint32_t kRgba = 0x11223344U;
  std::vector<std::string_view> keys;
  for (const auto& color : moonclient::skin_look::kColorRows) {
    if (color.since == moonclient::skin_look::kOriginalVersion) {
      keys.emplace_back(color.key);
    }
  }
  std::ranges::sort(keys);
  std::string old = "moonlook1:01" + Hex(std::bit_cast<std::uint32_t>(1.0F)) +
                    Hex(std::bit_cast<std::uint32_t>(1.0F));
  for (std::size_t rank = 0; rank < keys.size(); ++rank) {
    old += Hex(kRgba);
  }
  const auto shared = ReadSkinLookCode(old);
  REQUIRE(shared.has_value());
  const SkinLook& dark = SkinLookPresets()[kSombre];
  CHECK(shared->look.body == IM_COL32(0x11, 0x22, 0x33, 0x44));
  CHECK(shared->look.link == dark.link);
  CHECK(shared->look.good_text == dark.good_text);
}
