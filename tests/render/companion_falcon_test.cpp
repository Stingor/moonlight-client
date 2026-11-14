#include "ro/render/companion_falcon.h"

#include <cstddef>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/sprite_frame.h"

using Catch::Approx;
using ro::render::BornFalcon;
using ro::render::CarriesFalcon;
using ro::render::Falcon;
using ro::render::FalconAction;
using ro::render::FalconArtFor;
using ro::render::FalconFrameCount;
using ro::render::FalconGround;
using ro::render::FalconMaster;
using ro::render::FalconPoint;
using ro::render::FalconScreenDirection;
using ro::render::LaunchFalcon;
using ro::render::PlayerFalconArtFor;
using ro::render::StepFalcon;

namespace {

constexpr std::uint32_t kNovice = 0;
constexpr std::uint32_t kSniper = 4012;
constexpr std::uint32_t kBabyHunter = 4034;
constexpr std::uint32_t kHunter = 11;
constexpr std::uint32_t kRanger = 4056;
constexpr std::uint32_t kRangerTrans = 4062;
constexpr std::uint32_t kBabyRanger = 4098;
constexpr std::uint32_t kRangerOutfit = 4337;
constexpr std::uint32_t kWindHawk = 4257;
constexpr std::uint32_t kWindHawkRiding = 4278;
constexpr std::uint32_t kGangsi = 4050;
constexpr std::uint32_t kDeathKnight = 4051;
constexpr std::uint32_t kDarkCollector = 4052;
constexpr float kGroundHeight = -7.0F;
constexpr std::size_t kFramesPerAction = 4;

const FalconGround kFlatGround = [](float, float) { return kGroundHeight; };
const FalconFrameCount kFourFrames = [](std::size_t) { return kFramesPerAction; };

FalconMaster MasterAt(float x, float z) {
  return FalconMaster{FalconPoint{x, kGroundHeight, z}, 0.0F};
}

}  // namespace

TEST_CASE("l'oiseau se choisit sur le métier montré", "[render][falcon]") {
  CHECK(FalconArtFor(kNovice, 0).act_path == "data\\sprite\\이팩트\\매.act");
  CHECK(FalconArtFor(kSniper, 0).spr_path == "data\\sprite\\이팩트\\매2.spr");
  // Le bébé chasseur : l'image du faucon, ses propres mouvements.
  CHECK(FalconArtFor(kBabyHunter, 0).spr_path == "data\\sprite\\이팩트\\매.spr");
  CHECK(FalconArtFor(kBabyHunter, 0).act_path == "data\\sprite\\이팩트\\베이비매.act");
  CHECK(FalconArtFor(kRanger, 0).spr_path == "data\\sprite\\이팩트\\OWL.spr");
  CHECK(FalconArtFor(kRangerTrans, 0).spr_path == "data\\sprite\\이팩트\\OWL.spr");
  CHECK(FalconArtFor(kBabyRanger, 0).act_path == "data\\sprite\\이팩트\\OWL.act");
  CHECK(FalconArtFor(kWindHawk, 0).spr_path == "data\\sprite\\이팩트\\WINDHAWK_HAWK.spr");
  // La tenue alternative n'est pas un métier montré : faucon ordinaire.
  CHECK(FalconArtFor(kRangerOutfit, 0).spr_path == "data\\sprite\\이팩트\\매.spr");

  SECTION("un ranger monté perd sa chouette, un Wind Hawk monté garde son épervier") {
    CHECK(FalconArtFor(kRanger, ro::render::kOptionWolfRider).spr_path ==
          "data\\sprite\\이팩트\\매.spr");
    CHECK(FalconArtFor(kWindHawk, ro::render::kOptionWolfRider).spr_path ==
          "data\\sprite\\이팩트\\WINDHAWK_HAWK.spr");
  }
  SECTION("un costume rend le faucon ordinaire, sauf sous le vol — mariage excepté") {
    CHECK(FalconArtFor(kRanger, ro::render::kOptionXmas).spr_path ==
          "data\\sprite\\이팩트\\매.spr");
    CHECK(FalconArtFor(kRanger, ro::render::kOptionXmas | ro::render::kOptionFlying).spr_path ==
          "data\\sprite\\이팩트\\OWL.spr");
    CHECK(FalconArtFor(kRanger, ro::render::kOptionWedding | ro::render::kOptionFlying).spr_path ==
          "data\\sprite\\이팩트\\매.spr");
  }
}

