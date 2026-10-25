#pragma once

// GÉNÉRÉ par tools/packetgen/structgen.py — ne pas modifier à la main.
//
// Les décalages viennent du compilateur qui construit le serveur, pas d'une
// relecture de ses sources : voir le générateur pour la raison.
//
// Source : src/map/packets_struct.hpp et packets.hpp — la partie elle-même
//
// Les noms de champs sont ceux du SERVEUR, sans retouche. Les renommer romprait
// la correspondance avec la seule source qui dise ce qu'un champ signifie.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Un paquet est décrit par les deux sources : celui qui annonce une
// exclusion, que les trois serveurs envoient. Il n'est défini qu'une fois.
#include "ro/proto/common_packets.h"

namespace ro::proto {

// La version de protocole dont ces dispositions sont tirées.
inline constexpr std::int32_t kMapPacketVersion = 20250716;

// PACKET_ZC_PAR_CHANGE
struct ZcParChange {
  static constexpr std::uint16_t kOpcode = 0x00b0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t varID = 0;
  std::int32_t count = 0;
};

// PACKET_ZC_LONGPAR_CHANGE
struct ZcLongparChange {
  static constexpr std::uint16_t kOpcode = 0x00b1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t varID = 0;
  std::int32_t amount = 0;
};

// PACKET_ZC_STATUS_CHANGE
struct ZcStatusChange {
  static constexpr std::uint16_t kOpcode = 0x00be;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::uint16_t statusID = 0;
  std::uint8_t value = 0;
};

// PACKET_ZC_NOTIFY_CARTITEM_COUNTINFO
struct ZcNotifyCartitemCountinfo {
  static constexpr std::uint16_t kOpcode = 0x0121;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::int16_t curCount = 0;
  std::int16_t maxCount = 0;
  std::int32_t curWeight = 0;
  std::int32_t maxWeight = 0;
};

// PACKET_ZC_ATTACK_RANGE
struct ZcAttackRange {
  static constexpr std::uint16_t kOpcode = 0x013a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t currentAttRange = 0;
};

// PACKET_ZC_COUPLESTATUS
struct ZcCouplestatus {
  static constexpr std::uint16_t kOpcode = 0x0141;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t statusType = 0;
  std::int32_t defaultStatus = 0;
  std::int32_t plusStatus = 0;
};

// PACKET_ZC_LONGLONGPAR_CHANGE
struct ZcLonglongparChange {
  static constexpr std::uint16_t kOpcode = 0x0acb;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint16_t varID = 0;
  std::int64_t amount = 0;
};

// EQUIPSLOTINFO
struct Equipslotinfo {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::array<std::uint32_t, 4> card = {};
};

// NORMALITEM_INFO
struct NormalitemInfo {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 34;

  std::int16_t index = 0;
  std::uint32_t ITID = 0;
  std::uint8_t type = 0;
  std::int16_t count = 0;
  std::uint32_t WearState = 0;
  Equipslotinfo slot;
  std::int32_t HireExpireDate = 0;
  std::array<std::uint8_t, 1> Flag = {};
};

// ItemOptions
struct Itemoptions {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::int16_t index = 0;
  std::int16_t value = 0;
  std::uint8_t param = 0;
};

// EQUIPITEM_INFO
struct EquipitemInfo {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 68;

  std::int16_t index = 0;
  std::uint32_t ITID = 0;
  std::uint8_t type = 0;
  std::uint32_t location = 0;
  std::uint32_t WearState = 0;
  Equipslotinfo slot;
  std::int32_t HireExpireDate = 0;
  std::uint16_t bindOnEquipType = 0;
  std::uint16_t wItemSpriteNumber = 0;
  std::uint8_t option_count = 0;
  std::array<Itemoptions, 5> option_data = {};
  std::uint8_t RefiningLevel = 0;
  std::uint8_t grade = 0;
  std::array<std::uint8_t, 1> Flag = {};
};

// packet_authok
struct PacketAuthok {
  static constexpr std::uint16_t kOpcode = 0x02eb;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 13;

  std::uint32_t startTime = 0;
  std::array<std::uint8_t, 3> PosDir = {};
  std::uint8_t xSize = 0;
  std::uint8_t ySize = 0;
  std::int16_t font = 0;
};

// packet_monster_hp
struct PacketMonsterHp {
  static constexpr std::uint16_t kOpcode = 0x0977;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t GID = 0;
  std::int32_t HP = 0;
  std::int32_t MaxHP = 0;
};

// packet_sc_notick
struct PacketScNotick {
  static constexpr std::uint16_t kOpcode = 0x0196;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::int16_t index = 0;
  std::uint32_t AID = 0;
  std::uint8_t state = 0;
};

// PACKET_ZC_ITEM_PICKUP_ACK
struct ZcItemPickupAck {
  static constexpr std::uint16_t kOpcode = 0x0b41;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 70;

  std::uint16_t Index = 0;
  std::uint16_t count = 0;
  std::uint32_t nameid = 0;
  std::uint8_t IsIdentified = 0;
  std::uint8_t IsDamaged = 0;
  Equipslotinfo slot;
  std::uint32_t location = 0;
  std::uint8_t type = 0;
  std::uint8_t result = 0;
  std::int32_t HireExpireDate = 0;
  std::uint16_t bindOnEquipType = 0;
  std::array<Itemoptions, 5> option_data = {};
  std::uint8_t favorite = 0;
  std::uint16_t look = 0;
  std::uint8_t refiningLevel = 0;
  std::uint8_t grade = 0;
};

// packet_dropflooritem
struct PacketDropflooritem {
  static constexpr std::uint16_t kOpcode = 0x0add;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 24;

  std::uint32_t ITAID = 0;
  std::uint32_t ITID = 0;
  std::uint16_t type = 0;
  std::uint8_t IsIdentified = 0;
  std::int16_t xPos = 0;
  std::int16_t yPos = 0;
  std::uint8_t subX = 0;
  std::uint8_t subY = 0;
  std::int16_t count = 0;
  std::int8_t showdropeffect = 0;
  std::int16_t dropeffectmode = 0;
};

// packet_spawn_unit
struct PacketSpawnUnit {
  static constexpr std::uint16_t kOpcode = 0x09fe;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 107;

  std::uint8_t objecttype = 0;
  std::uint32_t AID = 0;
  std::uint32_t GID = 0;
  std::int16_t speed = 0;
  std::int16_t bodyState = 0;
  std::int16_t healthState = 0;
  std::int32_t effectState = 0;
  std::int16_t job = 0;
  std::uint16_t head = 0;
  std::uint32_t weapon = 0;
  std::uint32_t shield = 0;
  std::uint16_t accessory = 0;
  std::uint16_t accessory2 = 0;
  std::uint16_t accessory3 = 0;
  std::int16_t headpalette = 0;
  std::int16_t bodypalette = 0;
  std::int16_t headDir = 0;
  std::uint16_t robe = 0;
  std::uint32_t GUID = 0;
  std::int16_t GEmblemVer = 0;
  std::int16_t honor = 0;
  std::int32_t virtue = 0;
  std::uint8_t isPKModeON = 0;
  std::uint8_t sex = 0;
  std::array<std::uint8_t, 3> PosDir = {};
  std::uint8_t xSize = 0;
  std::uint8_t ySize = 0;
  std::int16_t clevel = 0;
  std::int16_t font = 0;
  std::int32_t maxHP = 0;
  std::int32_t HP = 0;
  std::uint8_t isBoss = 0;
  std::int16_t body = 0;
  std::string name;
};

// packet_unit_walking
struct PacketUnitWalking {
  static constexpr std::uint16_t kOpcode = 0x09fd;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 114;

  std::uint8_t objecttype = 0;
  std::uint32_t AID = 0;
  std::uint32_t GID = 0;
  std::int16_t speed = 0;
  std::int16_t bodyState = 0;
  std::int16_t healthState = 0;
  std::int32_t effectState = 0;
  std::int16_t job = 0;
  std::uint16_t head = 0;
  std::uint32_t weapon = 0;
  std::uint32_t shield = 0;
  std::uint16_t accessory = 0;
  std::uint32_t moveStartTime = 0;
  std::uint16_t accessory2 = 0;
  std::uint16_t accessory3 = 0;
  std::int16_t headpalette = 0;
  std::int16_t bodypalette = 0;
  std::int16_t headDir = 0;
  std::uint16_t robe = 0;
  std::uint32_t GUID = 0;
  std::int16_t GEmblemVer = 0;
  std::int16_t honor = 0;
  std::int32_t virtue = 0;
  std::uint8_t isPKModeON = 0;
  std::uint8_t sex = 0;
  std::array<std::uint8_t, 6> MoveData = {};
  std::uint8_t xSize = 0;
  std::uint8_t ySize = 0;
  std::int16_t clevel = 0;
  std::int16_t font = 0;
  std::int32_t maxHP = 0;
  std::int32_t HP = 0;
  std::uint8_t isBoss = 0;
  std::uint16_t body = 0;
  std::string name;
};

// packet_idle_unit
struct PacketIdleUnit {
  static constexpr std::uint16_t kOpcode = 0x09ff;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 108;

  std::uint8_t objecttype = 0;
  std::uint32_t AID = 0;
  std::uint32_t GID = 0;
  std::int16_t speed = 0;
  std::int16_t bodyState = 0;
  std::int16_t healthState = 0;
  std::int32_t effectState = 0;
  std::int16_t job = 0;
  std::uint16_t head = 0;
  std::uint32_t weapon = 0;
  std::uint32_t shield = 0;
  std::uint16_t accessory = 0;
  std::uint16_t accessory2 = 0;
  std::uint16_t accessory3 = 0;
  std::int16_t headpalette = 0;
  std::int16_t bodypalette = 0;
  std::int16_t headDir = 0;
  std::uint16_t robe = 0;
  std::uint32_t GUID = 0;
  std::int16_t GEmblemVer = 0;
  std::int16_t honor = 0;
  std::int32_t virtue = 0;
  std::uint8_t isPKModeON = 0;
  std::uint8_t sex = 0;
  std::array<std::uint8_t, 3> PosDir = {};
  std::uint8_t xSize = 0;
  std::uint8_t ySize = 0;
  std::uint8_t state = 0;
  std::int16_t clevel = 0;
  std::int16_t font = 0;
  std::int32_t maxHP = 0;
  std::int32_t HP = 0;
  std::uint8_t isBoss = 0;
  std::uint16_t body = 0;
  std::string name;
};

// packet_status_change
struct PacketStatusChange {
  static constexpr std::uint16_t kOpcode = 0x0983;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 29;

  std::int16_t index = 0;
  std::uint32_t AID = 0;
  std::uint8_t state = 0;
  std::uint32_t Total = 0;
  std::uint32_t Left = 0;
  std::int32_t val1 = 0;
  std::int32_t val2 = 0;
  std::int32_t val3 = 0;
};

// packet_status_change_end
struct PacketStatusChangeEnd {
  static constexpr std::uint16_t kOpcode = 0x0196;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::int16_t index = 0;
  std::uint32_t AID = 0;
  std::uint8_t state = 0;
};

// packet_status_change2
struct PacketStatusChange2 {
  static constexpr std::uint16_t kOpcode = 0x043f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 25;

  std::int16_t index = 0;
  std::uint32_t AID = 0;
  std::uint8_t state = 0;
  std::uint32_t Left = 0;
  std::int32_t val1 = 0;
  std::int32_t val2 = 0;
  std::int32_t val3 = 0;
};

// packet_maptypeproperty2
struct PacketMaptypeproperty2 {
  static constexpr std::uint16_t kOpcode = 0x099b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int16_t type = 0;
  std::array<std::uint8_t, 4> flag = {};
};

// packet_bgqueue_ack
struct PacketBgqueueAck {
  static constexpr std::uint16_t kOpcode = 0x08d8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 27;

  std::uint8_t type = 0;
  std::string bg_name;
};

// packet_bgqueue_notice_delete
struct PacketBgqueueNoticeDelete {
  static constexpr std::uint16_t kOpcode = 0x08db;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 27;

  std::uint8_t type = 0;
  std::string bg_name;
};

// packet_bgqueue_register
struct PacketBgqueueRegister {
  static constexpr std::uint16_t kOpcode = 0x08d7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 28;

  std::int16_t type = 0;
  std::string bg_name;
};

// packet_bgqueue_update_info
struct PacketBgqueueUpdateInfo {
  static constexpr std::uint16_t kOpcode = 0x08d9;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 30;

  std::string bg_name;
  std::int32_t position = 0;
};

// packet_bgqueue_checkstate
struct PacketBgqueueCheckstate {
  static constexpr std::uint16_t kOpcode = 0x090a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::string bg_name;
};

// packet_bgqueue_revoke_req
struct PacketBgqueueRevokeReq {
  static constexpr std::uint16_t kOpcode = 0x08da;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::string bg_name;
};

// packet_bgqueue_battlebegin_ack
struct PacketBgqueueBattlebeginAck {
  static constexpr std::uint16_t kOpcode = 0x08e0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 51;

  std::uint8_t result = 0;
  std::string bg_name;
  std::string game_name;
};

// packet_bgqueue_notify_entry
struct PacketBgqueueNotifyEntry {
  static constexpr std::uint16_t kOpcode = 0x08d9;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 30;

  std::string name;
  std::int32_t position = 0;
};

// packet_bgqueue_battlebegins
struct PacketBgqueueBattlebegins {
  static constexpr std::uint16_t kOpcode = 0x08df;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 50;

  std::string bg_name;
  std::string game_name;
};

// packet_script_clear
struct PacketScriptClear {
  static constexpr std::uint16_t kOpcode = 0x08d6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t NpcID = 0;
};

// PACKET_ZC_BROADCASTING_SPECIAL_ITEM_OBTAIN_item
struct ZcBroadcastingSpecialItemObtainItem {
  static constexpr std::uint16_t kOpcode = 0x0bba;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 44;

  std::uint8_t type = 0;
  std::uint32_t ItemID = 0;
  std::int8_t len = 0;
  std::string Name;
  std::int8_t boxItemID_len = 0;
  std::uint32_t BoxItemID = 0;
  std::int8_t refineLevel_len = 0;
  std::uint32_t refineLevel = 0;
};

// packet_item_drop_announce
struct PacketItemDropAnnounce {
  static constexpr std::uint16_t kOpcode = 0x07fd;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 59;

  std::uint8_t type = 0;
  std::uint32_t ItemID = 0;
  std::int8_t len = 0;
  std::string Name;
  std::string monsterNameLen;
  std::string monsterName;
};

// packet_cart_additem_ack
struct PacketCartAdditemAck {
  static constexpr std::uint16_t kOpcode = 0x012c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::int8_t result = 0;
};

// packet_banking_check
struct PacketBankingCheck {
  static constexpr std::uint16_t kOpcode = 0x09a6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::int64_t Money = 0;
  std::int16_t Reason = 0;
};

// packet_banking_deposit_req
struct PacketBankingDepositReq {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t AID = 0;
  std::int32_t Money = 0;
};

// packet_banking_withdraw_req
struct PacketBankingWithdrawReq {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t AID = 0;
  std::int32_t Money = 0;
};

// packet_banking_deposit_ack
struct PacketBankingDepositAck {
  static constexpr std::uint16_t kOpcode = 0x09a8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::int16_t Reason = 0;
  std::int64_t Money = 0;
  std::int32_t Balance = 0;
};

// packet_banking_withdraw_ack
struct PacketBankingWithdrawAck {
  static constexpr std::uint16_t kOpcode = 0x09aa;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::int16_t Reason = 0;
  std::int64_t Money = 0;
  std::int32_t Balance = 0;
};

// packet_roulette_open_ack
struct PacketRouletteOpenAck {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 25;

  std::int8_t Result = 0;
  std::int32_t Serial = 0;
  std::int8_t Step = 0;
  std::int8_t Idx = 0;
  std::uint32_t AdditionItemID = 0;
  std::int32_t GoldPoint = 0;
  std::int32_t SilverPoint = 0;
  std::int32_t BronzePoint = 0;
};

// packet_roulette_info_ack
struct PacketRouletteInfoAck {
  static constexpr std::uint16_t kOpcode = 0x0a1c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 512;

  std::uint32_t RouletteSerial = 0;
  std::array<std::uint8_t, 504> ItemInfo = {};
};

// packet_roulette_close_ack
struct PacketRouletteCloseAck {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t Result = 0;
};

// packet_roulette_generate_ack
struct PacketRouletteGenerateAck {
  static constexpr std::uint16_t kOpcode = 0x0a20;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 23;

  std::uint8_t Result = 0;
  std::uint16_t Step = 0;
  std::uint16_t Idx = 0;
  std::uint32_t AdditionItemID = 0;
  std::int32_t RemainGold = 0;
  std::int32_t RemainSilver = 0;
  std::int32_t RemainBronze = 0;
};

// packet_roulette_itemrecv_req
struct PacketRouletteItemrecvReq {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t Condition = 0;
};

// packet_roulette_itemrecv_ack
struct PacketRouletteItemrecvAck {
  static constexpr std::uint16_t kOpcode = 0x0a22;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint8_t Result = 0;
  std::uint32_t AdditionItemID = 0;
};

// packet_itemlist_normal
struct PacketItemlistNormal {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 5;

  std::uint8_t invType = 0;
  std::vector<NormalitemInfo> list;
};

// packet_itemlist_equip
struct PacketItemlistEquip {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 5;

  std::uint8_t invType = 0;
  std::vector<EquipitemInfo> list;
};

// ZC_STORE_ITEMLIST_NORMAL
struct ZcStoreItemlistNormal {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 5;

  std::uint8_t invType = 0;
  std::vector<NormalitemInfo> list;
};

// PACKET_ZC_INVENTORY_START
struct ZcInventoryStart {
  static constexpr std::uint16_t kOpcode = 0x0b08;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 5;

  std::uint8_t invType = 0;
  std::string name;
};

// PACKET_ZC_INVENTORY_END
struct ZcInventoryEnd {
  static constexpr std::uint16_t kOpcode = 0x0b0b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint8_t invType = 0;
  std::string flag;
};

// ZC_STORE_ITEMLIST_EQUIP
struct ZcStoreItemlistEquip {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 5;

  std::uint8_t invType = 0;
  std::vector<EquipitemInfo> list;
};

// packet_equip_item
struct PacketEquipItem {
  static constexpr std::uint16_t kOpcode = 0x0998;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t index = 0;
  std::uint32_t wearLocation = 0;
};

// PACKET_ZC_REQ_WEAR_EQUIP_ACK
struct ZcReqWearEquipAck {
  static constexpr std::uint16_t kOpcode = 0x0999;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::uint16_t index = 0;
  std::uint32_t wearLocation = 0;
  std::uint16_t wItemSpriteNumber = 0;
  std::uint8_t result = 0;
};

// packet_unequipitem_ack
struct PacketUnequipitemAck {
  static constexpr std::uint16_t kOpcode = 0x099a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::uint16_t index = 0;
  std::uint32_t wearLocation = 0;
  std::uint8_t result = 0;
};

// PACKET_ZC_EQUIPWIN_MICROSCOPE
struct ZcEquipwinMicroscope {
  static constexpr std::uint16_t kOpcode = 0x0b37;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 47;

  std::string characterName;
  std::int16_t job = 0;
  std::int16_t head = 0;
  std::int16_t accessory = 0;
  std::int16_t accessory2 = 0;
  std::int16_t accessory3 = 0;
  std::int16_t robe = 0;
  std::int16_t headpalette = 0;
  std::int16_t bodypalette = 0;
  std::int16_t body2 = 0;
  std::uint8_t sex = 0;
  std::vector<EquipitemInfo> list;
};

// packet_notify_bounditem
struct PacketNotifyBounditem {
  static constexpr std::uint16_t kOpcode = 0x02d3;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t index = 0;
};

// packet_graffiti_entry
struct PacketGraffitiEntry {
  static constexpr std::uint16_t kOpcode = 0x01c9;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 97;

  std::uint32_t AID = 0;
  std::uint32_t creatorAID = 0;
  std::int16_t xPos = 0;
  std::int16_t yPos = 0;
  std::uint8_t job = 0;
  std::uint8_t isVisible = 0;
  std::uint8_t isContens = 0;
  std::string msg;
};

// packet_damage
struct PacketDamage {
  static constexpr std::uint16_t kOpcode = 0x08c8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 34;

  std::uint32_t GID = 0;
  std::uint32_t targetGID = 0;
  std::uint32_t startTime = 0;
  std::int32_t attackMT = 0;
  std::int32_t attackedMT = 0;
  std::int32_t damage = 0;
  std::uint8_t is_sp_damaged = 0;
  std::int16_t count = 0;
  std::uint8_t action = 0;
  std::int32_t leftDamage = 0;
};

// packet_gm_monster_item
struct PacketGmMonsterItem {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 102;

  std::string str;
};

// PACKET_CZ_NPC_MARKET_PURCHASE_sub
struct CzNpcMarketPurchaseSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t ITID = 0;
  std::int32_t qty = 0;
};

// PACKET_CZ_NPC_MARKET_PURCHASE
struct CzNpcMarketPurchase {
  static constexpr std::uint16_t kOpcode = 0x09d6;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<CzNpcMarketPurchaseSub> list;
};

// PACKET_ZC_NPC_MARKET_OPEN_sub
struct ZcNpcMarketOpenSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 19;

  std::uint32_t nameid = 0;
  std::uint8_t type = 0;
  std::uint32_t price = 0;
  std::uint32_t qty = 0;
  std::uint16_t weight = 0;
  std::uint32_t location = 0;
};

// PACKET_ZC_NPC_MARKET_OPEN
struct ZcNpcMarketOpen {
  static constexpr std::uint16_t kOpcode = 0x0b7a;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcNpcMarketOpenSub> list;
};

// packet_wis_end
struct PacketWisEnd {
  static constexpr std::uint16_t kOpcode = 0x09df;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::int8_t result = 0;
  std::uint32_t AID = 0;
};

// packet_party_leader_changed
struct PacketPartyLeaderChanged {
  static constexpr std::uint16_t kOpcode = 0x07fc;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t prev_leader_aid = 0;
  std::uint32_t new_leader_aid = 0;
};

// hotkey_data
struct HotkeyData {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::int8_t isSkill = 0;
  std::uint32_t id = 0;
  std::int16_t count = 0;
};

// PACKET_ZC_SHORTCUT_KEY_LIST
struct ZcShortcutKeyList {
  static constexpr std::uint16_t kOpcode = 0x0b20;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 271;

  std::int8_t rotate = 0;
  std::int16_t tab = 0;
  std::array<HotkeyData, 38> hotkey = {};
};

// PACKET_CZ_SHORTCUT_KEY_CHANGE1
struct CzShortcutKeyChange1 {
  static constexpr std::uint16_t kOpcode = 0x02ba;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::uint16_t index = 0;
  HotkeyData hotkey;
};

// PACKET_CZ_SHORTCUT_KEY_CHANGE2
struct CzShortcutKeyChange2 {
  static constexpr std::uint16_t kOpcode = 0x0b21;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 13;

  std::uint16_t tab = 0;
  std::uint16_t index = 0;
  HotkeyData hotkey;
};

// PACKET_CZ_SHORTCUTKEYBAR_ROTATE1
struct CzShortcutkeybarRotate1 {
  static constexpr std::uint16_t kOpcode = 0x0a01;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t rowshift = 0;
};

// PACKET_CZ_SHORTCUTKEYBAR_ROTATE2
struct CzShortcutkeybarRotate2 {
  static constexpr std::uint16_t kOpcode = 0x0b22;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::uint16_t tab = 0;
  std::uint8_t rowshift = 0;
};

// packet_mission_info_sub
struct PacketMissionInfoSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 48;

  std::uint32_t huntIdent = 0;
  std::uint32_t huntIdent2 = 0;
  std::uint32_t mobType = 0;
  std::uint32_t mob_id = 0;
  std::int16_t levelMin = 0;
  std::int16_t levelMax = 0;
  std::int16_t huntCount = 0;
  std::int16_t maxCount = 0;
  std::string mobName;
};

// packet_quest_list_header
struct PacketQuestListHeader {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int32_t questCount = 0;
};

// packet_chat_message
struct PacketChatMessage {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::string message;
};

// packet_whisper_message
struct PacketWhisperMessage {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 28;

  std::string name;
  std::string message;
};

// PACKET_CZ_ADD_ITEM_TO_MAIL
struct CzAddItemToMail {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t index = 0;
  std::int16_t count = 0;
};

// PACKET_ZC_ACK_ADD_ITEM_RODEX
struct ZcAckAddItemRodex {
  static constexpr std::uint16_t kOpcode = 0x0b3f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 64;

  std::int8_t result = 0;
  std::int16_t index = 0;
  std::int16_t count = 0;
  std::uint32_t itemId = 0;
  std::int8_t type = 0;
  std::int8_t IsIdentified = 0;
  std::int8_t IsDamaged = 0;
  Equipslotinfo slot;
  std::array<Itemoptions, 5> optionData = {};
  std::int16_t weight = 0;
  std::uint8_t favorite = 0;
  std::uint32_t location = 0;
  std::int8_t refiningLevel = 0;
  std::int8_t grade = 0;
};

// PACKET_CZ_REQ_OPEN_WRITE_MAIL
struct CzReqOpenWriteMail {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::string receiveName;
};

// PACKET_ZC_ACK_OPEN_WRITE_MAIL
struct ZcAckOpenWriteMail {
  static constexpr std::uint16_t kOpcode = 0x0a12;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 27;

  std::string receiveName;
  std::int8_t result = 0;
};

// PACKET_CZ_REQ_REMOVE_ITEM_MAIL
struct CzReqRemoveItemMail {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t index = 0;
  std::uint16_t cnt = 0;
};

// PACKET_ZC_ACK_REMOVE_ITEM_MAIL
struct ZcAckRemoveItemMail {
  static constexpr std::uint16_t kOpcode = 0x0a07;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::int8_t result = 0;
  std::int16_t index = 0;
  std::uint16_t cnt = 0;
  std::int16_t weight = 0;
};

// PACKET_CZ_SEND_MAIL
struct CzSendMail {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 68;

  std::string receiveName;
  std::string senderName;
  std::int64_t zeny = 0;
  std::int16_t Titlelength = 0;
  std::int16_t TextcontentsLength = 0;
  std::int32_t receiver_char_id = 0;
  std::string string;
};

// PACKET_ZC_WRITE_MAIL_RESULT
struct ZcWriteMailResult {
  static constexpr std::uint16_t kOpcode = 0x09ed;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::int8_t result = 0;
};

// PACKET_CZ_CHECKNAME1
struct CzCheckname1 {
  static constexpr std::uint16_t kOpcode = 0x0a13;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::string Name;
};

// PACKET_CZ_CHECKNAME2
struct CzCheckname2 {
  static constexpr std::uint16_t kOpcode = 0x0b97;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 27;

  std::string Name;
  std::string own_char;
};

// PACKET_ZC_CHECKNAME
struct ZcCheckname {
  static constexpr std::uint16_t kOpcode = 0x0a51;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 34;

  std::int32_t CharId = 0;
  std::int16_t Class = 0;
  std::int16_t BaseLevel = 0;
  std::string Name;
};

// PACKET_ZC_NOTIFY_UNREADMAIL
struct ZcNotifyUnreadmail {
  static constexpr std::uint16_t kOpcode = 0x09e7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::string result;
};

// PACKET_ZC_MAIL_LIST
struct ZcMailList {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::int8_t IsEnd = 0;
};

// PACKET_CZ_REQ_NEXT_MAIL_LIST
struct CzReqNextMailList {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int8_t opentype = 0;
  std::int64_t Lower_MailID = 0;
};

// PACKET_CZ_REQ_OPEN_MAIL
struct CzReqOpenMail {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::int64_t char_Upper_MailID = 0;
  std::int64_t return_Upper_MailID = 0;
  std::int64_t account_Upper_MailID = 0;
};

// PACKET_CZ_REQ_READ_MAIL
struct CzReqReadMail {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int8_t opentype = 0;
  std::int64_t MailID = 0;
};

// PACKET_ZC_ACK_READ_RODEX
struct ZcAckReadRodex {
  static constexpr std::uint16_t kOpcode = 0x0b63;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 24;

