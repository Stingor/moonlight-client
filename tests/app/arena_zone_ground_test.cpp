// L'EFFET AU SOL DES ZONES DE L'ARÈNE : un pilier de Fire Pillar montre, tant
// qu'il dure, le cercle rouge que le jeu entretient sous le pilier en attente,
// et ce cercle s'éteint avec la zone. Le gardien l'allume une fois, ne le
// resème que mort, et l'éteint — effet ET acteur — quand la zone n'est plus.

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_catalog.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_track_seed.h"
#include "ro/sim/actor.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/temp_directory.h"
#include "survivors/arena_effect_plan.h"
#include "survivors/arena_zone_grounds.h"
#include "survivors/survivors_looks.h"

namespace {

// Le cercle rouge du pilier en attente, celui que la table des unités
// entretient sous l'unité 135 : distinct des flammes de l'éclatement.
constexpr std::uint32_t kWaitingPillarCircleEffect = 138;
// Les flammes du pilier : un effet que « Effets réduits » coupe.
constexpr std::uint32_t kPillarFlameEffect = 96;
constexpr std::uint16_t kFireWallSkill = 18;
constexpr std::size_t kPillars = 8;
constexpr std::uint32_t kFirstZoneSerial = 40;
constexpr std::uint32_t kPlacedMs = 10'000;
constexpr std::uint32_t kFrameMs = 16;

[[nodiscard]] std::vector<ro::sim::SurvivorsZone> PillarRing() {
  std::vector<ro::sim::SurvivorsZone> zones;
  for (std::size_t pillar = 0; pillar < kPillars; ++pillar) {
    const ro::sim::FieldPoint at{static_cast<std::int32_t>(pillar) * ro::sim::kSubunitsPerCell,
                                 ro::sim::kSubunitsPerCell};
    zones.push_back(
        ro::sim::SurvivorsZone{.serial = kFirstZoneSerial + static_cast<std::uint32_t>(pillar),
                               .skill_id = moonclient::kFirePillarSkill,
                               .archetype = ro::sim::SkillArchetype::kRing,
                               .from = at,
                               .to = at});
  }
  return zones;
}

[[nodiscard]] moonclient::ArenaZoneGround Circle(std::uint32_t serial) {
  return moonclient::ArenaZoneGround{.actor_id = moonclient::kFirstZoneGroundActorId + serial,
                                     .effect_id = kWaitingPillarCircleEffect};
}

// UN CLIENT QUI NE PORTE QUE LES TEXTURES DU CERCLE : il est procédural, et
// un nœud dont la texture ne se lit pas ne se sème pas.
[[nodiscard]] std::vector<ro::test_support::GrfFileSpec> CircleTextures() {
  constexpr std::int32_t kPixels = 4;
  const std::size_t pixel_count = static_cast<std::size_t>(kPixels) * kPixels;
  std::vector<ro::test_support::GrfFileSpec> files;
  const auto add = [&files, pixel_count](std::string_view path) {
    if (path.empty() ||
        std::ranges::any_of(files, [path](const auto& file) { return file.name == path; })) {
      return;
    }
    std::string lowered(path);
    std::ranges::transform(lowered, lowered.begin(), [](unsigned char letter) {
      return static_cast<char>(std::tolower(letter));
    });
    std::vector<std::uint8_t> content =
        lowered.ends_with(".tga")
            ? ro::test_support::BuildTga(kPixels, kPixels,
                                         std::vector<ro::test_support::TgaPixel>(pixel_count))
            : ro::test_support::BuildBmp(kPixels, kPixels,
                                         std::vector<ro::test_support::BmpPixel>(pixel_count));
    files.push_back(
        ro::test_support::GrfFileSpec{.name = std::string(path), .content = std::move(content)});
  };
  for (const ro::render::EffectNodePattern& pattern :
       ro::render::EffectNodePatternsFor(kWaitingPillarCircleEffect)) {
    for (const std::string_view path : ro::render::EffectNodePatternTextures(pattern)) add(path);
  }
  for (const ro::render::EffectRingNodePattern& ring :
       ro::render::EffectRingNodePatternsFor(kWaitingPillarCircleEffect)) {
    add(ring.texture);
  }
  return files;
}

// Des mains qui notent ce que le gardien leur demande.
struct Ledger {
  struct Sown {
    std::uint32_t actor_id = 0;
    bool first = false;
  };
  bool carries = true;
  bool sow_takes = true;
  std::vector<Sown> sown;
  std::vector<std::uint32_t> put_out;

  [[nodiscard]] moonclient::ArenaZoneGroundHands Hands() {
    return moonclient::ArenaZoneGroundHands{
        .carries = [this](const moonclient::ArenaZoneGround&) { return carries; },
        .sow =
            [this](const moonclient::ArenaZoneGround& ground, bool first) {
              sown.push_back(Sown{.actor_id = ground.actor_id, .first = first});
              return sow_takes;
            },
        .put_out =
            [this](const moonclient::ArenaZoneGround& ground) {
              put_out.push_back(ground.actor_id);
            },
    };
  }
};

}  // namespace

