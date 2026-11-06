// Le Laphine. Voir l'en-tête pour les deux temps d'une amélioration.

#include "ro/net/laphine.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/proto/item_option_packets.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

[[nodiscard]] FrameOutcome ApplyPieceChange(const Frame& frame, sim::Laphine& laphine,
                                            sim::Inventory& bag) {
  const core::Result<proto::ItemOptionChange> change = proto::DecodeItemOptionChange(frame.bytes);
  if (!change) {
    return FrameOutcome::kUndecodable;
  }
  std::array<sim::ItemOption, sim::kItemOptionSlots> options = {};
  for (std::size_t rank = 0; rank < options.size(); ++rank) {
    options[rank].id = static_cast<std::uint16_t>(change->options[rank].index);
    options[rank].value = change->options[rank].value;
    options[rank].param = change->options[rank].param;
  }
  const auto client_slot = static_cast<std::uint16_t>(change->client_slot);
  if (!bag.Reforge(client_slot, change->refine_level, change->grade, change->damaged, change->cards,
                   options)) {
    return FrameOutcome::kIgnored;
  }
  if (laphine.open() && laphine.kind() == sim::LaphineKind::kUpgrade) {
    laphine.NoteUpgraded(client_slot);
  }
  return FrameOutcome::kApplied;
}

[[nodiscard]] std::vector<std::uint8_t> EncodeRequest(const sim::Laphine& laphine,
                                                      const sim::LaphineRecipe& recipe,
                                                      const sim::Inventory& bag) {
  if (laphine.kind() == sim::LaphineKind::kUpgrade) {
    return proto::Encode(proto::CzReqRandomUpgradeItem{.itemId = laphine.trigger_item_id(),
                                                       .index = laphine.piles().front()});
  }
  proto::CzReqRandomCombineItem request;
  request.itemId = static_cast<std::int32_t>(laphine.trigger_item_id());
  for (const std::uint16_t client_slot : laphine.piles()) {
    const sim::LaphineMaterial* material = recipe.FindMaterial(bag.Find(client_slot)->item_id);
    request.items.push_back(
        proto::CzReqRandomCombineItemSub{.index = static_cast<std::int16_t>(client_slot),
                                         .count = static_cast<std::int16_t>(material->amount)});
  }
  return proto::Encode(request);
}

}  // namespace

FrameOutcome ApplyLaphineFrame(const Frame& frame, sim::Laphine& laphine, sim::Inventory& bag) {
  switch (frame.opcode) {
    case proto::ZcRandomCombineItemUiOpen::kOpcode: {
      proto::ZcRandomCombineItemUiOpen packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      laphine.TellOpening(sim::LaphineKind::kSynthesis, static_cast<std::uint32_t>(packet.itemId));
      return FrameOutcome::kApplied;
    }

    case proto::ZcRandomUpgradeItemUiOpen::kOpcode: {
      proto::ZcRandomUpgradeItemUiOpen packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      laphine.TellOpening(sim::LaphineKind::kUpgrade, packet.itemId);
      return FrameOutcome::kApplied;
    }

    case proto::ZcAckRandomCombineItem::kOpcode: {
      proto::ZcAckRandomCombineItem packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      laphine.TellResult(static_cast<std::uint16_t>(packet.result));
      return FrameOutcome::kApplied;
    }

    case proto::ZcAckRandomUpgradeItem::kOpcode: {
      proto::ZcAckRandomUpgradeItem packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      laphine.TellResult(packet.result);
      return FrameOutcome::kApplied;
    }

    case proto::ItemOptionChange::kOpcode:
      return ApplyPieceChange(frame, laphine, bag);

    case proto::ZcNpcackMapmove::kOpcode:
    case proto::ZcNpcackServermove::kOpcode:
      laphine.CloseSilently();
      return FrameOutcome::kUnhandled;

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestLaphineMake(Connection& link, sim::Laphine& laphine,
                                      const sim::LaphineRecipe& recipe, const sim::Inventory& bag) {
  if (!sim::CheckLaphineRequest(laphine, recipe, bag).passed()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "demande au Laphine que le serveur refuserait");
  }
  const std::vector<std::uint8_t> frame = EncodeRequest(laphine, recipe, bag);
  if (frame.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "demande au Laphine sans trame");
  }
  link.Send(frame);
  laphine.MarkRequestSent();
  return {};
}

void SayLaphineFarewells(Connection& link, sim::Laphine& laphine) {
  for (std::optional<sim::LaphineKind> due = laphine.TakeFarewell(); due.has_value();
       due = laphine.TakeFarewell()) {
    if (*due == sim::LaphineKind::kSynthesis) {
      link.Send(proto::Encode(proto::CzRandomCombineItemUiClose{}));
    } else {
      link.Send(proto::Encode(proto::CzRandomUpgradeItemUiClose{}));
    }
  }
}

}  // namespace ro::net