  std::int8_t opentype = 0;
  std::int64_t MailID = 0;
  std::int16_t TextcontentsLength = 0;
  std::int64_t zeny = 0;
  std::int8_t ItemCnt = 0;
  std::string Textcontent;
};

// PACKET_CZ_REQ_DELETE_MAIL
struct CzReqDeleteMail {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int8_t opentype = 0;
  std::int64_t MailID = 0;
};

// PACKET_ZC_ACK_DELETE_MAIL
struct ZcAckDeleteMail {
  static constexpr std::uint16_t kOpcode = 0x09f6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int8_t opentype = 0;
  std::int64_t MailID = 0;
};

// PACKET_CZ_REQ_REFRESH_MAIL_LIST
struct CzReqRefreshMailList {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::int64_t Upper_MailID = 0;
  std::array<std::int8_t, 16> unknown = {};
};

// PACKET_CZ_REQ_ZENY_FROM_MAIL
struct CzReqZenyFromMail {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int64_t MailID = 0;
  std::int8_t opentype = 0;
};

// PACKET_ZC_ACK_ZENY_FROM_MAIL
struct ZcAckZenyFromMail {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::int64_t MailID = 0;
  std::int8_t opentype = 0;
  std::int8_t result = 0;
};

// PACKET_CZ_REQ_ITEM_FROM_MAIL
struct CzReqItemFromMail {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int64_t MailID = 0;
  std::int8_t opentype = 0;
};

// PACKET_ZC_ACK_ITEM_FROM_MAIL
struct ZcAckItemFromMail {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::int64_t MailID = 0;
  std::int8_t opentype = 0;
  std::int8_t result = 0;
};

// PACKET_ZC_SKILL_SCALE
struct ZcSkillScale {
  static constexpr std::uint16_t kOpcode = 0x0a41;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 18;

  std::uint32_t AID = 0;
  std::int16_t skill_id = 0;
  std::int16_t skill_lv = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint32_t casttime = 0;
};

// ZC_PROGRESS_ACTOR
struct ZcProgressActor {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::int32_t GID = 0;
  std::int32_t color = 0;
  std::uint32_t time = 0;
};

// PACKET_ZC_ADD_MEMBER_TO_GROUP
struct ZcAddMemberToGroup {
  static constexpr std::uint16_t kOpcode = 0x0ae4;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 89;

  std::uint32_t AID = 0;
  std::uint32_t GID = 0;
  std::uint32_t leader = 0;
  std::int16_t class_ = 0;
  std::int16_t baseLevel = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint8_t offline = 0;
  std::string partyName;
  std::string playerName;
  std::string mapName;
  std::int8_t sharePickup = 0;
  std::int8_t shareLoot = 0;
};

// PACKET_ZC_GROUP_LIST_SUB
struct ZcGroupListSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 54;

  std::uint32_t AID = 0;
  std::uint32_t GID = 0;
  std::string playerName;
  std::string mapName;
  std::uint8_t leader = 0;
  std::uint8_t offline = 0;
  std::int16_t class_ = 0;
  std::int16_t baseLevel = 0;
};

// PACKET_ZC_GROUP_LIST
struct ZcGroupList {
  static constexpr std::uint16_t kOpcode = 0x0ae5;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 28;

  std::string partyName;
  std::vector<ZcGroupListSub> members;
};

// PACKET_ZC_CLANINFO
struct ZcClaninfo {
  static constexpr std::uint16_t kOpcode = 0x098a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 74;

  std::uint32_t ClanID = 0;
  std::string ClanName;
  std::string MasterName;
  std::string Map;
  std::uint8_t AllyCount = 0;
  std::uint8_t AntagonistCount = 0;
};

// PACKET_ZC_NOTIFY_CLAN_CONNECTINFO
struct ZcNotifyClanConnectinfo {
  static constexpr std::uint16_t kOpcode = 0x0988;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t NumConnect = 0;
  std::int16_t NumTotal = 0;
};

// PACKET_ZC_ACK_CLAN_LEAVE
struct ZcAckClanLeave {
  static constexpr std::uint16_t kOpcode = 0x0989;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_NOTIFY_CLAN_CHAT
struct ZcNotifyClanChat {
  static constexpr std::uint16_t kOpcode = 0x098e;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 28;

  std::string MemberName;
  std::string Message;
};

// packet_quest_hunt_sub
struct PacketQuestHuntSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 46;

  std::uint32_t huntIdent = 0;
  std::uint32_t huntIdent2 = 0;
  std::uint32_t mobType = 0;
  std::uint32_t mob_id = 0;
  std::int16_t levelMin = 0;
  std::int16_t levelMax = 0;
  std::int16_t huntCount = 0;
  std::string mobName;
};

// packet_quest_add_header
struct PacketQuestAddHeader {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 17;

  std::uint32_t questID = 0;
  std::uint8_t active = 0;
  std::int32_t quest_svrTime = 0;
  std::int32_t quest_endTime = 0;
  std::int16_t count = 0;
  std::vector<PacketQuestHuntSub> objectives;
};

// packet_quest_update_hunt
struct PacketQuestUpdateHunt {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::uint32_t questID = 0;
  std::uint32_t huntIdent = 0;
  std::uint32_t huntIdent2 = 0;
  std::int16_t maxCount = 0;
  std::int16_t count = 0;
};

// packet_quest_update_header
struct PacketQuestUpdateHeader {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t count = 0;
  std::vector<PacketQuestUpdateHunt> objectives;
};

// packet_quest_hunt_info_sub
struct PacketQuestHuntInfoSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t questID = 0;
  std::uint32_t mob_id = 0;
  std::int16_t maxCount = 0;
  std::int16_t count = 0;
};

// packet_quest_hunt_info
struct PacketQuestHuntInfo {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<PacketQuestHuntInfoSub> info;
};

// PACKET_ZC_FORMATSTRING_MSG
struct ZcFormatstringMsg {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t MessageId = 0;
  std::string MessageString;
};

// PACKET_ZC_FORMATSTRING_MSG_COLOR
struct ZcFormatstringMsgColor {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 10;

  std::uint16_t messageId = 0;
  std::uint32_t color = 0;
  std::string messageString;
};

// PACKET_ZC_MSG_COLOR
struct ZcMsgColor {
  static constexpr std::uint16_t kOpcode = 0x09cd;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t MessageId = 0;
  std::uint32_t MessageColor = 0;
};

// PACKET_CZ_OPEN_UI
struct CzOpenUi {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::int8_t UIType = 0;
};

// PACKET_ZC_UI_OPEN
struct ZcUiOpen {
  static constexpr std::uint16_t kOpcode = 0x0ae2;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::int8_t UIType = 0;
  std::int32_t data = 0;
};

// PACKET_ZC_UI_OPEN2
struct ZcUiOpen2 {
  static constexpr std::uint16_t kOpcode = 0x0b9a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int8_t UIType = 0;
  std::int64_t data = 0;
};

// PACKET_ZC_UI_ACTION
struct ZcUiAction {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int32_t UIType = 0;
  std::int32_t data = 0;
};

// PACKET_CZ_PRIVATE_AIRSHIP_REQUEST
struct CzPrivateAirshipRequest {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 22;

  std::string mapName;
  std::uint32_t ItemID = 0;
};

// PACKET_ZC_PRIVATE_AIRSHIP_RESPONSE
struct ZcPrivateAirshipResponse {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t flag = 0;
};

// PACKET_CZ_REQ_STYLE_CHANGE
struct CzReqStyleChange {
  static constexpr std::uint16_t kOpcode = 0x0a46;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::int16_t HeadPalette = 0;
  std::int16_t HeadStyle = 0;
  std::int16_t BodyPalette = 0;
  std::int16_t TopAccessory = 0;
  std::int16_t MidAccessory = 0;
  std::int16_t BottomAccessory = 0;
};

// PACKET_CZ_REQ_STYLE_CHANGE2
struct CzReqStyleChange2 {
  static constexpr std::uint16_t kOpcode = 0x0afc;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::int16_t HeadPalette = 0;
  std::int16_t HeadStyle = 0;
  std::int16_t BodyPalette = 0;
  std::int16_t TopAccessory = 0;
  std::int16_t MidAccessory = 0;
  std::int16_t BottomAccessory = 0;
  std::int16_t BodyStyle = 0;
};

// PACKET_ZC_STYLE_CHANGE_RES
struct ZcStyleChangeRes {
  static constexpr std::uint16_t kOpcode = 0x0a47;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::int8_t flag = 0;
};

// PACKET_CZ_PET_EVOLUTION
struct CzPetEvolution {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t EvolvedPetEggID = 0;
};

// packet_ZC_REFUSE_LOGIN
struct PacketZcRefuseLogin {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::uint32_t error_code = 0;
  std::string block_date;
};

// PACKET_ZC_NOTIFY_CHAT
struct ZcNotifyChat {
  static constexpr std::uint16_t kOpcode = 0x008d;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t GID = 0;
  std::string Message;
};

// PACKET_ZC_NOTIFY_PLAYERCHAT
struct ZcNotifyPlayerchat {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::string Message;
};

// PACKET_ZC_ITEM_ENTRY
struct ZcItemEntry {
  static constexpr std::uint16_t kOpcode = 0x009d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 19;

  std::uint32_t AID = 0;
  std::uint32_t itemId = 0;
  std::uint8_t identify = 0;
  std::uint16_t x = 0;
  std::uint16_t y = 0;
  std::uint16_t amount = 0;
  std::uint8_t subX = 0;
  std::uint8_t subY = 0;
};

// PACKET_ZC_ADD_ITEM_TO_STORE
struct ZcAddItemToStore {
  static constexpr std::uint16_t kOpcode = 0x0b44;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 58;

  std::int16_t index = 0;
  std::int32_t amount = 0;
  std::uint32_t itemId = 0;
  std::uint8_t itemType = 0;
  std::uint8_t identified = 0;
  std::uint8_t damaged = 0;
  Equipslotinfo slot;
  std::array<Itemoptions, 5> option_data = {};
  std::uint8_t refine = 0;
  std::uint8_t grade = 0;
};

// PACKET_ZC_MVP_GETTING_ITEM
struct ZcMvpGettingItem {
  static constexpr std::uint16_t kOpcode = 0x010a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t itemId = 0;
};

// PACKET_ZC_ACK_TOUSESKILL
struct ZcAckTouseskill {
  static constexpr std::uint16_t kOpcode = 0x0110;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint16_t skillId = 0;
  std::int32_t btype = 0;
  std::uint32_t itemId = 0;
  std::uint8_t flag = 0;
  std::uint8_t cause = 0;
};

// PACKET_ZC_ADD_ITEM_TO_CART
struct ZcAddItemToCart {
  static constexpr std::uint16_t kOpcode = 0x0b45;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 58;

  std::int16_t index = 0;
  std::int32_t amount = 0;
  std::uint32_t itemId = 0;
  std::uint8_t itemType = 0;
  std::uint8_t identified = 0;
  std::uint8_t damaged = 0;
  Equipslotinfo slot;
  std::array<Itemoptions, 5> option_data = {};
  std::uint8_t refine = 0;
  std::uint8_t grade = 0;
};

// PACKET_CZ_REQMAKINGITEM
struct CzReqmakingitem {
  static constexpr std::uint16_t kOpcode = 0x018e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 18;

  std::uint32_t itemId = 0;
  std::array<std::uint32_t, 3> material = {};
};

// PACKET_ZC_ACK_REQMAKINGITEM
struct ZcAckReqmakingitem {
  static constexpr std::uint16_t kOpcode = 0x018f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int16_t result = 0;
  std::uint32_t itemId = 0;
};

// PACKET_ZC_FEED_PET
struct ZcFeedPet {
  static constexpr std::uint16_t kOpcode = 0x01a3;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint8_t result = 0;
  std::uint32_t itemId = 0;
};

// PACKET_ZC_FEED_MER
struct ZcFeedMer {
  static constexpr std::uint16_t kOpcode = 0x022f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint8_t result = 0;
  std::uint32_t itemId = 0;
};

// PACKET_ZC_USE_ITEM_ACK
struct ZcUseItemAck {
  static constexpr std::uint16_t kOpcode = 0x01c8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 15;

  std::int16_t index = 0;
  std::uint32_t itemId = 0;
  std::uint32_t AID = 0;
  std::int16_t amount = 0;
  std::uint8_t result = 0;
};

// PACKET_ZC_SPRITE_CHANGE
struct ZcSpriteChange {
  static constexpr std::uint16_t kOpcode = 0x01d7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 15;

  std::uint32_t AID = 0;
  std::uint8_t type = 0;
  std::uint32_t val = 0;
  std::uint32_t val2 = 0;
};

// PACKET_ZC_ADD_EXCHANGE_ITEM
struct ZcAddExchangeItem {
  static constexpr std::uint16_t kOpcode = 0x0b42;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 62;

  std::uint32_t itemId = 0;
  std::uint8_t itemType = 0;
  std::int32_t amount = 0;
  std::uint8_t identified = 0;
  std::uint8_t damaged = 0;
  Equipslotinfo slot;
  std::array<Itemoptions, 5> option_data = {};
  std::uint32_t location = 0;
  std::uint16_t look = 0;
  std::uint8_t refine = 0;
  std::uint8_t grade = 0;
};

// PACKET_ZC_CASH_TIME_COUNTER
struct ZcCashTimeCounter {
  static constexpr std::uint16_t kOpcode = 0x0298;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t itemId = 0;
  std::uint32_t seconds = 0;
};

// PACKET_ZC_CASH_ITEM_DELETE
struct ZcCashItemDelete {
  static constexpr std::uint16_t kOpcode = 0x0299;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t index = 0;
  std::uint32_t itemId = 0;
};

// PACKET_ZC_ITEM_PICKUP_PARTY
struct ZcItemPickupParty {
  static constexpr std::uint16_t kOpcode = 0x0b67;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 33;

  std::uint32_t AID = 0;
  std::uint32_t itemId = 0;
  std::uint8_t identified = 0;
  std::uint8_t damaged = 0;
  Equipslotinfo slot;
  std::uint16_t location = 0;
  std::uint8_t itemType = 0;
  std::uint8_t refine = 0;
  std::uint8_t grade = 0;
};

// PACKET_ZC_UPDATE_ITEM_FROM_BUYING_STORE
struct ZcUpdateItemFromBuyingStore {
  static constexpr std::uint16_t kOpcode = 0x09e6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 24;

  std::uint32_t itemId = 0;
  std::uint16_t amount = 0;
  std::uint32_t zeny = 0;
  std::uint32_t zenyLimit = 0;
  std::uint32_t charId = 0;
  std::uint32_t updateTime = 0;
};

// PACKET_ZC_ACK_WEAPONREFINE
struct ZcAckWeaponrefine {
  static constexpr std::uint16_t kOpcode = 0x0223;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int32_t result = 0;
  std::uint32_t itemId = 0;
};

// PACKET_ZC_FAILED_TRADE_BUYING_STORE_TO_SELLER
struct ZcFailedTradeBuyingStoreToSeller {
  static constexpr std::uint16_t kOpcode = 0x0824;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t result = 0;
  std::uint32_t itemId = 0;
};

// REPAIRITEM_INFO2
struct RepairitemInfo2 {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 24;

  std::int16_t index = 0;
  std::uint32_t itemId = 0;
  Equipslotinfo slot;
  std::uint8_t refine = 0;
  std::uint8_t grade = 0;
};

// REPAIRITEM_INFO1
struct RepairitemInfo1 {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 23;

  std::int16_t index = 0;
  std::uint32_t itemId = 0;
  std::uint8_t refine = 0;
  Equipslotinfo slot;
};

// PACKET_CZ_REQ_ITEMREPAIR2
struct CzReqItemrepair2 {
  static constexpr std::uint16_t kOpcode = 0x0b66;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  RepairitemInfo2 item;
};

// PACKET_CZ_REQ_ITEMREPAIR1
struct CzReqItemrepair1 {
  static constexpr std::uint16_t kOpcode = 0x01fd;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 25;

  RepairitemInfo1 item;
};

// PACKET_CZ_REQ_MAKINGITEM
struct CzReqMakingitem {
  static constexpr std::uint16_t kOpcode = 0x025b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int16_t type = 0;
  std::uint32_t itemId = 0;
};

// PACKET_CZ_SSILIST_ITEM_CLICK
struct CzSsilistItemClick {
  static constexpr std::uint16_t kOpcode = 0x083c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t AID = 0;
  std::uint32_t storeId = 0;
  std::uint32_t itemId = 0;
};

// PACKET_ZC_ACK_SCHEDULER_CASHITEM_sub
struct ZcAckSchedulerCashitemSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t itemId = 0;
  std::uint32_t price = 0;
  std::uint16_t viewSprite = 0;
  std::uint32_t location = 0;
};

// PACKET_ZC_ACK_SCHEDULER_CASHITEM
struct ZcAckSchedulerCashitem {
  static constexpr std::uint16_t kOpcode = 0x08ca;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::int16_t count = 0;
  std::int16_t tabNum = 0;
  std::vector<ZcAckSchedulerCashitemSub> items;
};

// PACKET_ZC_PC_PURCHASE_MYITEMLIST_sub
struct ZcPcPurchaseMyitemlistSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 58;

  std::uint32_t price = 0;
  std::int16_t index = 0;
  std::int16_t amount = 0;
  std::uint8_t itemType = 0;
  std::uint32_t itemId = 0;
  std::uint8_t identified = 0;
  std::uint8_t damaged = 0;
  Equipslotinfo slot;
  std::array<Itemoptions, 5> option_data = {};
  std::uint8_t refine = 0;
  std::uint8_t grade = 0;
};

// PACKET_ZC_PC_PURCHASE_MYITEMLIST
struct ZcPcPurchaseMyitemlist {
  static constexpr std::uint16_t kOpcode = 0x0b40;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t AID = 0;
  std::vector<ZcPcPurchaseMyitemlistSub> items;
};

// PACKET_ZC_PC_PURCHASE_ITEMLIST_sub
struct ZcPcPurchaseItemlistSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 19;

  std::uint32_t itemId = 0;
  std::uint32_t price = 0;
  std::uint32_t discountPrice = 0;
  std::uint8_t itemType = 0;
  std::uint16_t viewSprite = 0;
  std::uint32_t location = 0;
};

// PACKET_ZC_PC_PURCHASE_ITEMLIST
struct ZcPcPurchaseItemlist {
  static constexpr std::uint16_t kOpcode = 0x0b77;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcPcPurchaseItemlistSub> items;
};

// PACKET_CZ_PC_PURCHASE_ITEMLIST_sub
struct CzPcPurchaseItemlistSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t amount = 0;
  std::uint32_t itemId = 0;
};

// PACKET_CZ_PC_PURCHASE_ITEMLIST
struct CzPcPurchaseItemlist {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<CzPcPurchaseItemlistSub> items;
};

// PACKET_CZ_REQ_OPEN_BUYING_STORE_sub
struct CzReqOpenBuyingStoreSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t itemId = 0;
  std::uint16_t amount = 0;
  std::uint32_t price = 0;
};

// PACKET_CZ_REQ_OPEN_BUYING_STORE
struct CzReqOpenBuyingStore {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 89;

  std::uint32_t zenyLimit = 0;
  std::uint8_t result = 0;
  std::string storeName;
  std::vector<CzReqOpenBuyingStoreSub> items;
};

// PACKET_ZC_MYITEMLIST_BUYING_STORE_sub
struct ZcMyitemlistBuyingStoreSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::uint32_t price = 0;
  std::uint16_t amount = 0;
  std::uint8_t itemType = 0;
  std::uint32_t itemId = 0;
};

// PACKET_ZC_MYITEMLIST_BUYING_STORE
struct ZcMyitemlistBuyingStore {
  static constexpr std::uint16_t kOpcode = 0x0813;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t AID = 0;
  std::uint32_t zenyLimit = 0;
  std::vector<ZcMyitemlistBuyingStoreSub> items;
};

// PACKET_ZC_PC_PURCHASE_ITEMLIST_FROMMC_sub
struct ZcPcPurchaseItemlistFrommcSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 64;

  std::uint32_t price = 0;
  std::uint16_t amount = 0;
  std::int16_t index = 0;
  std::uint8_t itemType = 0;
  std::uint32_t itemId = 0;
  std::uint8_t identified = 0;
  std::uint8_t damaged = 0;
  Equipslotinfo slot;
  std::array<Itemoptions, 5> option_data = {};
  std::uint32_t location = 0;
  std::uint16_t viewSprite = 0;
  std::uint8_t refine = 0;
  std::uint8_t grade = 0;
};

// PACKET_ZC_PC_PURCHASE_ITEMLIST_FROMMC
struct ZcPcPurchaseItemlistFrommc {
  static constexpr std::uint16_t kOpcode = 0x0b3d;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t AID = 0;
  std::uint32_t venderId = 0;
  std::vector<ZcPcPurchaseItemlistFrommcSub> items;
};

// PACKET_ZC_ACK_ITEMLIST_BUYING_STORE_sub
struct ZcAckItemlistBuyingStoreSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::uint32_t price = 0;
  std::uint16_t amount = 0;
  std::uint8_t itemType = 0;
  std::uint32_t itemId = 0;
};

// PACKET_ZC_ACK_ITEMLIST_BUYING_STORE
struct ZcAckItemlistBuyingStore {
  static constexpr std::uint16_t kOpcode = 0x0818;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 16;

  std::uint32_t AID = 0;
  std::uint32_t storeId = 0;
  std::uint32_t zenyLimit = 0;
  std::vector<ZcAckItemlistBuyingStoreSub> items;
};

// PACKET_CZ_REQ_TRADE_BUYING_STORE_sub
struct CzReqTradeBuyingStoreSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int16_t index = 0;
  std::uint32_t itemId = 0;
  std::uint16_t amount = 0;
};

// PACKET_CZ_REQ_TRADE_BUYING_STORE
struct CzReqTradeBuyingStore {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t AID = 0;
  std::uint32_t storeId = 0;
  std::vector<CzReqTradeBuyingStoreSub> items;
};

// PACKET_ZC_MAKABLEITEMLIST_sub
struct ZcMakableitemlistSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::uint32_t itemId = 0;
  std::array<std::uint32_t, 3> material = {};
};

// PACKET_ZC_MAKABLEITEMLIST
struct ZcMakableitemlist {
  static constexpr std::uint16_t kOpcode = 0x018d;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcMakableitemlistSub> items;
};

// PACKET_ZC_MAKINGARROW_LIST_sub
struct ZcMakingarrowListSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint32_t itemId = 0;
};

// PACKET_ZC_MAKINGARROW_LIST
struct ZcMakingarrowList {
  static constexpr std::uint16_t kOpcode = 0x01ad;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcMakingarrowListSub> items;
};

// PACKET_ZC_SKILL_SELECT_REQUEST
struct ZcSkillSelectRequest {
  static constexpr std::uint16_t kOpcode = 0x0442;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::int32_t flag = 0;
  std::vector<std::int16_t> skillIds;
};

// PACKET_CZ_SKILL_SELECT_RESPONSE
struct CzSkillSelectResponse {
  static constexpr std::uint16_t kOpcode = 0x0443;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int32_t flag = 0;
  std::int16_t selectedSkillId = 0;
};

// PACKET_ZC_NOTIFY_WEAPONITEMLIST_sub
struct ZcNotifyWeaponitemlistSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 23;

  std::int16_t index = 0;
  std::uint32_t itemId = 0;
  std::uint8_t refine = 0;
  Equipslotinfo slot;
};

// PACKET_ZC_NOTIFY_WEAPONITEMLIST
struct ZcNotifyWeaponitemlist {
  static constexpr std::uint16_t kOpcode = 0x0221;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcNotifyWeaponitemlistSub> items;
};

// PACKET_ZC_MAKINGITEM_LIST_sub
struct ZcMakingitemListSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint32_t itemId = 0;
};

// PACKET_ZC_MAKINGITEM_LIST
struct ZcMakingitemList {
  static constexpr std::uint16_t kOpcode = 0x025a;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t makeItem = 0;
  std::vector<ZcMakingitemListSub> items;
};

// PACKET_ZC_PC_CASH_POINT_ITEMLIST_sub
struct ZcPcCashPointItemlistSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 13;

  std::uint32_t price = 0;
  std::uint32_t discountPrice = 0;
  std::uint8_t itemType = 0;
  std::uint32_t itemId = 0;
};

// PACKET_ZC_PC_CASH_POINT_ITEMLIST
struct ZcPcCashPointItemlist {
  static constexpr std::uint16_t kOpcode = 0x0287;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t cashPoints = 0;
  std::uint32_t kafraPoints = 0;
  std::vector<ZcPcCashPointItemlistSub> items;
};

// PACKET_CZ_PC_BUY_CASH_POINT_ITEM_sub
struct CzPcBuyCashPointItemSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t amount = 0;
  std::uint32_t itemId = 0;
};

// PACKET_CZ_PC_BUY_CASH_POINT_ITEM
struct CzPcBuyCashPointItem {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t kafraPoints = 0;
  std::uint16_t count = 0;
  std::vector<CzPcBuyCashPointItemSub> items;
};

// PACKET_CZ_SEARCH_STORE_INFO_item
struct CzSearchStoreInfoItem {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint32_t itemId = 0;
};

// PACKET_CZ_SEARCH_STORE_INFO
struct CzSearchStoreInfo {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 15;

  std::uint8_t searchType = 0;
  std::uint32_t maxPrice = 0;
  std::uint32_t minPrice = 0;
  std::uint8_t itemsCount = 0;
  std::uint8_t cardsCount = 0;
  std::vector<CzSearchStoreInfoItem> items;
};

// PACKET_ZC_SEARCH_STORE_INFO_FAILED
struct ZcSearchStoreInfoFailed {
  static constexpr std::uint16_t kOpcode = 0x0837;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t reason = 0;
};

// PACKET_ZC_OPEN_SEARCH_STORE_INFO
struct ZcOpenSearchStoreInfo {
  static constexpr std::uint16_t kOpcode = 0x083a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::uint16_t effect = 0;
  std::uint8_t remainingUses = 0;
};

// PACKET_ZC_SSILIST_ITEM_CLICK_ACK
struct ZcSsilistItemClickAck {
  static constexpr std::uint16_t kOpcode = 0x083d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t x = 0;
  std::int16_t y = 0;
};

// PACKET_ZC_SEARCH_STORE_INFO_ACK_sub
struct ZcSearchStoreInfoAckSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 142;

  std::uint32_t storeId = 0;
  std::uint32_t AID = 0;
  std::string shopName;
  std::uint32_t itemId = 0;
  std::uint8_t itemType = 0;
  std::uint32_t price = 0;
  std::uint16_t amount = 0;
  Equipslotinfo slot;
  std::array<Itemoptions, 5> option_data = {};
  std::uint8_t refine = 0;
  std::uint8_t grade = 0;
};

// PACKET_ZC_SEARCH_STORE_INFO_ACK
struct ZcSearchStoreInfoAck {
  static constexpr std::uint16_t kOpcode = 0x0b64;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 7;

  std::uint8_t firstPage = 0;
  std::uint8_t nextPage = 0;
  std::uint8_t usesCount = 0;
  std::vector<ZcSearchStoreInfoAckSub> items;
};

// ach_list_info
struct AchListInfo {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 50;

  std::uint32_t ach_id = 0;
  std::uint8_t completed = 0;
  std::array<std::uint32_t, 10> objective = {};
  std::uint32_t completed_at = 0;
  std::uint8_t reward = 0;
};

// packet_achievement_update
struct PacketAchievementUpdate {
  static constexpr std::uint16_t kOpcode = 0x0a24;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 66;

  std::uint32_t total_points = 0;
  std::uint16_t rank = 0;
  std::uint32_t current_rank_points = 0;
  std::uint32_t next_rank_points = 0;
  AchListInfo ach;
};

// packet_achievement_reward_ack
struct PacketAchievementRewardAck {
  static constexpr std::uint16_t kOpcode = 0x0a26;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint8_t failed = 0;
  std::uint32_t ach_id = 0;
};

// packet_reqname_ack
struct PacketReqnameAck {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 30;

  std::int32_t gid = 0;
  std::string name;
};

// PACKET_ZC_ACK_REQNAMEALL
struct ZcAckReqnameall {
  static constexpr std::uint16_t kOpcode = 0x0a30;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 106;

