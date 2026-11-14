// QUAND UN EFFET REPREND LA PLACE DE SON PORTEUR : la table engendrée, son
// horaire, et ce que la troupe en fait.

#include "ro/render/effect_bearer_follow.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pool.h"

#include "support/effect_bearers.h"

using ro::render::EffectBearerFollow;
using ro::render::EffectBearerFollowFor;
using ro::render::EffectBearerFollowOf;
using ro::render::EffectFollowsBearerStepByStep;
using ro::render::EffectNodePool;
using ro::render::EffectRejoinsBearerAt;
using ro::render::EffectRejoinsBearerBetween;
using ro::render::kEffectFollowsToTheEnd;
using ro::render::kEffectTickMs;
using ro::render::PooledEffectNode;

namespace {

// L'EAU BÉNITE, LA FUMÉE, LA TORCHE : leur cas recopie la place du porteur à
// chaque tic. La torche est validée en jeu.
constexpr std::uint32_t kAquaEffect = 39;
constexpr std::uint32_t kSmokeEffect = 44;
constexpr std::uint32_t kTorchEffect = 47;
// SIGHT BLASTER et le mur de SAFETY WALL, validés en jeu eux aussi.
constexpr std::uint32_t kSightBlasterEffect = 601;
constexpr std::uint32_t kSafetyWallEffect = 315;
// LA BOUFFÉE DE POISON : son cas ne recopie jamais la place de son porteur.
constexpr std::uint32_t kPoisonHitEffect = 53;
// UN IMPACT : le coup s'affiche où il a porté, même si la cible s'en va.
constexpr std::uint32_t kBashEffect = 16;
// LE PORTAIL QUI S'OUVRE se recale sur son porteur tous les quatorze tics.
constexpr std::uint32_t kReadyPortalEffect = 316;
constexpr std::int32_t kReadyPortalPeriodTicks = 14;
// KAIZEL ne le rejoint qu'une fois, au quinzième tic.
constexpr std::uint32_t kKaizelEffect = 590;
constexpr std::int32_t kKaizelRejoinTick = 15;
// UN IDENTIFIANT SANS CAS : sa mise à jour ne touche jamais la place.
constexpr std::uint32_t kNoCaseEffect = 19;
// Assez de tics pour voir un horaire se répéter, et au-delà de tout recalage
// unique de la table.
constexpr std::int32_t kTicksLooked = 400;

constexpr std::uint32_t kBearer = 4000;
const glm::vec3 kBirthPlace{10.0F, 0.0F, 20.0F};

// Un nœud qui vit assez longtemps pour qu'aucun essai ne le voie mourir.
PooledEffectNode LongLivedNode() {
  PooledEffectNode node;
  node.node.life_ticks = 9999;
  node.ground_position = kBirthPlace;
  return node;
}

// Sème UN nœud de cet effet, accroché au porteur à sa place de naissance.
void SowOneNodeOf(EffectNodePool& pool, std::uint32_t effect_id) {
  const EffectNodePool::AnchoredSowing anchor{pool, kBearer, kBirthPlace, effect_id};
  pool.Add(LongLivedNode());
}

void AdvanceTicks(EffectNodePool& pool, std::int32_t ticks) {
  pool.Advance(static_cast<std::uint32_t>(ticks * kEffectTickMs));
}

}  // namespace

TEST_CASE("les effets validés en jeu suivent leur porteur à chaque tic", "[effect_bearer_follow]") {
  for (const std::uint32_t each_effect_id :
       {kAquaEffect, kSmokeEffect, kTorchEffect, kSightBlasterEffect, kSafetyWallEffect}) {
    INFO("effet " << each_effect_id);
    const EffectBearerFollow* follow = EffectBearerFollowFor(each_effect_id);
    REQUIRE(follow != nullptr);
    CHECK(follow->first_tick == 0);
    CHECK(follow->period_ticks == 1);
    CHECK(follow->last_tick == kEffectFollowsToTheEnd);
  }
}

TEST_CASE("la bouffée de poison et un impact restent où ils sont nés", "[effect_bearer_follow]") {
  // 🔴 C'EST L'ÉCART QUI A MOTIVÉ LA TABLE. Le cas de la bouffée de poison ne
  // recopie jamais la place de son porteur : elle reste où elle est née, même
  // quand la cible s'éloigne.
  for (const std::uint32_t each_effect_id : {kPoisonHitEffect, kBashEffect, kNoCaseEffect}) {
    INFO("effet " << each_effect_id);
    CHECK(EffectBearerFollowFor(each_effect_id) == nullptr);
    const EffectBearerFollow follow = EffectBearerFollowOf(each_effect_id);
    for (std::int32_t tick = 0; tick < kTicksLooked; ++tick) {
      CHECK_FALSE(EffectRejoinsBearerAt(follow, tick));
    }
  }
}

