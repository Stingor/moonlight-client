#include "ro/proto/common_packets.h"

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
[[maybe_unused]] void ReadCharacterInfo(ByteReader& reader, CharacterInfo& value);
[[maybe_unused]] void WriteCharacterInfo(ByteWriter& writer, const CharacterInfo& value);
[[maybe_unused]] void ReadAcAcceptLoginSub(ByteReader& reader, AcAcceptLoginSub& value);
[[maybe_unused]] void WriteAcAcceptLoginSub(ByteWriter& writer, const AcAcceptLoginSub& value);
[[maybe_unused]] void ReadHcBlockCharacterSub(ByteReader& reader, HcBlockCharacterSub& value);
[[maybe_unused]] void WriteHcBlockCharacterSub(ByteWriter& writer,
                                               const HcBlockCharacterSub& value);
[[maybe_unused]] void ReadHcNotifyAccessibleMapnameSub(ByteReader& reader,
                                                       HcNotifyAccessibleMapnameSub& value);
[[maybe_unused]] void WriteHcNotifyAccessibleMapnameSub(ByteWriter& writer,
                                                        const HcNotifyAccessibleMapnameSub& value);

void ReadCharacterInfo(ByteReader& reader, CharacterInfo& value) {
  value.GID = static_cast<std::uint32_t>(reader.U32());
  value.exp = static_cast<std::int64_t>(reader.I64());
  value.money = static_cast<std::int32_t>(reader.I32());
  value.jobexp = static_cast<std::int64_t>(reader.I64());
  value.joblevel = static_cast<std::int32_t>(reader.I32());
  value.bodystate = static_cast<std::int32_t>(reader.I32());
  value.healthstate = static_cast<std::int32_t>(reader.I32());
  value.effectstate = static_cast<std::int32_t>(reader.I32());
  value.virtue = static_cast<std::int32_t>(reader.I32());
  value.honor = static_cast<std::int32_t>(reader.I32());
  value.jobpoint = static_cast<std::int16_t>(reader.I16());
  value.hp = static_cast<std::int64_t>(reader.I64());
  value.maxhp = static_cast<std::int64_t>(reader.I64());
  value.sp = static_cast<std::int64_t>(reader.I64());
  value.maxsp = static_cast<std::int64_t>(reader.I64());
  value.speed = static_cast<std::int16_t>(reader.I16());
  value.job = static_cast<std::int16_t>(reader.I16());
  value.head = static_cast<std::int16_t>(reader.I16());
  value.body = static_cast<std::int16_t>(reader.I16());
  value.weapon = static_cast<std::int16_t>(reader.I16());
  value.level = static_cast<std::int16_t>(reader.I16());
  value.sppoint = static_cast<std::int16_t>(reader.I16());
  value.accessory = static_cast<std::int16_t>(reader.I16());
  value.shield = static_cast<std::int16_t>(reader.I16());
  value.accessory2 = static_cast<std::int16_t>(reader.I16());
  value.accessory3 = static_cast<std::int16_t>(reader.I16());
  value.headpalette = static_cast<std::int16_t>(reader.I16());
  value.bodypalette = static_cast<std::int16_t>(reader.I16());
  value.name = reader.FixedString(24);
  value.Str = static_cast<std::uint8_t>(reader.U8());
  value.Agi = static_cast<std::uint8_t>(reader.U8());
  value.Vit = static_cast<std::uint8_t>(reader.U8());
  value.Int = static_cast<std::uint8_t>(reader.U8());
  value.Dex = static_cast<std::uint8_t>(reader.U8());
  value.Luk = static_cast<std::uint8_t>(reader.U8());
  value.CharNum = static_cast<std::uint8_t>(reader.U8());
  value.hairColor = static_cast<std::uint8_t>(reader.U8());
  value.bIsChangedCharName = static_cast<std::int16_t>(reader.I16());
  value.mapName = reader.FixedString(16);
  value.DelRevDate = static_cast<std::int32_t>(reader.I32());
  value.robePalette = static_cast<std::int32_t>(reader.I32());
  value.chr_slot_changeCnt = static_cast<std::int32_t>(reader.I32());
  value.chr_name_changeCnt = static_cast<std::int32_t>(reader.I32());
  value.sex = static_cast<std::uint8_t>(reader.U8());
}