  std::int32_t gid = 0;
  std::string name;
  std::string party_name;
  std::string guild_name;
  std::string position_name;
  std::int32_t title_id = 0;
};

// PACKET_ZC_ACK_REQNAMEALL_NPC
struct ZcAckReqnameallNpc {
  static constexpr std::uint16_t kOpcode = 0x0adf;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 58;

  std::int32_t gid = 0;
  std::int32_t groupId = 0;
  std::string name;
  std::string title;
};

// PACKET_ZC_OVERWEIGHT_PERCENT
struct ZcOverweightPercent {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t percent = 0;
};

// PACKET_ZC_WARPLIST_sub
struct ZcWarplistSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::string map;
};

// PACKET_ZC_WARPLIST
struct ZcWarplist {
  static constexpr std::uint16_t kOpcode = 0x0abe;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t skillId = 0;
  std::vector<ZcWarplistSub> maps;
};

// PACKET_ZC_GROUP_ISALIVE
struct ZcGroupIsalive {
  static constexpr std::uint16_t kOpcode = 0x0ab2;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint32_t AID = 0;
  std::uint8_t isDead = 0;
};

// PACKET_ZC_GUILD_POSITION
struct ZcGuildPosition {
  static constexpr std::uint16_t kOpcode = 0x0afd;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t AID = 0;
  std::string position;
};

// PACKET_ZC_MOVE_ITEM_FAILED
struct ZcMoveItemFailed {
  static constexpr std::uint16_t kOpcode = 0x0aa7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t itemIndex = 0;
  std::int16_t itemCount = 0;
};

// PACKET_ZC_ACK_BAN_GUILD1
struct ZcAckBanGuild1 {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 90;

  std::string name;
  std::string reason;
  std::string account_name;
};

// PACKET_ZC_ACK_BAN_GUILD2
struct ZcAckBanGuild2 {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 66;

  std::string name;
  std::string reason;
};

// PACKET_ZC_ACK_BAN_GUILD3
struct ZcAckBanGuild3 {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 46;

  std::string reason;
  std::uint32_t GID = 0;
};

// PACKET_ZC_ACK_LEAVE_GUILD1
struct ZcAckLeaveGuild1 {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 66;

  std::string name;
  std::string reason;
};

// PACKET_ZC_ACK_LEAVE_GUILD2
struct ZcAckLeaveGuild2 {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 46;

  std::uint32_t GID = 0;
  std::string reason;
};

// PACKET_CZ_MEMORIALDUNGEON_COMMAND
struct CzMemorialdungeonCommand {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t command = 0;
};

// PACKET_ZC_REMOVE_EFFECT
struct ZcRemoveEffect {
  static constexpr std::uint16_t kOpcode = 0x0b0d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t aid = 0;
  std::uint32_t effectId = 0;
};

// PACKET_ZC_VIEW_CAMERAINFO
struct ZcViewCamerainfo {
  static constexpr std::uint16_t kOpcode = 0x0a78;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 15;

  std::int8_t action = 0;
  float range = 0;
  float rotation = 0;
  float latitude = 0;
};

// PACKET_CZ_VIEW_CAMERAINFO
struct CzViewCamerainfo {
  static constexpr std::uint16_t kOpcode = 0x0a77;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 15;

  std::int8_t action = 0;
  float range = 0;
  float rotation = 0;
  float latitude = 0;
};

// PACKET_ZC_UPDATE_CARDSLOT
struct ZcUpdateCardslot {
  static constexpr std::uint16_t kOpcode = 0x0a3f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int16_t wearState = 0;
  std::int16_t cardSlot = 0;
  std::int32_t itemId = 0;
  std::int8_t equipFlag = 0;
};

// PACKET_ZC_DEBUGMSG
struct ZcDebugmsg {
  static constexpr std::uint16_t kOpcode = 0x0adb;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::int32_t color = 0;
  std::string message;
};

// PACKET_CZ_USE_SKILL_START
struct CzUseSkillStart {
  static constexpr std::uint16_t kOpcode = 0x0b10;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int16_t skillId = 0;
  std::int16_t skillLv = 0;
  std::uint32_t targetId = 0;
};

// PACKET_CZ_USE_SKILL_END
struct CzUseSkillEnd {
  static constexpr std::uint16_t kOpcode = 0x0b11;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t skillId = 0;
};

// PACKET_ZC_EXTEND_BODYITEM_SIZE
struct ZcExtendBodyitemSize {
  static constexpr std::uint16_t kOpcode = 0x0b18;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t expansionSize = 0;
};

// PACKET_ZC_ACK_OPEN_MSGBOX_EXTEND_BODYITEM_SIZE
struct ZcAckOpenMsgboxExtendBodyitemSize {
  static constexpr std::uint16_t kOpcode = 0x0b15;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint8_t result = 0;
  std::uint32_t itemId = 0;
};

// PACKET_ZC_ACK_EXTEND_BODYITEM_SIZE
struct ZcAckExtendBodyitemSize {
  static constexpr std::uint16_t kOpcode = 0x0b17;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t result = 0;
};

// PACKET_CZ_REQ_OPEN_MSGBOX_EXTEND_BODYITEM_SIZE
struct CzReqOpenMsgboxExtendBodyitemSize {
  static constexpr std::uint16_t kOpcode = 0x0b14;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_REQ_EXTEND_BODYITEM_SIZE
struct CzReqExtendBodyitemSize {
  static constexpr std::uint16_t kOpcode = 0x0b16;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_CLOSE_MSGBOX_EXTEND_BODYITEM_SIZE
struct CzCloseMsgboxExtendBodyitemSize {
  static constexpr std::uint16_t kOpcode = 0x0b19;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_REQ_REMAINTIME
struct CzReqRemaintime {
  static constexpr std::uint16_t kOpcode = 0x01c0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_PARTY_CONFIG
struct CzPartyConfig {
  static constexpr std::uint16_t kOpcode = 0x02c8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t refuseInvite = 0;
};

// PACKET_ZC_NPC_BARTER_MARKET_ITEMINFO_sub
struct ZcNpcBarterMarketIteminfoSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 31;

  std::uint32_t nameid = 0;
  std::uint8_t type = 0;
  std::uint32_t amount = 0;
  std::uint32_t currencyNameid = 0;
  std::uint32_t currencyAmount = 0;
  std::uint32_t weight = 0;
  std::uint32_t index = 0;
  std::uint16_t viewSprite = 0;
  std::uint32_t location = 0;
};

// PACKET_ZC_NPC_BARTER_MARKET_ITEMINFO
struct ZcNpcBarterMarketIteminfo {
  static constexpr std::uint16_t kOpcode = 0x0b78;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcNpcBarterMarketIteminfoSub> list;
};

// PACKET_CZ_NPC_BARTER_MARKET_CLOSE
struct CzNpcBarterMarketClose {
  static constexpr std::uint16_t kOpcode = 0x0b12;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_NPC_BARTER_MARKET_PURCHASE_sub
struct CzNpcBarterMarketPurchaseSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t itemId = 0;
  std::uint32_t amount = 0;
  std::uint16_t invIndex = 0;
  std::uint32_t shopIndex = 0;
};

// PACKET_CZ_NPC_BARTER_MARKET_PURCHASE
struct CzNpcBarterMarketPurchase {
  static constexpr std::uint16_t kOpcode = 0x0b0f;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<CzNpcBarterMarketPurchaseSub> list;
};

// PACKET_ZC_USESKILL_ACK
struct ZcUseskillAck {
  static constexpr std::uint16_t kOpcode = 0x0b1a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 29;

  std::uint32_t srcId = 0;
  std::uint32_t dstId = 0;
  std::uint16_t x = 0;
  std::uint16_t y = 0;
  std::uint16_t skillId = 0;
  std::uint32_t element = 0;
  std::uint32_t delayTime = 0;
  std::uint8_t disposable = 0;
  std::uint32_t attackMT = 0;
};

// PACKET_CZ_CLIENT_VERSION
struct CzClientVersion {
  static constexpr std::uint16_t kOpcode = 0x044a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t clientVersion = 0;
};

// PACKET_CZ_PING_LIVE
struct CzPingLive {
  static constexpr std::uint16_t kOpcode = 0x0b1c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_PING_LIVE
struct ZcPingLive {
  static constexpr std::uint16_t kOpcode = 0x0b1d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_CMD_RESETCOOLTIME
struct CzCmdResetcooltime {
  static constexpr std::uint16_t kOpcode = 0x0a88;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_CLOSE_UI_STYLINGSHOP
struct CzCloseUiStylingshop {
  static constexpr std::uint16_t kOpcode = 0x0a48;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_NOTIFY_ACTORINIT
struct ZcNotifyActorinit {
  static constexpr std::uint16_t kOpcode = 0x0b1b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_PARTY_CONFIG
struct ZcPartyConfig {
  static constexpr std::uint16_t kOpcode = 0x02c9;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t denyPartyInvites = 0;
};

// PACKET_ZC_ROLE_CHANGE
struct ZcRoleChange {
  static constexpr std::uint16_t kOpcode = 0x00e1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 30;

  std::int32_t flag = 0;
  std::string name;
};

// PACKET_ZC_BAN_LIST_sub
struct ZcBanListSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 68;

  std::int32_t char_id = 0;
  std::string message;
  std::string char_name;
};

// PACKET_ZC_BAN_LIST
struct ZcBanList {
  static constexpr std::uint16_t kOpcode = 0x0b7c;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcBanListSub> chars;
};

// PACKET_ZC_ACK_CLOSE_ROULETTE
struct ZcAckCloseRoulette {
  static constexpr std::uint16_t kOpcode = 0x0a1e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t result = 0;
};

// PACKET_ZC_ACK_MERGE_ITEM
struct ZcAckMergeItem {
  static constexpr std::uint16_t kOpcode = 0x096f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::int16_t index = 0;
  std::int16_t amount = 0;
  std::uint8_t reason = 0;
};

// PACKET_ZC_MERGE_ITEM_OPEN_sub
struct ZcMergeItemOpenSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;

  std::int16_t index = 0;
};

// PACKET_ZC_MERGE_ITEM_OPEN
struct ZcMergeItemOpen {
  static constexpr std::uint16_t kOpcode = 0x096d;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcMergeItemOpenSub> items;
};

// PACKET_ZC_SE_PC_BUY_CASHITEM_RESULT
struct ZcSePcBuyCashitemResult {
  static constexpr std::uint16_t kOpcode = 0x0849;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::uint32_t itemId = 0;
  std::uint16_t result = 0;
  std::uint32_t cashPoints = 0;
  std::uint32_t kafraPoints = 0;
};

// PACKET_ZC_OPEN_REFINING_UI
struct ZcOpenRefiningUi {
  static constexpr std::uint16_t kOpcode = 0x0aa0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_REFINING_SELECT_ITEM
struct CzRefiningSelectItem {
  static constexpr std::uint16_t kOpcode = 0x0aa1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t index = 0;
};

// PACKET_ZC_REFINING_MATERIAL_LIST_SUB
struct ZcRefiningMaterialListSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::uint32_t itemId = 0;
  std::int8_t chance = 0;
  std::int32_t zeny = 0;
};

// PACKET_ZC_REFINING_MATERIAL_LIST
struct ZcRefiningMaterialList {
  static constexpr std::uint16_t kOpcode = 0x0aa2;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 7;

  std::int16_t itemIndex = 0;
  std::int8_t blacksmithBlessing = 0;
  std::vector<ZcRefiningMaterialListSub> req;
};

// PACKET_CZ_REQ_REFINING
struct CzReqRefining {
  static constexpr std::uint16_t kOpcode = 0x0aa3;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::int16_t index = 0;
  std::uint32_t itemId = 0;
  std::int8_t blacksmithBlessing = 0;
};

// PACKET_CZ_CLOSE_REFINING_UI
struct CzCloseRefiningUi {
  static constexpr std::uint16_t kOpcode = 0x0aa4;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_BROADCAST_ITEMREFINING_RESULT
struct ZcBroadcastItemrefiningResult {
  static constexpr std::uint16_t kOpcode = 0x0ada;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 32;

  std::string name;
  std::uint32_t itemId = 0;
  std::int8_t refine_level = 0;
  std::int8_t status = 0;
};

// PACKET_ZC_STATUS_CHANGE_ACK
struct ZcStatusChangeAck {
  static constexpr std::uint16_t kOpcode = 0x00bc;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t sp = 0;
  std::uint8_t ok = 0;
  std::uint8_t value = 0;
};

// PACKET_ZC_EQUIPMENT_EFFECT
struct ZcEquipmentEffect {
  static constexpr std::uint16_t kOpcode = 0x0a3b;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 9;

  std::uint32_t aid = 0;
  std::int8_t status = 0;
  std::vector<std::uint16_t> effects;
};

// SKILLDATA
struct Skilldata {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 37;

  std::uint16_t id = 0;
  std::int32_t inf = 0;
  std::uint16_t level = 0;
  std::uint16_t sp = 0;
  std::uint16_t range2 = 0;
  std::string name;
  std::uint8_t upFlag = 0;
};

// PACKET_ZC_ADD_SKILL
struct ZcAddSkill {
  static constexpr std::uint16_t kOpcode = 0x0111;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 39;

  Skilldata skill;
};

// PACKET_ZC_SKILLINFO_LIST
struct ZcSkillinfoList {
  static constexpr std::uint16_t kOpcode = 0x010f;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<Skilldata> skills;
};

// PACKET_ZC_SKILLINFO_UPDATE2
struct ZcSkillinfoUpdate2 {
  static constexpr std::uint16_t kOpcode = 0x07e1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 15;

  std::uint16_t id = 0;
  std::int32_t inf = 0;
  std::uint16_t level = 0;
  std::uint16_t sp = 0;
  std::uint16_t range2 = 0;
  std::uint8_t upFlag = 0;
};

// PACKET_ZC_NPC_MARKET_PURCHASE_RESULT_sub
struct ZcNpcMarketPurchaseResultSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t ITID = 0;
  std::uint16_t qty = 0;
  std::uint32_t price = 0;
};

// PACKET_ZC_NPC_MARKET_PURCHASE_RESULT
struct ZcNpcMarketPurchaseResult {
  static constexpr std::uint16_t kOpcode = 0x0b4e;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t result = 0;
  std::vector<ZcNpcMarketPurchaseResultSub> list;
};

// PACKET_ZC_TALKBOX_CHATCONTENTS
struct ZcTalkboxChatcontents {
  static constexpr std::uint16_t kOpcode = 0x0191;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 27;

  std::uint32_t aid = 0;
  std::string message;
};

// PACKET_ZC_GUILD_AGIT_INFO
struct ZcGuildAgitInfo {
  static constexpr std::uint16_t kOpcode = 0x0b27;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<std::int8_t> castle_list;
};

// PACKET_CZ_REQ_MOVE_GUILD_AGIT
struct CzReqMoveGuildAgit {
  static constexpr std::uint16_t kOpcode = 0x0b28;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::int8_t castle_id = 0;
};

// PACKET_ZC_REQ_ACK_MOVE_GUILD_AGIT
struct ZcReqAckMoveGuildAgit {
  static constexpr std::uint16_t kOpcode = 0x0b2e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t result = 0;
};

// PACKET_ZC_REQ_ACK_AGIT_INVESTMENT
struct ZcReqAckAgitInvestment {
  static constexpr std::uint16_t kOpcode = 0x0b2d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int8_t castle_id = 0;
  std::int32_t economy = 0;
  std::int32_t defense = 0;
};

// PACKET_CZ_REQ_AGIT_INVESTMENT
struct CzReqAgitInvestment {
  static constexpr std::uint16_t kOpcode = 0x0b2c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::int8_t castle_id = 0;
};

// PACKET_ZC_RANDOM_COMBINE_ITEM_UI_OPEN
struct ZcRandomCombineItemUiOpen {
  static constexpr std::uint16_t kOpcode = 0x0a4e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t itemId = 0;
};

// PACKET_CZ_RANDOM_COMBINE_ITEM_UI_CLOSE
struct CzRandomCombineItemUiClose {
  static constexpr std::uint16_t kOpcode = 0x0a70;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_REQ_RANDOM_COMBINE_ITEM_sub
struct CzReqRandomCombineItemSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t index = 0;
  std::int16_t count = 0;
};

// PACKET_CZ_REQ_RANDOM_COMBINE_ITEM
struct CzReqRandomCombineItem {
  static constexpr std::uint16_t kOpcode = 0x0a4f;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::int32_t itemId = 0;
  std::vector<CzReqRandomCombineItemSub> items;
};

// PACKET_ZC_ACK_RANDOM_COMBINE_ITEM
struct ZcAckRandomCombineItem {
  static constexpr std::uint16_t kOpcode = 0x0a50;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t result = 0;
};

// PACKET_CZ_UNINSTALLATION
struct CzUninstallation {
  static constexpr std::uint16_t kOpcode = 0x0b35;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t InstallationKind = 0;
};

// PACKET_ZC_NOTIFY_EFFECT3
struct ZcNotifyEffect3 {
  static constexpr std::uint16_t kOpcode = 0x0b69;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 18;

  std::uint32_t aid = 0;
  std::uint32_t effectId = 0;
  std::uint64_t num = 0;
};

// PACKET_CZ_SE_CASHSHOP_OPEN1
struct CzSeCashshopOpen1 {
  static constexpr std::uint16_t kOpcode = 0x0844;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_SE_CASHSHOP_OPEN2
struct CzSeCashshopOpen2 {
  static constexpr std::uint16_t kOpcode = 0x0b6d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t tab = 0;
};

// PACKET_CZ_GET_ACCOUNT_LIMTIED_SALE_LIST
struct CzGetAccountLimtiedSaleList {
  static constexpr std::uint16_t kOpcode = 0x0b4c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_SE_CASHSHOP_OPEN
struct ZcSeCashshopOpen {
  static constexpr std::uint16_t kOpcode = 0x0b6e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t cashPoints = 0;
  std::uint32_t kafraPoints = 0;
  std::uint32_t tab = 0;
};

// PACKET_CZ_NPC_EXPANDED_BARTER_MARKET_CLOSE
struct CzNpcExpandedBarterMarketClose {
  static constexpr std::uint16_t kOpcode = 0x0b58;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_NPC_EXPANDED_BARTER_MARKET_PURCHASE_sub
struct CzNpcExpandedBarterMarketPurchaseSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t itemId = 0;
  std::uint32_t shopIndex = 0;
  std::uint32_t amount = 0;
};

// PACKET_CZ_NPC_EXPANDED_BARTER_MARKET_PURCHASE
struct CzNpcExpandedBarterMarketPurchase {
  static constexpr std::uint16_t kOpcode = 0x0b57;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<CzNpcExpandedBarterMarketPurchaseSub> list;
};

// PACKET_ZC_STATE_CHANGE
struct ZcStateChange {
  static constexpr std::uint16_t kOpcode = 0x0229;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 15;

  std::uint32_t AID = 0;
  std::int16_t bodyState = 0;
  std::int16_t healthState = 0;
  std::int32_t effectState = 0;
  std::int8_t isPKModeON = 0;
};

// PACKET_ZC_AUTORUN_SKILL
struct ZcAutorunSkill {
  static constexpr std::uint16_t kOpcode = 0x0147;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 39;

  std::uint16_t skill_id = 0;
  std::uint32_t skill_type = 0;
  std::uint16_t skill_lv = 0;
  std::uint16_t skill_sp = 0;
  std::uint16_t skill_range = 0;
  std::string skill_name;
  std::string up_flag;
};

// PACKET_ZC_RANDOM_UPGRADE_ITEM_UI_OPEN
struct ZcRandomUpgradeItemUiOpen {
  static constexpr std::uint16_t kOpcode = 0x0ab4;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t itemId = 0;
};

// PACKET_ZC_ACK_RANDOM_UPGRADE_ITEM
struct ZcAckRandomUpgradeItem {
  static constexpr std::uint16_t kOpcode = 0x0ab7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t result = 0;
};

// PACKET_CZ_RANDOM_UPGRADE_ITEM_UI_CLOSE
struct CzRandomUpgradeItemUiClose {
  static constexpr std::uint16_t kOpcode = 0x0ab5;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_REQ_RANDOM_UPGRADE_ITEM
struct CzReqRandomUpgradeItem {
  static constexpr std::uint16_t kOpcode = 0x0ab6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t itemId = 0;
  std::uint16_t index = 0;
};

// PACKET_ZC_PERSONAL_INFOMATION_SUB
struct ZcPersonalInfomationSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 13;

  std::int8_t type = 0;
  std::int32_t exp = 0;
  std::int32_t death = 0;
  std::int32_t drop = 0;
};

// PACKET_ZC_PERSONAL_INFOMATION
struct ZcPersonalInfomation {
  static constexpr std::uint16_t kOpcode = 0x097b;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 16;

  std::int32_t total_exp = 0;
  std::int32_t total_death = 0;
  std::int32_t total_drop = 0;
  std::vector<ZcPersonalInfomationSub> details;
};

// PACKET_CZ_REQUEST_ACTNPC
struct CzRequestActnpc {
  // ⚠ Ce paquet ne porte pas son opcode : le serveur le pose depuis son
  // code, par une constante qu'aucune règle ne rattache à la structure.
  // Le décodeur ne peut donc pas le vérifier, et il n'y a pas d'encodeur.
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::uint32_t GID = 0;
  std::uint32_t targetGID = 0;
  std::int8_t action = 0;
};

// PACKET_ZC_NOTIFY_SKILL
struct ZcNotifySkill {
  static constexpr std::uint16_t kOpcode = 0x01de;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 33;

  std::uint16_t SKID = 0;
  std::uint32_t AID = 0;
  std::uint32_t targetID = 0;
  std::uint32_t startTime = 0;
  std::int32_t attackMT = 0;
  std::int32_t attackedMT = 0;
  std::int32_t damage = 0;
  std::int16_t level = 0;
  std::int16_t count = 0;
  std::int8_t action = 0;
};

// PACKET_ZC_USE_SKILL
struct ZcUseSkill {
  static constexpr std::uint16_t kOpcode = 0x09cb;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 17;

  std::uint16_t SKID = 0;
  std::int32_t level = 0;
  std::uint32_t targetAID = 0;
  std::uint32_t srcAID = 0;
  std::int8_t result = 0;
};

// PACKET_ZC_NOTIFY_GROUNDSKILL
struct ZcNotifyGroundskill {
  static constexpr std::uint16_t kOpcode = 0x0117;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 18;

  std::uint16_t SKID = 0;
  std::uint32_t AID = 0;
  std::int16_t level = 0;
  std::int16_t xPos = 0;
  std::int16_t yPos = 0;
  std::uint32_t startTime = 0;
};

// PACKET_ZC_SKILL_POSTDELAY
struct ZcSkillPostdelay {
  static constexpr std::uint16_t kOpcode = 0x043d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t SKID = 0;
  std::uint32_t DelayTM = 0;
};

// PACKET_ZC_NOTIFY_SKILL_POSITION
struct ZcNotifySkillPosition {
  static constexpr std::uint16_t kOpcode = 0x0115;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 35;

  std::uint16_t SKID = 0;
  std::uint32_t AID = 0;
  std::uint32_t targetID = 0;
  std::uint32_t startTime = 0;
  std::int32_t attackMT = 0;
  std::int32_t attackedMT = 0;
  std::int16_t xPos = 0;
  std::int16_t yPos = 0;
  std::int16_t damage = 0;
  std::int16_t level = 0;
  std::int16_t count = 0;
  std::int8_t action = 0;
};

// PACKET_ZC_C_MARKERINFO
struct ZcCMarkerinfo {
  static constexpr std::uint16_t kOpcode = 0x09c1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t AID = 0;
  std::int16_t xPos = 0;
  std::int16_t yPos = 0;
};

// GUILD_MEMBER_INFO
struct GuildMemberInfo {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 58;

  std::uint32_t AID = 0;
  std::uint32_t GID = 0;
  std::int16_t head = 0;
  std::int16_t headPalette = 0;
  std::int16_t sex = 0;
  std::int16_t job = 0;
  std::int16_t level = 0;
  std::int32_t contributionExp = 0;
  std::int32_t currentState = 0;
  std::int32_t positionID = 0;
  std::uint32_t lastLoginTime = 0;
  std::string char_name;
};

// PACKET_ZC_MEMBERMGR_INFO
struct ZcMembermgrInfo {
  static constexpr std::uint16_t kOpcode = 0x0b7d;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<GuildMemberInfo> guildMemberInfo;
};

// PACKET_ZC_GUILD_INFO
struct ZcGuildInfo {
  static constexpr std::uint16_t kOpcode = 0x0b7b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 118;

  std::int32_t GDID = 0;
  std::int32_t level = 0;
  std::int32_t userNum = 0;
  std::int32_t maxUserNum = 0;
  std::int32_t userAverageLevel = 0;
  std::int32_t exp = 0;
  std::int32_t maxExp = 0;
  std::int32_t point = 0;
  std::int32_t honor = 0;
  std::int32_t virtue = 0;
  std::int32_t emblemVersion = 0;
  std::string guildname;
  std::string manageLand;
  std::int32_t zeny = 0;
  std::int32_t masterGID = 0;
  std::string masterName;
};

// PACKET_ZC_POSITION_ID_NAME_INFO
struct ZcPositionIdNameInfo {
  static constexpr std::uint16_t kOpcode = 0x0166;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 564;

  std::array<std::uint8_t, 560> posInfo = {};
};

// PACKET_ZC_POSITION_INFO_sub
struct ZcPositionInfoSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::int32_t positionID = 0;
  std::int32_t right = 0;
  std::int32_t ranking = 0;
  std::int32_t payRate = 0;
};

// PACKET_ZC_POSITION_INFO
struct ZcPositionInfo {
  static constexpr std::uint16_t kOpcode = 0x0160;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcPositionInfoSub> posInfo;
};

// GUILD_SKILLDATA
struct GuildSkilldata {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 37;

  std::uint16_t id = 0;
  std::int32_t inf = 0;
  std::uint16_t level = 0;
  std::uint16_t sp = 0;
  std::uint16_t range2 = 0;
  std::string name;
  std::uint8_t upFlag = 0;
};

// PACKET_ZC_GUILD_SKILLINFO
struct ZcGuildSkillinfo {
  static constexpr std::uint16_t kOpcode = 0x0162;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t skillPoint = 0;
  std::vector<GuildSkilldata> skillInfo;
};

// RELATED_GUILD_INFO
struct RelatedGuildInfo {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 32;

  std::int32_t relation = 0;
  std::int32_t GDID = 0;
  std::string guildname;
};

// PACKET_ZC_MYGUILD_BASIC_INFO
struct ZcMyguildBasicInfo {
  static constexpr std::uint16_t kOpcode = 0x014c;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<RelatedGuildInfo> rgInfo;
};

// PACKET_CZ_REQ_UPLOAD_MACRO_DETECTOR
struct CzReqUploadMacroDetector {
  static constexpr std::uint16_t kOpcode = 0x0a52;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 20;

  std::string answer;
  std::uint16_t imageSize = 0;
};

// PACKET_ZC_ACK_UPLOAD_MACRO_DETECTOR
struct ZcAckUploadMacroDetector {
  static constexpr std::uint16_t kOpcode = 0x0a53;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::string captchaKey;
  std::int32_t captchaFlag = 0;
};

// PACKET_CZ_UPLOAD_MACRO_DETECTOR_CAPTCHA
struct CzUploadMacroDetectorCaptcha {
  static constexpr std::uint16_t kOpcode = 0x0a54;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::string captchaKey;
  std::string imageData;
};

// PACKET_ZC_COMPLETE_UPLOAD_MACRO_DETECTOR_CAPTCHA
struct ZcCompleteUploadMacroDetectorCaptcha {
  static constexpr std::uint16_t kOpcode = 0x0a55;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_REQ_APPLY_MACRO_DETECTOR
struct CzReqApplyMacroDetector {
  static constexpr std::uint16_t kOpcode = 0x0a56;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t AID = 0;
};

// PACKET_ZC_ACK_APPLY_MACRO_DETECTOR
struct ZcAckApplyMacroDetector {
  static constexpr std::uint16_t kOpcode = 0x0a57;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t status = 0;
};

// PACKET_ZC_APPLY_MACRO_DETECTOR
struct ZcApplyMacroDetector {
  static constexpr std::uint16_t kOpcode = 0x0a58;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t imageSize = 0;
  std::string captchaKey;
};

// PACKET_ZC_APPLY_MACRO_DETECTOR_CAPTCHA
struct ZcApplyMacroDetectorCaptcha {
  static constexpr std::uint16_t kOpcode = 0x0a59;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::string captchaKey;
  std::string imageData;
};

// PACKET_CZ_COMPLETE_APPLY_MACRO_DETECTOR_CAPTCHA
struct CzCompleteApplyMacroDetectorCaptcha {
  static constexpr std::uint16_t kOpcode = 0x0a5a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_REQ_ANSWER_MACRO_DETECTOR
struct ZcReqAnswerMacroDetector {
  static constexpr std::uint16_t kOpcode = 0x0a5b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint8_t retryCount = 0;
  std::int32_t timeout = 0;
};

// PACKET_CZ_ACK_ANSWER_MACRO_DETECTOR
struct CzAckAnswerMacroDetector {
  static constexpr std::uint16_t kOpcode = 0x0a5c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 18;