TEST_CASE("le portail qui s'ouvre se recale tous les quatorze tics", "[effect_bearer_follow]") {
  const EffectBearerFollow* follow = EffectBearerFollowFor(kReadyPortalEffect);
  REQUIRE(follow != nullptr);
  CHECK(follow->period_ticks == kReadyPortalPeriodTicks);
  for (std::int32_t tick = 0; tick < kTicksLooked; ++tick) {
    INFO("tic " << tick);
    CHECK(EffectRejoinsBearerAt(*follow, tick) == (tick % kReadyPortalPeriodTicks == 0));
  }
  CHECK_FALSE(EffectFollowsBearerStepByStep(*follow, kReadyPortalPeriodTicks));
}

TEST_CASE("Kaizel ne rejoint son porteur qu'au quinzième tic", "[effect_bearer_follow]") {
  const EffectBearerFollow* follow = EffectBearerFollowFor(kKaizelEffect);
  REQUIRE(follow != nullptr);
  for (std::int32_t tick = 0; tick < kTicksLooked; ++tick) {
    INFO("tic " << tick);
    CHECK(EffectRejoinsBearerAt(*follow, tick) == (tick == kKaizelRejoinTick));
  }
}

TEST_CASE("un recalage se compte une fois entre deux images", "[effect_bearer_follow]") {
  // Une image peut voir passer plusieurs tics, ou aucun : la question est
  // « un tic de l'horaire est-il tombé depuis la dernière fois ? ».
  const EffectBearerFollow kaizel = EffectBearerFollowOf(kKaizelEffect);
  CHECK_FALSE(EffectRejoinsBearerBetween(kaizel, 0, kKaizelRejoinTick - 1));
  CHECK(EffectRejoinsBearerBetween(kaizel, kKaizelRejoinTick - 1, kKaizelRejoinTick));
  CHECK(EffectRejoinsBearerBetween(kaizel, 3, kKaizelRejoinTick + 20));
  CHECK_FALSE(EffectRejoinsBearerBetween(kaizel, kKaizelRejoinTick, kTicksLooked));
  // Une image sans tic ne recale rien, même sur un tic de l'horaire.
  CHECK_FALSE(EffectRejoinsBearerBetween(kaizel, kKaizelRejoinTick, kKaizelRejoinTick));

  const EffectBearerFollow portal = EffectBearerFollowOf(kReadyPortalEffect);
  CHECK_FALSE(EffectRejoinsBearerBetween(portal, 1, kReadyPortalPeriodTicks - 1));
  CHECK(EffectRejoinsBearerBetween(portal, 1, kReadyPortalPeriodTicks));
  CHECK(EffectRejoinsBearerBetween(portal, kReadyPortalPeriodTicks * 7 + 3,
                                   kReadyPortalPeriodTicks * 8));
}

TEST_CASE("Troupe : un effet qui reste ne suit pas son porteur, mais lui appartient toujours",
          "[effect_bearer_follow]") {
  // 🔴 SUIVRE N'EST PAS APPARTENIR. La bouffée de poison reste où elle est née,
  // mais elle disparaît avec l'acteur qui l'a reçue : l'ancre doit tenir.
  EffectNodePool pool;
  SowOneNodeOf(pool, kPoisonHitEffect);
  REQUIRE(ro::test_support::IsAnchoredTo(pool, kBearer));
  AdvanceTicks(pool, 5);
  ro::test_support::MoveBearer(pool, kBearer, kBirthPlace + glm::vec3{40.0F, 0.0F, 0.0F});
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(kBirthPlace.x));
  CHECK(pool.nodes()[0].owner_position.x == Catch::Approx(0.0F));

  pool.DropAnchoredUnless([](std::uint32_t) { return false; });
  CHECK(pool.size() == 0);
}

TEST_CASE("Troupe : un effet qui suit à chaque tic suit à chaque image", "[effect_bearer_follow]") {
  // Le comportement de toute ancre avant la table : la torche en est un
  // témoin, validé en jeu.
  EffectNodePool pool;
  SowOneNodeOf(pool, kTorchEffect);
  ro::test_support::MoveBearer(pool, kBearer, kBirthPlace + glm::vec3{40.0F, 0.0F, 0.0F});
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(kBirthPlace.x + 40.0F));
  // SANS ATTENDRE LE TIC : une image qui n'en voit passer aucun suit quand même.
  ro::test_support::MoveBearer(pool, kBearer, kBirthPlace + glm::vec3{45.0F, 0.0F, 0.0F});
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(kBirthPlace.x + 45.0F));
}

TEST_CASE("Troupe : un recalage à un tic précis rattrape tout le chemin d'un coup",
          "[effect_bearer_follow]") {
  // 🔴 ENTRE DEUX RECALAGES, NI LE NŒUD NI LA PLACE DE L'ANCRE NE BOUGENT. Le
  // porteur de Kaizel peut avoir marché pendant quinze tics : au quinzième,
  // l'effet saute sur lui d'un seul coup, et tout ce qu'il a déjà semé avec.
  EffectNodePool pool;
  SowOneNodeOf(pool, kKaizelEffect);
  AdvanceTicks(pool, kKaizelRejoinTick - 1);
  ro::test_support::MoveBearer(pool, kBearer, kBirthPlace + glm::vec3{10.0F, 0.0F, 0.0F});
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(kBirthPlace.x));

  AdvanceTicks(pool, 1);
  ro::test_support::MoveBearer(pool, kBearer, kBirthPlace + glm::vec3{25.0F, 0.0F, 0.0F});
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(kBirthPlace.x + 25.0F));

  // ⚠ ET PLUS JAMAIS ENSUITE.
  AdvanceTicks(pool, 30);
  ro::test_support::MoveBearer(pool, kBearer, kBirthPlace + glm::vec3{60.0F, 0.0F, 0.0F});
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(kBirthPlace.x + 25.0F));
}

