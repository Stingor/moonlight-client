// La fiche d'un monstre : la demande, la réponse du serveur, et le relevé de
// Sense. Voir l'en-tête pour ce que chacun porte.

#include "ro/net/monster_sheet.h"

#include <string>
#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

// Le genre d'un butin : zéro pour l'ordinaire, un pour la récompense de MVP.
constexpr std::uint8_t kDropKindMvp = 1;

// Les deux indicateurs de l'EXP estimée : niveau de base, puis de job, au
// maximum.
constexpr std::uint8_t kBaseLevelMaxedBit = 0x01;
constexpr std::uint8_t kJobLevelMaxedBit = 0x02;

// Un texte préfixé de sa longueur sur un octet. Vide, et le lecteur en échec,
// quand la trame n'en porte pas autant.
//
// Les noms viennent du JEU, donc de sa page de codes.
//
// ⚠ TRONQUÉ AU PREMIER NUL : un nom de la base du serveur peut porter son
// remplissage dans la longueur annoncée — c'est arrivé sur la liste des
// sources d'un objet, où il se dessinait en glyphes « caractère manquant ».
[[nodiscard]] std::string LengthPrefixed(core::ByteReader& reader) {
  const std::size_t length = reader.U8();
  return core::Utf8FromCp1252(std::string(reader.FixedString(length)));
}

void ReadFixedBlock(core::ByteReader& reader, sim::MonsterSheet& sheet) {
  sheet.sprite_class = reader.U32();
  sheet.level = reader.U16();
  sheet.hp = reader.U32();
  sheet.sp = reader.U32();
  sheet.base_exp = reader.U32();
  sheet.job_exp = reader.U32();
  sheet.mvp_exp = reader.U32();
  sheet.atk_min = reader.U16();
  sheet.atk_max = reader.U16();
  sheet.matk_min = reader.U16();
  sheet.matk_max = reader.U16();
  sheet.def = reader.U16();
  sheet.def_bonus = reader.U16();
  sheet.mdef = reader.U16();
  sheet.mdef_bonus = reader.U16();
  sheet.str = reader.U16();
  sheet.agi = reader.U16();
  sheet.vit = reader.U16();
  sheet.int_stat = reader.U16();
  sheet.dex = reader.U16();
  sheet.luk = reader.U16();
  sheet.attack_range_cells = reader.U16();
  sheet.size = reader.U8();
  sheet.race = reader.U8();
  sheet.element = reader.U8();
  sheet.element_level = reader.U8();
  sheet.boss_rank = reader.U8();
  sheet.mob_class = reader.U8();
  sheet.mode = reader.U32();
  sheet.walk_ms_per_cell = reader.U16();
  for (std::int16_t& resist : sheet.resist_percent) {
    resist = reader.I16();
  }
}

// Les trois listes, dans l'ordre du serveur. Chacune s'arrête à la première
// entrée qui ne tient pas entière : ce qui précède reste bon.
void ReadDrops(core::ByteReader& reader, sim::MonsterSheet& sheet) {
  if (reader.remaining() < 1) {
    return;
  }
  const std::size_t count = reader.U8();
  for (std::size_t rank = 0; rank < count && reader.remaining() >= kMonsterDropHeadSize; ++rank) {
    sim::MonsterDrop drop;
    drop.item_id = reader.U32();
    drop.rate_hundredths = reader.U32();
    drop.mvp_reward = reader.U8() == kDropKindMvp;
    drop.name = LengthPrefixed(reader);
    if (!reader.ok()) {
      return;
    }
    sheet.drops.push_back(std::move(drop));
  }
}

void ReadSpawns(core::ByteReader& reader, sim::MonsterSheet& sheet) {
  if (reader.remaining() < 1) {
    return;
  }
  const std::size_t count = reader.U8();
  for (std::size_t rank = 0; rank < count && reader.remaining() >= kMonsterSpawnHeadSize; ++rank) {
    sim::MonsterSpawn spawn;
    spawn.count = reader.U16();
    spawn.map_name = LengthPrefixed(reader);
    if (!reader.ok()) {
      return;
    }
    sheet.spawns.push_back(std::move(spawn));
  }
}

void ReadSkills(core::ByteReader& reader, sim::MonsterSheet& sheet) {
  if (reader.remaining() < 1) {
    return;
  }
  const std::size_t count = reader.U8();
  for (std::size_t rank = 0; rank < count && reader.remaining() >= kMonsterSkillHeadSize; ++rank) {
    sim::MonsterSkill skill;
    skill.skill_id = reader.U16();
    skill.level = reader.U16();
    skill.server_name = LengthPrefixed(reader);
    if (!reader.ok()) {
      return;
    }
    sheet.skills.push_back(std::move(skill));
  }
}