void WriteCharacterInfo(ByteWriter& writer, const CharacterInfo& value) {
  writer.U32(static_cast<std::uint32_t>(value.GID));
  writer.I64(static_cast<std::int64_t>(value.exp));
  writer.I32(static_cast<std::int32_t>(value.money));
  writer.I64(static_cast<std::int64_t>(value.jobexp));
  writer.I32(static_cast<std::int32_t>(value.joblevel));
  writer.I32(static_cast<std::int32_t>(value.bodystate));
  writer.I32(static_cast<std::int32_t>(value.healthstate));
  writer.I32(static_cast<std::int32_t>(value.effectstate));
  writer.I32(static_cast<std::int32_t>(value.virtue));
  writer.I32(static_cast<std::int32_t>(value.honor));
  writer.I16(static_cast<std::int16_t>(value.jobpoint));
  writer.I64(static_cast<std::int64_t>(value.hp));
  writer.I64(static_cast<std::int64_t>(value.maxhp));
  writer.I64(static_cast<std::int64_t>(value.sp));
  writer.I64(static_cast<std::int64_t>(value.maxsp));
  writer.I16(static_cast<std::int16_t>(value.speed));
  writer.I16(static_cast<std::int16_t>(value.job));
  writer.I16(static_cast<std::int16_t>(value.head));
  writer.I16(static_cast<std::int16_t>(value.body));
  writer.I16(static_cast<std::int16_t>(value.weapon));
  writer.I16(static_cast<std::int16_t>(value.level));
  writer.I16(static_cast<std::int16_t>(value.sppoint));
  writer.I16(static_cast<std::int16_t>(value.accessory));
  writer.I16(static_cast<std::int16_t>(value.shield));
  writer.I16(static_cast<std::int16_t>(value.accessory2));
  writer.I16(static_cast<std::int16_t>(value.accessory3));
  writer.I16(static_cast<std::int16_t>(value.headpalette));
  writer.I16(static_cast<std::int16_t>(value.bodypalette));
  writer.FixedString(value.name, 24);
  writer.U8(static_cast<std::uint8_t>(value.Str));
  writer.U8(static_cast<std::uint8_t>(value.Agi));
  writer.U8(static_cast<std::uint8_t>(value.Vit));
  writer.U8(static_cast<std::uint8_t>(value.Int));
  writer.U8(static_cast<std::uint8_t>(value.Dex));
  writer.U8(static_cast<std::uint8_t>(value.Luk));
  writer.U8(static_cast<std::uint8_t>(value.CharNum));
  writer.U8(static_cast<std::uint8_t>(value.hairColor));
  writer.I16(static_cast<std::int16_t>(value.bIsChangedCharName));
  writer.FixedString(value.mapName, 16);
  writer.I32(static_cast<std::int32_t>(value.DelRevDate));
  writer.I32(static_cast<std::int32_t>(value.robePalette));
  writer.I32(static_cast<std::int32_t>(value.chr_slot_changeCnt));
  writer.I32(static_cast<std::int32_t>(value.chr_name_changeCnt));
  writer.U8(static_cast<std::uint8_t>(value.sex));
}

void ReadAcAcceptLoginSub(ByteReader& reader, AcAcceptLoginSub& value) {
  value.ip = static_cast<std::uint32_t>(reader.U32());
  value.port = static_cast<std::uint16_t>(reader.U16());
  value.name = reader.FixedString(20);
  value.users = static_cast<std::uint16_t>(reader.U16());
  value.type = static_cast<std::uint16_t>(reader.U16());
  value.new_ = static_cast<std::uint16_t>(reader.U16());
  for (std::uint8_t& element : value.unknown) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
}

void WriteAcAcceptLoginSub(ByteWriter& writer, const AcAcceptLoginSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.ip));
  writer.U16(static_cast<std::uint16_t>(value.port));
  writer.FixedString(value.name, 20);
  writer.U16(static_cast<std::uint16_t>(value.users));
  writer.U16(static_cast<std::uint16_t>(value.type));
  writer.U16(static_cast<std::uint16_t>(value.new_));
  for (const std::uint8_t element : value.unknown) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
}

void ReadHcBlockCharacterSub(ByteReader& reader, HcBlockCharacterSub& value) {
  value.CID = static_cast<std::uint32_t>(reader.U32());
  value.unblock_time = reader.FixedString(20);
}

void WriteHcBlockCharacterSub(ByteWriter& writer, const HcBlockCharacterSub& value) {
  writer.U32(static_cast<std::uint32_t>(value.CID));
  writer.FixedString(value.unblock_time, 20);
}

