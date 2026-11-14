// LA FLAMME D'UNE TORCHE DE CARTE, effet 47.
//
// Elle est la seule figure dont l'échelle vienne de l'objet du RSW : le semeur
// de la carte porte son paramètre jusqu'au nœud, et une torche lancée sans
// semeur garde celle du patron.

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_floor_spinner.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_track.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildGrf;
using ro::test_support::BuildSpr;
using ro::test_support::BuildTga;
using ro::test_support::GrfFileSpec;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::uint32_t kTorchEffect = 47;
// Ce que porte le patron engendré : le couple `.spr`/`.act` de la torche.
constexpr std::string_view kTorchSprite = "data\\sprite\\이팩트\\torch_01";
// L'échelle que portent les torches d'in_sphinx1, lue sur leur objet du RSW.
constexpr float kSphinxScale = 0.6F;
constexpr std::int32_t kTorchPixels = 8;

std::vector<std::uint8_t> TorchSpr() {
  SprBuildOptions options;
  options.indexed.push_back(SprIndexedFrameSpec{
      kTorchPixels, kTorchPixels,
      std::vector<std::uint8_t>(static_cast<std::size_t>(kTorchPixels * kTorchPixels), 1)});
  options.palette = {SpriteColor{}, SpriteColor{.red = 255, .green = 180, .blue = 0}};
  return BuildSpr(options);
}

std::vector<std::uint8_t> TorchAct() {
  ActBuildOptions options;
  options.actions.push_back(ActActionSpec{{ActFrameSpec{{ActLayerSpec{}}, {}, -1}}});
  options.speeds = {1.0F};
  return BuildAct(options);
}

// Un client qui ne porte que la torche, sous le chemin que le patron nomme.
void FillTorchClient(const TempDirectory& client) {
  const auto cp949 = [](std::string_view utf8) {
    auto converti = ro::vfs::Utf8ToCp949(utf8);
    REQUIRE(converti.has_value());
    return *converti;
  };
  client.WriteFile("torche.grf",
                   BuildGrf({GrfFileSpec{cp949(std::string(kTorchSprite) + ".spr"), TorchSpr()},
                             GrfFileSpec{cp949(std::string(kTorchSprite) + ".act"), TorchAct()}}));
}

// L'échelle du seul nœud semé.
float SownScale(float instance_scale) {
  const TempDirectory client;
  FillTorchClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"torche.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.PlayProcedural(kTorchEffect, glm::vec3(0.0F, 0.0F, 0.0F), std::nullopt, 0.0F, 0, 1,
                               0, 0, instance_scale));
  REQUIRE(nodes.living_nodes().size() == 1);
  return nodes.living_nodes().front().node.sprite_scale;
}

const ro::render::EffectNodePattern& TorchPattern() {
  const std::span<const ro::render::EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(kTorchEffect);
  REQUIRE(patterns.size() == 1);
  return patterns.front();
}

}  // namespace

TEST_CASE("la torche d'une carte prend l'échelle de son objet du RSW", "[effect_torch]") {
  // 🔴 ELLE REMPLACE CELLE DU PATRON, elle ne la multiplie pas : le cas ÉCRIT le
  // paramètre dans le champ du nœud. À 0,6 comme sur les torches d'in_sphinx1,
  // une flamme composée à un serait deux tiers trop grande.
  CHECK(SownScale(kSphinxScale) == Catch::Approx(kSphinxScale));
}

TEST_CASE("une torche sans semeur garde l'échelle du patron", "[effect_torch]") {
  // Un paramètre nul ou négatif vaut « aucun » : le client pose alors un, et
  // c'est ce que le patron porte. C'est le cas de tout lancement qui ne vient
  // pas d'une carte.
  CHECK(SownScale(0.0F) == Catch::Approx(TorchPattern().sprite_scale));
  CHECK(TorchPattern().sprite_scale == Catch::Approx(1.0F));
}

TEST_CASE("le patron de la torche porte son sprite et sa cadence", "[effect_torch]") {
  const ro::render::EffectNodePattern& pattern = TorchPattern();
  REQUIRE(pattern.shape == ro::render::EffectNodeShape::kSprite);
  const std::span<const std::string_view> couple = ro::render::EffectNodePatternTextures(pattern);
  REQUIRE(couple.size() == 1);
  CHECK(couple.front() == kTorchSprite);
  // ⚠ UN, ET NON ZÉRO : zéro désignerait le défaut du constructeur, QUATRE. Le
  // client tire la cadence du second paramètre du RSW, borné à un tic, et ce
  // paramètre ne dépasse un sur aucune des 4 566 torches de l'installation.
  CHECK(pattern.sprite_frame_hold_ticks == 1);
  CHECK(pattern.draws_bulb == false);
}

