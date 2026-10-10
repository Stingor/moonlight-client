#pragma once

// GÉNÉRÉ par tools/packetgen/structgen.py — ne pas modifier à la main.
//
// Les décalages viennent du compilateur qui construit le serveur, pas d'une
// relecture de ses sources : voir le générateur pour la raison.
//
// Source : src/common/packets.hpp — connexion et choix du personnage
//
// Les noms de champs sont ceux du SERVEUR, sans retouche. Les renommer romprait
// la correspondance avec la seule source qui dise ce qu'un champ signifie.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ro::proto {

// La version de protocole dont ces dispositions sont tirées.
inline constexpr std::int32_t kCommonPacketVersion = 20250716;

// CHARACTER_INFO
struct CharacterInfo {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 175;

  std::uint32_t GID = 0;
  std::int64_t exp = 0;
  std::int32_t money = 0;
  std::int64_t jobexp = 0;
  std::int32_t joblevel = 0;
  std::int32_t bodystate = 0;
  std::int32_t healthstate = 0;
  std::int32_t effectstate = 0;
  std::int32_t virtue = 0;
  std::int32_t honor = 0;
  std::int16_t jobpoint = 0;
  std::int64_t hp = 0;
  std::int64_t maxhp = 0;
  std::int64_t sp = 0;
  std::int64_t maxsp = 0;
  std::int16_t speed = 0;
  std::int16_t job = 0;
  std::int16_t head = 0;
  std::int16_t body = 0;
  std::int16_t weapon = 0;
  std::int16_t level = 0;
  std::int16_t sppoint = 0;
  std::int16_t accessory = 0;
  std::int16_t shield = 0;
  std::int16_t accessory2 = 0;
  std::int16_t accessory3 = 0;
  std::int16_t headpalette = 0;
  std::int16_t bodypalette = 0;
  std::string name;
  std::uint8_t Str = 0;
  std::uint8_t Agi = 0;
  std::uint8_t Vit = 0;
  std::uint8_t Int = 0;
  std::uint8_t Dex = 0;
  std::uint8_t Luk = 0;
  std::uint8_t CharNum = 0;
  std::uint8_t hairColor = 0;
  std::int16_t bIsChangedCharName = 0;
  std::string mapName;
  std::int32_t DelRevDate = 0;
  std::int32_t robePalette = 0;
  std::int32_t chr_slot_changeCnt = 0;
  std::int32_t chr_name_changeCnt = 0;
  std::uint8_t sex = 0;
};

// PACKET_CA_LOGIN
struct CaLogin {
  static constexpr std::uint16_t kOpcode = 0x0064;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 55;

  std::uint32_t version = 0;
  std::string username;
  std::string password;
  std::uint8_t clienttype = 0;
};

// PACKET_CH_SELECT_CHAR
struct ChSelectChar {
  static constexpr std::uint16_t kOpcode = 0x0066;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t slot = 0;
};

// PACKET_CH_MAKE_CHAR
struct ChMakeChar {
  static constexpr std::uint16_t kOpcode = 0x0a39;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 36;

  std::string name;
  std::uint8_t slot = 0;
  std::uint16_t hair_color = 0;
  std::uint16_t hair_style = 0;
  std::uint32_t job = 0;
  std::uint8_t sex = 0;
};

// PACKET_CH_DELETE_CHAR
struct ChDeleteChar {
  static constexpr std::uint16_t kOpcode = 0x01fb;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 56;

  std::uint32_t CID = 0;
  std::string key;
};

// PACKET_AC_ACCEPT_LOGIN_sub
struct AcAcceptLoginSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 160;

  std::uint32_t ip = 0;
  std::uint16_t port = 0;
  std::string name;
  std::uint16_t users = 0;
  std::uint16_t type = 0;
  std::uint16_t new_ = 0;
  std::array<std::uint8_t, 128> unknown = {};
};

// PACKET_AC_ACCEPT_LOGIN
struct AcAcceptLogin {
  static constexpr std::uint16_t kOpcode = 0x0ac4;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 64;

  std::uint32_t login_id1 = 0;
  std::uint32_t AID = 0;
  std::uint32_t login_id2 = 0;
  std::uint32_t last_ip = 0;
  std::string last_login;
  std::uint8_t sex = 0;
  std::string token;
  std::vector<AcAcceptLoginSub> char_servers;
};

// PACKET_AC_REFUSE_LOGIN
struct AcRefuseLogin {
  static constexpr std::uint16_t kOpcode = 0x083e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::uint32_t error = 0;
  std::string unblock_time;
};

// PACKET_HC_ACCEPT_ENTER
struct HcAcceptEnter {
  static constexpr std::uint16_t kOpcode = 0x006b;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 27;

  std::uint8_t total = 0;
  std::uint8_t premium_start = 0;
  std::uint8_t premium_end = 0;
  std::string extension;
  std::vector<CharacterInfo> characters;
};

