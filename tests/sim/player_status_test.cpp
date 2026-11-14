// Les paramètres du personnage, rangés sous les numéros du serveur.

#include "ro/sim/player_status.h"

#include <array>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"

namespace {

using ro::sim::SkillPointsSpendable;
using ro::sim::StatusPointsSpendable;

using ro::sim::ParameterOutcome;
using ro::sim::PlayerStatus;
using ro::sim::ServerParameter;

[[nodiscard]] std::uint16_t Number(ServerParameter parameter) {
  return static_cast<std::uint16_t>(parameter);
}

// Un joueur posé dans le monde, avec son identifiant connu : sans lui, la vie
// n'a aucun acteur où se ranger.
[[nodiscard]] ro::sim::World WorldWithPlayer(std::uint32_t id) {
  ro::sim::World world;
  ro::sim::Actor self;
  self.id = id;
  self.kind = ro::sim::ActorKind::kPlayer;
  world.Spawn(self);
  world.set_player_id(id);
  return world;
}

}  // namespace

TEST_CASE("un paramètre non dit vaut MOINS UN, jamais zéro", "[sim]") {
  // ⚠ La distinction n'est pas cosmétique : une bourse à zéro s'affiche, une
  // bourse dont le serveur n'a pas parlé ne s'affiche pas.
  const PlayerStatus fresh;
  CHECK(fresh.zeny == ro::sim::kUnknownStat);
  CHECK(fresh.base_level == ro::sim::kUnknownStat);
  CHECK(fresh.str == ro::sim::kUnknownStat);
  CHECK(fresh.max_sp == ro::sim::kUnknownStat);
  CHECK(fresh.zeny != 0);
}

TEST_CASE("chaque paramètre connu se range sous son numéro de serveur", "[sim]") {
  PlayerStatus status;
  CHECK(ro::sim::ApplyParameter(status, Number(ServerParameter::kZeny), 123456789012LL) ==
        ParameterOutcome::kStored);
  CHECK(ro::sim::ApplyParameter(status, Number(ServerParameter::kBaseLevel), 99) ==
        ParameterOutcome::kStored);
  CHECK(ro::sim::ApplyParameter(status, Number(ServerParameter::kStr), 42) ==
        ParameterOutcome::kStored);
  CHECK(ro::sim::ApplyParameter(status, Number(ServerParameter::kUStr), 7) ==
        ParameterOutcome::kStored);

  CHECK(status.zeny == 123456789012LL);
  CHECK(status.base_level == 99);
  CHECK(status.str == 42);
  CHECK(status.str_cost == 7);
}

TEST_CASE("la bourse ne se tronque pas sur trente-deux bits", "[sim]") {
  // C'est la raison d'être de ZC_LONGLONGPAR_CHANGE côté serveur : une valeur
  // rangée plus court se tronquerait EN SILENCE.
  PlayerStatus status;
  constexpr std::int64_t kBeyondThirtyTwoBits = 5'000'000'000LL;
  REQUIRE(ro::sim::ApplyParameter(status, Number(ServerParameter::kZeny), kBeyondThirtyTwoBits) ==
          ParameterOutcome::kStored);
  CHECK(status.zeny == kBeyondThirtyTwoBits);
}

TEST_CASE("un paramètre écarté volontairement n'est PAS un paramètre inconnu", "[sim]") {
  // ⚠ Les confondre ferait disparaître le travail restant du relevé des trames.
  PlayerStatus status;
  CHECK(ro::sim::ApplyParameter(status, Number(ServerParameter::kKarma), 3) ==
        ParameterOutcome::kKnownButUnused);
  // Les bonnes manières, elles, se gardent : un compte négatif coupe la parole.
  CHECK(ro::sim::ApplyParameter(status, Number(ServerParameter::kManner), -5) ==
        ParameterOutcome::kStored);
  CHECK(status.manner == -5);
  // Un numéro que personne n'a classé reste visible.
  constexpr std::uint16_t kNeverSeen = 999;
  CHECK(ro::sim::ApplyParameter(status, kNeverSeen, 1) == ParameterOutcome::kUnknown);
}