  std::string answer;
};

// PACKET_ZC_CLOSE_MACRO_DETECTOR
struct ZcCloseMacroDetector {
  static constexpr std::uint16_t kOpcode = 0x0a5d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t status = 0;
};

// PACKET_CZ_REQ_PREVIEW_MACRO_DETECTOR
struct CzReqPreviewMacroDetector {
  static constexpr std::uint16_t kOpcode = 0x0a69;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t captchaID = 0;
};

// PACKET_ZC_ACK_PREVIEW_MACRO_DETECTOR
struct ZcAckPreviewMacroDetector {
  static constexpr std::uint16_t kOpcode = 0x0a6a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::int32_t captchaFlag = 0;
  std::uint16_t imageSize = 0;
  std::string captchaKey;
};

// PACKET_ZC_PREVIEW_MACRO_DETECTOR_CAPTCHA
struct ZcPreviewMacroDetectorCaptcha {
  static constexpr std::uint16_t kOpcode = 0x0a6b;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::string captchaKey;
  std::string imageData;
};

// PACKET_CZ_REQ_PLAYER_AID_IN_RANGE
struct CzReqPlayerAidInRange {
  static constexpr std::uint16_t kOpcode = 0x0a6c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::int16_t xPos = 0;
  std::int16_t yPos = 0;
  std::int8_t RadiusRange = 0;
};

// PACKET_ZC_ACK_PLAYER_AID_IN_RANGE
struct ZcAckPlayerAidInRange {
  static constexpr std::uint16_t kOpcode = 0x0a6d;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<std::uint32_t> AID;
};

// PACKET_ZC_ACK_MAKE_GROUP
struct ZcAckMakeGroup {
  static constexpr std::uint16_t kOpcode = 0x00fa;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::int8_t result = 0;
};

// PACKET_ZC_PARTY_JOIN_REQ
struct ZcPartyJoinReq {
  static constexpr std::uint16_t kOpcode = 0x02c6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 30;

  std::int32_t GRID = 0;
  std::string groupName;
};

// PACKET_ZC_PARTY_JOIN_REQ_ACK
struct ZcPartyJoinReqAck {
  static constexpr std::uint16_t kOpcode = 0x02c5;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 30;

  std::string characterName;
  std::int32_t result = 0;
};

// PACKET_ZC_NOTIFY_CHAT_PARTY
struct ZcNotifyChatParty {
  static constexpr std::uint16_t kOpcode = 0x0109;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::int32_t AID = 0;
  std::string chatMsg;
};

// PACKET_ZC_NOTIFY_POSITION_TO_GROUPM
struct ZcNotifyPositionToGroupm {
  static constexpr std::uint16_t kOpcode = 0x0107;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int32_t AID = 0;
  std::int16_t xPos = 0;
  std::int16_t yPos = 0;
};

// PACKET_ZC_NOTIFY_HP_TO_GROUPM
struct ZcNotifyHpToGroupm {
  static constexpr std::uint16_t kOpcode = 0x080e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t AID = 0;
  std::int32_t hp = 0;
  std::int32_t maxhp = 0;
};

// PACKET_ZC_NOTIFY_MEMBERINFO_TO_GROUPM
struct ZcNotifyMemberinfoToGroupm {
  static constexpr std::uint16_t kOpcode = 0x0abd;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int32_t AID = 0;
  std::int16_t job = 0;
  std::int16_t level = 0;
};

// PACKET_ZC_DELETE_MEMBER_FROM_GROUP
struct ZcDeleteMemberFromGroup {
  static constexpr std::uint16_t kOpcode = 0x0105;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 31;

  std::int32_t AID = 0;
  std::string characterName;
  std::int8_t result = 0;
};

// PACKET_CZ_REQ_TAKEOFF_EQUIP_ALL
struct CzReqTakeoffEquipAll {
  static constexpr std::uint16_t kOpcode = 0x0bf5;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t location = 0;
};

// PACKET_ZC_ACK_TAKEOFF_EQUIP_ALL
struct ZcAckTakeoffEquipAll {
  static constexpr std::uint16_t kOpcode = 0x0bae;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t result = 0;
};

// PACKET_ZC_BATTLEFIELD_NOTIFY_HP
struct ZcBattlefieldNotifyHp {
  static constexpr std::uint16_t kOpcode = 0x0a0e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t AID = 0;
  std::int32_t hp = 0;
  std::int32_t maxhp = 0;
};

// PACKET_CZ_CHOOSE_MENU_ZERO
struct CzChooseMenuZero {
  static constexpr std::uint16_t kOpcode = 0x0ba8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint32_t NpcID = 0;
  std::uint8_t menuIndex = 0;
};

// PACKET_ZC_DIALOG_TEXT_ALIGN
struct ZcDialogTextAlign {
  static constexpr std::uint16_t kOpcode = 0x0ba1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t align = 0;
};

// PACKET_CZ_GRADE_ENCHANT_SELECT_EQUIPMENT
struct CzGradeEnchantSelectEquipment {
  static constexpr std::uint16_t kOpcode = 0x0b59;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t index = 0;
};

// GRADE_ENCHANT_BLESSING
struct GradeEnchantBlessing {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::int32_t id = 0;
  std::int32_t amount = 0;
  std::int32_t max_blessing = 0;
  std::int32_t bonus = 0;
};

// GRADE_ENCHANT_MATERIAL
struct GradeEnchantMaterial {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 17;

  std::int32_t nameid = 0;
  std::int32_t amount = 0;
  std::int32_t price = 0;
  std::int32_t downgrade = 0;
  std::int8_t breakable = 0;
};

// PACKET_ZC_GRADE_ENCHANT_MATERIAL_LIST
struct ZcGradeEnchantMaterialList {
  static constexpr std::uint16_t kOpcode = 0x0b5a;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 34;

  std::int16_t index = 0;
  std::int32_t success_chance = 0;
  GradeEnchantBlessing blessing_info;
  std::int32_t protect_itemid = 0;
  std::int32_t protect_amount = 0;
  std::vector<GradeEnchantMaterial> material_info;
};

// PACKET_CZ_GRADE_ENCHANT_REQUEST
struct CzGradeEnchantRequest {
  static constexpr std::uint16_t kOpcode = 0x0b5b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::int16_t index = 0;
  std::int32_t material_index = 0;
  std::int8_t blessing_flag = 0;
  std::int32_t blessing_amount = 0;
  std::int8_t protect_flag = 0;
};

// PACKET_CZ_GRADE_ENCHANT_CLOSE_UI
struct CzGradeEnchantCloseUi {
  static constexpr std::uint16_t kOpcode = 0x0b5c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_GRADE_ENCHANT_ACK
struct ZcGradeEnchantAck {
  static constexpr std::uint16_t kOpcode = 0x0b5d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int16_t index = 0;
  std::int16_t grade = 0;
  std::int32_t result = 0;
};

// PACKET_ZC_GRADE_ENCHANT_BROADCAST_RESULT
struct ZcGradeEnchantBroadcastResult {
  static constexpr std::uint16_t kOpcode = 0x0b5e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 33;

  std::string name;
  std::uint32_t itemId = 0;
  std::int16_t grade = 0;
  std::int8_t status = 0;
};

// PACKET_ZC_SHOW_IMAGE
struct ZcShowImage {
  static constexpr std::uint16_t kOpcode = 0x01b3;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 67;

  std::string image;
  std::uint8_t type = 0;
};

// PACKET_ZC_UPDATE_GDID
struct ZcUpdateGdid {
  static constexpr std::uint16_t kOpcode = 0x02f7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 47;

  std::uint32_t guildId = 0;
  std::int32_t emblemVersion = 0;
  std::uint32_t mode = 0;
  std::uint8_t isMaster = 0;
  std::int32_t interSid = 0;
  std::string guildName;
  std::uint32_t masterGID = 0;
};

// PACKET_CZ_APPROXIMATE_ACTOR
struct CzApproximateActor {
  static constexpr std::uint16_t kOpcode = 0x0bb0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::uint32_t masterGID = 0;
  std::uint16_t unused1 = 0;
  std::uint8_t unused2 = 0;
};

// PACKET_CZ_CONTACTNPC
struct CzContactnpc {
  static constexpr std::uint16_t kOpcode = 0x0090;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint32_t AID = 0;
  std::uint8_t type = 0;
};

// PACKET_ZC_ATTACK_FAILURE_FOR_DISTANCE
struct ZcAttackFailureForDistance {
  static constexpr std::uint16_t kOpcode = 0x0139;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::uint32_t targetAID = 0;
  std::int16_t targetXPos = 0;
  std::int16_t targetYPos = 0;
  std::int16_t xPos = 0;
  std::int16_t yPos = 0;
  std::int16_t currentAttRange = 0;
};

// PACKET_ZC_START_CAPTURE
struct ZcStartCapture {
  static constexpr std::uint16_t kOpcode = 0x019e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_TRYCAPTURE_MONSTER
struct ZcTrycaptureMonster {
  static constexpr std::uint16_t kOpcode = 0x01a0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::int8_t result = 0;
};

// PACKET_ZC_PROPERTY_PET
struct ZcPropertyPet {
  static constexpr std::uint16_t kOpcode = 0x01a2;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 37;

  std::string szName;
  std::int8_t bModified = 0;
  std::int16_t nLevel = 0;
  std::int16_t nFullness = 0;
  std::int16_t nRelationship = 0;
  std::int16_t ITID = 0;
  std::int16_t job = 0;
};

// PACKET_ZC_CHANGESTATE_PET
struct ZcChangestatePet {
  static constexpr std::uint16_t kOpcode = 0x01a4;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int8_t type = 0;
  std::int32_t GID = 0;
  std::int32_t data = 0;
};

// PACKET_ZC_SPIRITS
struct ZcSpirits {
  static constexpr std::uint16_t kOpcode = 0x01d0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t AID = 0;
  std::int16_t num = 0;
};

// PACKET_ZC_SPIRITS2
struct ZcSpirits2 {
  static constexpr std::uint16_t kOpcode = 0x01e1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t AID = 0;
  std::int16_t num = 0;
};

// PACKET_ZC_SOULENERGY
struct ZcSoulenergy {
  static constexpr std::uint16_t kOpcode = 0x0b73;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t AID = 0;
  std::uint16_t num = 0;
};

// PACKET_ZC_SAY_DIALOG
struct ZcSayDialog {
  static constexpr std::uint16_t kOpcode = 0x00b4;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t NpcID = 0;
  std::string message;
};

// PACKET_ZC_SAY_DIALOG2
struct ZcSayDialog2 {
  static constexpr std::uint16_t kOpcode = 0x0972;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 9;

  std::uint32_t NpcID = 0;
  std::uint8_t type = 0;
  std::string message;
};

// PACKET_ZC_WAIT_DIALOG
struct ZcWaitDialog {
  static constexpr std::uint16_t kOpcode = 0x00b5;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t NpcID = 0;
};

// PACKET_ZC_WAIT_DIALOG2
struct ZcWaitDialog2 {
  static constexpr std::uint16_t kOpcode = 0x0973;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint32_t NpcID = 0;
  std::uint8_t type = 0;
};

// PACKET_ZC_DIALOG_WINDOW_SIZE
struct ZcDialogWindowSize {
  static constexpr std::uint16_t kOpcode = 0x0ba2;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int32_t height = 0;
  std::int32_t width = 0;
};

// PACKET_ZC_DIALOG_WINDOW_POS
struct ZcDialogWindowPos {
  static constexpr std::uint16_t kOpcode = 0x0ba3;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int32_t x = 0;
  std::int32_t y = 0;
};

// PACKET_ZC_DIALOG_WINDOW_POS2
struct ZcDialogWindowPos2 {
  static constexpr std::uint16_t kOpcode = 0x0bb5;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int32_t x = 0;
  std::int32_t y = 0;
};

// PACKET_ZC_PLAY_NPC_BGM
struct ZcPlayNpcBgm {
  static constexpr std::uint16_t kOpcode = 0x0b8c;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 5;

  std::uint8_t playType = 0;
  std::string bgm;
};

// PACKET_CZ_MOVE_ITEM_FROM_BODY_TO_CART
struct CzMoveItemFromBodyToCart {
  static constexpr std::uint16_t kOpcode = 0x0126;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int16_t index = 0;
  std::int32_t count = 0;
};

// PACKET_ZC_SOUND
struct ZcSound {
  static constexpr std::uint16_t kOpcode = 0x01d3;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 35;

  std::string name;
  std::uint8_t act = 0;
  std::uint32_t term = 0;
  std::uint32_t AID = 0;
};

// PACKET_ZC_BUYING_STORE_ENTRY
struct ZcBuyingStoreEntry {
  static constexpr std::uint16_t kOpcode = 0x0814;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 86;

  std::uint32_t makerAID = 0;
  std::string storeName;
};

// PACKET_ZC_STORE_ENTRY
struct ZcStoreEntry {
  static constexpr std::uint16_t kOpcode = 0x0131;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 86;

  std::uint32_t makerAID = 0;
  std::string storeName;
};

// CZ_PURCHASE_ITEM_FROMMC
struct CzPurchaseItemFrommc {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t count = 0;
  std::int16_t index = 0;
};

// PACKET_CZ_PC_PURCHASE_ITEMLIST_FROMMC
struct CzPcPurchaseItemlistFrommc {
  static constexpr std::uint16_t kOpcode = 0x0134;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t AID = 0;
  std::vector<CzPurchaseItemFrommc> list;
};

// PACKET_CZ_PC_PURCHASE_ITEMLIST_FROMMC2
struct CzPcPurchaseItemlistFrommc2 {
  static constexpr std::uint16_t kOpcode = 0x0801;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t AID = 0;
  std::uint32_t UniqueID = 0;
  std::vector<CzPurchaseItemFrommc> list;
};

// PACKET_ZC_DISAPPEAR_BUYING_STORE_ENTRY
struct ZcDisappearBuyingStoreEntry {
  static constexpr std::uint16_t kOpcode = 0x0816;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t makerAID = 0;
};

// PACKET_ZC_OPEN_REFORM_UI
struct ZcOpenReformUi {
  static constexpr std::uint16_t kOpcode = 0x0b8f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t ITID = 0;
};

// PACKET_CZ_CLOSE_REFORM_UI
struct CzCloseReformUi {
  static constexpr std::uint16_t kOpcode = 0x0b90;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_ITEM_REFORM
struct CzItemReform {
  static constexpr std::uint16_t kOpcode = 0x0b91;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int32_t ITID = 0;
  std::int16_t index = 0;
};

// PACKET_ZC_ITEM_REFORM_ACK
struct ZcItemReformAck {
  static constexpr std::uint16_t kOpcode = 0x0b92;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::int16_t index = 0;
  std::int8_t result = 0;
};

// PACKET_CZ_USE_PACKAGEITEM
struct CzUsePackageitem {
  static constexpr std::uint16_t kOpcode = 0x0baf;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::uint16_t index = 0;
  std::uint32_t AID = 0;
  std::uint32_t itemID = 0;
  std::uint32_t BoxIndex = 0;
};

// PACKET_CZ_REQUEST_RANDOM_ENCHANT
struct CzRequestRandomEnchant {
  static constexpr std::uint16_t kOpcode = 0x0b9b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::int64_t enchant_group = 0;
  std::int16_t index = 0;
};

// PACKET_CZ_REQUEST_PERFECT_ENCHANT
struct CzRequestPerfectEnchant {
  static constexpr std::uint16_t kOpcode = 0x0b9c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::int64_t enchant_group = 0;
  std::int16_t index = 0;
  std::uint32_t ITID = 0;
};

// PACKET_CZ_REQUEST_UPGRADE_ENCHANT
struct CzRequestUpgradeEnchant {
  static constexpr std::uint16_t kOpcode = 0x0b9d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::int64_t enchant_group = 0;
  std::int16_t index = 0;
  std::int16_t slot = 0;
};

// PACKET_CZ_REQUEST_RESET_ENCHANT
struct CzRequestResetEnchant {
  static constexpr std::uint16_t kOpcode = 0x0b9e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::int64_t enchant_group = 0;
  std::int16_t index = 0;
};

// PACKET_ZC_RESPONSE_ENCHANT
struct ZcResponseEnchant {
  static constexpr std::uint16_t kOpcode = 0x0b9f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int32_t msgId = 0;
  std::uint32_t ITID = 0;
};

// PACKET_CZ_CLOSE_UI_ENCHANT
struct CzCloseUiEnchant {
  static constexpr std::uint16_t kOpcode = 0x0ba0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_SPECIALPOPUP
struct ZcSpecialpopup {
  static constexpr std::uint16_t kOpcode = 0x0bbe;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t ppId = 0;
};

// PACKET_ZC_GOLDPCCAFE_POINT
struct ZcGoldpccafePoint {
  static constexpr std::uint16_t kOpcode = 0x0a15;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::int8_t isActive = 0;
  std::int8_t mode = 0;
  std::int32_t point = 0;
  std::int32_t playedTime = 0;
};

// PACKET_CZ_DYNAMICNPC_CREATE_REQUEST
struct CzDynamicnpcCreateRequest {
  static constexpr std::uint16_t kOpcode = 0x0a16;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::string name;
};

// PACKET_ZC_DYNAMICNPC_CREATE_RESULT
struct ZcDynamicnpcCreateResult {
  static constexpr std::uint16_t kOpcode = 0x0a17;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t result = 0;
};

// PACKET_CZ_REQ_GUILD_EMBLEM_IMG1
struct CzReqGuildEmblemImg1 {
  static constexpr std::uint16_t kOpcode = 0x0151;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t guild_id = 0;
};

// PACKET_CZ_REQ_GUILD_EMBLEM_IMG3
struct CzReqGuildEmblemImg3 {
  static constexpr std::uint16_t kOpcode = 0x0b46;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int32_t guild_id = 0;
  std::int32_t emblem_id = 0;
};

// PACKET_CZ_REQ_GUILD_EMBLEM_IMG2
struct CzReqGuildEmblemImg2 {
  static constexpr std::uint16_t kOpcode = 0x0b1e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::int32_t guild_id = 0;
  std::int32_t emblem_id = 0;
  std::int32_t unused = 0;
};

// PACKET_ZC_CHANGE_GUILD
struct ZcChangeGuild {
  static constexpr std::uint16_t kOpcode = 0x0b47;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::int32_t guild_id = 0;
  std::uint32_t emblem_id = 0;
  std::uint32_t AID = 0;
};

// PACKET_ZC_GUILD_EMBLEM_IMG
struct ZcGuildEmblemImg {
  static constexpr std::uint16_t kOpcode = 0x0b36;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 14;

  std::uint16_t result = 0;
  std::int32_t guild_id = 0;
  std::uint32_t emblem_id = 0;
  std::string emblem_data;
};

// PACKET_CZ_ADVENTURER_AGENCY_JOIN_REQ
struct CzAdventurerAgencyJoinReq {
  static constexpr std::uint16_t kOpcode = 0x0ae6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int32_t GID = 0;
  std::int32_t AID = 0;
};

// PACKET_ZC_ADVENTURER_AGENCY_JOIN_RESULT
struct ZcAdventurerAgencyJoinResult {
  static constexpr std::uint16_t kOpcode = 0x0afa;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 58;

  std::string player_name;
  std::string party_name;
  std::int32_t AID = 0;
  std::int32_t result = 0;
};

// PACKET_ZC_ADVENTURER_AGENCY_JOIN_REQ
struct ZcAdventurerAgencyJoinReq {
  static constexpr std::uint16_t kOpcode = 0x0ae7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 38;

  std::int32_t GRID = 0;
  std::int32_t AID = 0;
  std::string groupName;
  std::int16_t level = 0;
  std::int16_t job = 0;
};

// PACKET_CZ_ADVENTURER_AGENCY_JOIN_RESULT
struct CzAdventurerAgencyJoinResult {
  static constexpr std::uint16_t kOpcode = 0x0af8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int32_t GRID = 0;
  std::int32_t AID = 0;
  std::int8_t result = 0;
};

// PACKET_ZC_MOONLIGHT_SETTINGS
struct ZcMoonlightSettings {
  static constexpr std::uint16_t kOpcode = 0x0f05;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t char_id = 0;
  std::int16_t count = 0;
};

// PACKET_CZ_MOONLIGHT_SETTING
struct CzMoonlightSetting {
  static constexpr std::uint16_t kOpcode = 0x0f04;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int16_t packetLength = 0;
  std::int16_t id = 0;
  std::uint32_t value = 0;
};

// PACKET_CZ_MOONLIGHT_INTEGRITY
struct CzMoonlightIntegrity {
  static constexpr std::uint16_t kOpcode = 0x0f02;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 76;

  std::array<std::uint8_t, 32> hash = {};
  std::string machine_guid;
  std::int32_t patch_index = 0;
};

// PACKET_CZ_MOONCLIENT_INTEGRITY
struct CzMoonclientIntegrity {
  static constexpr std::uint16_t kOpcode = 0x0f39;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 44;

  std::string machine_guid;
  std::int32_t patch_index = 0;
};

// PACKET_ZC_MOONLIGHT_KICK_NOTICE
struct ZcMoonlightKickNotice {
  static constexpr std::uint16_t kOpcode = 0x0f03;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;
};

// PACKET_ZC_MOONLIGHT_DISCORD_MSG
struct ZcMoonlightDiscordMsg {
  static constexpr std::uint16_t kOpcode = 0x0f08;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::string msg;
};

// PACKET_CZ_MOONLIGHT_PRESET_CMD
struct CzMoonlightPresetCmd {
  static constexpr std::uint16_t kOpcode = 0x0f06;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint8_t cmd = 0;
  std::uint8_t no = 0;
  std::string name;
};

// PACKET_ZC_MOONLIGHT_PRESET_LIST
struct ZcMoonlightPresetList {
  static constexpr std::uint16_t kOpcode = 0x0f07;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint8_t active_no = 0;
  std::uint8_t count = 0;
};

// PACKET_ZC_MOONLIGHT_SKILL_DMG
struct ZcMoonlightSkillDmg {
  static constexpr std::uint16_t kOpcode = 0x0f09;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t src_aid = 0;
  std::int32_t damage = 0;
};

// PACKET_CZ_MOONLIGHT_CHEAT_REPORT
struct CzMoonlightCheatReport {
  static constexpr std::uint16_t kOpcode = 0x0f0a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 100;

  std::int16_t packetLength = 0;
  std::string tool_name;
  std::string detail;
};

// PACKET_CZ_MOONLIGHT_REQ_TECHDATA
struct CzMoonlightReqTechdata {
  static constexpr std::uint16_t kOpcode = 0x0f0b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::int16_t packetLength = 0;
  std::uint32_t id = 0;
  std::uint8_t is_skill = 0;
  std::uint8_t scope = 0;
};

// PACKET_ZC_MOONLIGHT_TECHDATA
struct ZcMoonlightTechdata {
  static constexpr std::uint16_t kOpcode = 0x0f0c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t id = 0;
  std::uint8_t is_skill = 0;
  std::uint8_t scope = 0;
};

// PACKET_CZ_MOONLIGHT_REQ_DAMAGE
struct CzMoonlightReqDamage {
  static constexpr std::uint16_t kOpcode = 0x0f0d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::int16_t packetLength = 0;
  std::uint32_t skill_id = 0;
  std::uint16_t skill_lv = 0;
  std::uint32_t target_mob_id = 0;
};

// PACKET_ZC_MOONLIGHT_DAMAGE
struct ZcMoonlightDamage {
  static constexpr std::uint16_t kOpcode = 0x0f0e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 42;

  std::uint32_t skill_id = 0;
  std::uint16_t skill_lv = 0;
  std::uint32_t target_mob_id = 0;
  std::uint8_t status = 0;
  std::uint8_t atk_type = 0;
  std::uint16_t hits = 0;
  std::int64_t dmg_min = 0;
  std::int64_t dmg_max = 0;
  std::int64_t dmg_avg = 0;
};

// PACKET_ZC_MOONLIGHT_STORAGE_PRICES
struct ZcMoonlightStoragePrices {
  static constexpr std::uint16_t kOpcode = 0x0f0f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t count = 0;
};

// PACKET_CZ_MOONLIGHT_REQ_COMPAT_CARDS
struct CzMoonlightReqCompatCards {
  static constexpr std::uint16_t kOpcode = 0x0f18;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t packetLength = 0;
  std::uint16_t index_equip = 0;
};

// PACKET_ZC_MOONLIGHT_COMPAT_CARDS
struct ZcMoonlightCompatCards {
  static constexpr std::uint16_t kOpcode = 0x0f19;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t index_equip = 0;
  std::int16_t count = 0;
};

// PACKET_CZ_MOONLIGHT_JUMP
struct CzMoonlightJump {
  static constexpr std::uint16_t kOpcode = 0x0f1a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t packetLength = 0;
};

// PACKET_ZC_MOONLIGHT_JUMP
struct ZcMoonlightJump {
  static constexpr std::uint16_t kOpcode = 0x0f1b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t gid = 0;
};

// PACKET_ZC_MOONLIGHT_COOK_MASTERY
struct ZcMoonlightCookMastery {
  static constexpr std::uint16_t kOpcode = 0x0f1c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t mastery = 0;
};

// PACKET_CZ_MOONLIGHT_OPEN_STORAGE
struct CzMoonlightOpenStorage {
  static constexpr std::uint16_t kOpcode = 0x0f1d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::int16_t packetLength = 0;
  std::uint8_t stor_id = 0;
};

// PACKET_ZC_MOONLIGHT_STORAGE_LIST
struct ZcMoonlightStorageList {
  static constexpr std::uint16_t kOpcode = 0x0f1e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint8_t cur_id = 0;
  std::uint8_t count = 0;
};

// PACKET_CZ_MOONLIGHT_REQ_MOBINFO
struct CzMoonlightReqMobinfo {
  static constexpr std::uint16_t kOpcode = 0x0f1f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::int16_t packetLength = 0;
  std::uint32_t mob_id = 0;
  std::uint8_t by_view = 0;
};

// PACKET_ZC_MOONLIGHT_MOBINFO
struct ZcMoonlightMobinfo {
  static constexpr std::uint16_t kOpcode = 0x0f20;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::uint32_t mob_id = 0;
  std::uint8_t status = 0;
};

// PACKET_ZC_MOONLIGHT_CHANNEL_LIST
struct ZcMoonlightChannelList {
  static constexpr std::uint16_t kOpcode = 0x0f21;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::uint8_t count = 0;
};

// PACKET_CZ_MOONLIGHT_REQ_ENTITY_PROPS
struct CzMoonlightReqEntityProps {
  static constexpr std::uint16_t kOpcode = 0x0f22;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int16_t packetLength = 0;
  std::uint32_t gid = 0;
};

// PACKET_ZC_MOONLIGHT_ENTITY_PROPS
struct ZcMoonlightEntityProps {
  static constexpr std::uint16_t kOpcode = 0x0f23;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t gid = 0;
  std::uint8_t status = 0;
  std::uint8_t count = 0;
};

// PACKET_CZ_MOONLIGHT_UI_CAPS
struct CzMoonlightUiCaps {
  static constexpr std::uint16_t kOpcode = 0x0f24;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t caps = 0;
};

// PACKET_CZ_MOONLIGHT_NPC_ADMIN
struct CzMoonlightNpcAdmin {
  static constexpr std::uint16_t kOpcode = 0x0f25;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::int16_t packetLength = 0;
  std::uint32_t gid = 0;
  std::uint8_t action = 0;
};

// PACKET_CZ_MOONLIGHT_PLAYER_ADMIN
struct CzMoonlightPlayerAdmin {
  static constexpr std::uint16_t kOpcode = 0x0f2b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 13;

