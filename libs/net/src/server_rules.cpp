// Les règles du serveur. Voir l'en-tête.

#include "ro/net/server_rules.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/proto/map_packets.h"
#include "ro/sim/session_settings.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// Où la trame porte sa propre longueur : juste après l'opcode.
constexpr std::size_t kLengthOffset = 2;
constexpr unsigned kBitsPerByte = 8;
// La place de la moitié haute d'un `int64` coupé en deux valeurs de règle.
constexpr unsigned kBitsPerHalf = 32;

// LES DEUX MOITIÉS DU PLAFOND DE LA BANQUE, telles que la trame les a dites.
// ⚠ ELLES NE SE COMPOSENT QU'UNE FOIS LA TRAME LUE : leur ordre n'est pas fixé.
struct BankZenyMaxHalves {
  std::optional<std::uint32_t> low;
  std::optional<std::uint32_t> high;
};

void KeepBankZenyMax(sim::SessionSettings& session, const BankZenyMaxHalves& halves) {
  if (!halves.low.has_value() || !halves.high.has_value()) {
    return;
  }
  const auto composed = static_cast<std::int64_t>(
      (static_cast<std::uint64_t>(*halves.high) << kBitsPerHalf) | *halves.low);
  if (composed > 0) {
    session.bank_zeny_max = composed;
  }
}

// La longueur que la trame annonce, petit-boutiste.
[[nodiscard]] std::size_t AnnouncedLength(const Frame& frame) {
  if (frame.bytes.size() < kLengthOffset + 2) {
    return 0;
  }
  return static_cast<std::size_t>(frame.bytes[kLengthOffset]) |
         (static_cast<std::size_t>(frame.bytes[kLengthOffset + 1]) << kBitsPerByte);
}

void Keep(sim::SessionSettings& session, BankZenyMaxHalves& bank_halves,
          const proto::ZcMoonlightServerRulesEntry& rule) {
  switch (static_cast<ServerRuleKey>(rule.key)) {
    case ServerRuleKey::kViewCells:
      if (rule.value > 0) {
        session.view_cells = rule.value;
      }
      return;
    case ServerRuleKey::kHomunculusMaxLevel:
      if (rule.value > 0) {
        session.homunculus_max_level = rule.value;
      }
      return;
    case ServerRuleKey::kHomunculusSMaxLevel:
      if (rule.value > 0) {
        session.homunculus_s_max_level = rule.value;
      }
      return;
    case ServerRuleKey::kHomunculusTeleportHoldMs:
      if (rule.value >= 0) {
        session.homunculus_teleport_hold_ms = static_cast<std::uint32_t>(rule.value);
      }
      return;
    case ServerRuleKey::kVendingTaxPermyriad:
      if (rule.value >= 0 && rule.value <= sim::SessionSettings::kMaxVendingTaxPermyriad) {
        session.vending_tax_permyriad = rule.value;
      }
      return;
    case ServerRuleKey::kVendingTaxMinZeny:
      if (rule.value >= 0) {
        session.vending_tax_min_zeny = rule.value;
      }
      return;
    case ServerRuleKey::kGameAreaCells:
      if (rule.value > 0) {
        session.game_area_cells = rule.value;
      }
      return;
    case ServerRuleKey::kMaxWalkPathSteps:
      if (rule.value > 0) {
        session.max_walk_path_steps = rule.value;
      }
      return;
    case ServerRuleKey::kBankZenyMaxLow:
      bank_halves.low = static_cast<std::uint32_t>(rule.value);
      return;
    case ServerRuleKey::kBankZenyMaxHigh:
      bank_halves.high = static_cast<std::uint32_t>(rule.value);
      return;
  }
  // Une clé que ce client ne connaît pas : le serveur en a ajouté une.
}

}  // namespace

FrameOutcome ApplyServerRulesFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightServerRules::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  if (AnnouncedLength(frame) != frame.bytes.size()) {
    return FrameOutcome::kUndecodable;
  }
  return Decoded<proto::ZcMoonlightServerRules>(frame, [&world](const auto& packet) {
    BankZenyMaxHalves bank_halves;
    for (const proto::ZcMoonlightServerRulesEntry& rule : packet.rules) {
      Keep(world.session(), bank_halves, rule);
    }
    KeepBankZenyMax(world.session(), bank_halves);
    return FrameOutcome::kApplied;
  });
}

}  // namespace ro::net