void ReadHcNotifyAccessibleMapnameSub(ByteReader& reader, HcNotifyAccessibleMapnameSub& value) {
  value.status = static_cast<std::int32_t>(reader.I32());
  value.map = reader.FixedString(16);
}

void WriteHcNotifyAccessibleMapnameSub(ByteWriter& writer,
                                       const HcNotifyAccessibleMapnameSub& value) {
  writer.I32(static_cast<std::int32_t>(value.status));
  writer.FixedString(value.map, 16);
}

}  // namespace

bool Decode(std::span<const std::uint8_t> frame, CaLogin& packet) {
  if (frame.size() != CaLogin::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CaLogin::kOpcode) {
    return false;
  }
  packet.version = static_cast<std::uint32_t>(reader.U32());
  packet.username = reader.FixedString(24);
  packet.password = reader.FixedString(24);
  packet.clienttype = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CaLogin& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CaLogin::kWireSize);
  ByteWriter writer(out);
  writer.U16(CaLogin::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.version));
  writer.FixedString(packet.username, 24);
  writer.FixedString(packet.password, 24);
  writer.U8(static_cast<std::uint8_t>(packet.clienttype));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChSelectChar& packet) {
  if (frame.size() != ChSelectChar::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChSelectChar::kOpcode) {
    return false;
  }
  packet.slot = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChSelectChar& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChSelectChar::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChSelectChar::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.slot));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChMakeChar& packet) {
  if (frame.size() != ChMakeChar::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChMakeChar::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  packet.slot = static_cast<std::uint8_t>(reader.U8());
  packet.hair_color = static_cast<std::uint16_t>(reader.U16());
  packet.hair_style = static_cast<std::uint16_t>(reader.U16());
  packet.job = static_cast<std::uint32_t>(reader.U32());
  packet.sex = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChMakeChar& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChMakeChar::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChMakeChar::kOpcode);
  writer.FixedString(packet.name, 24);
  writer.U8(static_cast<std::uint8_t>(packet.slot));
  writer.U16(static_cast<std::uint16_t>(packet.hair_color));
  writer.U16(static_cast<std::uint16_t>(packet.hair_style));
  writer.U32(static_cast<std::uint32_t>(packet.job));
  writer.U8(static_cast<std::uint8_t>(packet.sex));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChDeleteChar& packet) {
  if (frame.size() != ChDeleteChar::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChDeleteChar::kOpcode) {
    return false;
  }
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.key = reader.FixedString(50);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChDeleteChar& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChDeleteChar::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChDeleteChar::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.FixedString(packet.key, 50);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, AcAcceptLogin& packet) {
  if (frame.size() < AcAcceptLogin::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != AcAcceptLogin::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.login_id1 = static_cast<std::uint32_t>(reader.U32());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.login_id2 = static_cast<std::uint32_t>(reader.U32());
  packet.last_ip = static_cast<std::uint32_t>(reader.U32());
  packet.last_login = reader.FixedString(26);
  packet.sex = static_cast<std::uint8_t>(reader.U8());
  packet.token = reader.FixedString(17);
  if (reader.remaining() % AcAcceptLoginSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.char_servers.resize(reader.remaining() / AcAcceptLoginSub::kWireSize);
    for (AcAcceptLoginSub& element : packet.char_servers) {
      ReadAcAcceptLoginSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const AcAcceptLogin& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(AcAcceptLogin::kWireSize);
  ByteWriter writer(out);
  writer.U16(AcAcceptLogin::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.login_id1));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.login_id2));
  writer.U32(static_cast<std::uint32_t>(packet.last_ip));
  writer.FixedString(packet.last_login, 26);
  writer.U8(static_cast<std::uint8_t>(packet.sex));
  writer.FixedString(packet.token, 17);
  for (const AcAcceptLoginSub& element : packet.char_servers) {
    WriteAcAcceptLoginSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, AcRefuseLogin& packet) {
  if (frame.size() != AcRefuseLogin::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != AcRefuseLogin::kOpcode) {
    return false;
  }
  packet.error = static_cast<std::uint32_t>(reader.U32());
  packet.unblock_time = reader.FixedString(20);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const AcRefuseLogin& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(AcRefuseLogin::kWireSize);
  ByteWriter writer(out);
  writer.U16(AcRefuseLogin::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.error));
  writer.FixedString(packet.unblock_time, 20);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcAcceptEnter& packet) {
  if (frame.size() < HcAcceptEnter::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcAcceptEnter::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.total = static_cast<std::uint8_t>(reader.U8());
  packet.premium_start = static_cast<std::uint8_t>(reader.U8());
  packet.premium_end = static_cast<std::uint8_t>(reader.U8());
  packet.extension = reader.FixedString(20);
  if (reader.remaining() % CharacterInfo::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.characters.resize(reader.remaining() / CharacterInfo::kWireSize);
    for (CharacterInfo& element : packet.characters) {
      ReadCharacterInfo(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcAcceptEnter& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcAcceptEnter::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcAcceptEnter::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.total));
  writer.U8(static_cast<std::uint8_t>(packet.premium_start));
  writer.U8(static_cast<std::uint8_t>(packet.premium_end));
  writer.FixedString(packet.extension, 20);
  for (const CharacterInfo& element : packet.characters) {
    WriteCharacterInfo(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcRefuseEnter& packet) {
  if (frame.size() != HcRefuseEnter::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcRefuseEnter::kOpcode) {
    return false;
  }
  packet.error = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcRefuseEnter& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcRefuseEnter::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcRefuseEnter::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.error));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcAcceptMakechar& packet) {
  if (frame.size() != HcAcceptMakechar::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcAcceptMakechar::kOpcode) {
    return false;
  }
  ReadCharacterInfo(reader, packet.character);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcAcceptMakechar& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcAcceptMakechar::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcAcceptMakechar::kOpcode);
  WriteCharacterInfo(writer, packet.character);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcRefuseMakechar& packet) {
  if (frame.size() != HcRefuseMakechar::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcRefuseMakechar::kOpcode) {
    return false;
  }
  packet.error = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcRefuseMakechar& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcRefuseMakechar::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcRefuseMakechar::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.error));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcAcceptDeletechar& /*packet*/) {
  if (frame.size() != HcAcceptDeletechar::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcAcceptDeletechar::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcAcceptDeletechar& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(HcAcceptDeletechar::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcAcceptDeletechar::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcRefuseDeletechar& packet) {
  if (frame.size() != HcRefuseDeletechar::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcRefuseDeletechar::kOpcode) {
    return false;
  }
  packet.error = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcRefuseDeletechar& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcRefuseDeletechar::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcRefuseDeletechar::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.error));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcNotifyZonesvr& packet) {
  if (frame.size() != HcNotifyZonesvr::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcNotifyZonesvr::kOpcode) {
    return false;
  }
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.mapname = reader.FixedString(16);
  packet.ip = static_cast<std::uint32_t>(reader.U32());
  packet.port = static_cast<std::uint16_t>(reader.U16());
  packet.domain = reader.FixedString(128);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcNotifyZonesvr& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcNotifyZonesvr::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcNotifyZonesvr::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.FixedString(packet.mapname, 16);
  writer.U32(static_cast<std::uint32_t>(packet.ip));
  writer.U16(static_cast<std::uint16_t>(packet.port));
  writer.FixedString(packet.domain, 128);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ScNotifyBan& packet) {
  if (frame.size() != ScNotifyBan::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ScNotifyBan::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ScNotifyBan& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ScNotifyBan::kWireSize);
  ByteWriter writer(out);
  writer.U16(ScNotifyBan::kOpcode);
  writer.U8(static_cast<std::uint8_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, Ping& packet) {
  if (frame.size() != Ping::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != Ping::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const Ping& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(Ping::kWireSize);
  ByteWriter writer(out);
  writer.U16(Ping::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CaReqHash& /*packet*/) {
  if (frame.size() != CaReqHash::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CaReqHash::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CaReqHash& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(CaReqHash::kWireSize);
  ByteWriter writer(out);
  writer.U16(CaReqHash::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, AcAckHash& packet) {
  if (frame.size() < AcAckHash::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != AcAckHash::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.salt = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const AcAckHash& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(AcAckHash::kWireSize);
  ByteWriter writer(out);
  writer.U16(AcAckHash::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.Text(packet.salt);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CaLogin2& packet) {
  if (frame.size() != CaLogin2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CaLogin2::kOpcode) {
    return false;
  }
  packet.version = static_cast<std::uint32_t>(reader.U32());
  packet.username = reader.FixedString(24);
  for (std::uint8_t& element : packet.passwordMD5) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  packet.clienttype = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CaLogin2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CaLogin2::kWireSize);
  ByteWriter writer(out);
  writer.U16(CaLogin2::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.version));
  writer.FixedString(packet.username, 24);
  for (const std::uint8_t element : packet.passwordMD5) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  writer.U8(static_cast<std::uint8_t>(packet.clienttype));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CaLogin3& packet) {
  if (frame.size() != CaLogin3::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CaLogin3::kOpcode) {
    return false;
  }
  packet.version = static_cast<std::uint32_t>(reader.U32());
  packet.username = reader.FixedString(24);
  for (std::uint8_t& element : packet.passwordMD5) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  packet.clienttype = static_cast<std::uint8_t>(reader.U8());
  packet.clientinfo = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CaLogin3& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CaLogin3::kWireSize);
  ByteWriter writer(out);
  writer.U16(CaLogin3::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.version));
  writer.FixedString(packet.username, 24);
  for (const std::uint8_t element : packet.passwordMD5) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  writer.U8(static_cast<std::uint8_t>(packet.clienttype));
  writer.U8(static_cast<std::uint8_t>(packet.clientinfo));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CaConnectInfoChanged& packet) {
  if (frame.size() != CaConnectInfoChanged::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CaConnectInfoChanged::kOpcode) {
    return false;
  }
  packet.name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CaConnectInfoChanged& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CaConnectInfoChanged::kWireSize);
  ByteWriter writer(out);
  writer.U16(CaConnectInfoChanged::kOpcode);
  writer.FixedString(packet.name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CaExeHashcheck& packet) {
  if (frame.size() != CaExeHashcheck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CaExeHashcheck::kOpcode) {
    return false;
  }
  packet.hash = reader.FixedString(16);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CaExeHashcheck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CaExeHashcheck::kWireSize);
  ByteWriter writer(out);
  writer.U16(CaExeHashcheck::kOpcode);
  writer.FixedString(packet.hash, 16);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcBlockCharacter& packet) {
  if (frame.size() < HcBlockCharacter::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcBlockCharacter::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % HcBlockCharacterSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.characters.resize(reader.remaining() / HcBlockCharacterSub::kWireSize);
    for (HcBlockCharacterSub& element : packet.characters) {
      ReadHcBlockCharacterSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcBlockCharacter& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcBlockCharacter::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcBlockCharacter::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const HcBlockCharacterSub& element : packet.characters) {
    WriteHcBlockCharacterSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CaLoginPcbang& packet) {
  if (frame.size() != CaLoginPcbang::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CaLoginPcbang::kOpcode) {
    return false;
  }
  packet.version = static_cast<std::uint32_t>(reader.U32());
  packet.username = reader.FixedString(24);
  packet.password = reader.FixedString(24);
  packet.clienttype = static_cast<std::uint8_t>(reader.U8());
  packet.ip = reader.FixedString(16);
  packet.mac = reader.FixedString(13);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CaLoginPcbang& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CaLoginPcbang::kWireSize);
  ByteWriter writer(out);
  writer.U16(CaLoginPcbang::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.version));
  writer.FixedString(packet.username, 24);
  writer.FixedString(packet.password, 24);
  writer.U8(static_cast<std::uint8_t>(packet.clienttype));
  writer.FixedString(packet.ip, 16);
  writer.FixedString(packet.mac, 13);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CaLogin4& packet) {
  if (frame.size() != CaLogin4::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CaLogin4::kOpcode) {
    return false;
  }
  packet.version = static_cast<std::uint32_t>(reader.U32());
  packet.username = reader.FixedString(24);
  for (std::uint8_t& element : packet.passwordMD5) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  packet.clienttype = static_cast<std::uint8_t>(reader.U8());
  packet.mac = reader.FixedString(13);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CaLogin4& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CaLogin4::kWireSize);
  ByteWriter writer(out);
  writer.U16(CaLogin4::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.version));
  writer.FixedString(packet.username, 24);
  for (const std::uint8_t element : packet.passwordMD5) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  writer.U8(static_cast<std::uint8_t>(packet.clienttype));
  writer.FixedString(packet.mac, 13);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChReqIsValidCharname& packet) {
  if (frame.size() != ChReqIsValidCharname::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChReqIsValidCharname::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.new_name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChReqIsValidCharname& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChReqIsValidCharname::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChReqIsValidCharname::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.FixedString(packet.new_name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcAckIsValidCharname& packet) {
  if (frame.size() != HcAckIsValidCharname::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcAckIsValidCharname::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcAckIsValidCharname& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcAckIsValidCharname::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcAckIsValidCharname::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChReqChangeCharname& packet) {
  if (frame.size() != ChReqChangeCharname::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChReqChangeCharname::kOpcode) {
    return false;
  }
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.new_name = reader.FixedString(24);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChReqChangeCharname& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChReqChangeCharname::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChReqChangeCharname::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.FixedString(packet.new_name, 24);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcAckChangeCharname& packet) {
  if (frame.size() != HcAckChangeCharname::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcAckChangeCharname::kOpcode) {
    return false;
  }
  packet.result = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcAckChangeCharname& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcAckChangeCharname::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcAckChangeCharname::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CaLoginChannel& packet) {
  if (frame.size() != CaLoginChannel::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CaLoginChannel::kOpcode) {
    return false;
  }
  packet.version = static_cast<std::uint32_t>(reader.U32());
  packet.username = reader.FixedString(24);
  packet.password = reader.FixedString(24);
  packet.clienttype = static_cast<std::uint8_t>(reader.U8());
  packet.ip = reader.FixedString(16);
  packet.mac = reader.FixedString(13);
  packet.is_gravity = static_cast<std::uint8_t>(reader.U8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CaLoginChannel& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CaLoginChannel::kWireSize);
  ByteWriter writer(out);
  writer.U16(CaLoginChannel::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.version));
  writer.FixedString(packet.username, 24);
  writer.FixedString(packet.password, 24);
  writer.U8(static_cast<std::uint8_t>(packet.clienttype));
  writer.FixedString(packet.ip, 16);
  writer.FixedString(packet.mac, 13);
  writer.U8(static_cast<std::uint8_t>(packet.is_gravity));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CaSsoLoginReq& packet) {
  if (frame.size() < CaSsoLoginReq::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CaSsoLoginReq::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.version = static_cast<std::uint32_t>(reader.U32());
  packet.clienttype = static_cast<std::uint8_t>(reader.U8());
  packet.username = reader.FixedString(24);
  packet.password = reader.FixedString(27);
  packet.mac = reader.FixedString(17);
  packet.ip = reader.FixedString(15);
  packet.token = reader.TailString();
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CaSsoLoginReq& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CaSsoLoginReq::kWireSize);
  ByteWriter writer(out);
  writer.U16(CaSsoLoginReq::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.version));
  writer.U8(static_cast<std::uint8_t>(packet.clienttype));
  writer.FixedString(packet.username, 24);
  writer.FixedString(packet.password, 27);
  writer.FixedString(packet.mac, 17);
  writer.FixedString(packet.ip, 15);
  writer.Text(packet.token);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChDeleteChar3Reserved& packet) {
  if (frame.size() != ChDeleteChar3Reserved::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChDeleteChar3Reserved::kOpcode) {
    return false;
  }
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChDeleteChar3Reserved& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChDeleteChar3Reserved::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChDeleteChar3Reserved::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcDeleteChar3Reserved& packet) {
  if (frame.size() != HcDeleteChar3Reserved::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcDeleteChar3Reserved::kOpcode) {
    return false;
  }
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.result = static_cast<std::int32_t>(reader.I32());
  packet.date = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcDeleteChar3Reserved& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcDeleteChar3Reserved::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcDeleteChar3Reserved::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.I32(static_cast<std::int32_t>(packet.result));
  writer.U32(static_cast<std::uint32_t>(packet.date));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChDeleteChar3& packet) {
  if (frame.size() != ChDeleteChar3::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChDeleteChar3::kOpcode) {
    return false;
  }
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.birthdate = reader.FixedString(6);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChDeleteChar3& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChDeleteChar3::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChDeleteChar3::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.FixedString(packet.birthdate, 6);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcDeleteChar3& packet) {
  if (frame.size() != HcDeleteChar3::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcDeleteChar3::kOpcode) {
    return false;
  }
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.result = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcDeleteChar3& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcDeleteChar3::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcDeleteChar3::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.I32(static_cast<std::int32_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChDeleteChar3Cancel& packet) {
  if (frame.size() != ChDeleteChar3Cancel::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChDeleteChar3Cancel::kOpcode) {
    return false;
  }
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChDeleteChar3Cancel& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChDeleteChar3Cancel::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChDeleteChar3Cancel::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcDeleteChar3Cancel& packet) {
  if (frame.size() != HcDeleteChar3Cancel::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcDeleteChar3Cancel::kOpcode) {
    return false;
  }
  packet.CID = static_cast<std::uint32_t>(reader.U32());
  packet.result = static_cast<std::int32_t>(reader.I32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcDeleteChar3Cancel& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcDeleteChar3Cancel::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcDeleteChar3Cancel::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.CID));
  writer.I32(static_cast<std::int32_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcAcceptEnter2& packet) {
  if (frame.size() < HcAcceptEnter2::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcAcceptEnter2::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.normal = static_cast<std::uint8_t>(reader.U8());
  packet.premium = static_cast<std::uint8_t>(reader.U8());
  packet.billing = static_cast<std::uint8_t>(reader.U8());
  packet.producible = static_cast<std::uint8_t>(reader.U8());
  packet.total = static_cast<std::uint8_t>(reader.U8());
  packet.extension = reader.FixedString(20);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcAcceptEnter2& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcAcceptEnter2::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcAcceptEnter2::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U8(static_cast<std::uint8_t>(packet.normal));
  writer.U8(static_cast<std::uint8_t>(packet.premium));
  writer.U8(static_cast<std::uint8_t>(packet.billing));
  writer.U8(static_cast<std::uint8_t>(packet.producible));
  writer.U8(static_cast<std::uint8_t>(packet.total));
  writer.FixedString(packet.extension, 20);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcNotifyAccessibleMapname& packet) {
  if (frame.size() < HcNotifyAccessibleMapname::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcNotifyAccessibleMapname::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % HcNotifyAccessibleMapnameSub::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.maps.resize(reader.remaining() / HcNotifyAccessibleMapnameSub::kWireSize);
    for (HcNotifyAccessibleMapnameSub& element : packet.maps) {
      ReadHcNotifyAccessibleMapnameSub(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcNotifyAccessibleMapname& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcNotifyAccessibleMapname::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcNotifyAccessibleMapname::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const HcNotifyAccessibleMapnameSub& element : packet.maps) {
    WriteHcNotifyAccessibleMapnameSub(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChSelectAccessibleMapname& packet) {
  if (frame.size() != ChSelectAccessibleMapname::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChSelectAccessibleMapname::kOpcode) {
    return false;
  }
  packet.slot = static_cast<std::int8_t>(reader.I8());
  packet.mapnumber = static_cast<std::int8_t>(reader.I8());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChSelectAccessibleMapname& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChSelectAccessibleMapname::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChSelectAccessibleMapname::kOpcode);
  writer.I8(static_cast<std::int8_t>(packet.slot));
  writer.I8(static_cast<std::int8_t>(packet.mapnumber));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChSecondPasswdAck& packet) {
  if (frame.size() != ChSecondPasswdAck::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChSecondPasswdAck::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.pin = reader.FixedString(4);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChSecondPasswdAck& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChSecondPasswdAck::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChSecondPasswdAck::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.FixedString(packet.pin, 4);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcSecondPasswdLogin& packet) {
  if (frame.size() != HcSecondPasswdLogin::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcSecondPasswdLogin::kOpcode) {
    return false;
  }
  packet.seed = static_cast<std::uint32_t>(reader.U32());
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.result = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcSecondPasswdLogin& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcSecondPasswdLogin::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcSecondPasswdLogin::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.seed));
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.U16(static_cast<std::uint16_t>(packet.result));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChMakeSecondPasswd& packet) {
  if (frame.size() != ChMakeSecondPasswd::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChMakeSecondPasswd::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.pin = reader.FixedString(4);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChMakeSecondPasswd& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChMakeSecondPasswd::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChMakeSecondPasswd::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.FixedString(packet.pin, 4);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChEditSecondPasswd& packet) {
  if (frame.size() != ChEditSecondPasswd::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChEditSecondPasswd::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  packet.old_pin = reader.FixedString(4);
  packet.new_pin = reader.FixedString(4);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChEditSecondPasswd& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChEditSecondPasswd::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChEditSecondPasswd::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  writer.FixedString(packet.old_pin, 4);
  writer.FixedString(packet.new_pin, 4);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChAvailableSecondPasswd& packet) {
  if (frame.size() != ChAvailableSecondPasswd::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChAvailableSecondPasswd::kOpcode) {
    return false;
  }
  packet.AID = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChAvailableSecondPasswd& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChAvailableSecondPasswd::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChAvailableSecondPasswd::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.AID));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChReqChangeCharacterSlot& packet) {
  if (frame.size() != ChReqChangeCharacterSlot::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChReqChangeCharacterSlot::kOpcode) {
    return false;
  }
  packet.slot_before = static_cast<std::uint16_t>(reader.U16());
  packet.slot_after = static_cast<std::uint16_t>(reader.U16());
  packet.remaining = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChReqChangeCharacterSlot& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(ChReqChangeCharacterSlot::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChReqChangeCharacterSlot::kOpcode);
  writer.U16(static_cast<std::uint16_t>(packet.slot_before));
  writer.U16(static_cast<std::uint16_t>(packet.slot_after));
  writer.U16(static_cast<std::uint16_t>(packet.remaining));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcAckChangeCharacterSlot& packet) {
  if (frame.size() < HcAckChangeCharacterSlot::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcAckChangeCharacterSlot::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.reason = static_cast<std::uint16_t>(reader.U16());
  packet.moves = static_cast<std::uint16_t>(reader.U16());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcAckChangeCharacterSlot& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcAckChangeCharacterSlot::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcAckChangeCharacterSlot::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U16(static_cast<std::uint16_t>(packet.reason));
  writer.U16(static_cast<std::uint16_t>(packet.moves));
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcAckCharinfoPerPage& packet) {
  if (frame.size() < HcAckCharinfoPerPage::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcAckCharinfoPerPage::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  if (reader.remaining() % CharacterInfo::kWireSize != 0) {
    reader.Invalidate();
  } else {
    packet.characters.resize(reader.remaining() / CharacterInfo::kWireSize);
    for (CharacterInfo& element : packet.characters) {
      ReadCharacterInfo(reader, element);
    }
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcAckCharinfoPerPage& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcAckCharinfoPerPage::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcAckCharinfoPerPage::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  for (const CharacterInfo& element : packet.characters) {
    WriteCharacterInfo(writer, element);
  }
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, HcCharlistNotify& packet) {
  if (frame.size() != HcCharlistNotify::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != HcCharlistNotify::kOpcode) {
    return false;
  }
  packet.total = static_cast<std::uint32_t>(reader.U32());
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const HcCharlistNotify& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(HcCharlistNotify::kWireSize);
  ByteWriter writer(out);
  writer.U16(HcCharlistNotify::kOpcode);
  writer.U32(static_cast<std::uint32_t>(packet.total));
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, ChCharlistReq& /*packet*/) {
  if (frame.size() != ChCharlistReq::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != ChCharlistReq::kOpcode) {
    return false;
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const ChCharlistReq& /*packet*/) {
  std::vector<std::uint8_t> out;
  out.reserve(ChCharlistReq::kWireSize);
  ByteWriter writer(out);
  writer.U16(ChCharlistReq::kOpcode);
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, CtAuth& packet) {
  if (frame.size() != CtAuth::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != CtAuth::kOpcode) {
    return false;
  }
  for (std::uint8_t& element : packet.unknown) {
    element = static_cast<std::uint8_t>(reader.U8());
  }
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const CtAuth& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(CtAuth::kWireSize);
  ByteWriter writer(out);
  writer.U16(CtAuth::kOpcode);
  for (const std::uint8_t element : packet.unknown) {
    writer.U8(static_cast<std::uint8_t>(element));
  }
  return out;
}

bool Decode(std::span<const std::uint8_t> frame, TcResult& packet) {
  if (frame.size() < TcResult::kWireSize) {
    return false;
  }
  ByteReader reader(frame);
  if (reader.U16() != TcResult::kOpcode) {
    return false;
  }
  // La longueur est déjà connue : c'est celle de la trame.
  reader.Skip(2);
  packet.type = static_cast<std::uint32_t>(reader.U32());
  packet.unknown1 = reader.FixedString(20);
  packet.unknown2 = reader.FixedString(6);
  return reader.ok();
}

std::vector<std::uint8_t> Encode(const TcResult& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(TcResult::kWireSize);
  ByteWriter writer(out);
  writer.U16(TcResult::kOpcode);
  // Réservée : la longueur ne sera connue qu'une fois la queue écrite.
  writer.U16(0);
  writer.U32(static_cast<std::uint32_t>(packet.type));
  writer.FixedString(packet.unknown1, 20);
  writer.FixedString(packet.unknown2, 6);
  if (writer.size() > kMaxFrameSize ||
      !writer.PatchU16(kLengthOffset, static_cast<std::uint16_t>(writer.size()))) {
    return {};
  }
  return out;
}

}  // namespace ro::proto