// ⚠ LES DEUX QUEUES SONT FACULTATIVES : un serveur qui ne les envoie pas
// laisse les valeurs par défaut, et la fiche se tait sur ce qu'elles disent.
void ReadIdentity(core::ByteReader& reader, sim::MonsterSheet& sheet) {
  if (reader.remaining() < 1) {
    return;
  }
  const std::size_t length = reader.U8();
  if (reader.remaining() < length) {
    return;
  }
  sheet.aegis_name = reader.FixedString(length);
  if (reader.remaining() < kMonsterIdentityTailSize) {
    return;
  }
  sheet.summoned = reader.U8() != 0;
  sheet.namesake_count = reader.U8();
  sheet.namesake_origin = reader.U32();
}

void ReadExpGain(core::ByteReader& reader, sim::MonsterSheet& sheet) {
  if (reader.remaining() < kMonsterExpGainSize) {
    return;
  }
  sim::MonsterExpGain gain;
  gain.base_exp = reader.U32();
  gain.job_exp = reader.U32();
  gain.next_base_exp = reader.U32();
  gain.next_job_exp = reader.U32();
  const std::uint8_t flags = reader.U8();
  gain.base_level_maxed = (flags & kBaseLevelMaxedBit) != 0;
  gain.job_level_maxed = (flags & kJobLevelMaxedBit) != 0;
  sheet.exp_gain = gain;
}

[[nodiscard]] FrameOutcome ApplySheetFrame(const Frame& frame, sim::MonsterSheetBook& book) {
  const std::optional<MonsterSheetReply> reply = DecodeMonsterSheet(frame.bytes);
  if (!reply.has_value()) {
    return FrameOutcome::kUndecodable;
  }
  if (reply->sheet.has_value()) {
    book.Receive(reply->asked_id, *reply->sheet);
  } else {
    book.NoteUnknown(reply->asked_id);
  }
  return FrameOutcome::kApplied;
}

[[nodiscard]] FrameOutcome ApplySenseFrame(const Frame& frame, sim::MonsterSheetBook& book) {
  proto::ZcMonsterInfo packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  sim::SenseReading reading;
  reading.sprite_class = packet.class_;
  reading.level = packet.level;
  reading.size = packet.size;
  reading.hp = packet.hp;
  reading.def = packet.def;
  reading.race = packet.race;
  reading.mdef = packet.mdef;
  reading.element = packet.element;
  reading.resist_percent = {packet.water, packet.earth,  packet.fire,  packet.wind,  packet.poison,
                            packet.holy,  packet.shadow, packet.ghost, packet.undead};
  book.ReceiveSense(reading);
  return FrameOutcome::kApplied;
}

}  // namespace

std::optional<MonsterSheetReply> DecodeMonsterSheet(std::span<const std::uint8_t> frame) {
  proto::ZcMoonlightMobinfo packet;
  if (!proto::Decode(frame, packet)) {
    return std::nullopt;
  }
  MonsterSheetReply reply;
  reply.asked_id = packet.mob_id;
  if (packet.status != kMonsterSheetFound) {
    return reply;
  }
  core::ByteReader reader(frame);
  reader.Seek(kMonsterSheetHeaderSize);
  // ⚠ UN BLOC FIXE INCOMPLET SE LIT COMME « INCONNU » : une fiche sans ses
  // statistiques n'en est pas une, et redemander ne rendrait pas mieux.
  if (reader.remaining() < kMonsterSheetFixedSize) {
    return reply;
  }
  sim::MonsterSheet sheet;
  ReadFixedBlock(reader, sheet);
  // Le nom affiché, puis les listes. ⚠ UN NOM QUI NE TIENT PAS laisse la
  // fiche sans nom plutôt que de la refuser : les chiffres restent justes.
  if (reader.remaining() >= 1) {
    const std::size_t length = reader.U8();
    if (reader.remaining() >= length) {
      sheet.name = core::Utf8FromCp1252(std::string(reader.FixedString(length)));
    }
  }
  ReadDrops(reader, sheet);
  ReadSpawns(reader, sheet);
  ReadSkills(reader, sheet);
  if (reader.ok()) {
    ReadIdentity(reader, sheet);
  }
  if (reader.ok()) {
    ReadExpGain(reader, sheet);
  }
  reply.sheet = std::move(sheet);
  return reply;
}

core::Result<void> RequestMonsterSheet(Connection& link, std::uint32_t monster_id, bool by_view) {
  proto::CzMoonlightReqMobinfo request;
  // ⚠ LA TAILLE EST ÉCRITE, le serveur la LIT.
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightReqMobinfo::kWireSize);
  request.mob_id = monster_id;
  request.by_view = by_view ? 1 : 0;
  link.Send(proto::Encode(request));
  return {};
}

FrameOutcome ApplyMonsterSheetFrame(const Frame& frame, sim::MonsterSheetBook& book) {
  if (frame.opcode == proto::ZcMoonlightMobinfo::kOpcode) {
    return ApplySheetFrame(frame, book);
  }
  if (frame.opcode == proto::ZcMonsterInfo::kOpcode) {
    return ApplySenseFrame(frame, book);
  }
  return FrameOutcome::kUnhandled;
}

}  // namespace ro::net
