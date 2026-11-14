#include <algorithm>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::World;

TEST_CASE("Monde : le délai de combo se note sur son acteur", "[sim][world]") {
  constexpr std::uint32_t kMoine = 170'021;
  constexpr std::uint32_t kUnknown = 170'099;
  constexpr std::uint32_t kDelaiMs = 800;
  constexpr std::uint32_t kInstantMs = 5'000;
  World world;
  Actor moine;
  moine.id = kMoine;
  world.Spawn(moine);
  REQUIRE(world.RecordComboDelay(kMoine, kDelaiMs, kInstantMs));
  CHECK(world.Find(kMoine)->combo_delay_ms == kDelaiMs);
  CHECK(world.Find(kMoine)->combo_delay_since_ms == kInstantMs);
  CHECK_FALSE(world.RecordComboDelay(kUnknown, kDelaiMs, kInstantMs));
}

TEST_CASE("Monde : les sphères élémentaires retiennent leur élément", "[sim][world]") {
  constexpr std::uint32_t kNinja = 170'031;
  constexpr std::int16_t kFeu = 3;
  constexpr std::int16_t kVent = 4;
  constexpr std::uint32_t kFireEffect = 735;
  constexpr std::uint32_t kWindEffect = 736;
  World world;
  Actor ninja;
  ninja.id = kNinja;
  world.Spawn(ninja);
  REQUIRE(world.RecordSpiritCharm(kNinja, kFeu, 2, 100));
  CHECK(world.Find(kNinja)->spirit_charm_element == kFeu);
  REQUIRE(world.Find(kNinja)->effects.size() == 1);
  CHECK(world.Find(kNinja)->effects[0].id == kFireEffect);
  CHECK(world.Find(kNinja)->effects[0].value == 2);
  // Un compte nul retire les sphères et GARDE l'élément.
  REQUIRE(world.RecordSpiritCharm(kNinja, kFeu, 0, 200));
  CHECK(world.Find(kNinja)->effects.empty());
  CHECK(world.Find(kNinja)->spirit_charm_element == kFeu);
  REQUIRE(world.RecordSpiritCharm(kNinja, kVent, 1, 300));
  CHECK(world.Find(kNinja)->spirit_charm_element == kVent);
  CHECK(world.Find(kNinja)->effects[0].id == kWindEffect);
  CHECK_FALSE(world.RecordSpiritCharm(170'099, kVent, 1, 300));
  // Le cinquième type montre la sphère du sort, que sa classe fait naître.
  constexpr std::int16_t kFifthType = 5;
  constexpr std::uint32_t kSpellSphere = 228;
  REQUIRE(world.RecordSpiritCharm(kNinja, kFifthType, 2, 400));
  REQUIRE(world.Find(kNinja)->effects.size() == 1);
  CHECK(world.Find(kNinja)->effects[0].id == kSpellSphere);
  CHECK(world.Find(kNinja)->effects[0].born_of_class);
  // Un type inconnu retient son élément et ne montre rien.
  constexpr std::int16_t kUnknownType = 9;
  REQUIRE(world.RecordSpiritCharm(kNinja, kUnknownType, 2, 500));
  CHECK(world.Find(kNinja)->spirit_charm_element == kUnknownType);
  CHECK(world.Find(kNinja)->effects.size() == 1);
}

TEST_CASE("Monde : les sphères spirituelles portent leur nombre, par l'effet de la classe",
          "[sim][world]") {
  constexpr std::uint32_t kMonk = 170'051;
  constexpr std::uint32_t kGunslinger = 170'052;
  constexpr std::uint16_t kMonkJob = 15;
  constexpr std::uint16_t kGunslingerJob = 24;
  constexpr std::uint32_t kMonkSpheres = 228;
  constexpr std::uint32_t kGunslingerEffect = 629;
  World world;
  Actor monk;
  monk.id = kMonk;
  monk.look.job = kMonkJob;
  world.Spawn(monk);
  Actor gunslinger;
  gunslinger.id = kGunslinger;
  gunslinger.look.job = kGunslingerJob;
  world.Spawn(gunslinger);

  REQUIRE(world.RecordSpiritSpheres(kMonk, 3, 100));
  REQUIRE(world.Find(kMonk)->effects.size() == 1);
  CHECK(world.Find(kMonk)->effects[0].id == kMonkSpheres);
  CHECK(world.Find(kMonk)->effects[0].value == 3);
  // Le nouveau compte REMPLACE l'ancien : jamais deux cercles à la fois.
  REQUIRE(world.RecordSpiritSpheres(kMonk, 5, 200));
  REQUIRE(world.Find(kMonk)->effects.size() == 1);
  CHECK(world.Find(kMonk)->effects[0].value == 5);
  // Zéro les retire.
  REQUIRE(world.RecordSpiritSpheres(kMonk, 0, 300));
  CHECK(world.Find(kMonk)->effects.empty());

  REQUIRE(world.RecordSpiritSpheres(kGunslinger, 2, 400));
  REQUIRE(world.Find(kGunslinger)->effects.size() == 1);
  CHECK(world.Find(kGunslinger)->effects[0].id == kGunslingerEffect);
  CHECK_FALSE(world.RecordSpiritSpheres(170'099, 1, 500));
}

TEST_CASE("Monde : un retrait s'inscrit au journal, même sans effet à retirer", "[sim][world]") {
  // 🔴 LE JOURNAL COUPE L'INSTANCE QUI JOUE, et la liste d'effets ne garde que
  // les derniers : un effet qu'elle a déjà oublié joue peut-être encore.
  constexpr std::uint32_t kBearer = 170'041;
  constexpr std::uint32_t kEffectId = 737;
  constexpr std::uint32_t kOther = 90;
  World world;
  Actor bearer;
  bearer.id = kBearer;
  world.Spawn(bearer);
  REQUIRE(world.RecordEffect(kBearer, kEffectId, 100));
  CHECK(world.RemoveEffect(kBearer, kEffectId));
  CHECK_FALSE(world.RemoveEffect(kBearer, kOther));
  const Actor& vu = *world.Find(kBearer);
  CHECK(vu.effects.empty());
  CHECK(vu.removed_effects_total == 2);
  REQUIRE(vu.removed_effects.size() == 2);
  CHECK(vu.removed_effects[0] == kEffectId);
  CHECK(vu.removed_effects[1] == kOther);
  // Les sphères retirent leurs quatre effets à chaque annonce.
  REQUIRE(world.RecordSpiritCharm(kBearer, 1, 3, 200));
  CHECK(world.Find(kBearer)->removed_effects_total == 6);
  CHECK_FALSE(world.RemoveEffect(170'099, kEffectId));
}

TEST_CASE("Monde : les armes de servant et les sphères d'ombre suivent le compte à leur façon",
          "[sim][world]") {
  constexpr std::uint32_t kKnight = 170'061;
  constexpr std::uint32_t kChaser = 170'062;
  constexpr std::uint16_t kDragonKnightJob = 4252;
  constexpr std::uint16_t kAbyssChaserJob = 4260;
  constexpr std::uint32_t kServantWeapons = 1694;
  constexpr std::uint32_t kShadowSpheres = 2109;
  World world;
  Actor knight;
  knight.id = kKnight;
  knight.look.job = kDragonKnightJob;
  world.Spawn(knight);
  Actor chaser;
  chaser.id = kChaser;
  chaser.look.job = kAbyssChaserJob;
  world.Spawn(chaser);

  // LES ARMES restent à zéro : l'instance les fait partir une à une.
  REQUIRE(world.RecordSpiritSpheres(kKnight, 3, 100));
  REQUIRE(world.Find(kKnight)->effects.size() == 1);
  CHECK(world.Find(kKnight)->effects[0].id == kServantWeapons);
  CHECK(world.Find(kKnight)->effects[0].value == 3);
  REQUIRE(world.RecordSpiritSpheres(kKnight, 0, 200));
  REQUIRE(world.Find(kKnight)->effects.size() == 1);
  CHECK(world.Find(kKnight)->effects[0].value == 0);
  CHECK(world.Find(kKnight)->effects[0].since_ms == 200);

  // LES SPHÈRES D'OMBRE gardent leur effet d'un compte à l'autre, et partent à zéro.
  REQUIRE(world.RecordSpiritSpheres(kChaser, 2, 300));
  REQUIRE(world.RecordSpiritSpheres(kChaser, 4, 400));
  REQUIRE(world.Find(kChaser)->effects.size() == 1);
  CHECK(world.Find(kChaser)->effects[0].id == kShadowSpheres);
  CHECK(world.Find(kChaser)->effects[0].value == 4);
  REQUIRE(world.RecordSpiritSpheres(kChaser, 0, 500));
  CHECK(world.Find(kChaser)->effects.empty());

  // CACHÉ, le Dragon Knight perd ses armes.
  REQUIRE(world.SetStates(kKnight, 0, 0, ro::sim::kOptionHide, 550));
  REQUIRE(world.RecordSpiritSpheres(kKnight, 2, 600));
  CHECK(std::ranges::find(world.Find(kKnight)->effects, kServantWeapons,
                          &ro::sim::PlayedEffect::id) == world.Find(kKnight)->effects.end());
}
