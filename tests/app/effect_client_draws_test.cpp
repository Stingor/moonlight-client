// LES TIRAGES ET LES COMPTES DU CLIENT, tels que la troupe les joue.
//
// Un rayon d'agilité naît sur un cercle ENTIER et sa variation de pas ne compte
// qu'une fois par tic ; les anneaux verts de Blitz Beat se comptent sur UN
// compteur que toutes les instances partagent ; les gerbes de l'effet 81 ne
// naissent jamais.

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_cross_spikes.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_screen_seed.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "support/client_vfs.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/temp_directory.h"

using Catch::Approx;
using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;
using ro::test_support::BuildGrf;
using ro::test_support::BuildTga;
using ro::test_support::Cp949;
using ro::test_support::GrfFileSpec;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::uint32_t kBlitzBeat = 115;
constexpr std::uint32_t kDecreaseAgility = 38;
constexpr std::uint32_t kYellowRing = 81;
constexpr std::int32_t kPixels = 4;
constexpr std::uint32_t kFirstFrameMs = 100;

// Un client qui porte une texture pour chaque figure du monde de ces effets.
void FillClientFor(const TempDirectory& client, std::initializer_list<std::uint32_t> effects) {
  const std::vector<TgaPixel> texture(static_cast<std::size_t>(kPixels * kPixels),
                                      TgaPixel{.red = 200, .green = 200, .blue = 60});
  const std::vector<BmpPixel> bmp_texture(static_cast<std::size_t>(kPixels * kPixels),
                                          BmpPixel{.red = 200, .green = 200, .blue = 60});
  std::vector<GrfFileSpec> fichiers;
  std::set<std::string> seen;
  for (const std::uint32_t effect_id : effects) {
    for (const ro::render::EffectNodePattern& pattern :
         ro::render::EffectNodePatternsFor(effect_id)) {
      for (const std::string_view name : ro::render::EffectNodePatternTextures(pattern)) {
        if (pattern.shape != ro::render::EffectNodeShape::kSprite &&
            seen.insert(std::string(name)).second) {
          // Le décodeur se choisit par l'extension : la pierre est un `.bmp`.
          fichiers.push_back(GrfFileSpec{Cp949(name), name.ends_with(".bmp")
                                                          ? BuildBmp(kPixels, kPixels, bmp_texture)
                                                          : BuildTga(kPixels, kPixels, texture)});
        }
      }
    }
  }
  client.WriteFile("effets.grf", BuildGrf(fichiers));
}

std::uint32_t PassTicks(moonclient::EffectNodesInPlay& nodes, std::uint32_t from_ms,
                        std::int32_t ticks) {
  const ro::render::Camera camera;
  const std::uint32_t jusqu_a_ms =
      from_ms + static_cast<std::uint32_t>(ticks * ro::render::kEffectTickMs);
  static_cast<void>(nodes.Drawn(jusqu_a_ms, camera));
  return jusqu_a_ms;
}

std::size_t CountShape(const moonclient::EffectNodesInPlay& nodes,
                       ro::render::PooledNodeShape forme) {
  std::size_t count = 0;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    count += node.shape == forme ? 1U : 0U;
  }
  return count;
}

}  // namespace

TEST_CASE("un rayon d'EF_DECAGILITY gagne sa variation de pas UNE fois par tic",
          "[effect_client_draws]") {
  const TempDirectory client;
  FillClientFor(client, {kDecreaseAgility});
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.PlayProcedural(kDecreaseAgility, glm::vec3(0.0F, 0.0F, 0.0F)));
  const ro::render::PooledEffectNode* radius = nullptr;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    if (node.shape == ro::render::PooledNodeShape::kCrossedPlanes) {
      radius = &node;
    }
  }
  REQUIRE(radius != nullptr);
  // 🔴 LE CERCLE EST ENTIER : le rayon naît à deux, trois… huit unités du
  // centre, jamais entre deux.
  const float au_sol = std::hypot(radius->ground_position.x, radius->ground_position.z);
  CHECK(std::abs(au_sol - std::round(au_sol)) < 1e-3F);
  CHECK(au_sol >= 2.0F - 1e-3F);
  CHECK(au_sol <= 8.0F + 1e-3F);

  // Dix tics : le pas vaut dix fois 0,015, et non vingt.
  constexpr std::int32_t kTicks = 10;
  constexpr float kStepChange = 0.015F;
  static_cast<void>(PassTicks(nodes, PassTicks(nodes, kFirstFrameMs, 0), kTicks));
  bool vu = false;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    if (node.shape == ro::render::PooledNodeShape::kCrossedPlanes &&
        node.node.age_ticks == kTicks) {
      CHECK(node.node.step_length == Approx(kStepChange * kTicks));
      vu = true;
    }
  }
  CHECK(vu);
}