TEST_CASE("l'oiseau d'un joueur se choisit sur son style de corps", "[render][falcon]") {
  const auto spr_of = [](std::uint32_t body_style) {
    return PlayerFalconArtFor(body_style).spr_path;
  };
  CHECK(spr_of(kHunter) == "data\\sprite\\이팩트\\매.spr");
  CHECK(spr_of(kSniper) == "data\\sprite\\이팩트\\매2.spr");
  CHECK(PlayerFalconArtFor(kSniper).act_path == "data\\sprite\\이팩트\\매2.act");
  CHECK(spr_of(kBabyHunter) == "data\\sprite\\이팩트\\매.spr");
  CHECK(PlayerFalconArtFor(kBabyHunter).act_path == "data\\sprite\\이팩트\\베이비매.act");
  CHECK(spr_of(kRanger) == "data\\sprite\\이팩트\\OWL.spr");
  CHECK(spr_of(kRangerTrans) == "data\\sprite\\이팩트\\OWL.spr");
  CHECK(spr_of(kBabyRanger) == "data\\sprite\\이팩트\\OWL.spr");
  CHECK(spr_of(kRangerOutfit) == "data\\sprite\\이팩트\\OWL.spr");
  CHECK(PlayerFalconArtFor(kRanger).act_path == "data\\sprite\\이팩트\\OWL.act");
  CHECK(spr_of(kWindHawk) == "data\\sprite\\이팩트\\WINDHAWK_HAWK.spr");
  CHECK(spr_of(kWindHawkRiding) == "data\\sprite\\이팩트\\WINDHAWK_HAWK.spr");
  CHECK(PlayerFalconArtFor(kWindHawk).act_path == "data\\sprite\\이팩트\\WINDHAWK_HAWK.act");
  CHECK(spr_of(kGangsi) == "data\\sprite\\몬스터\\demon_pungus.spr");
  CHECK(spr_of(kDeathKnight) == "data\\sprite\\몬스터\\explosion.spr");
  CHECK(spr_of(kDarkCollector) == "data\\sprite\\몬스터\\lude.spr");
  CHECK(spr_of(0) == "data\\sprite\\이팩트\\매.spr");

  SECTION("le skin du styliste porte l'oiseau du métier emprunté") {
    // Un tireur d'élite au skin de troisième classe porte le corps du ranger
    // transcendé ; au skin de quatrième, celui du Wind Hawk.
    CHECK(spr_of(kRangerTrans) != spr_of(kSniper));
    CHECK(spr_of(kWindHawk) != spr_of(kSniper));
    // Un chasseur au skin de troisième classe : le ranger.
    CHECK(spr_of(kRanger) == "data\\sprite\\이팩트\\OWL.spr");
  }
}

TEST_CASE("caché, camouflé ou invisible, un acteur perd son faucon", "[render][falcon]") {
  CHECK(CarriesFalcon(ro::render::kOptionFalcon));
  CHECK_FALSE(CarriesFalcon(0));
  CHECK_FALSE(CarriesFalcon(ro::render::kOptionFalcon | 0x2));
  CHECK_FALSE(CarriesFalcon(ro::render::kOptionFalcon | 0x4));
  CHECK_FALSE(CarriesFalcon(ro::render::kOptionFalcon | 0x40));
}

TEST_CASE("le faucon naît au-dessus et derrière son maître", "[render][falcon]") {
  const Falcon north = BornFalcon(FalconMaster{FalconPoint{100.0F, -3.0F, 50.0F}, 0.0F});
  CHECK(north.position.x == Approx(100.0F));
  CHECK(north.position.y == Approx(-23.0F));
  CHECK(north.position.z == Approx(55.0F));
  CHECK(north.arrived);

  const Falcon east = BornFalcon(FalconMaster{FalconPoint{100.0F, -3.0F, 50.0F}, 90.0F});
  CHECK(east.position.x == Approx(95.0F));
  CHECK(east.position.z == Approx(50.0F).margin(1e-4));
}

TEST_CASE("au repos, le faucon suit son maître et plane au-dessus du sol", "[render][falcon]") {
  Falcon falcon;
  falcon.position = FalconPoint{0.0F, 0.0F, 0.0F};

  SECTION("loin, une unité par tic") {
    StepFalcon(falcon, MasterAt(50.0F, 0.0F), kFlatGround, kFourFrames, 0.0F);
    CHECK(falcon.position.x == Approx(1.0F));
    CHECK(falcon.position.y == Approx(kGroundHeight - ro::render::kFalconHoverUnits));
    // Tourné vers l'abscisse croissante.
    CHECK(falcon.heading_degrees == Approx(90.0F));
  }
  SECTION("de près, un vingtième de l'écart ; tout près, plus rien") {
    StepFalcon(falcon, MasterAt(10.0F, 0.0F), kFlatGround, kFourFrames, 0.0F);
    CHECK(falcon.position.x == Approx(0.5F));
    falcon.position.x = 8.0F;
    StepFalcon(falcon, MasterAt(10.0F, 0.0F), kFlatGround, kFourFrames, 0.0F);
    CHECK(falcon.position.x == Approx(8.0F));
  }
  SECTION("il bat des ailes une image tous les six tics, en boucle") {
    std::size_t seen_last = 0;
    for (int tick = 0; tick < ro::render::kFalconFrameDelayTicks * 5; ++tick) {
      StepFalcon(falcon, MasterAt(0.0F, 0.0F), kFlatGround, kFourFrames, 0.0F);
      seen_last = falcon.frame;
    }
    // Cinq battements depuis l'image zéro, sur quatre images : la deuxième.
    CHECK(seen_last == 1);
  }
}

