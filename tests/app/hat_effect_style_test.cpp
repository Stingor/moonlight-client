// L'ONGLET « EFFETS » CÔTÉ JEU : le réglage tel que le fil le porte, l'inverse,
// la teinture du dessin, et le nom lisible d'un effet.

#include "character/hat_effect_style.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/player_style_layers.h"
#include "ro/render/effect_dye.h"
#include "ro/sim/actor.h"
#include "ro/sim/style.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "support/effect_builder.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/temp_directory.h"

namespace {

constexpr std::uint16_t kGlowOfNewYear = 20;

}  // namespace

TEST_CASE("Un réglage d'effet part au calque 8, profil 4, rang 0", "[hat_effect_style]") {
  const ro::ui::PaletteEffectSetting setting{
      .hue = 120, .saturation = -30, .lightness = -45, .darken = true};
  const ro::sim::StyleLayerRecipe recipe = moonclient::HatEffectRecipeOf(kGlowOfNewYear, setting);
  CHECK(recipe.layer == ro::sim::StyleLayerKind::kHatEffect);
  CHECK(recipe.key == kGlowOfNewYear);
  CHECK(recipe.profile == ro::net::kStyleLayerEffectProfile);
  CHECK(recipe.adjusts[0].hue == 120);
  CHECK(recipe.adjusts[0].saturation == -30);
  CHECK(recipe.adjusts[0].value == -45);
  // « Assombrir » voyage dans l'octet de l'absolu.
  CHECK(recipe.adjusts[0].absolute);
  for (std::size_t rank = 1; rank < recipe.adjusts.size(); ++rank) {
    CHECK(recipe.adjusts[rank].hue == 0);
  }
  // Et l'aller-retour rend le réglage.
  CHECK(moonclient::HatEffectSettingOf(recipe) == setting);
}

TEST_CASE("Un réglage d'effet donne la teinture que le dessin applique", "[hat_effect_style]") {
  const ro::ui::PaletteEffectSetting setting{.hue = 60, .lightness = -20};
  CHECK(moonclient::HatEffectDyeOf(setting) == ro::render::MakeEffectDye(60, 0, -20, false));
  CHECK(ro::render::IsNeutralEffectDye(moonclient::HatEffectDyeOf({})));
}

TEST_CASE("Le nom d'un effet se lit sans son préfixe", "[hat_effect_style]") {
  CHECK(moonclient::HatEffectDisplayName("HAT_EF_Glow_Of_New_Year") == "Glow Of New Year");
  CHECK(moonclient::HatEffectDisplayName("hat_ef_SNOW") == "SNOW");
  CHECK(moonclient::HatEffectDisplayName("AUTRE") == "AUTRE");
}

TEST_CASE("La couleur d'origine d'un effet est celle de ce qu'il peint", "[hat_effect_style]") {
  // Un `.str` d'une couche, une texture rouge sous une clé blanche, en addition.
  ro::test_support::StrLayerSpec layer;
  layer.texture_names = {"rouge.tga"};
  layer.key_frames.push_back(ro::test_support::StrKeyFrameSpec{});
  ro::test_support::StrBuildOptions options;
  options.layers = {layer};
  constexpr std::int32_t kSide = 4;
  const std::vector<ro::test_support::TgaPixel> pixels(static_cast<std::size_t>(kSide * kSide),
                                                       ro::test_support::TgaPixel{200, 0, 0, 255});
  const std::string table =
      "version: 1\n"
      "hat_effects:\n"
      "  1: {name: 'HAT_EF_ESSAI', file: 'essai\\rouge.str'}\n";
  const ro::test_support::TempDirectory client;
  client.WriteFile(
      "essai.grf",
      ro::test_support::BuildGrf(
          {ro::test_support::GrfFileSpec{"data\\yaml_files\\hat_effects.yml",
                                         std::vector<std::uint8_t>(table.begin(), table.end())},
           ro::test_support::GrfFileSpec{"data\\texture\\effect\\essai\\rouge.str",
                                         ro::test_support::BuildStr(options)},
           ro::test_support::GrfFileSpec{"data\\texture\\effect\\essai\\rouge.tga",
                                         ro::test_support::BuildTga(kSide, kSide, pixels)}}));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"essai.grf"}});
  REQUIRE(vfs.has_value());

  ro::sim::World world;
  constexpr std::uint32_t kPlayerId = 2000021;
  ro::sim::Actor player;
  player.id = kPlayerId;
  player.kind = ro::sim::ActorKind::kPlayer;
  player.hat_effects = {1};
  world.Spawn(player);
  world.set_player_id(kPlayerId);

  moonclient::HatEffectStyle style;
  style.SetSources(&*vfs, nullptr);
  ro::ui::PaletteEffectsState state;
  constexpr std::uint32_t kNowMs = 1000;
  style.Refresh(world, state, true, true, true, nullptr, kNowMs);
  REQUIRE(style.view().rows.size() == 1);
  const ro::ui::PaletteEffectRow& row = style.view().rows.front();
  CHECK(row.name == "ESSAI");
  CHECK(row.additive);
  // Portée à plein éclat : un rouge pur.
  REQUIRE(row.base_color.has_value());
  CHECK(*row.base_color == std::array<std::uint8_t, 3>{255, 0, 0});
}
