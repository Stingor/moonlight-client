// LES RÈGLES DU SERVEUR — `ZC_MOONLIGHT_SERVER_RULES 0x0F38`, et ce que la
// session en garde.

#include "ro/net/server_rules.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/session_settings.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::net::ServerRuleKey;
using ro::proto::ZcMoonlightServerRules;
using ro::proto::ZcMoonlightServerRulesEntry;
using ro::sim::SessionSettings;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kNowMs = 5000;
constexpr std::int32_t kMoonlightViewCells = 18;
constexpr std::int32_t kMoonlightMaxLevel = 350;
constexpr std::int32_t kTeleportHoldMs = 3000;
// La taxe des échoppes de vente que Moonlight désactive, et un seuil propre.
constexpr std::int32_t kNoVendingTax = 0;
constexpr std::int32_t kVendingTaxMinZeny = 50'000'000;
// Une clé que ce client ne connaît pas.
constexpr std::uint16_t kUnknownKey = 77;

[[nodiscard]] ZcMoonlightServerRulesEntry Rule(ServerRuleKey key, std::int32_t value) {
  return ZcMoonlightServerRulesEntry{.key = static_cast<std::uint16_t>(key), .value = value};
}

}  // namespace

TEST_CASE("Règles du serveur : sans paquet, les défauts d'un serveur d'usine", "[net][rules]") {
  const ro::sim::World world;
  CHECK(world.session().view_cells == SessionSettings::kDefaultViewCells);
  CHECK(world.session().homunculus_max_level == SessionSettings::kDefaultHomunculusMaxLevel);
  CHECK(world.session().homunculus_s_max_level == SessionSettings::kDefaultHomunculusSMaxLevel);
  CHECK(world.session().homunculus_teleport_hold_ms ==
        SessionSettings::kDefaultHomunculusTeleportHoldMs);
  CHECK(world.session().vending_tax_permyriad == SessionSettings::kDefaultVendingTaxPermyriad);
  CHECK(world.session().vending_tax_min_zeny == SessionSettings::kDefaultVendingTaxMinZeny);
}