// PACKET_HC_REFUSE_ENTER
struct HcRefuseEnter {
  static constexpr std::uint16_t kOpcode = 0x006c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t error = 0;
};

// PACKET_HC_ACCEPT_MAKECHAR
struct HcAcceptMakechar {
  static constexpr std::uint16_t kOpcode = 0x0b6f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 177;

  CharacterInfo character;
};

// PACKET_HC_REFUSE_MAKECHAR
struct HcRefuseMakechar {
  static constexpr std::uint16_t kOpcode = 0x006e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t error = 0;
};

// PACKET_HC_ACCEPT_DELETECHAR
struct HcAcceptDeletechar {
  static constexpr std::uint16_t kOpcode = 0x006f;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_HC_REFUSE_DELETECHAR
struct HcRefuseDeletechar {
  static constexpr std::uint16_t kOpcode = 0x0070;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t error = 0;
};

// PACKET_HC_NOTIFY_ZONESVR
struct HcNotifyZonesvr {
  static constexpr std::uint16_t kOpcode = 0x0ac5;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 156;

  std::uint32_t CID = 0;
  std::string mapname;
  std::uint32_t ip = 0;
  std::uint16_t port = 0;
  std::string domain;
};

// PACKET_SC_NOTIFY_BAN
struct ScNotifyBan {
  static constexpr std::uint16_t kOpcode = 0x0081;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t result = 0;
};

// PACKET_PING
struct Ping {
  static constexpr std::uint16_t kOpcode = 0x0187;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t AID = 0;
};

// PACKET_CA_REQ_HASH
struct CaReqHash {
  static constexpr std::uint16_t kOpcode = 0x01db;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_AC_ACK_HASH
struct AcAckHash {
  static constexpr std::uint16_t kOpcode = 0x01dc;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::string salt;
};

// PACKET_CA_LOGIN2
struct CaLogin2 {
  static constexpr std::uint16_t kOpcode = 0x01dd;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 47;

  std::uint32_t version = 0;
  std::string username;
  std::array<std::uint8_t, 16> passwordMD5 = {};
  std::uint8_t clienttype = 0;
};

// PACKET_CA_LOGIN3
struct CaLogin3 {
  static constexpr std::uint16_t kOpcode = 0x01fa;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 48;

  std::uint32_t version = 0;
  std::string username;
  std::array<std::uint8_t, 16> passwordMD5 = {};
  std::uint8_t clienttype = 0;
  std::uint8_t clientinfo = 0;
};

// PACKET_CA_CONNECT_INFO_CHANGED
struct CaConnectInfoChanged {
  static constexpr std::uint16_t kOpcode = 0x0200;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 26;

  std::string name;
};

// PACKET_CA_EXE_HASHCHECK
struct CaExeHashcheck {
  static constexpr std::uint16_t kOpcode = 0x0204;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 18;

  std::string hash;
};

// PACKET_HC_BLOCK_CHARACTER_sub
struct HcBlockCharacterSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 24;

  std::uint32_t CID = 0;
  std::string unblock_time;
};

// PACKET_HC_BLOCK_CHARACTER
struct HcBlockCharacter {
  static constexpr std::uint16_t kOpcode = 0x020d;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<HcBlockCharacterSub> characters;
};

// PACKET_CA_LOGIN_PCBANG
struct CaLoginPcbang {
  static constexpr std::uint16_t kOpcode = 0x0277;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 84;

  std::uint32_t version = 0;
  std::string username;
  std::string password;
  std::uint8_t clienttype = 0;
  std::string ip;
  std::string mac;
};

// PACKET_CA_LOGIN4
struct CaLogin4 {
  static constexpr std::uint16_t kOpcode = 0x027c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 60;

  std::uint32_t version = 0;
  std::string username;
  std::array<std::uint8_t, 16> passwordMD5 = {};
  std::uint8_t clienttype = 0;
  std::string mac;
};

// PACKET_CH_REQ_IS_VALID_CHARNAME
struct ChReqIsValidCharname {
  static constexpr std::uint16_t kOpcode = 0x028d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 34;

  std::uint32_t AID = 0;
  std::uint32_t CID = 0;
  std::string new_name;
};

// PACKET_HC_ACK_IS_VALID_CHARNAME
struct HcAckIsValidCharname {
  static constexpr std::uint16_t kOpcode = 0x028e;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t result = 0;
};

// PACKET_CH_REQ_CHANGE_CHARNAME
struct ChReqChangeCharname {
  static constexpr std::uint16_t kOpcode = 0x08fc;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 30;

