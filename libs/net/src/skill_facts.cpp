// Ce que le serveur seul sait d'une compétence : son cast, et ses dégâts.
// Voir l'en-tête pour ce que ces quatre paquets sont seuls à porter.

#include "ro/net/skill_facts.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/skill_facts.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// ⚠ LA PORTÉE D'UNE DEMANDE DE COMPÉTENCE VAUT ZÉRO, toujours : le serveur ne
// range par portée que les sources d'un OBJET — les monstres ordinaires d'un
// côté, les récompenses de boss de l'autre. Une compétence n'a qu'une table.
constexpr std::uint8_t kSkillScope = 0;

// ⚠ LE SERVEUR DEMANDE QU'ON DISE SI L'ON PARLE D'UNE COMPÉTENCE : le même
// paquet sert aux objets, et c'est cet octet qui décide de la forme de sa
// réponse.
constexpr std::uint8_t kAsksAboutSkill = 1;

[[nodiscard]] FrameOutcome ApplyCastFrame(const Frame& frame, sim::World& world) {
  proto::ZcMoonlightTechdata packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  // ⚠ LA RÉPONSE D'UN OBJET PASSE PAR LE MÊME OPCODE, et elle n'est pas pour
  // nous : ses entrées sont des monstres, pas des rangs. On la laisse passer
  // plutôt que de la lire de travers.
  if (packet.is_skill == 0) {
    return FrameOutcome::kApplied;
  }

  core::ByteReader reader(frame.bytes);
  reader.Seek(kSkillFactsFixedSize);
  if (reader.remaining() < kSkillCastCountSize) {
    return FrameOutcome::kUndecodable;
  }
  const std::size_t announced = reader.U8();
  // 🔴 LE COMPTE EST CRU, MAIS PAS SUR PAROLE : une trame tronquée en
  // annoncerait plus qu'elle n'en porte.
  const std::size_t fits = reader.remaining() / kSkillCastEntrySize;
  const std::size_t count = std::min(announced, fits);

  std::vector<sim::SkillCastFacts> levels;
  levels.reserve(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    sim::SkillCastFacts facts;
    facts.cast_variable_ms = reader.I32();
    facts.cast_fixed_ms = reader.I32();
    facts.cooldown_ms = reader.I32();
    facts.after_delay_ms = reader.I32();
    levels.push_back(facts);
  }
  if (!reader.ok()) {
    return FrameOutcome::kUndecodable;
  }
  // ⚠ ON POSE MÊME UNE TABLE VIDE : c'est la réponse « je n'ai rien », et elle
  // doit effacer ce qu'on croyait savoir.
  world.TellSkillCastFacts(packet.id, std::move(levels));
  return FrameOutcome::kApplied;
}

[[nodiscard]] FrameOutcome ApplyDamageFrame(const Frame& frame, sim::World& world) {
  proto::ZcMoonlightDamage packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  sim::SkillDamageFacts facts;
  facts.skill_id = packet.skill_id;
  facts.level = packet.skill_lv;
  facts.target_mob_id = packet.target_mob_id;
  facts.status = packet.status;
  facts.attack_kind = packet.atk_type;
  facts.hits = packet.hits;
  facts.least = packet.dmg_min;
  facts.most = packet.dmg_max;
  facts.average = packet.dmg_avg;

  // ⚠ LE NOM EST FACULTATIF : le mannequin neutre et le miroir n'en ont pas, et
  // son absence n'est pas une trame tronquée.
  core::ByteReader reader(frame.bytes);
  reader.Seek(kSkillDamageFixedSize);
  if (reader.remaining() >= 1) {
    const std::size_t length = reader.U8();
    if (reader.remaining() >= length) {
      const std::span<const std::uint8_t> raw = reader.Bytes(length);
      // Le nom vient du JEU, donc de sa page de codes.
      facts.target_name =
          core::Utf8FromCp1252(std::string(reinterpret_cast<const char*>(raw.data()), raw.size()));
    }
  }
  if (!reader.ok()) {
    return FrameOutcome::kUndecodable;
  }
  world.TellSkillDamageFacts(std::move(facts));
  return FrameOutcome::kApplied;
}

}  // namespace

core::Result<void> RequestSkillCastFacts(Connection& link, std::uint32_t skill_id) {
  proto::CzMoonlightReqTechdata request;
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightReqTechdata::kWireSize);
  request.id = skill_id;
  request.is_skill = kAsksAboutSkill;
  request.scope = kSkillScope;
  // ⚠ CES DEUX-LÀ S'ENCODENT SANS POUVOIR ÉCHOUER : leur taille est FIXE, et le
  // générateur rend donc la trame elle-même plutôt qu'un résultat. C'est ce qui
  // les sépare des paquets à liste, dont la longueur peut déborder.
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestSkillDamage(Connection& link, std::uint32_t skill_id, std::uint16_t level,
                                      std::uint32_t target_mob_id) {
  proto::CzMoonlightReqDamage request;
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightReqDamage::kWireSize);
  request.skill_id = skill_id;
  request.skill_lv = level;
  request.target_mob_id = target_mob_id;
  // ⚠ CES DEUX-LÀ S'ENCODENT SANS POUVOIR ÉCHOUER : leur taille est FIXE, et le
  // générateur rend donc la trame elle-même plutôt qu'un résultat. C'est ce qui
  // les sépare des paquets à liste, dont la longueur peut déborder.
  link.Send(proto::Encode(request));
  return {};
}

FrameOutcome ApplySkillFactsFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode == proto::ZcMoonlightTechdata::kOpcode) {
    return ApplyCastFrame(frame, world);
  }
  if (frame.opcode == proto::ZcMoonlightDamage::kOpcode) {
    return ApplyDamageFrame(frame, world);
  }
  return FrameOutcome::kUnhandled;
}

}  // namespace ro::net