TEST_CASE("Règles du serveur : les clés connues se rangent, une inconnue s'ignore",
          "[net][rules]") {
  ro::sim::World world;
  ZcMoonlightServerRules told;
  told.rules = {Rule(ServerRuleKey::kViewCells, kMoonlightViewCells),
                ZcMoonlightServerRulesEntry{.key = kUnknownKey, .value = -1},
                Rule(ServerRuleKey::kHomunculusMaxLevel, kMoonlightMaxLevel),
                Rule(ServerRuleKey::kHomunculusSMaxLevel, kMoonlightMaxLevel),
                Rule(ServerRuleKey::kHomunculusTeleportHoldMs, kTeleportHoldMs),
                Rule(ServerRuleKey::kVendingTaxPermyriad, kNoVendingTax),
                Rule(ServerRuleKey::kVendingTaxMinZeny, kVendingTaxMinZeny)};
  // 🔴 PAR L'AIGUILLAGE ORDINAIRE, comme toute trame du serveur.
  REQUIRE(ApplyFrame(FrameOf(Encode(told)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.session().view_cells == kMoonlightViewCells);
  CHECK(world.session().homunculus_max_level == kMoonlightMaxLevel);
  CHECK(world.session().homunculus_s_max_level == kMoonlightMaxLevel);
  CHECK(world.session().homunculus_teleport_hold_ms == kTeleportHoldMs);
  CHECK(world.session().vending_tax_permyriad == kNoVendingTax);
  CHECK(world.session().vending_tax_min_zeny == kVendingTaxMinZeny);
}

TEST_CASE("Règles du serveur : la portée du jeu et la plus longue marche se rangent à part",
          "[net][rules]") {
  constexpr std::int32_t kMoonclientViewCells = 40;
  constexpr std::int32_t kGameAreaCells = 18;
  constexpr std::int32_t kMaxWalkSteps = 18;
  ro::sim::World world;
  // Tant que le serveur ne l'a pas dite, la portée du jeu se confond avec la vue.
  CHECK(world.session().GameAreaCells() == world.session().view_cells);
  CHECK(world.session().max_walk_path_steps == SessionSettings::kDefaultMaxWalkPathSteps);
  ZcMoonlightServerRules told;
  told.rules = {Rule(ServerRuleKey::kViewCells, kMoonclientViewCells),
                Rule(ServerRuleKey::kGameAreaCells, kGameAreaCells),
                Rule(ServerRuleKey::kMaxWalkPathSteps, kMaxWalkSteps)};
  REQUIRE(ApplyFrame(FrameOf(Encode(told)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.session().view_cells == kMoonclientViewCells);
  CHECK(world.session().GameAreaCells() == kGameAreaCells);
  CHECK(world.session().max_walk_path_steps == kMaxWalkSteps);
}

TEST_CASE("Règles du serveur : une valeur hors de son domaine laisse la précédente",
          "[net][rules]") {
  ro::sim::World world;
  ZcMoonlightServerRules told;
  told.rules = {
      Rule(ServerRuleKey::kViewCells, 0), Rule(ServerRuleKey::kHomunculusTeleportHoldMs, -1),
      Rule(ServerRuleKey::kVendingTaxPermyriad, -1),
      Rule(ServerRuleKey::kVendingTaxPermyriad, SessionSettings::kMaxVendingTaxPermyriad + 1),
      Rule(ServerRuleKey::kVendingTaxMinZeny, -1)};
  REQUIRE(ApplyFrame(FrameOf(Encode(told)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.session().view_cells == SessionSettings::kDefaultViewCells);
  CHECK(world.session().homunculus_teleport_hold_ms ==
        SessionSettings::kDefaultHomunculusTeleportHoldMs);
  CHECK(world.session().vending_tax_permyriad == SessionSettings::kDefaultVendingTaxPermyriad);
  CHECK(world.session().vending_tax_min_zeny == SessionSettings::kDefaultVendingTaxMinZeny);
}

TEST_CASE("Règles du serveur : une longueur fausse rend la trame indécodable", "[net][rules]") {
  ro::sim::World world;
  ZcMoonlightServerRules told;
  told.rules = {Rule(ServerRuleKey::kViewCells, kMoonlightViewCells)};
  std::vector<std::uint8_t> bytes = Encode(told);

  // Un octet de trop : la queue ne tombe plus sur un nombre entier de paires.
  std::vector<std::uint8_t> ragged = bytes;
  ragged.push_back(0);
  ragged[2] = static_cast<std::uint8_t>(ragged.size());
  CHECK(ApplyFrame(FrameOf(ragged), world, kNowMs) == FrameOutcome::kUndecodable);

  // Une longueur annoncée qui contredit la trame.
  std::vector<std::uint8_t> lying = bytes;
  lying[2] = static_cast<std::uint8_t>(lying[2] + 1);
  CHECK(ApplyFrame(FrameOf(lying), world, kNowMs) == FrameOutcome::kUndecodable);

  // Rien n'en a été retenu.
  CHECK(world.session().view_cells == SessionSettings::kDefaultViewCells);
}

TEST_CASE("Règles du serveur : le plafond de la banque se recompose de ses deux moitiés",
          "[net][rules]") {
  // Le plafond du serveur Moonlight, au-delà de ce qu'un `int32` porte, et un
  // plafond dont la moitié basse se lit NÉGATIVE en `int32` : c'est un motif
  // brut, pas un nombre signé.
  constexpr std::int64_t kMoonlightBankZenyMax = 9'999'999'999'999'999;
  constexpr std::int64_t kLowHalfSignBitMax = 0x2'8000'0000;
  constexpr unsigned kBitsPerHalf = 32;
  constexpr std::uint64_t kLowHalfMask = 0xFFFF'FFFFU;
  const auto low_of = [](std::int64_t cap) {
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(cap) & kLowHalfMask));
  };
  const auto high_of = [](std::int64_t cap) {
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(cap) >> kBitsPerHalf));
  };
  REQUIRE(low_of(kLowHalfSignBitMax) < 0);

  ro::sim::World world;
  // Sans annonce : le plafond d'un rAthena d'usine.
  CHECK(world.session().bank_zeny_max == SessionSettings::kDefaultBankZenyMax);

  for (const std::int64_t cap : {kMoonlightBankZenyMax, kLowHalfSignBitMax}) {
    // ⚠ L'ORDRE DES DEUX MOITIÉS N'EST PAS FIXÉ : la haute d'abord.
    ZcMoonlightServerRules told;
    told.rules = {Rule(ServerRuleKey::kBankZenyMaxHigh, high_of(cap)),
                  Rule(ServerRuleKey::kViewCells, kMoonlightViewCells),
                  Rule(ServerRuleKey::kBankZenyMaxLow, low_of(cap))};
    REQUIRE(ApplyFrame(FrameOf(Encode(told)), world, kNowMs) == FrameOutcome::kApplied);
    CHECK(world.session().bank_zeny_max == cap);
    CHECK(world.session().view_cells == kMoonlightViewCells);
  }
}

TEST_CASE("Règles du serveur : une moitié seule du plafond de banque s'ignore", "[net][rules]") {
  ro::sim::World world;
  ZcMoonlightServerRules told;
  told.rules = {Rule(ServerRuleKey::kBankZenyMaxLow, 1)};
  REQUIRE(ApplyFrame(FrameOf(Encode(told)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.session().bank_zeny_max == SessionSettings::kDefaultBankZenyMax);

  // Ni un plafond nul, ni un plafond négatif (bit de signe dans la moitié haute).
  told.rules = {Rule(ServerRuleKey::kBankZenyMaxLow, 0), Rule(ServerRuleKey::kBankZenyMaxHigh, 0)};
  REQUIRE(ApplyFrame(FrameOf(Encode(told)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.session().bank_zeny_max == SessionSettings::kDefaultBankZenyMax);
  told.rules = {Rule(ServerRuleKey::kBankZenyMaxLow, 0), Rule(ServerRuleKey::kBankZenyMaxHigh, -1)};
  REQUIRE(ApplyFrame(FrameOf(Encode(told)), world, kNowMs) == FrameOutcome::kApplied);
  CHECK(world.session().bank_zeny_max == SessionSettings::kDefaultBankZenyMax);
}