  std::uint32_t CID = 0;
  std::string new_name;
};

// PACKET_HC_ACK_CHANGE_CHARNAME
struct HcAckChangeCharname {
  static constexpr std::uint16_t kOpcode = 0x08fd;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t result = 0;
};

// PACKET_CA_LOGIN_CHANNEL
struct CaLoginChannel {
  static constexpr std::uint16_t kOpcode = 0x02b0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 85;

  std::uint32_t version = 0;
  std::string username;
  std::string password;
  std::uint8_t clienttype = 0;
  std::string ip;
  std::string mac;
  std::uint8_t is_gravity = 0;
};

// PACKET_CA_SSO_LOGIN_REQ
struct CaSsoLoginReq {
  static constexpr std::uint16_t kOpcode = 0x0825;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 92;

  std::uint32_t version = 0;
  std::uint8_t clienttype = 0;
  std::string username;
  std::string password;
  std::string mac;
  std::string ip;
  std::string token;
};

// PACKET_CH_DELETE_CHAR3_RESERVED
struct ChDeleteChar3Reserved {
  static constexpr std::uint16_t kOpcode = 0x0827;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t CID = 0;
};

// PACKET_HC_DELETE_CHAR3_RESERVED
struct HcDeleteChar3Reserved {
  static constexpr std::uint16_t kOpcode = 0x0828;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t CID = 0;
  std::int32_t result = 0;
  std::uint32_t date = 0;
};

// PACKET_CH_DELETE_CHAR3
struct ChDeleteChar3 {
  static constexpr std::uint16_t kOpcode = 0x0829;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t CID = 0;
  std::string birthdate;
};

// PACKET_HC_DELETE_CHAR3
struct HcDeleteChar3 {
  static constexpr std::uint16_t kOpcode = 0x082a;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t CID = 0;
  std::int32_t result = 0;
};

// PACKET_CH_DELETE_CHAR3_CANCEL
struct ChDeleteChar3Cancel {
  static constexpr std::uint16_t kOpcode = 0x082b;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t CID = 0;
};

// PACKET_HC_DELETE_CHAR3_CANCEL
struct HcDeleteChar3Cancel {
  static constexpr std::uint16_t kOpcode = 0x082c;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t CID = 0;
  std::int32_t result = 0;
};

// PACKET_HC_ACCEPT_ENTER2
struct HcAcceptEnter2 {
  static constexpr std::uint16_t kOpcode = 0x082d;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 29;

  std::uint8_t normal = 0;
  std::uint8_t premium = 0;
  std::uint8_t billing = 0;
  std::uint8_t producible = 0;
  std::uint8_t total = 0;
  std::string extension;
};

// PACKET_HC_NOTIFY_ACCESSIBLE_MAPNAME_sub
struct HcNotifyAccessibleMapnameSub {
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 20;

  std::int32_t status = 0;
  std::string map;
};

// PACKET_HC_NOTIFY_ACCESSIBLE_MAPNAME
struct HcNotifyAccessibleMapname {
  static constexpr std::uint16_t kOpcode = 0x0840;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<HcNotifyAccessibleMapnameSub> maps;
};

// PACKET_CH_SELECT_ACCESSIBLE_MAPNAME
struct ChSelectAccessibleMapname {
  static constexpr std::uint16_t kOpcode = 0x0841;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 4;

  std::int8_t slot = 0;
  std::int8_t mapnumber = 0;
};

// PACKET_CH_SECOND_PASSWD_ACK
struct ChSecondPasswdAck {
  static constexpr std::uint16_t kOpcode = 0x08b8;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t AID = 0;
  std::string pin;
};

// PACKET_HC_SECOND_PASSWD_LOGIN
struct HcSecondPasswdLogin {
  static constexpr std::uint16_t kOpcode = 0x08b9;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 12;

  std::uint32_t seed = 0;
  std::uint32_t AID = 0;
  std::uint16_t result = 0;
};

// PACKET_CH_MAKE_SECOND_PASSWD
struct ChMakeSecondPasswd {
  static constexpr std::uint16_t kOpcode = 0x08ba;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 10;

  std::uint32_t AID = 0;
  std::string pin;
};

// PACKET_CH_EDIT_SECOND_PASSWD
struct ChEditSecondPasswd {
  static constexpr std::uint16_t kOpcode = 0x08be;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 14;

  std::uint32_t AID = 0;
  std::string old_pin;
  std::string new_pin;
};

// PACKET_CH_AVAILABLE_SECOND_PASSWD
struct ChAvailableSecondPasswd {
  static constexpr std::uint16_t kOpcode = 0x08c5;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t AID = 0;
};

// PACKET_CH_REQ_CHANGE_CHARACTER_SLOT
struct ChReqChangeCharacterSlot {
  static constexpr std::uint16_t kOpcode = 0x08d4;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t slot_before = 0;
  std::uint16_t slot_after = 0;
  std::uint16_t remaining = 0;
};

// PACKET_HC_ACK_CHANGE_CHARACTER_SLOT
struct HcAckChangeCharacterSlot {
  static constexpr std::uint16_t kOpcode = 0x0b70;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t reason = 0;
  std::uint16_t moves = 0;
};

// PACKET_HC_ACK_CHARINFO_PER_PAGE
struct HcAckCharinfoPerPage {
  static constexpr std::uint16_t kOpcode = 0x0b72;
  // Taille sur le fil SANS la queue variable, qui la porte elle-même.
  static constexpr std::size_t kWireSize = 4;