TEST_CASE("les anneaux verts de Blitz Beat se comptent sur un compteur partagé",
          "[effect_client_draws]") {
  // Deux coups : deux anneaux, aux tics 36 et 39. Une seconde instance née au
  // tic 37 remet le compteur à zéro : la première en sème alors deux de plus.
  const TempDirectory client;
  FillClientFor(client, {kBlitzBeat});
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  constexpr std::int32_t kHits = 2;
  const auto rings_after = [&](bool second) {
    moonclient::EffectNodesInPlay nodes(*vfs);
    REQUIRE(nodes.PlayProcedural(kBlitzBeat, glm::vec3(0.0F, 0.0F, 0.0F), std::nullopt, 0.0F, 0,
                                 kHits));
    std::uint32_t maintenant = PassTicks(nodes, kFirstFrameMs, 0);
    maintenant = PassTicks(nodes, maintenant, 37);
    if (second) {
      REQUIRE(nodes.PlayProcedural(kBlitzBeat, glm::vec3(100.0F, 0.0F, 0.0F), std::nullopt, 0.0F, 0,
                                   kHits));
    }
    // ⚠ SEPT PASSAGES : l'anneau du tic k naît au passage k + 1 compté depuis
    // la naissance, le bâtisseur semant à l'âge qu'il a en début de passage.
    static_cast<void>(PassTicks(nodes, maintenant, 7));
    // ⚠ PAR TIC, ET NON PAR IMAGE : une image qui en rattrape plusieurs les
    // passe un à un, semis puis mise à jour.
    std::set<std::int32_t> sown_at_tick;
    for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
      if (node.shape == ro::render::PooledNodeShape::kRingBand) {
        sown_at_tick.insert(node.sown_at_effect_tick);
      }
    }
    return sown_at_tick;
  };
  // Seule : le 36 et le 39 vivent, et le 42 ne naît pas — le compte a atteint
  // les deux coups. ⚠ Le 36 vit encore parce que les tics passent un à un :
  // semer les trente-sept d'un coup puis tout avancer lui en donnait trente-sept.
  CHECK(rings_after(false) == std::set<std::int32_t>{36, 39});
  // Compteur remis à zéro au tic 37 : la première sème encore au 42. La
  // seconde n'a pas encore l'âge de semer.
  CHECK(rings_after(true) == std::set<std::int32_t>{36, 39, 42});
}

TEST_CASE("les plans de Blitz Beat suivent leur porteur pas à pas", "[effect_client_draws]") {
  // Le mouvement des sous-types quinze et seize relit l'instance à chaque tic,
  // et le bâtisseur y recopie le porteur : les plans marchent avec lui.
  const TempDirectory client;
  FillClientFor(client, {kBlitzBeat});
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  constexpr std::uint32_t kBearer = 7000;
  constexpr float kStep = 30.0F;
  const glm::vec3 birth{0.0F, 0.0F, 0.0F};
  REQUIRE(nodes.PlayProcedural(kBlitzBeat, birth, std::nullopt, 0.0F, kBearer, 1));
  std::vector<float> before;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    before.push_back(node.ground_position.x);
  }
  REQUIRE(!before.empty());
  nodes.FollowActor(kBearer, birth + glm::vec3{kStep, 0.0F, 0.0F}, 0.0F);
  std::size_t rang = 0;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    CHECK(node.ground_position.x == Approx(before[rang] + kStep));
    ++rang;
  }
}

TEST_CASE("le réticule de visée flotte deux unités au-dessus du sol", "[effect_client_draws]") {
  constexpr std::uint32_t kLockOn = 60;
  std::size_t carres = 0;
  for (const ro::render::EffectNodePattern& pattern : ro::render::EffectNodePatternsFor(kLockOn)) {
    CHECK(pattern.ring.burial_depth == Approx(-2.0F));
    ++carres;
  }
  CHECK(carres == 2);

  // ET RIEN NE S'Y AJOUTE : la levée contre le sol ne vaut que pour un nœud posé
  // dans son plan, et le réticule en est déjà sorti de deux unités.
  const TempDirectory client;
  FillClientFor(client, {kLockOn});
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.PlayProcedural(kLockOn, glm::vec3(0.0F, 0.0F, 0.0F)));
  std::size_t seen = 0;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    CHECK(node.ground_position.y == Approx(2.0F));
    ++seen;
  }
  CHECK(seen > 0);
}