TEST_CASE("la vie et la vitesse ne passent pas par la table du personnage", "[sim]") {
  // Elles vivent sur l'acteur, parce que tout le monde en a. Cette fonction ne
  // connaît pas le monde : elle refuse plutôt que de les perdre.
  PlayerStatus status;
  CHECK(ro::sim::ApplyParameter(status, Number(ServerParameter::kHp), 100) ==
        ParameterOutcome::kUnknown);
  CHECK(ro::sim::ApplyParameter(status, Number(ServerParameter::kSpeed), 150) ==
        ParameterOutcome::kUnknown);
}

TEST_CASE("le monde route la vie vers l'acteur et le reste vers le personnage", "[sim]") {
  constexpr std::uint32_t kSelf = 2000000;
  ro::sim::World world = WorldWithPlayer(kSelf);

  REQUIRE(world.SetPlayerParameter(Number(ServerParameter::kMaxHp), 500) ==
          ParameterOutcome::kStored);
  REQUIRE(world.SetPlayerParameter(Number(ServerParameter::kHp), 320) == ParameterOutcome::kStored);
  const ro::sim::Actor* const self = world.player();
  REQUIRE(self != nullptr);
  CHECK(self->health == 320);
  CHECK(self->max_health == 500);

  REQUIRE(world.SetPlayerParameter(Number(ServerParameter::kSp), 44) == ParameterOutcome::kStored);
  CHECK(world.player_status().sp == 44);
  CHECK(world.player_status().max_sp == ro::sim::kUnknownStat);
}

TEST_CASE("un coup reçu n'efface pas la vie maximale", "[sim]") {
  // ⚠ LE DÉFAUT QUE CE TEST GARDE. Le serveur annonce la vie et son maximum en
  // DEUX paramètres ; `SetHealth` rend la vie inconnue dès que le maximum est
  // nul ou négatif. Poser la vie seule en oubliant le maximum effacerait donc
  // la barre à chaque coup reçu.
  constexpr std::uint32_t kSelf = 2000000;
  ro::sim::World world = WorldWithPlayer(kSelf);
  REQUIRE(world.SetPlayerParameter(Number(ServerParameter::kMaxHp), 500) ==
          ParameterOutcome::kStored);
  REQUIRE(world.SetPlayerParameter(Number(ServerParameter::kHp), 500) == ParameterOutcome::kStored);

  REQUIRE(world.SetPlayerParameter(Number(ServerParameter::kHp), 120) == ParameterOutcome::kStored);
  const ro::sim::Actor* const self = world.player();
  REQUIRE(self != nullptr);
  CHECK(self->health == 120);
  CHECK(self->max_health == 500);
}

TEST_CASE("la vie arrivée avant son maximum n'est pas perdue", "[sim]") {
  // 🔴 L'ORDRE N'EST PAS GARANTI : une vie isolée peut précéder le premier
  // maximum. Elle se lisait « inconnue », et le maximum la ramenait à zéro.
  constexpr std::uint32_t kSelf = 2000000;
  ro::sim::World world = WorldWithPlayer(kSelf);
  REQUIRE(world.SetPlayerParameter(Number(ServerParameter::kHp), 3000) ==
          ParameterOutcome::kStored);
  REQUIRE(world.SetPlayerParameter(Number(ServerParameter::kMaxHp), 5000) ==
          ParameterOutcome::kStored);
  const ro::sim::Actor* const self = world.player();
  REQUIRE(self != nullptr);
  CHECK(self->health == 3000);
  CHECK(self->max_health == 5000);
}

TEST_CASE("une caractéristique arrive avec son bonus, et les deux se gardent", "[sim]") {
  // « 40 + 12 » dit d'où vient le total ; un seul nombre ne le dirait pas.
  constexpr std::uint32_t kSelf = 2000000;
  ro::sim::World world = WorldWithPlayer(kSelf);

  REQUIRE(world.SetPlayerAttribute(Number(ServerParameter::kDex), 40, 12));
  CHECK(world.player_status().dex == 40);
  CHECK(world.player_status().dex_bonus == 12);

  // Un numéro qui n'est pas une caractéristique est refusé plutôt que rangé
  // n'importe où.
  CHECK_FALSE(world.SetPlayerAttribute(Number(ServerParameter::kZeny), 1, 2));
}