TEST_CASE("la flamme garde la couleur de ses calques", "[effect_torch]") {
  // 🔴 SANS LE BIT 0x200, LE HALO SE PEINT À PLEINE OPACITÉ. Le cas l'allume
  // et il veut dire « garde la couleur du CALQUE » : le `.act`
  // donne à son halo l'alpha 35 sur 255, et la teinte du nœud — blanc opaque —
  // l'écraserait : un disque orange noierait la torche et le mur.
  CHECK((TorchPattern().sprite_flags & ro::render::kEffectNodeKeepsLayerColor) != 0);
}

TEST_CASE("la flamme couronne son applique au lieu d'en toucher le pied", "[effect_torch]") {
  // Le cas ne pose aucun mouvement, seulement une hauteur : dix unités au-dessus
  // du point de l'effet.
  CHECK(TorchPattern().launch_drop == Catch::Approx(-10.0F));
}

TEST_CASE("une seule figure de la table tire son échelle du RSW", "[effect_torch]") {
  // 🔴 CE N'EST PAS UNE LOI GÉNÉRALE : chaque cas lit ce qu'il veut des quatre
  // paramètres. Une seconde figure marquée voudrait dire qu'on l'a déduite au
  // lieu de la lire.
  const std::span<const ro::render::EffectNodePattern> all = ro::render::EffectNodePatternTable();
  const auto count = std::ranges::count_if(all, [](const ro::render::EffectNodePattern& pattern) {
    return pattern.sprite_scale_from_instance;
  });
  CHECK(count == 1);
  CHECK(std::ranges::all_of(all, [](const ro::render::EffectNodePattern& pattern) {
    return !pattern.sprite_scale_from_instance || pattern.effect_id == kTorchEffect;
  }));
}

TEST_CASE("la torche prend sa cadence dans le second paramètre, un tic au moins",
          "[effect_torch]") {
  // Le cas écrit le second paramètre tronqué dans la cadence d'image, ramené à
  // un s'il est plus petit : 3,7 donne trois tics par image, 0,6 en donne un.
  const auto cadence = [](float second) {
    const TempDirectory client;
    FillTorchClient(client);
    auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"torche.grf"}});
    REQUIRE(vfs.has_value());
    moonclient::EffectNodesInPlay nodes(*vfs);
    REQUIRE(nodes.PlayProcedural(kTorchEffect, glm::vec3(0.0F, 0.0F, 0.0F), std::nullopt, 0.0F, 0,
                                 1, 0, 0, kSphinxScale, 0, second));
    REQUIRE(nodes.living_nodes().size() == 1);
    return nodes.living_nodes().front().node.frame_hold_ticks;
  };
  CHECK(cadence(3.7F) == 3);
  CHECK(cadence(0.6F) == 1);
  CHECK(cadence(0.0F) == 1);
  CHECK(TorchPattern().sprite_frame_hold_from_instance);
}

TEST_CASE("l'eau d'une carte prend son cap dans le second paramètre de son objet",
          "[effect_torch]") {
  // 🔴 LES DEUX PARAMÈTRES VOYAGENT JUSQU'AU SEMIS : le premier donne le rayon,
  // le second le cap. Sans lui, l'eau de 838 naissait toujours au cap zéro.
  constexpr std::uint32_t kWaterEffect = 838;
  constexpr float kWaterRadius = 33.0F;
  constexpr float kWaterHeading = 135.0F;
  const TempDirectory client;
  const std::vector<TgaPixel> pixels(4, TgaPixel{.red = 40, .green = 90, .blue = 200});
  client.WriteFile(
      "eau.grf",
      BuildGrf({GrfFileSpec{"data\\texture\\effect\\W_c_00.tga", BuildTga(2, 2, pixels)}}));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"eau.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.PlayProcedural(kWaterEffect, glm::vec3(0.0F, 0.0F, 0.0F), std::nullopt, 0.0F, 0, 1,
                               0, 0, kWaterRadius, 0, kWaterHeading));
  REQUIRE(nodes.living_nodes().size() == 1);
  const ro::render::EffectTrack& leader = nodes.living_nodes().front().tracks[0];
  CHECK(leader.azimuth_degrees == static_cast<std::int16_t>(kWaterHeading));
}