  std::int16_t packetLength = 0;
  std::uint32_t aid = 0;
  std::uint8_t action = 0;
  std::int32_t param = 0;
};

// PACKET_CZ_MOONLIGHT_STYLE
struct CzMoonlightStyle {
  static constexpr std::uint16_t kOpcode = 0x0f26;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 56;

  std::int16_t packetLength = 0;
  std::uint8_t version = 0;
  std::uint8_t flags = 0;
  std::uint32_t body_key = 0;
  std::int16_t palette_id = 0;
  std::int16_t hair_palette_id = 0;
  std::int16_t hair_style = 0;
  std::array<std::uint8_t, 40> adjusts = {};
};

// PACKET_ZC_MOONLIGHT_STYLES
struct ZcMoonlightStyles {
  static constexpr std::uint16_t kOpcode = 0x0f27;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t count = 0;
};

// PACKET_ZC_MOONLIGHT_STYLE_OPEN
struct ZcMoonlightStyleOpen {
  static constexpr std::uint16_t kOpcode = 0x0f28;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::uint8_t mode = 0;
};

// PACKET_CZ_MOONLIGHT_TARGET_INFO
struct CzMoonlightTargetInfo {
  static constexpr std::uint16_t kOpcode = 0x0f29;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int16_t packetLength = 0;
  std::uint32_t gid = 0;
};

// PACKET_ZC_MOONLIGHT_TARGET_INFO
struct ZcMoonlightTargetInfo {
  static constexpr std::uint16_t kOpcode = 0x0f2a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 34;

  std::uint32_t gid = 0;
  std::uint8_t status = 0;
  std::uint8_t known = 0;
  std::uint8_t type = 0;
  std::int16_t level = 0;
  std::uint32_t hp = 0;
  std::uint32_t maxhp = 0;
  std::uint32_t sp = 0;
  std::uint32_t maxsp = 0;
  std::uint8_t race = 0;
  std::uint8_t element = 0;
  std::uint8_t element_lv = 0;
  std::uint8_t size = 0;
  std::uint8_t boss = 0;
};

// PACKET_CZ_MOONLIGHT_MVP_CMD
struct CzMoonlightMvpCmd {
  static constexpr std::uint16_t kOpcode = 0x0f30;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint8_t cmd = 0;
  std::uint32_t a = 0;
  std::uint32_t b = 0;
  std::string text;
};

// PACKET_ZC_MOONLIGHT_MVP_STATE
struct ZcMoonlightMvpState {
  static constexpr std::uint16_t kOpcode = 0x0f31;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 15;

  std::uint8_t kind = 0;
  std::int64_t server_time = 0;
  std::uint16_t count = 0;
};

// PACKET_ZC_MOONLIGHT_MVP_GROUP
struct ZcMoonlightMvpGroup {
  static constexpr std::uint16_t kOpcode = 0x0f32;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 47;

  std::uint8_t kind = 0;
  std::uint8_t result = 0;
  std::uint32_t group_id = 0;
  std::uint32_t owner_user_id = 0;
  std::string name;
  std::uint8_t count = 0;
};

// PACKET_ZC_MOONLIGHT_PARTY_SHARE
struct ZcMoonlightPartyShare {
  static constexpr std::uint16_t kOpcode = 0x0f35;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t idle_secs = 0;
  std::uint16_t count = 0;
};

// PACKET_ZC_MOONLIGHT_FLAG_GRAFFITI
struct ZcMoonlightFlagGraffiti {
  static constexpr std::uint16_t kOpcode = 0x0f36;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 25;

  std::uint32_t unit_id = 0;
  std::uint32_t creator_id = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint32_t guild_id = 0;
  std::uint32_t emblem_id = 0;
  std::uint8_t level = 0;
};

// PACKET_ZC_MOONLIGHT_UNIT_MASTER
struct ZcMoonlightUnitMaster {
  static constexpr std::uint16_t kOpcode = 0x0f37;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t GID = 0;
  std::uint32_t master_id = 0;
};

// PACKET_ZC_MOONLIGHT_SERVER_RULES_entry
struct ZcMoonlightServerRulesEntry {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t key = 0;
  std::int32_t value = 0;
};

// PACKET_ZC_MOONLIGHT_SERVER_RULES
struct ZcMoonlightServerRules {
  static constexpr std::uint16_t kOpcode = 0x0f38;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcMoonlightServerRulesEntry> rules;
};

// PACKET_ZC_MOONLIGHT_CHAT_AUTHOR
struct ZcMoonlightChatAuthor {
  static constexpr std::uint16_t kOpcode = 0x0f3a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 44;

  std::uint16_t next_opcode = 0;
  std::uint8_t scope = 0;
  std::uint8_t channel_type = 0;
  std::uint32_t account_id = 0;
  std::uint32_t char_id = 0;
  std::uint16_t header_len = 0;
  std::uint16_t name_offset = 0;
  std::string name;
};

// PACKET_ZC_MOONLIGHT_QUEST_END
struct ZcMoonlightQuestEnd {
  static constexpr std::uint16_t kOpcode = 0x0f3b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 13;

  std::int32_t quest_id = 0;
  std::uint8_t reason = 0;
  std::int32_t next_quest_id = 0;
};

// PACKET_ZC_MOONLIGHT_DISCORD_RICH
struct ZcMoonlightDiscordRich {
  static constexpr std::uint16_t kOpcode = 0x0f3c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 18;

  std::uint64_t discord_user_id = 0;
  std::uint8_t channel_len = 0;
  std::uint8_t name_len = 0;
  std::uint8_t avatar_len = 0;
  std::uint8_t flags = 0;
  std::uint16_t text_len = 0;
};

// PACKET_MOONLIGHT_EQUIP_BONUS
struct MoonlightEquipBonus {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 23;

  std::uint8_t level = 0;
  std::uint16_t type = 0;
  std::array<std::int32_t, 5> values = {};
};

// PACKET_ZC_MOONLIGHT_EQUIP_ATTRIBUTES
struct ZcMoonlightEquipAttributes {
  static constexpr std::uint16_t kOpcode = 0x0f3d;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 10;

  std::uint8_t equipped = 0;
  std::uint8_t kind = 0;
  std::uint32_t id = 0;
  std::vector<MoonlightEquipBonus> bonuses;
};

// PACKET_MOONLIGHT_CHAT_BLOCK
struct MoonlightChatBlock {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 33;

  std::uint32_t date = 0;
  std::int32_t minutes = 0;
  std::uint8_t kind = 0;
  std::string author;
};

// PACKET_CZ_MOONLIGHT_REQ_CHAT_BLOCKS
struct CzMoonlightReqChatBlocks {
  static constexpr std::uint16_t kOpcode = 0x0f3e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int16_t packetLength = 0;
  std::uint32_t aid = 0;
};

// PACKET_ZC_MOONLIGHT_CHAT_BLOCKS
struct ZcMoonlightChatBlocks {
  static constexpr std::uint16_t kOpcode = 0x0f3f;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 9;

  std::uint32_t aid = 0;
  std::uint8_t status = 0;
  std::vector<MoonlightChatBlock> entries;
};

// PACKET_ZC_MOONLIGHT_INSTANCE_INFO
struct ZcMoonlightInstanceInfo {
  static constexpr std::uint16_t kOpcode = 0x0f40;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 49;

  std::uint32_t server_time = 0;
  std::uint32_t duration_limit = 0;
  std::uint32_t idle_timeout = 0;
  std::int16_t hardmode = 0;
  std::uint8_t turbo = 0;
  std::uint8_t mode = 0;
  std::uint8_t destroyable = 0;
  std::uint8_t may_destroy = 0;
  std::uint8_t on_instance_map = 0;
  std::uint16_t players_inside = 0;
  std::string owner_name;
};

// PACKET_MOONLIGHT_MARKET_ITEM_ID
struct MoonlightMarketItemId {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint32_t itemId = 0;
};

// PACKET_CZ_MOONLIGHT_MARKET_REQ
struct CzMoonlightMarketReq {
  static constexpr std::uint16_t kOpcode = 0x0f41;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 27;

  std::uint16_t query_id = 0;
  std::uint8_t kind = 0;
  std::uint8_t category = 0;
  std::uint8_t refine_min = 0;
  std::uint8_t refine_max = 0;
  std::uint8_t flags = 0;
  std::uint32_t price_min = 0;
  std::uint32_t price_max = 0;
  std::uint32_t store_id = 0;
  std::uint32_t owner_aid = 0;
  std::vector<MoonlightMarketItemId> items;
};

// PACKET_MOONLIGHT_MARKET_ENTRY
struct MoonlightMarketEntry {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 201;

  std::uint32_t store_id = 0;
  std::uint32_t owner_aid = 0;
  std::uint32_t itemId = 0;
  std::uint16_t amount = 0;
  std::uint32_t price = 0;
  std::uint8_t refine = 0;
  std::uint8_t grade = 0;
  std::uint8_t identified = 0;
  std::uint8_t damaged = 0;
  std::uint8_t autotrade = 0;
  std::array<std::uint32_t, 4> cards = {};
  std::array<Itemoptions, 5> options = {};
  std::string map;
  std::uint16_t x = 0;
  std::uint16_t y = 0;
  std::string title;
  std::uint8_t currency = 0;
  std::uint32_t currency_item = 0;
  std::string currency_name;
};

// PACKET_ZC_MOONLIGHT_MARKET
struct ZcMoonlightMarket {
  static constexpr std::uint16_t kOpcode = 0x0f42;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 74;

  std::uint16_t query_id = 0;
  std::uint8_t status = 0;
  std::uint8_t kind = 0;
  std::uint16_t part = 0;
  std::uint16_t part_count = 0;
  std::uint16_t total = 0;
  std::uint8_t truncated = 0;
  std::uint32_t store_id = 0;
  std::uint32_t owner_aid = 0;
  std::uint8_t reserved = 0;
  std::string query;
  std::vector<MoonlightMarketEntry> entries;
};

// PACKET_CZ_MOONLIGHT_GM_OUTFIT
struct CzMoonlightGmOutfit {
  static constexpr std::uint16_t kOpcode = 0x0f43;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::int16_t packetLength = 0;
  std::uint8_t worn = 0;
};

// PACKET_ZC_MOONLIGHT_GM_OUTFIT
struct ZcMoonlightGmOutfit {
  static constexpr std::uint16_t kOpcode = 0x0f44;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::uint32_t GID = 0;
  std::uint8_t worn = 0;
};

// PACKET_CZ_MOONCLIENT_MAP_EDIT_PUT
struct CzMoonclientMapEditPut {
  static constexpr std::uint16_t kOpcode = 0x0f45;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 36;

  std::string map;
  std::uint32_t base_revision = 0;
  std::uint32_t zipped_size = 0;
  std::uint32_t text_size = 0;
  std::uint32_t offset = 0;
};

// PACKET_ZC_MOONCLIENT_MAP_EDIT_RESULT
struct ZcMoonclientMapEditResult {
  static constexpr std::uint16_t kOpcode = 0x0f46;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 25;

  std::string map;
  std::uint8_t result = 0;
  std::uint32_t revision = 0;
};

// PACKET_ZC_MOONCLIENT_MAP_EDIT_DOC
struct ZcMoonclientMapEditDoc {
  static constexpr std::uint16_t kOpcode = 0x0f47;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 60;

  std::string map;
  std::uint32_t revision = 0;
  std::uint32_t zipped_size = 0;
  std::uint32_t text_size = 0;
  std::uint32_t offset = 0;
  std::string author;
};

// PACKET_ZC_MOONCLIENT_MAP_EDIT_REVISION
struct ZcMoonclientMapEditRevision {
  static constexpr std::uint16_t kOpcode = 0x0f48;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 25;

  std::string map;
  std::uint32_t revision = 0;
  std::uint8_t can_publish = 0;
};

// PACKET_CZ_MOONCLIENT_MAP_EDIT_GET
struct CzMoonclientMapEditGet {
  static constexpr std::uint16_t kOpcode = 0x0f49;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 20;

  std::int16_t packetLength = 0;
  std::string map;
};

// PACKET_ZC_MOONLIGHT_ATTACK_ELEMENT
struct ZcMoonlightAttackElement {
  static constexpr std::uint16_t kOpcode = 0x0f4a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::uint32_t GID = 0;
  std::uint8_t element = 0;
};

// PACKET_CZ_MOONLIGHT_MINIGAME_TROPHY
struct CzMoonlightMinigameTrophy {
  static constexpr std::uint16_t kOpcode = 0x0f4b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::int16_t packetLength = 0;
  std::uint8_t trophy = 0;
};

// PACKET_CZ_MOONLIGHT_SURVIVORS_BEGIN
struct CzMoonlightSurvivorsBegin {
  static constexpr std::uint16_t kOpcode = 0x0f4c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int16_t packetLength = 0;
  std::uint16_t ruleset = 0;
  std::uint16_t arena = 0;
  std::uint16_t class_id = 0;
  std::uint8_t board = 0;
};

// PACKET_ZC_MOONLIGHT_SURVIVORS_TICKET
struct ZcMoonlightSurvivorsTicket {
  static constexpr std::uint16_t kOpcode = 0x0f4d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 25;

  std::uint8_t result = 0;
  std::uint32_t ticket = 0;
  std::uint64_t seed = 0;
  std::int64_t issued_at = 0;
};

// PACKET_CZ_MOONLIGHT_SURVIVORS_END
struct CzMoonlightSurvivorsEnd {
  static constexpr std::uint16_t kOpcode = 0x0f4e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 29;

  std::int16_t packetLength = 0;
  std::uint32_t ticket = 0;
  std::uint32_t survived_steps = 0;
  std::uint32_t kill_points = 0;
  std::uint32_t kills = 0;
  std::uint16_t boss_kills = 0;
  std::uint16_t level = 0;
  std::uint8_t outcome = 0;
  std::uint32_t journal_bytes = 0;
};

// PACKET_ZC_MOONLIGHT_SURVIVORS_RESULT
struct ZcMoonlightSurvivorsResult {
  static constexpr std::uint16_t kOpcode = 0x0f4f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 27;

  std::uint32_t ticket = 0;
  std::uint8_t verdict = 0;
  std::uint64_t score = 0;
  std::uint16_t rank = 0;
  std::uint64_t best = 0;
};

// PACKET_CZ_MOONLIGHT_SURVIVORS_BOARD
struct CzMoonlightSurvivorsBoard {
  static constexpr std::uint16_t kOpcode = 0x0f50;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::int16_t packetLength = 0;
  std::uint16_t ruleset = 0;
  std::uint16_t arena = 0;
  std::uint16_t class_id = 0;
  std::uint8_t board = 0;
};

// PACKET_ZC_MOONLIGHT_SURVIVORS_BOARD_entry
struct ZcMoonlightSurvivorsBoardEntry {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 41;

  std::uint16_t rank = 0;
  std::uint64_t score = 0;
  std::uint32_t survived_steps = 0;
  std::uint16_t level = 0;
  std::uint8_t verified = 0;
  std::string name;
};

// PACKET_ZC_MOONLIGHT_SURVIVORS_BOARD
struct ZcMoonlightSurvivorsBoard {
  static constexpr std::uint16_t kOpcode = 0x0f51;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 22;

  std::uint16_t ruleset = 0;
  std::uint16_t arena = 0;
  std::uint16_t class_id = 0;
  std::uint8_t board = 0;
  std::uint16_t own_rank = 0;
  std::uint64_t own_score = 0;
  std::uint8_t count = 0;
  std::vector<ZcMoonlightSurvivorsBoardEntry> entries;
};

// PACKET_CZ_MOONLIGHT_SURVIVORS_JOURNAL
struct CzMoonlightSurvivorsJournal {
  static constexpr std::uint16_t kOpcode = 0x0f52;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t ticket = 0;
  std::uint32_t offset = 0;
  std::vector<std::uint8_t> data;
};

// PACKET_ZC_MOONLIGHT_SURVIVORS_SERVICE
struct ZcMoonlightSurvivorsService {
  static constexpr std::uint16_t kOpcode = 0x0f53;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::uint16_t ruleset_low = 0;
  std::uint16_t ruleset_high = 0;
  std::int64_t server_utc = 0;
};

// PACKET_ZC_MOONLIGHT_CARD_ALBUM
struct ZcMoonlightCardAlbum {
  static constexpr std::uint16_t kOpcode = 0x0f33;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint8_t result = 0;
  std::uint16_t count = 0;
};

// PACKET_CZ_MOONLIGHT_CARD_ALBUM_CMD
struct CzMoonlightCardAlbumCmd {
  static constexpr std::uint16_t kOpcode = 0x0f34;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::uint8_t cmd = 0;
  std::uint32_t arg = 0;
  std::uint16_t amount = 0;
};

// PACKET_CZ_MOONLIGHT_REQ_STATUS_LIST
struct CzMoonlightReqStatusList {
  static constexpr std::uint16_t kOpcode = 0x0f2c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int16_t packetLength = 0;
  std::uint32_t gid = 0;
};

// PACKET_ZC_MOONLIGHT_STATUS_LIST
struct ZcMoonlightStatusList {
  static constexpr std::uint16_t kOpcode = 0x0f2d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t gid = 0;
  std::uint8_t status = 0;
  std::uint8_t count = 0;
};

// PACKET_CZ_MOONLIGHT_REQ_LOOKS
struct CzMoonlightReqLooks {
  static constexpr std::uint16_t kOpcode = 0x0f2e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::int16_t packetLength = 0;
  std::uint8_t what = 0;
};

// PACKET_ZC_MOONLIGHT_LOOKS
struct ZcMoonlightLooks {
  static constexpr std::uint16_t kOpcode = 0x0f2f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t count = 0;
};

// PACKET_ZC_MOONLIGHT_STAT_BONUS
struct ZcMoonlightStatBonus {
  static constexpr std::uint16_t kOpcode = 0x0f10;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 224;

  std::array<std::int16_t, 6> param_equip = {};
  std::array<std::int16_t, 6> param_bonus = {};
  std::int32_t eatk = 0;
  std::int32_t ematk = 0;
  std::int32_t melee_pct = 0;
  std::int32_t ranged_pct = 0;
  std::int32_t crit_dmg_pct = 0;
  std::int32_t hp_add = 0;
  std::int32_t sp_add = 0;
  std::int32_t aspd_add = 0;
  std::int32_t vcast_pct = 0;
  std::int32_t fcast_pct = 0;
  std::int32_t atk_pct = 0;
  std::int32_t matk_pct = 0;
  std::int32_t dmg_ret_melee = 0;
  std::int32_t dmg_ret_ranged = 0;
  std::int32_t dmg_ret_magic = 0;
  std::int32_t double_pct = 0;
  std::int32_t perfect_hit = 0;
  std::int32_t hp_pct = 0;
  std::int32_t sp_pct = 0;
  std::int32_t hp_regen_pct = 0;
  std::int32_t sp_regen_pct = 0;
  std::int32_t crit_def_pct = 0;
  std::int32_t hp_on_kill = 0;
  std::int32_t sp_on_kill = 0;
  std::int32_t unbreak_pct = 0;
  std::int32_t pot_hp_pct = 0;
  std::int32_t pot_sp_pct = 0;
  std::int32_t heal_up_pct = 0;
  std::int32_t delay_pct = 0;
  std::int32_t add_vcast_ms = 0;
  std::int32_t add_fcast_ms = 0;
  std::int32_t steal_pct = 0;
  std::int32_t def_melee_pct = 0;
  std::int32_t def_ranged_pct = 0;
  std::int32_t def_magic_pct = 0;
  std::int32_t def_misc_pct = 0;
  std::int32_t splash = 0;
  std::int32_t splash_add = 0;
  std::int32_t hp_drain_pct = 0;
  std::int32_t sp_drain_pct = 0;
  std::int32_t break_weapon_pct = 0;
  std::int32_t break_armor_pct = 0;
  std::int32_t zeny_bonus_pct = 0;
  std::int32_t classchange_pct = 0;
  std::int32_t dmg_ret_reduce = 0;
  std::int32_t magic_hp_gain = 0;
  std::int32_t magic_sp_gain = 0;
  std::int32_t refine_atk = 0;
  std::int32_t refine_def = 0;
};

// PACKET_CZ_MOONLIGHT_REQ_ITEMSCRIPT
struct CzMoonlightReqItemscript {
  static constexpr std::uint16_t kOpcode = 0x0f11;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int16_t packetLength = 0;
  std::uint32_t id = 0;
};

// PACKET_ZC_MOONLIGHT_ITEMSCRIPT
struct ZcMoonlightItemscript {
  static constexpr std::uint16_t kOpcode = 0x0f12;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::uint32_t id = 0;
  std::uint8_t status = 0;
};

// PACKET_CZ_MOONLIGHT_BUG_REPORT
struct CzMoonlightBugReport {
  static constexpr std::uint16_t kOpcode = 0x0f13;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint8_t category = 0;
  std::uint16_t ctx_len = 0;
};

// PACKET_ZC_MOONLIGHT_BUG_REPORT_ACK
struct ZcMoonlightBugReportAck {
  static constexpr std::uint16_t kOpcode = 0x0f14;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::uint8_t status = 0;
};

// PACKET_CZ_MOONLIGHT_COMPANION
struct CzMoonlightCompanion {
  static constexpr std::uint16_t kOpcode = 0x0f15;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::int16_t packetLength = 0;
  std::uint8_t kind = 0;
  std::uint8_t action = 0;
  std::uint8_t arg = 0;
};

// PACKET_ZC_MOONLIGHT_COMPANION_STATE
struct ZcMoonlightCompanionState {
  static constexpr std::uint16_t kOpcode = 0x0f16;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 18;

  std::uint8_t pushcart_lv = 0;
  std::uint8_t changecart_lv = 0;
  std::uint8_t riding_lv = 0;
  std::uint8_t falcon_lv = 0;
  std::uint8_t cart_active = 0;
  std::uint8_t riding_active = 0;
  std::uint8_t falcon_active = 0;
  std::uint8_t cart_deco_max = 0;
  std::uint16_t pushcart_id = 0;
  std::uint16_t riding_id = 0;
  std::uint16_t falcon_id = 0;
};

// PACKET_ZC_MOONLIGHT_HATEFFECT_MAP
struct ZcMoonlightHateffectMap {
  static constexpr std::uint16_t kOpcode = 0x0f17;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t count = 0;
};

// PACKET_ZC_USER_COUNT
struct ZcUserCount {
  static constexpr std::uint16_t kOpcode = 0x00c2;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t playersCount = 0;
};

// PACKET_ZC_PC_PURCHASE_RESULT
struct ZcPcPurchaseResult {
  static constexpr std::uint16_t kOpcode = 0x00ca;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t result = 0;
};

// PACKET_CZ_REQ_MAKINGARROW
struct CzReqMakingarrow {
  static constexpr std::uint16_t kOpcode = 0x01ae;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t itemId = 0;
};

// PACKET_CZ_SE_PC_BUY_CASHITEM_LIST_sub
struct CzSePcBuyCashitemListSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t itemId = 0;
  std::uint32_t amount = 0;
  std::uint16_t tab = 0;
};

// PACKET_CZ_SE_PC_BUY_CASHITEM_LIST
struct CzSePcBuyCashitemList {
  static constexpr std::uint16_t kOpcode = 0x0848;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 10;

  std::uint16_t count = 0;
  std::uint32_t kafraPoints = 0;
  std::vector<CzSePcBuyCashitemListSub> items;
};

// PACKET_CZ_REQ_CASH_BARGAIN_SALE_ITEM_INFO
struct CzReqCashBargainSaleItemInfo {
  static constexpr std::uint16_t kOpcode = 0x09ac;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t AID = 0;
  std::uint32_t itemId = 0;
};

// PACKET_ZC_ACK_CASH_BARGAIN_SALE_ITEM_INFO
struct ZcAckCashBargainSaleItemInfo {
  static constexpr std::uint16_t kOpcode = 0x09ad;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint16_t result = 0;
  std::uint32_t itemId = 0;
  std::uint32_t price = 0;
};

// PACKET_CZ_REQ_APPLY_BARGAIN_SALE_ITEM
struct CzReqApplyBargainSaleItem {
  static constexpr std::uint16_t kOpcode = 0x09ae;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 20;

  std::uint32_t AID = 0;
  std::uint32_t itemId = 0;
  std::uint32_t amount = 0;
  std::uint32_t startTime = 0;
  std::uint16_t hours = 0;
};

// PACKET_CZ_REQ_REMOVE_BARGAIN_SALE_ITEM
struct CzReqRemoveBargainSaleItem {
  static constexpr std::uint16_t kOpcode = 0x09b0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t AID = 0;
  std::uint32_t itemId = 0;
};

// PACKET_ZC_NOTIFY_BARGAIN_SALE_SELLING
struct ZcNotifyBargainSaleSelling {
  static constexpr std::uint16_t kOpcode = 0x09b2;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t itemId = 0;
  std::uint32_t remainingTime = 0;
};

// PACKET_ZC_NOTIFY_BARGAIN_SALE_CLOSE
struct ZcNotifyBargainSaleClose {
  static constexpr std::uint16_t kOpcode = 0x09b3;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t itemId = 0;
};

// PACKET_ZC_ACK_COUNT_BARGAIN_SALE_ITEM
struct ZcAckCountBargainSaleItem {
  static constexpr std::uint16_t kOpcode = 0x09c4;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t itemId = 0;
  std::uint32_t amount = 0;
};

// PACKET_ZC_ACK_GUILDSTORAGE_LOG_sub
struct ZcAckGuildstorageLogSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 93;

  std::uint32_t id = 0;
  std::uint32_t itemId = 0;
  std::int32_t amount = 0;
  std::uint8_t action = 0;
  std::int32_t refine = 0;
  std::int64_t uniqueId = 0;
  std::uint8_t IsIdentified = 0;
  std::uint16_t itemType = 0;
  Equipslotinfo slot;
  std::string name;
  std::string time;
  std::uint8_t attribute = 0;
};

// PACKET_ZC_ACK_GUILDSTORAGE_LOG
struct ZcAckGuildstorageLog {
  static constexpr std::uint16_t kOpcode = 0x09da;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t result = 0;
  std::uint16_t amount = 0;
  std::vector<ZcAckGuildstorageLogSub> items;
};

// PACKET_CZ_ADVANCED_STATUS_CHANGE
struct CzAdvancedStatusChange {
  static constexpr std::uint16_t kOpcode = 0x0b24;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int16_t type = 0;
  std::int16_t amount = 0;
};

// PACKET_CZ_REQ_ADD_NEW_EMBLEM
struct CzReqAddNewEmblem {
  static constexpr std::uint16_t kOpcode = 0x0b46;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t guild_id = 0;
  std::uint32_t version = 0;
};

// PACKET_ZC_BROADCAST
struct ZcBroadcast {
  static constexpr std::uint16_t kOpcode = 0x009a;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::string message;
};

// PACKET_ZC_BROADCAST2
struct ZcBroadcast2 {
  static constexpr std::uint16_t kOpcode = 0x01c3;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 16;

