// Les règles du serveur. Voir l'en-tête.

#include "ro/net/server_rules.h"

#include <algorithm>
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

// Une valeur qui n'a de sens que strictement positive ; une autre est ignorée.
void KeepPositive(int& field, std::int32_t value) {
  if (value > 0) {
    field = value;
  }
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
    case ServerRuleKey::kStyleLayersCadenceMs:
      if (rule.value > 0) {
        session.style_layers_cadence_ms = static_cast<std::uint32_t>(rule.value);
      }
      return;
    case ServerRuleKey::kStyleBodyVariantsMax:
      KeepPositive(session.style_body_variants_max, rule.value);
      return;
    case ServerRuleKey::kStyleLayersMax:
      KeepPositive(session.style_layers_max, rule.value);
      return;
    case ServerRuleKey::kHairStyleMin:
      KeepPositive(session.hair_style_min, rule.value);
      return;
    case ServerRuleKey::kHairStyleMax:
      KeepPositive(session.hair_style_max, rule.value);
      return;
    case ServerRuleKey::kHairColorMin:
      KeepPositive(session.hair_color_min, rule.value);
      return;
    case ServerRuleKey::kHairColorMax:
      KeepPositive(session.hair_color_max, rule.value);
      return;
    case ServerRuleKey::kClothColorMin:
      KeepPositive(session.cloth_color_min, rule.value);
      return;
    case ServerRuleKey::kClothColorMax:
      KeepPositive(session.cloth_color_max, rule.value);
      return;
    case ServerRuleKey::kStyleLayersBytesPerMinute:
      KeepPositive(session.style_layers_bytes_per_minute, rule.value);
      return;
    case ServerRuleKey::kStyleAtelierCells:
      // ⚠ ZÉRO SE GARDE : il dit que l'atelier n'est plus exigé.
      session.style_atelier_cells = std::max(0, static_cast<int>(rule.value));
      return;
    case ServerRuleKey::kStyleOutfitCadenceMs:
      if (rule.value > 0) {
        session.style_outfit_cadence_ms = static_cast<std::uint32_t>(rule.value);
      }
      return;
    case ServerRuleKey::kStyleWardrobeSlots:
      KeepPositive(session.style_wardrobe_slots, rule.value);
      return;
    case ServerRuleKey::kStyleLayerRetouchesMost:
      KeepPositive(session.style_layer_retouches_most, rule.value);
      return;
    case ServerRuleKey::kStyleLayerLast:
      KeepPositive(session.style_layer_last, rule.value);
      return;
    case ServerRuleKey::kStyleAbsentCadenceMs:
      if (rule.value > 0) {
        session.style_absent_cadence_ms = static_cast<std::uint32_t>(rule.value);
      }
      return;
    case ServerRuleKey::kStyleAbsentIdsMost:
      KeepPositive(session.style_absent_ids_most, rule.value);
      return;
    case ServerRuleKey::kStyleCharSelect:
      session.style_char_select_served = rule.value > 0;
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
