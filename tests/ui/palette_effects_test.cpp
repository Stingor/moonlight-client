// L'ONGLET « EFFETS » DE L'ÉDITEUR DE STYLE : le réglage montré, l'effet
// choisi, les bornes et « Assombrir », réservé à un effet additif.

#include "ro/ui/palette_effects.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using ro::ui::ClampedEffectSetting;
using ro::ui::EffectBlockReason;
using ro::ui::EffectDraftOf;
using ro::ui::PaletteEffectBlock;
using ro::ui::PaletteEffectRow;
using ro::ui::PaletteEffectSetting;
using ro::ui::PaletteEffectsState;
using ro::ui::PaletteEffectsView;
using ro::ui::SelectedEffect;

namespace {

constexpr std::uint16_t kGlow = 20;
constexpr std::uint16_t kShrink = 17;
constexpr std::uint16_t kFlame = 164;
constexpr int kHue = 120;

}  // namespace

TEST_CASE("Effets : l'onglet montre ce qu'on règle, sinon ce qui est porté",
          "[ui][palette][effects]") {
  PaletteEffectRow row;
  row.number = kGlow;
  row.worn = PaletteEffectSetting{.hue = kHue};
  PaletteEffectsState state;
  CHECK(EffectDraftOf(state, row) == row.worn);
  state.drafts[kGlow] = PaletteEffectSetting{.saturation = -50};
  CHECK(EffectDraftOf(state, row).saturation == -50);
  CHECK(EffectDraftOf(state, row).hue == 0);
}

TEST_CASE("Effets : l'effet choisi saute ceux qui ne se teignent pas", "[ui][palette][effects]") {
  const std::array<PaletteEffectRow, 3> rows = {
      PaletteEffectRow{.number = kShrink, .blocked = PaletteEffectBlock::kChangesBody},
      PaletteEffectRow{.number = kGlow}, PaletteEffectRow{.number = kFlame}};
  const PaletteEffectsView view{.rows = rows};
  PaletteEffectsState state;
  // Sans choix, le premier qui se teint.
  REQUIRE(SelectedEffect(state, view) != nullptr);
  CHECK(SelectedEffect(state, view)->number == kGlow);
  state.selected = kFlame;
  CHECK(SelectedEffect(state, view)->number == kFlame);
  // Un choix qui ne se teint pas retombe sur le premier qui se teint.
  state.selected = kShrink;
  CHECK(SelectedEffect(state, view)->number == kGlow);
  // Rien à teindre : nul.
  const std::array<PaletteEffectRow, 1> blocked = {rows[0]};
  CHECK(SelectedEffect(state, PaletteEffectsView{.rows = blocked}) == nullptr);
  // Et un effet bloqué dit pourquoi.
  CHECK_FALSE(std::string_view(EffectBlockReason(PaletteEffectBlock::kChangesBody)).empty());
  CHECK(std::string_view(EffectBlockReason(PaletteEffectBlock::kNone)).empty());
}

TEST_CASE("Effets : un réglage se ramène dans les bornes du serveur", "[ui][palette][effects]") {
  const PaletteEffectSetting wild{.hue = 400, .saturation = -300, .lightness = 250, .darken = true};
  const PaletteEffectSetting kept = ClampedEffectSetting(wild, /*additive=*/true);
  CHECK(kept.hue == ro::ui::kPaletteEffectHueMost);
  CHECK(kept.saturation == -ro::ui::kPaletteEffectPercentMost);
  CHECK(kept.lightness == ro::ui::kPaletteEffectPercentMost);
  CHECK(kept.darken);
}

TEST_CASE("Effets : « Assombrir » ne vaut que sur un effet additif", "[ui][palette][effects]") {
  const PaletteEffectSetting asked{.hue = kHue, .darken = true};
  CHECK_FALSE(ClampedEffectSetting(asked, /*additive=*/false).darken);
  CHECK(ClampedEffectSetting(asked, /*additive=*/true).darken);
  CHECK(PaletteEffectSetting{}.neutral());
  CHECK_FALSE(asked.neutral());
}

namespace {

using Rgb = std::array<std::uint8_t, 3>;

// À quelques niveaux près : la TSL se calcule en flottants, et le réglage se
// prend en degrés et en pour-cent entiers.
bool Close(const Rgb& left, const Rgb& right) {
  constexpr int kTolerance = 6;
  for (std::size_t channel = 0; channel < left.size(); ++channel) {
    if (std::abs(static_cast<int>(left[channel]) - static_cast<int>(right[channel])) > kTolerance) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("Effets : la couleur choisie au nuancier devient le réglage qui la donne",
          "[ui][palette][effects]") {
  // L'or d'une aura, mené au bleu, au vert sombre et au rose pâle.
  const Rgb gold = {230, 200, 40};
  for (const Rgb wanted : {Rgb{40, 120, 230}, Rgb{30, 110, 40}, Rgb{240, 170, 200}}) {
    const PaletteEffectSetting setting =
        ro::ui::EffectSettingToward(gold, wanted, PaletteEffectSetting{}, /*additive=*/true);
    CHECK(Close(ro::ui::EffectColorOf(gold, setting), wanted));
  }
  // La couleur d'origine elle-même ne demande aucun réglage.
  CHECK(ro::ui::EffectSettingToward(gold, gold, PaletteEffectSetting{}, true).neutral());
}

TEST_CASE("Effets : le nuancier garde « Assombrir » et respecte les bornes",
          "[ui][palette][effects]") {
  const Rgb gold = {230, 200, 40};
  const PaletteEffectSetting dark{.darken = true};
  CHECK(ro::ui::EffectSettingToward(gold, Rgb{40, 120, 230}, dark, /*additive=*/true).darken);
  CHECK_FALSE(ro::ui::EffectSettingToward(gold, Rgb{40, 120, 230}, dark, false).darken);
  // Un blanc pur depuis un or sombre : la clarté ne dépasse pas sa borne.
  const PaletteEffectSetting bright =
      ro::ui::EffectSettingToward(Rgb{60, 50, 10}, Rgb{255, 255, 255}, {}, true);
  CHECK(bright.lightness <= ro::ui::kPaletteEffectPercentMost);
}

TEST_CASE("Effets : une couleur d'origine grise ne tourne pas, seule sa clarté suit",
          "[ui][palette][effects]") {
  const Rgb grey = {128, 128, 128};
  const PaletteEffectSetting setting =
      ro::ui::EffectSettingToward(grey, Rgb{200, 40, 40}, PaletteEffectSetting{}, true);
  CHECK(setting.hue == 0);
  CHECK(setting.saturation == 0);
}