  std::uint32_t fontColor = 0;
  std::int16_t fontType = 0;
  std::int16_t fontSize = 0;
  std::int16_t fontAlign = 0;
  std::int16_t fontY = 0;
  std::string message;
};

// PACKET_ZC_ENTRY_QUEUE_INIT
struct ZcEntryQueueInit {
  static constexpr std::uint16_t kOpcode = 0x090e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_RODEX_RETURN
struct CzRodexReturn {
  static constexpr std::uint16_t kOpcode = 0x0b98;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t msgId = 0;
};

// PACKET_ZC_RODEX_RETURN
struct ZcRodexReturn {
  static constexpr std::uint16_t kOpcode = 0x0b99;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t msgId = 0;
  std::uint32_t result = 0;
};

// PACKET_CZ_REQ_STYLE_CLOSE
struct CzReqStyleClose {
  static constexpr std::uint16_t kOpcode = 0x0a48;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_SUMMON_HP_INIT
struct ZcSummonHpInit {
  static constexpr std::uint16_t kOpcode = 0x0b6b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t summonAID = 0;
  std::uint32_t CurrentHP = 0;
  std::uint32_t MaxHP = 0;
};

// PACKET_ZC_SUMMON_HP_UPDATE
struct ZcSummonHpUpdate {
  static constexpr std::uint16_t kOpcode = 0x0b6c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t summonAID = 0;
  std::uint16_t VarId = 0;
  std::uint32_t Value = 0;
};

// PACKET_ZC_REPUTE_INFO_sub
struct ZcReputeInfoSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::uint64_t type = 0;
  std::int64_t points = 0;
};

// PACKET_ZC_REPUTE_INFO
struct ZcReputeInfo {
  static constexpr std::uint16_t kOpcode = 0x0b8d;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 5;

  std::uint8_t success = 0;
  std::vector<ZcReputeInfoSub> list;
};

// PACKET_ZC_REPUTE_OPEN
struct ZcReputeOpen {
  static constexpr std::uint16_t kOpcode = 0x0b8e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 18;

  std::uint64_t table = 0;
  std::uint32_t type = 0;
  std::uint32_t mode = 0;
};

// PACKET_ZC_UI_OPEN_V3
struct ZcUiOpenV3 {
  static constexpr std::uint16_t kOpcode = 0x0b9a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::uint8_t type = 0;
  std::uint64_t data = 0;
};

// PACKET_ZC_TARGET_SPIRITS
struct ZcTargetSpirits {
  static constexpr std::uint16_t kOpcode = 0x0b68;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t GID = 0;
  std::uint32_t unknown_val = 0;
  std::uint16_t amount = 0;
};

// PACKET_ZC_FRIENDS_LIST_sub
struct ZcFriendsListSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 32;

  std::uint32_t AID = 0;
  std::uint32_t CID = 0;
  std::string name;
};

// PACKET_ZC_FRIENDS_LIST
struct ZcFriendsList {
  static constexpr std::uint16_t kOpcode = 0x0201;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcFriendsListSub> friends;
};

// PACKET_CZ_PC_SELL_ITEMLIST_sub
struct CzPcSellItemlistSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t index = 0;
  std::uint16_t amount = 0;
};

// PACKET_CZ_PC_SELL_ITEMLIST
struct CzPcSellItemlist {
  static constexpr std::uint16_t kOpcode = 0x00c9;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<CzPcSellItemlistSub> sellList;
};

// PACKET_CZ_REQ_CHANGE_MEMBERPOS_sub
struct CzReqChangeMemberposSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t AID = 0;
  std::uint32_t CID = 0;
  std::int32_t position = 0;
};

// PACKET_CZ_REQ_CHANGE_MEMBERPOS
struct CzReqChangeMemberpos {
  static constexpr std::uint16_t kOpcode = 0x0155;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<CzReqChangeMemberposSub> list;
};

// PACKET_ZC_CLEAR_DIALOG
struct ZcClearDialog {
  static constexpr std::uint16_t kOpcode = 0x08d6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t GID = 0;
};

// PACKET_ZC_NOTIFY_BIND_ON_EQUIP
struct ZcNotifyBindOnEquip {
  static constexpr std::uint16_t kOpcode = 0x02d3;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t index = 0;
};

// PACKET_ZC_BANKING_CHECK
struct ZcBankingCheck {
  static constexpr std::uint16_t kOpcode = 0x09a6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::int64_t money = 0;
  std::int16_t reason = 0;
};

// PACKET_ZC_ACK_BANKING_WITHDRAW
struct ZcAckBankingWithdraw {
  static constexpr std::uint16_t kOpcode = 0x09aa;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::int16_t reason = 0;
  std::int64_t money = 0;
  std::int32_t zeny = 0;
};

// PACKET_ZC_ACK_BANKING_DEPOSIT
struct ZcAckBankingDeposit {
  static constexpr std::uint16_t kOpcode = 0x09a8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 16;

  std::int16_t reason = 0;
  std::int64_t money = 0;
  std::int32_t zeny = 0;
};

// PACKET_ZC_ACK_CLOSE_BANKING
struct ZcAckCloseBanking {
  static constexpr std::uint16_t kOpcode = 0x09b9;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t unknown = 0;
};

// PACKET_ZC_ACK_OPEN_BANKING
struct ZcAckOpenBanking {
  static constexpr std::uint16_t kOpcode = 0x09b7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t unknown = 0;
};

// PACKET_ZC_REQ_EXCHANGE_ITEM
struct ZcReqExchangeItem {
  static constexpr std::uint16_t kOpcode = 0x01f4;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 32;

  std::string requesterName;
  std::uint32_t targetId = 0;
  std::uint16_t targetLv = 0;
};

// PACKET_ZC_ACK_EXCHANGE_ITEM
struct ZcAckExchangeItem {
  static constexpr std::uint16_t kOpcode = 0x01f5;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::uint8_t result = 0;
  std::uint32_t targetId = 0;
  std::uint16_t targetLv = 0;
};

// PACKET_ZC_ACK_ADD_EXCHANGE_ITEM
struct ZcAckAddExchangeItem {
  static constexpr std::uint16_t kOpcode = 0x00ea;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::uint16_t index = 0;
  std::uint8_t result = 0;
};

// PACKET_ZC_COUPLENAME
struct ZcCouplename {
  static constexpr std::uint16_t kOpcode = 0x01e6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::string name;
};

// PACKET_CZ_PARTY_REQ_MASTER_TO_JOIN
struct CzPartyReqMasterToJoin {
  static constexpr std::uint16_t kOpcode = 0x0ae6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t CID = 0;
  std::uint32_t AID = 0;
};

// PACKET_ZC_PARTY_REQ_MASTER_TO_JOIN
struct ZcPartyReqMasterToJoin {
  static constexpr std::uint16_t kOpcode = 0x0ae7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 38;

  std::uint32_t CID = 0;
  std::uint32_t AID = 0;
  std::string name;
  std::uint16_t x = 0;
  std::uint16_t y = 0;
};

// PACKET_CZ_PARTY_REQ_ACK_MASTER_TO_JOIN
struct CzPartyReqAckMasterToJoin {
  static constexpr std::uint16_t kOpcode = 0x0af8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::uint32_t CID = 0;
  std::uint32_t AID = 0;
  std::uint8_t accept = 0;
};

// PACKET_ZC_PARTY_JOIN_REQ_ACK_FROM_MASTER
struct ZcPartyJoinReqAckFromMaster {
  static constexpr std::uint16_t kOpcode = 0x0afa;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 58;

  std::string player_name;
  std::string party_name;
  std::uint32_t AID = 0;
  std::uint32_t refused = 0;
};

// PACKET_CZ_REQ_SE_CASH_TAB_CODE
struct CzReqSeCashTabCode {
  static constexpr std::uint16_t kOpcode = 0x0846;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t tab = 0;
};

// PACKET_ZC_ACK_SE_CASH_ITEM_LIST2_sub
struct ZcAckSeCashItemList2Sub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t itemId = 0;
  std::int32_t price = 0;
};

// PACKET_ZC_ACK_SE_CASH_ITEM_LIST2
struct ZcAckSeCashItemList2 {
  static constexpr std::uint16_t kOpcode = 0x08c0;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t tab = 0;
  std::int16_t count = 0;
  std::vector<ZcAckSeCashItemList2Sub> items;
};

// PACKET_CZ_REQ_MERGE_ITEM
struct CzReqMergeItem {
  static constexpr std::uint16_t kOpcode = 0x096e;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<std::uint16_t> indices;
};

// PACKET_CZ_RESET_SKILL
struct CzResetSkill {
  static constexpr std::uint16_t kOpcode = 0x0bb1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t unknown = 0;
};

// PACKET_ZC_SKILLINFO_UPDATE
struct ZcSkillinfoUpdate {
  static constexpr std::uint16_t kOpcode = 0x010e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::uint16_t skillId = 0;
  std::uint16_t level = 0;
  std::uint16_t sp = 0;
  std::uint16_t range2 = 0;
  bool upFlag = false;
};

// PACKET_ZC_BOSS_INFO
struct ZcBossInfo {
  static constexpr std::uint16_t kOpcode = 0x0293;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 70;

  std::uint8_t type = 0;
  std::uint32_t x = 0;
  std::uint32_t y = 0;
  std::uint16_t minHours = 0;
  std::uint16_t minMinutes = 0;
  std::uint16_t maxHours = 0;
  std::uint16_t maxMinutes = 0;
  std::string name;
};

// PACKET_CZ_INVENTORY_TAB
struct CzInventoryTab {
  static constexpr std::uint16_t kOpcode = 0x0907;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::int16_t index = 0;
  bool favorite = false;
};

// PACKET_ZC_INVENTORY_TAB
struct ZcInventoryTab {
  static constexpr std::uint16_t kOpcode = 0x0908;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::int16_t index = 0;
  bool favorite = false;
};

// PACKET_CZ_REQ_OPEN_BANKING
struct CzReqOpenBanking {
  static constexpr std::uint16_t kOpcode = 0x09b6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t AID = 0;
};

// PACKET_CZ_REQ_CLOSE_BANKING
struct CzReqCloseBanking {
  static constexpr std::uint16_t kOpcode = 0x09b8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t AID = 0;
};

// PACKET_CZ_REQ_BANKING_CHECK
struct CzReqBankingCheck {
  static constexpr std::uint16_t kOpcode = 0x09ab;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t AID = 0;
};

// PACKET_CZ_REQ_BANKING_DEPOSIT
struct CzReqBankingDeposit {
  static constexpr std::uint16_t kOpcode = 0x09a7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t AID = 0;
  std::int32_t zeny = 0;
};

// PACKET_CZ_REQ_BANKING_WITHDRAW
struct CzReqBankingWithdraw {
  static constexpr std::uint16_t kOpcode = 0x09a9;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t AID = 0;
  std::int32_t zeny = 0;
};

// PACKET_ZC_ACCEPT_ENTER
struct ZcAcceptEnter {
  static constexpr std::uint16_t kOpcode = 0x02eb;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 13;

  std::uint32_t startTime = 0;
  std::array<std::uint8_t, 3> posDir = {};
  std::uint8_t xSize = 0;
  std::uint8_t ySize = 0;
  std::uint16_t font = 0;
};

// PACKET_ZC_REFUSE_ENTER
struct ZcRefuseEnter {
  static constexpr std::uint16_t kOpcode = 0x0074;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t errorCode = 0;
};

// PACKET_ZC_RESTART_ACK
struct ZcRestartAck {
  static constexpr std::uint16_t kOpcode = 0x00b3;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t type = 0;
};

// PACKET_ZC_NOTIFY_VANISH
struct ZcNotifyVanish {
  static constexpr std::uint16_t kOpcode = 0x0080;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint32_t gid = 0;
  std::uint8_t type = 0;
};

// PACKET_ZC_ITEM_DISAPPEAR
struct ZcItemDisappear {
  static constexpr std::uint16_t kOpcode = 0x00a1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t itemAid = 0;
};

// PACKET_ZC_MILLENNIUMSHIELD
struct ZcMillenniumshield {
  static constexpr std::uint16_t kOpcode = 0x0440;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t aid = 0;
  std::int16_t num = 0;
  std::int16_t state = 0;
};

// PACKET_ZC_SPIRITS_ATTRIBUTE
struct ZcSpiritsAttribute {
  static constexpr std::uint16_t kOpcode = 0x08cf;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t aid = 0;
  std::int16_t spiritsType = 0;
  std::int16_t num = 0;
};

// PACKET_ZC_CHANGESTATE_MER
struct ZcChangestateMer {
  static constexpr std::uint16_t kOpcode = 0x0230;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint8_t type = 0;
  std::uint8_t state = 0;
  std::uint32_t gid = 0;
  std::uint32_t data = 0;
};

// PACKET_ZC_HOSKILLINFO_LIST_sub
struct ZcHoskillinfoListSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 37;

  std::uint16_t id = 0;
  std::uint16_t inf = 0;
  std::uint16_t unknown = 0;
  std::uint16_t level = 0;
  std::uint16_t sp = 0;
  std::uint16_t range = 0;
  std::string name;
  std::uint8_t upgradable = 0;
};

// PACKET_ZC_HOSKILLINFO_LIST
struct ZcHoskillinfoList {
  static constexpr std::uint16_t kOpcode = 0x0235;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcHoskillinfoListSub> skills;
};

// PACKET_ZC_HOSKILLINFO_UPDATE
struct ZcHoskillinfoUpdate {
  static constexpr std::uint16_t kOpcode = 0x0239;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::uint16_t skill_id = 0;
  std::int16_t Level = 0;
  std::int16_t SP = 0;
  std::int16_t AttackRange = 0;
  bool upgradable = false;
};

// PACKET_ZC_NOTIFY_PLAYERMOVE
struct ZcNotifyPlayermove {
  static constexpr std::uint16_t kOpcode = 0x0087;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t moveStartTime = 0;
  std::array<std::uint8_t, 6> moveData = {};
};

// PACKET_ZC_CHANGE_DIRECTION
struct ZcChangeDirection {
  static constexpr std::uint16_t kOpcode = 0x009c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::uint32_t srcId = 0;
  std::uint16_t headDir = 0;
  std::uint8_t dir = 0;
};

// PACKET_ZC_NPCACK_MAPMOVE
struct ZcNpcackMapmove {
  static constexpr std::uint16_t kOpcode = 0x0091;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 22;

  std::string mapName;
  std::uint16_t xPos = 0;
  std::uint16_t yPos = 0;
};

// PACKET_ZC_NPCACK_SERVERMOVE
struct ZcNpcackServermove {
  static constexpr std::uint16_t kOpcode = 0x0ac7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 156;

  std::string mapName;
  std::uint16_t xPos = 0;
  std::uint16_t yPos = 0;
  std::uint32_t ip = 0;
  std::uint16_t port = 0;
  std::string domain;
};

// PACKET_ZC_STOPMOVE
struct ZcStopmove {
  static constexpr std::uint16_t kOpcode = 0x0088;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t AID = 0;
  std::uint16_t xPos = 0;
  std::uint16_t yPos = 0;
};

// PACKET_ZC_SELECT_DEALTYPE
struct ZcSelectDealtype {
  static constexpr std::uint16_t kOpcode = 0x00c4;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t npcId = 0;
};

// PACKET_ZC_PC_SELL_ITEMLIST_sub
struct ZcPcSellItemlistSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint16_t index = 0;
  std::uint32_t price = 0;
  std::uint32_t overcharge = 0;
};

// PACKET_ZC_PC_SELL_ITEMLIST
struct ZcPcSellItemlist {
  static constexpr std::uint16_t kOpcode = 0x00c7;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcPcSellItemlistSub> items;
};

// PACKET_ZC_CLOSE_DIALOG
struct ZcCloseDialog {
  static constexpr std::uint16_t kOpcode = 0x00b6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t npcId = 0;
};

// PACKET_ZC_MENU_LIST
struct ZcMenuList {
  static constexpr std::uint16_t kOpcode = 0x00b7;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t npcId = 0;
  std::string menu;
};

// PACKET_ZC_OPEN_EDITDLG
struct ZcOpenEditdlg {
  static constexpr std::uint16_t kOpcode = 0x0142;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t npcId = 0;
};

// PACKET_ZC_OPEN_EDITDLGSTR
struct ZcOpenEditdlgstr {
  static constexpr std::uint16_t kOpcode = 0x01d4;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t npcId = 0;
};

// PACKET_ZC_ACK_REQ_ALLY_GUILD
struct ZcAckReqAllyGuild {
  static constexpr std::uint16_t kOpcode = 0x0173;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t flag = 0;
};

// PACKET_ZC_DELETE_RELATED_GUILD
struct ZcDeleteRelatedGuild {
  static constexpr std::uint16_t kOpcode = 0x0184;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t allyID = 0;
  std::uint32_t flag = 0;
};

// PACKET_ZC_REQ_ALLY_GUILD
struct ZcReqAllyGuild {
  static constexpr std::uint16_t kOpcode = 0x0171;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 30;

  std::uint32_t inviterId = 0;
  std::string inviterGuildName;
};

// PACKET_ZC_ACK_REQ_HOSTILE_GUILD
struct ZcAckReqHostileGuild {
  static constexpr std::uint16_t kOpcode = 0x0181;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t flag = 0;
};

// PACKET_ZC_COMPASS
struct ZcCompass {
  static constexpr std::uint16_t kOpcode = 0x0144;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 23;

  std::uint32_t npcId = 0;
  std::uint32_t type = 0;
  std::uint32_t xPos = 0;
  std::uint32_t yPos = 0;
  std::uint8_t id = 0;
  std::uint32_t color = 0;
};

// PACKET_ZC_ITEM_THROW_ACK
struct ZcItemThrowAck {
  static constexpr std::uint16_t kOpcode = 0x00af;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t index = 0;
  std::uint16_t count = 0;
};

// PACKET_ZC_DELETE_ITEM_FROM_BODY
struct ZcDeleteItemFromBody {
  static constexpr std::uint16_t kOpcode = 0x07fa;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::int16_t deleteType = 0;
  std::uint16_t index = 0;
  std::int16_t count = 0;
};

// PACKET_ZC_CARTOFF
struct ZcCartoff {
  static constexpr std::uint16_t kOpcode = 0x012b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_ACK_GUILD_MENUINTERFACE
struct ZcAckGuildMenuinterface {
  static constexpr std::uint16_t kOpcode = 0x014e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t menuFlag = 0;
};

// PACKET_ZC_ACK_CHANGE_GUILD_POSITIONINFO_sub
struct ZcAckChangeGuildPositioninfoSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 40;

  std::int32_t positionID = 0;
  std::int32_t mode = 0;
  std::int32_t ranking = 0;
  std::int32_t payRate = 0;
  std::string posName;
};

// PACKET_ZC_ACK_CHANGE_GUILD_POSITIONINFO
struct ZcAckChangeGuildPositioninfo {
  static constexpr std::uint16_t kOpcode = 0x0174;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcAckChangeGuildPositioninfoSub> posInfo;
};

// PACKET_ZC_NOTIFY_POSITION_TO_GUILDM
struct ZcNotifyPositionToGuildm {
  static constexpr std::uint16_t kOpcode = 0x01eb;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t aid = 0;
  std::int16_t xPos = 0;
  std::int16_t yPos = 0;
};

// PACKET_ZC_GUILD_CHAT
struct ZcGuildChat {
  static constexpr std::uint16_t kOpcode = 0x017f;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::string message;
};

// PACKET_ZC_UPDATE_CHARSTAT
struct ZcUpdateCharstat {
  static constexpr std::uint16_t kOpcode = 0x01f2;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 20;

  std::uint32_t aid = 0;
  std::uint32_t cid = 0;
  std::uint32_t status = 0;
  std::uint16_t gender = 0;
  std::uint16_t hairStyle = 0;
  std::uint16_t hairColor = 0;
};

// PACKET_ZC_ACK_REQ_CHANGE_MEMBERS_sub
struct ZcAckReqChangeMembersSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t accId = 0;
  std::uint32_t charId = 0;
  std::int32_t positionID = 0;
};

// PACKET_ZC_ACK_REQ_CHANGE_MEMBERS
struct ZcAckReqChangeMembers {
  static constexpr std::uint16_t kOpcode = 0x0156;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcAckReqChangeMembersSub> members;
};

// PACKET_ZC_STATUS
struct ZcStatus {
  static constexpr std::uint16_t kOpcode = 0x00bd;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 44;

  std::uint16_t point = 0;
  std::uint8_t str = 0;
  std::uint8_t standardStr = 0;
  std::uint8_t agi = 0;
  std::uint8_t standardAgi = 0;
  std::uint8_t vit = 0;
  std::uint8_t standardVit = 0;
  std::uint8_t int_ = 0;
  std::uint8_t standardInt = 0;
  std::uint8_t dex = 0;
  std::uint8_t standardDex = 0;
  std::uint8_t luk = 0;
  std::uint8_t standardLuk = 0;
  std::int16_t attPower = 0;
  std::int16_t refiningPower = 0;
  std::int16_t max_mattPower = 0;
  std::int16_t min_mattPower = 0;
  std::int16_t itemdefPower = 0;
  std::int16_t plusdefPower = 0;
  std::int16_t mdefPower = 0;
  std::int16_t plusmdefPower = 0;
  std::int16_t hitSuccessValue = 0;
  std::int16_t avoidSuccessValue = 0;
  std::int16_t plusAvoidSuccessValue = 0;
  std::int16_t criticalSuccessValue = 0;
  std::int16_t ASPD = 0;
  std::int16_t plusASPD = 0;
};

// PACKET_ZC_NOTIFY_MAPINFO
struct ZcNotifyMapinfo {
  static constexpr std::uint16_t kOpcode = 0x0189;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t type = 0;
};

// PACKET_ZC_ACK_REMEMBER_WARPPOINT
struct ZcAckRememberWarppoint {
  static constexpr std::uint16_t kOpcode = 0x011e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t type = 0;
};

// PACKET_ZC_DISPEL
struct ZcDispel {
  static constexpr std::uint16_t kOpcode = 0x01b9;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t gid = 0;
};

// PACKET_ZC_RESURRECTION
struct ZcResurrection {
  static constexpr std::uint16_t kOpcode = 0x0148;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t gid = 0;
  std::int16_t type = 0;
};

// PACKET_ZC_NOTIFY_MAPPROPERTY2
struct ZcNotifyMapproperty2 {
  static constexpr std::uint16_t kOpcode = 0x01d6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t type = 0;
};

// PACKET_ZC_ACK_ITEMREFINING
struct ZcAckItemrefining {
  static constexpr std::uint16_t kOpcode = 0x0188;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t result = 0;
  std::uint16_t index = 0;
  std::uint16_t value = 0;
};

// PACKET_ZC_PAR_CHANGE_USER
struct ZcParChangeUser {
  static constexpr std::uint16_t kOpcode = 0x01ab;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t gid = 0;
  std::int16_t type = 0;
  std::uint32_t value = 0;
};

// PACKET_ZC_CHANGE_CHATROOM
struct ZcChangeChatroom {
  static constexpr std::uint16_t kOpcode = 0x00df;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 17;

  std::uint32_t ownerId = 0;
  std::uint32_t chatId = 0;
  std::uint16_t limit = 0;
  std::uint16_t users = 0;
  std::uint8_t flag = 0;
  std::string title;
};

// PACKET_ZC_EQUIP_ARROW
struct ZcEquipArrow {
  static constexpr std::uint16_t kOpcode = 0x013c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t index = 0;
};

// PACKET_ZC_CLOSE_STORE
struct ZcCloseStore {
  static constexpr std::uint16_t kOpcode = 0x00f8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_DELETE_ITEM_FROM_STORE
struct ZcDeleteItemFromStore {
  static constexpr std::uint16_t kOpcode = 0x00f6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t index = 0;
  std::uint32_t amount = 0;
};

// PACKET_ZC_NOTIFY_STOREITEM_COUNTINFO
struct ZcNotifyStoreitemCountinfo {
  static constexpr std::uint16_t kOpcode = 0x00f2;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t amount = 0;
  std::uint16_t max_amount = 0;
};

// PACKET_ZC_EXCHANGEITEM_UNDO
struct ZcExchangeitemUndo {
  static constexpr std::uint16_t kOpcode = 0x00f1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_EXEC_EXCHANGE_ITEM
struct ZcExecExchangeItem {
  static constexpr std::uint16_t kOpcode = 0x00f0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t result = 0;
};

// PACKET_ZC_CANCEL_EXCHANGE_ITEM
struct ZcCancelExchangeItem {
  static constexpr std::uint16_t kOpcode = 0x00ee;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_CONCLUDE_EXCHANGE_ITEM
struct ZcConcludeExchangeItem {
  static constexpr std::uint16_t kOpcode = 0x00ec;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t who = 0;
};

// PACKET_ZC_ACK_CREATE_CHATROOM
struct ZcAckCreateChatroom {
  static constexpr std::uint16_t kOpcode = 0x00d6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t flag = 0;
};

// PACKET_ZC_REFUSE_ENTER_ROOM
struct ZcRefuseEnterRoom {
  static constexpr std::uint16_t kOpcode = 0x00da;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t result = 0;
};

// PACKET_ZC_DESTROY_ROOM
struct ZcDestroyRoom {
  static constexpr std::uint16_t kOpcode = 0x00d8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t chatId = 0;
};

// PACKET_ZC_ENTER_ROOM_sub
struct ZcEnterRoomSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 28;

  std::uint32_t flag = 0;
  std::string name;
};

// PACKET_ZC_ENTER_ROOM
struct ZcEnterRoom {
  static constexpr std::uint16_t kOpcode = 0x00db;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::uint32_t chatId = 0;
  std::vector<ZcEnterRoomSub> members;
};

// PACKET_ZC_NPC_SHOWEFST_UPDATE
struct ZcNpcShowefstUpdate {
  static constexpr std::uint16_t kOpcode = 0x028a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 18;

  std::uint32_t gid = 0;
  std::uint32_t effectState = 0;
  std::int32_t level = 0;
  std::uint32_t showEFST = 0;
};

// PACKET_ZC_ACTION_FAILURE
struct ZcActionFailure {
  static constexpr std::uint16_t kOpcode = 0x013b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t type = 0;
};

// PACKET_ZC_NOTIFY_EFFECT
struct ZcNotifyEffect {
  static constexpr std::uint16_t kOpcode = 0x019b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t aid = 0;
  std::uint32_t effectId = 0;
};

// PACKET_ZC_ACK_ITEMCOMPOSITION
struct ZcAckItemcomposition {
  static constexpr std::uint16_t kOpcode = 0x017d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint16_t equipIndex = 0;
  std::uint16_t cardIndex = 0;
  std::uint8_t result = 0;
};

// PACKET_ZC_ACK_ITEMIDENTIFY
struct ZcAckItemidentify {
  static constexpr std::uint16_t kOpcode = 0x0179;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::uint16_t index = 0;
  std::uint8_t result = 0;
};

// PACKET_ZC_ACK_ITEMREPAIR
struct ZcAckItemrepair {
  static constexpr std::uint16_t kOpcode = 0x01fe;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 5;

  std::uint16_t index = 0;
  std::uint8_t result = 0;
};

// PACKET_ZC_EQUIPITEM_DAMAGED
struct ZcEquipitemDamaged {
  static constexpr std::uint16_t kOpcode = 0x02bb;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t equipLocation = 0;
  std::uint32_t GID = 0;
};

// PACKET_ZC_DELETE_ITEM_FROM_CART
struct ZcDeleteItemFromCart {
  static constexpr std::uint16_t kOpcode = 0x0125;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t index = 0;
  std::int32_t amount = 0;
};

// PACKET_ZC_OPENSTORE
struct ZcOpenstore {
  static constexpr std::uint16_t kOpcode = 0x012d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t num = 0;
};

// PACKET_ZC_PC_PURCHASE_RESULT_FROMMC
struct ZcPcPurchaseResultFrommc {
  static constexpr std::uint16_t kOpcode = 0x0135;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint16_t index = 0;
  std::uint16_t amount = 0;
  std::uint8_t result = 0;
};

// PACKET_ZC_ACK_OPENSTORE2
struct ZcAckOpenstore2 {
  static constexpr std::uint16_t kOpcode = 0x0a28;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t result = 0;
};

// PACKET_ZC_SKILL_DISAPPEAR
struct ZcSkillDisappear {
  static constexpr std::uint16_t kOpcode = 0x0120;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t GID = 0;
};

// PACKET_ZC_SKILLINFO_DELETE
struct ZcSkillinfoDelete {
  static constexpr std::uint16_t kOpcode = 0x0441;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t skillID = 0;
};

// PACKET_ZC_SKILL_UPDATE
struct ZcSkillUpdate {
  static constexpr std::uint16_t kOpcode = 0x01ac;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t GID = 0;
};

// PACKET_ZC_HIGHJUMP
struct ZcHighjump {
  static constexpr std::uint16_t kOpcode = 0x01ff;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t srcId = 0;
  std::uint16_t x = 0;
  std::uint16_t y = 0;
};

// PACKET_ZC_RECOVERY
struct ZcRecovery {
  static constexpr std::uint16_t kOpcode = 0x0a27;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t type = 0;
  std::int32_t amount = 0;
};

// PACKET_ZC_ACK_WHISPER
struct ZcAckWhisper {
  static constexpr std::uint16_t kOpcode = 0x09df;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint8_t result = 0;
  std::uint32_t CID = 0;
};

// PACKET_ZC_ACK_ADDITEM_TO_CART
struct ZcAckAdditemToCart {
  static constexpr std::uint16_t kOpcode = 0x012c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t result = 0;
};

// PACKET_ZC_DELETEITEM_FROM_MCSTORE
struct ZcDeleteitemFromMcstore {
  static constexpr std::uint16_t kOpcode = 0x09e5;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 18;

