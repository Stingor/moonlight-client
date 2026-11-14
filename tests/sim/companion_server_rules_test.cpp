// LES RÉGLAGES DE L'IA SOUS LES RÈGLES DU SERVEUR : les distances bornées au
// rayon de vue, et le délai d'arrivée d'usine qui suit celui du serveur.

#include "ro/sim/companion_server_rules.h"

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/session_settings.h"

using ro::sim::CompanionAiSettings;
using ro::sim::SessionSettings;
using ro::sim::SettingsUnderServerRules;

namespace {

constexpr int kMoonlightViewCells = 18;
constexpr int kFarCells = 30;
constexpr int kNearCells = 7;
constexpr std::uint32_t kServerHoldMs = 3000;
constexpr std::uint32_t kChosenHoldMs = 1500;

[[nodiscard]] SessionSettings Moonlight() {
  SessionSettings session;
  session.view_cells = kMoonlightViewCells;
  return session;
}

}  // namespace

TEST_CASE(
    "IA sous les règles du serveur : la portée du jeu borne, même quand la vue porte plus loin",
    "[sim][companion_rules]") {
  constexpr int kMoonclientViewCells = 40;
  SessionSettings session;
  session.view_cells = kMoonclientViewCells;
  session.game_area_cells = kMoonlightViewCells;
  CompanionAiSettings settings;
  settings.engage_still_cells = kFarCells;
  const auto applied = SettingsUnderServerRules(settings, session);
  REQUIRE(applied.has_value());
  CHECK(applied->engage_still_cells == kMoonlightViewCells);
}

TEST_CASE("IA sous les règles du serveur : rien ne change, rien ne se copie",
          "[sim][companion_rules]") {
  CompanionAiSettings settings;
  settings.follow_distance_cells = kNearCells;
  settings.engage_still_cells = kMoonlightViewCells;
  CHECK_FALSE(SettingsUnderServerRules(settings, Moonlight()).has_value());
}

TEST_CASE("IA sous les règles du serveur : une distance au-delà du rayon de vue y est bornée",
          "[sim][companion_rules]") {
  CompanionAiSettings settings;
  settings.follow_distance_cells = kNearCells;
  settings.leash_still_cells = kFarCells;
  settings.leash_walking_cells = kFarCells;
  settings.engage_still_cells = kFarCells;
  settings.engage_walking_cells = kMoonlightViewCells + 1;
  const std::optional<CompanionAiSettings> applied =
      SettingsUnderServerRules(settings, Moonlight());
  REQUIRE(applied.has_value());
  CHECK(applied->follow_distance_cells == kNearCells);
  CHECK(applied->leash_still_cells == kMoonlightViewCells);
  CHECK(applied->leash_walking_cells == kMoonlightViewCells);
  CHECK(applied->engage_still_cells == kMoonlightViewCells);
  CHECK(applied->engage_walking_cells == kMoonlightViewCells);
  // ⚠ LE PROFIL GARDE LA VALEUR DU JOUEUR : seule la copie appliquée est bornée.
  CHECK(settings.leash_still_cells == kFarCells);

  // Sans règle dite, le rayon par défaut d'un serveur d'usine borne.
  const std::optional<CompanionAiSettings> stock = SettingsUnderServerRules(settings, {});
  REQUIRE(stock.has_value());
  CHECK(stock->leash_still_cells == SessionSettings::kDefaultViewCells);
}

TEST_CASE("IA sous les règles du serveur : le délai d'arrivée d'usine suit celui du serveur",
          "[sim][companion_rules]") {
  SessionSettings session = Moonlight();
  session.homunculus_teleport_hold_ms = kServerHoldMs;
  const CompanionAiSettings factory;
  const std::optional<CompanionAiSettings> applied = SettingsUnderServerRules(factory, session);
  REQUIRE(applied.has_value());
  CHECK(applied->arrival_attack_hold_ms == kServerHoldMs);

  // UN DÉLAI CHOISI PAR LE JOUEUR reste le sien.
  CompanionAiSettings chosen;
  chosen.arrival_attack_hold_ms = kChosenHoldMs;
  CHECK_FALSE(SettingsUnderServerRules(chosen, session).has_value());
}