TEST_CASE("Troupe : un nœud semé plus tard compte l'âge de son EFFET", "[effect_bearer_follow]") {
  // Un nœud de Kaizel né au dixième tic de l'effet n'a que cinq tics quand
  // l'effet atteint le quinzième : c'est l'âge de l'effet que l'horaire compte.
  EffectNodePool pool;
  {
    const EffectNodePool::AnchoredSowing anchor{pool, kBearer, kBirthPlace, kKaizelEffect};
    pool.SowAtEffectTick(10);
    pool.Add(LongLivedNode());
  }
  AdvanceTicks(pool, kKaizelRejoinTick - 10 - 1);
  ro::test_support::MoveBearer(pool, kBearer, kBirthPlace + glm::vec3{8.0F, 0.0F, 0.0F});
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(kBirthPlace.x));
  AdvanceTicks(pool, 1);
  ro::test_support::MoveBearer(pool, kBearer, kBirthPlace + glm::vec3{8.0F, 0.0F, 0.0F});
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(kBirthPlace.x + 8.0F));
}

TEST_CASE("Troupe : un nœud semé hors de tout effet nommé suit à chaque tic",
          "[effect_bearer_follow]") {
  // Rien ne dit qu'il resterait en place : c'était la règle de toute ancre.
  EffectNodePool pool;
  pool.AnchorNextTo(kBearer, kBirthPlace);
  pool.Add(LongLivedNode());
  pool.AnchorNextTo(0);
  ro::test_support::MoveBearer(pool, kBearer, kBirthPlace + glm::vec3{12.0F, 0.0F, 0.0F});
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(kBirthPlace.x + 12.0F));
}

TEST_CASE("Troupe : suivre une liste de porteurs revient à les suivre un par un",
          "[effect_bearer_follow]") {
  constexpr std::uint32_t kFirstBearer = 2000;
  constexpr std::uint32_t kSecondBearer = 2001;
  constexpr std::uint32_t kAbsentBearer = 2002;
  const glm::vec3 kFirstPlace{40.0F, 3.0F, 70.0F};
  const glm::vec3 kSecondPlace{-5.0F, 1.0F, 9.0F};

  // Un nœud accroché au premier qui vise le second, l'inverse, un accroché à un
  // absent, et une marque sans porteur.
  const auto fill = [](EffectNodePool& pool) {
    const auto add = [&pool](std::uint32_t anchor_id, std::uint32_t target_id) {
      PooledEffectNode node;
      node.node.life_ticks = 9999;
      node.target_actor_id = target_id;
      pool.AnchorNextTo(anchor_id);
      pool.Add(node);
      pool.AnchorNextTo(0);
    };
    add(kFirstBearer, kSecondBearer);
    add(kSecondBearer, kFirstBearer);
    add(kAbsentBearer, 0);
    add(0, 0);
  };
  EffectNodePool one_by_one;
  fill(one_by_one);
  EffectNodePool all_at_once;
  fill(all_at_once);

  ro::test_support::MoveBearer(one_by_one, kFirstBearer, kFirstPlace);
  ro::test_support::MoveBearer(one_by_one, kSecondBearer, kSecondPlace);
  const std::vector<ro::render::EffectBearerPlace> places = {
      {.actor_id = kFirstBearer, .ground_position = kFirstPlace},
      {.actor_id = kSecondBearer, .ground_position = kSecondPlace}};
  all_at_once.MoveBearers(places);

  REQUIRE(all_at_once.size() == one_by_one.size());
  for (std::size_t rank = 0; rank < one_by_one.size(); ++rank) {
    const PooledEffectNode& expected = one_by_one.nodes()[rank];
    const PooledEffectNode& got = all_at_once.nodes()[rank];
    CHECK(got.ground_position == expected.ground_position);
    CHECK(got.owner_position == expected.owner_position);
    CHECK(got.anchor_ground_position == expected.anchor_ground_position);
    CHECK(got.target_position == expected.target_position);
  }
  // ⚠ L'ESSAI NE COMPARE PAS DU VIDE : le suivi a bien eu lieu.
  CHECK(all_at_once.nodes()[0].ground_position == kFirstPlace);
  CHECK(all_at_once.nodes()[0].target_position == kSecondPlace);

  std::vector<std::uint32_t> ids;
  all_at_once.CollectBearerIds(ids);
  // Une ancre et une cible pour chacun des deux premiers nœuds, une ancre pour le
  // troisième, et jamais le zéro de la marque.
  CHECK(ids == std::vector<std::uint32_t>{kFirstBearer, kSecondBearer, kSecondBearer, kFirstBearer,
                                          kAbsentBearer});
}