TEST_CASE("Effet au sol des zones : Fire Pillar pose le cercle du pilier en attente",
          "[app][survivors]") {
  const ro::sim::SurvivorsSkillRule* pillar =
      ro::sim::FindSkill(ro::sim::ShippedSurvivorsRules(), moonclient::kFirePillarSkill);
  REQUIRE(pillar != nullptr);
  const moonclient::SkillLook look = moonclient::SkillLookOf(*pillar);
  CHECK(look.ground_effect_id == kWaitingPillarCircleEffect);
  // Le cercle n'est pas l'effet des flammes : les deux se montrent.
  CHECK(look.ground_effect_id != look.effect_id);
  CHECK(moonclient::EffectNodesInPlay::KnowsProcedural(look.ground_effect_id));

  // Un mur de feu n'a pas d'unité écrite : ses flammes seules le montrent.
  const ro::sim::SurvivorsSkillRule* wall =
      ro::sim::FindSkill(ro::sim::ShippedSurvivorsRules(), kFireWallSkill);
  REQUIRE(wall != nullptr);
  CHECK(moonclient::SkillLookOf(*wall).ground_effect_id == 0);
}

TEST_CASE("Effet au sol des zones : un cercle par pilier posé, éteint avec sa zone",
          "[app][survivors]") {
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  std::vector<ro::sim::SurvivorsZone> zones = PillarRing();
  std::vector<moonclient::ArenaZoneGround> grounds;

  moonclient::PlanArenaZoneGrounds(zones, looks, {}, grounds);
  REQUIRE(grounds.size() == kPillars);
  for (std::size_t pillar = 0; pillar < kPillars; ++pillar) {
    INFO("pilier " << pillar);
    CHECK(grounds[pillar].effect_id == kWaitingPillarCircleEffect);
    CHECK(grounds[pillar].at == zones[pillar].from);
    CHECK(grounds[pillar].actor_id == moonclient::kFirstZoneGroundActorId + zones[pillar].serial);
  }

  // LE PILIER QUI S'ÉTEINT — expiré ou au bout de ses coups, la simulation le
  // retire de l'état — sort du plan ; les autres restent.
  const std::uint32_t gone_actor = grounds.front().actor_id;
  zones.erase(zones.begin());
  moonclient::PlanArenaZoneGrounds(zones, looks, {}, grounds);
  CHECK(grounds.size() == kPillars - 1);
  for (const moonclient::ArenaZoneGround& ground : grounds) CHECK(ground.actor_id != gone_actor);

  zones.clear();
  moonclient::PlanArenaZoneGrounds(zones, looks, {}, grounds);
  CHECK(grounds.empty());
}

TEST_CASE("Effet au sol des zones : le cercle reste sous « Effets réduits » et sans effets",
          "[app][survivors]") {
  // Le jeu ne coupe jamais ce cercle : il marque où le pilier frappera.
  const moonclient::ArenaSkillLooks looks(ro::sim::ShippedSurvivorsRules());
  const std::vector<ro::sim::SurvivorsZone> zones = PillarRing();
  std::vector<moonclient::ArenaZoneGround> grounds;

  moonclient::PlanArenaZoneGrounds(zones, looks, {.reduced = true}, grounds);
  CHECK(grounds.size() == kPillars);
  moonclient::PlanArenaZoneGrounds(zones, looks, {.shown = false}, grounds);
  CHECK(grounds.size() == kPillars);

  // Un effet au sol ordinaire, lui, suit les réglages.
  CHECK(moonclient::ArenaZoneGroundShown(kPillarFlameEffect, {}));
  CHECK_FALSE(moonclient::ArenaZoneGroundShown(kPillarFlameEffect, {.reduced = true}));
  CHECK_FALSE(moonclient::ArenaZoneGroundShown(kPillarFlameEffect, {.shown = false}));
}

TEST_CASE("Effet au sol des zones : le gardien allume, ne relance pas, resème mort, éteint",
          "[app][survivors]") {
  moonclient::ArenaZoneGroundKeeper keeper;
  Ledger ledger;
  const std::vector<moonclient::ArenaZoneGround> two = {Circle(1), Circle(2)};

  // UNE ZONE NEUVE S'ALLUME, une fois, en s'annonçant.
  keeper.Keep(two, kPlacedMs, ledger.Hands());
  REQUIRE(ledger.sown.size() == 2);
  CHECK(ledger.sown[0].actor_id == two[0].actor_id);
  CHECK(ledger.sown[0].first);
  CHECK(keeper.size() == 2);
  CHECK(keeper.sown_total() == 2);

  // RIEN NE CHANGE, LE VISUEL VIT : rien ne se relance.
  keeper.Keep(two, kPlacedMs + kFrameMs, ledger.Hands());
  CHECK(ledger.sown.size() == 2);
  CHECK(ledger.put_out.empty());

  // MORT DE SA PROPRE VIE, il se resème, sans s'annoncer.
  ledger.carries = false;
  keeper.Keep(two, kPlacedMs + 2 * kFrameMs, ledger.Hands());
  REQUIRE(ledger.sown.size() == 4);
  CHECK_FALSE(ledger.sown[2].first);
  ledger.carries = true;

  // LA ZONE SORTIE DU PLAN éteint son effet et son acteur, et elle seule.
  const std::vector<moonclient::ArenaZoneGround> one = {two[1]};
  keeper.Keep(one, kPlacedMs + 3 * kFrameMs, ledger.Hands());
  REQUIRE(ledger.put_out.size() == 1);
  CHECK(ledger.put_out.front() == two[0].actor_id);
  CHECK(keeper.size() == 1);

  // « Rejouer » éteint tout.
  keeper.Forget(ledger.Hands());
  CHECK(ledger.put_out.size() == 2);
  CHECK(keeper.size() == 0);
}