TEST_CASE("les treize valeurs dérivées du combat se rangent", "[sim]") {
  // ⚠ ELLES NE SE DÉDUISENT PAS DES CARACTÉRISTIQUES : le serveur les CALCULE,
  // arme et raffinage compris. Un client qui les referait afficherait autre
  // chose que ce qui le frappe.
  PlayerStatus status;
  const std::array<std::pair<ServerParameter, std::int64_t>, 13> derived = {{
      {ServerParameter::kAtk1, 120},
      {ServerParameter::kAtk2, 35},
      {ServerParameter::kMatk1, 40},
      {ServerParameter::kMatk2, 55},
      {ServerParameter::kDef1, 22},
      {ServerParameter::kDef2, 8},
      {ServerParameter::kMdef1, 11},
      {ServerParameter::kMdef2, 4},
      {ServerParameter::kHit, 90},
      {ServerParameter::kFlee1, 77},
      {ServerParameter::kFlee2, 6},
      {ServerParameter::kCritical, 13},
      {ServerParameter::kAspd, 165},
  }};
  for (const auto& [parameter, value] : derived) {
    CHECK(ro::sim::ApplyParameter(status, Number(parameter), value) == ParameterOutcome::kStored);
  }

  CHECK(status.atk1 == 120);
  CHECK(status.atk2 == 35);
  CHECK(status.matk1 == 40);
  CHECK(status.matk2 == 55);
  CHECK(status.def1 == 22);
  CHECK(status.mdef2 == 4);
  CHECK(status.hit == 90);
  CHECK(status.flee2 == 6);
  CHECK(status.critical == 13);
  // ⚠ SOUS SON VRAI NOM : SP_ASPD porte le délai de motion, pas la vitesse
  // d'attaque affichée. Le ranger dans un champ nommé `aspd` aurait invité à
  // l'afficher tel quel.
  CHECK(status.amotion == 165);
}

TEST_CASE("les treize étaient INCONNUES avant d'arriver", "[sim]") {
  // Ce qui distingue « pas encore dit » de « zéro » : une défense nulle
  // s'affiche, une défense dont le serveur n'a pas parlé ne s'affiche pas.
  const PlayerStatus fresh;
  CHECK(fresh.atk1 == ro::sim::kUnknownStat);
  CHECK(fresh.critical == ro::sim::kUnknownStat);
  CHECK(fresh.amotion == ro::sim::kUnknownStat);
}

TEST_CASE("la vitesse d'attaque affichee se derive du delai de motion", "[sim][fiche]") {
  // 🔴 LE SERVEUR N'ENVOIE JAMAIS CE NOMBRE. Il envoie `amotion`, et le client le
  // retourne : deux secondes de delai valent zero, et chaque centieme de seconde
  // gagne vaut un point. Releve dans le client, pas deduit.
  CHECK(ro::sim::DisplayedAspd(2000) == 0);
  CHECK(ro::sim::DisplayedAspd(1000) == 100);
  CHECK(ro::sim::DisplayedAspd(165) == 183);
  CHECK(ro::sim::DisplayedAspd(200) == 180);

  // ⚠ « PAS ENCORE DIT » NE DOIT PAS DEVENIR UN CHIFFRE PLAUSIBLE. Sans garde,
  // la sentinelle donnerait 200 — une vitesse d'attaque que personne ne mettrait
  // en doute, et qui serait fausse.
  CHECK(ro::sim::DisplayedAspd(ro::sim::kUnknownStat) == ro::sim::kUnknownStat);
}

TEST_CASE("des points de statut se placent quand une caractéristique les vaut", "[sim][status]") {
  PlayerStatus status;
  // Rien n'est encore dit.
  CHECK_FALSE(StatusPointsSpendable(status));
  status.status_point = 5;
  status.str_cost = 6;
  status.agi_cost = 9;
  // Cinq points, et tout coûte davantage.
  CHECK_FALSE(StatusPointsSpendable(status));
  status.agi_cost = 5;
  CHECK(StatusPointsSpendable(status));
  // Au plafond, le serveur annonce un coût nul : les points restent, sans
  // pouvoir se placer.
  status.str_cost = 0;
  status.agi_cost = 0;
  CHECK_FALSE(StatusPointsSpendable(status));
  status.status_point = 0;
  status.str_cost = 2;
  CHECK_FALSE(StatusPointsSpendable(status));
}

TEST_CASE("des points de skill se placent dès qu'il en reste un", "[sim][status]") {
  PlayerStatus status;
  CHECK_FALSE(SkillPointsSpendable(status));
  status.skill_point = 0;
  CHECK_FALSE(SkillPointsSpendable(status));
  status.skill_point = 1;
  CHECK(SkillPointsSpendable(status));
}