TEST_CASE("les buffs génériques 2288 à 2300 ont deux branches", "[effect_client_draws]") {
  // Pairs : le rayon MONTE, cyan ; impairs : il TOMBE, rouge. Les deux naissent
  // un tic sur deux sur le cercle entier.
  constexpr std::uint32_t kFirst = 2288;
  constexpr std::uint32_t kLast = 2300;
  for (std::uint32_t effect_id = kFirst; effect_id <= kLast; ++effect_id) {
    CAPTURE(effect_id);
    const auto patterns = ro::render::EffectNodePatternsFor(effect_id);
    REQUIRE(patterns.size() == 1);
    const ro::render::EffectNodePattern& radius = patterns.front();
    CHECK(radius.period_ticks == 2);
    CHECK(radius.last_tick == 99);
    CHECK(radius.ring.birth_circle_is_integer);
    const bool rise = effect_id % 2 == 0;
    CHECK(radius.launch_pitch_low == Approx(rise ? 90.0F : -90.0F));
    CHECK(radius.color[0] == (rise ? 0 : 255));
    CHECK(radius.color[1] == (rise ? 255 : 0));
    CHECK(radius.ring.step_length_change == Approx(rise ? 0.0F : 0.015F));
    // ET SON ICÔNE, qui monte ou descend, à la taille de son image.
    const auto icones = ro::render::EffectScreenPatternsFor(effect_id);
    REQUIRE(icones.size() == 1);
    CHECK(icones.front().half_size_per_texture_pixel == Approx(0.625F));
    CHECK(icones.front().heading_degrees.low == Approx(rise ? 0.0F : 180.0F));
  }
}

TEST_CASE("les gerbes de l'effet 81 ne naissent jamais", "[effect_client_draws]") {
  for (const ro::render::EffectNodePattern& pattern :
       ro::render::EffectNodePatternsFor(kYellowRing)) {
    CHECK(pattern.shape != ro::render::EffectNodeShape::kSprite);
  }
}

TEST_CASE("les pointes de Grimtooth passent l'épreuve de profondeur contre le sol",
          "[effect_client_draws]") {
  // Le bâtisseur lève 0x200 sur le mot de ses trois pointes, qui naissent à
  // quarante unités sous le sol. Mais la croix de pointes soumet ses triangles
  // sous un mot FIXE : la file les range dans le seau trié, épreuve de
  // profondeur comprise, et le sol cache leur partie enfouie. Sous le mot du
  // nœud, elles passaient par-dessus tout et se voyaient à travers le terrain.
  constexpr std::uint32_t kGrimtoothAttack = 132;
  const auto patterns = ro::render::EffectNodePatternsFor(kGrimtoothAttack);
  REQUIRE(patterns.size() == 1);
  REQUIRE_FALSE(ro::render::EffectLotIsDepthTested(patterns.front().render_word));

  const TempDirectory client;
  FillClientFor(client, {kGrimtoothAttack});
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.PlayProcedural(kGrimtoothAttack, glm::vec3(0.0F, 0.0F, 0.0F)));
  // Un passage entier : avant lui, rien de ce qui naît ne se dessine.
  static_cast<void>(PassTicks(nodes, PassTicks(nodes, kFirstFrameMs, 0), 1));
  CHECK(CountShape(nodes, ro::render::PooledNodeShape::kCrossSpikes) == 3);
  std::size_t lots = 0;
  for (const ro::render::DrawnWorldQuads& lot : nodes.WorldQuads()) {
    CHECK(ro::render::EffectLotIsDepthTested(lot.render_word));
    CHECK(ro::render::EffectLotBucketRank(lot.render_word) ==
          ro::render::EffectBucketRank::kSorted);
    ++lots;
  }
  CHECK(lots == 3);
}

TEST_CASE("les dessinateurs à mot fixe imposent le leur au nœud", "[effect_client_draws]") {
  // Le tronc de cône, les plans croisés, la croix de pointes et la sphère
  // soumettent un mot en dur ; la couronne plate et le quad orienté soumettent
  // celui du nœud.
  using ro::render::EffectNodeQueuedRenderWord;
  using ro::render::PooledNodeShape;
  constexpr std::uint32_t kOverEverything = 0x201;
  constexpr std::uint8_t kPlainMark = 0;
  for (const PooledNodeShape shape : {PooledNodeShape::kRingBand, PooledNodeShape::kCrossedPlanes,
                                      PooledNodeShape::kCrossSpikes, PooledNodeShape::kUvSphere}) {
    CHECK(EffectNodeQueuedRenderWord(shape, kOverEverything, kPlainMark) ==
          ro::render::kEffectRenderWordOfConstructor);
  }
  for (const PooledNodeShape shape :
       {PooledNodeShape::kRingCylinder, PooledNodeShape::kOrientedQuad}) {
    CHECK(EffectNodeQueuedRenderWord(shape, kOverEverything, kPlainMark) == kOverEverything);
  }
  // La croix À PART passe au mélange propre, dans le seau des fichiers.
  const std::uint32_t own_color = EffectNodeQueuedRenderWord(
      PooledNodeShape::kCrossSpikes, kOverEverything, ro::render::kCrossSpikeDrawnColor);
  CHECK(own_color ==
        (ro::render::kEffectRenderWordTranslucent | ro::render::kEffectRenderWordOwnBlend));
  CHECK(ro::render::EffectLotIsDepthTested(own_color));
  // Et le mot ne vient que de là : la marque ne touche pas les autres formes.
  CHECK(EffectNodeQueuedRenderWord(PooledNodeShape::kRingBand, kOverEverything,
                                   ro::render::kCrossSpikeDrawnColor) ==
        ro::render::kEffectRenderWordOfConstructor);
}
