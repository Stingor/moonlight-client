// Le troc étendu. Voir l'en-tête pour le pas variable que le générateur manque.

#include "ro/proto/barter_packets.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/proto/packet_lengths.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

}  // namespace

core::Result<ExpandedBarterItemList> DecodeExpandedBarterItemList(
    std::span<const std::uint8_t> frame) {
  const auto declared = PacketLength(ExpandedBarterItemList::kOpcode);
  if (!declared.has_value() || *declared != kVariableLength) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la liste du troc étendu n'est plus déclarée de longueur variable");
  }
  ByteReader reader(frame);
  if (reader.U16() != ExpandedBarterItemList::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet que la liste du troc");
  }
  const std::uint16_t length = reader.U16();
  ExpandedBarterItemList list;
  list.announced_count = reader.I32();
  if (!reader.ok() || length != frame.size()) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "liste du troc étendu tronquée");
  }
  while (reader.remaining() > 0) {
    if (reader.remaining() < ExpandedBarterItem::kFixedWireSize) {
      return core::Fail(ErrorCode::kCorruptData,
                        "un reste de trame plus court qu'un article du troc étendu");
    }
    ExpandedBarterItem item;
    item.item_id = reader.U32();
    item.type = reader.U16();
    item.stock = reader.U32();
    item.weight = reader.U32();
    item.shop_index = reader.U32();
    item.zeny = reader.U32();
    item.view_sprite = reader.U16();
    item.location = reader.U32();
    const std::uint32_t currency_count = reader.U32();
    item.refine_level = reader.U32();
    // ⚠ LE COMPTE SE BORNE PAR CE QUI RESTE AVANT D'ALLOUER : un compte faux
    // demanderait sinon des milliards d'entrées.
    if (currency_count > reader.remaining() / ExpandedBarterCurrency::kWireSize) {
      return core::Fail(ErrorCode::kCorruptData,
                        "un article du troc étendu annonce plus de devises que la trame n'en "
                        "porte");
    }
    item.currencies.resize(currency_count);
    for (ExpandedBarterCurrency& currency : item.currencies) {
      currency.item_id = reader.U32();
      currency.refine_level = reader.U16();
      currency.amount = reader.U32();
      currency.type = reader.U16();
    }
    list.items.push_back(std::move(item));
  }
  if (!reader.ok()) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "liste du troc étendu tronquée");
  }
  return list;
}

std::vector<std::uint8_t> EncodeExpandedBarterItemList(const ExpandedBarterItemList& list) {
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(ExpandedBarterItemList::kOpcode);
  writer.U16(0);
  writer.I32(list.announced_count);
  for (const ExpandedBarterItem& item : list.items) {
    writer.U32(item.item_id);
    writer.U16(item.type);
    writer.U32(item.stock);
    writer.U32(item.weight);
    writer.U32(item.shop_index);
    writer.U32(item.zeny);
    writer.U16(item.view_sprite);
    writer.U32(item.location);
    writer.U32(static_cast<std::uint32_t>(item.currencies.size()));
    writer.U32(item.refine_level);
    for (const ExpandedBarterCurrency& currency : item.currencies) {
      writer.U32(currency.item_id);
      writer.U16(currency.refine_level);
      writer.U32(currency.amount);
      writer.U16(currency.type);
    }
  }
  constexpr std::size_t kLengthOffset = 2;
  static_cast<void>(writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(frame.size())));
  return frame;
}

}  // namespace ro::proto