TEST_CASE("Effet au sol des zones : la bascule n'éteint que ce que le jeu coupe",
          "[app][survivors]") {
  moonclient::ArenaZoneGroundKeeper keeper;
  Ledger ledger;
  moonclient::ArenaZoneGround flame = Circle(2);
  flame.effect_id = kPillarFlameEffect;
  const std::vector<moonclient::ArenaZoneGround> grounds = {Circle(1), flame};
  keeper.Keep(grounds, kPlacedMs, ledger.Hands());

  keeper.PutOutHidden({.reduced = true}, ledger.Hands());
  REQUIRE(ledger.put_out.size() == 1);
  CHECK(ledger.put_out.front() == flame.actor_id);
  CHECK(keeper.size() == 1);
}

TEST_CASE("Effet au sol des zones : un semis refusé attend avant de se retenter",
          "[app][survivors]") {
  moonclient::ArenaZoneGroundKeeper keeper;
  Ledger ledger;
  ledger.sow_takes = false;
  const std::vector<moonclient::ArenaZoneGround> one = {Circle(1)};

  keeper.Keep(one, kPlacedMs, ledger.Hands());
  keeper.Keep(one, kPlacedMs + kFrameMs, ledger.Hands());
  // Une seule tentative dans le délai, et rien ne se compte semé.
  CHECK(ledger.sown.size() == 1);
  CHECK(keeper.sown_total() == 0);

  ledger.sow_takes = true;
  keeper.Keep(one, kPlacedMs + moonclient::kZoneGroundRetryMs, ledger.Hands());
  REQUIRE(ledger.sown.size() == 2);
  // Le premier semis qui prend est celui qui s'annonce.
  CHECK(ledger.sown.back().first);
  CHECK(keeper.sown_total() == 1);
}

TEST_CASE("Effet au sol des zones : sur le vrai moteur, le cercle naît et meurt avec son acteur",
          "[app][survivors]") {
  ro::test_support::TempDirectory client;
  client.WriteFile("cercle.grf", ro::test_support::BuildGrf(CircleTextures()));
  auto mounted = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"cercle.grf"}});
  REQUIRE(mounted.has_value());
  ro::vfs::Vfs vfs = std::move(*mounted);
  ro::render::EffectCatalog catalog(vfs);
  moonclient::EffectNodesInPlay nodes(vfs);
  moonclient::EffectsInPlay effects(catalog, nodes, vfs);
  ro::sim::World world;

  const moonclient::ArenaZoneGround circle = Circle(1);
  const auto hands = [&effects, &world](std::uint32_t now_ms) {
    return moonclient::ArenaZoneGroundHandsOn(
        effects, world,
        [&world](const moonclient::ArenaZoneGround& ground) {
          ro::sim::Actor bearer;
          bearer.id = ground.actor_id;
          bearer.kind = ro::sim::ActorKind::kPlayer;
          world.Spawn(bearer);
        },
        [](ro::sim::FieldPoint) { return glm::vec3(0.0F); }, now_ms);
  };

  moonclient::ArenaZoneGroundKeeper keeper;
  const std::vector<moonclient::ArenaZoneGround> one = {circle};
  keeper.Keep(one, kPlacedMs, hands(kPlacedMs));
  CHECK(keeper.sown_total() == 1);
  CHECK(effects.Carries(circle.effect_id, circle.actor_id));
  CHECK(world.Find(circle.actor_id) != nullptr);

  // Vivant, il ne se resème pas.
  keeper.Keep(one, kPlacedMs + kFrameMs, hands(kPlacedMs + kFrameMs));
  CHECK(keeper.sown_total() == 1);

  // Le pilier éclaté emporte son cercle et son acteur.
  keeper.Keep({}, kPlacedMs + 2 * kFrameMs, hands(kPlacedMs + 2 * kFrameMs));
  CHECK_FALSE(effects.Carries(circle.effect_id, circle.actor_id));
  CHECK(world.Find(circle.actor_id) == nullptr);
}