TEST_CASE("le lancer : piqué, palier, remontée, puis retour en planant", "[render][falcon]") {
  Falcon falcon;
  falcon.position = FalconPoint{0.0F, -25.0F, 0.0F};
  falcon.counter_ticks = 1;
  LaunchFalcon(falcon, FalconPoint{20.0F, 0.0F, 10.0F});
  CHECK(falcon.flying);
  CHECK_FALSE(falcon.arrived);
  CHECK(falcon.velocity.x == Approx(2.0F));
  CHECK(falcon.velocity.y == Approx(2.5F));

  const FalconMaster master = MasterAt(0.0F, 0.0F);
  // Du premier au dixième tic, il descend et avance.
  for (int tick = 1; tick <= ro::render::kFalconDescentLastTick; ++tick) {
    StepFalcon(falcon, master, kFlatGround, kFourFrames, 0.0F);
  }
  CHECK(falcon.position.x == Approx(20.0F));
  CHECK(falcon.position.y == Approx(0.0F));
  CHECK(falcon.position.z == Approx(10.0F));
  CHECK(FalconAction(falcon, 0.0F) >= ro::render::kFalconFlightActionBase);
  CHECK(falcon.frame == 0);
  // Du onzième au vingt-quatrième, il file à plat.
  for (int tick = ro::render::kFalconDescentLastTick + 1; tick < ro::render::kFalconClimbFirstTick;
       ++tick) {
    StepFalcon(falcon, master, kFlatGround, kFourFrames, 0.0F);
  }
  CHECK(falcon.position.y == Approx(0.0F));
  CHECK_FALSE(falcon.climbing);
  // À partir du vingt-cinquième, la vitesse verticale change de signe et grandit.
  StepFalcon(falcon, master, kFlatGround, kFourFrames, 0.0F);
  CHECK(falcon.climbing);
  CHECK(falcon.velocity.y == Approx(2.5F * ro::render::kFalconClimbFactor));
  while (falcon.counter_ticks <= ro::render::kFalconFlightLastTick) {
    StepFalcon(falcon, master, kFlatGround, kFourFrames, 0.0F);
  }
  // Le tic qui clôt le vol ne bouge rien.
  const FalconPoint before = falcon.position;
  StepFalcon(falcon, master, kFlatGround, kFourFrames, 0.0F);
  CHECK_FALSE(falcon.flying);
  CHECK(falcon.position.x == Approx(before.x));
  // Puis il revient en planant, ailes figées.
  StepFalcon(falcon, master, kFlatGround, kFourFrames, 0.0F);
  CHECK(falcon.frame == 0);  // l'image figée dépasse les quatre images : elle retombe à zéro
  CHECK(falcon.position.y == Approx(kGroundHeight - ro::render::kFalconHoverUnits));
}

TEST_CASE("Blitz Beat et Falcon Assault envoient le faucon vers leur cible", "[render][falcon]") {
  // Les cinq compétences de dégâts qui appellent l'oiseau de leur auteur.
  constexpr std::uint32_t kBlitzBeat = 129;
  constexpr std::uint32_t kFalconAssault = 381;
  constexpr std::uint32_t kHawkRush = 5326;
  constexpr std::uint32_t kHawkBoomerang = 5329;
  constexpr std::uint32_t kWildWalk = 6520;
  CHECK(ro::render::SkillHitSendsFalcon(kBlitzBeat));
  CHECK(ro::render::SkillHitSendsFalcon(kFalconAssault));
  CHECK(ro::render::SkillHitSendsFalcon(kHawkRush));
  CHECK(ro::render::SkillHitSendsFalcon(kHawkBoomerang));
  CHECK(ro::render::SkillHitSendsFalcon(kWildWalk));

  SECTION("ni Detecting, qui le lance par sa case, ni Spring Trap, par son piège") {
    constexpr std::uint32_t kDetecting = 130;
    constexpr std::uint32_t kSpringTrap = 131;
    CHECK_FALSE(ro::render::SkillHitSendsFalcon(kDetecting));
    CHECK_FALSE(ro::render::SkillHitSendsFalcon(kSpringTrap));
  }
  SECTION("ni les crocs du loup du ranger, qui appellent un autre compagnon") {
    constexpr std::uint32_t kWargStrike = 2243;
    constexpr std::uint32_t kWargBite = 2244;
    CHECK_FALSE(ro::render::SkillHitSendsFalcon(kWargStrike));
    CHECK_FALSE(ro::render::SkillHitSendsFalcon(kWargBite));
  }
}

TEST_CASE("la direction d'écran d'un cap rejoint celle d'une boussole", "[render][falcon]") {
  constexpr float kDegreesPerEighth = 45.0F;
  for (std::size_t compass = 0; compass < ro::render::kDirectionCount; ++compass) {
    for (int turned = 0; turned < 8; ++turned) {
      const float yaw = kDegreesPerEighth * static_cast<float>(turned);
      const float heading = kDegreesPerEighth * static_cast<float>((compass + 4) % 8);
      CHECK(FalconScreenDirection(heading, yaw) == ro::render::SpriteDirection(compass, yaw));
    }
  }
}