  std::uint16_t index = 0;
  std::uint16_t amount = 0;
  std::uint32_t buyerCID = 0;
  std::uint32_t date = 0;
  std::int32_t zeny = 0;
};

// PACKET_CZ_REQ_BAN_GUILD
struct CzReqBanGuild {
  static constexpr std::uint16_t kOpcode = 0x015b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 54;

  std::uint32_t guild_id = 0;
  std::uint32_t AID = 0;
  std::uint32_t CID = 0;
  std::string message;
};

// PACKET_CZ_REQ_LEAVE_GUILD
struct CzReqLeaveGuild {
  static constexpr std::uint16_t kOpcode = 0x0159;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 54;

  std::uint32_t guild_id = 0;
  std::uint32_t AID = 0;
  std::uint32_t CID = 0;
  std::string message;
};

// PACKET_CZ_REQ_DISORGANIZE_GUILD
struct CzReqDisorganizeGuild {
  static constexpr std::uint16_t kOpcode = 0x015d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 42;

  std::string key;
};

// PACKET_ZC_ACK_DISORGANIZE_GUILD_RESULT
struct ZcAckDisorganizeGuildResult {
  static constexpr std::uint16_t kOpcode = 0x015e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t result = 0;
};

// PACKET_ZC_RESULT_MAKE_GUILD
struct ZcResultMakeGuild {
  static constexpr std::uint16_t kOpcode = 0x0167;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t result = 0;
};

// PACKET_CZ_REQ_JOIN_GUILD
struct CzReqJoinGuild {
  static constexpr std::uint16_t kOpcode = 0x0168;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t AID = 0;
  std::uint32_t inviter_AID = 0;
  std::uint32_t inviter_CID = 0;
};

// PACKET_ZC_ACK_REQ_JOIN_GUILD
struct ZcAckReqJoinGuild {
  static constexpr std::uint16_t kOpcode = 0x0169;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t result = 0;
};

// PACKET_ZC_REQ_JOIN_GUILD
struct ZcReqJoinGuild {
  static constexpr std::uint16_t kOpcode = 0x016a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 30;

  std::uint32_t guild_id = 0;
  std::string guild_name;
};

// PACKET_CZ_JOIN_GUILD
struct CzJoinGuild {
  static constexpr std::uint16_t kOpcode = 0x016b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t guild_id = 0;
  std::int32_t answer = 0;
};

// PACKET_ZC_GUILD_NOTICE
struct ZcGuildNotice {
  static constexpr std::uint16_t kOpcode = 0x016f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 182;

  std::string subject;
  std::string notice;
};

// PACKET_CZ_REQ_JOIN_GUILD2
struct CzReqJoinGuild2 {
  static constexpr std::uint16_t kOpcode = 0x0916;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::string name;
};

// PACKET_CZ_REQ_JOIN_GROUP
struct CzReqJoinGroup {
  static constexpr std::uint16_t kOpcode = 0x00fc;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t AID = 0;
};

// PACKET_CZ_JOIN_GROUP
struct CzJoinGroup {
  static constexpr std::uint16_t kOpcode = 0x00ff;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t party_id = 0;
  std::int32_t flag = 0;
};

// PACKET_CZ_REQ_LEAVE_GROUP
struct CzReqLeaveGroup {
  static constexpr std::uint16_t kOpcode = 0x0100;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_REQ_EXPEL_GROUP_MEMBER
struct CzReqExpelGroupMember {
  static constexpr std::uint16_t kOpcode = 0x0103;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 30;

  std::uint32_t AID = 0;
  std::string name;
};

// PACKET_CZ_PARTY_JOIN_REQ
struct CzPartyJoinReq {
  static constexpr std::uint16_t kOpcode = 0x02c4;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::string name;
};

// PACKET_CZ_PARTY_JOIN_REQ_ACK
struct CzPartyJoinReqAck {
  static constexpr std::uint16_t kOpcode = 0x02c7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint32_t party_id = 0;
  std::uint8_t flag = 0;
};

// PACKET_ZC_EL_PAR_CHANGE
struct ZcElParChange {
  static constexpr std::uint16_t kOpcode = 0x081e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t type = 0;
  std::uint32_t value = 0;
};

// PACKET_CZ_REQ_EMOTION
struct CzReqEmotion {
  static constexpr std::uint16_t kOpcode = 0x00bf;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t emotion_type = 0;
};

// PACKET_ZC_NOTIFY_ACT
struct ZcNotifyAct {
  static constexpr std::uint16_t kOpcode = 0x08c8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 34;

  std::int32_t srcID = 0;
  std::int32_t targetID = 0;
  std::int32_t serverTick = 0;
  std::int32_t srcSpeed = 0;
  std::int32_t dmgSpeed = 0;
  std::int32_t damage = 0;
  std::int8_t isSPDamage = 0;
  std::uint16_t div = 0;
  std::uint8_t type = 0;
  std::int32_t damage2 = 0;
};

// PACKET_CZ_REQUEST_MOVENPC
struct CzRequestMovenpc {
  static constexpr std::uint16_t kOpcode = 0x0232;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::uint32_t GID = 0;
  std::array<std::uint8_t, 3> PosDir = {};
};

// PACKET_ZC_NPCSPRITE_CHANGE
struct ZcNpcspriteChange {
  static constexpr std::uint16_t kOpcode = 0x01b0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 11;

  std::uint32_t GID = 0;
  std::uint8_t type = 0;
  std::uint32_t class_ = 0;
};

// PACKET_ZC_MEMBER_NEWENTRY
struct ZcMemberNewentry {
  static constexpr std::uint16_t kOpcode = 0x00dc;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 28;

  std::uint16_t count = 0;
  std::string name;
};

// PACKET_ZC_MEMBER_EXIT
struct ZcMemberExit {
  static constexpr std::uint16_t kOpcode = 0x00dd;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 29;

  std::uint16_t count = 0;
  std::string name;
  std::uint8_t kicked = 0;
};

// PACKET_CZ_MOVETO_MAP
struct CzMovetoMap {
  static constexpr std::uint16_t kOpcode = 0x0140;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 22;

  std::string map;
  std::uint16_t x = 0;
  std::uint16_t y = 0;
};

// PACKET_CZ_BROADCAST
struct CzBroadcast {
  static constexpr std::uint16_t kOpcode = 0x0099;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::string message;
};

// PACKET_CZ_REQ_WEAR_EQUIP
struct CzReqWearEquip {
  static constexpr std::uint16_t kOpcode = 0x0998;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t index = 0;
  std::uint32_t position = 0;
};

// PACKET_CZ_ACK_SELECT_DEALTYPE
struct CzAckSelectDealtype {
  static constexpr std::uint16_t kOpcode = 0x00c5;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint32_t GID = 0;
  std::uint8_t type = 0;
};

// PACKET_CZ_CREATE_CHATROOM
struct CzCreateChatroom {
  static constexpr std::uint16_t kOpcode = 0x00d5;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 15;

  std::uint16_t limit = 0;
  std::uint8_t type = 0;
  std::string password;
  std::string title;
};

// PACKET_CZ_BLACKSMITH_RANK
struct CzBlacksmithRank {
  static constexpr std::uint16_t kOpcode = 0x0217;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_ALCHEMIST_RANK
struct CzAlchemistRank {
  static constexpr std::uint16_t kOpcode = 0x0218;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_BLACKSMITH_POINT
struct ZcBlacksmithPoint {
  static constexpr std::uint16_t kOpcode = 0x021b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t points = 0;
  std::uint32_t points_total = 0;
};

// PACKET_ZC_ALCHEMIST_POINT
struct ZcAlchemistPoint {
  static constexpr std::uint16_t kOpcode = 0x021c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t points = 0;
  std::uint32_t points_total = 0;
};

// PACKET_ZC_TAEKWON_POINT
struct ZcTaekwonPoint {
  static constexpr std::uint16_t kOpcode = 0x0224;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t points = 0;
  std::uint32_t points_total = 0;
};

// PACKET_CZ_TAEKWON_RANK
struct CzTaekwonRank {
  static constexpr std::uint16_t kOpcode = 0x0225;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_KILLER_POINT
struct ZcKillerPoint {
  static constexpr std::uint16_t kOpcode = 0x0236;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t points = 0;
  std::uint32_t points_total = 0;
};

// PACKET_CZ_KILLER_RANK
struct CzKillerRank {
  static constexpr std::uint16_t kOpcode = 0x0237;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CZ_REQ_RANKING
struct CzReqRanking {
  static constexpr std::uint16_t kOpcode = 0x097c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t type = 0;
};

// PACKET_ZC_UPDATE_RANKING_POINT
struct ZcUpdateRankingPoint {
  static constexpr std::uint16_t kOpcode = 0x097e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint16_t type = 0;
  std::uint32_t points = 0;
  std::uint32_t points_total = 0;
};

// PACKET_ZC_ACK_RANKING2
struct ZcAckRanking2 {
  static constexpr std::uint16_t kOpcode = 0x0af6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 88;

  std::uint16_t type = 0;
  std::array<std::uint32_t, 10> CIDs = {};
  std::array<std::uint32_t, 10> points = {};
  std::uint32_t mypoints = 0;
};

// PACKET_CZ_LESSEFFECT
struct CzLesseffect {
  static constexpr std::uint16_t kOpcode = 0x021d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t state = 0;
};

// PACKET_CZ_ACTIVE_QUEST
struct CzActiveQuest {
  static constexpr std::uint16_t kOpcode = 0x02b6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint32_t quest_id = 0;
  std::uint8_t active = 0;
};

// PACKET_CZ_JOIN_BABY
struct CzJoinBaby {
  static constexpr std::uint16_t kOpcode = 0x01f7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t father_AID = 0;
  std::uint32_t mother_AID = 0;
  std::uint32_t accepted = 0;
};

// PACKET_CZ_AUCTION_ITEM_SEARCH
struct CzAuctionItemSearch {
  static constexpr std::uint16_t kOpcode = 0x0251;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 34;

  std::uint16_t type = 0;
  std::uint32_t auction_id = 0;
  std::string text;
  std::uint16_t page = 0;
};

// PACKET_CZ_AUCTION_BUY
struct CzAuctionBuy {
  static constexpr std::uint16_t kOpcode = 0x024f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t auction_id = 0;
  std::uint32_t money = 0;
};

// PACKET_CZ_AUCTION_ADD
struct CzAuctionAdd {
  static constexpr std::uint16_t kOpcode = 0x024d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t now_money = 0;
  std::uint32_t max_money = 0;
  std::uint16_t hours = 0;
};

// PACKET_ZC_DRESSROOM_OPEN
struct ZcDressroomOpen {
  static constexpr std::uint16_t kOpcode = 0x0a02;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t view = 0;
};

// PACKET_ZC_ROOM_NEWENTRY
struct ZcRoomNewentry {
  static constexpr std::uint16_t kOpcode = 0x00d7;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 17;

  std::int32_t owner = 0;
  std::int32_t id = 0;
  std::uint16_t limit = 0;
  std::uint16_t users = 0;
  std::uint8_t type = 0;
  std::string title;
};

// PACKET_ZC_MONSTER_INFO
struct ZcMonsterInfo {
  static constexpr std::uint16_t kOpcode = 0x018c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 29;

  std::uint16_t class_ = 0;
  std::uint16_t level = 0;
  std::uint16_t size = 0;
  std::uint32_t hp = 0;
  std::int16_t def = 0;
  std::uint16_t race = 0;
  std::int16_t mdef = 0;
  std::uint16_t element = 0;
  std::uint8_t water = 0;
  std::uint8_t earth = 0;
  std::uint8_t fire = 0;
  std::uint8_t wind = 0;
  std::uint8_t poison = 0;
  std::uint8_t holy = 0;
  std::uint8_t shadow = 0;
  std::uint8_t ghost = 0;
  std::uint8_t undead = 0;
};

// PACKET_ZC_ACK_REQNAME_BYGID
struct ZcAckReqnameBygid {
  static constexpr std::uint16_t kOpcode = 0x0af7;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 32;

  std::uint16_t flag = 0;
  std::uint32_t CID = 0;
  std::string name;
};

// PACKET_ZC_PET_ACT
struct ZcPetAct {
  static constexpr std::uint16_t kOpcode = 0x01aa;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t GID = 0;
  std::int32_t data = 0;
};

// PACKET_ZC_COMBODELAY
struct ZcCombodelay {
  static constexpr std::uint16_t kOpcode = 0x01d2;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t AID = 0;
  std::uint32_t delay = 0;
};

// PACKET_ZC_BLADESTOP
struct ZcBladestop {
  static constexpr std::uint16_t kOpcode = 0x01d1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t srcId = 0;
  std::uint32_t targetId = 0;
  std::uint32_t flag = 0;
};

// PACKET_ZC_MVP
struct ZcMvp {
  static constexpr std::uint16_t kOpcode = 0x010c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t AID = 0;
};

// PACKET_ZC_MVP_GETTING_SPECIAL_EXP
struct ZcMvpGettingSpecialExp {
  static constexpr std::uint16_t kOpcode = 0x010b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t exp = 0;
};

// PACKET_ZC_THROW_MVPITEM
struct ZcThrowMvpitem {
  static constexpr std::uint16_t kOpcode = 0x010d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_ZC_UPDATE_MAPINFO
struct ZcUpdateMapinfo {
  static constexpr std::uint16_t kOpcode = 0x0192;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 24;

  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t type = 0;
  std::string mapname;
};

// PACKET_CZ_REQ_ENTER_ROOM
struct CzReqEnterRoom {
  static constexpr std::uint16_t kOpcode = 0x00d9;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t chat_id = 0;
  std::string password;
};

// PACKET_CZ_CHANGE_CHATROOM
struct CzChangeChatroom {
  static constexpr std::uint16_t kOpcode = 0x00de;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 15;

  std::uint16_t limit = 0;
  std::uint8_t type = 0;
  std::string password;
  std::string title;
};

// PACKET_CZ_ADD_EXCHANGE_ITEM
struct CzAddExchangeItem {
  static constexpr std::uint16_t kOpcode = 0x00e8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t index = 0;
  std::int32_t amount = 0;
};

// PACKET_CZ_MOVE_ITEM_FROM_CART_TO_BODY
struct CzMoveItemFromCartToBody {
  static constexpr std::uint16_t kOpcode = 0x0127;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t index = 0;
  std::int32_t amount = 0;
};

// PACKET_CZ_SELECT_WARPPOINT
struct CzSelectWarppoint {
  static constexpr std::uint16_t kOpcode = 0x011b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 20;

  std::int16_t skill_id = 0;
  std::string mapname;
};

// PACKET_CZ_INPUT_EDITDLG
struct CzInputEditdlg {
  static constexpr std::uint16_t kOpcode = 0x0143;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t GID = 0;
  std::int32_t value = 0;
};

// PACKET_CZ_INPUT_EDITDLGSTR
struct CzInputEditdlgstr {
  static constexpr std::uint16_t kOpcode = 0x01d5;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 8;

  std::int32_t GID = 0;
  std::string value;
};

// PACKET_CZ_CLOSE_DIALOG
struct CzCloseDialog {
  static constexpr std::uint16_t kOpcode = 0x0146;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t GID = 0;
};

// PACKET_CZ_RESET
struct CzReset {
  static constexpr std::uint16_t kOpcode = 0x0197;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t type = 0;
};

// PACKET_CZ_MOVE_ITEM_FROM_STORE_TO_CART
struct CzMoveItemFromStoreToCart {
  static constexpr std::uint16_t kOpcode = 0x0128;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t index = 0;
  std::int32_t amount = 0;
};

// PACKET_CZ_REQ_ITEMIDENTIFY
struct CzReqItemidentify {
  static constexpr std::uint16_t kOpcode = 0x0178;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t index = 0;
};

// PACKET_CZ_SELECTAUTOSPELL
struct CzSelectautospell {
  static constexpr std::uint16_t kOpcode = 0x01ce;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t skill_id = 0;
};

// PACKET_CZ_REQ_ITEMCOMPOSITION_LIST
struct CzReqItemcompositionList {
  static constexpr std::uint16_t kOpcode = 0x017a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t index = 0;
};

// PACKET_CZ_REQ_ITEMCOMPOSITION
struct CzReqItemcomposition {
  static constexpr std::uint16_t kOpcode = 0x017c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t index_card = 0;
  std::uint16_t index_equip = 0;
};

// PACKET_CZ_LOCALBROADCAST
struct CzLocalbroadcast {
  static constexpr std::uint16_t kOpcode = 0x019c;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::string message;
};

// PACKET_CZ_MOVE_ITEM_FROM_CART_TO_STORE
struct CzMoveItemFromCartToStore {
  static constexpr std::uint16_t kOpcode = 0x0129;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t index = 0;
  std::int32_t amount = 0;
};

// PACKET_ZC_FRIENDS_STATE
struct ZcFriendsState {
  static constexpr std::uint16_t kOpcode = 0x0206;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 35;

  std::uint32_t AID = 0;
  std::uint32_t CID = 0;
  std::uint8_t offline = 0;
  std::string name;
};

// PACKET_CZ_MAKE_GROUP
struct CzMakeGroup {
  static constexpr std::uint16_t kOpcode = 0x00f9;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::string name;
};

// PACKET_CZ_MAKE_GROUP2
struct CzMakeGroup2 {
  static constexpr std::uint16_t kOpcode = 0x01e8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 28;

  std::string name;
  std::uint8_t item_pickup = 0;
  std::uint8_t item_share = 0;
};

// PACKET_CZ_GM_CHECKER
struct CzGmChecker {
  static constexpr std::uint16_t kOpcode = 0x0c0b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 18;

  std::string mapname;
};

// PACKET_ZC_GM_CHECKER
struct ZcGmChecker {
  static constexpr std::uint16_t kOpcode = 0x0c0c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int16_t result = 0;
};

// PACKET_ZC_CONFIG_NOTIFY
struct ZcConfigNotify {
  static constexpr std::uint16_t kOpcode = 0x02da;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t flag = 0;
};

// PACKET_ZC_CONGRATULATION
struct ZcCongratulation {
  static constexpr std::uint16_t kOpcode = 0x01ea;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t GID = 0;
};

// PACKET_ZC_DIVORCE
struct ZcDivorce {
  static constexpr std::uint16_t kOpcode = 0x0205;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::string name;
};

// PACKET_ZC_EMOTION
struct ZcEmotion {
  static constexpr std::uint16_t kOpcode = 0x00c0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::int32_t GID = 0;
  std::uint8_t type = 0;
};

// PACKET_ZC_MSG
struct ZcMsg {
  static constexpr std::uint16_t kOpcode = 0x0291;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t msgId = 0;
};

// PACKET_ZC_MSG_SKILL
struct ZcMsgSkill {
  static constexpr std::uint16_t kOpcode = 0x07e6;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t skillId = 0;
  std::int32_t msgId = 0;
};

// PACKET_ZC_MSG_VALUE
struct ZcMsgValue {
  static constexpr std::uint16_t kOpcode = 0x07e2;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t message = 0;
  std::int32_t value = 0;
};

// PACKET_ZC_NOTIFY_MANNER_POINT_GIVEN
struct ZcNotifyMannerPointGiven {
  static constexpr std::uint16_t kOpcode = 0x014b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 27;

  std::uint8_t type = 0;
  std::string name;
};

// PACKET_ZC_NOTIFY_TIME
struct ZcNotifyTime {
  static constexpr std::uint16_t kOpcode = 0x007f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t time = 0;
};

// PACKET_ZC_SETTING_WHISPER_PC
struct ZcSettingWhisperPc {
  static constexpr std::uint16_t kOpcode = 0x00d1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint8_t type = 0;
  std::uint8_t result = 0;
};

// PACKET_ZC_SETTING_WHISPER_STATE
struct ZcSettingWhisperState {
  static constexpr std::uint16_t kOpcode = 0x00d2;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint8_t type = 0;
  std::uint8_t result = 0;
};

// PACKET_ZC_SKILLMSG
struct ZcSkillmsg {
  static constexpr std::uint16_t kOpcode = 0x0215;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t msgId = 0;
};

// PACKET_CZ_REQ_EMOTION_EXPANSION
struct CzReqEmotionExpansion {
  static constexpr std::uint16_t kOpcode = 0x0be9;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::array<std::uint8_t, 4> unknown = {};
};

// PACKET_CZ_REQ_BUY_CASH_EMOTION
struct CzReqBuyCashEmotion {
  static constexpr std::uint16_t kOpcode = 0x0bec;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 7;

  std::uint16_t pack_id = 0;
  std::array<std::uint8_t, 3> unknown = {};
};

// PACKET_ZC_ACK_BUY_CASH_EMOTION
struct ZcAckBuyCashEmotion {
  static constexpr std::uint16_t kOpcode = 0x0bed;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 9;

  std::uint16_t pack_id = 0;
  std::uint8_t has_count = 0;
  std::int32_t count = 0;
};

// PACKET_ZC_PLAY_CASH_EMOTION
struct ZcPlayCashEmotion {
  static constexpr std::uint16_t kOpcode = 0x0bea;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t GID = 0;
  std::uint16_t pack_id = 0;
  std::uint16_t emotion_id = 0;
};

// PACKET_ZC_DISAPPEAR_ENTRY
struct ZcDisappearEntry {
  static constexpr std::uint16_t kOpcode = 0x0132;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::int32_t GID = 0;
};

// PACKET_ZC_EFST_SET_ENTER
struct ZcEfstSetEnter {
  static constexpr std::uint16_t kOpcode = 0x0984;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 28;

  std::uint32_t targetID = 0;
  std::uint16_t type = 0;
  std::uint32_t duration = 0;
  std::uint32_t duration2 = 0;
  std::int32_t val1 = 0;
  std::int32_t val2 = 0;
  std::int32_t val3 = 0;
};

// PACKET_CZ_SETTING_WHISPER_STATE
struct CzSettingWhisperState {
  static constexpr std::uint16_t kOpcode = 0x00d0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::int8_t type = 0;
};

// PACKET_ZC_WHISPER_LIST_sub
struct ZcWhisperListSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 24;

  std::string name;
};

// PACKET_ZC_WHISPER_LIST
struct ZcWhisperList {
  static constexpr std::uint16_t kOpcode = 0x00d4;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<ZcWhisperListSub> names;
};

// PACKET_CZ_ALLY_CHAT
struct CzAllyChat {
  static constexpr std::uint16_t kOpcode = 0x0bdd;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;
};

// PACKET_CZ_REQ_REPORT_USER
struct CzReqReportUser {
  static constexpr std::uint16_t kOpcode = 0x0be2;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 137;

  std::uint32_t AID = 0;
  std::uint32_t targetAID = 0;
  std::string targetName;
  std::int16_t reason = 0;
  std::string message;
};

// PACKET_ZC_ACK_REPORT_USER
struct ZcAckReportUser {
  static constexpr std::uint16_t kOpcode = 0x0be3;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 34;

