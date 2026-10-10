#include "ro/proto/map_packets.h"

// GÉNÉRÉ par tools/packetgen/structgen.py — ne pas modifier à la main.

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;

// Où la longueur d'une trame variable est écrite : juste après l'opcode.
constexpr std::size_t kLengthOffset = 2;

// La plus grande longueur qu'une trame puisse annoncer sur deux octets.
constexpr std::size_t kMaxFrameSize = 0xFFFF;

// Déclarées d'avance : une structure peut en contenir une autre, et
// l'ordre du fichier n'a pas à décider lesquelles.
//
// ⚠ [[maybe_unused]] N'EST PAS UNE COMMODITÉ. Ces fonctions sont écrites
// pour TOUTE sous-structure du protocole, et le sens de circulation d'un
// paquet décide seul de celle qui sert : un paquet que le client reçoit
// sans jamais l'émettre n'appelle pas son écriture. Savoir lesquelles
// retirer supposerait de connaître le graphe d'emploi AVANT de les
// écrire. g++ les signale une par une sous -Wunused-function.
[[maybe_unused]] void ReadEquipslotinfo(ByteReader& reader, Equipslotinfo& value);
[[maybe_unused]] void WriteEquipslotinfo(ByteWriter& writer, const Equipslotinfo& value);
[[maybe_unused]] void ReadNormalitemInfo(ByteReader& reader, NormalitemInfo& value);
[[maybe_unused]] void WriteNormalitemInfo(ByteWriter& writer, const NormalitemInfo& value);
[[maybe_unused]] void ReadItemoptions(ByteReader& reader, Itemoptions& value);
[[maybe_unused]] void WriteItemoptions(ByteWriter& writer, const Itemoptions& value);
[[maybe_unused]] void ReadEquipitemInfo(ByteReader& reader, EquipitemInfo& value);
[[maybe_unused]] void WriteEquipitemInfo(ByteWriter& writer, const EquipitemInfo& value);
[[maybe_unused]] void ReadCzNpcMarketPurchaseSub(ByteReader& reader, CzNpcMarketPurchaseSub& value);
[[maybe_unused]] void WriteCzNpcMarketPurchaseSub(ByteWriter& writer,
                                                  const CzNpcMarketPurchaseSub& value);
[[maybe_unused]] void ReadZcNpcMarketOpenSub(ByteReader& reader, ZcNpcMarketOpenSub& value);
[[maybe_unused]] void WriteZcNpcMarketOpenSub(ByteWriter& writer, const ZcNpcMarketOpenSub& value);
[[maybe_unused]] void ReadHotkeyData(ByteReader& reader, HotkeyData& value);
[[maybe_unused]] void WriteHotkeyData(ByteWriter& writer, const HotkeyData& value);
[[maybe_unused]] void ReadPacketMissionInfoSub(ByteReader& reader, PacketMissionInfoSub& value);
[[maybe_unused]] void WritePacketMissionInfoSub(ByteWriter& writer,
                                                const PacketMissionInfoSub& value);
[[maybe_unused]] void ReadZcGroupListSub(ByteReader& reader, ZcGroupListSub& value);
[[maybe_unused]] void WriteZcGroupListSub(ByteWriter& writer, const ZcGroupListSub& value);
[[maybe_unused]] void ReadPacketQuestHuntSub(ByteReader& reader, PacketQuestHuntSub& value);
[[maybe_unused]] void WritePacketQuestHuntSub(ByteWriter& writer, const PacketQuestHuntSub& value);
[[maybe_unused]] void ReadPacketQuestUpdateHunt(ByteReader& reader, PacketQuestUpdateHunt& value);
[[maybe_unused]] void WritePacketQuestUpdateHunt(ByteWriter& writer,
                                                 const PacketQuestUpdateHunt& value);
[[maybe_unused]] void ReadPacketQuestHuntInfoSub(ByteReader& reader, PacketQuestHuntInfoSub& value);
[[maybe_unused]] void WritePacketQuestHuntInfoSub(ByteWriter& writer,
                                                  const PacketQuestHuntInfoSub& value);
[[maybe_unused]] void ReadRepairitemInfo2(ByteReader& reader, RepairitemInfo2& value);
[[maybe_unused]] void WriteRepairitemInfo2(ByteWriter& writer, const RepairitemInfo2& value);
[[maybe_unused]] void ReadRepairitemInfo1(ByteReader& reader, RepairitemInfo1& value);
[[maybe_unused]] void WriteRepairitemInfo1(ByteWriter& writer, const RepairitemInfo1& value);
[[maybe_unused]] void ReadZcAckSchedulerCashitemSub(ByteReader& reader,
                                                    ZcAckSchedulerCashitemSub& value);
[[maybe_unused]] void WriteZcAckSchedulerCashitemSub(ByteWriter& writer,
                                                     const ZcAckSchedulerCashitemSub& value);
[[maybe_unused]] void ReadZcPcPurchaseMyitemlistSub(ByteReader& reader,
                                                    ZcPcPurchaseMyitemlistSub& value);
[[maybe_unused]] void WriteZcPcPurchaseMyitemlistSub(ByteWriter& writer,
                                                     const ZcPcPurchaseMyitemlistSub& value);
[[maybe_unused]] void ReadZcPcPurchaseItemlistSub(ByteReader& reader,
                                                  ZcPcPurchaseItemlistSub& value);
[[maybe_unused]] void WriteZcPcPurchaseItemlistSub(ByteWriter& writer,
                                                   const ZcPcPurchaseItemlistSub& value);
[[maybe_unused]] void ReadCzPcPurchaseItemlistSub(ByteReader& reader,
                                                  CzPcPurchaseItemlistSub& value);
[[maybe_unused]] void WriteCzPcPurchaseItemlistSub(ByteWriter& writer,
                                                   const CzPcPurchaseItemlistSub& value);
[[maybe_unused]] void ReadCzReqOpenBuyingStoreSub(ByteReader& reader,
                                                  CzReqOpenBuyingStoreSub& value);
[[maybe_unused]] void WriteCzReqOpenBuyingStoreSub(ByteWriter& writer,
                                                   const CzReqOpenBuyingStoreSub& value);
[[maybe_unused]] void ReadZcMyitemlistBuyingStoreSub(ByteReader& reader,
                                                     ZcMyitemlistBuyingStoreSub& value);
[[maybe_unused]] void WriteZcMyitemlistBuyingStoreSub(ByteWriter& writer,
                                                      const ZcMyitemlistBuyingStoreSub& value);
[[maybe_unused]] void ReadZcPcPurchaseItemlistFrommcSub(ByteReader& reader,
                                                        ZcPcPurchaseItemlistFrommcSub& value);
[[maybe_unused]] void WriteZcPcPurchaseItemlistFrommcSub(
    ByteWriter& writer, const ZcPcPurchaseItemlistFrommcSub& value);
[[maybe_unused]] void ReadZcAckItemlistBuyingStoreSub(ByteReader& reader,
                                                      ZcAckItemlistBuyingStoreSub& value);
[[maybe_unused]] void WriteZcAckItemlistBuyingStoreSub(ByteWriter& writer,
                                                       const ZcAckItemlistBuyingStoreSub& value);
[[maybe_unused]] void ReadCzReqTradeBuyingStoreSub(ByteReader& reader,
                                                   CzReqTradeBuyingStoreSub& value);
[[maybe_unused]] void WriteCzReqTradeBuyingStoreSub(ByteWriter& writer,
                                                    const CzReqTradeBuyingStoreSub& value);
[[maybe_unused]] void ReadZcMakableitemlistSub(ByteReader& reader, ZcMakableitemlistSub& value);
[[maybe_unused]] void WriteZcMakableitemlistSub(ByteWriter& writer,
                                                const ZcMakableitemlistSub& value);
[[maybe_unused]] void ReadZcMakingarrowListSub(ByteReader& reader, ZcMakingarrowListSub& value);
[[maybe_unused]] void WriteZcMakingarrowListSub(ByteWriter& writer,
                                                const ZcMakingarrowListSub& value);
[[maybe_unused]] void ReadZcNotifyWeaponitemlistSub(ByteReader& reader,
                                                    ZcNotifyWeaponitemlistSub& value);
[[maybe_unused]] void WriteZcNotifyWeaponitemlistSub(ByteWriter& writer,
                                                     const ZcNotifyWeaponitemlistSub& value);
[[maybe_unused]] void ReadZcMakingitemListSub(ByteReader& reader, ZcMakingitemListSub& value);
[[maybe_unused]] void WriteZcMakingitemListSub(ByteWriter& writer,
                                               const ZcMakingitemListSub& value);
[[maybe_unused]] void ReadZcPcCashPointItemlistSub(ByteReader& reader,
                                                   ZcPcCashPointItemlistSub& value);
[[maybe_unused]] void WriteZcPcCashPointItemlistSub(ByteWriter& writer,
                                                    const ZcPcCashPointItemlistSub& value);
[[maybe_unused]] void ReadCzPcBuyCashPointItemSub(ByteReader& reader,
                                                  CzPcBuyCashPointItemSub& value);
[[maybe_unused]] void WriteCzPcBuyCashPointItemSub(ByteWriter& writer,
                                                   const CzPcBuyCashPointItemSub& value);
[[maybe_unused]] void ReadCzSearchStoreInfoItem(ByteReader& reader, CzSearchStoreInfoItem& value);
[[maybe_unused]] void WriteCzSearchStoreInfoItem(ByteWriter& writer,
                                                 const CzSearchStoreInfoItem& value);
[[maybe_unused]] void ReadZcSearchStoreInfoAckSub(ByteReader& reader,
                                                  ZcSearchStoreInfoAckSub& value);
[[maybe_unused]] void WriteZcSearchStoreInfoAckSub(ByteWriter& writer,
                                                   const ZcSearchStoreInfoAckSub& value);
[[maybe_unused]] void ReadAchListInfo(ByteReader& reader, AchListInfo& value);
[[maybe_unused]] void WriteAchListInfo(ByteWriter& writer, const AchListInfo& value);
[[maybe_unused]] void ReadZcWarplistSub(ByteReader& reader, ZcWarplistSub& value);
[[maybe_unused]] void WriteZcWarplistSub(ByteWriter& writer, const ZcWarplistSub& value);
[[maybe_unused]] void ReadZcNpcBarterMarketIteminfoSub(ByteReader& reader,
                                                       ZcNpcBarterMarketIteminfoSub& value);
[[maybe_unused]] void WriteZcNpcBarterMarketIteminfoSub(ByteWriter& writer,
                                                        const ZcNpcBarterMarketIteminfoSub& value);
[[maybe_unused]] void ReadCzNpcBarterMarketPurchaseSub(ByteReader& reader,
                                                       CzNpcBarterMarketPurchaseSub& value);
[[maybe_unused]] void WriteCzNpcBarterMarketPurchaseSub(ByteWriter& writer,
                                                        const CzNpcBarterMarketPurchaseSub& value);
[[maybe_unused]] void ReadZcBanListSub(ByteReader& reader, ZcBanListSub& value);
[[maybe_unused]] void WriteZcBanListSub(ByteWriter& writer, const ZcBanListSub& value);
[[maybe_unused]] void ReadZcMergeItemOpenSub(ByteReader& reader, ZcMergeItemOpenSub& value);
[[maybe_unused]] void WriteZcMergeItemOpenSub(ByteWriter& writer, const ZcMergeItemOpenSub& value);
[[maybe_unused]] void ReadZcRefiningMaterialListSub(ByteReader& reader,
                                                    ZcRefiningMaterialListSub& value);
[[maybe_unused]] void WriteZcRefiningMaterialListSub(ByteWriter& writer,
                                                     const ZcRefiningMaterialListSub& value);
[[maybe_unused]] void ReadSkilldata(ByteReader& reader, Skilldata& value);
[[maybe_unused]] void WriteSkilldata(ByteWriter& writer, const Skilldata& value);
[[maybe_unused]] void ReadZcNpcMarketPurchaseResultSub(ByteReader& reader,
                                                       ZcNpcMarketPurchaseResultSub& value);
[[maybe_unused]] void WriteZcNpcMarketPurchaseResultSub(ByteWriter& writer,
                                                        const ZcNpcMarketPurchaseResultSub& value);
[[maybe_unused]] void ReadCzReqRandomCombineItemSub(ByteReader& reader,
                                                    CzReqRandomCombineItemSub& value);
[[maybe_unused]] void WriteCzReqRandomCombineItemSub(ByteWriter& writer,
                                                     const CzReqRandomCombineItemSub& value);
[[maybe_unused]] void ReadCzNpcExpandedBarterMarketPurchaseSub(
    ByteReader& reader, CzNpcExpandedBarterMarketPurchaseSub& value);
[[maybe_unused]] void WriteCzNpcExpandedBarterMarketPurchaseSub(
    ByteWriter& writer, const CzNpcExpandedBarterMarketPurchaseSub& value);
[[maybe_unused]] void ReadZcPersonalInfomationSub(ByteReader& reader,
                                                  ZcPersonalInfomationSub& value);
[[maybe_unused]] void WriteZcPersonalInfomationSub(ByteWriter& writer,
                                                   const ZcPersonalInfomationSub& value);
[[maybe_unused]] void ReadGuildMemberInfo(ByteReader& reader, GuildMemberInfo& value);
[[maybe_unused]] void WriteGuildMemberInfo(ByteWriter& writer, const GuildMemberInfo& value);
[[maybe_unused]] void ReadZcPositionInfoSub(ByteReader& reader, ZcPositionInfoSub& value);
[[maybe_unused]] void WriteZcPositionInfoSub(ByteWriter& writer, const ZcPositionInfoSub& value);
[[maybe_unused]] void ReadGuildSkilldata(ByteReader& reader, GuildSkilldata& value);
[[maybe_unused]] void WriteGuildSkilldata(ByteWriter& writer, const GuildSkilldata& value);
[[maybe_unused]] void ReadRelatedGuildInfo(ByteReader& reader, RelatedGuildInfo& value);
[[maybe_unused]] void WriteRelatedGuildInfo(ByteWriter& writer, const RelatedGuildInfo& value);
[[maybe_unused]] void ReadGradeEnchantBlessing(ByteReader& reader, GradeEnchantBlessing& value);
[[maybe_unused]] void WriteGradeEnchantBlessing(ByteWriter& writer,
                                                const GradeEnchantBlessing& value);
[[maybe_unused]] void ReadGradeEnchantMaterial(ByteReader& reader, GradeEnchantMaterial& value);
[[maybe_unused]] void WriteGradeEnchantMaterial(ByteWriter& writer,
                                                const GradeEnchantMaterial& value);
[[maybe_unused]] void ReadCzPurchaseItemFrommc(ByteReader& reader, CzPurchaseItemFrommc& value);
[[maybe_unused]] void WriteCzPurchaseItemFrommc(ByteWriter& writer,
                                                const CzPurchaseItemFrommc& value);
[[maybe_unused]] void ReadZcMoonlightServerRulesEntry(ByteReader& reader,
                                                      ZcMoonlightServerRulesEntry& value);
[[maybe_unused]] void WriteZcMoonlightServerRulesEntry(ByteWriter& writer,
                                                       const ZcMoonlightServerRulesEntry& value);
[[maybe_unused]] void ReadCzSePcBuyCashitemListSub(ByteReader& reader,
                                                   CzSePcBuyCashitemListSub& value);
[[maybe_unused]] void WriteCzSePcBuyCashitemListSub(ByteWriter& writer,
                                                    const CzSePcBuyCashitemListSub& value);
[[maybe_unused]] void ReadZcAckGuildstorageLogSub(ByteReader& reader,
                                                  ZcAckGuildstorageLogSub& value);
[[maybe_unused]] void WriteZcAckGuildstorageLogSub(ByteWriter& writer,
                                                   const ZcAckGuildstorageLogSub& value);
[[maybe_unused]] void ReadZcReputeInfoSub(ByteReader& reader, ZcReputeInfoSub& value);
[[maybe_unused]] void WriteZcReputeInfoSub(ByteWriter& writer, const ZcReputeInfoSub& value);
[[maybe_unused]] void ReadZcFriendsListSub(ByteReader& reader, ZcFriendsListSub& value);
[[maybe_unused]] void WriteZcFriendsListSub(ByteWriter& writer, const ZcFriendsListSub& value);
[[maybe_unused]] void ReadCzPcSellItemlistSub(ByteReader& reader, CzPcSellItemlistSub& value);
[[maybe_unused]] void WriteCzPcSellItemlistSub(ByteWriter& writer,
                                               const CzPcSellItemlistSub& value);
[[maybe_unused]] void ReadCzReqChangeMemberposSub(ByteReader& reader,
                                                  CzReqChangeMemberposSub& value);
[[maybe_unused]] void WriteCzReqChangeMemberposSub(ByteWriter& writer,
                                                   const CzReqChangeMemberposSub& value);
[[maybe_unused]] void ReadZcAckSeCashItemList2Sub(ByteReader& reader,
                                                  ZcAckSeCashItemList2Sub& value);
[[maybe_unused]] void WriteZcAckSeCashItemList2Sub(ByteWriter& writer,
                                                   const ZcAckSeCashItemList2Sub& value);
[[maybe_unused]] void ReadZcHoskillinfoListSub(ByteReader& reader, ZcHoskillinfoListSub& value);
[[maybe_unused]] void WriteZcHoskillinfoListSub(ByteWriter& writer,
                                                const ZcHoskillinfoListSub& value);
[[maybe_unused]] void ReadZcPcSellItemlistSub(ByteReader& reader, ZcPcSellItemlistSub& value);
[[maybe_unused]] void WriteZcPcSellItemlistSub(ByteWriter& writer,
                                               const ZcPcSellItemlistSub& value);
[[maybe_unused]] void ReadZcAckChangeGuildPositioninfoSub(ByteReader& reader,
                                                          ZcAckChangeGuildPositioninfoSub& value);
[[maybe_unused]] void WriteZcAckChangeGuildPositioninfoSub(
    ByteWriter& writer, const ZcAckChangeGuildPositioninfoSub& value);
[[maybe_unused]] void ReadZcAckReqChangeMembersSub(ByteReader& reader,
                                                   ZcAckReqChangeMembersSub& value);
[[maybe_unused]] void WriteZcAckReqChangeMembersSub(ByteWriter& writer,
                                                    const ZcAckReqChangeMembersSub& value);
[[maybe_unused]] void ReadZcEnterRoomSub(ByteReader& reader, ZcEnterRoomSub& value);
[[maybe_unused]] void WriteZcEnterRoomSub(ByteWriter& writer, const ZcEnterRoomSub& value);
[[maybe_unused]] void ReadZcWhisperListSub(ByteReader& reader, ZcWhisperListSub& value);
[[maybe_unused]] void WriteZcWhisperListSub(ByteWriter& writer, const ZcWhisperListSub& value);

void ReadEquipslotinfo(ByteReader& reader, Equipslotinfo& value) {
  for (std::uint32_t& element : value.card) {
    element = static_cast<std::uint32_t>(reader.U32());
  }
}

void WriteEquipslotinfo(ByteWriter& writer, const Equipslotinfo& value) {
  for (const std::uint32_t element : value.card) {
    writer.U32(static_cast<std::uint32_t>(element));
  }
}

void ReadNormalitemInfo(ByteReader& reader, NormalitemInfo& value) {
  value.index = static_cast<std::int16_t>(reader.I16());
  value.ITID = static_cast<std::uint32_t>(reader.U32());
  value.type = static_cast<std::uint8_t>(reader.U8());
  value.count = static_cast<std::int16_t>(reader.I16());
  value.WearState = static_cast<std::uint32_t>(reader.U32());
  ReadEquipslotinfo(reader, value.slot);
  value.HireExpireDate = static_cast<std::int32_t>(reader.I32());
  for (std::uint8_t& element : value.Flag) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
}

void WriteNormalitemInfo(ByteWriter& writer, const NormalitemInfo& value) {
  writer.I16(static_cast<std::int16_t>(value.index));
  writer.U32(static_cast<std::uint32_t>(value.ITID));
  writer.U8(static_cast<std::uint8_t>(value.type));
  writer.I16(static_cast<std::int16_t>(value.count));
  writer.U32(static_cast<std::uint32_t>(value.WearState));
  WriteEquipslotinfo(writer, value.slot);
  writer.I32(static_cast<std::int32_t>(value.HireExpireDate));
  for (const std::uint8_t element : value.Flag) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
}

void ReadItemoptions(ByteReader& reader, Itemoptions& value) {
  value.index = static_cast<std::int16_t>(reader.I16());
  value.value = static_cast<std::int16_t>(reader.I16());
  value.param = static_cast<std::uint8_t>(reader.U8());
}

void WriteItemoptions(ByteWriter& writer, const Itemoptions& value) {
  writer.I16(static_cast<std::int16_t>(value.index));
  writer.I16(static_cast<std::int16_t>(value.value));
  writer.U8(static_cast<std::uint8_t>(value.param));
}

void ReadEquipitemInfo(ByteReader& reader, EquipitemInfo& value) {
  value.index = static_cast<std::int16_t>(reader.I16());
  value.ITID = static_cast<std::uint32_t>(reader.U32());
  value.type = static_cast<std::uint8_t>(reader.U8());
  value.location = static_cast<std::uint32_t>(reader.U32());
  value.WearState = static_cast<std::uint32_t>(reader.U32());
  ReadEquipslotinfo(reader, value.slot);
  value.HireExpireDate = static_cast<std::int32_t>(reader.I32());
  value.bindOnEquipType = static_cast<std::uint16_t>(reader.U16());
  value.wItemSpriteNumber = static_cast<std::uint16_t>(reader.U16());
  value.option_count = static_cast<std::uint8_t>(reader.U8());
  for (Itemoptions& element : value.option_data) {
    ReadItemoptions(reader, element);
  }
  value.RefiningLevel = static_cast<std::uint8_t>(reader.U8());
  value.grade = static_cast<std::uint8_t>(reader.U8());
  for (std::uint8_t& element : value.Flag) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
}

void WriteEquipitemInfo(ByteWriter& writer, const EquipitemInfo& value) {
  writer.I16(static_cast<std::int16_t>(value.index));
  writer.U32(static_cast<std::uint32_t>(value.ITID));
  writer.U8(static_cast<std::uint8_t>(value.type));
  writer.U32(static_cast<std::uint32_t>(value.location));
  writer.U32(static_cast<std::uint32_t>(value.WearState));
  WriteEquipslotinfo(writer, value.slot);
  writer.I32(static_cast<std::int32_t>(value.HireExpireDate));
  writer.U16(static_cast<std::uint16_t>(value.bindOnEquipType));
  writer.U16(static_cast<std::uint16_t>(value.wItemSpriteNumber));
  writer.U8(static_cast<std::uint8_t>(value.option_count));
  for (const Itemoptions& element : value.option_data) {
    WriteItemoptions(writer, element);
  }
  writer.U8(static_cast<std::uint8_t>(value.RefiningLevel));
  writer.U8(static_cast<std::uint8_t>(value.grade));
  for (const std::uint8_t element : value.Flag) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
}

void ReadCzNpcMarketPurchaseSub(ByteReader& reader, CzNpcMarketPurchaseSub& value) {
  value.ITID = static_cast<std::uint32_t>(reader.U32());
  value.qty = static_cast<std::int32_t>(reader.I32());
}

void WriteCzNpcMarketPurchaseSub(ByteWriter& writer, const CzNpcMarketPurchaseSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.ITID));
  writer.I32(static_cast<std::int32_t>(value.qty));
}

void ReadZcNpcMarketOpenSub(ByteReader& reader, ZcNpcMarketOpenSub& value) {
  value.nameid = static_cast<std::uint32_t>(reader.U32());
  value.type = static_cast<std::uint8_t>(reader.U8());
  value.price = static_cast<std::uint32_t>(reader.U32());
  value.qty = static_cast<std::uint32_t>(reader.U32());
  value.weight = static_cast<std::uint16_t>(reader.U16());
  value.location = static_cast<std::uint32_t>(reader.U32());
}

void WriteZcNpcMarketOpenSub(ByteWriter& writer, const ZcNpcMarketOpenSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.nameid));
  writer.U8(static_cast<std::uint8_t>(value.type));
  writer.U32(static_cast<std::uint32_t>(value.price));
  writer.U32(static_cast<std::uint32_t>(value.qty));
  writer.U16(static_cast<std::uint16_t>(value.weight));
  writer.U32(static_cast<std::uint32_t>(value.location));
}

void ReadHotkeyData(ByteReader& reader, HotkeyData& value) {
  value.isSkill = static_cast<std::int8_t>(reader.I8());
  value.id = static_cast<std::uint32_t>(reader.U32());
  value.count = static_cast<std::int16_t>(reader.I16());
}

void WriteHotkeyData(ByteWriter& writer, const HotkeyData& value) {
  writer.I8(static_cast<std::int8_t>(value.isSkill));
  writer.U32(static_cast<std::uint32_t>(value.id));
  writer.I16(static_cast<std::int16_t>(value.count));
}

void ReadPacketMissionInfoSub(ByteReader& reader, PacketMissionInfoSub& value) {
  value.huntIdent = static_cast<std::uint32_t>(reader.U32());
  value.huntIdent2 = static_cast<std::uint32_t>(reader.U32());
  value.mobType = static_cast<std::uint32_t>(reader.U32());
  value.mob_id = static_cast<std::uint32_t>(reader.U32());
  value.levelMin = static_cast<std::int16_t>(reader.I16());
  value.levelMax = static_cast<std::int16_t>(reader.I16());
  value.huntCount = static_cast<std::int16_t>(reader.I16());
  value.maxCount = static_cast<std::int16_t>(reader.I16());
  value.mobName = reader.FixedString(24);
}

void WritePacketMissionInfoSub(ByteWriter& writer, const PacketMissionInfoSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.huntIdent));
  writer.U32(static_cast<std::uint32_t>(value.huntIdent2));
  writer.U32(static_cast<std::uint32_t>(value.mobType));
  writer.U32(static_cast<std::uint32_t>(value.mob_id));
  writer.I16(static_cast<std::int16_t>(value.levelMin));
  writer.I16(static_cast<std::int16_t>(value.levelMax));
  writer.I16(static_cast<std::int16_t>(value.huntCount));
  writer.I16(static_cast<std::int16_t>(value.maxCount));
  writer.FixedString(value.mobName, 24);
}

void ReadZcGroupListSub(ByteReader& reader, ZcGroupListSub& value) {
  value.AID = static_cast<std::uint32_t>(reader.U32());
  value.GID = static_cast<std::uint32_t>(reader.U32());
  value.playerName = reader.FixedString(24);
  value.mapName = reader.FixedString(16);
  value.leader = static_cast<std::uint8_t>(reader.U8());
  value.offline = static_cast<std::uint8_t>(reader.U8());
  value.class_ = static_cast<std::int16_t>(reader.I16());
  value.baseLevel = static_cast<std::int16_t>(reader.I16());
}

void WriteZcGroupListSub(ByteWriter& writer, const ZcGroupListSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.AID));
  writer.U32(static_cast<std::uint32_t>(value.GID));
  writer.FixedString(value.playerName, 24);
  writer.FixedString(value.mapName, 16);
  writer.U8(static_cast<std::uint8_t>(value.leader));
  writer.U8(static_cast<std::uint8_t>(value.offline));
  writer.I16(static_cast<std::int16_t>(value.class_));
  writer.I16(static_cast<std::int16_t>(value.baseLevel));
}

void ReadPacketQuestHuntSub(ByteReader& reader, PacketQuestHuntSub& value) {
  value.huntIdent = static_cast<std::uint32_t>(reader.U32());
  value.huntIdent2 = static_cast<std::uint32_t>(reader.U32());
  value.mobType = static_cast<std::uint32_t>(reader.U32());
  value.mob_id = static_cast<std::uint32_t>(reader.U32());
  value.levelMin = static_cast<std::int16_t>(reader.I16());
  value.levelMax = static_cast<std::int16_t>(reader.I16());
  value.huntCount = static_cast<std::int16_t>(reader.I16());
  value.mobName = reader.FixedString(24);
}

void WritePacketQuestHuntSub(ByteWriter& writer, const PacketQuestHuntSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.huntIdent));
  writer.U32(static_cast<std::uint32_t>(value.huntIdent2));
  writer.U32(static_cast<std::uint32_t>(value.mobType));
  writer.U32(static_cast<std::uint32_t>(value.mob_id));
  writer.I16(static_cast<std::int16_t>(value.levelMin));
  writer.I16(static_cast<std::int16_t>(value.levelMax));
  writer.I16(static_cast<std::int16_t>(value.huntCount));
  writer.FixedString(value.mobName, 24);
}

void ReadPacketQuestUpdateHunt(ByteReader& reader, PacketQuestUpdateHunt& value) {
  value.questID = static_cast<std::uint32_t>(reader.U32());
  value.huntIdent = static_cast<std::uint32_t>(reader.U32());
  value.huntIdent2 = static_cast<std::uint32_t>(reader.U32());
  value.maxCount = static_cast<std::int16_t>(reader.I16());
  value.count = static_cast<std::int16_t>(reader.I16());
}

void WritePacketQuestUpdateHunt(ByteWriter& writer, const PacketQuestUpdateHunt& value) {
  writer.U32(static_cast<std::uint32_t>(value.questID));
  writer.U32(static_cast<std::uint32_t>(value.huntIdent));
  writer.U32(static_cast<std::uint32_t>(value.huntIdent2));
  writer.I16(static_cast<std::int16_t>(value.maxCount));
  writer.I16(static_cast<std::int16_t>(value.count));
}

void ReadPacketQuestHuntInfoSub(ByteReader& reader, PacketQuestHuntInfoSub& value) {
  value.questID = static_cast<std::uint32_t>(reader.U32());
  value.mob_id = static_cast<std::uint32_t>(reader.U32());
  value.maxCount = static_cast<std::int16_t>(reader.I16());
  value.count = static_cast<std::int16_t>(reader.I16());
}

void WritePacketQuestHuntInfoSub(ByteWriter& writer, const PacketQuestHuntInfoSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.questID));
  writer.U32(static_cast<std::uint32_t>(value.mob_id));
  writer.I16(static_cast<std::int16_t>(value.maxCount));
  writer.I16(static_cast<std::int16_t>(value.count));
}

void ReadRepairitemInfo2(ByteReader& reader, RepairitemInfo2& value) {
  value.index = static_cast<std::int16_t>(reader.I16());
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  ReadEquipslotinfo(reader, value.slot);
  value.refine = static_cast<std::uint8_t>(reader.U8());
  value.grade = static_cast<std::uint8_t>(reader.U8());
}

void WriteRepairitemInfo2(ByteWriter& writer, const RepairitemInfo2& value) {
  writer.I16(static_cast<std::int16_t>(value.index));
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  WriteEquipslotinfo(writer, value.slot);
  writer.U8(static_cast<std::uint8_t>(value.refine));
  writer.U8(static_cast<std::uint8_t>(value.grade));
}

void ReadRepairitemInfo1(ByteReader& reader, RepairitemInfo1& value) {
  value.index = static_cast<std::int16_t>(reader.I16());
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.refine = static_cast<std::uint8_t>(reader.U8());
  ReadEquipslotinfo(reader, value.slot);
}

void WriteRepairitemInfo1(ByteWriter& writer, const RepairitemInfo1& value) {
  writer.I16(static_cast<std::int16_t>(value.index));
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.U8(static_cast<std::uint8_t>(value.refine));
  WriteEquipslotinfo(writer, value.slot);
}

void ReadZcAckSchedulerCashitemSub(ByteReader& reader, ZcAckSchedulerCashitemSub& value) {
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.price = static_cast<std::uint32_t>(reader.U32());
  value.viewSprite = static_cast<std::uint16_t>(reader.U16());
  value.location = static_cast<std::uint32_t>(reader.U32());
}

void WriteZcAckSchedulerCashitemSub(ByteWriter& writer, const ZcAckSchedulerCashitemSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.U32(static_cast<std::uint32_t>(value.price));
  writer.U16(static_cast<std::uint16_t>(value.viewSprite));
  writer.U32(static_cast<std::uint32_t>(value.location));
}

void ReadZcPcPurchaseMyitemlistSub(ByteReader& reader, ZcPcPurchaseMyitemlistSub& value) {
  value.price = static_cast<std::uint32_t>(reader.U32());
  value.index = static_cast<std::int16_t>(reader.I16());
  value.amount = static_cast<std::int16_t>(reader.I16());
  value.itemType = static_cast<std::uint8_t>(reader.U8());
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.identified = static_cast<std::uint8_t>(reader.U8());
  value.damaged = static_cast<std::uint8_t>(reader.U8());
  ReadEquipslotinfo(reader, value.slot);
  for (Itemoptions& element : value.option_data) {
    ReadItemoptions(reader, element);
  }
  value.refine = static_cast<std::uint8_t>(reader.U8());
  value.grade = static_cast<std::uint8_t>(reader.U8());
}

void WriteZcPcPurchaseMyitemlistSub(ByteWriter& writer, const ZcPcPurchaseMyitemlistSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.price));
  writer.I16(static_cast<std::int16_t>(value.index));
  writer.I16(static_cast<std::int16_t>(value.amount));
  writer.U8(static_cast<std::uint8_t>(value.itemType));
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.U8(static_cast<std::uint8_t>(value.identified));
  writer.U8(static_cast<std::uint8_t>(value.damaged));
  WriteEquipslotinfo(writer, value.slot);
  for (const Itemoptions& element : value.option_data) {
    WriteItemoptions(writer, element);
  }
  writer.U8(static_cast<std::uint8_t>(value.refine));
  writer.U8(static_cast<std::uint8_t>(value.grade));
}

void ReadZcPcPurchaseItemlistSub(ByteReader& reader, ZcPcPurchaseItemlistSub& value) {
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.price = static_cast<std::uint32_t>(reader.U32());
  value.discountPrice = static_cast<std::uint32_t>(reader.U32());
  value.itemType = static_cast<std::uint8_t>(reader.U8());
  value.viewSprite = static_cast<std::uint16_t>(reader.U16());
  value.location = static_cast<std::uint32_t>(reader.U32());
}

void WriteZcPcPurchaseItemlistSub(ByteWriter& writer, const ZcPcPurchaseItemlistSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.U32(static_cast<std::uint32_t>(value.price));
  writer.U32(static_cast<std::uint32_t>(value.discountPrice));
  writer.U8(static_cast<std::uint8_t>(value.itemType));
  writer.U16(static_cast<std::uint16_t>(value.viewSprite));
  writer.U32(static_cast<std::uint32_t>(value.location));
}

void ReadCzPcPurchaseItemlistSub(ByteReader& reader, CzPcPurchaseItemlistSub& value) {
  value.amount = static_cast<std::uint16_t>(reader.U16());
  value.itemId = static_cast<std::uint32_t>(reader.U32());
}

void WriteCzPcPurchaseItemlistSub(ByteWriter& writer, const CzPcPurchaseItemlistSub& value) {
  writer.U16(static_cast<std::uint16_t>(value.amount));
  writer.U32(static_cast<std::uint32_t>(value.itemId));
}

void ReadCzReqOpenBuyingStoreSub(ByteReader& reader, CzReqOpenBuyingStoreSub& value) {
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.amount = static_cast<std::uint16_t>(reader.U16());
  value.price = static_cast<std::uint32_t>(reader.U32());
}

void WriteCzReqOpenBuyingStoreSub(ByteWriter& writer, const CzReqOpenBuyingStoreSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.U16(static_cast<std::uint16_t>(value.amount));
  writer.U32(static_cast<std::uint32_t>(value.price));
}

void ReadZcMyitemlistBuyingStoreSub(ByteReader& reader, ZcMyitemlistBuyingStoreSub& value) {
  value.price = static_cast<std::uint32_t>(reader.U32());
  value.amount = static_cast<std::uint16_t>(reader.U16());
  value.itemType = static_cast<std::uint8_t>(reader.U8());
  value.itemId = static_cast<std::uint32_t>(reader.U32());
}

void WriteZcMyitemlistBuyingStoreSub(ByteWriter& writer, const ZcMyitemlistBuyingStoreSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.price));
  writer.U16(static_cast<std::uint16_t>(value.amount));
  writer.U8(static_cast<std::uint8_t>(value.itemType));
  writer.U32(static_cast<std::uint32_t>(value.itemId));
}

void ReadZcPcPurchaseItemlistFrommcSub(ByteReader& reader, ZcPcPurchaseItemlistFrommcSub& value) {
  value.price = static_cast<std::uint32_t>(reader.U32());
  value.amount = static_cast<std::uint16_t>(reader.U16());
  value.index = static_cast<std::int16_t>(reader.I16());
  value.itemType = static_cast<std::uint8_t>(reader.U8());
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.identified = static_cast<std::uint8_t>(reader.U8());
  value.damaged = static_cast<std::uint8_t>(reader.U8());
  ReadEquipslotinfo(reader, value.slot);
  for (Itemoptions& element : value.option_data) {
    ReadItemoptions(reader, element);
  }
  value.location = static_cast<std::uint32_t>(reader.U32());
  value.viewSprite = static_cast<std::uint16_t>(reader.U16());
  value.refine = static_cast<std::uint8_t>(reader.U8());
  value.grade = static_cast<std::uint8_t>(reader.U8());
}

void WriteZcPcPurchaseItemlistFrommcSub(ByteWriter& writer,
                                        const ZcPcPurchaseItemlistFrommcSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.price));
  writer.U16(static_cast<std::uint16_t>(value.amount));
  writer.I16(static_cast<std::int16_t>(value.index));
  writer.U8(static_cast<std::uint8_t>(value.itemType));
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.U8(static_cast<std::uint8_t>(value.identified));
  writer.U8(static_cast<std::uint8_t>(value.damaged));
  WriteEquipslotinfo(writer, value.slot);
  for (const Itemoptions& element : value.option_data) {
    WriteItemoptions(writer, element);
  }
  writer.U32(static_cast<std::uint32_t>(value.location));
  writer.U16(static_cast<std::uint16_t>(value.viewSprite));
  writer.U8(static_cast<std::uint8_t>(value.refine));
  writer.U8(static_cast<std::uint8_t>(value.grade));
}

void ReadZcAckItemlistBuyingStoreSub(ByteReader& reader, ZcAckItemlistBuyingStoreSub& value) {
  value.price = static_cast<std::uint32_t>(reader.U32());
  value.amount = static_cast<std::uint16_t>(reader.U16());
  value.itemType = static_cast<std::uint8_t>(reader.U8());
  value.itemId = static_cast<std::uint32_t>(reader.U32());
}

void WriteZcAckItemlistBuyingStoreSub(ByteWriter& writer,
                                      const ZcAckItemlistBuyingStoreSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.price));
  writer.U16(static_cast<std::uint16_t>(value.amount));
  writer.U8(static_cast<std::uint8_t>(value.itemType));
  writer.U32(static_cast<std::uint32_t>(value.itemId));
}

void ReadCzReqTradeBuyingStoreSub(ByteReader& reader, CzReqTradeBuyingStoreSub& value) {
  value.index = static_cast<std::int16_t>(reader.I16());
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.amount = static_cast<std::uint16_t>(reader.U16());
}

void WriteCzReqTradeBuyingStoreSub(ByteWriter& writer, const CzReqTradeBuyingStoreSub& value) {
  writer.I16(static_cast<std::int16_t>(value.index));
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.U16(static_cast<std::uint16_t>(value.amount));
}

void ReadZcMakableitemlistSub(ByteReader& reader, ZcMakableitemlistSub& value) {
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  for (std::uint32_t& element : value.material) {
    element = static_cast<std::uint32_t>(reader.U32());
  }
}

void WriteZcMakableitemlistSub(ByteWriter& writer, const ZcMakableitemlistSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  for (const std::uint32_t element : value.material) {
    writer.U32(static_cast<std::uint32_t>(element));
  }
}

void ReadZcMakingarrowListSub(ByteReader& reader, ZcMakingarrowListSub& value) {
  value.itemId = static_cast<std::uint32_t>(reader.U32());
}

void WriteZcMakingarrowListSub(ByteWriter& writer, const ZcMakingarrowListSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.itemId));
}

void ReadZcNotifyWeaponitemlistSub(ByteReader& reader, ZcNotifyWeaponitemlistSub& value) {
  value.index = static_cast<std::int16_t>(reader.I16());
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.refine = static_cast<std::uint8_t>(reader.U8());
  ReadEquipslotinfo(reader, value.slot);
}

void WriteZcNotifyWeaponitemlistSub(ByteWriter& writer, const ZcNotifyWeaponitemlistSub& value) {
  writer.I16(static_cast<std::int16_t>(value.index));
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.U8(static_cast<std::uint8_t>(value.refine));
  WriteEquipslotinfo(writer, value.slot);
}

void ReadZcMakingitemListSub(ByteReader& reader, ZcMakingitemListSub& value) {
  value.itemId = static_cast<std::uint32_t>(reader.U32());
}

void WriteZcMakingitemListSub(ByteWriter& writer, const ZcMakingitemListSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.itemId));
}

void ReadZcPcCashPointItemlistSub(ByteReader& reader, ZcPcCashPointItemlistSub& value) {
  value.price = static_cast<std::uint32_t>(reader.U32());
  value.discountPrice = static_cast<std::uint32_t>(reader.U32());
  value.itemType = static_cast<std::uint8_t>(reader.U8());
  value.itemId = static_cast<std::uint32_t>(reader.U32());
}

void WriteZcPcCashPointItemlistSub(ByteWriter& writer, const ZcPcCashPointItemlistSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.price));
  writer.U32(static_cast<std::uint32_t>(value.discountPrice));
  writer.U8(static_cast<std::uint8_t>(value.itemType));
  writer.U32(static_cast<std::uint32_t>(value.itemId));
}

void ReadCzPcBuyCashPointItemSub(ByteReader& reader, CzPcBuyCashPointItemSub& value) {
  value.amount = static_cast<std::uint16_t>(reader.U16());
  value.itemId = static_cast<std::uint32_t>(reader.U32());
}

void WriteCzPcBuyCashPointItemSub(ByteWriter& writer, const CzPcBuyCashPointItemSub& value) {
  writer.U16(static_cast<std::uint16_t>(value.amount));
  writer.U32(static_cast<std::uint32_t>(value.itemId));
}

void ReadCzSearchStoreInfoItem(ByteReader& reader, CzSearchStoreInfoItem& value) {
  value.itemId = static_cast<std::uint32_t>(reader.U32());
}

void WriteCzSearchStoreInfoItem(ByteWriter& writer, const CzSearchStoreInfoItem& value) {
  writer.U32(static_cast<std::uint32_t>(value.itemId));
}

void ReadZcSearchStoreInfoAckSub(ByteReader& reader, ZcSearchStoreInfoAckSub& value) {
  value.storeId = static_cast<std::uint32_t>(reader.U32());
  value.AID = static_cast<std::uint32_t>(reader.U32());
  value.shopName = reader.FixedString(80);
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.itemType = static_cast<std::uint8_t>(reader.U8());
  value.price = static_cast<std::uint32_t>(reader.U32());
  value.amount = static_cast<std::uint16_t>(reader.U16());
  ReadEquipslotinfo(reader, value.slot);
  for (Itemoptions& element : value.option_data) {
    ReadItemoptions(reader, element);
  }
  value.refine = static_cast<std::uint8_t>(reader.U8());
  value.grade = static_cast<std::uint8_t>(reader.U8());
}

void WriteZcSearchStoreInfoAckSub(ByteWriter& writer, const ZcSearchStoreInfoAckSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.storeId));
  writer.U32(static_cast<std::uint32_t>(value.AID));
  writer.FixedString(value.shopName, 80);
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.U8(static_cast<std::uint8_t>(value.itemType));
  writer.U32(static_cast<std::uint32_t>(value.price));
  writer.U16(static_cast<std::uint16_t>(value.amount));
  WriteEquipslotinfo(writer, value.slot);
  for (const Itemoptions& element : value.option_data) {
    WriteItemoptions(writer, element);
  }
  writer.U8(static_cast<std::uint8_t>(value.refine));
  writer.U8(static_cast<std::uint8_t>(value.grade));
}

void ReadAchListInfo(ByteReader& reader, AchListInfo& value) {
  value.ach_id = static_cast<std::uint32_t>(reader.U32());
  value.completed = static_cast<std::uint8_t>(reader.U8());
  for (std::uint32_t& element : value.objective) {
    element = static_cast<std::uint32_t>(reader.U32());
  }
  value.completed_at = static_cast<std::uint32_t>(reader.U32());
  value.reward = static_cast<std::uint8_t>(reader.U8());
}

void WriteAchListInfo(ByteWriter& writer, const AchListInfo& value) {
  writer.U32(static_cast<std::uint32_t>(value.ach_id));
  writer.U8(static_cast<std::uint8_t>(value.completed));
  for (const std::uint32_t element : value.objective) {
    writer.U32(static_cast<std::uint32_t>(element));
  }
  writer.U32(static_cast<std::uint32_t>(value.completed_at));
  writer.U8(static_cast<std::uint8_t>(value.reward));
}

void ReadZcWarplistSub(ByteReader& reader, ZcWarplistSub& value) {
  value.map = reader.FixedString(16);
}

void WriteZcWarplistSub(ByteWriter& writer, const ZcWarplistSub& value) {
  writer.FixedString(value.map, 16);
}

void ReadZcNpcBarterMarketIteminfoSub(ByteReader& reader, ZcNpcBarterMarketIteminfoSub& value) {
  value.nameid = static_cast<std::uint32_t>(reader.U32());
  value.type = static_cast<std::uint8_t>(reader.U8());
  value.amount = static_cast<std::uint32_t>(reader.U32());
  value.currencyNameid = static_cast<std::uint32_t>(reader.U32());
  value.currencyAmount = static_cast<std::uint32_t>(reader.U32());
  value.weight = static_cast<std::uint32_t>(reader.U32());
  value.index = static_cast<std::uint32_t>(reader.U32());
  value.viewSprite = static_cast<std::uint16_t>(reader.U16());
  value.location = static_cast<std::uint32_t>(reader.U32());
}

void WriteZcNpcBarterMarketIteminfoSub(ByteWriter& writer,
                                       const ZcNpcBarterMarketIteminfoSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.nameid));
  writer.U8(static_cast<std::uint8_t>(value.type));
  writer.U32(static_cast<std::uint32_t>(value.amount));
  writer.U32(static_cast<std::uint32_t>(value.currencyNameid));
  writer.U32(static_cast<std::uint32_t>(value.currencyAmount));
  writer.U32(static_cast<std::uint32_t>(value.weight));
  writer.U32(static_cast<std::uint32_t>(value.index));
  writer.U16(static_cast<std::uint16_t>(value.viewSprite));
  writer.U32(static_cast<std::uint32_t>(value.location));
}

void ReadCzNpcBarterMarketPurchaseSub(ByteReader& reader, CzNpcBarterMarketPurchaseSub& value) {
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.amount = static_cast<std::uint32_t>(reader.U32());
  value.invIndex = static_cast<std::uint16_t>(reader.U16());
  value.shopIndex = static_cast<std::uint32_t>(reader.U32());
}

void WriteCzNpcBarterMarketPurchaseSub(ByteWriter& writer,
                                       const CzNpcBarterMarketPurchaseSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.U32(static_cast<std::uint32_t>(value.amount));
  writer.U16(static_cast<std::uint16_t>(value.invIndex));
  writer.U32(static_cast<std::uint32_t>(value.shopIndex));
}

void ReadZcBanListSub(ByteReader& reader, ZcBanListSub& value) {
  value.char_id = static_cast<std::int32_t>(reader.I32());
  value.message = reader.FixedString(40);
  value.char_name = reader.FixedString(24);
}

void WriteZcBanListSub(ByteWriter& writer, const ZcBanListSub& value) {
  writer.I32(static_cast<std::int32_t>(value.char_id));
  writer.FixedString(value.message, 40);
  writer.FixedString(value.char_name, 24);
}

void ReadZcMergeItemOpenSub(ByteReader& reader, ZcMergeItemOpenSub& value) {
  value.index = static_cast<std::int16_t>(reader.I16());
}

void WriteZcMergeItemOpenSub(ByteWriter& writer, const ZcMergeItemOpenSub& value) {
  writer.I16(static_cast<std::int16_t>(value.index));
}

void ReadZcRefiningMaterialListSub(ByteReader& reader, ZcRefiningMaterialListSub& value) {
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.chance = static_cast<std::int8_t>(reader.I8());
  value.zeny = static_cast<std::int32_t>(reader.I32());
}

void WriteZcRefiningMaterialListSub(ByteWriter& writer, const ZcRefiningMaterialListSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.I8(static_cast<std::int8_t>(value.chance));
  writer.I32(static_cast<std::int32_t>(value.zeny));
}

void ReadSkilldata(ByteReader& reader, Skilldata& value) {
  value.id = static_cast<std::uint16_t>(reader.U16());
  value.inf = static_cast<std::int32_t>(reader.I32());
  value.level = static_cast<std::uint16_t>(reader.U16());
  value.sp = static_cast<std::uint16_t>(reader.U16());
  value.range2 = static_cast<std::uint16_t>(reader.U16());
  value.name = reader.FixedString(24);
  value.upFlag = static_cast<std::uint8_t>(reader.U8());
}

void WriteSkilldata(ByteWriter& writer, const Skilldata& value) {
  writer.U16(static_cast<std::uint16_t>(value.id));
  writer.I32(static_cast<std::int32_t>(value.inf));
  writer.U16(static_cast<std::uint16_t>(value.level));
  writer.U16(static_cast<std::uint16_t>(value.sp));
  writer.U16(static_cast<std::uint16_t>(value.range2));
  writer.FixedString(value.name, 24);
  writer.U8(static_cast<std::uint8_t>(value.upFlag));
}

void ReadZcNpcMarketPurchaseResultSub(ByteReader& reader, ZcNpcMarketPurchaseResultSub& value) {
  value.ITID = static_cast<std::uint32_t>(reader.U32());
  value.qty = static_cast<std::uint16_t>(reader.U16());
  value.price = static_cast<std::uint32_t>(reader.U32());
}

void WriteZcNpcMarketPurchaseResultSub(ByteWriter& writer,
                                       const ZcNpcMarketPurchaseResultSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.ITID));
  writer.U16(static_cast<std::uint16_t>(value.qty));
  writer.U32(static_cast<std::uint32_t>(value.price));
}

void ReadCzReqRandomCombineItemSub(ByteReader& reader, CzReqRandomCombineItemSub& value) {
  value.index = static_cast<std::int16_t>(reader.I16());
  value.count = static_cast<std::int16_t>(reader.I16());
}

void WriteCzReqRandomCombineItemSub(ByteWriter& writer, const CzReqRandomCombineItemSub& value) {
  writer.I16(static_cast<std::int16_t>(value.index));
  writer.I16(static_cast<std::int16_t>(value.count));
}

void ReadCzNpcExpandedBarterMarketPurchaseSub(ByteReader& reader,
                                              CzNpcExpandedBarterMarketPurchaseSub& value) {
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.shopIndex = static_cast<std::uint32_t>(reader.U32());
  value.amount = static_cast<std::uint32_t>(reader.U32());
}

void WriteCzNpcExpandedBarterMarketPurchaseSub(ByteWriter& writer,
                                               const CzNpcExpandedBarterMarketPurchaseSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.U32(static_cast<std::uint32_t>(value.shopIndex));
  writer.U32(static_cast<std::uint32_t>(value.amount));
}

void ReadZcPersonalInfomationSub(ByteReader& reader, ZcPersonalInfomationSub& value) {
  value.type = static_cast<std::int8_t>(reader.I8());
  value.exp = static_cast<std::int32_t>(reader.I32());
  value.death = static_cast<std::int32_t>(reader.I32());
  value.drop = static_cast<std::int32_t>(reader.I32());
}

void WriteZcPersonalInfomationSub(ByteWriter& writer, const ZcPersonalInfomationSub& value) {
  writer.I8(static_cast<std::int8_t>(value.type));
  writer.I32(static_cast<std::int32_t>(value.exp));
  writer.I32(static_cast<std::int32_t>(value.death));
  writer.I32(static_cast<std::int32_t>(value.drop));
}

void ReadGuildMemberInfo(ByteReader& reader, GuildMemberInfo& value) {
  value.AID = static_cast<std::uint32_t>(reader.U32());
  value.GID = static_cast<std::uint32_t>(reader.U32());
  value.head = static_cast<std::int16_t>(reader.I16());
  value.headPalette = static_cast<std::int16_t>(reader.I16());
  value.sex = static_cast<std::int16_t>(reader.I16());
  value.job = static_cast<std::int16_t>(reader.I16());
  value.level = static_cast<std::int16_t>(reader.I16());
  value.contributionExp = static_cast<std::int32_t>(reader.I32());
  value.currentState = static_cast<std::int32_t>(reader.I32());
  value.positionID = static_cast<std::int32_t>(reader.I32());
  value.lastLoginTime = static_cast<std::uint32_t>(reader.U32());
  value.char_name = reader.FixedString(24);
}

void WriteGuildMemberInfo(ByteWriter& writer, const GuildMemberInfo& value) {
  writer.U32(static_cast<std::uint32_t>(value.AID));
  writer.U32(static_cast<std::uint32_t>(value.GID));
  writer.I16(static_cast<std::int16_t>(value.head));
  writer.I16(static_cast<std::int16_t>(value.headPalette));
  writer.I16(static_cast<std::int16_t>(value.sex));
  writer.I16(static_cast<std::int16_t>(value.job));
  writer.I16(static_cast<std::int16_t>(value.level));
  writer.I32(static_cast<std::int32_t>(value.contributionExp));
  writer.I32(static_cast<std::int32_t>(value.currentState));
  writer.I32(static_cast<std::int32_t>(value.positionID));
  writer.U32(static_cast<std::uint32_t>(value.lastLoginTime));
  writer.FixedString(value.char_name, 24);
}

void ReadZcPositionInfoSub(ByteReader& reader, ZcPositionInfoSub& value) {
  value.positionID = static_cast<std::int32_t>(reader.I32());
  value.right = static_cast<std::int32_t>(reader.I32());
  value.ranking = static_cast<std::int32_t>(reader.I32());
  value.payRate = static_cast<std::int32_t>(reader.I32());
}

void WriteZcPositionInfoSub(ByteWriter& writer, const ZcPositionInfoSub& value) {
  writer.I32(static_cast<std::int32_t>(value.positionID));
  writer.I32(static_cast<std::int32_t>(value.right));
  writer.I32(static_cast<std::int32_t>(value.ranking));
  writer.I32(static_cast<std::int32_t>(value.payRate));
}

void ReadGuildSkilldata(ByteReader& reader, GuildSkilldata& value) {
  value.id = static_cast<std::uint16_t>(reader.U16());
  value.inf = static_cast<std::int32_t>(reader.I32());
  value.level = static_cast<std::uint16_t>(reader.U16());
  value.sp = static_cast<std::uint16_t>(reader.U16());
  value.range2 = static_cast<std::uint16_t>(reader.U16());
  value.name = reader.FixedString(24);
  value.upFlag = static_cast<std::uint8_t>(reader.U8());
}

void WriteGuildSkilldata(ByteWriter& writer, const GuildSkilldata& value) {
  writer.U16(static_cast<std::uint16_t>(value.id));
  writer.I32(static_cast<std::int32_t>(value.inf));
  writer.U16(static_cast<std::uint16_t>(value.level));
  writer.U16(static_cast<std::uint16_t>(value.sp));
  writer.U16(static_cast<std::uint16_t>(value.range2));
  writer.FixedString(value.name, 24);
  writer.U8(static_cast<std::uint8_t>(value.upFlag));
}

void ReadRelatedGuildInfo(ByteReader& reader, RelatedGuildInfo& value) {
  value.relation = static_cast<std::int32_t>(reader.I32());
  value.GDID = static_cast<std::int32_t>(reader.I32());
  value.guildname = reader.FixedString(24);
}

void WriteRelatedGuildInfo(ByteWriter& writer, const RelatedGuildInfo& value) {
  writer.I32(static_cast<std::int32_t>(value.relation));
  writer.I32(static_cast<std::int32_t>(value.GDID));
  writer.FixedString(value.guildname, 24);
}

void ReadGradeEnchantBlessing(ByteReader& reader, GradeEnchantBlessing& value) {
  value.id = static_cast<std::int32_t>(reader.I32());
  value.amount = static_cast<std::int32_t>(reader.I32());
  value.max_blessing = static_cast<std::int32_t>(reader.I32());
  value.bonus = static_cast<std::int32_t>(reader.I32());
}

void WriteGradeEnchantBlessing(ByteWriter& writer, const GradeEnchantBlessing& value) {
  writer.I32(static_cast<std::int32_t>(value.id));
  writer.I32(static_cast<std::int32_t>(value.amount));
  writer.I32(static_cast<std::int32_t>(value.max_blessing));
  writer.I32(static_cast<std::int32_t>(value.bonus));
}

void ReadGradeEnchantMaterial(ByteReader& reader, GradeEnchantMaterial& value) {
  value.nameid = static_cast<std::int32_t>(reader.I32());
  value.amount = static_cast<std::int32_t>(reader.I32());
  value.price = static_cast<std::int32_t>(reader.I32());
  value.downgrade = static_cast<std::int32_t>(reader.I32());
  value.breakable = static_cast<std::int8_t>(reader.I8());
}

void WriteGradeEnchantMaterial(ByteWriter& writer, const GradeEnchantMaterial& value) {
  writer.I32(static_cast<std::int32_t>(value.nameid));
  writer.I32(static_cast<std::int32_t>(value.amount));
  writer.I32(static_cast<std::int32_t>(value.price));
  writer.I32(static_cast<std::int32_t>(value.downgrade));
  writer.I8(static_cast<std::int8_t>(value.breakable));
}

void ReadCzPurchaseItemFrommc(ByteReader& reader, CzPurchaseItemFrommc& value) {
  value.count = static_cast<std::int16_t>(reader.I16());
  value.index = static_cast<std::int16_t>(reader.I16());
}

void WriteCzPurchaseItemFrommc(ByteWriter& writer, const CzPurchaseItemFrommc& value) {
  writer.I16(static_cast<std::int16_t>(value.count));
  writer.I16(static_cast<std::int16_t>(value.index));
}

void ReadZcMoonlightServerRulesEntry(ByteReader& reader, ZcMoonlightServerRulesEntry& value) {
  value.key = static_cast<std::uint16_t>(reader.U16());
  value.value = static_cast<std::int32_t>(reader.I32());
}

void WriteZcMoonlightServerRulesEntry(ByteWriter& writer,
                                      const ZcMoonlightServerRulesEntry& value) {
  writer.U16(static_cast<std::uint16_t>(value.key));
  writer.I32(static_cast<std::int32_t>(value.value));
}

void ReadCzSePcBuyCashitemListSub(ByteReader& reader, CzSePcBuyCashitemListSub& value) {
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.amount = static_cast<std::uint32_t>(reader.U32());
  value.tab = static_cast<std::uint16_t>(reader.U16());
}

void WriteCzSePcBuyCashitemListSub(ByteWriter& writer, const CzSePcBuyCashitemListSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.U32(static_cast<std::uint32_t>(value.amount));
  writer.U16(static_cast<std::uint16_t>(value.tab));
}

void ReadZcAckGuildstorageLogSub(ByteReader& reader, ZcAckGuildstorageLogSub& value) {
  value.id = static_cast<std::uint32_t>(reader.U32());
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.amount = static_cast<std::int32_t>(reader.I32());
  value.action = static_cast<std::uint8_t>(reader.U8());
  value.refine = static_cast<std::int32_t>(reader.I32());
  value.uniqueId = static_cast<std::int64_t>(reader.I64());
  value.IsIdentified = static_cast<std::uint8_t>(reader.U8());
  value.itemType = static_cast<std::uint16_t>(reader.U16());
  ReadEquipslotinfo(reader, value.slot);
  value.name = reader.FixedString(24);
  value.time = reader.FixedString(24);
  value.attribute = static_cast<std::uint8_t>(reader.U8());
}

void WriteZcAckGuildstorageLogSub(ByteWriter& writer, const ZcAckGuildstorageLogSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.id));
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.I32(static_cast<std::int32_t>(value.amount));
  writer.U8(static_cast<std::uint8_t>(value.action));
  writer.I32(static_cast<std::int32_t>(value.refine));
  writer.I64(static_cast<std::int64_t>(value.uniqueId));
  writer.U8(static_cast<std::uint8_t>(value.IsIdentified));
  writer.U16(static_cast<std::uint16_t>(value.itemType));
  WriteEquipslotinfo(writer, value.slot);
  writer.FixedString(value.name, 24);
  writer.FixedString(value.time, 24);
  writer.U8(static_cast<std::uint8_t>(value.attribute));
}

void ReadZcReputeInfoSub(ByteReader& reader, ZcReputeInfoSub& value) {
  value.type = static_cast<std::uint64_t>(reader.U64());
  value.points = static_cast<std::int64_t>(reader.I64());
}

void WriteZcReputeInfoSub(ByteWriter& writer, const ZcReputeInfoSub& value) {
  writer.U64(static_cast<std::uint64_t>(value.type));
  writer.I64(static_cast<std::int64_t>(value.points));
}

void ReadZcFriendsListSub(ByteReader& reader, ZcFriendsListSub& value) {
  value.AID = static_cast<std::uint32_t>(reader.U32());
  value.CID = static_cast<std::uint32_t>(reader.U32());
  value.name = reader.FixedString(24);
}

void WriteZcFriendsListSub(ByteWriter& writer, const ZcFriendsListSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.AID));
  writer.U32(static_cast<std::uint32_t>(value.CID));
  writer.FixedString(value.name, 24);
}

void ReadCzPcSellItemlistSub(ByteReader& reader, CzPcSellItemlistSub& value) {
  value.index = static_cast<std::uint16_t>(reader.U16());
  value.amount = static_cast<std::uint16_t>(reader.U16());
}

void WriteCzPcSellItemlistSub(ByteWriter& writer, const CzPcSellItemlistSub& value) {
  writer.U16(static_cast<std::uint16_t>(value.index));
  writer.U16(static_cast<std::uint16_t>(value.amount));
}

void ReadCzReqChangeMemberposSub(ByteReader& reader, CzReqChangeMemberposSub& value) {
  value.AID = static_cast<std::uint32_t>(reader.U32());
  value.CID = static_cast<std::uint32_t>(reader.U32());
  value.position = static_cast<std::int32_t>(reader.I32());
}

void WriteCzReqChangeMemberposSub(ByteWriter& writer, const CzReqChangeMemberposSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.AID));
  writer.U32(static_cast<std::uint32_t>(value.CID));
  writer.I32(static_cast<std::int32_t>(value.position));
}

void ReadZcAckSeCashItemList2Sub(ByteReader& reader, ZcAckSeCashItemList2Sub& value) {
  value.itemId = static_cast<std::uint32_t>(reader.U32());
  value.price = static_cast<std::int32_t>(reader.I32());
}

void WriteZcAckSeCashItemList2Sub(ByteWriter& writer, const ZcAckSeCashItemList2Sub& value) {
  writer.U32(static_cast<std::uint32_t>(value.itemId));
  writer.I32(static_cast<std::int32_t>(value.price));
}

void ReadZcHoskillinfoListSub(ByteReader& reader, ZcHoskillinfoListSub& value) {
  value.id = static_cast<std::uint16_t>(reader.U16());
  value.inf = static_cast<std::uint16_t>(reader.U16());
  value.unknown = static_cast<std::uint16_t>(reader.U16());
  value.level = static_cast<std::uint16_t>(reader.U16());
  value.sp = static_cast<std::uint16_t>(reader.U16());
  value.range = static_cast<std::uint16_t>(reader.U16());
  value.name = reader.FixedString(24);
  value.upgradable = static_cast<std::uint8_t>(reader.U8());
}

void WriteZcHoskillinfoListSub(ByteWriter& writer, const ZcHoskillinfoListSub& value) {
  writer.U16(static_cast<std::uint16_t>(value.id));
  writer.U16(static_cast<std::uint16_t>(value.inf));
  writer.U16(static_cast<std::uint16_t>(value.unknown));
  writer.U16(static_cast<std::uint16_t>(value.level));
  writer.U16(static_cast<std::uint16_t>(value.sp));
  writer.U16(static_cast<std::uint16_t>(value.range));
  writer.FixedString(value.name, 24);
  writer.U8(static_cast<std::uint8_t>(value.upgradable));
}

void ReadZcPcSellItemlistSub(ByteReader& reader, ZcPcSellItemlistSub& value) {
  value.index = static_cast<std::uint16_t>(reader.U16());
  value.price = static_cast<std::uint32_t>(reader.U32());
  value.overcharge = static_cast<std::uint32_t>(reader.U32());
}

void WriteZcPcSellItemlistSub(ByteWriter& writer, const ZcPcSellItemlistSub& value) {
  writer.U16(static_cast<std::uint16_t>(value.index));
  writer.U32(static_cast<std::uint32_t>(value.price));
  writer.U32(static_cast<std::uint32_t>(value.overcharge));
}

void ReadZcAckChangeGuildPositioninfoSub(ByteReader& reader,
                                         ZcAckChangeGuildPositioninfoSub& value) {
  value.positionID = static_cast<std::int32_t>(reader.I32());
  value.mode = static_cast<std::int32_t>(reader.I32());
  value.ranking = static_cast<std::int32_t>(reader.I32());
  value.payRate = static_cast<std::int32_t>(reader.I32());
  value.posName = reader.FixedString(24);
}

void WriteZcAckChangeGuildPositioninfoSub(ByteWriter& writer,
                                          const ZcAckChangeGuildPositioninfoSub& value) {
  writer.I32(static_cast<std::int32_t>(value.positionID));
  writer.I32(static_cast<std::int32_t>(value.mode));
  writer.I32(static_cast<std::int32_t>(value.ranking));
  writer.I32(static_cast<std::int32_t>(value.payRate));
  writer.FixedString(value.posName, 24);
}

void ReadZcAckReqChangeMembersSub(ByteReader& reader, ZcAckReqChangeMembersSub& value) {
  value.accId = static_cast<std::uint32_t>(reader.U32());
  value.charId = static_cast<std::uint32_t>(reader.U32());
  value.positionID = static_cast<std::int32_t>(reader.I32());
}

void WriteZcAckReqChangeMembersSub(ByteWriter& writer, const ZcAckReqChangeMembersSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.accId));
  writer.U32(static_cast<std::uint32_t>(value.charId));
  writer.I32(static_cast<std::int32_t>(value.positionID));
}

void ReadZcEnterRoomSub(ByteReader& reader, ZcEnterRoomSub& value) {
  value.flag = static_cast<std::uint32_t>(reader.U32());
  value.name = reader.FixedString(24);
}

void WriteZcEnterRoomSub(ByteWriter& writer, const ZcEnterRoomSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.flag));
  writer.FixedString(value.name, 24);
}

void ReadZcWhisperListSub(ByteReader& reader, ZcWhisperListSub& value) {
  value.name = reader.FixedString(24);
}

void WriteZcWhisperListSub(ByteWriter& writer, const ZcWhisperListSub& value) {
  writer.FixedString(value.name, 24);
}

}  // namespace

bool Decode(std::span<const std::uint8_t> frame, ZcParChange& packet) {
  if (frame.size() != ZcParChange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcParChange::kOpcode) {
    return false;
  }
  packet.varID = static_cast<std::uint16_t>(reader.U16());
  packet.count = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcParChange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcParChange::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcParChange::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.varID));
  writer.I32(static_cast<std::int32_t>(packet.count));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcLongparChange& packet) {
  if (frame.size() != ZcLongparChange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcLongparChange::kOpcode) {
    return false;
  }
  packet.varID = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcLongparChange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcLongparChange::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcLongparChange::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.varID));
  writer.I32(static_cast<std::int32_t>(packet.amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcStatusChange& packet) {
  if (frame.size() != ZcStatusChange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcStatusChange::kOpcode) {
    return false;
  }
  packet.statusID = static_cast<std::uint16_t>(reader.U16());
  packet.value = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcStatusChange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcStatusChange::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcStatusChange::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.statusID));
  writer.U8(static_cast<std::uint8_t>(packet.value));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyCartitemCountinfo& packet) {
  if (frame.size() != ZcNotifyCartitemCountinfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyCartitemCountinfo::kOpcode) {
    return false;
  }
  packet.curCount = static_cast<std::int16_t>(reader.I16());
  packet.maxCount = static_cast<std::int16_t>(reader.I16());
  packet.curWeight = static_cast<std::int32_t>(reader.I32());
  packet.maxWeight = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyCartitemCountinfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyCartitemCountinfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyCartitemCountinfo::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.curCount));
  writer.I16(static_cast<std::int16_t>(packet.maxCount));
  writer.I32(static_cast<std::int32_t>(packet.curWeight));
  writer.I32(static_cast<std::int32_t>(packet.maxWeight));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAttackRange& packet) {
  if (frame.size() != ZcAttackRange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAttackRange::kOpcode) {
    return false;
  }
  packet.currentAttRange = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAttackRange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAttackRange::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAttackRange::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.currentAttRange));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCouplestatus& packet) {
  if (frame.size() != ZcCouplestatus::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCouplestatus::kOpcode) {
    return false;
  }
  packet.statusType = static_cast<std::uint32_t>(reader.U32());
  packet.defaultStatus = static_cast<std::int32_t>(reader.I32());
  packet.plusStatus = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCouplestatus& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCouplestatus::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCouplestatus::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.statusType));
  writer.I32(static_cast<std::int32_t>(packet.defaultStatus));
  writer.I32(static_cast<std::int32_t>(packet.plusStatus));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcLonglongparChange& packet) {
  if (frame.size() != ZcLonglongparChange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcLonglongparChange::kOpcode) {
    return false;
  }
  packet.varID = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::int64_t>(reader.I64());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcLonglongparChange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcLonglongparChange::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcLonglongparChange::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.varID));
  writer.I64(static_cast<std::int64_t>(packet.amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketAuthok& packet) {
  if (frame.size() != PacketAuthok::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketAuthok::kOpcode) {
    return false;
  }
  packet.startTime = static_cast<std::uint32_t>(reader.U32());
  for (std::uint8_t& element : packet.PosDir) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  packet.xSize = static_cast<std::uint8_t>(reader.U8());
  packet.ySize = static_cast<std::uint8_t>(reader.U8());
  packet.font = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketAuthok& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketAuthok::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketAuthok::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.startTime));
  for (const std::uint8_t element : packet.PosDir) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  writer.U8(static_cast<std::uint8_t>(packet.xSize));
  writer.U8(static_cast<std::uint8_t>(packet.ySize));
  writer.I16(static_cast<std::int16_t>(packet.font));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketMonsterHp& packet) {
  if (frame.size() != PacketMonsterHp::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketMonsterHp::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.HP = static_cast<std::int32_t>(reader.I32());
  packet.MaxHP = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketMonsterHp& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketMonsterHp::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketMonsterHp::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.I32(static_cast<std::int32_t>(packet.HP));
  writer.I32(static_cast<std::int32_t>(packet.MaxHP));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketScNotick& packet) {
  if (frame.size() != PacketScNotick::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketScNotick::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.state = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketScNotick& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketScNotick::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketScNotick::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U8(static_cast<std::uint8_t>(packet.state));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcItemPickupAck& packet) {
  if (frame.size() != ZcItemPickupAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcItemPickupAck::kOpcode) {
    return false;
  }
  packet.Index = static_cast<std::uint16_t>(reader.U16());
  packet.count = static_cast<std::uint16_t>(reader.U16());
  packet.nameid = static_cast<std::uint32_t>(reader.U32());
  packet.IsIdentified = static_cast<std::uint8_t>(reader.U8());
  packet.IsDamaged = static_cast<std::uint8_t>(reader.U8());
  ReadEquipslotinfo(reader, packet.slot);
  packet.location = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  packet.HireExpireDate = static_cast<std::int32_t>(reader.I32());
  packet.bindOnEquipType = static_cast<std::uint16_t>(reader.U16());
  for (Itemoptions& element : packet.option_data) {
    ReadItemoptions(reader, element);
  }
  packet.favorite = static_cast<std::uint8_t>(reader.U8());
  packet.look = static_cast<std::uint16_t>(reader.U16());
  packet.refiningLevel = static_cast<std::uint8_t>(reader.U8());
  packet.grade = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcItemPickupAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcItemPickupAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcItemPickupAck::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.Index));
  writer.U16(static_cast<std::uint16_t>(packet.count));
  writer.U32(static_cast<std::uint32_t>(packet.nameid));
  writer.U8(static_cast<std::uint8_t>(packet.IsIdentified));
  writer.U8(static_cast<std::uint8_t>(packet.IsDamaged));
  WriteEquipslotinfo(writer, packet.slot);
  writer.U32(static_cast<std::uint32_t>(packet.location));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.U8(static_cast<std::uint8_t>(packet.result));
  writer.I32(static_cast<std::int32_t>(packet.HireExpireDate));
  writer.U16(static_cast<std::uint16_t>(packet.bindOnEquipType));
  for (const Itemoptions& element : packet.option_data) {
    WriteItemoptions(writer, element);
  }
  writer.U8(static_cast<std::uint8_t>(packet.favorite));
  writer.U16(static_cast<std::uint16_t>(packet.look));
  writer.U8(static_cast<std::uint8_t>(packet.refiningLevel));
  writer.U8(static_cast<std::uint8_t>(packet.grade));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketDropflooritem& packet) {
  if (frame.size() != PacketDropflooritem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketDropflooritem::kOpcode) {
    return false;
  }
  packet.ITAID = static_cast<std::uint32_t>(reader.U32());
  packet.ITID = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::uint16_t>(reader.U16());
  packet.IsIdentified = static_cast<std::uint8_t>(reader.U8());
  packet.xPos = static_cast<std::int16_t>(reader.I16());
  packet.yPos = static_cast<std::int16_t>(reader.I16());
  packet.subX = static_cast<std::uint8_t>(reader.U8());
  packet.subY = static_cast<std::uint8_t>(reader.U8());
  packet.count = static_cast<std::int16_t>(reader.I16());
  packet.showdropeffect = static_cast<std::int8_t>(reader.I8());
  packet.dropeffectmode = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketDropflooritem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketDropflooritem::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketDropflooritem::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.ITAID));
  writer.U32(static_cast<std::uint32_t>(packet.ITID));
  writer.U16(static_cast<std::uint16_t>(packet.type));
  writer.U8(static_cast<std::uint8_t>(packet.IsIdentified));
  writer.I16(static_cast<std::int16_t>(packet.xPos));
  writer.I16(static_cast<std::int16_t>(packet.yPos));
  writer.U8(static_cast<std::uint8_t>(packet.subX));
  writer.U8(static_cast<std::uint8_t>(packet.subY));
  writer.I16(static_cast<std::int16_t>(packet.count));
  writer.I8(static_cast<std::int8_t>(packet.showdropeffect));
  writer.I16(static_cast<std::int16_t>(packet.dropeffectmode));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketSpawnUnit& packet) {
  if (frame.size() < PacketSpawnUnit::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketSpawnUnit::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.objecttype = static_cast<std::uint8_t>(reader.U8());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.speed = static_cast<std::int16_t>(reader.I16());
  packet.bodyState = static_cast<std::int16_t>(reader.I16());
  packet.healthState = static_cast<std::int16_t>(reader.I16());
  packet.effectState = static_cast<std::int32_t>(reader.I32());
  packet.job = static_cast<std::int16_t>(reader.I16());
  packet.head = static_cast<std::uint16_t>(reader.U16());
  packet.weapon = static_cast<std::uint32_t>(reader.U32());
  packet.shield = static_cast<std::uint32_t>(reader.U32());
  packet.accessory = static_cast<std::uint16_t>(reader.U16());
  packet.accessory2 = static_cast<std::uint16_t>(reader.U16());
  packet.accessory3 = static_cast<std::uint16_t>(reader.U16());
  packet.headpalette = static_cast<std::int16_t>(reader.I16());
  packet.bodypalette = static_cast<std::int16_t>(reader.I16());
  packet.headDir = static_cast<std::int16_t>(reader.I16());
  packet.robe = static_cast<std::uint16_t>(reader.U16());
  packet.GUID = static_cast<std::uint32_t>(reader.U32());
  packet.GEmblemVer = static_cast<std::int16_t>(reader.I16());
  packet.honor = static_cast<std::int16_t>(reader.I16());
  packet.virtue = static_cast<std::int32_t>(reader.I32());
  packet.isPKModeON = static_cast<std::uint8_t>(reader.U8());
  packet.sex = static_cast<std::uint8_t>(reader.U8());
  for (std::uint8_t& element : packet.PosDir) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  packet.xSize = static_cast<std::uint8_t>(reader.U8());
  packet.ySize = static_cast<std::uint8_t>(reader.U8());
  packet.clevel = static_cast<std::int16_t>(reader.I16());
  packet.font = static_cast<std::int16_t>(reader.I16());
  packet.maxHP = static_cast<std::int32_t>(reader.I32());
  packet.HP = static_cast<std::int32_t>(reader.I32());
  packet.isBoss = static_cast<std::uint8_t>(reader.U8());
  packet.body = static_cast<std::int16_t>(reader.I16());
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketSpawnUnit& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketSpawnUnit::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketSpawnUnit::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.objecttype));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.I16(static_cast<std::int16_t>(packet.speed));
  writer.I16(static_cast<std::int16_t>(packet.bodyState));
  writer.I16(static_cast<std::int16_t>(packet.healthState));
  writer.I32(static_cast<std::int32_t>(packet.effectState));
  writer.I16(static_cast<std::int16_t>(packet.job));
  writer.U16(static_cast<std::uint16_t>(packet.head));
  writer.U32(static_cast<std::uint32_t>(packet.weapon));
  writer.U32(static_cast<std::uint32_t>(packet.shield));
  writer.U16(static_cast<std::uint16_t>(packet.accessory));
  writer.U16(static_cast<std::uint16_t>(packet.accessory2));
  writer.U16(static_cast<std::uint16_t>(packet.accessory3));
  writer.I16(static_cast<std::int16_t>(packet.headpalette));
  writer.I16(static_cast<std::int16_t>(packet.bodypalette));
  writer.I16(static_cast<std::int16_t>(packet.headDir));
  writer.U16(static_cast<std::uint16_t>(packet.robe));
  writer.U32(static_cast<std::uint32_t>(packet.GUID));
  writer.I16(static_cast<std::int16_t>(packet.GEmblemVer));
  writer.I16(static_cast<std::int16_t>(packet.honor));
  writer.I32(static_cast<std::int32_t>(packet.virtue));
  writer.U8(static_cast<std::uint8_t>(packet.isPKModeON));
  writer.U8(static_cast<std::uint8_t>(packet.sex));
  for (const std::uint8_t element : packet.PosDir) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  writer.U8(static_cast<std::uint8_t>(packet.xSize));
  writer.U8(static_cast<std::uint8_t>(packet.ySize));
  writer.I16(static_cast<std::int16_t>(packet.clevel));
  writer.I16(static_cast<std::int16_t>(packet.font));
  writer.I32(static_cast<std::int32_t>(packet.maxHP));
  writer.I32(static_cast<std::int32_t>(packet.HP));
  writer.U8(static_cast<std::uint8_t>(packet.isBoss));
  writer.I16(static_cast<std::int16_t>(packet.body));
  writer.FixedString(packet.name, 24);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketUnitWalking& packet) {
  if (frame.size() < PacketUnitWalking::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketUnitWalking::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.objecttype = static_cast<std::uint8_t>(reader.U8());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.speed = static_cast<std::int16_t>(reader.I16());
  packet.bodyState = static_cast<std::int16_t>(reader.I16());
  packet.healthState = static_cast<std::int16_t>(reader.I16());
  packet.effectState = static_cast<std::int32_t>(reader.I32());
  packet.job = static_cast<std::int16_t>(reader.I16());
  packet.head = static_cast<std::uint16_t>(reader.U16());
  packet.weapon = static_cast<std::uint32_t>(reader.U32());
  packet.shield = static_cast<std::uint32_t>(reader.U32());
  packet.accessory = static_cast<std::uint16_t>(reader.U16());
  packet.moveStartTime = static_cast<std::uint32_t>(reader.U32());
  packet.accessory2 = static_cast<std::uint16_t>(reader.U16());
  packet.accessory3 = static_cast<std::uint16_t>(reader.U16());
  packet.headpalette = static_cast<std::int16_t>(reader.I16());
  packet.bodypalette = static_cast<std::int16_t>(reader.I16());
  packet.headDir = static_cast<std::int16_t>(reader.I16());
  packet.robe = static_cast<std::uint16_t>(reader.U16());
  packet.GUID = static_cast<std::uint32_t>(reader.U32());
  packet.GEmblemVer = static_cast<std::int16_t>(reader.I16());
  packet.honor = static_cast<std::int16_t>(reader.I16());
  packet.virtue = static_cast<std::int32_t>(reader.I32());
  packet.isPKModeON = static_cast<std::uint8_t>(reader.U8());
  packet.sex = static_cast<std::uint8_t>(reader.U8());
  for (std::uint8_t& element : packet.MoveData) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  packet.xSize = static_cast<std::uint8_t>(reader.U8());
  packet.ySize = static_cast<std::uint8_t>(reader.U8());
  packet.clevel = static_cast<std::int16_t>(reader.I16());
  packet.font = static_cast<std::int16_t>(reader.I16());
  packet.maxHP = static_cast<std::int32_t>(reader.I32());
  packet.HP = static_cast<std::int32_t>(reader.I32());
  packet.isBoss = static_cast<std::uint8_t>(reader.U8());
  packet.body = static_cast<std::uint16_t>(reader.U16());
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketUnitWalking& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketUnitWalking::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketUnitWalking::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.objecttype));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.I16(static_cast<std::int16_t>(packet.speed));
  writer.I16(static_cast<std::int16_t>(packet.bodyState));
  writer.I16(static_cast<std::int16_t>(packet.healthState));
  writer.I32(static_cast<std::int32_t>(packet.effectState));
  writer.I16(static_cast<std::int16_t>(packet.job));
  writer.U16(static_cast<std::uint16_t>(packet.head));
  writer.U32(static_cast<std::uint32_t>(packet.weapon));
  writer.U32(static_cast<std::uint32_t>(packet.shield));
  writer.U16(static_cast<std::uint16_t>(packet.accessory));
  writer.U32(static_cast<std::uint32_t>(packet.moveStartTime));
  writer.U16(static_cast<std::uint16_t>(packet.accessory2));
  writer.U16(static_cast<std::uint16_t>(packet.accessory3));
  writer.I16(static_cast<std::int16_t>(packet.headpalette));
  writer.I16(static_cast<std::int16_t>(packet.bodypalette));
  writer.I16(static_cast<std::int16_t>(packet.headDir));
  writer.U16(static_cast<std::uint16_t>(packet.robe));
  writer.U32(static_cast<std::uint32_t>(packet.GUID));
  writer.I16(static_cast<std::int16_t>(packet.GEmblemVer));
  writer.I16(static_cast<std::int16_t>(packet.honor));
  writer.I32(static_cast<std::int32_t>(packet.virtue));
  writer.U8(static_cast<std::uint8_t>(packet.isPKModeON));
  writer.U8(static_cast<std::uint8_t>(packet.sex));
  for (const std::uint8_t element : packet.MoveData) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  writer.U8(static_cast<std::uint8_t>(packet.xSize));
  writer.U8(static_cast<std::uint8_t>(packet.ySize));
  writer.I16(static_cast<std::int16_t>(packet.clevel));
  writer.I16(static_cast<std::int16_t>(packet.font));
  writer.I32(static_cast<std::int32_t>(packet.maxHP));
  writer.I32(static_cast<std::int32_t>(packet.HP));
  writer.U8(static_cast<std::uint8_t>(packet.isBoss));
  writer.U16(static_cast<std::uint16_t>(packet.body));
  writer.FixedString(packet.name, 24);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketIdleUnit& packet) {
  if (frame.size() < PacketIdleUnit::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketIdleUnit::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.objecttype = static_cast<std::uint8_t>(reader.U8());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.speed = static_cast<std::int16_t>(reader.I16());
  packet.bodyState = static_cast<std::int16_t>(reader.I16());
  packet.healthState = static_cast<std::int16_t>(reader.I16());
  packet.effectState = static_cast<std::int32_t>(reader.I32());
  packet.job = static_cast<std::int16_t>(reader.I16());
  packet.head = static_cast<std::uint16_t>(reader.U16());
  packet.weapon = static_cast<std::uint32_t>(reader.U32());
  packet.shield = static_cast<std::uint32_t>(reader.U32());
  packet.accessory = static_cast<std::uint16_t>(reader.U16());
  packet.accessory2 = static_cast<std::uint16_t>(reader.U16());
  packet.accessory3 = static_cast<std::uint16_t>(reader.U16());
  packet.headpalette = static_cast<std::int16_t>(reader.I16());
  packet.bodypalette = static_cast<std::int16_t>(reader.I16());
  packet.headDir = static_cast<std::int16_t>(reader.I16());
  packet.robe = static_cast<std::uint16_t>(reader.U16());
  packet.GUID = static_cast<std::uint32_t>(reader.U32());
  packet.GEmblemVer = static_cast<std::int16_t>(reader.I16());
  packet.honor = static_cast<std::int16_t>(reader.I16());
  packet.virtue = static_cast<std::int32_t>(reader.I32());
  packet.isPKModeON = static_cast<std::uint8_t>(reader.U8());
  packet.sex = static_cast<std::uint8_t>(reader.U8());
  for (std::uint8_t& element : packet.PosDir) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  packet.xSize = static_cast<std::uint8_t>(reader.U8());
  packet.ySize = static_cast<std::uint8_t>(reader.U8());
  packet.state = static_cast<std::uint8_t>(reader.U8());
  packet.clevel = static_cast<std::int16_t>(reader.I16());
  packet.font = static_cast<std::int16_t>(reader.I16());
  packet.maxHP = static_cast<std::int32_t>(reader.I32());
  packet.HP = static_cast<std::int32_t>(reader.I32());
  packet.isBoss = static_cast<std::uint8_t>(reader.U8());
  packet.body = static_cast<std::uint16_t>(reader.U16());
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketIdleUnit& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketIdleUnit::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketIdleUnit::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.objecttype));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.I16(static_cast<std::int16_t>(packet.speed));
  writer.I16(static_cast<std::int16_t>(packet.bodyState));
  writer.I16(static_cast<std::int16_t>(packet.healthState));
  writer.I32(static_cast<std::int32_t>(packet.effectState));
  writer.I16(static_cast<std::int16_t>(packet.job));
  writer.U16(static_cast<std::uint16_t>(packet.head));
  writer.U32(static_cast<std::uint32_t>(packet.weapon));
  writer.U32(static_cast<std::uint32_t>(packet.shield));
  writer.U16(static_cast<std::uint16_t>(packet.accessory));
  writer.U16(static_cast<std::uint16_t>(packet.accessory2));
  writer.U16(static_cast<std::uint16_t>(packet.accessory3));
  writer.I16(static_cast<std::int16_t>(packet.headpalette));
  writer.I16(static_cast<std::int16_t>(packet.bodypalette));
  writer.I16(static_cast<std::int16_t>(packet.headDir));
  writer.U16(static_cast<std::uint16_t>(packet.robe));
  writer.U32(static_cast<std::uint32_t>(packet.GUID));
  writer.I16(static_cast<std::int16_t>(packet.GEmblemVer));
  writer.I16(static_cast<std::int16_t>(packet.honor));
  writer.I32(static_cast<std::int32_t>(packet.virtue));
  writer.U8(static_cast<std::uint8_t>(packet.isPKModeON));
  writer.U8(static_cast<std::uint8_t>(packet.sex));
  for (const std::uint8_t element : packet.PosDir) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  writer.U8(static_cast<std::uint8_t>(packet.xSize));
  writer.U8(static_cast<std::uint8_t>(packet.ySize));
  writer.U8(static_cast<std::uint8_t>(packet.state));
  writer.I16(static_cast<std::int16_t>(packet.clevel));
  writer.I16(static_cast<std::int16_t>(packet.font));
  writer.I32(static_cast<std::int32_t>(packet.maxHP));
  writer.I32(static_cast<std::int32_t>(packet.HP));
  writer.U8(static_cast<std::uint8_t>(packet.isBoss));
  writer.U16(static_cast<std::uint16_t>(packet.body));
  writer.FixedString(packet.name, 24);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketStatusChange& packet) {
  if (frame.size() != PacketStatusChange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketStatusChange::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.state = static_cast<std::uint8_t>(reader.U8());
  packet.Total = static_cast<std::uint32_t>(reader.U32());
  packet.Left = static_cast<std::uint32_t>(reader.U32());
  packet.val1 = static_cast<std::int32_t>(reader.I32());
  packet.val2 = static_cast<std::int32_t>(reader.I32());
  packet.val3 = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketStatusChange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketStatusChange::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketStatusChange::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U8(static_cast<std::uint8_t>(packet.state));
  writer.U32(static_cast<std::uint32_t>(packet.Total));
  writer.U32(static_cast<std::uint32_t>(packet.Left));
  writer.I32(static_cast<std::int32_t>(packet.val1));
  writer.I32(static_cast<std::int32_t>(packet.val2));
  writer.I32(static_cast<std::int32_t>(packet.val3));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketStatusChangeEnd& packet) {
  if (frame.size() != PacketStatusChangeEnd::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketStatusChangeEnd::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.state = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketStatusChangeEnd& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketStatusChangeEnd::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketStatusChangeEnd::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U8(static_cast<std::uint8_t>(packet.state));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketStatusChange2& packet) {
  if (frame.size() != PacketStatusChange2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketStatusChange2::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.state = static_cast<std::uint8_t>(reader.U8());
  packet.Left = static_cast<std::uint32_t>(reader.U32());
  packet.val1 = static_cast<std::int32_t>(reader.I32());
  packet.val2 = static_cast<std::int32_t>(reader.I32());
  packet.val3 = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketStatusChange2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketStatusChange2::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketStatusChange2::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U8(static_cast<std::uint8_t>(packet.state));
  writer.U32(static_cast<std::uint32_t>(packet.Left));
  writer.I32(static_cast<std::int32_t>(packet.val1));
  writer.I32(static_cast<std::int32_t>(packet.val2));
  writer.I32(static_cast<std::int32_t>(packet.val3));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketMaptypeproperty2& packet) {
  if (frame.size() != PacketMaptypeproperty2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketMaptypeproperty2::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::int16_t>(reader.I16());
  for (std::uint8_t& element : packet.flag) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketMaptypeproperty2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketMaptypeproperty2::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketMaptypeproperty2::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.type));
  for (const std::uint8_t element : packet.flag) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueAck& packet) {
  if (frame.size() != PacketBgqueueAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketBgqueueAck::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.bg_name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketBgqueueAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketBgqueueAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketBgqueueAck::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.FixedString(packet.bg_name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueNoticeDelete& packet) {
  if (frame.size() != PacketBgqueueNoticeDelete::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketBgqueueNoticeDelete::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.bg_name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketBgqueueNoticeDelete& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketBgqueueNoticeDelete::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketBgqueueNoticeDelete::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.FixedString(packet.bg_name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueRegister& packet) {
  if (frame.size() != PacketBgqueueRegister::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketBgqueueRegister::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::int16_t>(reader.I16());
  packet.bg_name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketBgqueueRegister& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketBgqueueRegister::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketBgqueueRegister::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.type));
  writer.FixedString(packet.bg_name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueUpdateInfo& packet) {
  if (frame.size() != PacketBgqueueUpdateInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketBgqueueUpdateInfo::kOpcode) {
    return false;
  }
  packet.bg_name = reader.FixedString(24);
  packet.position = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketBgqueueUpdateInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketBgqueueUpdateInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketBgqueueUpdateInfo::kOpcode);
  writer.FixedString(packet.bg_name, 24);
  writer.I32(static_cast<std::int32_t>(packet.position));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueCheckstate& packet) {
  if (frame.size() != PacketBgqueueCheckstate::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketBgqueueCheckstate::kOpcode) {
    return false;
  }
  packet.bg_name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketBgqueueCheckstate& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketBgqueueCheckstate::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketBgqueueCheckstate::kOpcode);
  writer.FixedString(packet.bg_name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueRevokeReq& packet) {
  if (frame.size() != PacketBgqueueRevokeReq::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketBgqueueRevokeReq::kOpcode) {
    return false;
  }
  packet.bg_name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketBgqueueRevokeReq& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketBgqueueRevokeReq::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketBgqueueRevokeReq::kOpcode);
  writer.FixedString(packet.bg_name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueBattlebeginAck& packet) {
  if (frame.size() != PacketBgqueueBattlebeginAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketBgqueueBattlebeginAck::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  packet.bg_name = reader.FixedString(24);
  packet.game_name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketBgqueueBattlebeginAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketBgqueueBattlebeginAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketBgqueueBattlebeginAck::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  writer.FixedString(packet.bg_name, 24);
  writer.FixedString(packet.game_name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueNotifyEntry& packet) {
  if (frame.size() != PacketBgqueueNotifyEntry::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketBgqueueNotifyEntry::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  packet.position = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketBgqueueNotifyEntry& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketBgqueueNotifyEntry::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketBgqueueNotifyEntry::kOpcode);
  writer.FixedString(packet.name, 24);
  writer.I32(static_cast<std::int32_t>(packet.position));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueBattlebegins& packet) {
  if (frame.size() != PacketBgqueueBattlebegins::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketBgqueueBattlebegins::kOpcode) {
    return false;
  }
  packet.bg_name = reader.FixedString(24);
  packet.game_name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketBgqueueBattlebegins& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketBgqueueBattlebegins::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketBgqueueBattlebegins::kOpcode);
  writer.FixedString(packet.bg_name, 24);
  writer.FixedString(packet.game_name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketScriptClear& packet) {
  if (frame.size() != PacketScriptClear::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketScriptClear::kOpcode) {
    return false;
  }
  packet.NpcID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketScriptClear& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketScriptClear::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketScriptClear::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.NpcID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcBroadcastingSpecialItemObtainItem& packet) {
  if (frame.size() < ZcBroadcastingSpecialItemObtainItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcBroadcastingSpecialItemObtainItem::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.ItemID = static_cast<std::uint32_t>(reader.U32());
  packet.len = static_cast<std::int8_t>(reader.I8());
  packet.Name = reader.FixedString(24);
  packet.boxItemID_len = static_cast<std::int8_t>(reader.I8());
  packet.BoxItemID = static_cast<std::uint32_t>(reader.U32());
  packet.refineLevel_len = static_cast<std::int8_t>(reader.I8());
  packet.refineLevel = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcBroadcastingSpecialItemObtainItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcBroadcastingSpecialItemObtainItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcBroadcastingSpecialItemObtainItem::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.ItemID));
  writer.I8(static_cast<std::int8_t>(packet.len));
  writer.FixedString(packet.Name, 24);
  writer.I8(static_cast<std::int8_t>(packet.boxItemID_len));
  writer.U32(static_cast<std::uint32_t>(packet.BoxItemID));
  writer.I8(static_cast<std::int8_t>(packet.refineLevel_len));
  writer.U32(static_cast<std::uint32_t>(packet.refineLevel));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketItemDropAnnounce& packet) {
  if (frame.size() < PacketItemDropAnnounce::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketItemDropAnnounce::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.ItemID = static_cast<std::uint32_t>(reader.U32());
  packet.len = static_cast<std::int8_t>(reader.I8());
  packet.Name = reader.FixedString(24);
  packet.monsterNameLen = reader.FixedString(1);
  packet.monsterName = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketItemDropAnnounce& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketItemDropAnnounce::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketItemDropAnnounce::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.ItemID));
  writer.I8(static_cast<std::int8_t>(packet.len));
  writer.FixedString(packet.Name, 24);
  writer.FixedString(packet.monsterNameLen, 1);
  writer.FixedString(packet.monsterName, 24);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketCartAdditemAck& packet) {
  if (frame.size() != PacketCartAdditemAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketCartAdditemAck::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketCartAdditemAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketCartAdditemAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketCartAdditemAck::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketBankingCheck& packet) {
  if (frame.size() != PacketBankingCheck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketBankingCheck::kOpcode) {
    return false;
  }
  packet.Money = static_cast<std::int64_t>(reader.I64());
  packet.Reason = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketBankingCheck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketBankingCheck::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketBankingCheck::kOpcode);
  writer.I64(static_cast<std::int64_t>(packet.Money));
  writer.I16(static_cast<std::int16_t>(packet.Reason));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketBankingDepositReq& packet) {
  if (frame.size() != PacketBankingDepositReq::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.Money = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, PacketBankingWithdrawReq& packet) {
  if (frame.size() != PacketBankingWithdrawReq::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.Money = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, PacketBankingDepositAck& packet) {
  if (frame.size() != PacketBankingDepositAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketBankingDepositAck::kOpcode) {
    return false;
  }
  packet.Reason = static_cast<std::int16_t>(reader.I16());
  packet.Money = static_cast<std::int64_t>(reader.I64());
  packet.Balance = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketBankingDepositAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketBankingDepositAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketBankingDepositAck::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.Reason));
  writer.I64(static_cast<std::int64_t>(packet.Money));
  writer.I32(static_cast<std::int32_t>(packet.Balance));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketBankingWithdrawAck& packet) {
  if (frame.size() != PacketBankingWithdrawAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketBankingWithdrawAck::kOpcode) {
    return false;
  }
  packet.Reason = static_cast<std::int16_t>(reader.I16());
  packet.Money = static_cast<std::int64_t>(reader.I64());
  packet.Balance = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketBankingWithdrawAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketBankingWithdrawAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketBankingWithdrawAck::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.Reason));
  writer.I64(static_cast<std::int64_t>(packet.Money));
  writer.I32(static_cast<std::int32_t>(packet.Balance));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketRouletteOpenAck& packet) {
  if (frame.size() != PacketRouletteOpenAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.Result = static_cast<std::int8_t>(reader.I8());
  packet.Serial = static_cast<std::int32_t>(reader.I32());
  packet.Step = static_cast<std::int8_t>(reader.I8());
  packet.Idx = static_cast<std::int8_t>(reader.I8());
  packet.AdditionItemID = static_cast<std::uint32_t>(reader.U32());
  packet.GoldPoint = static_cast<std::int32_t>(reader.I32());
  packet.SilverPoint = static_cast<std::int32_t>(reader.I32());
  packet.BronzePoint = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, PacketRouletteInfoAck& packet) {
  if (frame.size() < PacketRouletteInfoAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketRouletteInfoAck::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.RouletteSerial = static_cast<std::uint32_t>(reader.U32());
  for (std::uint8_t& element : packet.ItemInfo) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketRouletteInfoAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketRouletteInfoAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketRouletteInfoAck::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.RouletteSerial));
  for (const std::uint8_t element : packet.ItemInfo) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketRouletteCloseAck& packet) {
  if (frame.size() != PacketRouletteCloseAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.Result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, PacketRouletteGenerateAck& packet) {
  if (frame.size() != PacketRouletteGenerateAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketRouletteGenerateAck::kOpcode) {
    return false;
  }
  packet.Result = static_cast<std::uint8_t>(reader.U8());
  packet.Step = static_cast<std::uint16_t>(reader.U16());
  packet.Idx = static_cast<std::uint16_t>(reader.U16());
  packet.AdditionItemID = static_cast<std::uint32_t>(reader.U32());
  packet.RemainGold = static_cast<std::int32_t>(reader.I32());
  packet.RemainSilver = static_cast<std::int32_t>(reader.I32());
  packet.RemainBronze = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketRouletteGenerateAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketRouletteGenerateAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketRouletteGenerateAck::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.Result));
  writer.U16(static_cast<std::uint16_t>(packet.Step));
  writer.U16(static_cast<std::uint16_t>(packet.Idx));
  writer.U32(static_cast<std::uint32_t>(packet.AdditionItemID));
  writer.I32(static_cast<std::int32_t>(packet.RemainGold));
  writer.I32(static_cast<std::int32_t>(packet.RemainSilver));
  writer.I32(static_cast<std::int32_t>(packet.RemainBronze));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketRouletteItemrecvReq& packet) {
  if (frame.size() != PacketRouletteItemrecvReq::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.Condition = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, PacketRouletteItemrecvAck& packet) {
  if (frame.size() != PacketRouletteItemrecvAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketRouletteItemrecvAck::kOpcode) {
    return false;
  }
  packet.Result = static_cast<std::uint8_t>(reader.U8());
  packet.AdditionItemID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketRouletteItemrecvAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketRouletteItemrecvAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketRouletteItemrecvAck::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.Result));
  writer.U32(static_cast<std::uint32_t>(packet.AdditionItemID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketItemlistNormal& packet) {
  if (frame.size() < PacketItemlistNormal::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.invType = static_cast<std::uint8_t>(reader.U8());
  if (reader.remaining() % NormalitemInfo::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / NormalitemInfo::kWireSize);
    for (NormalitemInfo& element : packet.list) {
      ReadNormalitemInfo(reader, element);
    }
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, PacketItemlistEquip& packet) {
  if (frame.size() < PacketItemlistEquip::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.invType = static_cast<std::uint8_t>(reader.U8());
  if (reader.remaining() % EquipitemInfo::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / EquipitemInfo::kWireSize);
    for (EquipitemInfo& element : packet.list) {
      ReadEquipitemInfo(reader, element);
    }
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcStoreItemlistNormal& packet) {
  if (frame.size() < ZcStoreItemlistNormal::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.invType = static_cast<std::uint8_t>(reader.U8());
  if (reader.remaining() % NormalitemInfo::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / NormalitemInfo::kWireSize);
    for (NormalitemInfo& element : packet.list) {
      ReadNormalitemInfo(reader, element);
    }
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcInventoryStart& packet) {
  if (frame.size() < ZcInventoryStart::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcInventoryStart::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.invType = static_cast<std::uint8_t>(reader.U8());
  packet.name = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcInventoryStart& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcInventoryStart::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcInventoryStart::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.invType));
  writer.Text(packet.name);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcInventoryEnd& packet) {
  if (frame.size() != ZcInventoryEnd::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcInventoryEnd::kOpcode) {
    return false;
  }
  packet.invType = static_cast<std::uint8_t>(reader.U8());
  packet.flag = reader.FixedString(1);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcInventoryEnd& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcInventoryEnd::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcInventoryEnd::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.invType));
  writer.FixedString(packet.flag, 1);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcStoreItemlistEquip& packet) {
  if (frame.size() < ZcStoreItemlistEquip::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.invType = static_cast<std::uint8_t>(reader.U8());
  if (reader.remaining() % EquipitemInfo::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / EquipitemInfo::kWireSize);
    for (EquipitemInfo& element : packet.list) {
      ReadEquipitemInfo(reader, element);
    }
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, PacketEquipItem& packet) {
  if (frame.size() != PacketEquipItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketEquipItem::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.wearLocation = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketEquipItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketEquipItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketEquipItem::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.wearLocation));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcReqWearEquipAck& packet) {
  if (frame.size() != ZcReqWearEquipAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcReqWearEquipAck::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.wearLocation = static_cast<std::uint32_t>(reader.U32());
  packet.wItemSpriteNumber = static_cast<std::uint16_t>(reader.U16());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcReqWearEquipAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcReqWearEquipAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcReqWearEquipAck::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.wearLocation));
  writer.U16(static_cast<std::uint16_t>(packet.wItemSpriteNumber));
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketUnequipitemAck& packet) {
  if (frame.size() != PacketUnequipitemAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketUnequipitemAck::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.wearLocation = static_cast<std::uint32_t>(reader.U32());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketUnequipitemAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketUnequipitemAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketUnequipitemAck::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.wearLocation));
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcEquipwinMicroscope& packet) {
  if (frame.size() < ZcEquipwinMicroscope::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcEquipwinMicroscope::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.characterName = reader.FixedString(24);
  packet.job = static_cast<std::int16_t>(reader.I16());
  packet.head = static_cast<std::int16_t>(reader.I16());
  packet.accessory = static_cast<std::int16_t>(reader.I16());
  packet.accessory2 = static_cast<std::int16_t>(reader.I16());
  packet.accessory3 = static_cast<std::int16_t>(reader.I16());
  packet.robe = static_cast<std::int16_t>(reader.I16());
  packet.headpalette = static_cast<std::int16_t>(reader.I16());
  packet.bodypalette = static_cast<std::int16_t>(reader.I16());
  packet.body2 = static_cast<std::int16_t>(reader.I16());
  packet.sex = static_cast<std::uint8_t>(reader.U8());
  if (reader.remaining() % EquipitemInfo::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / EquipitemInfo::kWireSize);
    for (EquipitemInfo& element : packet.list) {
      ReadEquipitemInfo(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcEquipwinMicroscope& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcEquipwinMicroscope::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcEquipwinMicroscope::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.FixedString(packet.characterName, 24);
  writer.I16(static_cast<std::int16_t>(packet.job));
  writer.I16(static_cast<std::int16_t>(packet.head));
  writer.I16(static_cast<std::int16_t>(packet.accessory));
  writer.I16(static_cast<std::int16_t>(packet.accessory2));
  writer.I16(static_cast<std::int16_t>(packet.accessory3));
  writer.I16(static_cast<std::int16_t>(packet.robe));
  writer.I16(static_cast<std::int16_t>(packet.headpalette));
  writer.I16(static_cast<std::int16_t>(packet.bodypalette));
  writer.I16(static_cast<std::int16_t>(packet.body2));
  writer.U8(static_cast<std::uint8_t>(packet.sex));
  for (const EquipitemInfo& element : packet.list) {
    WriteEquipitemInfo(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketNotifyBounditem& packet) {
  if (frame.size() != PacketNotifyBounditem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketNotifyBounditem::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketNotifyBounditem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketNotifyBounditem::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketNotifyBounditem::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketGraffitiEntry& packet) {
  if (frame.size() != PacketGraffitiEntry::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketGraffitiEntry::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.creatorAID = static_cast<std::uint32_t>(reader.U32());
  packet.xPos = static_cast<std::int16_t>(reader.I16());
  packet.yPos = static_cast<std::int16_t>(reader.I16());
  packet.job = static_cast<std::uint8_t>(reader.U8());
  packet.isVisible = static_cast<std::uint8_t>(reader.U8());
  packet.isContens = static_cast<std::uint8_t>(reader.U8());
  packet.msg = reader.FixedString(80);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketGraffitiEntry& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketGraffitiEntry::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketGraffitiEntry::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.creatorAID));
  writer.I16(static_cast<std::int16_t>(packet.xPos));
  writer.I16(static_cast<std::int16_t>(packet.yPos));
  writer.U8(static_cast<std::uint8_t>(packet.job));
  writer.U8(static_cast<std::uint8_t>(packet.isVisible));
  writer.U8(static_cast<std::uint8_t>(packet.isContens));
  writer.FixedString(packet.msg, 80);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketDamage& packet) {
  if (frame.size() != PacketDamage::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketDamage::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.targetGID = static_cast<std::uint32_t>(reader.U32());
  packet.startTime = static_cast<std::uint32_t>(reader.U32());
  packet.attackMT = static_cast<std::int32_t>(reader.I32());
  packet.attackedMT = static_cast<std::int32_t>(reader.I32());
  packet.damage = static_cast<std::int32_t>(reader.I32());
  packet.is_sp_damaged = static_cast<std::uint8_t>(reader.U8());
  packet.count = static_cast<std::int16_t>(reader.I16());
  packet.action = static_cast<std::uint8_t>(reader.U8());
  packet.leftDamage = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketDamage& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketDamage::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketDamage::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.U32(static_cast<std::uint32_t>(packet.targetGID));
  writer.U32(static_cast<std::uint32_t>(packet.startTime));
  writer.I32(static_cast<std::int32_t>(packet.attackMT));
  writer.I32(static_cast<std::int32_t>(packet.attackedMT));
  writer.I32(static_cast<std::int32_t>(packet.damage));
  writer.U8(static_cast<std::uint8_t>(packet.is_sp_damaged));
  writer.I16(static_cast<std::int16_t>(packet.count));
  writer.U8(static_cast<std::uint8_t>(packet.action));
  writer.I32(static_cast<std::int32_t>(packet.leftDamage));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketGmMonsterItem& packet) {
  if (frame.size() != PacketGmMonsterItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.str = reader.FixedString(100);
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, CzNpcMarketPurchase& packet) {
  if (frame.size() < CzNpcMarketPurchase::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzNpcMarketPurchase::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % CzNpcMarketPurchaseSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / CzNpcMarketPurchaseSub::kWireSize);
    for (CzNpcMarketPurchaseSub& element : packet.list) {
      ReadCzNpcMarketPurchaseSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzNpcMarketPurchase& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzNpcMarketPurchase::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzNpcMarketPurchase::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const CzNpcMarketPurchaseSub& element : packet.list) {
    WriteCzNpcMarketPurchaseSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNpcMarketOpen& packet) {
  if (frame.size() < ZcNpcMarketOpen::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNpcMarketOpen::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcNpcMarketOpenSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / ZcNpcMarketOpenSub::kWireSize);
    for (ZcNpcMarketOpenSub& element : packet.list) {
      ReadZcNpcMarketOpenSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNpcMarketOpen& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNpcMarketOpen::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNpcMarketOpen::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcNpcMarketOpenSub& element : packet.list) {
    WriteZcNpcMarketOpenSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketWisEnd& packet) {
  if (frame.size() != PacketWisEnd::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketWisEnd::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int8_t>(reader.I8());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketWisEnd& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketWisEnd::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketWisEnd::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketPartyLeaderChanged& packet) {
  if (frame.size() != PacketPartyLeaderChanged::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketPartyLeaderChanged::kOpcode) {
    return false;
  }
  packet.prev_leader_aid = static_cast<std::uint32_t>(reader.U32());
  packet.new_leader_aid = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketPartyLeaderChanged& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketPartyLeaderChanged::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketPartyLeaderChanged::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.prev_leader_aid));
  writer.U32(static_cast<std::uint32_t>(packet.new_leader_aid));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcShortcutKeyList& packet) {
  if (frame.size() != ZcShortcutKeyList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcShortcutKeyList::kOpcode) {
    return false;
  }
  packet.rotate = static_cast<std::int8_t>(reader.I8());
  packet.tab = static_cast<std::int16_t>(reader.I16());
  for (HotkeyData& element : packet.hotkey) {
    ReadHotkeyData(reader, element);
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcShortcutKeyList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcShortcutKeyList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcShortcutKeyList::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.rotate));
  writer.I16(static_cast<std::int16_t>(packet.tab));
  for (const HotkeyData& element : packet.hotkey) {
    WriteHotkeyData(writer, element);
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzShortcutKeyChange1& packet) {
  if (frame.size() != CzShortcutKeyChange1::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzShortcutKeyChange1::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  ReadHotkeyData(reader, packet.hotkey);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzShortcutKeyChange1& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzShortcutKeyChange1::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzShortcutKeyChange1::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  WriteHotkeyData(writer, packet.hotkey);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzShortcutKeyChange2& packet) {
  if (frame.size() != CzShortcutKeyChange2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzShortcutKeyChange2::kOpcode) {
    return false;
  }
  packet.tab = static_cast<std::uint16_t>(reader.U16());
  packet.index = static_cast<std::uint16_t>(reader.U16());
  ReadHotkeyData(reader, packet.hotkey);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzShortcutKeyChange2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzShortcutKeyChange2::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzShortcutKeyChange2::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.tab));
  writer.U16(static_cast<std::uint16_t>(packet.index));
  WriteHotkeyData(writer, packet.hotkey);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzShortcutkeybarRotate1& packet) {
  if (frame.size() != CzShortcutkeybarRotate1::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzShortcutkeybarRotate1::kOpcode) {
    return false;
  }
  packet.rowshift = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzShortcutkeybarRotate1& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzShortcutkeybarRotate1::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzShortcutkeybarRotate1::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.rowshift));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzShortcutkeybarRotate2& packet) {
  if (frame.size() != CzShortcutkeybarRotate2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzShortcutkeybarRotate2::kOpcode) {
    return false;
  }
  packet.tab = static_cast<std::uint16_t>(reader.U16());
  packet.rowshift = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzShortcutkeybarRotate2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzShortcutkeybarRotate2::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzShortcutkeybarRotate2::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.tab));
  writer.U8(static_cast<std::uint8_t>(packet.rowshift));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketQuestListHeader& packet) {
  if (frame.size() < PacketQuestListHeader::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.questCount = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, PacketChatMessage& packet) {
  if (frame.size() < PacketChatMessage::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.message = reader.TailString();
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, PacketWhisperMessage& packet) {
  if (frame.size() < PacketWhisperMessage::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.name = reader.FixedString(24);
  packet.message = reader.TailString();
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, CzAddItemToMail& packet) {
  if (frame.size() != CzAddItemToMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.count = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckAddItemRodex& packet) {
  if (frame.size() != ZcAckAddItemRodex::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckAddItemRodex::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int8_t>(reader.I8());
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.count = static_cast<std::int16_t>(reader.I16());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::int8_t>(reader.I8());
  packet.IsIdentified = static_cast<std::int8_t>(reader.I8());
  packet.IsDamaged = static_cast<std::int8_t>(reader.I8());
  ReadEquipslotinfo(reader, packet.slot);
  for (Itemoptions& element : packet.optionData) {
    ReadItemoptions(reader, element);
  }
  packet.weight = static_cast<std::int16_t>(reader.I16());
  packet.favorite = static_cast<std::uint8_t>(reader.U8());
  packet.location = static_cast<std::uint32_t>(reader.U32());
  packet.refiningLevel = static_cast<std::int8_t>(reader.I8());
  packet.grade = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckAddItemRodex& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckAddItemRodex::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckAddItemRodex::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.result));
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.I16(static_cast<std::int16_t>(packet.count));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.I8(static_cast<std::int8_t>(packet.type));
  writer.I8(static_cast<std::int8_t>(packet.IsIdentified));
  writer.I8(static_cast<std::int8_t>(packet.IsDamaged));
  WriteEquipslotinfo(writer, packet.slot);
  for (const Itemoptions& element : packet.optionData) {
    WriteItemoptions(writer, element);
  }
  writer.I16(static_cast<std::int16_t>(packet.weight));
  writer.U8(static_cast<std::uint8_t>(packet.favorite));
  writer.U32(static_cast<std::uint32_t>(packet.location));
  writer.I8(static_cast<std::int8_t>(packet.refiningLevel));
  writer.I8(static_cast<std::int8_t>(packet.grade));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqOpenWriteMail& packet) {
  if (frame.size() != CzReqOpenWriteMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.receiveName = reader.FixedString(24);
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckOpenWriteMail& packet) {
  if (frame.size() != ZcAckOpenWriteMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckOpenWriteMail::kOpcode) {
    return false;
  }
  packet.receiveName = reader.FixedString(24);
  packet.result = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckOpenWriteMail& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckOpenWriteMail::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckOpenWriteMail::kOpcode);
  writer.FixedString(packet.receiveName, 24);
  writer.I8(static_cast<std::int8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqRemoveItemMail& packet) {
  if (frame.size() != CzReqRemoveItemMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.cnt = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckRemoveItemMail& packet) {
  if (frame.size() != ZcAckRemoveItemMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckRemoveItemMail::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int8_t>(reader.I8());
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.cnt = static_cast<std::uint16_t>(reader.U16());
  packet.weight = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckRemoveItemMail& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckRemoveItemMail::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckRemoveItemMail::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.result));
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.U16(static_cast<std::uint16_t>(packet.cnt));
  writer.I16(static_cast<std::int16_t>(packet.weight));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzSendMail& packet) {
  if (frame.size() < CzSendMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.receiveName = reader.FixedString(24);
  packet.senderName = reader.FixedString(24);
  packet.zeny = static_cast<std::int64_t>(reader.I64());
  packet.Titlelength = static_cast<std::int16_t>(reader.I16());
  packet.TextcontentsLength = static_cast<std::int16_t>(reader.I16());
  packet.receiver_char_id = static_cast<std::int32_t>(reader.I32());
  packet.string = reader.TailString();
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcWriteMailResult& packet) {
  if (frame.size() != ZcWriteMailResult::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcWriteMailResult::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcWriteMailResult& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcWriteMailResult::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcWriteMailResult::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzCheckname1& packet) {
  if (frame.size() != CzCheckname1::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzCheckname1::kOpcode) {
    return false;
  }
  packet.Name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzCheckname1& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzCheckname1::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzCheckname1::kOpcode);
  writer.FixedString(packet.Name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzCheckname2& packet) {
  if (frame.size() != CzCheckname2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzCheckname2::kOpcode) {
    return false;
  }
  packet.Name = reader.FixedString(24);
  packet.own_char = reader.FixedString(1);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzCheckname2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzCheckname2::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzCheckname2::kOpcode);
  writer.FixedString(packet.Name, 24);
  writer.FixedString(packet.own_char, 1);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCheckname& packet) {
  if (frame.size() != ZcCheckname::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCheckname::kOpcode) {
    return false;
  }
  packet.CharId = static_cast<std::int32_t>(reader.I32());
  packet.Class = static_cast<std::int16_t>(reader.I16());
  packet.BaseLevel = static_cast<std::int16_t>(reader.I16());
  packet.Name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCheckname& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCheckname::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCheckname::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.CharId));
  writer.I16(static_cast<std::int16_t>(packet.Class));
  writer.I16(static_cast<std::int16_t>(packet.BaseLevel));
  writer.FixedString(packet.Name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyUnreadmail& packet) {
  if (frame.size() != ZcNotifyUnreadmail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyUnreadmail::kOpcode) {
    return false;
  }
  packet.result = reader.FixedString(1);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyUnreadmail& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyUnreadmail::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyUnreadmail::kOpcode);
  writer.FixedString(packet.result, 1);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMailList& packet) {
  if (frame.size() < ZcMailList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.IsEnd = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, CzReqNextMailList& packet) {
  if (frame.size() != CzReqNextMailList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.opentype = static_cast<std::int8_t>(reader.I8());
  packet.Lower_MailID = static_cast<std::int64_t>(reader.I64());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, CzReqOpenMail& packet) {
  if (frame.size() != CzReqOpenMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.char_Upper_MailID = static_cast<std::int64_t>(reader.I64());
  packet.return_Upper_MailID = static_cast<std::int64_t>(reader.I64());
  packet.account_Upper_MailID = static_cast<std::int64_t>(reader.I64());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, CzReqReadMail& packet) {
  if (frame.size() != CzReqReadMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.opentype = static_cast<std::int8_t>(reader.I8());
  packet.MailID = static_cast<std::int64_t>(reader.I64());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckReadRodex& packet) {
  if (frame.size() < ZcAckReadRodex::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckReadRodex::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.opentype = static_cast<std::int8_t>(reader.I8());
  packet.MailID = static_cast<std::int64_t>(reader.I64());
  packet.TextcontentsLength = static_cast<std::int16_t>(reader.I16());
  packet.zeny = static_cast<std::int64_t>(reader.I64());
  packet.ItemCnt = static_cast<std::int8_t>(reader.I8());
  packet.Textcontent = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckReadRodex& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckReadRodex::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckReadRodex::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I8(static_cast<std::int8_t>(packet.opentype));
  writer.I64(static_cast<std::int64_t>(packet.MailID));
  writer.I16(static_cast<std::int16_t>(packet.TextcontentsLength));
  writer.I64(static_cast<std::int64_t>(packet.zeny));
  writer.I8(static_cast<std::int8_t>(packet.ItemCnt));
  writer.Text(packet.Textcontent);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqDeleteMail& packet) {
  if (frame.size() != CzReqDeleteMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.opentype = static_cast<std::int8_t>(reader.I8());
  packet.MailID = static_cast<std::int64_t>(reader.I64());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckDeleteMail& packet) {
  if (frame.size() != ZcAckDeleteMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckDeleteMail::kOpcode) {
    return false;
  }
  packet.opentype = static_cast<std::int8_t>(reader.I8());
  packet.MailID = static_cast<std::int64_t>(reader.I64());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckDeleteMail& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckDeleteMail::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckDeleteMail::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.opentype));
  writer.I64(static_cast<std::int64_t>(packet.MailID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqRefreshMailList& packet) {
  if (frame.size() != CzReqRefreshMailList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.Upper_MailID = static_cast<std::int64_t>(reader.I64());
  for (std::int8_t& element : packet.unknown) {
    element = static_cast<std::int8_t>(reader.I8());
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, CzReqZenyFromMail& packet) {
  if (frame.size() != CzReqZenyFromMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.MailID = static_cast<std::int64_t>(reader.I64());
  packet.opentype = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckZenyFromMail& packet) {
  if (frame.size() != ZcAckZenyFromMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.MailID = static_cast<std::int64_t>(reader.I64());
  packet.opentype = static_cast<std::int8_t>(reader.I8());
  packet.result = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, CzReqItemFromMail& packet) {
  if (frame.size() != CzReqItemFromMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.MailID = static_cast<std::int64_t>(reader.I64());
  packet.opentype = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckItemFromMail& packet) {
  if (frame.size() != ZcAckItemFromMail::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.MailID = static_cast<std::int64_t>(reader.I64());
  packet.opentype = static_cast<std::int8_t>(reader.I8());
  packet.result = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcSkillScale& packet) {
  if (frame.size() != ZcSkillScale::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSkillScale::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.skill_id = static_cast<std::int16_t>(reader.I16());
  packet.skill_lv = static_cast<std::int16_t>(reader.I16());
  packet.x = static_cast<std::int16_t>(reader.I16());
  packet.y = static_cast<std::int16_t>(reader.I16());
  packet.casttime = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSkillScale& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSkillScale::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSkillScale::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.I16(static_cast<std::int16_t>(packet.skill_id));
  writer.I16(static_cast<std::int16_t>(packet.skill_lv));
  writer.I16(static_cast<std::int16_t>(packet.x));
  writer.I16(static_cast<std::int16_t>(packet.y));
  writer.U32(static_cast<std::uint32_t>(packet.casttime));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcProgressActor& packet) {
  if (frame.size() != ZcProgressActor::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.GID = static_cast<std::int32_t>(reader.I32());
  packet.color = static_cast<std::int32_t>(reader.I32());
  packet.time = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAddMemberToGroup& packet) {
  if (frame.size() != ZcAddMemberToGroup::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAddMemberToGroup::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.leader = static_cast<std::uint32_t>(reader.U32());
  packet.class_ = static_cast<std::int16_t>(reader.I16());
  packet.baseLevel = static_cast<std::int16_t>(reader.I16());
  packet.x = static_cast<std::int16_t>(reader.I16());
  packet.y = static_cast<std::int16_t>(reader.I16());
  packet.offline = static_cast<std::uint8_t>(reader.U8());
  packet.partyName = reader.FixedString(24);
  packet.playerName = reader.FixedString(24);
  packet.mapName = reader.FixedString(16);
  packet.sharePickup = static_cast<std::int8_t>(reader.I8());
  packet.shareLoot = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAddMemberToGroup& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAddMemberToGroup::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAddMemberToGroup::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.U32(static_cast<std::uint32_t>(packet.leader));
  writer.I16(static_cast<std::int16_t>(packet.class_));
  writer.I16(static_cast<std::int16_t>(packet.baseLevel));
  writer.I16(static_cast<std::int16_t>(packet.x));
  writer.I16(static_cast<std::int16_t>(packet.y));
  writer.U8(static_cast<std::uint8_t>(packet.offline));
  writer.FixedString(packet.partyName, 24);
  writer.FixedString(packet.playerName, 24);
  writer.FixedString(packet.mapName, 16);
  writer.I8(static_cast<std::int8_t>(packet.sharePickup));
  writer.I8(static_cast<std::int8_t>(packet.shareLoot));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGroupList& packet) {
  if (frame.size() < ZcGroupList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGroupList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.partyName = reader.FixedString(24);
  if (reader.remaining() % ZcGroupListSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.members.resize(reader.remaining() / ZcGroupListSub::kWireSize);
    for (ZcGroupListSub& element : packet.members) {
      ReadZcGroupListSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGroupList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGroupList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGroupList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.FixedString(packet.partyName, 24);
  for (const ZcGroupListSub& element : packet.members) {
    WriteZcGroupListSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcClaninfo& packet) {
  if (frame.size() < ZcClaninfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcClaninfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.ClanID = static_cast<std::uint32_t>(reader.U32());
  packet.ClanName = reader.FixedString(24);
  packet.MasterName = reader.FixedString(24);
  packet.Map = reader.FixedString(16);
  packet.AllyCount = static_cast<std::uint8_t>(reader.U8());
  packet.AntagonistCount = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcClaninfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcClaninfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcClaninfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.ClanID));
  writer.FixedString(packet.ClanName, 24);
  writer.FixedString(packet.MasterName, 24);
  writer.FixedString(packet.Map, 16);
  writer.U8(static_cast<std::uint8_t>(packet.AllyCount));
  writer.U8(static_cast<std::uint8_t>(packet.AntagonistCount));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyClanConnectinfo& packet) {
  if (frame.size() != ZcNotifyClanConnectinfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyClanConnectinfo::kOpcode) {
    return false;
  }
  packet.NumConnect = static_cast<std::int16_t>(reader.I16());
  packet.NumTotal = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyClanConnectinfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyClanConnectinfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyClanConnectinfo::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.NumConnect));
  writer.I16(static_cast<std::int16_t>(packet.NumTotal));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckClanLeave& /*packet*/) {
  if (frame.size() != ZcAckClanLeave::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckClanLeave::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckClanLeave& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckClanLeave::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckClanLeave::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyClanChat& packet) {
  if (frame.size() < ZcNotifyClanChat::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyClanChat::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.MemberName = reader.FixedString(24);
  packet.Message = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyClanChat& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyClanChat::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyClanChat::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.FixedString(packet.MemberName, 24);
  writer.Text(packet.Message);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketQuestAddHeader& packet) {
  if (frame.size() != PacketQuestAddHeader::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.questID = static_cast<std::uint32_t>(reader.U32());
  packet.active = static_cast<std::uint8_t>(reader.U8());
  packet.quest_svrTime = static_cast<std::int32_t>(reader.I32());
  packet.quest_endTime = static_cast<std::int32_t>(reader.I32());
  packet.count = static_cast<std::int16_t>(reader.I16());
  if (reader.remaining() % PacketQuestHuntSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.objectives.resize(reader.remaining() / PacketQuestHuntSub::kWireSize);
    for (PacketQuestHuntSub& element : packet.objectives) {
      ReadPacketQuestHuntSub(reader, element);
    }
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, PacketQuestUpdateHeader& packet) {
  if (frame.size() < PacketQuestUpdateHeader::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.count = static_cast<std::int16_t>(reader.I16());
  if (reader.remaining() % PacketQuestUpdateHunt::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.objectives.resize(reader.remaining() / PacketQuestUpdateHunt::kWireSize);
    for (PacketQuestUpdateHunt& element : packet.objectives) {
      ReadPacketQuestUpdateHunt(reader, element);
    }
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, PacketQuestHuntInfo& packet) {
  if (frame.size() < PacketQuestHuntInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % PacketQuestHuntInfoSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.info.resize(reader.remaining() / PacketQuestHuntInfoSub::kWireSize);
    for (PacketQuestHuntInfoSub& element : packet.info) {
      ReadPacketQuestHuntInfoSub(reader, element);
    }
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcFormatstringMsg& packet) {
  if (frame.size() < ZcFormatstringMsg::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.MessageId = static_cast<std::uint16_t>(reader.U16());
  packet.MessageString = reader.TailString();
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcFormatstringMsgColor& packet) {
  if (frame.size() < ZcFormatstringMsgColor::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.messageId = static_cast<std::uint16_t>(reader.U16());
  packet.color = static_cast<std::uint32_t>(reader.U32());
  packet.messageString = reader.TailString();
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcMsgColor& packet) {
  if (frame.size() != ZcMsgColor::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMsgColor::kOpcode) {
    return false;
  }
  packet.MessageId = static_cast<std::uint16_t>(reader.U16());
  packet.MessageColor = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMsgColor& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMsgColor::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMsgColor::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.MessageId));
  writer.U32(static_cast<std::uint32_t>(packet.MessageColor));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzOpenUi& packet) {
  if (frame.size() != CzOpenUi::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.UIType = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcUiOpen& packet) {
  if (frame.size() != ZcUiOpen::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUiOpen::kOpcode) {
    return false;
  }
  packet.UIType = static_cast<std::int8_t>(reader.I8());
  packet.data = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUiOpen& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUiOpen::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUiOpen::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.UIType));
  writer.I32(static_cast<std::int32_t>(packet.data));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUiOpen2& packet) {
  if (frame.size() != ZcUiOpen2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUiOpen2::kOpcode) {
    return false;
  }
  packet.UIType = static_cast<std::int8_t>(reader.I8());
  packet.data = static_cast<std::int64_t>(reader.I64());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUiOpen2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUiOpen2::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUiOpen2::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.UIType));
  writer.I64(static_cast<std::int64_t>(packet.data));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUiAction& packet) {
  if (frame.size() != ZcUiAction::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.UIType = static_cast<std::int32_t>(reader.I32());
  packet.data = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, CzPrivateAirshipRequest& packet) {
  if (frame.size() != CzPrivateAirshipRequest::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.mapName = reader.FixedString(16);
  packet.ItemID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcPrivateAirshipResponse& packet) {
  if (frame.size() != ZcPrivateAirshipResponse::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.flag = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, CzReqStyleChange& packet) {
  if (frame.size() != CzReqStyleChange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqStyleChange::kOpcode) {
    return false;
  }
  packet.HeadPalette = static_cast<std::int16_t>(reader.I16());
  packet.HeadStyle = static_cast<std::int16_t>(reader.I16());
  packet.BodyPalette = static_cast<std::int16_t>(reader.I16());
  packet.TopAccessory = static_cast<std::int16_t>(reader.I16());
  packet.MidAccessory = static_cast<std::int16_t>(reader.I16());
  packet.BottomAccessory = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqStyleChange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqStyleChange::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqStyleChange::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.HeadPalette));
  writer.I16(static_cast<std::int16_t>(packet.HeadStyle));
  writer.I16(static_cast<std::int16_t>(packet.BodyPalette));
  writer.I16(static_cast<std::int16_t>(packet.TopAccessory));
  writer.I16(static_cast<std::int16_t>(packet.MidAccessory));
  writer.I16(static_cast<std::int16_t>(packet.BottomAccessory));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqStyleChange2& packet) {
  if (frame.size() != CzReqStyleChange2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqStyleChange2::kOpcode) {
    return false;
  }
  packet.HeadPalette = static_cast<std::int16_t>(reader.I16());
  packet.HeadStyle = static_cast<std::int16_t>(reader.I16());
  packet.BodyPalette = static_cast<std::int16_t>(reader.I16());
  packet.TopAccessory = static_cast<std::int16_t>(reader.I16());
  packet.MidAccessory = static_cast<std::int16_t>(reader.I16());
  packet.BottomAccessory = static_cast<std::int16_t>(reader.I16());
  packet.BodyStyle = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqStyleChange2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqStyleChange2::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqStyleChange2::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.HeadPalette));
  writer.I16(static_cast<std::int16_t>(packet.HeadStyle));
  writer.I16(static_cast<std::int16_t>(packet.BodyPalette));
  writer.I16(static_cast<std::int16_t>(packet.TopAccessory));
  writer.I16(static_cast<std::int16_t>(packet.MidAccessory));
  writer.I16(static_cast<std::int16_t>(packet.BottomAccessory));
  writer.I16(static_cast<std::int16_t>(packet.BodyStyle));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcStyleChangeRes& packet) {
  if (frame.size() != ZcStyleChangeRes::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcStyleChangeRes::kOpcode) {
    return false;
  }
  packet.flag = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcStyleChangeRes& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcStyleChangeRes::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcStyleChangeRes::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.flag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzPetEvolution& packet) {
  if (frame.size() < CzPetEvolution::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.EvolvedPetEggID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, PacketZcRefuseLogin& packet) {
  if (frame.size() != PacketZcRefuseLogin::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.error_code = static_cast<std::uint32_t>(reader.U32());
  packet.block_date = reader.FixedString(20);
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyChat& packet) {
  if (frame.size() < ZcNotifyChat::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyChat::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.Message = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyChat& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyChat::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyChat::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.Text(packet.Message);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyPlayerchat& packet) {
  if (frame.size() < ZcNotifyPlayerchat::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.Message = reader.TailString();
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcItemEntry& packet) {
  if (frame.size() != ZcItemEntry::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcItemEntry::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.identify = static_cast<std::uint8_t>(reader.U8());
  packet.x = static_cast<std::uint16_t>(reader.U16());
  packet.y = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::uint16_t>(reader.U16());
  packet.subX = static_cast<std::uint8_t>(reader.U8());
  packet.subY = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcItemEntry& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcItemEntry::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcItemEntry::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U8(static_cast<std::uint8_t>(packet.identify));
  writer.U16(static_cast<std::uint16_t>(packet.x));
  writer.U16(static_cast<std::uint16_t>(packet.y));
  writer.U16(static_cast<std::uint16_t>(packet.amount));
  writer.U8(static_cast<std::uint8_t>(packet.subX));
  writer.U8(static_cast<std::uint8_t>(packet.subY));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAddItemToStore& packet) {
  if (frame.size() != ZcAddItemToStore::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAddItemToStore::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.amount = static_cast<std::int32_t>(reader.I32());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.itemType = static_cast<std::uint8_t>(reader.U8());
  packet.identified = static_cast<std::uint8_t>(reader.U8());
  packet.damaged = static_cast<std::uint8_t>(reader.U8());
  ReadEquipslotinfo(reader, packet.slot);
  for (Itemoptions& element : packet.option_data) {
    ReadItemoptions(reader, element);
  }
  packet.refine = static_cast<std::uint8_t>(reader.U8());
  packet.grade = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAddItemToStore& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAddItemToStore::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAddItemToStore::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.I32(static_cast<std::int32_t>(packet.amount));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U8(static_cast<std::uint8_t>(packet.itemType));
  writer.U8(static_cast<std::uint8_t>(packet.identified));
  writer.U8(static_cast<std::uint8_t>(packet.damaged));
  WriteEquipslotinfo(writer, packet.slot);
  for (const Itemoptions& element : packet.option_data) {
    WriteItemoptions(writer, element);
  }
  writer.U8(static_cast<std::uint8_t>(packet.refine));
  writer.U8(static_cast<std::uint8_t>(packet.grade));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMvpGettingItem& packet) {
  if (frame.size() != ZcMvpGettingItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMvpGettingItem::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMvpGettingItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMvpGettingItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMvpGettingItem::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckTouseskill& packet) {
  if (frame.size() != ZcAckTouseskill::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckTouseskill::kOpcode) {
    return false;
  }
  packet.skillId = static_cast<std::uint16_t>(reader.U16());
  packet.btype = static_cast<std::int32_t>(reader.I32());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.flag = static_cast<std::uint8_t>(reader.U8());
  packet.cause = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckTouseskill& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckTouseskill::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckTouseskill::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.skillId));
  writer.I32(static_cast<std::int32_t>(packet.btype));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U8(static_cast<std::uint8_t>(packet.flag));
  writer.U8(static_cast<std::uint8_t>(packet.cause));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAddItemToCart& packet) {
  if (frame.size() != ZcAddItemToCart::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAddItemToCart::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.amount = static_cast<std::int32_t>(reader.I32());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.itemType = static_cast<std::uint8_t>(reader.U8());
  packet.identified = static_cast<std::uint8_t>(reader.U8());
  packet.damaged = static_cast<std::uint8_t>(reader.U8());
  ReadEquipslotinfo(reader, packet.slot);
  for (Itemoptions& element : packet.option_data) {
    ReadItemoptions(reader, element);
  }
  packet.refine = static_cast<std::uint8_t>(reader.U8());
  packet.grade = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAddItemToCart& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAddItemToCart::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAddItemToCart::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.I32(static_cast<std::int32_t>(packet.amount));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U8(static_cast<std::uint8_t>(packet.itemType));
  writer.U8(static_cast<std::uint8_t>(packet.identified));
  writer.U8(static_cast<std::uint8_t>(packet.damaged));
  WriteEquipslotinfo(writer, packet.slot);
  for (const Itemoptions& element : packet.option_data) {
    WriteItemoptions(writer, element);
  }
  writer.U8(static_cast<std::uint8_t>(packet.refine));
  writer.U8(static_cast<std::uint8_t>(packet.grade));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqmakingitem& packet) {
  if (frame.size() != CzReqmakingitem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqmakingitem::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  for (std::uint32_t& element : packet.material) {
    element = static_cast<std::uint32_t>(reader.U32());
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqmakingitem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqmakingitem::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqmakingitem::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  for (const std::uint32_t element : packet.material) {
    writer.U32(static_cast<std::uint32_t>(element));
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckReqmakingitem& packet) {
  if (frame.size() != ZcAckReqmakingitem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckReqmakingitem::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int16_t>(reader.I16());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckReqmakingitem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckReqmakingitem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckReqmakingitem::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcFeedPet& packet) {
  if (frame.size() != ZcFeedPet::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcFeedPet::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcFeedPet& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcFeedPet::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcFeedPet::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcFeedMer& packet) {
  if (frame.size() != ZcFeedMer::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcFeedMer::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcFeedMer& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcFeedMer::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcFeedMer::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUseItemAck& packet) {
  if (frame.size() != ZcUseItemAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUseItemAck::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.amount = static_cast<std::int16_t>(reader.I16());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUseItemAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUseItemAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUseItemAck::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.I16(static_cast<std::int16_t>(packet.amount));
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSpriteChange& packet) {
  if (frame.size() != ZcSpriteChange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSpriteChange::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.val = static_cast<std::uint32_t>(reader.U32());
  packet.val2 = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSpriteChange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSpriteChange::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSpriteChange::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.val));
  writer.U32(static_cast<std::uint32_t>(packet.val2));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAddExchangeItem& packet) {
  if (frame.size() != ZcAddExchangeItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAddExchangeItem::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.itemType = static_cast<std::uint8_t>(reader.U8());
  packet.amount = static_cast<std::int32_t>(reader.I32());
  packet.identified = static_cast<std::uint8_t>(reader.U8());
  packet.damaged = static_cast<std::uint8_t>(reader.U8());
  ReadEquipslotinfo(reader, packet.slot);
  for (Itemoptions& element : packet.option_data) {
    ReadItemoptions(reader, element);
  }
  packet.location = static_cast<std::uint32_t>(reader.U32());
  packet.look = static_cast<std::uint16_t>(reader.U16());
  packet.refine = static_cast<std::uint8_t>(reader.U8());
  packet.grade = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAddExchangeItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAddExchangeItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAddExchangeItem::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U8(static_cast<std::uint8_t>(packet.itemType));
  writer.I32(static_cast<std::int32_t>(packet.amount));
  writer.U8(static_cast<std::uint8_t>(packet.identified));
  writer.U8(static_cast<std::uint8_t>(packet.damaged));
  WriteEquipslotinfo(writer, packet.slot);
  for (const Itemoptions& element : packet.option_data) {
    WriteItemoptions(writer, element);
  }
  writer.U32(static_cast<std::uint32_t>(packet.location));
  writer.U16(static_cast<std::uint16_t>(packet.look));
  writer.U8(static_cast<std::uint8_t>(packet.refine));
  writer.U8(static_cast<std::uint8_t>(packet.grade));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCashTimeCounter& packet) {
  if (frame.size() != ZcCashTimeCounter::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCashTimeCounter::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.seconds = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCashTimeCounter& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCashTimeCounter::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCashTimeCounter::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U32(static_cast<std::uint32_t>(packet.seconds));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCashItemDelete& packet) {
  if (frame.size() != ZcCashItemDelete::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCashItemDelete::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCashItemDelete& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCashItemDelete::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCashItemDelete::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcItemPickupParty& packet) {
  if (frame.size() != ZcItemPickupParty::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcItemPickupParty::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.identified = static_cast<std::uint8_t>(reader.U8());
  packet.damaged = static_cast<std::uint8_t>(reader.U8());
  ReadEquipslotinfo(reader, packet.slot);
  packet.location = static_cast<std::uint16_t>(reader.U16());
  packet.itemType = static_cast<std::uint8_t>(reader.U8());
  packet.refine = static_cast<std::uint8_t>(reader.U8());
  packet.grade = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcItemPickupParty& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcItemPickupParty::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcItemPickupParty::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U8(static_cast<std::uint8_t>(packet.identified));
  writer.U8(static_cast<std::uint8_t>(packet.damaged));
  WriteEquipslotinfo(writer, packet.slot);
  writer.U16(static_cast<std::uint16_t>(packet.location));
  writer.U8(static_cast<std::uint8_t>(packet.itemType));
  writer.U8(static_cast<std::uint8_t>(packet.refine));
  writer.U8(static_cast<std::uint8_t>(packet.grade));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUpdateItemFromBuyingStore& packet) {
  if (frame.size() != ZcUpdateItemFromBuyingStore::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUpdateItemFromBuyingStore::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.amount = static_cast<std::uint16_t>(reader.U16());
  packet.zeny = static_cast<std::uint32_t>(reader.U32());
  packet.zenyLimit = static_cast<std::uint32_t>(reader.U32());
  packet.charId = static_cast<std::uint32_t>(reader.U32());
  packet.updateTime = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUpdateItemFromBuyingStore& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUpdateItemFromBuyingStore::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUpdateItemFromBuyingStore::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U16(static_cast<std::uint16_t>(packet.amount));
  writer.U32(static_cast<std::uint32_t>(packet.zeny));
  writer.U32(static_cast<std::uint32_t>(packet.zenyLimit));
  writer.U32(static_cast<std::uint32_t>(packet.charId));
  writer.U32(static_cast<std::uint32_t>(packet.updateTime));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckWeaponrefine& packet) {
  if (frame.size() != ZcAckWeaponrefine::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckWeaponrefine::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int32_t>(reader.I32());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckWeaponrefine& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckWeaponrefine::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckWeaponrefine::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcFailedTradeBuyingStoreToSeller& packet) {
  if (frame.size() != ZcFailedTradeBuyingStoreToSeller::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcFailedTradeBuyingStoreToSeller::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint16_t>(reader.U16());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcFailedTradeBuyingStoreToSeller& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcFailedTradeBuyingStoreToSeller::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcFailedTradeBuyingStoreToSeller::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqItemrepair2& packet) {
  if (frame.size() != CzReqItemrepair2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqItemrepair2::kOpcode) {
    return false;
  }
  ReadRepairitemInfo2(reader, packet.item);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqItemrepair2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqItemrepair2::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqItemrepair2::kOpcode);
  WriteRepairitemInfo2(writer, packet.item);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqItemrepair1& packet) {
  if (frame.size() != CzReqItemrepair1::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqItemrepair1::kOpcode) {
    return false;
  }
  ReadRepairitemInfo1(reader, packet.item);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqItemrepair1& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqItemrepair1::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqItemrepair1::kOpcode);
  WriteRepairitemInfo1(writer, packet.item);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqMakingitem& packet) {
  if (frame.size() != CzReqMakingitem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqMakingitem::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::int16_t>(reader.I16());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqMakingitem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqMakingitem::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqMakingitem::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzSsilistItemClick& packet) {
  if (frame.size() != CzSsilistItemClick::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzSsilistItemClick::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.storeId = static_cast<std::uint32_t>(reader.U32());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzSsilistItemClick& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzSsilistItemClick::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzSsilistItemClick::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.storeId));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckSchedulerCashitem& packet) {
  if (frame.size() < ZcAckSchedulerCashitem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckSchedulerCashitem::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.count = static_cast<std::int16_t>(reader.I16());
  packet.tabNum = static_cast<std::int16_t>(reader.I16());
  if (reader.remaining() % ZcAckSchedulerCashitemSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcAckSchedulerCashitemSub::kWireSize);
    for (ZcAckSchedulerCashitemSub& element : packet.items) {
      ReadZcAckSchedulerCashitemSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckSchedulerCashitem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckSchedulerCashitem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckSchedulerCashitem::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I16(static_cast<std::int16_t>(packet.count));
  writer.I16(static_cast<std::int16_t>(packet.tabNum));
  for (const ZcAckSchedulerCashitemSub& element : packet.items) {
    WriteZcAckSchedulerCashitemSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPcPurchaseMyitemlist& packet) {
  if (frame.size() < ZcPcPurchaseMyitemlist::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPcPurchaseMyitemlist::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  if (reader.remaining() % ZcPcPurchaseMyitemlistSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcPcPurchaseMyitemlistSub::kWireSize);
    for (ZcPcPurchaseMyitemlistSub& element : packet.items) {
      ReadZcPcPurchaseMyitemlistSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPcPurchaseMyitemlist& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPcPurchaseMyitemlist::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPcPurchaseMyitemlist::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  for (const ZcPcPurchaseMyitemlistSub& element : packet.items) {
    WriteZcPcPurchaseMyitemlistSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPcPurchaseItemlist& packet) {
  if (frame.size() < ZcPcPurchaseItemlist::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPcPurchaseItemlist::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcPcPurchaseItemlistSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcPcPurchaseItemlistSub::kWireSize);
    for (ZcPcPurchaseItemlistSub& element : packet.items) {
      ReadZcPcPurchaseItemlistSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPcPurchaseItemlist& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPcPurchaseItemlist::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPcPurchaseItemlist::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcPcPurchaseItemlistSub& element : packet.items) {
    WriteZcPcPurchaseItemlistSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzPcPurchaseItemlist& packet) {
  if (frame.size() < CzPcPurchaseItemlist::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % CzPcPurchaseItemlistSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / CzPcPurchaseItemlistSub::kWireSize);
    for (CzPcPurchaseItemlistSub& element : packet.items) {
      ReadCzPcPurchaseItemlistSub(reader, element);
    }
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, CzReqOpenBuyingStore& packet) {
  if (frame.size() < CzReqOpenBuyingStore::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.zenyLimit = static_cast<std::uint32_t>(reader.U32());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  packet.storeName = reader.FixedString(80);
  if (reader.remaining() % CzReqOpenBuyingStoreSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / CzReqOpenBuyingStoreSub::kWireSize);
    for (CzReqOpenBuyingStoreSub& element : packet.items) {
      ReadCzReqOpenBuyingStoreSub(reader, element);
    }
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcMyitemlistBuyingStore& packet) {
  if (frame.size() < ZcMyitemlistBuyingStore::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMyitemlistBuyingStore::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.zenyLimit = static_cast<std::uint32_t>(reader.U32());
  if (reader.remaining() % ZcMyitemlistBuyingStoreSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcMyitemlistBuyingStoreSub::kWireSize);
    for (ZcMyitemlistBuyingStoreSub& element : packet.items) {
      ReadZcMyitemlistBuyingStoreSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMyitemlistBuyingStore& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMyitemlistBuyingStore::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMyitemlistBuyingStore::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.zenyLimit));
  for (const ZcMyitemlistBuyingStoreSub& element : packet.items) {
    WriteZcMyitemlistBuyingStoreSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPcPurchaseItemlistFrommc& packet) {
  if (frame.size() < ZcPcPurchaseItemlistFrommc::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPcPurchaseItemlistFrommc::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.venderId = static_cast<std::uint32_t>(reader.U32());
  if (reader.remaining() % ZcPcPurchaseItemlistFrommcSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcPcPurchaseItemlistFrommcSub::kWireSize);
    for (ZcPcPurchaseItemlistFrommcSub& element : packet.items) {
      ReadZcPcPurchaseItemlistFrommcSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPcPurchaseItemlistFrommc& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPcPurchaseItemlistFrommc::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPcPurchaseItemlistFrommc::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.venderId));
  for (const ZcPcPurchaseItemlistFrommcSub& element : packet.items) {
    WriteZcPcPurchaseItemlistFrommcSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckItemlistBuyingStore& packet) {
  if (frame.size() < ZcAckItemlistBuyingStore::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckItemlistBuyingStore::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.storeId = static_cast<std::uint32_t>(reader.U32());
  packet.zenyLimit = static_cast<std::uint32_t>(reader.U32());
  if (reader.remaining() % ZcAckItemlistBuyingStoreSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcAckItemlistBuyingStoreSub::kWireSize);
    for (ZcAckItemlistBuyingStoreSub& element : packet.items) {
      ReadZcAckItemlistBuyingStoreSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckItemlistBuyingStore& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckItemlistBuyingStore::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckItemlistBuyingStore::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.storeId));
  writer.U32(static_cast<std::uint32_t>(packet.zenyLimit));
  for (const ZcAckItemlistBuyingStoreSub& element : packet.items) {
    WriteZcAckItemlistBuyingStoreSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqTradeBuyingStore& packet) {
  if (frame.size() < CzReqTradeBuyingStore::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.storeId = static_cast<std::uint32_t>(reader.U32());
  if (reader.remaining() % CzReqTradeBuyingStoreSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / CzReqTradeBuyingStoreSub::kWireSize);
    for (CzReqTradeBuyingStoreSub& element : packet.items) {
      ReadCzReqTradeBuyingStoreSub(reader, element);
    }
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcMakableitemlist& packet) {
  if (frame.size() < ZcMakableitemlist::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMakableitemlist::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcMakableitemlistSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcMakableitemlistSub::kWireSize);
    for (ZcMakableitemlistSub& element : packet.items) {
      ReadZcMakableitemlistSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMakableitemlist& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMakableitemlist::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMakableitemlist::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcMakableitemlistSub& element : packet.items) {
    WriteZcMakableitemlistSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMakingarrowList& packet) {
  if (frame.size() < ZcMakingarrowList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMakingarrowList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcMakingarrowListSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcMakingarrowListSub::kWireSize);
    for (ZcMakingarrowListSub& element : packet.items) {
      ReadZcMakingarrowListSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMakingarrowList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMakingarrowList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMakingarrowList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcMakingarrowListSub& element : packet.items) {
    WriteZcMakingarrowListSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSkillSelectRequest& packet) {
  if (frame.size() < ZcSkillSelectRequest::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSkillSelectRequest::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.flag = static_cast<std::int32_t>(reader.I32());
  if (reader.remaining() % 2 != 0) {
    reader.Invalidate();
  } else {
    packet.skillIds.resize(reader.remaining() / 2);
    for (std::int16_t& element : packet.skillIds) {
      element = static_cast<std::int16_t>(reader.I16());
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSkillSelectRequest& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSkillSelectRequest::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSkillSelectRequest::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I32(static_cast<std::int32_t>(packet.flag));
  for (const std::int16_t element : packet.skillIds) {
    writer.I16(static_cast<std::int16_t>(element));
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzSkillSelectResponse& packet) {
  if (frame.size() != CzSkillSelectResponse::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzSkillSelectResponse::kOpcode) {
    return false;
  }
  packet.flag = static_cast<std::int32_t>(reader.I32());
  packet.selectedSkillId = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzSkillSelectResponse& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzSkillSelectResponse::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzSkillSelectResponse::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.flag));
  writer.I16(static_cast<std::int16_t>(packet.selectedSkillId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyWeaponitemlist& packet) {
  if (frame.size() < ZcNotifyWeaponitemlist::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyWeaponitemlist::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcNotifyWeaponitemlistSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcNotifyWeaponitemlistSub::kWireSize);
    for (ZcNotifyWeaponitemlistSub& element : packet.items) {
      ReadZcNotifyWeaponitemlistSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyWeaponitemlist& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyWeaponitemlist::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyWeaponitemlist::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcNotifyWeaponitemlistSub& element : packet.items) {
    WriteZcNotifyWeaponitemlistSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMakingitemList& packet) {
  if (frame.size() < ZcMakingitemList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMakingitemList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.makeItem = static_cast<std::uint16_t>(reader.U16());
  if (reader.remaining() % ZcMakingitemListSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcMakingitemListSub::kWireSize);
    for (ZcMakingitemListSub& element : packet.items) {
      ReadZcMakingitemListSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMakingitemList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMakingitemList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMakingitemList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.makeItem));
  for (const ZcMakingitemListSub& element : packet.items) {
    WriteZcMakingitemListSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPcCashPointItemlist& packet) {
  if (frame.size() < ZcPcCashPointItemlist::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPcCashPointItemlist::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.cashPoints = static_cast<std::uint32_t>(reader.U32());
  packet.kafraPoints = static_cast<std::uint32_t>(reader.U32());
  if (reader.remaining() % ZcPcCashPointItemlistSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcPcCashPointItemlistSub::kWireSize);
    for (ZcPcCashPointItemlistSub& element : packet.items) {
      ReadZcPcCashPointItemlistSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPcCashPointItemlist& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPcCashPointItemlist::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPcCashPointItemlist::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.cashPoints));
  writer.U32(static_cast<std::uint32_t>(packet.kafraPoints));
  for (const ZcPcCashPointItemlistSub& element : packet.items) {
    WriteZcPcCashPointItemlistSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzPcBuyCashPointItem& packet) {
  if (frame.size() < CzPcBuyCashPointItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.kafraPoints = static_cast<std::uint32_t>(reader.U32());
  packet.count = static_cast<std::uint16_t>(reader.U16());
  if (reader.remaining() % CzPcBuyCashPointItemSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / CzPcBuyCashPointItemSub::kWireSize);
    for (CzPcBuyCashPointItemSub& element : packet.items) {
      ReadCzPcBuyCashPointItemSub(reader, element);
    }
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, CzSearchStoreInfo& packet) {
  if (frame.size() < CzSearchStoreInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.searchType = static_cast<std::uint8_t>(reader.U8());
  packet.maxPrice = static_cast<std::uint32_t>(reader.U32());
  packet.minPrice = static_cast<std::uint32_t>(reader.U32());
  packet.itemsCount = static_cast<std::uint8_t>(reader.U8());
  packet.cardsCount = static_cast<std::uint8_t>(reader.U8());
  if (reader.remaining() % CzSearchStoreInfoItem::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / CzSearchStoreInfoItem::kWireSize);
    for (CzSearchStoreInfoItem& element : packet.items) {
      ReadCzSearchStoreInfoItem(reader, element);
    }
  }
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcSearchStoreInfoFailed& packet) {
  if (frame.size() != ZcSearchStoreInfoFailed::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSearchStoreInfoFailed::kOpcode) {
    return false;
  }
  packet.reason = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSearchStoreInfoFailed& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSearchStoreInfoFailed::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSearchStoreInfoFailed::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.reason));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcOpenSearchStoreInfo& packet) {
  if (frame.size() != ZcOpenSearchStoreInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcOpenSearchStoreInfo::kOpcode) {
    return false;
  }
  packet.effect = static_cast<std::uint16_t>(reader.U16());
  packet.remainingUses = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcOpenSearchStoreInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcOpenSearchStoreInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcOpenSearchStoreInfo::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.effect));
  writer.U8(static_cast<std::uint8_t>(packet.remainingUses));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSsilistItemClickAck& packet) {
  if (frame.size() != ZcSsilistItemClickAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSsilistItemClickAck::kOpcode) {
    return false;
  }
  packet.x = static_cast<std::int16_t>(reader.I16());
  packet.y = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSsilistItemClickAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSsilistItemClickAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSsilistItemClickAck::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.x));
  writer.I16(static_cast<std::int16_t>(packet.y));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSearchStoreInfoAck& packet) {
  if (frame.size() < ZcSearchStoreInfoAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSearchStoreInfoAck::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.firstPage = static_cast<std::uint8_t>(reader.U8());
  packet.nextPage = static_cast<std::uint8_t>(reader.U8());
  packet.usesCount = static_cast<std::uint8_t>(reader.U8());
  if (reader.remaining() % ZcSearchStoreInfoAckSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcSearchStoreInfoAckSub::kWireSize);
    for (ZcSearchStoreInfoAckSub& element : packet.items) {
      ReadZcSearchStoreInfoAckSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSearchStoreInfoAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSearchStoreInfoAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSearchStoreInfoAck::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.firstPage));
  writer.U8(static_cast<std::uint8_t>(packet.nextPage));
  writer.U8(static_cast<std::uint8_t>(packet.usesCount));
  for (const ZcSearchStoreInfoAckSub& element : packet.items) {
    WriteZcSearchStoreInfoAckSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketAchievementUpdate& packet) {
  if (frame.size() != PacketAchievementUpdate::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketAchievementUpdate::kOpcode) {
    return false;
  }
  packet.total_points = static_cast<std::uint32_t>(reader.U32());
  packet.rank = static_cast<std::uint16_t>(reader.U16());
  packet.current_rank_points = static_cast<std::uint32_t>(reader.U32());
  packet.next_rank_points = static_cast<std::uint32_t>(reader.U32());
  ReadAchListInfo(reader, packet.ach);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketAchievementUpdate& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketAchievementUpdate::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketAchievementUpdate::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.total_points));
  writer.U16(static_cast<std::uint16_t>(packet.rank));
  writer.U32(static_cast<std::uint32_t>(packet.current_rank_points));
  writer.U32(static_cast<std::uint32_t>(packet.next_rank_points));
  WriteAchListInfo(writer, packet.ach);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketAchievementRewardAck& packet) {
  if (frame.size() != PacketAchievementRewardAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != PacketAchievementRewardAck::kOpcode) {
    return false;
  }
  packet.failed = static_cast<std::uint8_t>(reader.U8());
  packet.ach_id = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const PacketAchievementRewardAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(PacketAchievementRewardAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(PacketAchievementRewardAck::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.failed));
  writer.U32(static_cast<std::uint32_t>(packet.ach_id));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, PacketReqnameAck& packet) {
  if (frame.size() != PacketReqnameAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.gid = static_cast<std::int32_t>(reader.I32());
  packet.name = reader.FixedString(24);
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckReqnameall& packet) {
  if (frame.size() != ZcAckReqnameall::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckReqnameall::kOpcode) {
    return false;
  }
  packet.gid = static_cast<std::int32_t>(reader.I32());
  packet.name = reader.FixedString(24);
  packet.party_name = reader.FixedString(24);
  packet.guild_name = reader.FixedString(24);
  packet.position_name = reader.FixedString(24);
  packet.title_id = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckReqnameall& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckReqnameall::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckReqnameall::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.gid));
  writer.FixedString(packet.name, 24);
  writer.FixedString(packet.party_name, 24);
  writer.FixedString(packet.guild_name, 24);
  writer.FixedString(packet.position_name, 24);
  writer.I32(static_cast<std::int32_t>(packet.title_id));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckReqnameallNpc& packet) {
  if (frame.size() != ZcAckReqnameallNpc::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckReqnameallNpc::kOpcode) {
    return false;
  }
  packet.gid = static_cast<std::int32_t>(reader.I32());
  packet.groupId = static_cast<std::int32_t>(reader.I32());
  packet.name = reader.FixedString(24);
  packet.title = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckReqnameallNpc& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckReqnameallNpc::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckReqnameallNpc::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.gid));
  writer.I32(static_cast<std::int32_t>(packet.groupId));
  writer.FixedString(packet.name, 24);
  writer.FixedString(packet.title, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcOverweightPercent& packet) {
  if (frame.size() != ZcOverweightPercent::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.percent = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcWarplist& packet) {
  if (frame.size() < ZcWarplist::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcWarplist::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.skillId = static_cast<std::uint16_t>(reader.U16());
  if (reader.remaining() % ZcWarplistSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.maps.resize(reader.remaining() / ZcWarplistSub::kWireSize);
    for (ZcWarplistSub& element : packet.maps) {
      ReadZcWarplistSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcWarplist& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcWarplist::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcWarplist::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.skillId));
  for (const ZcWarplistSub& element : packet.maps) {
    WriteZcWarplistSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGroupIsalive& packet) {
  if (frame.size() != ZcGroupIsalive::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGroupIsalive::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.isDead = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGroupIsalive& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGroupIsalive::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGroupIsalive::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U8(static_cast<std::uint8_t>(packet.isDead));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGuildPosition& packet) {
  if (frame.size() < ZcGuildPosition::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGuildPosition::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.position = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGuildPosition& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGuildPosition::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGuildPosition::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.Text(packet.position);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoveItemFailed& packet) {
  if (frame.size() != ZcMoveItemFailed::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoveItemFailed::kOpcode) {
    return false;
  }
  packet.itemIndex = static_cast<std::int16_t>(reader.I16());
  packet.itemCount = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoveItemFailed& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoveItemFailed::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoveItemFailed::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.itemIndex));
  writer.I16(static_cast<std::int16_t>(packet.itemCount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckBanGuild1& packet) {
  if (frame.size() != ZcAckBanGuild1::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.name = reader.FixedString(24);
  packet.reason = reader.FixedString(40);
  packet.account_name = reader.FixedString(24);
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckBanGuild2& packet) {
  if (frame.size() != ZcAckBanGuild2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.name = reader.FixedString(24);
  packet.reason = reader.FixedString(40);
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckBanGuild3& packet) {
  if (frame.size() != ZcAckBanGuild3::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.reason = reader.FixedString(40);
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckLeaveGuild1& packet) {
  if (frame.size() != ZcAckLeaveGuild1::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.name = reader.FixedString(24);
  packet.reason = reader.FixedString(40);
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckLeaveGuild2& packet) {
  if (frame.size() != ZcAckLeaveGuild2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.reason = reader.FixedString(40);
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, CzMemorialdungeonCommand& packet) {
  if (frame.size() != CzMemorialdungeonCommand::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.command = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcRemoveEffect& packet) {
  if (frame.size() != ZcRemoveEffect::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcRemoveEffect::kOpcode) {
    return false;
  }
  packet.aid = static_cast<std::uint32_t>(reader.U32());
  packet.effectId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcRemoveEffect& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcRemoveEffect::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcRemoveEffect::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.aid));
  writer.U32(static_cast<std::uint32_t>(packet.effectId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcViewCamerainfo& packet) {
  if (frame.size() != ZcViewCamerainfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcViewCamerainfo::kOpcode) {
    return false;
  }
  packet.action = static_cast<std::int8_t>(reader.I8());
  packet.range = static_cast<float>(reader.F32());
  packet.rotation = static_cast<float>(reader.F32());
  packet.latitude = static_cast<float>(reader.F32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcViewCamerainfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcViewCamerainfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcViewCamerainfo::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.action));
  writer.F32(static_cast<float>(packet.range));
  writer.F32(static_cast<float>(packet.rotation));
  writer.F32(static_cast<float>(packet.latitude));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzViewCamerainfo& packet) {
  if (frame.size() != CzViewCamerainfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzViewCamerainfo::kOpcode) {
    return false;
  }
  packet.action = static_cast<std::int8_t>(reader.I8());
  packet.range = static_cast<float>(reader.F32());
  packet.rotation = static_cast<float>(reader.F32());
  packet.latitude = static_cast<float>(reader.F32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzViewCamerainfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzViewCamerainfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzViewCamerainfo::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.action));
  writer.F32(static_cast<float>(packet.range));
  writer.F32(static_cast<float>(packet.rotation));
  writer.F32(static_cast<float>(packet.latitude));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUpdateCardslot& packet) {
  if (frame.size() != ZcUpdateCardslot::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUpdateCardslot::kOpcode) {
    return false;
  }
  packet.wearState = static_cast<std::int16_t>(reader.I16());
  packet.cardSlot = static_cast<std::int16_t>(reader.I16());
  packet.itemId = static_cast<std::int32_t>(reader.I32());
  packet.equipFlag = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUpdateCardslot& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUpdateCardslot::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUpdateCardslot::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.wearState));
  writer.I16(static_cast<std::int16_t>(packet.cardSlot));
  writer.I32(static_cast<std::int32_t>(packet.itemId));
  writer.I8(static_cast<std::int8_t>(packet.equipFlag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDebugmsg& packet) {
  if (frame.size() < ZcDebugmsg::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDebugmsg::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.color = static_cast<std::int32_t>(reader.I32());
  packet.message = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDebugmsg& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDebugmsg::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDebugmsg::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I32(static_cast<std::int32_t>(packet.color));
  writer.Text(packet.message);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzUseSkillStart& packet) {
  if (frame.size() != CzUseSkillStart::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzUseSkillStart::kOpcode) {
    return false;
  }
  packet.skillId = static_cast<std::int16_t>(reader.I16());
  packet.skillLv = static_cast<std::int16_t>(reader.I16());
  packet.targetId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzUseSkillStart& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzUseSkillStart::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzUseSkillStart::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.skillId));
  writer.I16(static_cast<std::int16_t>(packet.skillLv));
  writer.U32(static_cast<std::uint32_t>(packet.targetId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzUseSkillEnd& packet) {
  if (frame.size() != CzUseSkillEnd::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzUseSkillEnd::kOpcode) {
    return false;
  }
  packet.skillId = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzUseSkillEnd& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzUseSkillEnd::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzUseSkillEnd::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.skillId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcExtendBodyitemSize& packet) {
  if (frame.size() != ZcExtendBodyitemSize::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcExtendBodyitemSize::kOpcode) {
    return false;
  }
  packet.expansionSize = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcExtendBodyitemSize& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcExtendBodyitemSize::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcExtendBodyitemSize::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.expansionSize));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckOpenMsgboxExtendBodyitemSize& packet) {
  if (frame.size() != ZcAckOpenMsgboxExtendBodyitemSize::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckOpenMsgboxExtendBodyitemSize::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckOpenMsgboxExtendBodyitemSize& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckOpenMsgboxExtendBodyitemSize::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckOpenMsgboxExtendBodyitemSize::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckExtendBodyitemSize& packet) {
  if (frame.size() != ZcAckExtendBodyitemSize::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckExtendBodyitemSize::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckExtendBodyitemSize& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckExtendBodyitemSize::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckExtendBodyitemSize::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqOpenMsgboxExtendBodyitemSize& /*packet*/) {
  if (frame.size() != CzReqOpenMsgboxExtendBodyitemSize::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqOpenMsgboxExtendBodyitemSize::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqOpenMsgboxExtendBodyitemSize& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqOpenMsgboxExtendBodyitemSize::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqOpenMsgboxExtendBodyitemSize::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqExtendBodyitemSize& /*packet*/) {
  if (frame.size() != CzReqExtendBodyitemSize::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqExtendBodyitemSize::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqExtendBodyitemSize& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqExtendBodyitemSize::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqExtendBodyitemSize::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzCloseMsgboxExtendBodyitemSize& /*packet*/) {
  if (frame.size() != CzCloseMsgboxExtendBodyitemSize::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzCloseMsgboxExtendBodyitemSize::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzCloseMsgboxExtendBodyitemSize& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzCloseMsgboxExtendBodyitemSize::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzCloseMsgboxExtendBodyitemSize::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqRemaintime& /*packet*/) {
  if (frame.size() != CzReqRemaintime::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqRemaintime::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqRemaintime& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqRemaintime::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqRemaintime::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzPartyConfig& packet) {
  if (frame.size() != CzPartyConfig::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzPartyConfig::kOpcode) {
    return false;
  }
  packet.refuseInvite = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzPartyConfig& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzPartyConfig::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzPartyConfig::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.refuseInvite));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNpcBarterMarketIteminfo& packet) {
  if (frame.size() < ZcNpcBarterMarketIteminfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNpcBarterMarketIteminfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcNpcBarterMarketIteminfoSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / ZcNpcBarterMarketIteminfoSub::kWireSize);
    for (ZcNpcBarterMarketIteminfoSub& element : packet.list) {
      ReadZcNpcBarterMarketIteminfoSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNpcBarterMarketIteminfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNpcBarterMarketIteminfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNpcBarterMarketIteminfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcNpcBarterMarketIteminfoSub& element : packet.list) {
    WriteZcNpcBarterMarketIteminfoSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzNpcBarterMarketClose& /*packet*/) {
  if (frame.size() != CzNpcBarterMarketClose::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzNpcBarterMarketClose::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzNpcBarterMarketClose& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzNpcBarterMarketClose::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzNpcBarterMarketClose::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzNpcBarterMarketPurchase& packet) {
  if (frame.size() < CzNpcBarterMarketPurchase::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzNpcBarterMarketPurchase::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % CzNpcBarterMarketPurchaseSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / CzNpcBarterMarketPurchaseSub::kWireSize);
    for (CzNpcBarterMarketPurchaseSub& element : packet.list) {
      ReadCzNpcBarterMarketPurchaseSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzNpcBarterMarketPurchase& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzNpcBarterMarketPurchase::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzNpcBarterMarketPurchase::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const CzNpcBarterMarketPurchaseSub& element : packet.list) {
    WriteCzNpcBarterMarketPurchaseSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUseskillAck& packet) {
  if (frame.size() != ZcUseskillAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUseskillAck::kOpcode) {
    return false;
  }
  packet.srcId = static_cast<std::uint32_t>(reader.U32());
  packet.dstId = static_cast<std::uint32_t>(reader.U32());
  packet.x = static_cast<std::uint16_t>(reader.U16());
  packet.y = static_cast<std::uint16_t>(reader.U16());
  packet.skillId = static_cast<std::uint16_t>(reader.U16());
  packet.element = static_cast<std::uint32_t>(reader.U32());
  packet.delayTime = static_cast<std::uint32_t>(reader.U32());
  packet.disposable = static_cast<std::uint8_t>(reader.U8());
  packet.attackMT = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUseskillAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUseskillAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUseskillAck::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.srcId));
  writer.U32(static_cast<std::uint32_t>(packet.dstId));
  writer.U16(static_cast<std::uint16_t>(packet.x));
  writer.U16(static_cast<std::uint16_t>(packet.y));
  writer.U16(static_cast<std::uint16_t>(packet.skillId));
  writer.U32(static_cast<std::uint32_t>(packet.element));
  writer.U32(static_cast<std::uint32_t>(packet.delayTime));
  writer.U8(static_cast<std::uint8_t>(packet.disposable));
  writer.U32(static_cast<std::uint32_t>(packet.attackMT));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzClientVersion& packet) {
  if (frame.size() != CzClientVersion::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzClientVersion::kOpcode) {
    return false;
  }
  packet.clientVersion = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzClientVersion& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzClientVersion::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzClientVersion::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.clientVersion));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzPingLive& /*packet*/) {
  if (frame.size() != CzPingLive::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzPingLive::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzPingLive& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzPingLive::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzPingLive::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPingLive& /*packet*/) {
  if (frame.size() != ZcPingLive::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPingLive::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPingLive& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPingLive::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPingLive::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzCmdResetcooltime& /*packet*/) {
  if (frame.size() != CzCmdResetcooltime::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzCmdResetcooltime::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzCmdResetcooltime& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzCmdResetcooltime::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzCmdResetcooltime::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzCloseUiStylingshop& /*packet*/) {
  if (frame.size() != CzCloseUiStylingshop::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzCloseUiStylingshop::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzCloseUiStylingshop& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzCloseUiStylingshop::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzCloseUiStylingshop::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyActorinit& /*packet*/) {
  if (frame.size() != ZcNotifyActorinit::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyActorinit::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyActorinit& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyActorinit::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyActorinit::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPartyConfig& packet) {
  if (frame.size() != ZcPartyConfig::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPartyConfig::kOpcode) {
    return false;
  }
  packet.denyPartyInvites = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPartyConfig& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPartyConfig::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPartyConfig::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.denyPartyInvites));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcRoleChange& packet) {
  if (frame.size() != ZcRoleChange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcRoleChange::kOpcode) {
    return false;
  }
  packet.flag = static_cast<std::int32_t>(reader.I32());
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcRoleChange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcRoleChange::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcRoleChange::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.flag));
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcBanList& packet) {
  if (frame.size() < ZcBanList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcBanList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcBanListSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.chars.resize(reader.remaining() / ZcBanListSub::kWireSize);
    for (ZcBanListSub& element : packet.chars) {
      ReadZcBanListSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcBanList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcBanList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcBanList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcBanListSub& element : packet.chars) {
    WriteZcBanListSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckCloseRoulette& packet) {
  if (frame.size() != ZcAckCloseRoulette::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckCloseRoulette::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckCloseRoulette& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckCloseRoulette::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckCloseRoulette::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckMergeItem& packet) {
  if (frame.size() != ZcAckMergeItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckMergeItem::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.amount = static_cast<std::int16_t>(reader.I16());
  packet.reason = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckMergeItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckMergeItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckMergeItem::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.I16(static_cast<std::int16_t>(packet.amount));
  writer.U8(static_cast<std::uint8_t>(packet.reason));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMergeItemOpen& packet) {
  if (frame.size() < ZcMergeItemOpen::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMergeItemOpen::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcMergeItemOpenSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcMergeItemOpenSub::kWireSize);
    for (ZcMergeItemOpenSub& element : packet.items) {
      ReadZcMergeItemOpenSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMergeItemOpen& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMergeItemOpen::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMergeItemOpen::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcMergeItemOpenSub& element : packet.items) {
    WriteZcMergeItemOpenSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSePcBuyCashitemResult& packet) {
  if (frame.size() != ZcSePcBuyCashitemResult::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSePcBuyCashitemResult::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.result = static_cast<std::uint16_t>(reader.U16());
  packet.cashPoints = static_cast<std::uint32_t>(reader.U32());
  packet.kafraPoints = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSePcBuyCashitemResult& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSePcBuyCashitemResult::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSePcBuyCashitemResult::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U16(static_cast<std::uint16_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.cashPoints));
  writer.U32(static_cast<std::uint32_t>(packet.kafraPoints));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcOpenRefiningUi& /*packet*/) {
  if (frame.size() != ZcOpenRefiningUi::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcOpenRefiningUi::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcOpenRefiningUi& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcOpenRefiningUi::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcOpenRefiningUi::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzRefiningSelectItem& packet) {
  if (frame.size() != CzRefiningSelectItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzRefiningSelectItem::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzRefiningSelectItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzRefiningSelectItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzRefiningSelectItem::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcRefiningMaterialList& packet) {
  if (frame.size() < ZcRefiningMaterialList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcRefiningMaterialList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.itemIndex = static_cast<std::int16_t>(reader.I16());
  packet.blacksmithBlessing = static_cast<std::int8_t>(reader.I8());
  if (reader.remaining() % ZcRefiningMaterialListSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.req.resize(reader.remaining() / ZcRefiningMaterialListSub::kWireSize);
    for (ZcRefiningMaterialListSub& element : packet.req) {
      ReadZcRefiningMaterialListSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcRefiningMaterialList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcRefiningMaterialList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcRefiningMaterialList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I16(static_cast<std::int16_t>(packet.itemIndex));
  writer.I8(static_cast<std::int8_t>(packet.blacksmithBlessing));
  for (const ZcRefiningMaterialListSub& element : packet.req) {
    WriteZcRefiningMaterialListSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqRefining& packet) {
  if (frame.size() != CzReqRefining::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqRefining::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.blacksmithBlessing = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqRefining& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqRefining::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqRefining::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.I8(static_cast<std::int8_t>(packet.blacksmithBlessing));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzCloseRefiningUi& /*packet*/) {
  if (frame.size() != CzCloseRefiningUi::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzCloseRefiningUi::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzCloseRefiningUi& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzCloseRefiningUi::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzCloseRefiningUi::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcBroadcastItemrefiningResult& packet) {
  if (frame.size() != ZcBroadcastItemrefiningResult::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcBroadcastItemrefiningResult::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.refine_level = static_cast<std::int8_t>(reader.I8());
  packet.status = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcBroadcastItemrefiningResult& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcBroadcastItemrefiningResult::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcBroadcastItemrefiningResult::kOpcode);
  writer.FixedString(packet.name, 24);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.I8(static_cast<std::int8_t>(packet.refine_level));
  writer.I8(static_cast<std::int8_t>(packet.status));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcStatusChangeAck& packet) {
  if (frame.size() != ZcStatusChangeAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcStatusChangeAck::kOpcode) {
    return false;
  }
  packet.sp = static_cast<std::uint16_t>(reader.U16());
  packet.ok = static_cast<std::uint8_t>(reader.U8());
  packet.value = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcStatusChangeAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcStatusChangeAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcStatusChangeAck::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.sp));
  writer.U8(static_cast<std::uint8_t>(packet.ok));
  writer.U8(static_cast<std::uint8_t>(packet.value));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcEquipmentEffect& packet) {
  if (frame.size() < ZcEquipmentEffect::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcEquipmentEffect::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.aid = static_cast<std::uint32_t>(reader.U32());
  packet.status = static_cast<std::int8_t>(reader.I8());
  if (reader.remaining() % 2 != 0) {
    reader.Invalidate();
  } else {
    packet.effects.resize(reader.remaining() / 2);
    for (std::uint16_t& element : packet.effects) {
      element = static_cast<std::uint16_t>(reader.U16());
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcEquipmentEffect& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcEquipmentEffect::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcEquipmentEffect::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.aid));
  writer.I8(static_cast<std::int8_t>(packet.status));
  for (const std::uint16_t element : packet.effects) {
    writer.U16(static_cast<std::uint16_t>(element));
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAddSkill& packet) {
  if (frame.size() != ZcAddSkill::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAddSkill::kOpcode) {
    return false;
  }
  ReadSkilldata(reader, packet.skill);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAddSkill& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAddSkill::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAddSkill::kOpcode);
  WriteSkilldata(writer, packet.skill);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSkillinfoList& packet) {
  if (frame.size() < ZcSkillinfoList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSkillinfoList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % Skilldata::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.skills.resize(reader.remaining() / Skilldata::kWireSize);
    for (Skilldata& element : packet.skills) {
      ReadSkilldata(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSkillinfoList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSkillinfoList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSkillinfoList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const Skilldata& element : packet.skills) {
    WriteSkilldata(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSkillinfoUpdate2& packet) {
  if (frame.size() != ZcSkillinfoUpdate2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSkillinfoUpdate2::kOpcode) {
    return false;
  }
  packet.id = static_cast<std::uint16_t>(reader.U16());
  packet.inf = static_cast<std::int32_t>(reader.I32());
  packet.level = static_cast<std::uint16_t>(reader.U16());
  packet.sp = static_cast<std::uint16_t>(reader.U16());
  packet.range2 = static_cast<std::uint16_t>(reader.U16());
  packet.upFlag = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSkillinfoUpdate2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSkillinfoUpdate2::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSkillinfoUpdate2::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.id));
  writer.I32(static_cast<std::int32_t>(packet.inf));
  writer.U16(static_cast<std::uint16_t>(packet.level));
  writer.U16(static_cast<std::uint16_t>(packet.sp));
  writer.U16(static_cast<std::uint16_t>(packet.range2));
  writer.U8(static_cast<std::uint8_t>(packet.upFlag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNpcMarketPurchaseResult& packet) {
  if (frame.size() < ZcNpcMarketPurchaseResult::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNpcMarketPurchaseResult::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.result = static_cast<std::uint16_t>(reader.U16());
  if (reader.remaining() % ZcNpcMarketPurchaseResultSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / ZcNpcMarketPurchaseResultSub::kWireSize);
    for (ZcNpcMarketPurchaseResultSub& element : packet.list) {
      ReadZcNpcMarketPurchaseResultSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNpcMarketPurchaseResult& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNpcMarketPurchaseResult::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNpcMarketPurchaseResult::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.result));
  for (const ZcNpcMarketPurchaseResultSub& element : packet.list) {
    WriteZcNpcMarketPurchaseResultSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcTalkboxChatcontents& packet) {
  if (frame.size() != ZcTalkboxChatcontents::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcTalkboxChatcontents::kOpcode) {
    return false;
  }
  packet.aid = static_cast<std::uint32_t>(reader.U32());
  packet.message = reader.FixedString(21);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcTalkboxChatcontents& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcTalkboxChatcontents::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcTalkboxChatcontents::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.aid));
  writer.FixedString(packet.message, 21);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGuildAgitInfo& packet) {
  if (frame.size() < ZcGuildAgitInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGuildAgitInfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % 1 != 0) {
    reader.Invalidate();
  } else {
    packet.castle_list.resize(reader.remaining() / 1);
    for (std::int8_t& element : packet.castle_list) {
      element = static_cast<std::int8_t>(reader.I8());
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGuildAgitInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGuildAgitInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGuildAgitInfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const std::int8_t element : packet.castle_list) {
    writer.I8(static_cast<std::int8_t>(element));
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqMoveGuildAgit& packet) {
  if (frame.size() != CzReqMoveGuildAgit::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqMoveGuildAgit::kOpcode) {
    return false;
  }
  packet.castle_id = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqMoveGuildAgit& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqMoveGuildAgit::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqMoveGuildAgit::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.castle_id));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcReqAckMoveGuildAgit& packet) {
  if (frame.size() != ZcReqAckMoveGuildAgit::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcReqAckMoveGuildAgit::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcReqAckMoveGuildAgit& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcReqAckMoveGuildAgit::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcReqAckMoveGuildAgit::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcReqAckAgitInvestment& packet) {
  if (frame.size() != ZcReqAckAgitInvestment::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcReqAckAgitInvestment::kOpcode) {
    return false;
  }
  packet.castle_id = static_cast<std::int8_t>(reader.I8());
  packet.economy = static_cast<std::int32_t>(reader.I32());
  packet.defense = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcReqAckAgitInvestment& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcReqAckAgitInvestment::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcReqAckAgitInvestment::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.castle_id));
  writer.I32(static_cast<std::int32_t>(packet.economy));
  writer.I32(static_cast<std::int32_t>(packet.defense));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqAgitInvestment& packet) {
  if (frame.size() != CzReqAgitInvestment::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqAgitInvestment::kOpcode) {
    return false;
  }
  packet.castle_id = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqAgitInvestment& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqAgitInvestment::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqAgitInvestment::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.castle_id));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcRandomCombineItemUiOpen& packet) {
  if (frame.size() != ZcRandomCombineItemUiOpen::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcRandomCombineItemUiOpen::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcRandomCombineItemUiOpen& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcRandomCombineItemUiOpen::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcRandomCombineItemUiOpen::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzRandomCombineItemUiClose& /*packet*/) {
  if (frame.size() != CzRandomCombineItemUiClose::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzRandomCombineItemUiClose::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzRandomCombineItemUiClose& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzRandomCombineItemUiClose::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzRandomCombineItemUiClose::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqRandomCombineItem& packet) {
  if (frame.size() < CzReqRandomCombineItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqRandomCombineItem::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.itemId = static_cast<std::int32_t>(reader.I32());
  if (reader.remaining() % CzReqRandomCombineItemSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / CzReqRandomCombineItemSub::kWireSize);
    for (CzReqRandomCombineItemSub& element : packet.items) {
      ReadCzReqRandomCombineItemSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqRandomCombineItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqRandomCombineItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqRandomCombineItem::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I32(static_cast<std::int32_t>(packet.itemId));
  for (const CzReqRandomCombineItemSub& element : packet.items) {
    WriteCzReqRandomCombineItemSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckRandomCombineItem& packet) {
  if (frame.size() != ZcAckRandomCombineItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckRandomCombineItem::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckRandomCombineItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckRandomCombineItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckRandomCombineItem::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzUninstallation& packet) {
  if (frame.size() != CzUninstallation::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzUninstallation::kOpcode) {
    return false;
  }
  packet.InstallationKind = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzUninstallation& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzUninstallation::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzUninstallation::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.InstallationKind));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyEffect3& packet) {
  if (frame.size() != ZcNotifyEffect3::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyEffect3::kOpcode) {
    return false;
  }
  packet.aid = static_cast<std::uint32_t>(reader.U32());
  packet.effectId = static_cast<std::uint32_t>(reader.U32());
  packet.num = static_cast<std::uint64_t>(reader.U64());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyEffect3& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyEffect3::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyEffect3::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.aid));
  writer.U32(static_cast<std::uint32_t>(packet.effectId));
  writer.U64(static_cast<std::uint64_t>(packet.num));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzSeCashshopOpen1& /*packet*/) {
  if (frame.size() != CzSeCashshopOpen1::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzSeCashshopOpen1::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzSeCashshopOpen1& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzSeCashshopOpen1::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzSeCashshopOpen1::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzSeCashshopOpen2& packet) {
  if (frame.size() != CzSeCashshopOpen2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzSeCashshopOpen2::kOpcode) {
    return false;
  }
  packet.tab = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzSeCashshopOpen2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzSeCashshopOpen2::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzSeCashshopOpen2::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.tab));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzGetAccountLimtiedSaleList& /*packet*/) {
  if (frame.size() != CzGetAccountLimtiedSaleList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzGetAccountLimtiedSaleList::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzGetAccountLimtiedSaleList& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzGetAccountLimtiedSaleList::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzGetAccountLimtiedSaleList::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSeCashshopOpen& packet) {
  if (frame.size() != ZcSeCashshopOpen::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSeCashshopOpen::kOpcode) {
    return false;
  }
  packet.cashPoints = static_cast<std::uint32_t>(reader.U32());
  packet.kafraPoints = static_cast<std::uint32_t>(reader.U32());
  packet.tab = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSeCashshopOpen& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSeCashshopOpen::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSeCashshopOpen::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.cashPoints));
  writer.U32(static_cast<std::uint32_t>(packet.kafraPoints));
  writer.U32(static_cast<std::uint32_t>(packet.tab));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzNpcExpandedBarterMarketClose& /*packet*/) {
  if (frame.size() != CzNpcExpandedBarterMarketClose::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzNpcExpandedBarterMarketClose::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzNpcExpandedBarterMarketClose& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzNpcExpandedBarterMarketClose::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzNpcExpandedBarterMarketClose::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzNpcExpandedBarterMarketPurchase& packet) {
  if (frame.size() < CzNpcExpandedBarterMarketPurchase::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzNpcExpandedBarterMarketPurchase::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % CzNpcExpandedBarterMarketPurchaseSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / CzNpcExpandedBarterMarketPurchaseSub::kWireSize);
    for (CzNpcExpandedBarterMarketPurchaseSub& element : packet.list) {
      ReadCzNpcExpandedBarterMarketPurchaseSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzNpcExpandedBarterMarketPurchase& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzNpcExpandedBarterMarketPurchase::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzNpcExpandedBarterMarketPurchase::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const CzNpcExpandedBarterMarketPurchaseSub& element : packet.list) {
    WriteCzNpcExpandedBarterMarketPurchaseSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcStateChange& packet) {
  if (frame.size() != ZcStateChange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcStateChange::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.bodyState = static_cast<std::int16_t>(reader.I16());
  packet.healthState = static_cast<std::int16_t>(reader.I16());
  packet.effectState = static_cast<std::int32_t>(reader.I32());
  packet.isPKModeON = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcStateChange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcStateChange::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcStateChange::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.I16(static_cast<std::int16_t>(packet.bodyState));
  writer.I16(static_cast<std::int16_t>(packet.healthState));
  writer.I32(static_cast<std::int32_t>(packet.effectState));
  writer.I8(static_cast<std::int8_t>(packet.isPKModeON));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAutorunSkill& packet) {
  if (frame.size() != ZcAutorunSkill::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAutorunSkill::kOpcode) {
    return false;
  }
  packet.skill_id = static_cast<std::uint16_t>(reader.U16());
  packet.skill_type = static_cast<std::uint32_t>(reader.U32());
  packet.skill_lv = static_cast<std::uint16_t>(reader.U16());
  packet.skill_sp = static_cast<std::uint16_t>(reader.U16());
  packet.skill_range = static_cast<std::uint16_t>(reader.U16());
  packet.skill_name = reader.FixedString(24);
  packet.up_flag = reader.FixedString(1);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAutorunSkill& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAutorunSkill::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAutorunSkill::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.skill_id));
  writer.U32(static_cast<std::uint32_t>(packet.skill_type));
  writer.U16(static_cast<std::uint16_t>(packet.skill_lv));
  writer.U16(static_cast<std::uint16_t>(packet.skill_sp));
  writer.U16(static_cast<std::uint16_t>(packet.skill_range));
  writer.FixedString(packet.skill_name, 24);
  writer.FixedString(packet.up_flag, 1);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcRandomUpgradeItemUiOpen& packet) {
  if (frame.size() != ZcRandomUpgradeItemUiOpen::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcRandomUpgradeItemUiOpen::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcRandomUpgradeItemUiOpen& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcRandomUpgradeItemUiOpen::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcRandomUpgradeItemUiOpen::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckRandomUpgradeItem& packet) {
  if (frame.size() != ZcAckRandomUpgradeItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckRandomUpgradeItem::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckRandomUpgradeItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckRandomUpgradeItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckRandomUpgradeItem::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzRandomUpgradeItemUiClose& /*packet*/) {
  if (frame.size() != CzRandomUpgradeItemUiClose::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzRandomUpgradeItemUiClose::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzRandomUpgradeItemUiClose& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzRandomUpgradeItemUiClose::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzRandomUpgradeItemUiClose::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqRandomUpgradeItem& packet) {
  if (frame.size() != CzReqRandomUpgradeItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqRandomUpgradeItem::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.index = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqRandomUpgradeItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqRandomUpgradeItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqRandomUpgradeItem::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U16(static_cast<std::uint16_t>(packet.index));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPersonalInfomation& packet) {
  if (frame.size() < ZcPersonalInfomation::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPersonalInfomation::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.total_exp = static_cast<std::int32_t>(reader.I32());
  packet.total_death = static_cast<std::int32_t>(reader.I32());
  packet.total_drop = static_cast<std::int32_t>(reader.I32());
  if (reader.remaining() % ZcPersonalInfomationSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.details.resize(reader.remaining() / ZcPersonalInfomationSub::kWireSize);
    for (ZcPersonalInfomationSub& element : packet.details) {
      ReadZcPersonalInfomationSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPersonalInfomation& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPersonalInfomation::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPersonalInfomation::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I32(static_cast<std::int32_t>(packet.total_exp));
  writer.I32(static_cast<std::int32_t>(packet.total_death));
  writer.I32(static_cast<std::int32_t>(packet.total_drop));
  for (const ZcPersonalInfomationSub& element : packet.details) {
    WriteZcPersonalInfomationSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzRequestActnpc& packet) {
  if (frame.size() != CzRequestActnpc::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  // L'opcode n'est pas vérifiable : il n'est écrit nulle part.
  reader.Skip(2);
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.targetGID = static_cast<std::uint32_t>(reader.U32());
  packet.action = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifySkill& packet) {
  if (frame.size() != ZcNotifySkill::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifySkill::kOpcode) {
    return false;
  }
  packet.SKID = static_cast<std::uint16_t>(reader.U16());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.targetID = static_cast<std::uint32_t>(reader.U32());
  packet.startTime = static_cast<std::uint32_t>(reader.U32());
  packet.attackMT = static_cast<std::int32_t>(reader.I32());
  packet.attackedMT = static_cast<std::int32_t>(reader.I32());
  packet.damage = static_cast<std::int32_t>(reader.I32());
  packet.level = static_cast<std::int16_t>(reader.I16());
  packet.count = static_cast<std::int16_t>(reader.I16());
  packet.action = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifySkill& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifySkill::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifySkill::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.SKID));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.targetID));
  writer.U32(static_cast<std::uint32_t>(packet.startTime));
  writer.I32(static_cast<std::int32_t>(packet.attackMT));
  writer.I32(static_cast<std::int32_t>(packet.attackedMT));
  writer.I32(static_cast<std::int32_t>(packet.damage));
  writer.I16(static_cast<std::int16_t>(packet.level));
  writer.I16(static_cast<std::int16_t>(packet.count));
  writer.I8(static_cast<std::int8_t>(packet.action));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUseSkill& packet) {
  if (frame.size() != ZcUseSkill::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUseSkill::kOpcode) {
    return false;
  }
  packet.SKID = static_cast<std::uint16_t>(reader.U16());
  packet.level = static_cast<std::int32_t>(reader.I32());
  packet.targetAID = static_cast<std::uint32_t>(reader.U32());
  packet.srcAID = static_cast<std::uint32_t>(reader.U32());
  packet.result = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUseSkill& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUseSkill::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUseSkill::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.SKID));
  writer.I32(static_cast<std::int32_t>(packet.level));
  writer.U32(static_cast<std::uint32_t>(packet.targetAID));
  writer.U32(static_cast<std::uint32_t>(packet.srcAID));
  writer.I8(static_cast<std::int8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyGroundskill& packet) {
  if (frame.size() != ZcNotifyGroundskill::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyGroundskill::kOpcode) {
    return false;
  }
  packet.SKID = static_cast<std::uint16_t>(reader.U16());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.level = static_cast<std::int16_t>(reader.I16());
  packet.xPos = static_cast<std::int16_t>(reader.I16());
  packet.yPos = static_cast<std::int16_t>(reader.I16());
  packet.startTime = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyGroundskill& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyGroundskill::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyGroundskill::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.SKID));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.I16(static_cast<std::int16_t>(packet.level));
  writer.I16(static_cast<std::int16_t>(packet.xPos));
  writer.I16(static_cast<std::int16_t>(packet.yPos));
  writer.U32(static_cast<std::uint32_t>(packet.startTime));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSkillPostdelay& packet) {
  if (frame.size() != ZcSkillPostdelay::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSkillPostdelay::kOpcode) {
    return false;
  }
  packet.SKID = static_cast<std::uint16_t>(reader.U16());
  packet.DelayTM = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSkillPostdelay& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSkillPostdelay::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSkillPostdelay::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.SKID));
  writer.U32(static_cast<std::uint32_t>(packet.DelayTM));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifySkillPosition& packet) {
  if (frame.size() != ZcNotifySkillPosition::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifySkillPosition::kOpcode) {
    return false;
  }
  packet.SKID = static_cast<std::uint16_t>(reader.U16());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.targetID = static_cast<std::uint32_t>(reader.U32());
  packet.startTime = static_cast<std::uint32_t>(reader.U32());
  packet.attackMT = static_cast<std::int32_t>(reader.I32());
  packet.attackedMT = static_cast<std::int32_t>(reader.I32());
  packet.xPos = static_cast<std::int16_t>(reader.I16());
  packet.yPos = static_cast<std::int16_t>(reader.I16());
  packet.damage = static_cast<std::int16_t>(reader.I16());
  packet.level = static_cast<std::int16_t>(reader.I16());
  packet.count = static_cast<std::int16_t>(reader.I16());
  packet.action = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifySkillPosition& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifySkillPosition::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifySkillPosition::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.SKID));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.targetID));
  writer.U32(static_cast<std::uint32_t>(packet.startTime));
  writer.I32(static_cast<std::int32_t>(packet.attackMT));
  writer.I32(static_cast<std::int32_t>(packet.attackedMT));
  writer.I16(static_cast<std::int16_t>(packet.xPos));
  writer.I16(static_cast<std::int16_t>(packet.yPos));
  writer.I16(static_cast<std::int16_t>(packet.damage));
  writer.I16(static_cast<std::int16_t>(packet.level));
  writer.I16(static_cast<std::int16_t>(packet.count));
  writer.I8(static_cast<std::int8_t>(packet.action));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCMarkerinfo& packet) {
  if (frame.size() != ZcCMarkerinfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCMarkerinfo::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.xPos = static_cast<std::int16_t>(reader.I16());
  packet.yPos = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCMarkerinfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCMarkerinfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCMarkerinfo::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.I16(static_cast<std::int16_t>(packet.xPos));
  writer.I16(static_cast<std::int16_t>(packet.yPos));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMembermgrInfo& packet) {
  if (frame.size() < ZcMembermgrInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMembermgrInfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % GuildMemberInfo::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.guildMemberInfo.resize(reader.remaining() / GuildMemberInfo::kWireSize);
    for (GuildMemberInfo& element : packet.guildMemberInfo) {
      ReadGuildMemberInfo(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMembermgrInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMembermgrInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMembermgrInfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const GuildMemberInfo& element : packet.guildMemberInfo) {
    WriteGuildMemberInfo(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGuildInfo& packet) {
  if (frame.size() != ZcGuildInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGuildInfo::kOpcode) {
    return false;
  }
  packet.GDID = static_cast<std::int32_t>(reader.I32());
  packet.level = static_cast<std::int32_t>(reader.I32());
  packet.userNum = static_cast<std::int32_t>(reader.I32());
  packet.maxUserNum = static_cast<std::int32_t>(reader.I32());
  packet.userAverageLevel = static_cast<std::int32_t>(reader.I32());
  packet.exp = static_cast<std::int32_t>(reader.I32());
  packet.maxExp = static_cast<std::int32_t>(reader.I32());
  packet.point = static_cast<std::int32_t>(reader.I32());
  packet.honor = static_cast<std::int32_t>(reader.I32());
  packet.virtue = static_cast<std::int32_t>(reader.I32());
  packet.emblemVersion = static_cast<std::int32_t>(reader.I32());
  packet.guildname = reader.FixedString(24);
  packet.manageLand = reader.FixedString(16);
  packet.zeny = static_cast<std::int32_t>(reader.I32());
  packet.masterGID = static_cast<std::int32_t>(reader.I32());
  packet.masterName = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGuildInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGuildInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGuildInfo::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.GDID));
  writer.I32(static_cast<std::int32_t>(packet.level));
  writer.I32(static_cast<std::int32_t>(packet.userNum));
  writer.I32(static_cast<std::int32_t>(packet.maxUserNum));
  writer.I32(static_cast<std::int32_t>(packet.userAverageLevel));
  writer.I32(static_cast<std::int32_t>(packet.exp));
  writer.I32(static_cast<std::int32_t>(packet.maxExp));
  writer.I32(static_cast<std::int32_t>(packet.point));
  writer.I32(static_cast<std::int32_t>(packet.honor));
  writer.I32(static_cast<std::int32_t>(packet.virtue));
  writer.I32(static_cast<std::int32_t>(packet.emblemVersion));
  writer.FixedString(packet.guildname, 24);
  writer.FixedString(packet.manageLand, 16);
  writer.I32(static_cast<std::int32_t>(packet.zeny));
  writer.I32(static_cast<std::int32_t>(packet.masterGID));
  writer.FixedString(packet.masterName, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPositionIdNameInfo& packet) {
  if (frame.size() < ZcPositionIdNameInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPositionIdNameInfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  for (std::uint8_t& element : packet.posInfo) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPositionIdNameInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPositionIdNameInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPositionIdNameInfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const std::uint8_t element : packet.posInfo) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPositionInfo& packet) {
  if (frame.size() < ZcPositionInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPositionInfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcPositionInfoSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.posInfo.resize(reader.remaining() / ZcPositionInfoSub::kWireSize);
    for (ZcPositionInfoSub& element : packet.posInfo) {
      ReadZcPositionInfoSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPositionInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPositionInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPositionInfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcPositionInfoSub& element : packet.posInfo) {
    WriteZcPositionInfoSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGuildSkillinfo& packet) {
  if (frame.size() < ZcGuildSkillinfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGuildSkillinfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.skillPoint = static_cast<std::int16_t>(reader.I16());
  if (reader.remaining() % GuildSkilldata::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.skillInfo.resize(reader.remaining() / GuildSkilldata::kWireSize);
    for (GuildSkilldata& element : packet.skillInfo) {
      ReadGuildSkilldata(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGuildSkillinfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGuildSkillinfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGuildSkillinfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I16(static_cast<std::int16_t>(packet.skillPoint));
  for (const GuildSkilldata& element : packet.skillInfo) {
    WriteGuildSkilldata(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMyguildBasicInfo& packet) {
  if (frame.size() < ZcMyguildBasicInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMyguildBasicInfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % RelatedGuildInfo::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.rgInfo.resize(reader.remaining() / RelatedGuildInfo::kWireSize);
    for (RelatedGuildInfo& element : packet.rgInfo) {
      ReadRelatedGuildInfo(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMyguildBasicInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMyguildBasicInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMyguildBasicInfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const RelatedGuildInfo& element : packet.rgInfo) {
    WriteRelatedGuildInfo(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqUploadMacroDetector& packet) {
  if (frame.size() != CzReqUploadMacroDetector::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqUploadMacroDetector::kOpcode) {
    return false;
  }
  packet.answer = reader.FixedString(16);
  packet.imageSize = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqUploadMacroDetector& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqUploadMacroDetector::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqUploadMacroDetector::kOpcode);
  writer.FixedString(packet.answer, 16);
  writer.U16(static_cast<std::uint16_t>(packet.imageSize));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckUploadMacroDetector& packet) {
  if (frame.size() != ZcAckUploadMacroDetector::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckUploadMacroDetector::kOpcode) {
    return false;
  }
  packet.captchaKey = reader.FixedString(4);
  packet.captchaFlag = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckUploadMacroDetector& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckUploadMacroDetector::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckUploadMacroDetector::kOpcode);
  writer.FixedString(packet.captchaKey, 4);
  writer.I32(static_cast<std::int32_t>(packet.captchaFlag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzUploadMacroDetectorCaptcha& packet) {
  if (frame.size() < CzUploadMacroDetectorCaptcha::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzUploadMacroDetectorCaptcha::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.captchaKey = reader.FixedString(4);
  packet.imageData = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzUploadMacroDetectorCaptcha& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzUploadMacroDetectorCaptcha::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzUploadMacroDetectorCaptcha::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.FixedString(packet.captchaKey, 4);
  writer.Text(packet.imageData);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCompleteUploadMacroDetectorCaptcha& /*packet*/) {
  if (frame.size() != ZcCompleteUploadMacroDetectorCaptcha::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCompleteUploadMacroDetectorCaptcha::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCompleteUploadMacroDetectorCaptcha& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCompleteUploadMacroDetectorCaptcha::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCompleteUploadMacroDetectorCaptcha::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqApplyMacroDetector& packet) {
  if (frame.size() != CzReqApplyMacroDetector::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqApplyMacroDetector::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqApplyMacroDetector& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqApplyMacroDetector::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqApplyMacroDetector::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckApplyMacroDetector& packet) {
  if (frame.size() != ZcAckApplyMacroDetector::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckApplyMacroDetector::kOpcode) {
    return false;
  }
  packet.status = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckApplyMacroDetector& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckApplyMacroDetector::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckApplyMacroDetector::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.status));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcApplyMacroDetector& packet) {
  if (frame.size() != ZcApplyMacroDetector::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcApplyMacroDetector::kOpcode) {
    return false;
  }
  packet.imageSize = static_cast<std::uint16_t>(reader.U16());
  packet.captchaKey = reader.FixedString(4);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcApplyMacroDetector& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcApplyMacroDetector::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcApplyMacroDetector::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.imageSize));
  writer.FixedString(packet.captchaKey, 4);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcApplyMacroDetectorCaptcha& packet) {
  if (frame.size() < ZcApplyMacroDetectorCaptcha::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcApplyMacroDetectorCaptcha::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.captchaKey = reader.FixedString(4);
  packet.imageData = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcApplyMacroDetectorCaptcha& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcApplyMacroDetectorCaptcha::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcApplyMacroDetectorCaptcha::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.FixedString(packet.captchaKey, 4);
  writer.Text(packet.imageData);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzCompleteApplyMacroDetectorCaptcha& /*packet*/) {
  if (frame.size() != CzCompleteApplyMacroDetectorCaptcha::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzCompleteApplyMacroDetectorCaptcha::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzCompleteApplyMacroDetectorCaptcha& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzCompleteApplyMacroDetectorCaptcha::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzCompleteApplyMacroDetectorCaptcha::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcReqAnswerMacroDetector& packet) {
  if (frame.size() != ZcReqAnswerMacroDetector::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcReqAnswerMacroDetector::kOpcode) {
    return false;
  }
  packet.retryCount = static_cast<std::uint8_t>(reader.U8());
  packet.timeout = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcReqAnswerMacroDetector& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcReqAnswerMacroDetector::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcReqAnswerMacroDetector::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.retryCount));
  writer.I32(static_cast<std::int32_t>(packet.timeout));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzAckAnswerMacroDetector& packet) {
  if (frame.size() != CzAckAnswerMacroDetector::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzAckAnswerMacroDetector::kOpcode) {
    return false;
  }
  packet.answer = reader.FixedString(16);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzAckAnswerMacroDetector& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzAckAnswerMacroDetector::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzAckAnswerMacroDetector::kOpcode);
  writer.FixedString(packet.answer, 16);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCloseMacroDetector& packet) {
  if (frame.size() != ZcCloseMacroDetector::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCloseMacroDetector::kOpcode) {
    return false;
  }
  packet.status = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCloseMacroDetector& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCloseMacroDetector::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCloseMacroDetector::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.status));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqPreviewMacroDetector& packet) {
  if (frame.size() != CzReqPreviewMacroDetector::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqPreviewMacroDetector::kOpcode) {
    return false;
  }
  packet.captchaID = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqPreviewMacroDetector& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqPreviewMacroDetector::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqPreviewMacroDetector::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.captchaID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckPreviewMacroDetector& packet) {
  if (frame.size() != ZcAckPreviewMacroDetector::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckPreviewMacroDetector::kOpcode) {
    return false;
  }
  packet.captchaFlag = static_cast<std::int32_t>(reader.I32());
  packet.imageSize = static_cast<std::uint16_t>(reader.U16());
  packet.captchaKey = reader.FixedString(4);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckPreviewMacroDetector& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckPreviewMacroDetector::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckPreviewMacroDetector::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.captchaFlag));
  writer.U16(static_cast<std::uint16_t>(packet.imageSize));
  writer.FixedString(packet.captchaKey, 4);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPreviewMacroDetectorCaptcha& packet) {
  if (frame.size() < ZcPreviewMacroDetectorCaptcha::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPreviewMacroDetectorCaptcha::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.captchaKey = reader.FixedString(4);
  packet.imageData = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPreviewMacroDetectorCaptcha& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPreviewMacroDetectorCaptcha::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPreviewMacroDetectorCaptcha::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.FixedString(packet.captchaKey, 4);
  writer.Text(packet.imageData);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqPlayerAidInRange& packet) {
  if (frame.size() != CzReqPlayerAidInRange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqPlayerAidInRange::kOpcode) {
    return false;
  }
  packet.xPos = static_cast<std::int16_t>(reader.I16());
  packet.yPos = static_cast<std::int16_t>(reader.I16());
  packet.RadiusRange = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqPlayerAidInRange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqPlayerAidInRange::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqPlayerAidInRange::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.xPos));
  writer.I16(static_cast<std::int16_t>(packet.yPos));
  writer.I8(static_cast<std::int8_t>(packet.RadiusRange));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckPlayerAidInRange& packet) {
  if (frame.size() < ZcAckPlayerAidInRange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckPlayerAidInRange::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % 4 != 0) {
    reader.Invalidate();
  } else {
    packet.AID.resize(reader.remaining() / 4);
    for (std::uint32_t& element : packet.AID) {
      element = static_cast<std::uint32_t>(reader.U32());
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckPlayerAidInRange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckPlayerAidInRange::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckPlayerAidInRange::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const std::uint32_t element : packet.AID) {
    writer.U32(static_cast<std::uint32_t>(element));
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckMakeGroup& packet) {
  if (frame.size() != ZcAckMakeGroup::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckMakeGroup::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckMakeGroup& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckMakeGroup::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckMakeGroup::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPartyJoinReq& packet) {
  if (frame.size() != ZcPartyJoinReq::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPartyJoinReq::kOpcode) {
    return false;
  }
  packet.GRID = static_cast<std::int32_t>(reader.I32());
  packet.groupName = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPartyJoinReq& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPartyJoinReq::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPartyJoinReq::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.GRID));
  writer.FixedString(packet.groupName, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPartyJoinReqAck& packet) {
  if (frame.size() != ZcPartyJoinReqAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPartyJoinReqAck::kOpcode) {
    return false;
  }
  packet.characterName = reader.FixedString(24);
  packet.result = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPartyJoinReqAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPartyJoinReqAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPartyJoinReqAck::kOpcode);
  writer.FixedString(packet.characterName, 24);
  writer.I32(static_cast<std::int32_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyChatParty& packet) {
  if (frame.size() < ZcNotifyChatParty::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyChatParty::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.AID = static_cast<std::int32_t>(reader.I32());
  packet.chatMsg = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyChatParty& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyChatParty::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyChatParty::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I32(static_cast<std::int32_t>(packet.AID));
  writer.Text(packet.chatMsg);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyPositionToGroupm& packet) {
  if (frame.size() != ZcNotifyPositionToGroupm::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyPositionToGroupm::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::int32_t>(reader.I32());
  packet.xPos = static_cast<std::int16_t>(reader.I16());
  packet.yPos = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyPositionToGroupm& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyPositionToGroupm::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyPositionToGroupm::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.AID));
  writer.I16(static_cast<std::int16_t>(packet.xPos));
  writer.I16(static_cast<std::int16_t>(packet.yPos));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyHpToGroupm& packet) {
  if (frame.size() != ZcNotifyHpToGroupm::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyHpToGroupm::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.hp = static_cast<std::int32_t>(reader.I32());
  packet.maxhp = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyHpToGroupm& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyHpToGroupm::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyHpToGroupm::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.I32(static_cast<std::int32_t>(packet.hp));
  writer.I32(static_cast<std::int32_t>(packet.maxhp));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyMemberinfoToGroupm& packet) {
  if (frame.size() != ZcNotifyMemberinfoToGroupm::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyMemberinfoToGroupm::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::int32_t>(reader.I32());
  packet.job = static_cast<std::int16_t>(reader.I16());
  packet.level = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyMemberinfoToGroupm& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyMemberinfoToGroupm::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyMemberinfoToGroupm::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.AID));
  writer.I16(static_cast<std::int16_t>(packet.job));
  writer.I16(static_cast<std::int16_t>(packet.level));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDeleteMemberFromGroup& packet) {
  if (frame.size() != ZcDeleteMemberFromGroup::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDeleteMemberFromGroup::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::int32_t>(reader.I32());
  packet.characterName = reader.FixedString(24);
  packet.result = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDeleteMemberFromGroup& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDeleteMemberFromGroup::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDeleteMemberFromGroup::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.AID));
  writer.FixedString(packet.characterName, 24);
  writer.I8(static_cast<std::int8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqTakeoffEquipAll& packet) {
  if (frame.size() != CzReqTakeoffEquipAll::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqTakeoffEquipAll::kOpcode) {
    return false;
  }
  packet.location = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqTakeoffEquipAll& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqTakeoffEquipAll::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqTakeoffEquipAll::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.location));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckTakeoffEquipAll& packet) {
  if (frame.size() != ZcAckTakeoffEquipAll::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckTakeoffEquipAll::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckTakeoffEquipAll& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckTakeoffEquipAll::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckTakeoffEquipAll::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcBattlefieldNotifyHp& packet) {
  if (frame.size() != ZcBattlefieldNotifyHp::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcBattlefieldNotifyHp::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.hp = static_cast<std::int32_t>(reader.I32());
  packet.maxhp = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcBattlefieldNotifyHp& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcBattlefieldNotifyHp::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcBattlefieldNotifyHp::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.I32(static_cast<std::int32_t>(packet.hp));
  writer.I32(static_cast<std::int32_t>(packet.maxhp));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzChooseMenuZero& packet) {
  if (frame.size() != CzChooseMenuZero::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzChooseMenuZero::kOpcode) {
    return false;
  }
  packet.NpcID = static_cast<std::uint32_t>(reader.U32());
  packet.menuIndex = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzChooseMenuZero& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzChooseMenuZero::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzChooseMenuZero::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.NpcID));
  writer.U8(static_cast<std::uint8_t>(packet.menuIndex));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDialogTextAlign& packet) {
  if (frame.size() != ZcDialogTextAlign::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDialogTextAlign::kOpcode) {
    return false;
  }
  packet.align = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDialogTextAlign& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDialogTextAlign::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDialogTextAlign::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.align));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzGradeEnchantSelectEquipment& packet) {
  if (frame.size() != CzGradeEnchantSelectEquipment::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzGradeEnchantSelectEquipment::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzGradeEnchantSelectEquipment& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzGradeEnchantSelectEquipment::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzGradeEnchantSelectEquipment::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGradeEnchantMaterialList& packet) {
  if (frame.size() < ZcGradeEnchantMaterialList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGradeEnchantMaterialList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.success_chance = static_cast<std::int32_t>(reader.I32());
  ReadGradeEnchantBlessing(reader, packet.blessing_info);
  packet.protect_itemid = static_cast<std::int32_t>(reader.I32());
  packet.protect_amount = static_cast<std::int32_t>(reader.I32());
  if (reader.remaining() % GradeEnchantMaterial::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.material_info.resize(reader.remaining() / GradeEnchantMaterial::kWireSize);
    for (GradeEnchantMaterial& element : packet.material_info) {
      ReadGradeEnchantMaterial(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGradeEnchantMaterialList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGradeEnchantMaterialList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGradeEnchantMaterialList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.I32(static_cast<std::int32_t>(packet.success_chance));
  WriteGradeEnchantBlessing(writer, packet.blessing_info);
  writer.I32(static_cast<std::int32_t>(packet.protect_itemid));
  writer.I32(static_cast<std::int32_t>(packet.protect_amount));
  for (const GradeEnchantMaterial& element : packet.material_info) {
    WriteGradeEnchantMaterial(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzGradeEnchantRequest& packet) {
  if (frame.size() != CzGradeEnchantRequest::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzGradeEnchantRequest::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.material_index = static_cast<std::int32_t>(reader.I32());
  packet.blessing_flag = static_cast<std::int8_t>(reader.I8());
  packet.blessing_amount = static_cast<std::int32_t>(reader.I32());
  packet.protect_flag = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzGradeEnchantRequest& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzGradeEnchantRequest::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzGradeEnchantRequest::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.I32(static_cast<std::int32_t>(packet.material_index));
  writer.I8(static_cast<std::int8_t>(packet.blessing_flag));
  writer.I32(static_cast<std::int32_t>(packet.blessing_amount));
  writer.I8(static_cast<std::int8_t>(packet.protect_flag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzGradeEnchantCloseUi& /*packet*/) {
  if (frame.size() != CzGradeEnchantCloseUi::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzGradeEnchantCloseUi::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzGradeEnchantCloseUi& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzGradeEnchantCloseUi::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzGradeEnchantCloseUi::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGradeEnchantAck& packet) {
  if (frame.size() != ZcGradeEnchantAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGradeEnchantAck::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.grade = static_cast<std::int16_t>(reader.I16());
  packet.result = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGradeEnchantAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGradeEnchantAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGradeEnchantAck::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.I16(static_cast<std::int16_t>(packet.grade));
  writer.I32(static_cast<std::int32_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGradeEnchantBroadcastResult& packet) {
  if (frame.size() != ZcGradeEnchantBroadcastResult::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGradeEnchantBroadcastResult::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.grade = static_cast<std::int16_t>(reader.I16());
  packet.status = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGradeEnchantBroadcastResult& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGradeEnchantBroadcastResult::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGradeEnchantBroadcastResult::kOpcode);
  writer.FixedString(packet.name, 24);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.I16(static_cast<std::int16_t>(packet.grade));
  writer.I8(static_cast<std::int8_t>(packet.status));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcShowImage& packet) {
  if (frame.size() != ZcShowImage::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcShowImage::kOpcode) {
    return false;
  }
  packet.image = reader.FixedString(64);
  packet.type = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcShowImage& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcShowImage::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcShowImage::kOpcode);
  writer.FixedString(packet.image, 64);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUpdateGdid& packet) {
  if (frame.size() != ZcUpdateGdid::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUpdateGdid::kOpcode) {
    return false;
  }
  packet.guildId = static_cast<std::uint32_t>(reader.U32());
  packet.emblemVersion = static_cast<std::int32_t>(reader.I32());
  packet.mode = static_cast<std::uint32_t>(reader.U32());
  packet.isMaster = static_cast<std::uint8_t>(reader.U8());
  packet.interSid = static_cast<std::int32_t>(reader.I32());
  packet.guildName = reader.FixedString(24);
  packet.masterGID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUpdateGdid& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUpdateGdid::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUpdateGdid::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.guildId));
  writer.I32(static_cast<std::int32_t>(packet.emblemVersion));
  writer.U32(static_cast<std::uint32_t>(packet.mode));
  writer.U8(static_cast<std::uint8_t>(packet.isMaster));
  writer.I32(static_cast<std::int32_t>(packet.interSid));
  writer.FixedString(packet.guildName, 24);
  writer.U32(static_cast<std::uint32_t>(packet.masterGID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzApproximateActor& packet) {
  if (frame.size() != CzApproximateActor::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzApproximateActor::kOpcode) {
    return false;
  }
  packet.masterGID = static_cast<std::uint32_t>(reader.U32());
  packet.unused1 = static_cast<std::uint16_t>(reader.U16());
  packet.unused2 = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzApproximateActor& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzApproximateActor::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzApproximateActor::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.masterGID));
  writer.U16(static_cast<std::uint16_t>(packet.unused1));
  writer.U8(static_cast<std::uint8_t>(packet.unused2));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzContactnpc& packet) {
  if (frame.size() != CzContactnpc::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzContactnpc::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzContactnpc& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzContactnpc::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzContactnpc::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAttackFailureForDistance& packet) {
  if (frame.size() != ZcAttackFailureForDistance::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAttackFailureForDistance::kOpcode) {
    return false;
  }
  packet.targetAID = static_cast<std::uint32_t>(reader.U32());
  packet.targetXPos = static_cast<std::int16_t>(reader.I16());
  packet.targetYPos = static_cast<std::int16_t>(reader.I16());
  packet.xPos = static_cast<std::int16_t>(reader.I16());
  packet.yPos = static_cast<std::int16_t>(reader.I16());
  packet.currentAttRange = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAttackFailureForDistance& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAttackFailureForDistance::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAttackFailureForDistance::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.targetAID));
  writer.I16(static_cast<std::int16_t>(packet.targetXPos));
  writer.I16(static_cast<std::int16_t>(packet.targetYPos));
  writer.I16(static_cast<std::int16_t>(packet.xPos));
  writer.I16(static_cast<std::int16_t>(packet.yPos));
  writer.I16(static_cast<std::int16_t>(packet.currentAttRange));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcStartCapture& /*packet*/) {
  if (frame.size() != ZcStartCapture::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcStartCapture::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcStartCapture& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcStartCapture::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcStartCapture::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcTrycaptureMonster& packet) {
  if (frame.size() != ZcTrycaptureMonster::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcTrycaptureMonster::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcTrycaptureMonster& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcTrycaptureMonster::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcTrycaptureMonster::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPropertyPet& packet) {
  if (frame.size() != ZcPropertyPet::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPropertyPet::kOpcode) {
    return false;
  }
  packet.szName = reader.FixedString(24);
  packet.bModified = static_cast<std::int8_t>(reader.I8());
  packet.nLevel = static_cast<std::int16_t>(reader.I16());
  packet.nFullness = static_cast<std::int16_t>(reader.I16());
  packet.nRelationship = static_cast<std::int16_t>(reader.I16());
  packet.ITID = static_cast<std::int16_t>(reader.I16());
  packet.job = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPropertyPet& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPropertyPet::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPropertyPet::kOpcode);
  writer.FixedString(packet.szName, 24);
  writer.I8(static_cast<std::int8_t>(packet.bModified));
  writer.I16(static_cast<std::int16_t>(packet.nLevel));
  writer.I16(static_cast<std::int16_t>(packet.nFullness));
  writer.I16(static_cast<std::int16_t>(packet.nRelationship));
  writer.I16(static_cast<std::int16_t>(packet.ITID));
  writer.I16(static_cast<std::int16_t>(packet.job));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcChangestatePet& packet) {
  if (frame.size() != ZcChangestatePet::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcChangestatePet::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::int8_t>(reader.I8());
  packet.GID = static_cast<std::int32_t>(reader.I32());
  packet.data = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcChangestatePet& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcChangestatePet::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcChangestatePet::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.type));
  writer.I32(static_cast<std::int32_t>(packet.GID));
  writer.I32(static_cast<std::int32_t>(packet.data));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSpirits& packet) {
  if (frame.size() != ZcSpirits::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSpirits::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.num = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSpirits& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSpirits::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSpirits::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.I16(static_cast<std::int16_t>(packet.num));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSpirits2& packet) {
  if (frame.size() != ZcSpirits2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSpirits2::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.num = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSpirits2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSpirits2::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSpirits2::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.I16(static_cast<std::int16_t>(packet.num));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSoulenergy& packet) {
  if (frame.size() != ZcSoulenergy::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSoulenergy::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.num = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSoulenergy& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSoulenergy::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSoulenergy::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U16(static_cast<std::uint16_t>(packet.num));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSayDialog& packet) {
  if (frame.size() < ZcSayDialog::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSayDialog::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.NpcID = static_cast<std::uint32_t>(reader.U32());
  packet.message = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSayDialog& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSayDialog::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSayDialog::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.NpcID));
  writer.Text(packet.message);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSayDialog2& packet) {
  if (frame.size() < ZcSayDialog2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSayDialog2::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.NpcID = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.message = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSayDialog2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSayDialog2::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSayDialog2::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.NpcID));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.Text(packet.message);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcWaitDialog& packet) {
  if (frame.size() != ZcWaitDialog::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcWaitDialog::kOpcode) {
    return false;
  }
  packet.NpcID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcWaitDialog& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcWaitDialog::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcWaitDialog::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.NpcID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcWaitDialog2& packet) {
  if (frame.size() != ZcWaitDialog2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcWaitDialog2::kOpcode) {
    return false;
  }
  packet.NpcID = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcWaitDialog2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcWaitDialog2::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcWaitDialog2::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.NpcID));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDialogWindowSize& packet) {
  if (frame.size() != ZcDialogWindowSize::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDialogWindowSize::kOpcode) {
    return false;
  }
  packet.height = static_cast<std::int32_t>(reader.I32());
  packet.width = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDialogWindowSize& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDialogWindowSize::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDialogWindowSize::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.height));
  writer.I32(static_cast<std::int32_t>(packet.width));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDialogWindowPos& packet) {
  if (frame.size() != ZcDialogWindowPos::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDialogWindowPos::kOpcode) {
    return false;
  }
  packet.x = static_cast<std::int32_t>(reader.I32());
  packet.y = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDialogWindowPos& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDialogWindowPos::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDialogWindowPos::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.x));
  writer.I32(static_cast<std::int32_t>(packet.y));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDialogWindowPos2& packet) {
  if (frame.size() != ZcDialogWindowPos2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDialogWindowPos2::kOpcode) {
    return false;
  }
  packet.x = static_cast<std::int32_t>(reader.I32());
  packet.y = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDialogWindowPos2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDialogWindowPos2::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDialogWindowPos2::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.x));
  writer.I32(static_cast<std::int32_t>(packet.y));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPlayNpcBgm& packet) {
  if (frame.size() < ZcPlayNpcBgm::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPlayNpcBgm::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.playType = static_cast<std::uint8_t>(reader.U8());
  packet.bgm = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPlayNpcBgm& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPlayNpcBgm::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPlayNpcBgm::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.playType));
  writer.Text(packet.bgm);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoveItemFromBodyToCart& packet) {
  if (frame.size() != CzMoveItemFromBodyToCart::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoveItemFromBodyToCart::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.count = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoveItemFromBodyToCart& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoveItemFromBodyToCart::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoveItemFromBodyToCart::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.I32(static_cast<std::int32_t>(packet.count));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSound& packet) {
  if (frame.size() != ZcSound::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSound::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  packet.act = static_cast<std::uint8_t>(reader.U8());
  packet.term = static_cast<std::uint32_t>(reader.U32());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSound& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSound::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSound::kOpcode);
  writer.FixedString(packet.name, 24);
  writer.U8(static_cast<std::uint8_t>(packet.act));
  writer.U32(static_cast<std::uint32_t>(packet.term));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcBuyingStoreEntry& packet) {
  if (frame.size() != ZcBuyingStoreEntry::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcBuyingStoreEntry::kOpcode) {
    return false;
  }
  packet.makerAID = static_cast<std::uint32_t>(reader.U32());
  packet.storeName = reader.FixedString(80);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcBuyingStoreEntry& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcBuyingStoreEntry::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcBuyingStoreEntry::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.makerAID));
  writer.FixedString(packet.storeName, 80);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcStoreEntry& packet) {
  if (frame.size() != ZcStoreEntry::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcStoreEntry::kOpcode) {
    return false;
  }
  packet.makerAID = static_cast<std::uint32_t>(reader.U32());
  packet.storeName = reader.FixedString(80);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcStoreEntry& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcStoreEntry::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcStoreEntry::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.makerAID));
  writer.FixedString(packet.storeName, 80);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzPcPurchaseItemlistFrommc& packet) {
  if (frame.size() < CzPcPurchaseItemlistFrommc::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzPcPurchaseItemlistFrommc::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  if (reader.remaining() % CzPurchaseItemFrommc::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / CzPurchaseItemFrommc::kWireSize);
    for (CzPurchaseItemFrommc& element : packet.list) {
      ReadCzPurchaseItemFrommc(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzPcPurchaseItemlistFrommc& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzPcPurchaseItemlistFrommc::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzPcPurchaseItemlistFrommc::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  for (const CzPurchaseItemFrommc& element : packet.list) {
    WriteCzPurchaseItemFrommc(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzPcPurchaseItemlistFrommc2& packet) {
  if (frame.size() < CzPcPurchaseItemlistFrommc2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzPcPurchaseItemlistFrommc2::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.UniqueID = static_cast<std::uint32_t>(reader.U32());
  if (reader.remaining() % CzPurchaseItemFrommc::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / CzPurchaseItemFrommc::kWireSize);
    for (CzPurchaseItemFrommc& element : packet.list) {
      ReadCzPurchaseItemFrommc(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzPcPurchaseItemlistFrommc2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzPcPurchaseItemlistFrommc2::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzPcPurchaseItemlistFrommc2::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.UniqueID));
  for (const CzPurchaseItemFrommc& element : packet.list) {
    WriteCzPurchaseItemFrommc(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDisappearBuyingStoreEntry& packet) {
  if (frame.size() != ZcDisappearBuyingStoreEntry::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDisappearBuyingStoreEntry::kOpcode) {
    return false;
  }
  packet.makerAID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDisappearBuyingStoreEntry& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDisappearBuyingStoreEntry::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDisappearBuyingStoreEntry::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.makerAID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcOpenReformUi& packet) {
  if (frame.size() != ZcOpenReformUi::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcOpenReformUi::kOpcode) {
    return false;
  }
  packet.ITID = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcOpenReformUi& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcOpenReformUi::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcOpenReformUi::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.ITID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzCloseReformUi& /*packet*/) {
  if (frame.size() != CzCloseReformUi::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzCloseReformUi::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzCloseReformUi& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzCloseReformUi::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzCloseReformUi::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzItemReform& packet) {
  if (frame.size() != CzItemReform::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzItemReform::kOpcode) {
    return false;
  }
  packet.ITID = static_cast<std::int32_t>(reader.I32());
  packet.index = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzItemReform& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzItemReform::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzItemReform::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.ITID));
  writer.I16(static_cast<std::int16_t>(packet.index));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcItemReformAck& packet) {
  if (frame.size() != ZcItemReformAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcItemReformAck::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.result = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcItemReformAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcItemReformAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcItemReformAck::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.I8(static_cast<std::int8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzUsePackageitem& packet) {
  if (frame.size() != CzUsePackageitem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzUsePackageitem::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.itemID = static_cast<std::uint32_t>(reader.U32());
  packet.BoxIndex = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzUsePackageitem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzUsePackageitem::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzUsePackageitem::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.itemID));
  writer.U32(static_cast<std::uint32_t>(packet.BoxIndex));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzRequestRandomEnchant& packet) {
  if (frame.size() != CzRequestRandomEnchant::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzRequestRandomEnchant::kOpcode) {
    return false;
  }
  packet.enchant_group = static_cast<std::int64_t>(reader.I64());
  packet.index = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzRequestRandomEnchant& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzRequestRandomEnchant::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzRequestRandomEnchant::kOpcode);
  writer.I64(static_cast<std::int64_t>(packet.enchant_group));
  writer.I16(static_cast<std::int16_t>(packet.index));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzRequestPerfectEnchant& packet) {
  if (frame.size() != CzRequestPerfectEnchant::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzRequestPerfectEnchant::kOpcode) {
    return false;
  }
  packet.enchant_group = static_cast<std::int64_t>(reader.I64());
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.ITID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzRequestPerfectEnchant& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzRequestPerfectEnchant::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzRequestPerfectEnchant::kOpcode);
  writer.I64(static_cast<std::int64_t>(packet.enchant_group));
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.ITID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzRequestUpgradeEnchant& packet) {
  if (frame.size() != CzRequestUpgradeEnchant::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzRequestUpgradeEnchant::kOpcode) {
    return false;
  }
  packet.enchant_group = static_cast<std::int64_t>(reader.I64());
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.slot = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzRequestUpgradeEnchant& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzRequestUpgradeEnchant::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzRequestUpgradeEnchant::kOpcode);
  writer.I64(static_cast<std::int64_t>(packet.enchant_group));
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.I16(static_cast<std::int16_t>(packet.slot));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzRequestResetEnchant& packet) {
  if (frame.size() != CzRequestResetEnchant::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzRequestResetEnchant::kOpcode) {
    return false;
  }
  packet.enchant_group = static_cast<std::int64_t>(reader.I64());
  packet.index = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzRequestResetEnchant& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzRequestResetEnchant::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzRequestResetEnchant::kOpcode);
  writer.I64(static_cast<std::int64_t>(packet.enchant_group));
  writer.I16(static_cast<std::int16_t>(packet.index));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcResponseEnchant& packet) {
  if (frame.size() != ZcResponseEnchant::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcResponseEnchant::kOpcode) {
    return false;
  }
  packet.msgId = static_cast<std::int32_t>(reader.I32());
  packet.ITID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcResponseEnchant& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcResponseEnchant::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcResponseEnchant::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.msgId));
  writer.U32(static_cast<std::uint32_t>(packet.ITID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzCloseUiEnchant& /*packet*/) {
  if (frame.size() != CzCloseUiEnchant::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzCloseUiEnchant::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzCloseUiEnchant& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzCloseUiEnchant::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzCloseUiEnchant::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSpecialpopup& packet) {
  if (frame.size() != ZcSpecialpopup::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSpecialpopup::kOpcode) {
    return false;
  }
  packet.ppId = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSpecialpopup& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSpecialpopup::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSpecialpopup::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.ppId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGoldpccafePoint& packet) {
  if (frame.size() != ZcGoldpccafePoint::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGoldpccafePoint::kOpcode) {
    return false;
  }
  packet.isActive = static_cast<std::int8_t>(reader.I8());
  packet.mode = static_cast<std::int8_t>(reader.I8());
  packet.point = static_cast<std::int32_t>(reader.I32());
  packet.playedTime = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGoldpccafePoint& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGoldpccafePoint::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGoldpccafePoint::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.isActive));
  writer.I8(static_cast<std::int8_t>(packet.mode));
  writer.I32(static_cast<std::int32_t>(packet.point));
  writer.I32(static_cast<std::int32_t>(packet.playedTime));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzDynamicnpcCreateRequest& packet) {
  if (frame.size() != CzDynamicnpcCreateRequest::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzDynamicnpcCreateRequest::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzDynamicnpcCreateRequest& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzDynamicnpcCreateRequest::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzDynamicnpcCreateRequest::kOpcode);
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDynamicnpcCreateResult& packet) {
  if (frame.size() != ZcDynamicnpcCreateResult::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDynamicnpcCreateResult::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDynamicnpcCreateResult& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDynamicnpcCreateResult::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDynamicnpcCreateResult::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqGuildEmblemImg1& packet) {
  if (frame.size() != CzReqGuildEmblemImg1::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqGuildEmblemImg1::kOpcode) {
    return false;
  }
  packet.guild_id = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqGuildEmblemImg1& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqGuildEmblemImg1::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqGuildEmblemImg1::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.guild_id));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqGuildEmblemImg3& packet) {
  if (frame.size() != CzReqGuildEmblemImg3::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqGuildEmblemImg3::kOpcode) {
    return false;
  }
  packet.guild_id = static_cast<std::int32_t>(reader.I32());
  packet.emblem_id = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqGuildEmblemImg3& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqGuildEmblemImg3::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqGuildEmblemImg3::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.guild_id));
  writer.I32(static_cast<std::int32_t>(packet.emblem_id));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqGuildEmblemImg2& packet) {
  if (frame.size() != CzReqGuildEmblemImg2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqGuildEmblemImg2::kOpcode) {
    return false;
  }
  packet.guild_id = static_cast<std::int32_t>(reader.I32());
  packet.emblem_id = static_cast<std::int32_t>(reader.I32());
  packet.unused = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqGuildEmblemImg2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqGuildEmblemImg2::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqGuildEmblemImg2::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.guild_id));
  writer.I32(static_cast<std::int32_t>(packet.emblem_id));
  writer.I32(static_cast<std::int32_t>(packet.unused));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcChangeGuild& packet) {
  if (frame.size() != ZcChangeGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcChangeGuild::kOpcode) {
    return false;
  }
  packet.guild_id = static_cast<std::int32_t>(reader.I32());
  packet.emblem_id = static_cast<std::uint32_t>(reader.U32());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcChangeGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcChangeGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcChangeGuild::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.guild_id));
  writer.U32(static_cast<std::uint32_t>(packet.emblem_id));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGuildEmblemImg& packet) {
  if (frame.size() < ZcGuildEmblemImg::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGuildEmblemImg::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.result = static_cast<std::uint16_t>(reader.U16());
  packet.guild_id = static_cast<std::int32_t>(reader.I32());
  packet.emblem_id = static_cast<std::uint32_t>(reader.U32());
  packet.emblem_data = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGuildEmblemImg& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGuildEmblemImg::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGuildEmblemImg::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.result));
  writer.I32(static_cast<std::int32_t>(packet.guild_id));
  writer.U32(static_cast<std::uint32_t>(packet.emblem_id));
  writer.Text(packet.emblem_data);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzAdventurerAgencyJoinReq& packet) {
  if (frame.size() != CzAdventurerAgencyJoinReq::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzAdventurerAgencyJoinReq::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::int32_t>(reader.I32());
  packet.AID = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzAdventurerAgencyJoinReq& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzAdventurerAgencyJoinReq::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzAdventurerAgencyJoinReq::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.GID));
  writer.I32(static_cast<std::int32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAdventurerAgencyJoinResult& packet) {
  if (frame.size() != ZcAdventurerAgencyJoinResult::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAdventurerAgencyJoinResult::kOpcode) {
    return false;
  }
  packet.player_name = reader.FixedString(24);
  packet.party_name = reader.FixedString(24);
  packet.AID = static_cast<std::int32_t>(reader.I32());
  packet.result = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAdventurerAgencyJoinResult& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAdventurerAgencyJoinResult::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAdventurerAgencyJoinResult::kOpcode);
  writer.FixedString(packet.player_name, 24);
  writer.FixedString(packet.party_name, 24);
  writer.I32(static_cast<std::int32_t>(packet.AID));
  writer.I32(static_cast<std::int32_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAdventurerAgencyJoinReq& packet) {
  if (frame.size() != ZcAdventurerAgencyJoinReq::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAdventurerAgencyJoinReq::kOpcode) {
    return false;
  }
  packet.GRID = static_cast<std::int32_t>(reader.I32());
  packet.AID = static_cast<std::int32_t>(reader.I32());
  packet.groupName = reader.FixedString(24);
  packet.level = static_cast<std::int16_t>(reader.I16());
  packet.job = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAdventurerAgencyJoinReq& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAdventurerAgencyJoinReq::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAdventurerAgencyJoinReq::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.GRID));
  writer.I32(static_cast<std::int32_t>(packet.AID));
  writer.FixedString(packet.groupName, 24);
  writer.I16(static_cast<std::int16_t>(packet.level));
  writer.I16(static_cast<std::int16_t>(packet.job));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzAdventurerAgencyJoinResult& packet) {
  if (frame.size() != CzAdventurerAgencyJoinResult::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzAdventurerAgencyJoinResult::kOpcode) {
    return false;
  }
  packet.GRID = static_cast<std::int32_t>(reader.I32());
  packet.AID = static_cast<std::int32_t>(reader.I32());
  packet.result = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzAdventurerAgencyJoinResult& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzAdventurerAgencyJoinResult::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzAdventurerAgencyJoinResult::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.GRID));
  writer.I32(static_cast<std::int32_t>(packet.AID));
  writer.I8(static_cast<std::int8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightSettings& packet) {
  if (frame.size() < ZcMoonlightSettings::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightSettings::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.char_id = static_cast<std::uint32_t>(reader.U32());
  packet.count = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightSettings& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightSettings::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightSettings::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.char_id));
  writer.I16(static_cast<std::int16_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightSetting& packet) {
  if (frame.size() != CzMoonlightSetting::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightSetting::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.id = static_cast<std::int16_t>(reader.I16());
  packet.value = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightSetting& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightSetting::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightSetting::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.I16(static_cast<std::int16_t>(packet.id));
  writer.U32(static_cast<std::uint32_t>(packet.value));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightIntegrity& packet) {
  if (frame.size() < CzMoonlightIntegrity::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightIntegrity::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  for (std::uint8_t& element : packet.hash) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  packet.machine_guid = reader.FixedString(36);
  packet.patch_index = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightIntegrity& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightIntegrity::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightIntegrity::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const std::uint8_t element : packet.hash) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  writer.FixedString(packet.machine_guid, 36);
  writer.I32(static_cast<std::int32_t>(packet.patch_index));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonclientIntegrity& packet) {
  if (frame.size() < CzMoonclientIntegrity::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonclientIntegrity::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.machine_guid = reader.FixedString(36);
  packet.patch_index = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonclientIntegrity& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonclientIntegrity::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonclientIntegrity::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.FixedString(packet.machine_guid, 36);
  writer.I32(static_cast<std::int32_t>(packet.patch_index));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightKickNotice& /*packet*/) {
  if (frame.size() < ZcMoonlightKickNotice::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightKickNotice::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightKickNotice& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightKickNotice::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightKickNotice::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightDiscordMsg& packet) {
  if (frame.size() < ZcMoonlightDiscordMsg::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightDiscordMsg::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.msg = reader.FixedString(1);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightDiscordMsg& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightDiscordMsg::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightDiscordMsg::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.FixedString(packet.msg, 1);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightPresetCmd& packet) {
  if (frame.size() < CzMoonlightPresetCmd::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightPresetCmd::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.cmd = static_cast<std::uint8_t>(reader.U8());
  packet.no = static_cast<std::uint8_t>(reader.U8());
  packet.name = reader.FixedString(1);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightPresetCmd& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightPresetCmd::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightPresetCmd::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.cmd));
  writer.U8(static_cast<std::uint8_t>(packet.no));
  writer.FixedString(packet.name, 1);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightPresetList& packet) {
  if (frame.size() < ZcMoonlightPresetList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightPresetList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.active_no = static_cast<std::uint8_t>(reader.U8());
  packet.count = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightPresetList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightPresetList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightPresetList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.active_no));
  writer.U8(static_cast<std::uint8_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightSkillDmg& packet) {
  if (frame.size() < ZcMoonlightSkillDmg::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightSkillDmg::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.src_aid = static_cast<std::uint32_t>(reader.U32());
  packet.damage = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightSkillDmg& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightSkillDmg::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightSkillDmg::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.src_aid));
  writer.I32(static_cast<std::int32_t>(packet.damage));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightCheatReport& packet) {
  if (frame.size() != CzMoonlightCheatReport::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightCheatReport::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.tool_name = reader.FixedString(32);
  packet.detail = reader.FixedString(64);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightCheatReport& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightCheatReport::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightCheatReport::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.FixedString(packet.tool_name, 32);
  writer.FixedString(packet.detail, 64);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqTechdata& packet) {
  if (frame.size() != CzMoonlightReqTechdata::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightReqTechdata::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.id = static_cast<std::uint32_t>(reader.U32());
  packet.is_skill = static_cast<std::uint8_t>(reader.U8());
  packet.scope = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightReqTechdata& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightReqTechdata::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightReqTechdata::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U32(static_cast<std::uint32_t>(packet.id));
  writer.U8(static_cast<std::uint8_t>(packet.is_skill));
  writer.U8(static_cast<std::uint8_t>(packet.scope));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightTechdata& packet) {
  if (frame.size() < ZcMoonlightTechdata::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightTechdata::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.id = static_cast<std::uint32_t>(reader.U32());
  packet.is_skill = static_cast<std::uint8_t>(reader.U8());
  packet.scope = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightTechdata& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightTechdata::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightTechdata::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.id));
  writer.U8(static_cast<std::uint8_t>(packet.is_skill));
  writer.U8(static_cast<std::uint8_t>(packet.scope));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqDamage& packet) {
  if (frame.size() != CzMoonlightReqDamage::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightReqDamage::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.skill_id = static_cast<std::uint32_t>(reader.U32());
  packet.skill_lv = static_cast<std::uint16_t>(reader.U16());
  packet.target_mob_id = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightReqDamage& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightReqDamage::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightReqDamage::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U32(static_cast<std::uint32_t>(packet.skill_id));
  writer.U16(static_cast<std::uint16_t>(packet.skill_lv));
  writer.U32(static_cast<std::uint32_t>(packet.target_mob_id));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightDamage& packet) {
  if (frame.size() < ZcMoonlightDamage::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightDamage::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.skill_id = static_cast<std::uint32_t>(reader.U32());
  packet.skill_lv = static_cast<std::uint16_t>(reader.U16());
  packet.target_mob_id = static_cast<std::uint32_t>(reader.U32());
  packet.status = static_cast<std::uint8_t>(reader.U8());
  packet.atk_type = static_cast<std::uint8_t>(reader.U8());
  packet.hits = static_cast<std::uint16_t>(reader.U16());
  packet.dmg_min = static_cast<std::int64_t>(reader.I64());
  packet.dmg_max = static_cast<std::int64_t>(reader.I64());
  packet.dmg_avg = static_cast<std::int64_t>(reader.I64());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightDamage& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightDamage::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightDamage::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.skill_id));
  writer.U16(static_cast<std::uint16_t>(packet.skill_lv));
  writer.U32(static_cast<std::uint32_t>(packet.target_mob_id));
  writer.U8(static_cast<std::uint8_t>(packet.status));
  writer.U8(static_cast<std::uint8_t>(packet.atk_type));
  writer.U16(static_cast<std::uint16_t>(packet.hits));
  writer.I64(static_cast<std::int64_t>(packet.dmg_min));
  writer.I64(static_cast<std::int64_t>(packet.dmg_max));
  writer.I64(static_cast<std::int64_t>(packet.dmg_avg));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightStoragePrices& packet) {
  if (frame.size() < ZcMoonlightStoragePrices::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightStoragePrices::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.count = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightStoragePrices& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightStoragePrices::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightStoragePrices::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I16(static_cast<std::int16_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqCompatCards& packet) {
  if (frame.size() != CzMoonlightReqCompatCards::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightReqCompatCards::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.index_equip = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightReqCompatCards& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightReqCompatCards::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightReqCompatCards::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U16(static_cast<std::uint16_t>(packet.index_equip));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightCompatCards& packet) {
  if (frame.size() < ZcMoonlightCompatCards::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightCompatCards::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.index_equip = static_cast<std::uint16_t>(reader.U16());
  packet.count = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightCompatCards& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightCompatCards::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightCompatCards::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.index_equip));
  writer.I16(static_cast<std::int16_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightJump& packet) {
  if (frame.size() != CzMoonlightJump::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightJump::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightJump& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightJump::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightJump::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightJump& packet) {
  if (frame.size() < ZcMoonlightJump::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightJump::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightJump& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightJump::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightJump::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightCookMastery& packet) {
  if (frame.size() < ZcMoonlightCookMastery::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightCookMastery::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.mastery = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightCookMastery& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightCookMastery::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightCookMastery::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I16(static_cast<std::int16_t>(packet.mastery));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightOpenStorage& packet) {
  if (frame.size() != CzMoonlightOpenStorage::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightOpenStorage::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.stor_id = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightOpenStorage& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightOpenStorage::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightOpenStorage::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U8(static_cast<std::uint8_t>(packet.stor_id));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightStorageList& packet) {
  if (frame.size() < ZcMoonlightStorageList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightStorageList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.cur_id = static_cast<std::uint8_t>(reader.U8());
  packet.count = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightStorageList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightStorageList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightStorageList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.cur_id));
  writer.U8(static_cast<std::uint8_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqMobinfo& packet) {
  if (frame.size() != CzMoonlightReqMobinfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightReqMobinfo::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.mob_id = static_cast<std::uint32_t>(reader.U32());
  packet.by_view = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightReqMobinfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightReqMobinfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightReqMobinfo::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U32(static_cast<std::uint32_t>(packet.mob_id));
  writer.U8(static_cast<std::uint8_t>(packet.by_view));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightMobinfo& packet) {
  if (frame.size() < ZcMoonlightMobinfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightMobinfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.mob_id = static_cast<std::uint32_t>(reader.U32());
  packet.status = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightMobinfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightMobinfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightMobinfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.mob_id));
  writer.U8(static_cast<std::uint8_t>(packet.status));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightChannelList& packet) {
  if (frame.size() < ZcMoonlightChannelList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightChannelList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.count = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightChannelList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightChannelList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightChannelList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqEntityProps& packet) {
  if (frame.size() != CzMoonlightReqEntityProps::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightReqEntityProps::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightReqEntityProps& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightReqEntityProps::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightReqEntityProps::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightEntityProps& packet) {
  if (frame.size() < ZcMoonlightEntityProps::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightEntityProps::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  packet.status = static_cast<std::uint8_t>(reader.U8());
  packet.count = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightEntityProps& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightEntityProps::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightEntityProps::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  writer.U8(static_cast<std::uint8_t>(packet.status));
  writer.U8(static_cast<std::uint8_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightUiCaps& packet) {
  if (frame.size() < CzMoonlightUiCaps::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightUiCaps::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.caps = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightUiCaps& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightUiCaps::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightUiCaps::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.caps));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightNpcAdmin& packet) {
  if (frame.size() != CzMoonlightNpcAdmin::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightNpcAdmin::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  packet.action = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightNpcAdmin& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightNpcAdmin::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightNpcAdmin::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  writer.U8(static_cast<std::uint8_t>(packet.action));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightPlayerAdmin& packet) {
  if (frame.size() != CzMoonlightPlayerAdmin::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightPlayerAdmin::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.aid = static_cast<std::uint32_t>(reader.U32());
  packet.action = static_cast<std::uint8_t>(reader.U8());
  packet.param = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightPlayerAdmin& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightPlayerAdmin::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightPlayerAdmin::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U32(static_cast<std::uint32_t>(packet.aid));
  writer.U8(static_cast<std::uint8_t>(packet.action));
  writer.I32(static_cast<std::int32_t>(packet.param));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightStyle& packet) {
  if (frame.size() != CzMoonlightStyle::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightStyle::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.version = static_cast<std::uint8_t>(reader.U8());
  packet.flags = static_cast<std::uint8_t>(reader.U8());
  packet.body_key = static_cast<std::uint32_t>(reader.U32());
  packet.palette_id = static_cast<std::int16_t>(reader.I16());
  packet.hair_palette_id = static_cast<std::int16_t>(reader.I16());
  packet.hair_style = static_cast<std::int16_t>(reader.I16());
  for (std::uint8_t& element : packet.adjusts) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightStyle& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightStyle::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightStyle::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U8(static_cast<std::uint8_t>(packet.version));
  writer.U8(static_cast<std::uint8_t>(packet.flags));
  writer.U32(static_cast<std::uint32_t>(packet.body_key));
  writer.I16(static_cast<std::int16_t>(packet.palette_id));
  writer.I16(static_cast<std::int16_t>(packet.hair_palette_id));
  writer.I16(static_cast<std::int16_t>(packet.hair_style));
  for (const std::uint8_t element : packet.adjusts) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightStyles& packet) {
  if (frame.size() < ZcMoonlightStyles::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightStyles::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.count = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightStyles& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightStyles::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightStyles::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I16(static_cast<std::int16_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightStyleOpen& packet) {
  if (frame.size() < ZcMoonlightStyleOpen::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightStyleOpen::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.mode = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightStyleOpen& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightStyleOpen::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightStyleOpen::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.mode));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightTargetInfo& packet) {
  if (frame.size() != CzMoonlightTargetInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightTargetInfo::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightTargetInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightTargetInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightTargetInfo::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightTargetInfo& packet) {
  if (frame.size() < ZcMoonlightTargetInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightTargetInfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  packet.status = static_cast<std::uint8_t>(reader.U8());
  packet.known = static_cast<std::uint8_t>(reader.U8());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.level = static_cast<std::int16_t>(reader.I16());
  packet.hp = static_cast<std::uint32_t>(reader.U32());
  packet.maxhp = static_cast<std::uint32_t>(reader.U32());
  packet.sp = static_cast<std::uint32_t>(reader.U32());
  packet.maxsp = static_cast<std::uint32_t>(reader.U32());
  packet.race = static_cast<std::uint8_t>(reader.U8());
  packet.element = static_cast<std::uint8_t>(reader.U8());
  packet.element_lv = static_cast<std::uint8_t>(reader.U8());
  packet.size = static_cast<std::uint8_t>(reader.U8());
  packet.boss = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightTargetInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightTargetInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightTargetInfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  writer.U8(static_cast<std::uint8_t>(packet.status));
  writer.U8(static_cast<std::uint8_t>(packet.known));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.I16(static_cast<std::int16_t>(packet.level));
  writer.U32(static_cast<std::uint32_t>(packet.hp));
  writer.U32(static_cast<std::uint32_t>(packet.maxhp));
  writer.U32(static_cast<std::uint32_t>(packet.sp));
  writer.U32(static_cast<std::uint32_t>(packet.maxsp));
  writer.U8(static_cast<std::uint8_t>(packet.race));
  writer.U8(static_cast<std::uint8_t>(packet.element));
  writer.U8(static_cast<std::uint8_t>(packet.element_lv));
  writer.U8(static_cast<std::uint8_t>(packet.size));
  writer.U8(static_cast<std::uint8_t>(packet.boss));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightMvpCmd& packet) {
  if (frame.size() < CzMoonlightMvpCmd::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightMvpCmd::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.cmd = static_cast<std::uint8_t>(reader.U8());
  packet.a = static_cast<std::uint32_t>(reader.U32());
  packet.b = static_cast<std::uint32_t>(reader.U32());
  packet.text = reader.FixedString(1);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightMvpCmd& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightMvpCmd::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightMvpCmd::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.cmd));
  writer.U32(static_cast<std::uint32_t>(packet.a));
  writer.U32(static_cast<std::uint32_t>(packet.b));
  writer.FixedString(packet.text, 1);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightMvpState& packet) {
  if (frame.size() < ZcMoonlightMvpState::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightMvpState::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.kind = static_cast<std::uint8_t>(reader.U8());
  packet.server_time = static_cast<std::int64_t>(reader.I64());
  packet.count = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightMvpState& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightMvpState::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightMvpState::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.kind));
  writer.I64(static_cast<std::int64_t>(packet.server_time));
  writer.U16(static_cast<std::uint16_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightMvpGroup& packet) {
  if (frame.size() < ZcMoonlightMvpGroup::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightMvpGroup::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.kind = static_cast<std::uint8_t>(reader.U8());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  packet.group_id = static_cast<std::uint32_t>(reader.U32());
  packet.owner_user_id = static_cast<std::uint32_t>(reader.U32());
  packet.name = reader.FixedString(32);
  packet.count = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightMvpGroup& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightMvpGroup::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightMvpGroup::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.kind));
  writer.U8(static_cast<std::uint8_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.group_id));
  writer.U32(static_cast<std::uint32_t>(packet.owner_user_id));
  writer.FixedString(packet.name, 32);
  writer.U8(static_cast<std::uint8_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightPartyShare& packet) {
  if (frame.size() < ZcMoonlightPartyShare::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightPartyShare::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.idle_secs = static_cast<std::uint16_t>(reader.U16());
  packet.count = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightPartyShare& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightPartyShare::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightPartyShare::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.idle_secs));
  writer.U16(static_cast<std::uint16_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightFlagGraffiti& packet) {
  if (frame.size() < ZcMoonlightFlagGraffiti::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightFlagGraffiti::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.unit_id = static_cast<std::uint32_t>(reader.U32());
  packet.creator_id = static_cast<std::uint32_t>(reader.U32());
  packet.x = static_cast<std::int16_t>(reader.I16());
  packet.y = static_cast<std::int16_t>(reader.I16());
  packet.guild_id = static_cast<std::uint32_t>(reader.U32());
  packet.emblem_id = static_cast<std::uint32_t>(reader.U32());
  packet.level = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightFlagGraffiti& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightFlagGraffiti::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightFlagGraffiti::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.unit_id));
  writer.U32(static_cast<std::uint32_t>(packet.creator_id));
  writer.I16(static_cast<std::int16_t>(packet.x));
  writer.I16(static_cast<std::int16_t>(packet.y));
  writer.U32(static_cast<std::uint32_t>(packet.guild_id));
  writer.U32(static_cast<std::uint32_t>(packet.emblem_id));
  writer.U8(static_cast<std::uint8_t>(packet.level));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightUnitMaster& packet) {
  if (frame.size() < ZcMoonlightUnitMaster::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightUnitMaster::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.master_id = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightUnitMaster& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightUnitMaster::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightUnitMaster::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.U32(static_cast<std::uint32_t>(packet.master_id));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightServerRules& packet) {
  if (frame.size() < ZcMoonlightServerRules::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightServerRules::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcMoonlightServerRulesEntry::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.rules.resize(reader.remaining() / ZcMoonlightServerRulesEntry::kWireSize);
    for (ZcMoonlightServerRulesEntry& element : packet.rules) {
      ReadZcMoonlightServerRulesEntry(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightServerRules& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightServerRules::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightServerRules::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcMoonlightServerRulesEntry& element : packet.rules) {
    WriteZcMoonlightServerRulesEntry(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightChatAuthor& packet) {
  if (frame.size() < ZcMoonlightChatAuthor::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightChatAuthor::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.next_opcode = static_cast<std::uint16_t>(reader.U16());
  packet.scope = static_cast<std::uint8_t>(reader.U8());
  packet.channel_type = static_cast<std::uint8_t>(reader.U8());
  packet.account_id = static_cast<std::uint32_t>(reader.U32());
  packet.char_id = static_cast<std::uint32_t>(reader.U32());
  packet.header_len = static_cast<std::uint16_t>(reader.U16());
  packet.name_offset = static_cast<std::uint16_t>(reader.U16());
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightChatAuthor& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightChatAuthor::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightChatAuthor::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.next_opcode));
  writer.U8(static_cast<std::uint8_t>(packet.scope));
  writer.U8(static_cast<std::uint8_t>(packet.channel_type));
  writer.U32(static_cast<std::uint32_t>(packet.account_id));
  writer.U32(static_cast<std::uint32_t>(packet.char_id));
  writer.U16(static_cast<std::uint16_t>(packet.header_len));
  writer.U16(static_cast<std::uint16_t>(packet.name_offset));
  writer.FixedString(packet.name, 24);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightQuestEnd& packet) {
  if (frame.size() < ZcMoonlightQuestEnd::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightQuestEnd::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.quest_id = static_cast<std::int32_t>(reader.I32());
  packet.reason = static_cast<std::uint8_t>(reader.U8());
  packet.next_quest_id = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightQuestEnd& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightQuestEnd::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightQuestEnd::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I32(static_cast<std::int32_t>(packet.quest_id));
  writer.U8(static_cast<std::uint8_t>(packet.reason));
  writer.I32(static_cast<std::int32_t>(packet.next_quest_id));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightDiscordRich& packet) {
  if (frame.size() < ZcMoonlightDiscordRich::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightDiscordRich::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.discord_user_id = static_cast<std::uint64_t>(reader.U64());
  packet.channel_len = static_cast<std::uint8_t>(reader.U8());
  packet.name_len = static_cast<std::uint8_t>(reader.U8());
  packet.avatar_len = static_cast<std::uint8_t>(reader.U8());
  packet.flags = static_cast<std::uint8_t>(reader.U8());
  packet.text_len = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightDiscordRich& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightDiscordRich::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightDiscordRich::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U64(static_cast<std::uint64_t>(packet.discord_user_id));
  writer.U8(static_cast<std::uint8_t>(packet.channel_len));
  writer.U8(static_cast<std::uint8_t>(packet.name_len));
  writer.U8(static_cast<std::uint8_t>(packet.avatar_len));
  writer.U8(static_cast<std::uint8_t>(packet.flags));
  writer.U16(static_cast<std::uint16_t>(packet.text_len));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightCardAlbum& packet) {
  if (frame.size() < ZcMoonlightCardAlbum::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightCardAlbum::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.result = static_cast<std::uint8_t>(reader.U8());
  packet.count = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightCardAlbum& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightCardAlbum::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightCardAlbum::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  writer.U16(static_cast<std::uint16_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightCardAlbumCmd& packet) {
  if (frame.size() < CzMoonlightCardAlbumCmd::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightCardAlbumCmd::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.cmd = static_cast<std::uint8_t>(reader.U8());
  packet.arg = static_cast<std::uint32_t>(reader.U32());
  packet.amount = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightCardAlbumCmd& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightCardAlbumCmd::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightCardAlbumCmd::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.cmd));
  writer.U32(static_cast<std::uint32_t>(packet.arg));
  writer.U16(static_cast<std::uint16_t>(packet.amount));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqStatusList& packet) {
  if (frame.size() != CzMoonlightReqStatusList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightReqStatusList::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightReqStatusList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightReqStatusList::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightReqStatusList::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightStatusList& packet) {
  if (frame.size() < ZcMoonlightStatusList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightStatusList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  packet.status = static_cast<std::uint8_t>(reader.U8());
  packet.count = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightStatusList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightStatusList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightStatusList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  writer.U8(static_cast<std::uint8_t>(packet.status));
  writer.U8(static_cast<std::uint8_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqLooks& packet) {
  if (frame.size() != CzMoonlightReqLooks::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightReqLooks::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.what = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightReqLooks& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightReqLooks::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightReqLooks::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U8(static_cast<std::uint8_t>(packet.what));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightLooks& packet) {
  if (frame.size() < ZcMoonlightLooks::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightLooks::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.count = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightLooks& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightLooks::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightLooks::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightStatBonus& packet) {
  if (frame.size() < ZcMoonlightStatBonus::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightStatBonus::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  for (std::int16_t& element : packet.param_equip) {
    element = static_cast<std::int16_t>(reader.I16());
  }
  for (std::int16_t& element : packet.param_bonus) {
    element = static_cast<std::int16_t>(reader.I16());
  }
  packet.eatk = static_cast<std::int32_t>(reader.I32());
  packet.ematk = static_cast<std::int32_t>(reader.I32());
  packet.melee_pct = static_cast<std::int32_t>(reader.I32());
  packet.ranged_pct = static_cast<std::int32_t>(reader.I32());
  packet.crit_dmg_pct = static_cast<std::int32_t>(reader.I32());
  packet.hp_add = static_cast<std::int32_t>(reader.I32());
  packet.sp_add = static_cast<std::int32_t>(reader.I32());
  packet.aspd_add = static_cast<std::int32_t>(reader.I32());
  packet.vcast_pct = static_cast<std::int32_t>(reader.I32());
  packet.fcast_pct = static_cast<std::int32_t>(reader.I32());
  packet.atk_pct = static_cast<std::int32_t>(reader.I32());
  packet.matk_pct = static_cast<std::int32_t>(reader.I32());
  packet.dmg_ret_melee = static_cast<std::int32_t>(reader.I32());
  packet.dmg_ret_ranged = static_cast<std::int32_t>(reader.I32());
  packet.dmg_ret_magic = static_cast<std::int32_t>(reader.I32());
  packet.double_pct = static_cast<std::int32_t>(reader.I32());
  packet.perfect_hit = static_cast<std::int32_t>(reader.I32());
  packet.hp_pct = static_cast<std::int32_t>(reader.I32());
  packet.sp_pct = static_cast<std::int32_t>(reader.I32());
  packet.hp_regen_pct = static_cast<std::int32_t>(reader.I32());
  packet.sp_regen_pct = static_cast<std::int32_t>(reader.I32());
  packet.crit_def_pct = static_cast<std::int32_t>(reader.I32());
  packet.hp_on_kill = static_cast<std::int32_t>(reader.I32());
  packet.sp_on_kill = static_cast<std::int32_t>(reader.I32());
  packet.unbreak_pct = static_cast<std::int32_t>(reader.I32());
  packet.pot_hp_pct = static_cast<std::int32_t>(reader.I32());
  packet.pot_sp_pct = static_cast<std::int32_t>(reader.I32());
  packet.heal_up_pct = static_cast<std::int32_t>(reader.I32());
  packet.delay_pct = static_cast<std::int32_t>(reader.I32());
  packet.add_vcast_ms = static_cast<std::int32_t>(reader.I32());
  packet.add_fcast_ms = static_cast<std::int32_t>(reader.I32());
  packet.steal_pct = static_cast<std::int32_t>(reader.I32());
  packet.def_melee_pct = static_cast<std::int32_t>(reader.I32());
  packet.def_ranged_pct = static_cast<std::int32_t>(reader.I32());
  packet.def_magic_pct = static_cast<std::int32_t>(reader.I32());
  packet.def_misc_pct = static_cast<std::int32_t>(reader.I32());
  packet.splash = static_cast<std::int32_t>(reader.I32());
  packet.splash_add = static_cast<std::int32_t>(reader.I32());
  packet.hp_drain_pct = static_cast<std::int32_t>(reader.I32());
  packet.sp_drain_pct = static_cast<std::int32_t>(reader.I32());
  packet.break_weapon_pct = static_cast<std::int32_t>(reader.I32());
  packet.break_armor_pct = static_cast<std::int32_t>(reader.I32());
  packet.zeny_bonus_pct = static_cast<std::int32_t>(reader.I32());
  packet.classchange_pct = static_cast<std::int32_t>(reader.I32());
  packet.dmg_ret_reduce = static_cast<std::int32_t>(reader.I32());
  packet.magic_hp_gain = static_cast<std::int32_t>(reader.I32());
  packet.magic_sp_gain = static_cast<std::int32_t>(reader.I32());
  packet.refine_atk = static_cast<std::int32_t>(reader.I32());
  packet.refine_def = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightStatBonus& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightStatBonus::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightStatBonus::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const std::int16_t element : packet.param_equip) {
    writer.I16(static_cast<std::int16_t>(element));
  }
  for (const std::int16_t element : packet.param_bonus) {
    writer.I16(static_cast<std::int16_t>(element));
  }
  writer.I32(static_cast<std::int32_t>(packet.eatk));
  writer.I32(static_cast<std::int32_t>(packet.ematk));
  writer.I32(static_cast<std::int32_t>(packet.melee_pct));
  writer.I32(static_cast<std::int32_t>(packet.ranged_pct));
  writer.I32(static_cast<std::int32_t>(packet.crit_dmg_pct));
  writer.I32(static_cast<std::int32_t>(packet.hp_add));
  writer.I32(static_cast<std::int32_t>(packet.sp_add));
  writer.I32(static_cast<std::int32_t>(packet.aspd_add));
  writer.I32(static_cast<std::int32_t>(packet.vcast_pct));
  writer.I32(static_cast<std::int32_t>(packet.fcast_pct));
  writer.I32(static_cast<std::int32_t>(packet.atk_pct));
  writer.I32(static_cast<std::int32_t>(packet.matk_pct));
  writer.I32(static_cast<std::int32_t>(packet.dmg_ret_melee));
  writer.I32(static_cast<std::int32_t>(packet.dmg_ret_ranged));
  writer.I32(static_cast<std::int32_t>(packet.dmg_ret_magic));
  writer.I32(static_cast<std::int32_t>(packet.double_pct));
  writer.I32(static_cast<std::int32_t>(packet.perfect_hit));
  writer.I32(static_cast<std::int32_t>(packet.hp_pct));
  writer.I32(static_cast<std::int32_t>(packet.sp_pct));
  writer.I32(static_cast<std::int32_t>(packet.hp_regen_pct));
  writer.I32(static_cast<std::int32_t>(packet.sp_regen_pct));
  writer.I32(static_cast<std::int32_t>(packet.crit_def_pct));
  writer.I32(static_cast<std::int32_t>(packet.hp_on_kill));
  writer.I32(static_cast<std::int32_t>(packet.sp_on_kill));
  writer.I32(static_cast<std::int32_t>(packet.unbreak_pct));
  writer.I32(static_cast<std::int32_t>(packet.pot_hp_pct));
  writer.I32(static_cast<std::int32_t>(packet.pot_sp_pct));
  writer.I32(static_cast<std::int32_t>(packet.heal_up_pct));
  writer.I32(static_cast<std::int32_t>(packet.delay_pct));
  writer.I32(static_cast<std::int32_t>(packet.add_vcast_ms));
  writer.I32(static_cast<std::int32_t>(packet.add_fcast_ms));
  writer.I32(static_cast<std::int32_t>(packet.steal_pct));
  writer.I32(static_cast<std::int32_t>(packet.def_melee_pct));
  writer.I32(static_cast<std::int32_t>(packet.def_ranged_pct));
  writer.I32(static_cast<std::int32_t>(packet.def_magic_pct));
  writer.I32(static_cast<std::int32_t>(packet.def_misc_pct));
  writer.I32(static_cast<std::int32_t>(packet.splash));
  writer.I32(static_cast<std::int32_t>(packet.splash_add));
  writer.I32(static_cast<std::int32_t>(packet.hp_drain_pct));
  writer.I32(static_cast<std::int32_t>(packet.sp_drain_pct));
  writer.I32(static_cast<std::int32_t>(packet.break_weapon_pct));
  writer.I32(static_cast<std::int32_t>(packet.break_armor_pct));
  writer.I32(static_cast<std::int32_t>(packet.zeny_bonus_pct));
  writer.I32(static_cast<std::int32_t>(packet.classchange_pct));
  writer.I32(static_cast<std::int32_t>(packet.dmg_ret_reduce));
  writer.I32(static_cast<std::int32_t>(packet.magic_hp_gain));
  writer.I32(static_cast<std::int32_t>(packet.magic_sp_gain));
  writer.I32(static_cast<std::int32_t>(packet.refine_atk));
  writer.I32(static_cast<std::int32_t>(packet.refine_def));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqItemscript& packet) {
  if (frame.size() != CzMoonlightReqItemscript::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightReqItemscript::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.id = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightReqItemscript& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightReqItemscript::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightReqItemscript::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U32(static_cast<std::uint32_t>(packet.id));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightItemscript& packet) {
  if (frame.size() < ZcMoonlightItemscript::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightItemscript::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.id = static_cast<std::uint32_t>(reader.U32());
  packet.status = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightItemscript& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightItemscript::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightItemscript::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.id));
  writer.U8(static_cast<std::uint8_t>(packet.status));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightBugReport& packet) {
  if (frame.size() < CzMoonlightBugReport::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightBugReport::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.category = static_cast<std::uint8_t>(reader.U8());
  packet.ctx_len = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightBugReport& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightBugReport::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightBugReport::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.category));
  writer.U16(static_cast<std::uint16_t>(packet.ctx_len));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightBugReportAck& packet) {
  if (frame.size() < ZcMoonlightBugReportAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightBugReportAck::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.status = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightBugReportAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightBugReportAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightBugReportAck::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.status));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoonlightCompanion& packet) {
  if (frame.size() != CzMoonlightCompanion::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoonlightCompanion::kOpcode) {
    return false;
  }
  packet.packetLength = static_cast<std::int16_t>(reader.I16());
  packet.kind = static_cast<std::uint8_t>(reader.U8());
  packet.action = static_cast<std::uint8_t>(reader.U8());
  packet.arg = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoonlightCompanion& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoonlightCompanion::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoonlightCompanion::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.packetLength));
  writer.U8(static_cast<std::uint8_t>(packet.kind));
  writer.U8(static_cast<std::uint8_t>(packet.action));
  writer.U8(static_cast<std::uint8_t>(packet.arg));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightCompanionState& packet) {
  if (frame.size() < ZcMoonlightCompanionState::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightCompanionState::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.pushcart_lv = static_cast<std::uint8_t>(reader.U8());
  packet.changecart_lv = static_cast<std::uint8_t>(reader.U8());
  packet.riding_lv = static_cast<std::uint8_t>(reader.U8());
  packet.falcon_lv = static_cast<std::uint8_t>(reader.U8());
  packet.cart_active = static_cast<std::uint8_t>(reader.U8());
  packet.riding_active = static_cast<std::uint8_t>(reader.U8());
  packet.falcon_active = static_cast<std::uint8_t>(reader.U8());
  packet.cart_deco_max = static_cast<std::uint8_t>(reader.U8());
  packet.pushcart_id = static_cast<std::uint16_t>(reader.U16());
  packet.riding_id = static_cast<std::uint16_t>(reader.U16());
  packet.falcon_id = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightCompanionState& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightCompanionState::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightCompanionState::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.pushcart_lv));
  writer.U8(static_cast<std::uint8_t>(packet.changecart_lv));
  writer.U8(static_cast<std::uint8_t>(packet.riding_lv));
  writer.U8(static_cast<std::uint8_t>(packet.falcon_lv));
  writer.U8(static_cast<std::uint8_t>(packet.cart_active));
  writer.U8(static_cast<std::uint8_t>(packet.riding_active));
  writer.U8(static_cast<std::uint8_t>(packet.falcon_active));
  writer.U8(static_cast<std::uint8_t>(packet.cart_deco_max));
  writer.U16(static_cast<std::uint16_t>(packet.pushcart_id));
  writer.U16(static_cast<std::uint16_t>(packet.riding_id));
  writer.U16(static_cast<std::uint16_t>(packet.falcon_id));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightHateffectMap& packet) {
  if (frame.size() < ZcMoonlightHateffectMap::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMoonlightHateffectMap::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.count = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMoonlightHateffectMap& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMoonlightHateffectMap::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMoonlightHateffectMap::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I16(static_cast<std::int16_t>(packet.count));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUserCount& packet) {
  if (frame.size() != ZcUserCount::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUserCount::kOpcode) {
    return false;
  }
  packet.playersCount = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUserCount& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUserCount::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUserCount::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.playersCount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPcPurchaseResult& packet) {
  if (frame.size() != ZcPcPurchaseResult::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPcPurchaseResult::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPcPurchaseResult& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPcPurchaseResult::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPcPurchaseResult::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqMakingarrow& packet) {
  if (frame.size() != CzReqMakingarrow::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqMakingarrow::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqMakingarrow& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqMakingarrow::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqMakingarrow::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzSePcBuyCashitemList& packet) {
  if (frame.size() < CzSePcBuyCashitemList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzSePcBuyCashitemList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.count = static_cast<std::uint16_t>(reader.U16());
  packet.kafraPoints = static_cast<std::uint32_t>(reader.U32());
  if (reader.remaining() % CzSePcBuyCashitemListSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / CzSePcBuyCashitemListSub::kWireSize);
    for (CzSePcBuyCashitemListSub& element : packet.items) {
      ReadCzSePcBuyCashitemListSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzSePcBuyCashitemList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzSePcBuyCashitemList::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzSePcBuyCashitemList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.count));
  writer.U32(static_cast<std::uint32_t>(packet.kafraPoints));
  for (const CzSePcBuyCashitemListSub& element : packet.items) {
    WriteCzSePcBuyCashitemListSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqCashBargainSaleItemInfo& packet) {
  if (frame.size() < CzReqCashBargainSaleItemInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqCashBargainSaleItemInfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqCashBargainSaleItemInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqCashBargainSaleItemInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqCashBargainSaleItemInfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckCashBargainSaleItemInfo& packet) {
  if (frame.size() != ZcAckCashBargainSaleItemInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckCashBargainSaleItemInfo::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint16_t>(reader.U16());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.price = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckCashBargainSaleItemInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckCashBargainSaleItemInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckCashBargainSaleItemInfo::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U32(static_cast<std::uint32_t>(packet.price));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqApplyBargainSaleItem& packet) {
  if (frame.size() != CzReqApplyBargainSaleItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqApplyBargainSaleItem::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.amount = static_cast<std::uint32_t>(reader.U32());
  packet.startTime = static_cast<std::uint32_t>(reader.U32());
  packet.hours = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqApplyBargainSaleItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqApplyBargainSaleItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqApplyBargainSaleItem::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U32(static_cast<std::uint32_t>(packet.amount));
  writer.U32(static_cast<std::uint32_t>(packet.startTime));
  writer.U16(static_cast<std::uint16_t>(packet.hours));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqRemoveBargainSaleItem& packet) {
  if (frame.size() != CzReqRemoveBargainSaleItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqRemoveBargainSaleItem::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqRemoveBargainSaleItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqRemoveBargainSaleItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqRemoveBargainSaleItem::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyBargainSaleSelling& packet) {
  if (frame.size() != ZcNotifyBargainSaleSelling::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyBargainSaleSelling::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.remainingTime = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyBargainSaleSelling& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyBargainSaleSelling::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyBargainSaleSelling::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U32(static_cast<std::uint32_t>(packet.remainingTime));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyBargainSaleClose& packet) {
  if (frame.size() != ZcNotifyBargainSaleClose::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyBargainSaleClose::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyBargainSaleClose& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyBargainSaleClose::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyBargainSaleClose::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckCountBargainSaleItem& packet) {
  if (frame.size() != ZcAckCountBargainSaleItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckCountBargainSaleItem::kOpcode) {
    return false;
  }
  packet.itemId = static_cast<std::uint32_t>(reader.U32());
  packet.amount = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckCountBargainSaleItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckCountBargainSaleItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckCountBargainSaleItem::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemId));
  writer.U32(static_cast<std::uint32_t>(packet.amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckGuildstorageLog& packet) {
  if (frame.size() < ZcAckGuildstorageLog::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckGuildstorageLog::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.result = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::uint16_t>(reader.U16());
  if (reader.remaining() % ZcAckGuildstorageLogSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcAckGuildstorageLogSub::kWireSize);
    for (ZcAckGuildstorageLogSub& element : packet.items) {
      ReadZcAckGuildstorageLogSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckGuildstorageLog& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckGuildstorageLog::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckGuildstorageLog::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.result));
  writer.U16(static_cast<std::uint16_t>(packet.amount));
  for (const ZcAckGuildstorageLogSub& element : packet.items) {
    WriteZcAckGuildstorageLogSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzAdvancedStatusChange& packet) {
  if (frame.size() != CzAdvancedStatusChange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzAdvancedStatusChange::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::int16_t>(reader.I16());
  packet.amount = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzAdvancedStatusChange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzAdvancedStatusChange::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzAdvancedStatusChange::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.type));
  writer.I16(static_cast<std::int16_t>(packet.amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqAddNewEmblem& packet) {
  if (frame.size() != CzReqAddNewEmblem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqAddNewEmblem::kOpcode) {
    return false;
  }
  packet.guild_id = static_cast<std::uint32_t>(reader.U32());
  packet.version = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqAddNewEmblem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqAddNewEmblem::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqAddNewEmblem::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.guild_id));
  writer.U32(static_cast<std::uint32_t>(packet.version));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcBroadcast& packet) {
  if (frame.size() < ZcBroadcast::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcBroadcast::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.message = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcBroadcast& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcBroadcast::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcBroadcast::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.Text(packet.message);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcBroadcast2& packet) {
  if (frame.size() < ZcBroadcast2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcBroadcast2::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.fontColor = static_cast<std::uint32_t>(reader.U32());
  packet.fontType = static_cast<std::int16_t>(reader.I16());
  packet.fontSize = static_cast<std::int16_t>(reader.I16());
  packet.fontAlign = static_cast<std::int16_t>(reader.I16());
  packet.fontY = static_cast<std::int16_t>(reader.I16());
  packet.message = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcBroadcast2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcBroadcast2::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcBroadcast2::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.fontColor));
  writer.I16(static_cast<std::int16_t>(packet.fontType));
  writer.I16(static_cast<std::int16_t>(packet.fontSize));
  writer.I16(static_cast<std::int16_t>(packet.fontAlign));
  writer.I16(static_cast<std::int16_t>(packet.fontY));
  writer.Text(packet.message);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcEntryQueueInit& /*packet*/) {
  if (frame.size() != ZcEntryQueueInit::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcEntryQueueInit::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcEntryQueueInit& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcEntryQueueInit::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcEntryQueueInit::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzRodexReturn& packet) {
  if (frame.size() != CzRodexReturn::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzRodexReturn::kOpcode) {
    return false;
  }
  packet.msgId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzRodexReturn& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzRodexReturn::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzRodexReturn::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.msgId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcRodexReturn& packet) {
  if (frame.size() != ZcRodexReturn::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcRodexReturn::kOpcode) {
    return false;
  }
  packet.msgId = static_cast<std::uint32_t>(reader.U32());
  packet.result = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcRodexReturn& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcRodexReturn::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcRodexReturn::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.msgId));
  writer.U32(static_cast<std::uint32_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqStyleClose& /*packet*/) {
  if (frame.size() != CzReqStyleClose::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqStyleClose::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqStyleClose& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqStyleClose::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqStyleClose::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSummonHpInit& packet) {
  if (frame.size() != ZcSummonHpInit::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSummonHpInit::kOpcode) {
    return false;
  }
  packet.summonAID = static_cast<std::uint32_t>(reader.U32());
  packet.CurrentHP = static_cast<std::uint32_t>(reader.U32());
  packet.MaxHP = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSummonHpInit& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSummonHpInit::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSummonHpInit::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.summonAID));
  writer.U32(static_cast<std::uint32_t>(packet.CurrentHP));
  writer.U32(static_cast<std::uint32_t>(packet.MaxHP));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSummonHpUpdate& packet) {
  if (frame.size() != ZcSummonHpUpdate::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSummonHpUpdate::kOpcode) {
    return false;
  }
  packet.summonAID = static_cast<std::uint32_t>(reader.U32());
  packet.VarId = static_cast<std::uint16_t>(reader.U16());
  packet.Value = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSummonHpUpdate& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSummonHpUpdate::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSummonHpUpdate::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.summonAID));
  writer.U16(static_cast<std::uint16_t>(packet.VarId));
  writer.U32(static_cast<std::uint32_t>(packet.Value));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcReputeInfo& packet) {
  if (frame.size() < ZcReputeInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcReputeInfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.success = static_cast<std::uint8_t>(reader.U8());
  if (reader.remaining() % ZcReputeInfoSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / ZcReputeInfoSub::kWireSize);
    for (ZcReputeInfoSub& element : packet.list) {
      ReadZcReputeInfoSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcReputeInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcReputeInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcReputeInfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.success));
  for (const ZcReputeInfoSub& element : packet.list) {
    WriteZcReputeInfoSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcReputeOpen& packet) {
  if (frame.size() != ZcReputeOpen::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcReputeOpen::kOpcode) {
    return false;
  }
  packet.table = static_cast<std::uint64_t>(reader.U64());
  packet.type = static_cast<std::uint32_t>(reader.U32());
  packet.mode = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcReputeOpen& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcReputeOpen::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcReputeOpen::kOpcode);
  writer.U64(static_cast<std::uint64_t>(packet.table));
  writer.U32(static_cast<std::uint32_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.mode));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUiOpenV3& packet) {
  if (frame.size() != ZcUiOpenV3::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUiOpenV3::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.data = static_cast<std::uint64_t>(reader.U64());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUiOpenV3& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUiOpenV3::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUiOpenV3::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.U64(static_cast<std::uint64_t>(packet.data));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcTargetSpirits& packet) {
  if (frame.size() != ZcTargetSpirits::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcTargetSpirits::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.unknown_val = static_cast<std::uint32_t>(reader.U32());
  packet.amount = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcTargetSpirits& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcTargetSpirits::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcTargetSpirits::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.U32(static_cast<std::uint32_t>(packet.unknown_val));
  writer.U16(static_cast<std::uint16_t>(packet.amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcFriendsList& packet) {
  if (frame.size() < ZcFriendsList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcFriendsList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcFriendsListSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.friends.resize(reader.remaining() / ZcFriendsListSub::kWireSize);
    for (ZcFriendsListSub& element : packet.friends) {
      ReadZcFriendsListSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcFriendsList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcFriendsList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcFriendsList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcFriendsListSub& element : packet.friends) {
    WriteZcFriendsListSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzPcSellItemlist& packet) {
  if (frame.size() < CzPcSellItemlist::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzPcSellItemlist::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % CzPcSellItemlistSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.sellList.resize(reader.remaining() / CzPcSellItemlistSub::kWireSize);
    for (CzPcSellItemlistSub& element : packet.sellList) {
      ReadCzPcSellItemlistSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzPcSellItemlist& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzPcSellItemlist::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzPcSellItemlist::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const CzPcSellItemlistSub& element : packet.sellList) {
    WriteCzPcSellItemlistSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqChangeMemberpos& packet) {
  if (frame.size() < CzReqChangeMemberpos::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqChangeMemberpos::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % CzReqChangeMemberposSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.list.resize(reader.remaining() / CzReqChangeMemberposSub::kWireSize);
    for (CzReqChangeMemberposSub& element : packet.list) {
      ReadCzReqChangeMemberposSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqChangeMemberpos& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqChangeMemberpos::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqChangeMemberpos::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const CzReqChangeMemberposSub& element : packet.list) {
    WriteCzReqChangeMemberposSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcClearDialog& packet) {
  if (frame.size() != ZcClearDialog::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcClearDialog::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcClearDialog& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcClearDialog::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcClearDialog::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyBindOnEquip& packet) {
  if (frame.size() != ZcNotifyBindOnEquip::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyBindOnEquip::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyBindOnEquip& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyBindOnEquip::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyBindOnEquip::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcBankingCheck& packet) {
  if (frame.size() != ZcBankingCheck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcBankingCheck::kOpcode) {
    return false;
  }
  packet.money = static_cast<std::int64_t>(reader.I64());
  packet.reason = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcBankingCheck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcBankingCheck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcBankingCheck::kOpcode);
  writer.I64(static_cast<std::int64_t>(packet.money));
  writer.I16(static_cast<std::int16_t>(packet.reason));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckBankingWithdraw& packet) {
  if (frame.size() != ZcAckBankingWithdraw::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckBankingWithdraw::kOpcode) {
    return false;
  }
  packet.reason = static_cast<std::int16_t>(reader.I16());
  packet.money = static_cast<std::int64_t>(reader.I64());
  packet.zeny = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckBankingWithdraw& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckBankingWithdraw::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckBankingWithdraw::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.reason));
  writer.I64(static_cast<std::int64_t>(packet.money));
  writer.I32(static_cast<std::int32_t>(packet.zeny));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckBankingDeposit& packet) {
  if (frame.size() != ZcAckBankingDeposit::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckBankingDeposit::kOpcode) {
    return false;
  }
  packet.reason = static_cast<std::int16_t>(reader.I16());
  packet.money = static_cast<std::int64_t>(reader.I64());
  packet.zeny = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckBankingDeposit& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckBankingDeposit::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckBankingDeposit::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.reason));
  writer.I64(static_cast<std::int64_t>(packet.money));
  writer.I32(static_cast<std::int32_t>(packet.zeny));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckCloseBanking& packet) {
  if (frame.size() != ZcAckCloseBanking::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckCloseBanking::kOpcode) {
    return false;
  }
  packet.unknown = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckCloseBanking& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckCloseBanking::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckCloseBanking::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.unknown));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckOpenBanking& packet) {
  if (frame.size() != ZcAckOpenBanking::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckOpenBanking::kOpcode) {
    return false;
  }
  packet.unknown = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckOpenBanking& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckOpenBanking::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckOpenBanking::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.unknown));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcReqExchangeItem& packet) {
  if (frame.size() != ZcReqExchangeItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcReqExchangeItem::kOpcode) {
    return false;
  }
  packet.requesterName = reader.FixedString(24);
  packet.targetId = static_cast<std::uint32_t>(reader.U32());
  packet.targetLv = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcReqExchangeItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcReqExchangeItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcReqExchangeItem::kOpcode);
  writer.FixedString(packet.requesterName, 24);
  writer.U32(static_cast<std::uint32_t>(packet.targetId));
  writer.U16(static_cast<std::uint16_t>(packet.targetLv));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckExchangeItem& packet) {
  if (frame.size() != ZcAckExchangeItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckExchangeItem::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  packet.targetId = static_cast<std::uint32_t>(reader.U32());
  packet.targetLv = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckExchangeItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckExchangeItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckExchangeItem::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.targetId));
  writer.U16(static_cast<std::uint16_t>(packet.targetLv));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckAddExchangeItem& packet) {
  if (frame.size() != ZcAckAddExchangeItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckAddExchangeItem::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckAddExchangeItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckAddExchangeItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckAddExchangeItem::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCouplename& packet) {
  if (frame.size() != ZcCouplename::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCouplename::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCouplename& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCouplename::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCouplename::kOpcode);
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzPartyReqMasterToJoin& packet) {
  if (frame.size() != CzPartyReqMasterToJoin::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzPartyReqMasterToJoin::kOpcode) {
    return false;
  }
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzPartyReqMasterToJoin& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzPartyReqMasterToJoin::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzPartyReqMasterToJoin::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPartyReqMasterToJoin& packet) {
  if (frame.size() != ZcPartyReqMasterToJoin::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPartyReqMasterToJoin::kOpcode) {
    return false;
  }
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.name = reader.FixedString(24);
  packet.x = static_cast<std::uint16_t>(reader.U16());
  packet.y = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPartyReqMasterToJoin& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPartyReqMasterToJoin::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPartyReqMasterToJoin::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.FixedString(packet.name, 24);
  writer.U16(static_cast<std::uint16_t>(packet.x));
  writer.U16(static_cast<std::uint16_t>(packet.y));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzPartyReqAckMasterToJoin& packet) {
  if (frame.size() != CzPartyReqAckMasterToJoin::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzPartyReqAckMasterToJoin::kOpcode) {
    return false;
  }
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.accept = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzPartyReqAckMasterToJoin& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzPartyReqAckMasterToJoin::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzPartyReqAckMasterToJoin::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U8(static_cast<std::uint8_t>(packet.accept));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPartyJoinReqAckFromMaster& packet) {
  if (frame.size() != ZcPartyJoinReqAckFromMaster::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPartyJoinReqAckFromMaster::kOpcode) {
    return false;
  }
  packet.player_name = reader.FixedString(24);
  packet.party_name = reader.FixedString(24);
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.refused = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPartyJoinReqAckFromMaster& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPartyJoinReqAckFromMaster::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPartyJoinReqAckFromMaster::kOpcode);
  writer.FixedString(packet.player_name, 24);
  writer.FixedString(packet.party_name, 24);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.refused));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqSeCashTabCode& packet) {
  if (frame.size() != CzReqSeCashTabCode::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqSeCashTabCode::kOpcode) {
    return false;
  }
  packet.tab = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqSeCashTabCode& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqSeCashTabCode::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqSeCashTabCode::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.tab));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckSeCashItemList2& packet) {
  if (frame.size() < ZcAckSeCashItemList2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckSeCashItemList2::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.tab = static_cast<std::uint32_t>(reader.U32());
  packet.count = static_cast<std::int16_t>(reader.I16());
  if (reader.remaining() % ZcAckSeCashItemList2Sub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcAckSeCashItemList2Sub::kWireSize);
    for (ZcAckSeCashItemList2Sub& element : packet.items) {
      ReadZcAckSeCashItemList2Sub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckSeCashItemList2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckSeCashItemList2::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckSeCashItemList2::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.tab));
  writer.I16(static_cast<std::int16_t>(packet.count));
  for (const ZcAckSeCashItemList2Sub& element : packet.items) {
    WriteZcAckSeCashItemList2Sub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqMergeItem& packet) {
  if (frame.size() < CzReqMergeItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqMergeItem::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % 2 != 0) {
    reader.Invalidate();
  } else {
    packet.indices.resize(reader.remaining() / 2);
    for (std::uint16_t& element : packet.indices) {
      element = static_cast<std::uint16_t>(reader.U16());
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqMergeItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqMergeItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqMergeItem::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const std::uint16_t element : packet.indices) {
    writer.U16(static_cast<std::uint16_t>(element));
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzResetSkill& packet) {
  if (frame.size() != CzResetSkill::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzResetSkill::kOpcode) {
    return false;
  }
  packet.unknown = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzResetSkill& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzResetSkill::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzResetSkill::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.unknown));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSkillinfoUpdate& packet) {
  if (frame.size() != ZcSkillinfoUpdate::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSkillinfoUpdate::kOpcode) {
    return false;
  }
  packet.skillId = static_cast<std::uint16_t>(reader.U16());
  packet.level = static_cast<std::uint16_t>(reader.U16());
  packet.sp = static_cast<std::uint16_t>(reader.U16());
  packet.range2 = static_cast<std::uint16_t>(reader.U16());
  packet.upFlag = static_cast<bool>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSkillinfoUpdate& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSkillinfoUpdate::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSkillinfoUpdate::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.skillId));
  writer.U16(static_cast<std::uint16_t>(packet.level));
  writer.U16(static_cast<std::uint16_t>(packet.sp));
  writer.U16(static_cast<std::uint16_t>(packet.range2));
  writer.U8(static_cast<std::uint8_t>(packet.upFlag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcBossInfo& packet) {
  if (frame.size() != ZcBossInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcBossInfo::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.x = static_cast<std::uint32_t>(reader.U32());
  packet.y = static_cast<std::uint32_t>(reader.U32());
  packet.minHours = static_cast<std::uint16_t>(reader.U16());
  packet.minMinutes = static_cast<std::uint16_t>(reader.U16());
  packet.maxHours = static_cast<std::uint16_t>(reader.U16());
  packet.maxMinutes = static_cast<std::uint16_t>(reader.U16());
  packet.name = reader.FixedString(51);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcBossInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcBossInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcBossInfo::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.x));
  writer.U32(static_cast<std::uint32_t>(packet.y));
  writer.U16(static_cast<std::uint16_t>(packet.minHours));
  writer.U16(static_cast<std::uint16_t>(packet.minMinutes));
  writer.U16(static_cast<std::uint16_t>(packet.maxHours));
  writer.U16(static_cast<std::uint16_t>(packet.maxMinutes));
  writer.FixedString(packet.name, 51);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzInventoryTab& packet) {
  if (frame.size() != CzInventoryTab::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzInventoryTab::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.favorite = static_cast<bool>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzInventoryTab& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzInventoryTab::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzInventoryTab::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.U8(static_cast<std::uint8_t>(packet.favorite));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcInventoryTab& packet) {
  if (frame.size() != ZcInventoryTab::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcInventoryTab::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::int16_t>(reader.I16());
  packet.favorite = static_cast<bool>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcInventoryTab& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcInventoryTab::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcInventoryTab::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.index));
  writer.U8(static_cast<std::uint8_t>(packet.favorite));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqOpenBanking& packet) {
  if (frame.size() != CzReqOpenBanking::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqOpenBanking::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqOpenBanking& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqOpenBanking::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqOpenBanking::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqCloseBanking& packet) {
  if (frame.size() != CzReqCloseBanking::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqCloseBanking::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqCloseBanking& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqCloseBanking::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqCloseBanking::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqBankingCheck& packet) {
  if (frame.size() != CzReqBankingCheck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqBankingCheck::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqBankingCheck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqBankingCheck::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqBankingCheck::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqBankingDeposit& packet) {
  if (frame.size() != CzReqBankingDeposit::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqBankingDeposit::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.zeny = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqBankingDeposit& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqBankingDeposit::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqBankingDeposit::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.I32(static_cast<std::int32_t>(packet.zeny));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqBankingWithdraw& packet) {
  if (frame.size() != CzReqBankingWithdraw::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqBankingWithdraw::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.zeny = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqBankingWithdraw& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqBankingWithdraw::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqBankingWithdraw::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.I32(static_cast<std::int32_t>(packet.zeny));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAcceptEnter& packet) {
  if (frame.size() != ZcAcceptEnter::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAcceptEnter::kOpcode) {
    return false;
  }
  packet.startTime = static_cast<std::uint32_t>(reader.U32());
  for (std::uint8_t& element : packet.posDir) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  packet.xSize = static_cast<std::uint8_t>(reader.U8());
  packet.ySize = static_cast<std::uint8_t>(reader.U8());
  packet.font = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAcceptEnter& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAcceptEnter::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAcceptEnter::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.startTime));
  for (const std::uint8_t element : packet.posDir) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  writer.U8(static_cast<std::uint8_t>(packet.xSize));
  writer.U8(static_cast<std::uint8_t>(packet.ySize));
  writer.U16(static_cast<std::uint16_t>(packet.font));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcRefuseEnter& packet) {
  if (frame.size() != ZcRefuseEnter::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcRefuseEnter::kOpcode) {
    return false;
  }
  packet.errorCode = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcRefuseEnter& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcRefuseEnter::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcRefuseEnter::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.errorCode));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcRestartAck& packet) {
  if (frame.size() != ZcRestartAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcRestartAck::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcRestartAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcRestartAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcRestartAck::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyVanish& packet) {
  if (frame.size() != ZcNotifyVanish::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyVanish::kOpcode) {
    return false;
  }
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyVanish& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyVanish::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyVanish::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcItemDisappear& packet) {
  if (frame.size() != ZcItemDisappear::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcItemDisappear::kOpcode) {
    return false;
  }
  packet.itemAid = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcItemDisappear& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcItemDisappear::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcItemDisappear::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.itemAid));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMillenniumshield& packet) {
  if (frame.size() != ZcMillenniumshield::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMillenniumshield::kOpcode) {
    return false;
  }
  packet.aid = static_cast<std::uint32_t>(reader.U32());
  packet.num = static_cast<std::int16_t>(reader.I16());
  packet.state = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMillenniumshield& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMillenniumshield::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMillenniumshield::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.aid));
  writer.I16(static_cast<std::int16_t>(packet.num));
  writer.I16(static_cast<std::int16_t>(packet.state));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSpiritsAttribute& packet) {
  if (frame.size() != ZcSpiritsAttribute::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSpiritsAttribute::kOpcode) {
    return false;
  }
  packet.aid = static_cast<std::uint32_t>(reader.U32());
  packet.spiritsType = static_cast<std::int16_t>(reader.I16());
  packet.num = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSpiritsAttribute& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSpiritsAttribute::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSpiritsAttribute::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.aid));
  writer.I16(static_cast<std::int16_t>(packet.spiritsType));
  writer.I16(static_cast<std::int16_t>(packet.num));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcChangestateMer& packet) {
  if (frame.size() != ZcChangestateMer::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcChangestateMer::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.state = static_cast<std::uint8_t>(reader.U8());
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  packet.data = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcChangestateMer& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcChangestateMer::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcChangestateMer::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.U8(static_cast<std::uint8_t>(packet.state));
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  writer.U32(static_cast<std::uint32_t>(packet.data));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcHoskillinfoList& packet) {
  if (frame.size() < ZcHoskillinfoList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcHoskillinfoList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcHoskillinfoListSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.skills.resize(reader.remaining() / ZcHoskillinfoListSub::kWireSize);
    for (ZcHoskillinfoListSub& element : packet.skills) {
      ReadZcHoskillinfoListSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcHoskillinfoList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcHoskillinfoList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcHoskillinfoList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcHoskillinfoListSub& element : packet.skills) {
    WriteZcHoskillinfoListSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcHoskillinfoUpdate& packet) {
  if (frame.size() != ZcHoskillinfoUpdate::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcHoskillinfoUpdate::kOpcode) {
    return false;
  }
  packet.skill_id = static_cast<std::uint16_t>(reader.U16());
  packet.Level = static_cast<std::int16_t>(reader.I16());
  packet.SP = static_cast<std::int16_t>(reader.I16());
  packet.AttackRange = static_cast<std::int16_t>(reader.I16());
  packet.upgradable = static_cast<bool>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcHoskillinfoUpdate& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcHoskillinfoUpdate::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcHoskillinfoUpdate::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.skill_id));
  writer.I16(static_cast<std::int16_t>(packet.Level));
  writer.I16(static_cast<std::int16_t>(packet.SP));
  writer.I16(static_cast<std::int16_t>(packet.AttackRange));
  writer.U8(static_cast<std::uint8_t>(packet.upgradable));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyPlayermove& packet) {
  if (frame.size() != ZcNotifyPlayermove::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyPlayermove::kOpcode) {
    return false;
  }
  packet.moveStartTime = static_cast<std::uint32_t>(reader.U32());
  for (std::uint8_t& element : packet.moveData) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyPlayermove& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyPlayermove::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyPlayermove::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.moveStartTime));
  for (const std::uint8_t element : packet.moveData) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcChangeDirection& packet) {
  if (frame.size() != ZcChangeDirection::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcChangeDirection::kOpcode) {
    return false;
  }
  packet.srcId = static_cast<std::uint32_t>(reader.U32());
  packet.headDir = static_cast<std::uint16_t>(reader.U16());
  packet.dir = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcChangeDirection& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcChangeDirection::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcChangeDirection::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.srcId));
  writer.U16(static_cast<std::uint16_t>(packet.headDir));
  writer.U8(static_cast<std::uint8_t>(packet.dir));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNpcackMapmove& packet) {
  if (frame.size() != ZcNpcackMapmove::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNpcackMapmove::kOpcode) {
    return false;
  }
  packet.mapName = reader.FixedString(16);
  packet.xPos = static_cast<std::uint16_t>(reader.U16());
  packet.yPos = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNpcackMapmove& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNpcackMapmove::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNpcackMapmove::kOpcode);
  writer.FixedString(packet.mapName, 16);
  writer.U16(static_cast<std::uint16_t>(packet.xPos));
  writer.U16(static_cast<std::uint16_t>(packet.yPos));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNpcackServermove& packet) {
  if (frame.size() != ZcNpcackServermove::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNpcackServermove::kOpcode) {
    return false;
  }
  packet.mapName = reader.FixedString(16);
  packet.xPos = static_cast<std::uint16_t>(reader.U16());
  packet.yPos = static_cast<std::uint16_t>(reader.U16());
  packet.ip = static_cast<std::uint32_t>(reader.U32());
  packet.port = static_cast<std::uint16_t>(reader.U16());
  packet.domain = reader.FixedString(128);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNpcackServermove& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNpcackServermove::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNpcackServermove::kOpcode);
  writer.FixedString(packet.mapName, 16);
  writer.U16(static_cast<std::uint16_t>(packet.xPos));
  writer.U16(static_cast<std::uint16_t>(packet.yPos));
  writer.U32(static_cast<std::uint32_t>(packet.ip));
  writer.U16(static_cast<std::uint16_t>(packet.port));
  writer.FixedString(packet.domain, 128);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcStopmove& packet) {
  if (frame.size() != ZcStopmove::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcStopmove::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.xPos = static_cast<std::uint16_t>(reader.U16());
  packet.yPos = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcStopmove& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcStopmove::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcStopmove::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U16(static_cast<std::uint16_t>(packet.xPos));
  writer.U16(static_cast<std::uint16_t>(packet.yPos));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSelectDealtype& packet) {
  if (frame.size() != ZcSelectDealtype::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSelectDealtype::kOpcode) {
    return false;
  }
  packet.npcId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSelectDealtype& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSelectDealtype::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSelectDealtype::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.npcId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPcSellItemlist& packet) {
  if (frame.size() < ZcPcSellItemlist::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPcSellItemlist::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcPcSellItemlistSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.items.resize(reader.remaining() / ZcPcSellItemlistSub::kWireSize);
    for (ZcPcSellItemlistSub& element : packet.items) {
      ReadZcPcSellItemlistSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPcSellItemlist& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPcSellItemlist::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPcSellItemlist::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcPcSellItemlistSub& element : packet.items) {
    WriteZcPcSellItemlistSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCloseDialog& packet) {
  if (frame.size() != ZcCloseDialog::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCloseDialog::kOpcode) {
    return false;
  }
  packet.npcId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCloseDialog& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCloseDialog::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCloseDialog::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.npcId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMenuList& packet) {
  if (frame.size() < ZcMenuList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMenuList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.npcId = static_cast<std::uint32_t>(reader.U32());
  packet.menu = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMenuList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMenuList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMenuList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.npcId));
  writer.Text(packet.menu);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcOpenEditdlg& packet) {
  if (frame.size() != ZcOpenEditdlg::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcOpenEditdlg::kOpcode) {
    return false;
  }
  packet.npcId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcOpenEditdlg& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcOpenEditdlg::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcOpenEditdlg::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.npcId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcOpenEditdlgstr& packet) {
  if (frame.size() != ZcOpenEditdlgstr::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcOpenEditdlgstr::kOpcode) {
    return false;
  }
  packet.npcId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcOpenEditdlgstr& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcOpenEditdlgstr::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcOpenEditdlgstr::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.npcId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckReqAllyGuild& packet) {
  if (frame.size() != ZcAckReqAllyGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckReqAllyGuild::kOpcode) {
    return false;
  }
  packet.flag = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckReqAllyGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckReqAllyGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckReqAllyGuild::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.flag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDeleteRelatedGuild& packet) {
  if (frame.size() != ZcDeleteRelatedGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDeleteRelatedGuild::kOpcode) {
    return false;
  }
  packet.allyID = static_cast<std::uint32_t>(reader.U32());
  packet.flag = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDeleteRelatedGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDeleteRelatedGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDeleteRelatedGuild::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.allyID));
  writer.U32(static_cast<std::uint32_t>(packet.flag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcReqAllyGuild& packet) {
  if (frame.size() != ZcReqAllyGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcReqAllyGuild::kOpcode) {
    return false;
  }
  packet.inviterId = static_cast<std::uint32_t>(reader.U32());
  packet.inviterGuildName = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcReqAllyGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcReqAllyGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcReqAllyGuild::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.inviterId));
  writer.FixedString(packet.inviterGuildName, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckReqHostileGuild& packet) {
  if (frame.size() != ZcAckReqHostileGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckReqHostileGuild::kOpcode) {
    return false;
  }
  packet.flag = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckReqHostileGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckReqHostileGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckReqHostileGuild::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.flag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCompass& packet) {
  if (frame.size() != ZcCompass::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCompass::kOpcode) {
    return false;
  }
  packet.npcId = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::uint32_t>(reader.U32());
  packet.xPos = static_cast<std::uint32_t>(reader.U32());
  packet.yPos = static_cast<std::uint32_t>(reader.U32());
  packet.id = static_cast<std::uint8_t>(reader.U8());
  packet.color = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCompass& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCompass::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCompass::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.npcId));
  writer.U32(static_cast<std::uint32_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.xPos));
  writer.U32(static_cast<std::uint32_t>(packet.yPos));
  writer.U8(static_cast<std::uint8_t>(packet.id));
  writer.U32(static_cast<std::uint32_t>(packet.color));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcItemThrowAck& packet) {
  if (frame.size() != ZcItemThrowAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcItemThrowAck::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.count = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcItemThrowAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcItemThrowAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcItemThrowAck::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U16(static_cast<std::uint16_t>(packet.count));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDeleteItemFromBody& packet) {
  if (frame.size() != ZcDeleteItemFromBody::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDeleteItemFromBody::kOpcode) {
    return false;
  }
  packet.deleteType = static_cast<std::int16_t>(reader.I16());
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.count = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDeleteItemFromBody& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDeleteItemFromBody::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDeleteItemFromBody::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.deleteType));
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.I16(static_cast<std::int16_t>(packet.count));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCartoff& /*packet*/) {
  if (frame.size() != ZcCartoff::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCartoff::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCartoff& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCartoff::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCartoff::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckGuildMenuinterface& packet) {
  if (frame.size() != ZcAckGuildMenuinterface::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckGuildMenuinterface::kOpcode) {
    return false;
  }
  packet.menuFlag = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckGuildMenuinterface& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckGuildMenuinterface::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckGuildMenuinterface::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.menuFlag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckChangeGuildPositioninfo& packet) {
  if (frame.size() < ZcAckChangeGuildPositioninfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckChangeGuildPositioninfo::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcAckChangeGuildPositioninfoSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.posInfo.resize(reader.remaining() / ZcAckChangeGuildPositioninfoSub::kWireSize);
    for (ZcAckChangeGuildPositioninfoSub& element : packet.posInfo) {
      ReadZcAckChangeGuildPositioninfoSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckChangeGuildPositioninfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckChangeGuildPositioninfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckChangeGuildPositioninfo::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcAckChangeGuildPositioninfoSub& element : packet.posInfo) {
    WriteZcAckChangeGuildPositioninfoSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyPositionToGuildm& packet) {
  if (frame.size() != ZcNotifyPositionToGuildm::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyPositionToGuildm::kOpcode) {
    return false;
  }
  packet.aid = static_cast<std::uint32_t>(reader.U32());
  packet.xPos = static_cast<std::int16_t>(reader.I16());
  packet.yPos = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyPositionToGuildm& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyPositionToGuildm::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyPositionToGuildm::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.aid));
  writer.I16(static_cast<std::int16_t>(packet.xPos));
  writer.I16(static_cast<std::int16_t>(packet.yPos));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGuildChat& packet) {
  if (frame.size() < ZcGuildChat::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGuildChat::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.message = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGuildChat& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGuildChat::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGuildChat::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.Text(packet.message);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUpdateCharstat& packet) {
  if (frame.size() != ZcUpdateCharstat::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUpdateCharstat::kOpcode) {
    return false;
  }
  packet.aid = static_cast<std::uint32_t>(reader.U32());
  packet.cid = static_cast<std::uint32_t>(reader.U32());
  packet.status = static_cast<std::uint32_t>(reader.U32());
  packet.gender = static_cast<std::uint16_t>(reader.U16());
  packet.hairStyle = static_cast<std::uint16_t>(reader.U16());
  packet.hairColor = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUpdateCharstat& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUpdateCharstat::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUpdateCharstat::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.aid));
  writer.U32(static_cast<std::uint32_t>(packet.cid));
  writer.U32(static_cast<std::uint32_t>(packet.status));
  writer.U16(static_cast<std::uint16_t>(packet.gender));
  writer.U16(static_cast<std::uint16_t>(packet.hairStyle));
  writer.U16(static_cast<std::uint16_t>(packet.hairColor));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckReqChangeMembers& packet) {
  if (frame.size() < ZcAckReqChangeMembers::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckReqChangeMembers::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcAckReqChangeMembersSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.members.resize(reader.remaining() / ZcAckReqChangeMembersSub::kWireSize);
    for (ZcAckReqChangeMembersSub& element : packet.members) {
      ReadZcAckReqChangeMembersSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckReqChangeMembers& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckReqChangeMembers::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckReqChangeMembers::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcAckReqChangeMembersSub& element : packet.members) {
    WriteZcAckReqChangeMembersSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcStatus& packet) {
  if (frame.size() != ZcStatus::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcStatus::kOpcode) {
    return false;
  }
  packet.point = static_cast<std::uint16_t>(reader.U16());
  packet.str = static_cast<std::uint8_t>(reader.U8());
  packet.standardStr = static_cast<std::uint8_t>(reader.U8());
  packet.agi = static_cast<std::uint8_t>(reader.U8());
  packet.standardAgi = static_cast<std::uint8_t>(reader.U8());
  packet.vit = static_cast<std::uint8_t>(reader.U8());
  packet.standardVit = static_cast<std::uint8_t>(reader.U8());
  packet.int_ = static_cast<std::uint8_t>(reader.U8());
  packet.standardInt = static_cast<std::uint8_t>(reader.U8());
  packet.dex = static_cast<std::uint8_t>(reader.U8());
  packet.standardDex = static_cast<std::uint8_t>(reader.U8());
  packet.luk = static_cast<std::uint8_t>(reader.U8());
  packet.standardLuk = static_cast<std::uint8_t>(reader.U8());
  packet.attPower = static_cast<std::int16_t>(reader.I16());
  packet.refiningPower = static_cast<std::int16_t>(reader.I16());
  packet.max_mattPower = static_cast<std::int16_t>(reader.I16());
  packet.min_mattPower = static_cast<std::int16_t>(reader.I16());
  packet.itemdefPower = static_cast<std::int16_t>(reader.I16());
  packet.plusdefPower = static_cast<std::int16_t>(reader.I16());
  packet.mdefPower = static_cast<std::int16_t>(reader.I16());
  packet.plusmdefPower = static_cast<std::int16_t>(reader.I16());
  packet.hitSuccessValue = static_cast<std::int16_t>(reader.I16());
  packet.avoidSuccessValue = static_cast<std::int16_t>(reader.I16());
  packet.plusAvoidSuccessValue = static_cast<std::int16_t>(reader.I16());
  packet.criticalSuccessValue = static_cast<std::int16_t>(reader.I16());
  packet.ASPD = static_cast<std::int16_t>(reader.I16());
  packet.plusASPD = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcStatus& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcStatus::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcStatus::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.point));
  writer.U8(static_cast<std::uint8_t>(packet.str));
  writer.U8(static_cast<std::uint8_t>(packet.standardStr));
  writer.U8(static_cast<std::uint8_t>(packet.agi));
  writer.U8(static_cast<std::uint8_t>(packet.standardAgi));
  writer.U8(static_cast<std::uint8_t>(packet.vit));
  writer.U8(static_cast<std::uint8_t>(packet.standardVit));
  writer.U8(static_cast<std::uint8_t>(packet.int_));
  writer.U8(static_cast<std::uint8_t>(packet.standardInt));
  writer.U8(static_cast<std::uint8_t>(packet.dex));
  writer.U8(static_cast<std::uint8_t>(packet.standardDex));
  writer.U8(static_cast<std::uint8_t>(packet.luk));
  writer.U8(static_cast<std::uint8_t>(packet.standardLuk));
  writer.I16(static_cast<std::int16_t>(packet.attPower));
  writer.I16(static_cast<std::int16_t>(packet.refiningPower));
  writer.I16(static_cast<std::int16_t>(packet.max_mattPower));
  writer.I16(static_cast<std::int16_t>(packet.min_mattPower));
  writer.I16(static_cast<std::int16_t>(packet.itemdefPower));
  writer.I16(static_cast<std::int16_t>(packet.plusdefPower));
  writer.I16(static_cast<std::int16_t>(packet.mdefPower));
  writer.I16(static_cast<std::int16_t>(packet.plusmdefPower));
  writer.I16(static_cast<std::int16_t>(packet.hitSuccessValue));
  writer.I16(static_cast<std::int16_t>(packet.avoidSuccessValue));
  writer.I16(static_cast<std::int16_t>(packet.plusAvoidSuccessValue));
  writer.I16(static_cast<std::int16_t>(packet.criticalSuccessValue));
  writer.I16(static_cast<std::int16_t>(packet.ASPD));
  writer.I16(static_cast<std::int16_t>(packet.plusASPD));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyMapinfo& packet) {
  if (frame.size() != ZcNotifyMapinfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyMapinfo::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyMapinfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyMapinfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyMapinfo::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckRememberWarppoint& packet) {
  if (frame.size() != ZcAckRememberWarppoint::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckRememberWarppoint::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckRememberWarppoint& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckRememberWarppoint::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckRememberWarppoint::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDispel& packet) {
  if (frame.size() != ZcDispel::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDispel::kOpcode) {
    return false;
  }
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDispel& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDispel::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDispel::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcResurrection& packet) {
  if (frame.size() != ZcResurrection::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcResurrection::kOpcode) {
    return false;
  }
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcResurrection& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcResurrection::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcResurrection::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  writer.I16(static_cast<std::int16_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyMapproperty2& packet) {
  if (frame.size() != ZcNotifyMapproperty2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyMapproperty2::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyMapproperty2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyMapproperty2::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyMapproperty2::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckItemrefining& packet) {
  if (frame.size() != ZcAckItemrefining::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckItemrefining::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint16_t>(reader.U16());
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.value = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckItemrefining& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckItemrefining::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckItemrefining::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.result));
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U16(static_cast<std::uint16_t>(packet.value));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcParChangeUser& packet) {
  if (frame.size() != ZcParChangeUser::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcParChangeUser::kOpcode) {
    return false;
  }
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::int16_t>(reader.I16());
  packet.value = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcParChangeUser& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcParChangeUser::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcParChangeUser::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  writer.I16(static_cast<std::int16_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.value));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcChangeChatroom& packet) {
  if (frame.size() < ZcChangeChatroom::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcChangeChatroom::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.ownerId = static_cast<std::uint32_t>(reader.U32());
  packet.chatId = static_cast<std::uint32_t>(reader.U32());
  packet.limit = static_cast<std::uint16_t>(reader.U16());
  packet.users = static_cast<std::uint16_t>(reader.U16());
  packet.flag = static_cast<std::uint8_t>(reader.U8());
  packet.title = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcChangeChatroom& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcChangeChatroom::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcChangeChatroom::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.ownerId));
  writer.U32(static_cast<std::uint32_t>(packet.chatId));
  writer.U16(static_cast<std::uint16_t>(packet.limit));
  writer.U16(static_cast<std::uint16_t>(packet.users));
  writer.U8(static_cast<std::uint8_t>(packet.flag));
  writer.Text(packet.title);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcEquipArrow& packet) {
  if (frame.size() != ZcEquipArrow::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcEquipArrow::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcEquipArrow& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcEquipArrow::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcEquipArrow::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCloseStore& /*packet*/) {
  if (frame.size() != ZcCloseStore::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCloseStore::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCloseStore& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCloseStore::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCloseStore::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDeleteItemFromStore& packet) {
  if (frame.size() != ZcDeleteItemFromStore::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDeleteItemFromStore::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDeleteItemFromStore& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDeleteItemFromStore::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDeleteItemFromStore::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyStoreitemCountinfo& packet) {
  if (frame.size() != ZcNotifyStoreitemCountinfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyStoreitemCountinfo::kOpcode) {
    return false;
  }
  packet.amount = static_cast<std::uint16_t>(reader.U16());
  packet.max_amount = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyStoreitemCountinfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyStoreitemCountinfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyStoreitemCountinfo::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.amount));
  writer.U16(static_cast<std::uint16_t>(packet.max_amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcExchangeitemUndo& /*packet*/) {
  if (frame.size() != ZcExchangeitemUndo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcExchangeitemUndo::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcExchangeitemUndo& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcExchangeitemUndo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcExchangeitemUndo::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcExecExchangeItem& packet) {
  if (frame.size() != ZcExecExchangeItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcExecExchangeItem::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcExecExchangeItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcExecExchangeItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcExecExchangeItem::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCancelExchangeItem& /*packet*/) {
  if (frame.size() != ZcCancelExchangeItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCancelExchangeItem::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCancelExchangeItem& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCancelExchangeItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCancelExchangeItem::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcConcludeExchangeItem& packet) {
  if (frame.size() != ZcConcludeExchangeItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcConcludeExchangeItem::kOpcode) {
    return false;
  }
  packet.who = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcConcludeExchangeItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcConcludeExchangeItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcConcludeExchangeItem::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.who));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckCreateChatroom& packet) {
  if (frame.size() != ZcAckCreateChatroom::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckCreateChatroom::kOpcode) {
    return false;
  }
  packet.flag = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckCreateChatroom& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckCreateChatroom::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckCreateChatroom::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.flag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcRefuseEnterRoom& packet) {
  if (frame.size() != ZcRefuseEnterRoom::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcRefuseEnterRoom::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcRefuseEnterRoom& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcRefuseEnterRoom::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcRefuseEnterRoom::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDestroyRoom& packet) {
  if (frame.size() != ZcDestroyRoom::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDestroyRoom::kOpcode) {
    return false;
  }
  packet.chatId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDestroyRoom& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDestroyRoom::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDestroyRoom::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.chatId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcEnterRoom& packet) {
  if (frame.size() < ZcEnterRoom::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcEnterRoom::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.chatId = static_cast<std::uint32_t>(reader.U32());
  if (reader.remaining() % ZcEnterRoomSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.members.resize(reader.remaining() / ZcEnterRoomSub::kWireSize);
    for (ZcEnterRoomSub& element : packet.members) {
      ReadZcEnterRoomSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcEnterRoom& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcEnterRoom::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcEnterRoom::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.chatId));
  for (const ZcEnterRoomSub& element : packet.members) {
    WriteZcEnterRoomSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNpcShowefstUpdate& packet) {
  if (frame.size() != ZcNpcShowefstUpdate::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNpcShowefstUpdate::kOpcode) {
    return false;
  }
  packet.gid = static_cast<std::uint32_t>(reader.U32());
  packet.effectState = static_cast<std::uint32_t>(reader.U32());
  packet.level = static_cast<std::int32_t>(reader.I32());
  packet.showEFST = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNpcShowefstUpdate& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNpcShowefstUpdate::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNpcShowefstUpdate::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.gid));
  writer.U32(static_cast<std::uint32_t>(packet.effectState));
  writer.I32(static_cast<std::int32_t>(packet.level));
  writer.U32(static_cast<std::uint32_t>(packet.showEFST));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcActionFailure& packet) {
  if (frame.size() != ZcActionFailure::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcActionFailure::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcActionFailure& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcActionFailure::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcActionFailure::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyEffect& packet) {
  if (frame.size() != ZcNotifyEffect::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyEffect::kOpcode) {
    return false;
  }
  packet.aid = static_cast<std::uint32_t>(reader.U32());
  packet.effectId = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyEffect& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyEffect::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyEffect::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.aid));
  writer.U32(static_cast<std::uint32_t>(packet.effectId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckItemcomposition& packet) {
  if (frame.size() != ZcAckItemcomposition::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckItemcomposition::kOpcode) {
    return false;
  }
  packet.equipIndex = static_cast<std::uint16_t>(reader.U16());
  packet.cardIndex = static_cast<std::uint16_t>(reader.U16());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckItemcomposition& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckItemcomposition::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckItemcomposition::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.equipIndex));
  writer.U16(static_cast<std::uint16_t>(packet.cardIndex));
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckItemidentify& packet) {
  if (frame.size() != ZcAckItemidentify::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckItemidentify::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckItemidentify& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckItemidentify::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckItemidentify::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckItemrepair& packet) {
  if (frame.size() != ZcAckItemrepair::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckItemrepair::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckItemrepair& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckItemrepair::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckItemrepair::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcEquipitemDamaged& packet) {
  if (frame.size() != ZcEquipitemDamaged::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcEquipitemDamaged::kOpcode) {
    return false;
  }
  packet.equipLocation = static_cast<std::uint16_t>(reader.U16());
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcEquipitemDamaged& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcEquipitemDamaged::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcEquipitemDamaged::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.equipLocation));
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDeleteItemFromCart& packet) {
  if (frame.size() != ZcDeleteItemFromCart::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDeleteItemFromCart::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDeleteItemFromCart& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDeleteItemFromCart::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDeleteItemFromCart::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.I32(static_cast<std::int32_t>(packet.amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcOpenstore& packet) {
  if (frame.size() != ZcOpenstore::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcOpenstore::kOpcode) {
    return false;
  }
  packet.num = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcOpenstore& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcOpenstore::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcOpenstore::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.num));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPcPurchaseResultFrommc& packet) {
  if (frame.size() != ZcPcPurchaseResultFrommc::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPcPurchaseResultFrommc::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::uint16_t>(reader.U16());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPcPurchaseResultFrommc& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPcPurchaseResultFrommc::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPcPurchaseResultFrommc::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U16(static_cast<std::uint16_t>(packet.amount));
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckOpenstore2& packet) {
  if (frame.size() != ZcAckOpenstore2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckOpenstore2::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckOpenstore2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckOpenstore2::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckOpenstore2::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSkillDisappear& packet) {
  if (frame.size() != ZcSkillDisappear::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSkillDisappear::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSkillDisappear& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSkillDisappear::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSkillDisappear::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSkillinfoDelete& packet) {
  if (frame.size() != ZcSkillinfoDelete::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSkillinfoDelete::kOpcode) {
    return false;
  }
  packet.skillID = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSkillinfoDelete& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSkillinfoDelete::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSkillinfoDelete::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.skillID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSkillUpdate& packet) {
  if (frame.size() != ZcSkillUpdate::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSkillUpdate::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSkillUpdate& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSkillUpdate::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSkillUpdate::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcHighjump& packet) {
  if (frame.size() != ZcHighjump::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcHighjump::kOpcode) {
    return false;
  }
  packet.srcId = static_cast<std::uint32_t>(reader.U32());
  packet.x = static_cast<std::uint16_t>(reader.U16());
  packet.y = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcHighjump& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcHighjump::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcHighjump::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.srcId));
  writer.U16(static_cast<std::uint16_t>(packet.x));
  writer.U16(static_cast<std::uint16_t>(packet.y));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcRecovery& packet) {
  if (frame.size() != ZcRecovery::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcRecovery::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcRecovery& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcRecovery::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcRecovery::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.type));
  writer.I32(static_cast<std::int32_t>(packet.amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckWhisper& packet) {
  if (frame.size() != ZcAckWhisper::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckWhisper::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckWhisper& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckWhisper::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckWhisper::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckAdditemToCart& packet) {
  if (frame.size() != ZcAckAdditemToCart::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckAdditemToCart::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckAdditemToCart& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckAdditemToCart::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckAdditemToCart::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDeleteitemFromMcstore& packet) {
  if (frame.size() != ZcDeleteitemFromMcstore::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDeleteitemFromMcstore::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::uint16_t>(reader.U16());
  packet.buyerCID = static_cast<std::uint32_t>(reader.U32());
  packet.date = static_cast<std::uint32_t>(reader.U32());
  packet.zeny = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDeleteitemFromMcstore& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDeleteitemFromMcstore::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDeleteitemFromMcstore::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U16(static_cast<std::uint16_t>(packet.amount));
  writer.U32(static_cast<std::uint32_t>(packet.buyerCID));
  writer.U32(static_cast<std::uint32_t>(packet.date));
  writer.I32(static_cast<std::int32_t>(packet.zeny));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqBanGuild& packet) {
  if (frame.size() != CzReqBanGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqBanGuild::kOpcode) {
    return false;
  }
  packet.guild_id = static_cast<std::uint32_t>(reader.U32());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.message = reader.FixedString(40);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqBanGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqBanGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqBanGuild::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.guild_id));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.FixedString(packet.message, 40);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqLeaveGuild& packet) {
  if (frame.size() != CzReqLeaveGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqLeaveGuild::kOpcode) {
    return false;
  }
  packet.guild_id = static_cast<std::uint32_t>(reader.U32());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.message = reader.FixedString(40);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqLeaveGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqLeaveGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqLeaveGuild::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.guild_id));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.FixedString(packet.message, 40);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqDisorganizeGuild& packet) {
  if (frame.size() != CzReqDisorganizeGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqDisorganizeGuild::kOpcode) {
    return false;
  }
  packet.key = reader.FixedString(40);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqDisorganizeGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqDisorganizeGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqDisorganizeGuild::kOpcode);
  writer.FixedString(packet.key, 40);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckDisorganizeGuildResult& packet) {
  if (frame.size() != ZcAckDisorganizeGuildResult::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckDisorganizeGuildResult::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckDisorganizeGuildResult& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckDisorganizeGuildResult::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckDisorganizeGuildResult::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcResultMakeGuild& packet) {
  if (frame.size() != ZcResultMakeGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcResultMakeGuild::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcResultMakeGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcResultMakeGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcResultMakeGuild::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqJoinGuild& packet) {
  if (frame.size() != CzReqJoinGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqJoinGuild::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.inviter_AID = static_cast<std::uint32_t>(reader.U32());
  packet.inviter_CID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqJoinGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqJoinGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqJoinGuild::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.inviter_AID));
  writer.U32(static_cast<std::uint32_t>(packet.inviter_CID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckReqJoinGuild& packet) {
  if (frame.size() != ZcAckReqJoinGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckReqJoinGuild::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckReqJoinGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckReqJoinGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckReqJoinGuild::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcReqJoinGuild& packet) {
  if (frame.size() != ZcReqJoinGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcReqJoinGuild::kOpcode) {
    return false;
  }
  packet.guild_id = static_cast<std::uint32_t>(reader.U32());
  packet.guild_name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcReqJoinGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcReqJoinGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcReqJoinGuild::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.guild_id));
  writer.FixedString(packet.guild_name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzJoinGuild& packet) {
  if (frame.size() != CzJoinGuild::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzJoinGuild::kOpcode) {
    return false;
  }
  packet.guild_id = static_cast<std::uint32_t>(reader.U32());
  packet.answer = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzJoinGuild& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzJoinGuild::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzJoinGuild::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.guild_id));
  writer.I32(static_cast<std::int32_t>(packet.answer));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGuildNotice& packet) {
  if (frame.size() != ZcGuildNotice::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGuildNotice::kOpcode) {
    return false;
  }
  packet.subject = reader.FixedString(60);
  packet.notice = reader.FixedString(120);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGuildNotice& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGuildNotice::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGuildNotice::kOpcode);
  writer.FixedString(packet.subject, 60);
  writer.FixedString(packet.notice, 120);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqJoinGuild2& packet) {
  if (frame.size() != CzReqJoinGuild2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqJoinGuild2::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqJoinGuild2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqJoinGuild2::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqJoinGuild2::kOpcode);
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqJoinGroup& packet) {
  if (frame.size() != CzReqJoinGroup::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqJoinGroup::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqJoinGroup& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqJoinGroup::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqJoinGroup::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzJoinGroup& packet) {
  if (frame.size() != CzJoinGroup::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzJoinGroup::kOpcode) {
    return false;
  }
  packet.party_id = static_cast<std::uint32_t>(reader.U32());
  packet.flag = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzJoinGroup& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzJoinGroup::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzJoinGroup::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.party_id));
  writer.I32(static_cast<std::int32_t>(packet.flag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqLeaveGroup& /*packet*/) {
  if (frame.size() != CzReqLeaveGroup::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqLeaveGroup::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqLeaveGroup& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqLeaveGroup::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqLeaveGroup::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqExpelGroupMember& packet) {
  if (frame.size() != CzReqExpelGroupMember::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqExpelGroupMember::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqExpelGroupMember& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqExpelGroupMember::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqExpelGroupMember::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzPartyJoinReq& packet) {
  if (frame.size() != CzPartyJoinReq::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzPartyJoinReq::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzPartyJoinReq& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzPartyJoinReq::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzPartyJoinReq::kOpcode);
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzPartyJoinReqAck& packet) {
  if (frame.size() != CzPartyJoinReqAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzPartyJoinReqAck::kOpcode) {
    return false;
  }
  packet.party_id = static_cast<std::uint32_t>(reader.U32());
  packet.flag = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzPartyJoinReqAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzPartyJoinReqAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzPartyJoinReqAck::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.party_id));
  writer.U8(static_cast<std::uint8_t>(packet.flag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcElParChange& packet) {
  if (frame.size() != ZcElParChange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcElParChange::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint16_t>(reader.U16());
  packet.value = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcElParChange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcElParChange::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcElParChange::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.value));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqEmotion& packet) {
  if (frame.size() != CzReqEmotion::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqEmotion::kOpcode) {
    return false;
  }
  packet.emotion_type = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqEmotion& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqEmotion::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqEmotion::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.emotion_type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyAct& packet) {
  if (frame.size() != ZcNotifyAct::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyAct::kOpcode) {
    return false;
  }
  packet.srcID = static_cast<std::int32_t>(reader.I32());
  packet.targetID = static_cast<std::int32_t>(reader.I32());
  packet.serverTick = static_cast<std::int32_t>(reader.I32());
  packet.srcSpeed = static_cast<std::int32_t>(reader.I32());
  packet.dmgSpeed = static_cast<std::int32_t>(reader.I32());
  packet.damage = static_cast<std::int32_t>(reader.I32());
  packet.isSPDamage = static_cast<std::int8_t>(reader.I8());
  packet.div = static_cast<std::uint16_t>(reader.U16());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.damage2 = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyAct& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyAct::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyAct::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.srcID));
  writer.I32(static_cast<std::int32_t>(packet.targetID));
  writer.I32(static_cast<std::int32_t>(packet.serverTick));
  writer.I32(static_cast<std::int32_t>(packet.srcSpeed));
  writer.I32(static_cast<std::int32_t>(packet.dmgSpeed));
  writer.I32(static_cast<std::int32_t>(packet.damage));
  writer.I8(static_cast<std::int8_t>(packet.isSPDamage));
  writer.U16(static_cast<std::uint16_t>(packet.div));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.I32(static_cast<std::int32_t>(packet.damage2));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzRequestMovenpc& packet) {
  if (frame.size() != CzRequestMovenpc::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzRequestMovenpc::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  for (std::uint8_t& element : packet.PosDir) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzRequestMovenpc& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzRequestMovenpc::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzRequestMovenpc::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  for (const std::uint8_t element : packet.PosDir) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNpcspriteChange& packet) {
  if (frame.size() != ZcNpcspriteChange::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNpcspriteChange::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.class_ = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNpcspriteChange& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNpcspriteChange::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNpcspriteChange::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.class_));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMemberNewentry& packet) {
  if (frame.size() != ZcMemberNewentry::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMemberNewentry::kOpcode) {
    return false;
  }
  packet.count = static_cast<std::uint16_t>(reader.U16());
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMemberNewentry& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMemberNewentry::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMemberNewentry::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.count));
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMemberExit& packet) {
  if (frame.size() != ZcMemberExit::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMemberExit::kOpcode) {
    return false;
  }
  packet.count = static_cast<std::uint16_t>(reader.U16());
  packet.name = reader.FixedString(24);
  packet.kicked = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMemberExit& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMemberExit::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMemberExit::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.count));
  writer.FixedString(packet.name, 24);
  writer.U8(static_cast<std::uint8_t>(packet.kicked));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMovetoMap& packet) {
  if (frame.size() != CzMovetoMap::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMovetoMap::kOpcode) {
    return false;
  }
  packet.map = reader.FixedString(16);
  packet.x = static_cast<std::uint16_t>(reader.U16());
  packet.y = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMovetoMap& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMovetoMap::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMovetoMap::kOpcode);
  writer.FixedString(packet.map, 16);
  writer.U16(static_cast<std::uint16_t>(packet.x));
  writer.U16(static_cast<std::uint16_t>(packet.y));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzBroadcast& packet) {
  if (frame.size() < CzBroadcast::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzBroadcast::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.message = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzBroadcast& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzBroadcast::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzBroadcast::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.Text(packet.message);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqWearEquip& packet) {
  if (frame.size() != CzReqWearEquip::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqWearEquip::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.position = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqWearEquip& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqWearEquip::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqWearEquip::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.position));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzAckSelectDealtype& packet) {
  if (frame.size() != CzAckSelectDealtype::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzAckSelectDealtype::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzAckSelectDealtype& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzAckSelectDealtype::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzAckSelectDealtype::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzCreateChatroom& packet) {
  if (frame.size() < CzCreateChatroom::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzCreateChatroom::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.limit = static_cast<std::uint16_t>(reader.U16());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.password = reader.FixedString(8);
  packet.title = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzCreateChatroom& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzCreateChatroom::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzCreateChatroom::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.limit));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.FixedString(packet.password, 8);
  writer.Text(packet.title);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzBlacksmithRank& /*packet*/) {
  if (frame.size() != CzBlacksmithRank::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzBlacksmithRank::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzBlacksmithRank& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzBlacksmithRank::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzBlacksmithRank::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzAlchemistRank& /*packet*/) {
  if (frame.size() != CzAlchemistRank::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzAlchemistRank::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzAlchemistRank& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzAlchemistRank::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzAlchemistRank::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcBlacksmithPoint& packet) {
  if (frame.size() != ZcBlacksmithPoint::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcBlacksmithPoint::kOpcode) {
    return false;
  }
  packet.points = static_cast<std::uint32_t>(reader.U32());
  packet.points_total = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcBlacksmithPoint& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcBlacksmithPoint::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcBlacksmithPoint::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.points));
  writer.U32(static_cast<std::uint32_t>(packet.points_total));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAlchemistPoint& packet) {
  if (frame.size() != ZcAlchemistPoint::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAlchemistPoint::kOpcode) {
    return false;
  }
  packet.points = static_cast<std::uint32_t>(reader.U32());
  packet.points_total = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAlchemistPoint& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAlchemistPoint::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAlchemistPoint::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.points));
  writer.U32(static_cast<std::uint32_t>(packet.points_total));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcTaekwonPoint& packet) {
  if (frame.size() != ZcTaekwonPoint::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcTaekwonPoint::kOpcode) {
    return false;
  }
  packet.points = static_cast<std::uint32_t>(reader.U32());
  packet.points_total = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcTaekwonPoint& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcTaekwonPoint::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcTaekwonPoint::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.points));
  writer.U32(static_cast<std::uint32_t>(packet.points_total));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzTaekwonRank& /*packet*/) {
  if (frame.size() != CzTaekwonRank::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzTaekwonRank::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzTaekwonRank& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzTaekwonRank::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzTaekwonRank::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcKillerPoint& packet) {
  if (frame.size() != ZcKillerPoint::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcKillerPoint::kOpcode) {
    return false;
  }
  packet.points = static_cast<std::uint32_t>(reader.U32());
  packet.points_total = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcKillerPoint& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcKillerPoint::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcKillerPoint::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.points));
  writer.U32(static_cast<std::uint32_t>(packet.points_total));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzKillerRank& /*packet*/) {
  if (frame.size() != CzKillerRank::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzKillerRank::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzKillerRank& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzKillerRank::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzKillerRank::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqRanking& packet) {
  if (frame.size() != CzReqRanking::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqRanking::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqRanking& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqRanking::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqRanking::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUpdateRankingPoint& packet) {
  if (frame.size() != ZcUpdateRankingPoint::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUpdateRankingPoint::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint16_t>(reader.U16());
  packet.points = static_cast<std::uint32_t>(reader.U32());
  packet.points_total = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUpdateRankingPoint& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUpdateRankingPoint::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUpdateRankingPoint::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.points));
  writer.U32(static_cast<std::uint32_t>(packet.points_total));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckRanking2& packet) {
  if (frame.size() != ZcAckRanking2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckRanking2::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint16_t>(reader.U16());
  for (std::uint32_t& element : packet.CIDs) {
    element = static_cast<std::uint32_t>(reader.U32());
  }
  for (std::uint32_t& element : packet.points) {
    element = static_cast<std::uint32_t>(reader.U32());
  }
  packet.mypoints = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckRanking2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckRanking2::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckRanking2::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.type));
  for (const std::uint32_t element : packet.CIDs) {
    writer.U32(static_cast<std::uint32_t>(element));
  }
  for (const std::uint32_t element : packet.points) {
    writer.U32(static_cast<std::uint32_t>(element));
  }
  writer.U32(static_cast<std::uint32_t>(packet.mypoints));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzLesseffect& packet) {
  if (frame.size() != CzLesseffect::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzLesseffect::kOpcode) {
    return false;
  }
  packet.state = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzLesseffect& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzLesseffect::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzLesseffect::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.state));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzActiveQuest& packet) {
  if (frame.size() != CzActiveQuest::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzActiveQuest::kOpcode) {
    return false;
  }
  packet.quest_id = static_cast<std::uint32_t>(reader.U32());
  packet.active = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzActiveQuest& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzActiveQuest::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzActiveQuest::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.quest_id));
  writer.U8(static_cast<std::uint8_t>(packet.active));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzJoinBaby& packet) {
  if (frame.size() != CzJoinBaby::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzJoinBaby::kOpcode) {
    return false;
  }
  packet.father_AID = static_cast<std::uint32_t>(reader.U32());
  packet.mother_AID = static_cast<std::uint32_t>(reader.U32());
  packet.accepted = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzJoinBaby& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzJoinBaby::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzJoinBaby::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.father_AID));
  writer.U32(static_cast<std::uint32_t>(packet.mother_AID));
  writer.U32(static_cast<std::uint32_t>(packet.accepted));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzAuctionItemSearch& packet) {
  if (frame.size() != CzAuctionItemSearch::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzAuctionItemSearch::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint16_t>(reader.U16());
  packet.auction_id = static_cast<std::uint32_t>(reader.U32());
  packet.text = reader.FixedString(24);
  packet.page = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzAuctionItemSearch& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzAuctionItemSearch::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzAuctionItemSearch::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.auction_id));
  writer.FixedString(packet.text, 24);
  writer.U16(static_cast<std::uint16_t>(packet.page));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzAuctionBuy& packet) {
  if (frame.size() != CzAuctionBuy::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzAuctionBuy::kOpcode) {
    return false;
  }
  packet.auction_id = static_cast<std::uint32_t>(reader.U32());
  packet.money = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzAuctionBuy& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzAuctionBuy::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzAuctionBuy::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.auction_id));
  writer.U32(static_cast<std::uint32_t>(packet.money));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzAuctionAdd& packet) {
  if (frame.size() != CzAuctionAdd::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzAuctionAdd::kOpcode) {
    return false;
  }
  packet.now_money = static_cast<std::uint32_t>(reader.U32());
  packet.max_money = static_cast<std::uint32_t>(reader.U32());
  packet.hours = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzAuctionAdd& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzAuctionAdd::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzAuctionAdd::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.now_money));
  writer.U32(static_cast<std::uint32_t>(packet.max_money));
  writer.U16(static_cast<std::uint16_t>(packet.hours));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDressroomOpen& packet) {
  if (frame.size() != ZcDressroomOpen::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDressroomOpen::kOpcode) {
    return false;
  }
  packet.view = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDressroomOpen& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDressroomOpen::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDressroomOpen::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.view));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcRoomNewentry& packet) {
  if (frame.size() < ZcRoomNewentry::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcRoomNewentry::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.owner = static_cast<std::int32_t>(reader.I32());
  packet.id = static_cast<std::int32_t>(reader.I32());
  packet.limit = static_cast<std::uint16_t>(reader.U16());
  packet.users = static_cast<std::uint16_t>(reader.U16());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.title = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcRoomNewentry& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcRoomNewentry::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcRoomNewentry::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I32(static_cast<std::int32_t>(packet.owner));
  writer.I32(static_cast<std::int32_t>(packet.id));
  writer.U16(static_cast<std::uint16_t>(packet.limit));
  writer.U16(static_cast<std::uint16_t>(packet.users));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.Text(packet.title);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMonsterInfo& packet) {
  if (frame.size() != ZcMonsterInfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMonsterInfo::kOpcode) {
    return false;
  }
  packet.class_ = static_cast<std::uint16_t>(reader.U16());
  packet.level = static_cast<std::uint16_t>(reader.U16());
  packet.size = static_cast<std::uint16_t>(reader.U16());
  packet.hp = static_cast<std::uint32_t>(reader.U32());
  packet.def = static_cast<std::int16_t>(reader.I16());
  packet.race = static_cast<std::uint16_t>(reader.U16());
  packet.mdef = static_cast<std::int16_t>(reader.I16());
  packet.element = static_cast<std::uint16_t>(reader.U16());
  packet.water = static_cast<std::uint8_t>(reader.U8());
  packet.earth = static_cast<std::uint8_t>(reader.U8());
  packet.fire = static_cast<std::uint8_t>(reader.U8());
  packet.wind = static_cast<std::uint8_t>(reader.U8());
  packet.poison = static_cast<std::uint8_t>(reader.U8());
  packet.holy = static_cast<std::uint8_t>(reader.U8());
  packet.shadow = static_cast<std::uint8_t>(reader.U8());
  packet.ghost = static_cast<std::uint8_t>(reader.U8());
  packet.undead = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMonsterInfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMonsterInfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMonsterInfo::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.class_));
  writer.U16(static_cast<std::uint16_t>(packet.level));
  writer.U16(static_cast<std::uint16_t>(packet.size));
  writer.U32(static_cast<std::uint32_t>(packet.hp));
  writer.I16(static_cast<std::int16_t>(packet.def));
  writer.U16(static_cast<std::uint16_t>(packet.race));
  writer.I16(static_cast<std::int16_t>(packet.mdef));
  writer.U16(static_cast<std::uint16_t>(packet.element));
  writer.U8(static_cast<std::uint8_t>(packet.water));
  writer.U8(static_cast<std::uint8_t>(packet.earth));
  writer.U8(static_cast<std::uint8_t>(packet.fire));
  writer.U8(static_cast<std::uint8_t>(packet.wind));
  writer.U8(static_cast<std::uint8_t>(packet.poison));
  writer.U8(static_cast<std::uint8_t>(packet.holy));
  writer.U8(static_cast<std::uint8_t>(packet.shadow));
  writer.U8(static_cast<std::uint8_t>(packet.ghost));
  writer.U8(static_cast<std::uint8_t>(packet.undead));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckReqnameBygid& packet) {
  if (frame.size() != ZcAckReqnameBygid::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckReqnameBygid::kOpcode) {
    return false;
  }
  packet.flag = static_cast<std::uint16_t>(reader.U16());
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckReqnameBygid& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckReqnameBygid::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckReqnameBygid::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.flag));
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPetAct& packet) {
  if (frame.size() != ZcPetAct::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPetAct::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.data = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPetAct& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPetAct::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPetAct::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.I32(static_cast<std::int32_t>(packet.data));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCombodelay& packet) {
  if (frame.size() != ZcCombodelay::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCombodelay::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.delay = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCombodelay& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCombodelay::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCombodelay::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.delay));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcBladestop& packet) {
  if (frame.size() != ZcBladestop::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcBladestop::kOpcode) {
    return false;
  }
  packet.srcId = static_cast<std::uint32_t>(reader.U32());
  packet.targetId = static_cast<std::uint32_t>(reader.U32());
  packet.flag = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcBladestop& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcBladestop::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcBladestop::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.srcId));
  writer.U32(static_cast<std::uint32_t>(packet.targetId));
  writer.U32(static_cast<std::uint32_t>(packet.flag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMvp& packet) {
  if (frame.size() != ZcMvp::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMvp::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMvp& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMvp::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMvp::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMvpGettingSpecialExp& packet) {
  if (frame.size() != ZcMvpGettingSpecialExp::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMvpGettingSpecialExp::kOpcode) {
    return false;
  }
  packet.exp = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMvpGettingSpecialExp& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMvpGettingSpecialExp::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMvpGettingSpecialExp::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.exp));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcThrowMvpitem& /*packet*/) {
  if (frame.size() != ZcThrowMvpitem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcThrowMvpitem::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcThrowMvpitem& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcThrowMvpitem::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcThrowMvpitem::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcUpdateMapinfo& packet) {
  if (frame.size() != ZcUpdateMapinfo::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcUpdateMapinfo::kOpcode) {
    return false;
  }
  packet.x = static_cast<std::int16_t>(reader.I16());
  packet.y = static_cast<std::int16_t>(reader.I16());
  packet.type = static_cast<std::int16_t>(reader.I16());
  packet.mapname = reader.FixedString(16);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcUpdateMapinfo& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcUpdateMapinfo::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcUpdateMapinfo::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.x));
  writer.I16(static_cast<std::int16_t>(packet.y));
  writer.I16(static_cast<std::int16_t>(packet.type));
  writer.FixedString(packet.mapname, 16);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqEnterRoom& packet) {
  if (frame.size() != CzReqEnterRoom::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqEnterRoom::kOpcode) {
    return false;
  }
  packet.chat_id = static_cast<std::uint32_t>(reader.U32());
  packet.password = reader.FixedString(8);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqEnterRoom& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqEnterRoom::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqEnterRoom::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.chat_id));
  writer.FixedString(packet.password, 8);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzChangeChatroom& packet) {
  if (frame.size() < CzChangeChatroom::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzChangeChatroom::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.limit = static_cast<std::uint16_t>(reader.U16());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.password = reader.FixedString(8);
  packet.title = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzChangeChatroom& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzChangeChatroom::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzChangeChatroom::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.limit));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.FixedString(packet.password, 8);
  writer.Text(packet.title);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzAddExchangeItem& packet) {
  if (frame.size() != CzAddExchangeItem::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzAddExchangeItem::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzAddExchangeItem& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzAddExchangeItem::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzAddExchangeItem::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.I32(static_cast<std::int32_t>(packet.amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoveItemFromCartToBody& packet) {
  if (frame.size() != CzMoveItemFromCartToBody::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoveItemFromCartToBody::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoveItemFromCartToBody& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoveItemFromCartToBody::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoveItemFromCartToBody::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.I32(static_cast<std::int32_t>(packet.amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzSelectWarppoint& packet) {
  if (frame.size() != CzSelectWarppoint::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzSelectWarppoint::kOpcode) {
    return false;
  }
  packet.skill_id = static_cast<std::int16_t>(reader.I16());
  packet.mapname = reader.FixedString(16);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzSelectWarppoint& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzSelectWarppoint::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzSelectWarppoint::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.skill_id));
  writer.FixedString(packet.mapname, 16);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzInputEditdlg& packet) {
  if (frame.size() != CzInputEditdlg::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzInputEditdlg::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.value = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzInputEditdlg& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzInputEditdlg::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzInputEditdlg::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.I32(static_cast<std::int32_t>(packet.value));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzInputEditdlgstr& packet) {
  if (frame.size() < CzInputEditdlgstr::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzInputEditdlgstr::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.GID = static_cast<std::int32_t>(reader.I32());
  packet.value = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzInputEditdlgstr& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzInputEditdlgstr::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzInputEditdlgstr::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.I32(static_cast<std::int32_t>(packet.GID));
  writer.Text(packet.value);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzCloseDialog& packet) {
  if (frame.size() != CzCloseDialog::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzCloseDialog::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzCloseDialog& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzCloseDialog::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzCloseDialog::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReset& packet) {
  if (frame.size() != CzReset::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReset::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReset& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReset::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReset::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoveItemFromStoreToCart& packet) {
  if (frame.size() != CzMoveItemFromStoreToCart::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoveItemFromStoreToCart::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoveItemFromStoreToCart& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoveItemFromStoreToCart::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoveItemFromStoreToCart::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.I32(static_cast<std::int32_t>(packet.amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqItemidentify& packet) {
  if (frame.size() != CzReqItemidentify::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqItemidentify::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqItemidentify& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqItemidentify::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqItemidentify::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzSelectautospell& packet) {
  if (frame.size() != CzSelectautospell::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzSelectautospell::kOpcode) {
    return false;
  }
  packet.skill_id = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzSelectautospell& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzSelectautospell::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzSelectautospell::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.skill_id));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqItemcompositionList& packet) {
  if (frame.size() != CzReqItemcompositionList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqItemcompositionList::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqItemcompositionList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqItemcompositionList::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqItemcompositionList::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqItemcomposition& packet) {
  if (frame.size() != CzReqItemcomposition::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqItemcomposition::kOpcode) {
    return false;
  }
  packet.index_card = static_cast<std::uint16_t>(reader.U16());
  packet.index_equip = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqItemcomposition& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqItemcomposition::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqItemcomposition::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index_card));
  writer.U16(static_cast<std::uint16_t>(packet.index_equip));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzLocalbroadcast& packet) {
  if (frame.size() < CzLocalbroadcast::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzLocalbroadcast::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.message = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzLocalbroadcast& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzLocalbroadcast::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzLocalbroadcast::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.Text(packet.message);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoveItemFromCartToStore& packet) {
  if (frame.size() != CzMoveItemFromCartToStore::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoveItemFromCartToStore::kOpcode) {
    return false;
  }
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoveItemFromCartToStore& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoveItemFromCartToStore::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoveItemFromCartToStore::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.I32(static_cast<std::int32_t>(packet.amount));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcFriendsState& packet) {
  if (frame.size() != ZcFriendsState::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcFriendsState::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.offline = static_cast<std::uint8_t>(reader.U8());
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcFriendsState& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcFriendsState::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcFriendsState::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.U8(static_cast<std::uint8_t>(packet.offline));
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMakeGroup& packet) {
  if (frame.size() != CzMakeGroup::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMakeGroup::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMakeGroup& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMakeGroup::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMakeGroup::kOpcode);
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMakeGroup2& packet) {
  if (frame.size() != CzMakeGroup2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMakeGroup2::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  packet.item_pickup = static_cast<std::uint8_t>(reader.U8());
  packet.item_share = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMakeGroup2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMakeGroup2::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMakeGroup2::kOpcode);
  writer.FixedString(packet.name, 24);
  writer.U8(static_cast<std::uint8_t>(packet.item_pickup));
  writer.U8(static_cast<std::uint8_t>(packet.item_share));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzGmChecker& packet) {
  if (frame.size() != CzGmChecker::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzGmChecker::kOpcode) {
    return false;
  }
  packet.mapname = reader.FixedString(16);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzGmChecker& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzGmChecker::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzGmChecker::kOpcode);
  writer.FixedString(packet.mapname, 16);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcGmChecker& packet) {
  if (frame.size() != ZcGmChecker::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcGmChecker::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::int16_t>(reader.I16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcGmChecker& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcGmChecker::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcGmChecker::kOpcode);
  writer.I16(static_cast<std::int16_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcConfigNotify& packet) {
  if (frame.size() != ZcConfigNotify::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcConfigNotify::kOpcode) {
    return false;
  }
  packet.flag = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcConfigNotify& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcConfigNotify::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcConfigNotify::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.flag));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcCongratulation& packet) {
  if (frame.size() != ZcCongratulation::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcCongratulation::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcCongratulation& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcCongratulation::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcCongratulation::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.GID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDivorce& packet) {
  if (frame.size() != ZcDivorce::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDivorce::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDivorce& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDivorce::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDivorce::kOpcode);
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcEmotion& packet) {
  if (frame.size() != ZcEmotion::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcEmotion::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::int32_t>(reader.I32());
  packet.type = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcEmotion& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcEmotion::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcEmotion::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.GID));
  writer.U8(static_cast<std::uint8_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMsg& packet) {
  if (frame.size() != ZcMsg::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMsg::kOpcode) {
    return false;
  }
  packet.msgId = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMsg& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMsg::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMsg::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.msgId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMsgSkill& packet) {
  if (frame.size() != ZcMsgSkill::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMsgSkill::kOpcode) {
    return false;
  }
  packet.skillId = static_cast<std::uint16_t>(reader.U16());
  packet.msgId = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMsgSkill& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMsgSkill::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMsgSkill::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.skillId));
  writer.I32(static_cast<std::int32_t>(packet.msgId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcMsgValue& packet) {
  if (frame.size() != ZcMsgValue::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcMsgValue::kOpcode) {
    return false;
  }
  packet.message = static_cast<std::uint16_t>(reader.U16());
  packet.value = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcMsgValue& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcMsgValue::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcMsgValue::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.message));
  writer.I32(static_cast<std::int32_t>(packet.value));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyMannerPointGiven& packet) {
  if (frame.size() != ZcNotifyMannerPointGiven::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyMannerPointGiven::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyMannerPointGiven& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyMannerPointGiven::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyMannerPointGiven::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcNotifyTime& packet) {
  if (frame.size() != ZcNotifyTime::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcNotifyTime::kOpcode) {
    return false;
  }
  packet.time = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcNotifyTime& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcNotifyTime::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcNotifyTime::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.time));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSettingWhisperPc& packet) {
  if (frame.size() != ZcSettingWhisperPc::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSettingWhisperPc::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSettingWhisperPc& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSettingWhisperPc::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSettingWhisperPc::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSettingWhisperState& packet) {
  if (frame.size() != ZcSettingWhisperState::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSettingWhisperState::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::uint8_t>(reader.U8());
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSettingWhisperState& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSettingWhisperState::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSettingWhisperState::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.type));
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcSkillmsg& packet) {
  if (frame.size() != ZcSkillmsg::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcSkillmsg::kOpcode) {
    return false;
  }
  packet.msgId = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcSkillmsg& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcSkillmsg::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcSkillmsg::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.msgId));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqEmotionExpansion& packet) {
  if (frame.size() != CzReqEmotionExpansion::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqEmotionExpansion::kOpcode) {
    return false;
  }
  for (std::uint8_t& element : packet.unknown) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqEmotionExpansion& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqEmotionExpansion::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqEmotionExpansion::kOpcode);
  for (const std::uint8_t element : packet.unknown) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqBuyCashEmotion& packet) {
  if (frame.size() != CzReqBuyCashEmotion::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqBuyCashEmotion::kOpcode) {
    return false;
  }
  packet.pack_id = static_cast<std::uint16_t>(reader.U16());
  for (std::uint8_t& element : packet.unknown) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqBuyCashEmotion& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqBuyCashEmotion::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqBuyCashEmotion::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.pack_id));
  for (const std::uint8_t element : packet.unknown) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcAckBuyCashEmotion& packet) {
  if (frame.size() != ZcAckBuyCashEmotion::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcAckBuyCashEmotion::kOpcode) {
    return false;
  }
  packet.pack_id = static_cast<std::uint16_t>(reader.U16());
  packet.has_count = static_cast<std::uint8_t>(reader.U8());
  packet.count = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcAckBuyCashEmotion& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcAckBuyCashEmotion::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcAckBuyCashEmotion::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.pack_id));
  writer.U8(static_cast<std::uint8_t>(packet.has_count));
  writer.I32(static_cast<std::int32_t>(packet.count));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcPlayCashEmotion& packet) {
  if (frame.size() != ZcPlayCashEmotion::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcPlayCashEmotion::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::uint32_t>(reader.U32());
  packet.pack_id = static_cast<std::uint16_t>(reader.U16());
  packet.emotion_id = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcPlayCashEmotion& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcPlayCashEmotion::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcPlayCashEmotion::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.GID));
  writer.U16(static_cast<std::uint16_t>(packet.pack_id));
  writer.U16(static_cast<std::uint16_t>(packet.emotion_id));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcDisappearEntry& packet) {
  if (frame.size() != ZcDisappearEntry::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcDisappearEntry::kOpcode) {
    return false;
  }
  packet.GID = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcDisappearEntry& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcDisappearEntry::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcDisappearEntry::kOpcode);
  writer.I32(static_cast<std::int32_t>(packet.GID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcEfstSetEnter& packet) {
  if (frame.size() != ZcEfstSetEnter::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcEfstSetEnter::kOpcode) {
    return false;
  }
  packet.targetID = static_cast<std::uint32_t>(reader.U32());
  packet.type = static_cast<std::uint16_t>(reader.U16());
  packet.duration = static_cast<std::uint32_t>(reader.U32());
  packet.duration2 = static_cast<std::uint32_t>(reader.U32());
  packet.val1 = static_cast<std::int32_t>(reader.I32());
  packet.val2 = static_cast<std::int32_t>(reader.I32());
  packet.val3 = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcEfstSetEnter& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcEfstSetEnter::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcEfstSetEnter::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.targetID));
  writer.U16(static_cast<std::uint16_t>(packet.type));
  writer.U32(static_cast<std::uint32_t>(packet.duration));
  writer.U32(static_cast<std::uint32_t>(packet.duration2));
  writer.I32(static_cast<std::int32_t>(packet.val1));
  writer.I32(static_cast<std::int32_t>(packet.val2));
  writer.I32(static_cast<std::int32_t>(packet.val3));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzSettingWhisperState& packet) {
  if (frame.size() != CzSettingWhisperState::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzSettingWhisperState::kOpcode) {
    return false;
  }
  packet.type = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzSettingWhisperState& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzSettingWhisperState::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzSettingWhisperState::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.type));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ZcWhisperList& packet) {
  if (frame.size() < ZcWhisperList::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ZcWhisperList::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % ZcWhisperListSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.names.resize(reader.remaining() / ZcWhisperListSub::kWireSize);
    for (ZcWhisperListSub& element : packet.names) {
      ReadZcWhisperListSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ZcWhisperList& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ZcWhisperList::kWireSize);
  ByteWriter writer(out);
  writer.U16(ZcWhisperList::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const ZcWhisperListSub& element : packet.names) {
    WriteZcWhisperListSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzAllyChat& /*packet*/) {
  if (frame.size() < CzAllyChat::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzAllyChat::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzAllyChat& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzAllyChat::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzAllyChat::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzReqReportUser& packet) {
  if (frame.size() != CzReqReportUser::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzReqReportUser::kOpcode) {
    return false;
  }
  for (std::uint8_t& element : packet.unknown) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzReqReportUser& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzReqReportUser::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzReqReportUser::kOpcode);
  for (const std::uint8_t element : packet.unknown) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzQuestStatusReq& /*packet*/) {
  if (frame.size() < CzQuestStatusReq::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzQuestStatusReq::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzQuestStatusReq& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CzQuestStatusReq::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzQuestStatusReq::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CzMoveItemToPersonal& packet) {
  if (frame.size() != CzMoveItemToPersonal::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CzMoveItemToPersonal::kOpcode) {
    return false;
  }
  packet.unknown = static_cast<std::uint32_t>(reader.U32());
  packet.index = static_cast<std::uint16_t>(reader.U16());
  packet.amount = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CzMoveItemToPersonal& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CzMoveItemToPersonal::kWireSize);
  ByteWriter writer(out);
  writer.U16(CzMoveItemToPersonal::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.unknown));
  writer.U16(static_cast<std::uint16_t>(packet.index));
  writer.U32(static_cast<std::uint32_t>(packet.amount));
  return out;
}

}  // namespace ro::proto