  std::vector<CharacterInfo> characters;
};

// PACKET_HC_CHARLIST_NOTIFY
struct HcCharlistNotify {
  static constexpr std::uint16_t kOpcode = 0x09a0;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t total = 0;
};

// PACKET_CH_CHARLIST_REQ
struct ChCharlistReq {
  static constexpr std::uint16_t kOpcode = 0x09a1;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 2;
};

// PACKET_CT_AUTH
struct CtAuth {
  static constexpr std::uint16_t kOpcode = 0x0acf;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 68;

  std::array<std::uint8_t, 66> unknown = {};
};

// PACKET_TC_RESULT
struct TcResult {
  static constexpr std::uint16_t kOpcode = 0x0ae3;
  // Taille sur le fil, en octets.
  static constexpr std::size_t kWireSize = 34;

  std::uint32_t type = 0;
  std::string unknown1;
  std::string unknown2;
};

// Lit une trame ENTIÈRE, opcode compris, et rend faux si elle ne correspond
// pas : mauvais opcode, longueur inattendue, ou queue incomplète. Un paquet
// partiellement rempli n'est jamais rendu comme valide.
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CaLogin& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChSelectChar& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChMakeChar& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChDeleteChar& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, AcAcceptLogin& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, AcRefuseLogin& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcAcceptEnter& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcRefuseEnter& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcAcceptMakechar& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcRefuseMakechar& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcAcceptDeletechar& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcRefuseDeletechar& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcNotifyZonesvr& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ScNotifyBan& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, Ping& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CaReqHash& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, AcAckHash& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CaLogin2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CaLogin3& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CaConnectInfoChanged& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CaExeHashcheck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcBlockCharacter& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CaLoginPcbang& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CaLogin4& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChReqIsValidCharname& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcAckIsValidCharname& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChReqChangeCharname& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcAckChangeCharname& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CaLoginChannel& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CaSsoLoginReq& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChDeleteChar3Reserved& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcDeleteChar3Reserved& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChDeleteChar3& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcDeleteChar3& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChDeleteChar3Cancel& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcDeleteChar3Cancel& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcAcceptEnter2& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcNotifyAccessibleMapname& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChSelectAccessibleMapname& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChSecondPasswdAck& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcSecondPasswdLogin& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChMakeSecondPasswd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChEditSecondPasswd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChAvailableSecondPasswd& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChReqChangeCharacterSlot& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcAckChangeCharacterSlot& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcAckCharinfoPerPage& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, HcCharlistNotify& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, ChCharlistReq& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, CtAuth& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, TcResult& packet);

// Rend la trame entière, prête à émettre : opcode en tête, et longueur
// renseignée si le paquet en porte une. Vide si elle ne tient pas sur le fil.
[[nodiscard]] std::vector<std::uint8_t> Encode(const CaLogin& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChSelectChar& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChMakeChar& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChDeleteChar& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const AcAcceptLogin& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const AcRefuseLogin& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcAcceptEnter& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcRefuseEnter& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcAcceptMakechar& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcRefuseMakechar& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcAcceptDeletechar& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcRefuseDeletechar& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcNotifyZonesvr& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ScNotifyBan& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const Ping& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CaReqHash& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const AcAckHash& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CaLogin2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CaLogin3& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CaConnectInfoChanged& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CaExeHashcheck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcBlockCharacter& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CaLoginPcbang& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CaLogin4& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChReqIsValidCharname& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcAckIsValidCharname& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChReqChangeCharname& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcAckChangeCharname& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CaLoginChannel& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CaSsoLoginReq& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChDeleteChar3Reserved& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcDeleteChar3Reserved& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChDeleteChar3& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcDeleteChar3& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChDeleteChar3Cancel& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcDeleteChar3Cancel& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcAcceptEnter2& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcNotifyAccessibleMapname& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChSelectAccessibleMapname& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChSecondPasswdAck& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcSecondPasswdLogin& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChMakeSecondPasswd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChEditSecondPasswd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChAvailableSecondPasswd& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChReqChangeCharacterSlot& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcAckChangeCharacterSlot& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcAckCharinfoPerPage& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const HcCharlistNotify& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const ChCharlistReq& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const CtAuth& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const TcResult& packet);

}  // namespace ro::proto