  std::uint32_t targetAID = 0;
  std::string targetName;
  std::int32_t result = 0;
};

// PACKET_CZ_QUEST_STATUS_REQ
struct CzQuestStatusReq {
  static constexpr std::uint16_t kOpcode = 0x0bf3;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;
};

// PACKET_CZ_MOVE_ITEM_TO_PERSONAL
struct CzMoveItemToPersonal {
  static constexpr std::uint16_t kOpcode = 0x0c22;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t unknown = 0;
  std::uint16_t index = 0;
  std::uint32_t amount = 0;
};

// Lit une trame ENTIÈRE, opcode compris, et rend faux si elle ne correspond
// pas : mauvais opcode, longueur inattendue, ou queue incomplète. Un paquet
// partiellement rempli n'est jamais rendu comme valide.
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcParChange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcLongparChange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcStatusChange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyCartitemCountinfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAttackRange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCouplestatus& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcLonglongparChange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketAuthok& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketMonsterHp& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketScNotick& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcItemPickupAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketDropflooritem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketSpawnUnit& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketUnitWalking& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketIdleUnit& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketStatusChange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketStatusChangeEnd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketStatusChange2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketMaptypeproperty2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueNoticeDelete& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueRegister& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueUpdateInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueCheckstate& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueRevokeReq& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueBattlebeginAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueNotifyEntry& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBgqueueBattlebegins& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketScriptClear& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          ZcBroadcastingSpecialItemObtainItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketItemDropAnnounce& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketCartAdditemAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBankingCheck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBankingDepositReq& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBankingWithdrawReq& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBankingDepositAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketBankingWithdrawAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketRouletteOpenAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketRouletteInfoAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketRouletteCloseAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketRouletteGenerateAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketRouletteItemrecvReq& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketRouletteItemrecvAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketItemlistNormal& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketItemlistEquip& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcStoreItemlistNormal& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcInventoryStart& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcInventoryEnd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcStoreItemlistEquip& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketEquipItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcReqWearEquipAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketUnequipitemAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcEquipwinMicroscope& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketNotifyBounditem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketGraffitiEntry& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketDamage& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketGmMonsterItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzNpcMarketPurchase& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNpcMarketOpen& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketWisEnd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketPartyLeaderChanged& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcShortcutKeyList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzShortcutKeyChange1& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzShortcutKeyChange2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzShortcutkeybarRotate1& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzShortcutkeybarRotate2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketQuestListHeader& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketChatMessage& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketWhisperMessage& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzAddItemToMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckAddItemRodex& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqOpenWriteMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckOpenWriteMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqRemoveItemMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckRemoveItemMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzSendMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcWriteMailResult& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzCheckname1& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzCheckname2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCheckname& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyUnreadmail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMailList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqNextMailList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqOpenMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqReadMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckReadRodex& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqDeleteMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckDeleteMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqRefreshMailList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqZenyFromMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckZenyFromMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqItemFromMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckItemFromMail& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSkillScale& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcProgressActor& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAddMemberToGroup& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGroupList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcClaninfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyClanConnectinfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckClanLeave& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyClanChat& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketQuestAddHeader& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketQuestUpdateHeader& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketQuestHuntInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcFormatstringMsg& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcFormatstringMsgColor& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMsgColor& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzOpenUi& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUiOpen& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUiOpen2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUiAction& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPrivateAirshipRequest& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPrivateAirshipResponse& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqStyleChange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqStyleChange2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcStyleChangeRes& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPetEvolution& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketZcRefuseLogin& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyChat& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyPlayerchat& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcItemEntry& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAddItemToStore& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMvpGettingItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckTouseskill& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAddItemToCart& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqmakingitem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckReqmakingitem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcFeedPet& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcFeedMer& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUseItemAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSpriteChange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAddExchangeItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCashTimeCounter& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCashItemDelete& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcItemPickupParty& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUpdateItemFromBuyingStore& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckWeaponrefine& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          ZcFailedTradeBuyingStoreToSeller& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqItemrepair2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqItemrepair1& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqMakingitem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzSsilistItemClick& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckSchedulerCashitem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPcPurchaseMyitemlist& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPcPurchaseItemlist& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPcPurchaseItemlist& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqOpenBuyingStore& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMyitemlistBuyingStore& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPcPurchaseItemlistFrommc& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckItemlistBuyingStore& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqTradeBuyingStore& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMakableitemlist& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMakingarrowList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSkillSelectRequest& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzSkillSelectResponse& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyWeaponitemlist& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMakingitemList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPcCashPointItemlist& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPcBuyCashPointItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzSearchStoreInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSearchStoreInfoFailed& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcOpenSearchStoreInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSsilistItemClickAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSearchStoreInfoAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketAchievementUpdate& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketAchievementRewardAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, PacketReqnameAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckReqnameall& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckReqnameallNpc& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcOverweightPercent& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcWarplist& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGroupIsalive& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGuildPosition& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoveItemFailed& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckBanGuild1& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckBanGuild2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckBanGuild3& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckLeaveGuild1& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckLeaveGuild2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMemorialdungeonCommand& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcRemoveEffect& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcViewCamerainfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzViewCamerainfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUpdateCardslot& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDebugmsg& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzUseSkillStart& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzUseSkillEnd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcExtendBodyitemSize& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          ZcAckOpenMsgboxExtendBodyitemSize& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckExtendBodyitemSize& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          CzReqOpenMsgboxExtendBodyitemSize& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqExtendBodyitemSize& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          CzCloseMsgboxExtendBodyitemSize& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqRemaintime& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPartyConfig& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNpcBarterMarketIteminfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzNpcBarterMarketClose& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzNpcBarterMarketPurchase& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUseskillAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzClientVersion& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPingLive& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPingLive& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzCmdResetcooltime& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzCloseUiStylingshop& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyActorinit& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPartyConfig& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcRoleChange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcBanList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckCloseRoulette& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckMergeItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMergeItemOpen& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSePcBuyCashitemResult& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcOpenRefiningUi& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzRefiningSelectItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcRefiningMaterialList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqRefining& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzCloseRefiningUi& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          ZcBroadcastItemrefiningResult& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcStatusChangeAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcEquipmentEffect& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAddSkill& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSkillinfoList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSkillinfoUpdate2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNpcMarketPurchaseResult& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcTalkboxChatcontents& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGuildAgitInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqMoveGuildAgit& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcReqAckMoveGuildAgit& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcReqAckAgitInvestment& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqAgitInvestment& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcRandomCombineItemUiOpen& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzRandomCombineItemUiClose& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqRandomCombineItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckRandomCombineItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzUninstallation& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyEffect3& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzSeCashshopOpen1& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzSeCashshopOpen2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzGetAccountLimtiedSaleList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSeCashshopOpen& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          CzNpcExpandedBarterMarketClose& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          CzNpcExpandedBarterMarketPurchase& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcStateChange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAutorunSkill& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcRandomUpgradeItemUiOpen& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckRandomUpgradeItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzRandomUpgradeItemUiClose& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqRandomUpgradeItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPersonalInfomation& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzRequestActnpc& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifySkill& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUseSkill& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyGroundskill& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSkillPostdelay& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifySkillPosition& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCMarkerinfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMembermgrInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGuildInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPositionIdNameInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPositionInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGuildSkillinfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMyguildBasicInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqUploadMacroDetector& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckUploadMacroDetector& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          CzUploadMacroDetectorCaptcha& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          ZcCompleteUploadMacroDetectorCaptcha& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqApplyMacroDetector& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckApplyMacroDetector& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcApplyMacroDetector& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcApplyMacroDetectorCaptcha& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          CzCompleteApplyMacroDetectorCaptcha& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcReqAnswerMacroDetector& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzAckAnswerMacroDetector& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCloseMacroDetector& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqPreviewMacroDetector& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckPreviewMacroDetector& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          ZcPreviewMacroDetectorCaptcha& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqPlayerAidInRange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckPlayerAidInRange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckMakeGroup& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPartyJoinReq& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPartyJoinReqAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyChatParty& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyPositionToGroupm& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyHpToGroupm& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyMemberinfoToGroupm& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDeleteMemberFromGroup& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqTakeoffEquipAll& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckTakeoffEquipAll& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcBattlefieldNotifyHp& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzChooseMenuZero& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDialogTextAlign& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          CzGradeEnchantSelectEquipment& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGradeEnchantMaterialList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzGradeEnchantRequest& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzGradeEnchantCloseUi& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGradeEnchantAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          ZcGradeEnchantBroadcastResult& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcShowImage& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUpdateGdid& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzApproximateActor& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzContactnpc& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAttackFailureForDistance& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcStartCapture& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcTrycaptureMonster& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPropertyPet& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcChangestatePet& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSpirits& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSpirits2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSoulenergy& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSayDialog& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSayDialog2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcWaitDialog& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcWaitDialog2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDialogWindowSize& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDialogWindowPos& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDialogWindowPos2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPlayNpcBgm& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoveItemFromBodyToCart& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSound& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcBuyingStoreEntry& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcStoreEntry& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPcPurchaseItemlistFrommc& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPcPurchaseItemlistFrommc2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDisappearBuyingStoreEntry& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcOpenReformUi& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzCloseReformUi& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzItemReform& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcItemReformAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzUsePackageitem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzRequestRandomEnchant& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzRequestPerfectEnchant& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzRequestUpgradeEnchant& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzRequestResetEnchant& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcResponseEnchant& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzCloseUiEnchant& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSpecialpopup& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGoldpccafePoint& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzDynamicnpcCreateRequest& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDynamicnpcCreateResult& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqGuildEmblemImg1& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqGuildEmblemImg3& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqGuildEmblemImg2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcChangeGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGuildEmblemImg& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzAdventurerAgencyJoinReq& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          ZcAdventurerAgencyJoinResult& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAdventurerAgencyJoinReq& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          CzAdventurerAgencyJoinResult& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightSettings& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightSetting& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightIntegrity& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonclientIntegrity& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightKickNotice& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightDiscordMsg& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightPresetCmd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightPresetList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightSkillDmg& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightCheatReport& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqTechdata& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightTechdata& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqDamage& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightDamage& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightStoragePrices& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqCompatCards& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightCompatCards& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightJump& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightJump& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightCookMastery& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightOpenStorage& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightStorageList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqMobinfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightMobinfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightChannelList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqEntityProps& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightEntityProps& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightUiCaps& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightNpcAdmin& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightPlayerAdmin& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightStyle& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightStyles& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightStyleOpen& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightTargetInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightTargetInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightMvpCmd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightMvpState& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightMvpGroup& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightPartyShare& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightFlagGraffiti& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightUnitMaster& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightServerRules& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightChatAuthor& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightQuestEnd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightDiscordRich& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightEquipAttributes& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqChatBlocks& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightChatBlocks& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightInstanceInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightMarketReq& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightMarket& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightGmOutfit& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightGmOutfit& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonclientMapEditPut& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonclientMapEditResult& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonclientMapEditDoc& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonclientMapEditRevision& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonclientMapEditGet& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightAttackElement& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightMinigameTrophy& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightSurvivorsBegin& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightSurvivorsTicket& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightSurvivorsEnd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightSurvivorsResult& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightSurvivorsBoard& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightSurvivorsBoard& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightSurvivorsJournal& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightSurvivorsService& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightCardAlbum& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightCardAlbumCmd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqStatusList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightStatusList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqLooks& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightLooks& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightStatBonus& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightReqItemscript& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightItemscript& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightBugReport& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightBugReportAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoonlightCompanion& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightCompanionState& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMoonlightHateffectMap& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUserCount& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPcPurchaseResult& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqMakingarrow& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzSePcBuyCashitemList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          CzReqCashBargainSaleItemInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          ZcAckCashBargainSaleItemInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqApplyBargainSaleItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqRemoveBargainSaleItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyBargainSaleSelling& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyBargainSaleClose& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckCountBargainSaleItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckGuildstorageLog& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzAdvancedStatusChange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqAddNewEmblem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcBroadcast& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcBroadcast2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcEntryQueueInit& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzRodexReturn& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcRodexReturn& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqStyleClose& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSummonHpInit& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSummonHpUpdate& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcReputeInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcReputeOpen& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUiOpenV3& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcTargetSpirits& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcFriendsList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPcSellItemlist& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqChangeMemberpos& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcClearDialog& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyBindOnEquip& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcBankingCheck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckBankingWithdraw& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckBankingDeposit& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckCloseBanking& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckOpenBanking& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcReqExchangeItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckExchangeItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckAddExchangeItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCouplename& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPartyReqMasterToJoin& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPartyReqMasterToJoin& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPartyReqAckMasterToJoin& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPartyJoinReqAckFromMaster& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqSeCashTabCode& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckSeCashItemList2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqMergeItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzResetSkill& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSkillinfoUpdate& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcBossInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzInventoryTab& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcInventoryTab& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqOpenBanking& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqCloseBanking& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqBankingCheck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqBankingDeposit& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqBankingWithdraw& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAcceptEnter& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcRefuseEnter& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcRestartAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyVanish& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcItemDisappear& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMillenniumshield& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSpiritsAttribute& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcChangestateMer& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcHoskillinfoList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcHoskillinfoUpdate& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyPlayermove& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcChangeDirection& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNpcackMapmove& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNpcackServermove& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcStopmove& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSelectDealtype& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPcSellItemlist& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCloseDialog& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMenuList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcOpenEditdlg& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcOpenEditdlgstr& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckReqAllyGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDeleteRelatedGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcReqAllyGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckReqHostileGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCompass& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcItemThrowAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDeleteItemFromBody& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCartoff& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckGuildMenuinterface& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame,
                          ZcAckChangeGuildPositioninfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyPositionToGuildm& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGuildChat& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUpdateCharstat& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckReqChangeMembers& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcStatus& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyMapinfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckRememberWarppoint& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDispel& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcResurrection& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyMapproperty2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckItemrefining& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcParChangeUser& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcChangeChatroom& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcEquipArrow& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCloseStore& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDeleteItemFromStore& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyStoreitemCountinfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcExchangeitemUndo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcExecExchangeItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCancelExchangeItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcConcludeExchangeItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckCreateChatroom& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcRefuseEnterRoom& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDestroyRoom& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcEnterRoom& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNpcShowefstUpdate& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcActionFailure& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyEffect& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckItemcomposition& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckItemidentify& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckItemrepair& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcEquipitemDamaged& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDeleteItemFromCart& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcOpenstore& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPcPurchaseResultFrommc& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckOpenstore2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSkillDisappear& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSkillinfoDelete& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSkillUpdate& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcHighjump& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcRecovery& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckWhisper& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckAdditemToCart& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDeleteitemFromMcstore& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqBanGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqLeaveGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqDisorganizeGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckDisorganizeGuildResult& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcResultMakeGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqJoinGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckReqJoinGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcReqJoinGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzJoinGuild& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGuildNotice& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqJoinGuild2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqJoinGroup& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzJoinGroup& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqLeaveGroup& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqExpelGroupMember& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPartyJoinReq& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzPartyJoinReqAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcElParChange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqEmotion& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyAct& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzRequestMovenpc& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNpcspriteChange& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMemberNewentry& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMemberExit& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMovetoMap& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzBroadcast& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqWearEquip& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzAckSelectDealtype& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzCreateChatroom& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzBlacksmithRank& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzAlchemistRank& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcBlacksmithPoint& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAlchemistPoint& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcTaekwonPoint& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzTaekwonRank& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcKillerPoint& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzKillerRank& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqRanking& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUpdateRankingPoint& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckRanking2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzLesseffect& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzActiveQuest& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzJoinBaby& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzAuctionItemSearch& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzAuctionBuy& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzAuctionAdd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDressroomOpen& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcRoomNewentry& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMonsterInfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckReqnameBygid& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPetAct& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCombodelay& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcBladestop& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMvp& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMvpGettingSpecialExp& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcThrowMvpitem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcUpdateMapinfo& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqEnterRoom& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzChangeChatroom& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzAddExchangeItem& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoveItemFromCartToBody& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzSelectWarppoint& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzInputEditdlg& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzInputEditdlgstr& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzCloseDialog& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReset& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoveItemFromStoreToCart& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqItemidentify& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzSelectautospell& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqItemcompositionList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqItemcomposition& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzLocalbroadcast& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoveItemFromCartToStore& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcFriendsState& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMakeGroup& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMakeGroup2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzGmChecker& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcGmChecker& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcConfigNotify& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcCongratulation& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDivorce& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcEmotion& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMsg& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMsgSkill& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcMsgValue& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyMannerPointGiven& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcNotifyTime& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSettingWhisperPc& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSettingWhisperState& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcSkillmsg& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqEmotionExpansion& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqBuyCashEmotion& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckBuyCashEmotion& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcPlayCashEmotion& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcDisappearEntry& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcEfstSetEnter& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzSettingWhisperState& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcWhisperList& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzAllyChat& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzReqReportUser& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ZcAckReportUser& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzQuestStatusReq& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CzMoveItemToPersonal& packet);

// Rend la trame entière, prête à émettre : opcode en tête, et longueur
// renseignée si le paquet en porte une. Vide si elle ne tient pas sur le fil.
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcParChange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcLongparChange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcStatusChange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyCartitemCountinfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAttackRange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCouplestatus& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcLonglongparChange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketAuthok& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketMonsterHp& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketScNotick& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcItemPickupAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketDropflooritem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketSpawnUnit& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketUnitWalking& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketIdleUnit& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketStatusChange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketStatusChangeEnd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketStatusChange2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketMaptypeproperty2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketBgqueueAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketBgqueueNoticeDelete& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketBgqueueRegister& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketBgqueueUpdateInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketBgqueueCheckstate& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketBgqueueRevokeReq& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketBgqueueBattlebeginAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketBgqueueNotifyEntry& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketBgqueueBattlebegins& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketScriptClear& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcBroadcastingSpecialItemObtainItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketItemDropAnnounce& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketCartAdditemAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketBankingCheck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketBankingDepositAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketBankingWithdrawAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketRouletteInfoAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketRouletteGenerateAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketRouletteItemrecvAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcInventoryStart& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcInventoryEnd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketEquipItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcReqWearEquipAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketUnequipitemAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcEquipwinMicroscope& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketNotifyBounditem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketGraffitiEntry& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketDamage& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzNpcMarketPurchase& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNpcMarketOpen& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketWisEnd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketPartyLeaderChanged& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcShortcutKeyList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzShortcutKeyChange1& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzShortcutKeyChange2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzShortcutkeybarRotate1& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzShortcutkeybarRotate2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckAddItemRodex& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckOpenWriteMail& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckRemoveItemMail& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcWriteMailResult& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzCheckname1& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzCheckname2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCheckname& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyUnreadmail& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckReadRodex& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckDeleteMail& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSkillScale& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAddMemberToGroup& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGroupList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcClaninfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyClanConnectinfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckClanLeave& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyClanChat& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMsgColor& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUiOpen& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUiOpen2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqStyleChange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqStyleChange2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcStyleChangeRes& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyChat& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcItemEntry& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAddItemToStore& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMvpGettingItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckTouseskill& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAddItemToCart& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqmakingitem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckReqmakingitem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcFeedPet& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcFeedMer& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUseItemAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSpriteChange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAddExchangeItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCashTimeCounter& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCashItemDelete& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcItemPickupParty& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUpdateItemFromBuyingStore& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckWeaponrefine& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcFailedTradeBuyingStoreToSeller& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqItemrepair2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqItemrepair1& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqMakingitem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzSsilistItemClick& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckSchedulerCashitem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPcPurchaseMyitemlist& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPcPurchaseItemlist& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMyitemlistBuyingStore& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPcPurchaseItemlistFrommc& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckItemlistBuyingStore& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMakableitemlist& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMakingarrowList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSkillSelectRequest& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzSkillSelectResponse& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyWeaponitemlist& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMakingitemList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPcCashPointItemlist& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSearchStoreInfoFailed& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcOpenSearchStoreInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSsilistItemClickAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSearchStoreInfoAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketAchievementUpdate& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const PacketAchievementRewardAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckReqnameall& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckReqnameallNpc& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcWarplist& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGroupIsalive& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGuildPosition& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoveItemFailed& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcRemoveEffect& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcViewCamerainfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzViewCamerainfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUpdateCardslot& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDebugmsg& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzUseSkillStart& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzUseSkillEnd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcExtendBodyitemSize& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckOpenMsgboxExtendBodyitemSize& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckExtendBodyitemSize& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqOpenMsgboxExtendBodyitemSize& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqExtendBodyitemSize& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzCloseMsgboxExtendBodyitemSize& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqRemaintime& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzPartyConfig& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNpcBarterMarketIteminfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzNpcBarterMarketClose& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzNpcBarterMarketPurchase& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUseskillAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzClientVersion& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzPingLive& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPingLive& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzCmdResetcooltime& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzCloseUiStylingshop& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyActorinit& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPartyConfig& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcRoleChange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcBanList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckCloseRoulette& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckMergeItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMergeItemOpen& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSePcBuyCashitemResult& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcOpenRefiningUi& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzRefiningSelectItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcRefiningMaterialList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqRefining& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzCloseRefiningUi& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcBroadcastItemrefiningResult& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcStatusChangeAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcEquipmentEffect& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAddSkill& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSkillinfoList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSkillinfoUpdate2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNpcMarketPurchaseResult& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcTalkboxChatcontents& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGuildAgitInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqMoveGuildAgit& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcReqAckMoveGuildAgit& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcReqAckAgitInvestment& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqAgitInvestment& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcRandomCombineItemUiOpen& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzRandomCombineItemUiClose& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqRandomCombineItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckRandomCombineItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzUninstallation& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyEffect3& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzSeCashshopOpen1& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzSeCashshopOpen2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzGetAccountLimtiedSaleList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSeCashshopOpen& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzNpcExpandedBarterMarketClose& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzNpcExpandedBarterMarketPurchase& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcStateChange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAutorunSkill& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcRandomUpgradeItemUiOpen& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckRandomUpgradeItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzRandomUpgradeItemUiClose& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqRandomUpgradeItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPersonalInfomation& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifySkill& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUseSkill& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyGroundskill& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSkillPostdelay& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifySkillPosition& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCMarkerinfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMembermgrInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGuildInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPositionIdNameInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPositionInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGuildSkillinfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMyguildBasicInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqUploadMacroDetector& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckUploadMacroDetector& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzUploadMacroDetectorCaptcha& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCompleteUploadMacroDetectorCaptcha& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqApplyMacroDetector& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckApplyMacroDetector& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcApplyMacroDetector& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcApplyMacroDetectorCaptcha& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzCompleteApplyMacroDetectorCaptcha& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcReqAnswerMacroDetector& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzAckAnswerMacroDetector& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCloseMacroDetector& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqPreviewMacroDetector& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckPreviewMacroDetector& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPreviewMacroDetectorCaptcha& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqPlayerAidInRange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckPlayerAidInRange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckMakeGroup& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPartyJoinReq& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPartyJoinReqAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyChatParty& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyPositionToGroupm& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyHpToGroupm& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyMemberinfoToGroupm& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDeleteMemberFromGroup& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqTakeoffEquipAll& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckTakeoffEquipAll& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcBattlefieldNotifyHp& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzChooseMenuZero& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDialogTextAlign& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzGradeEnchantSelectEquipment& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGradeEnchantMaterialList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzGradeEnchantRequest& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzGradeEnchantCloseUi& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGradeEnchantAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGradeEnchantBroadcastResult& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcShowImage& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUpdateGdid& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzApproximateActor& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzContactnpc& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAttackFailureForDistance& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcStartCapture& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcTrycaptureMonster& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPropertyPet& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcChangestatePet& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSpirits& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSpirits2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSoulenergy& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSayDialog& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSayDialog2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcWaitDialog& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcWaitDialog2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDialogWindowSize& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDialogWindowPos& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDialogWindowPos2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPlayNpcBgm& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoveItemFromBodyToCart& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSound& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcBuyingStoreEntry& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcStoreEntry& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzPcPurchaseItemlistFrommc& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzPcPurchaseItemlistFrommc2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDisappearBuyingStoreEntry& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcOpenReformUi& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzCloseReformUi& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzItemReform& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcItemReformAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzUsePackageitem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzRequestRandomEnchant& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzRequestPerfectEnchant& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzRequestUpgradeEnchant& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzRequestResetEnchant& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcResponseEnchant& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzCloseUiEnchant& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSpecialpopup& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGoldpccafePoint& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzDynamicnpcCreateRequest& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDynamicnpcCreateResult& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqGuildEmblemImg1& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqGuildEmblemImg3& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqGuildEmblemImg2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcChangeGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGuildEmblemImg& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzAdventurerAgencyJoinReq& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAdventurerAgencyJoinResult& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAdventurerAgencyJoinReq& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzAdventurerAgencyJoinResult& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightSettings& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightSetting& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightIntegrity& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonclientIntegrity& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightKickNotice& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightDiscordMsg& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightPresetCmd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightPresetList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightSkillDmg& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightCheatReport& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightReqTechdata& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightTechdata& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightReqDamage& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightDamage& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightStoragePrices& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightReqCompatCards& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightCompatCards& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightJump& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightJump& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightCookMastery& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightOpenStorage& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightStorageList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightReqMobinfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightMobinfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightChannelList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightReqEntityProps& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightEntityProps& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightUiCaps& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightNpcAdmin& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightPlayerAdmin& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightStyle& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightStyles& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightStyleOpen& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightTargetInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightTargetInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightMvpCmd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightMvpState& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightMvpGroup& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightPartyShare& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightFlagGraffiti& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightUnitMaster& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightServerRules& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightChatAuthor& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightQuestEnd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightDiscordRich& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightEquipAttributes& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightReqChatBlocks& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightChatBlocks& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightInstanceInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightMarketReq& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightMarket& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightGmOutfit& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightGmOutfit& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonclientMapEditPut& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonclientMapEditResult& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonclientMapEditDoc& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonclientMapEditRevision& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonclientMapEditGet& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightAttackElement& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightMinigameTrophy& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightSurvivorsBegin& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightSurvivorsTicket& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightSurvivorsEnd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightSurvivorsResult& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightSurvivorsBoard& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightSurvivorsBoard& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightSurvivorsJournal& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightSurvivorsService& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightCardAlbum& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightCardAlbumCmd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightReqStatusList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightStatusList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightReqLooks& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightLooks& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightStatBonus& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightReqItemscript& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightItemscript& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightBugReport& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightBugReportAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoonlightCompanion& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightCompanionState& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMoonlightHateffectMap& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUserCount& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPcPurchaseResult& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqMakingarrow& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzSePcBuyCashitemList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqCashBargainSaleItemInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckCashBargainSaleItemInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqApplyBargainSaleItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqRemoveBargainSaleItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyBargainSaleSelling& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyBargainSaleClose& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckCountBargainSaleItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckGuildstorageLog& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzAdvancedStatusChange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqAddNewEmblem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcBroadcast& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcBroadcast2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcEntryQueueInit& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzRodexReturn& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcRodexReturn& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqStyleClose& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSummonHpInit& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSummonHpUpdate& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcReputeInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcReputeOpen& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUiOpenV3& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcTargetSpirits& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcFriendsList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzPcSellItemlist& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqChangeMemberpos& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcClearDialog& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyBindOnEquip& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcBankingCheck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckBankingWithdraw& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckBankingDeposit& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckCloseBanking& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckOpenBanking& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcReqExchangeItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckExchangeItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckAddExchangeItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCouplename& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzPartyReqMasterToJoin& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPartyReqMasterToJoin& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzPartyReqAckMasterToJoin& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPartyJoinReqAckFromMaster& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqSeCashTabCode& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckSeCashItemList2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqMergeItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzResetSkill& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSkillinfoUpdate& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcBossInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzInventoryTab& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcInventoryTab& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqOpenBanking& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqCloseBanking& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqBankingCheck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqBankingDeposit& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqBankingWithdraw& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAcceptEnter& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcRefuseEnter& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcRestartAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyVanish& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcItemDisappear& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMillenniumshield& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSpiritsAttribute& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcChangestateMer& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcHoskillinfoList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcHoskillinfoUpdate& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyPlayermove& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcChangeDirection& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNpcackMapmove& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNpcackServermove& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcStopmove& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSelectDealtype& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPcSellItemlist& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCloseDialog& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMenuList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcOpenEditdlg& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcOpenEditdlgstr& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckReqAllyGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDeleteRelatedGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcReqAllyGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckReqHostileGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCompass& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcItemThrowAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDeleteItemFromBody& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCartoff& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckGuildMenuinterface& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckChangeGuildPositioninfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyPositionToGuildm& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGuildChat& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUpdateCharstat& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckReqChangeMembers& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcStatus& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyMapinfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckRememberWarppoint& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDispel& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcResurrection& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyMapproperty2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckItemrefining& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcParChangeUser& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcChangeChatroom& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcEquipArrow& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCloseStore& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDeleteItemFromStore& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyStoreitemCountinfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcExchangeitemUndo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcExecExchangeItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCancelExchangeItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcConcludeExchangeItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckCreateChatroom& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcRefuseEnterRoom& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDestroyRoom& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcEnterRoom& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNpcShowefstUpdate& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcActionFailure& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyEffect& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckItemcomposition& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckItemidentify& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckItemrepair& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcEquipitemDamaged& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDeleteItemFromCart& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcOpenstore& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPcPurchaseResultFrommc& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckOpenstore2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSkillDisappear& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSkillinfoDelete& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSkillUpdate& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcHighjump& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcRecovery& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckWhisper& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckAdditemToCart& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDeleteitemFromMcstore& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqBanGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqLeaveGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqDisorganizeGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckDisorganizeGuildResult& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcResultMakeGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqJoinGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckReqJoinGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcReqJoinGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzJoinGuild& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGuildNotice& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqJoinGuild2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqJoinGroup& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzJoinGroup& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqLeaveGroup& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqExpelGroupMember& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzPartyJoinReq& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzPartyJoinReqAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcElParChange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqEmotion& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyAct& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzRequestMovenpc& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNpcspriteChange& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMemberNewentry& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMemberExit& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMovetoMap& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzBroadcast& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqWearEquip& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzAckSelectDealtype& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzCreateChatroom& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzBlacksmithRank& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzAlchemistRank& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcBlacksmithPoint& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAlchemistPoint& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcTaekwonPoint& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzTaekwonRank& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcKillerPoint& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzKillerRank& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqRanking& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUpdateRankingPoint& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckRanking2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzLesseffect& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzActiveQuest& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzJoinBaby& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzAuctionItemSearch& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzAuctionBuy& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzAuctionAdd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDressroomOpen& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcRoomNewentry& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMonsterInfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckReqnameBygid& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPetAct& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCombodelay& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcBladestop& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMvp& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMvpGettingSpecialExp& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcThrowMvpitem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcUpdateMapinfo& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqEnterRoom& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzChangeChatroom& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzAddExchangeItem& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoveItemFromCartToBody& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzSelectWarppoint& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzInputEditdlg& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzInputEditdlgstr& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzCloseDialog& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReset& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoveItemFromStoreToCart& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqItemidentify& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzSelectautospell& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqItemcompositionList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqItemcomposition& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzLocalbroadcast& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoveItemFromCartToStore& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcFriendsState& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMakeGroup& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMakeGroup2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzGmChecker& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcGmChecker& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcConfigNotify& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcCongratulation& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDivorce& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcEmotion& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMsg& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMsgSkill& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcMsgValue& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyMannerPointGiven& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcNotifyTime& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSettingWhisperPc& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSettingWhisperState& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcSkillmsg& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqEmotionExpansion& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqBuyCashEmotion& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckBuyCashEmotion& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcPlayCashEmotion& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcDisappearEntry& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcEfstSetEnter& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzSettingWhisperState& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcWhisperList& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzAllyChat& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzReqReportUser& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ZcAckReportUser& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzQuestStatusReq& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CzMoveItemToPersonal& packet);

}  // namespace ro::proto
