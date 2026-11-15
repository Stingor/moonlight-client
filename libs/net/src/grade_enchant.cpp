// L'amélioration de grade, sur le fil. Voir l'en-tête.

#include "ro/net/grade_enchant.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

#include "ro/core/log.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

// La case que le serveur envoie quand il ne gère pas le niveau de la pièce.
constexpr std::int16_t kNoPiece = -1;

[[nodiscard]] std::uint32_t Unsigned(std::int32_t value) noexcept {
  return static_cast<std::uint32_t>(std::max(value, 0));
}

[[nodiscard]] sim::GradeEnchantOffer OfferOf(const proto::ZcGradeEnchantMaterialList& packet,
                                             std::uint16_t chosen_slot) {
  sim::GradeEnchantOffer offer;
  // ⚠ SANS PIÈCE, LE SERVEUR DIT « PAS CE NIVEAU » : la réponse se rapporte au choix.
  offer.supported = packet.index != kNoPiece;
  offer.client_slot = offer.supported ? static_cast<std::uint16_t>(packet.index) : chosen_slot;
  offer.chance_percent = Unsigned(packet.success_chance);
  offer.blessing =
      sim::GradeEnchantBlessing{.item_id = Unsigned(packet.blessing_info.id),
                                .per_step = Unsigned(packet.blessing_info.amount),
                                .most_steps = Unsigned(packet.blessing_info.max_blessing),
                                .bonus_percent = Unsigned(packet.blessing_info.bonus)};
  for (const proto::GradeEnchantMaterial& material : packet.material_info) {
    offer.stakes.push_back(sim::GradeEnchantStake{.item_id = Unsigned(material.nameid),
                                                  .amount = Unsigned(material.amount),
                                                  .zeny = std::max(material.price, 0),
                                                  .downgrades = material.downgrade != 0,
                                                  .breaks = material.breakable != 0});
  }
  return offer;
}

// LA RÉUSSITE : la pièce passe au grade dit, et son raffinement revient à zéro.
void Upgrade(sim::Inventory& bag, std::uint16_t slot, std::uint8_t grade) {
  const sim::InventoryItem* item = bag.Find(slot);
  if (item == nullptr) {
    return;
  }
  const sim::InventoryItem kept = *item;
  static_cast<void>(bag.Reforge(slot, 0, grade, kept.damaged, kept.cards, kept.options));
}

}  // namespace

FrameOutcome ApplyGradeEnchantFrame(const Frame& frame, sim::GradeEnchant& grade,
                                    sim::Inventory& bag) {
  switch (frame.opcode) {
    case proto::ZcUiOpen::kOpcode: {
      proto::ZcUiOpen packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      if (packet.UIType != sim::kGradeEnchantUiType) {
        return FrameOutcome::kUnhandled;
      }
      grade.TellOpening();
      return FrameOutcome::kApplied;
    }

    case proto::ZcGradeEnchantMaterialList::kOpcode: {
      proto::ZcGradeEnchantMaterialList packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      grade.TellOffer(OfferOf(packet, grade.chosen_slot().value_or(0)));
      return FrameOutcome::kApplied;
    }

    case proto::ZcGradeEnchantAck::kOpcode: {
      proto::ZcGradeEnchantAck packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      const auto result = static_cast<sim::GradeEnchantResult>(packet.result);
      if (result == sim::GradeEnchantResult::kSucceeded) {
        Upgrade(bag, static_cast<std::uint16_t>(packet.index),
                static_cast<std::uint8_t>(packet.grade));
      } else if (result == sim::GradeEnchantResult::kDowngraded) {
        // Le serveur ne dit pas de combien : le sac reste tel quel jusqu'à la
        // prochaine liste.
        core::LogWarning("grade : raffinement baissé, de combien le serveur ne le dit pas");
      }
      grade.TellResult(result);
      return FrameOutcome::kApplied;
    }

    case proto::ZcNpcackMapmove::kOpcode:
    case proto::ZcNpcackServermove::kOpcode:
      grade.CloseSilently();
      return FrameOutcome::kUnhandled;

    default:
      return FrameOutcome::kUnhandled;
  }
}

void AskGradeEnchantOffer(Connection& link, sim::GradeEnchant& grade) {
  if (const std::optional<std::uint16_t> slot = grade.TakeSelection(); slot.has_value()) {
    link.Send(proto::Encode(
        proto::CzGradeEnchantSelectEquipment{.index = static_cast<std::int16_t>(*slot)}));
  }
}

core::Result<void> RequestGradeEnchant(Connection& link, sim::GradeEnchant& grade,
                                       sim::GradeEnchantAttempt attempt, const sim::Inventory& bag,
                                       std::int64_t zeny) {
  if (!sim::CheckGradeEnchantRequest(grade, attempt, bag, zeny).passed()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "demande de grade que le serveur refuserait");
  }
  const sim::GradeEnchantOffer& offer = *grade.offer();
  const std::uint32_t steps = std::min(attempt.blessing_steps, offer.blessing.most_steps);
  // ⚠ SANS DOSE, PAS DE DRAPEAU : le serveur chercherait une poussière qui n'existe pas.
  const bool blessed = steps > 0 && offer.blessing.item_id != 0;
  link.Send(proto::Encode(proto::CzGradeEnchantRequest{
      .index = static_cast<std::int16_t>(*grade.chosen_slot()),
      .material_index = static_cast<std::int32_t>(attempt.stake),
      .blessing_flag = static_cast<std::int8_t>(blessed ? 1 : 0),
      .blessing_amount = static_cast<std::int32_t>(blessed ? steps : 0),
      .protect_flag = 0}));
  grade.MarkRequestSent();
  return {};
}

void SayGradeEnchantFarewell(Connection& link, sim::GradeEnchant& grade) {
  if (grade.TakeFarewell()) {
    link.Send(proto::Encode(proto::CzGradeEnchantCloseUi{}));
  }
}

}  // namespace ro::net
