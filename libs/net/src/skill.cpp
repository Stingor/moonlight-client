#include "ro/net/skill.h"

#include <string>
#include <utility>

#include "ro/core/text.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/skill_text_packets.h"

#include "send_encoded.h"

namespace ro::net {
namespace {

// Ce qu'une ligne du serveur devient dans le grimoire.
//
// Le serveur envoie la même structure dans la liste entière et dans l'ajout
// d'une compétence seule ; la traduire deux fois inviterait les deux à
// diverger.
sim::KnownSkill SkillFrom(const proto::Skilldata& data) {
  sim::KnownSkill skill;
  skill.id = data.id;
  skill.level = data.level;
  skill.sp_cost = data.sp;
  skill.range_cells = data.range2;
  // Le serveur écrit les drapeaux sur quatre octets signés, et n'en emploie
  // que six bits : e_skill_inf tient sur seize.
  skill.targeting = static_cast<std::uint16_t>(data.inf);
  // Le nom vient du JEU, donc de sa page de codes — même si celui d'une
  // compétence n'a jamais rien d'accentué.
  skill.name = core::Utf8FromCp1252(data.name);
  skill.upgradable = data.upFlag != 0;
  return skill;
}

}  // namespace

core::Result<void> RequestSkillOnTarget(Connection& link, std::uint16_t skill_id,
                                        std::uint16_t skill_level, std::uint32_t target_id) {
  proto::SkillOnTargetRequest request;
  request.skill_id = skill_id;
  request.skill_level = skill_level;
  request.target_id = target_id;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestSkillUp(Connection& link, std::uint16_t skill_id) {
  proto::SkillUpRequest request;
  request.skill_id = skill_id;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestSkillOnGround(Connection& link, std::uint16_t skill_id,
                                        std::uint16_t skill_level, sim::CellPosition where) {
  proto::SkillOnGroundRequest request;
  request.skill_id = skill_id;
  request.skill_level = skill_level;
  request.cell_x = where.x;
  request.cell_y = where.y;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestSkillOnGroundWithText(Connection& link, std::uint16_t skill_id,
                                                std::uint16_t skill_level, sim::CellPosition where,
                                                std::string_view text) {
  proto::SkillOnGroundWithTextRequest request;
  request.skill_id = skill_id;
  request.skill_level = skill_level;
  request.cell_x = where.x;
  request.cell_y = where.y;
  // UNE PHRASE, pas un identifiant : en 1252 tant qu'elle s'y écrit, en UTF-8
  // sinon. ⚠ COUPÉE ICI ET NON À L'ENCODAGE quand elle est en UTF-8 : couper un
  // caractère en deux laisserait au-dessus du piège un octet orphelin.
  std::string wire = core::WireFromUtf8Text(text);
  constexpr std::size_t kTextBytes = proto::SkillOnGroundWithTextRequest::kTextSize - 1;
  if (wire.size() > kTextBytes) {
    const bool utf8 = wire != core::Cp1252FromUtf8(text);
    wire.resize(kTextBytes);
    if (utf8) {
      wire.resize(core::WithoutDanglingUtf8(wire).size());
    }
  }
  request.text = std::move(wire);
  return SendEncoded(link, proto::Encode(request));
}

FrameOutcome ApplySkillFrame(const Frame& frame, sim::SkillBook& book, std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::ZcSkillPostdelay::kOpcode: {
      proto::ZcSkillPostdelay packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // Le temps d'attente d'une compétence qu'on vient de lancer. Voir
      // SkillBook::StartCooldown : il se note même pour une compétence que le
      // grimoire ne porte pas.
      book.StartCooldown(packet.SKID, packet.DelayTM, now_ms);
      return FrameOutcome::kApplied;
    }

    case proto::ZcSkillinfoList::kOpcode: {
      proto::ZcSkillinfoList packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // La liste REMPLACE le grimoire : le serveur l'envoie entière, et garder
      // ce qu'elle ne porte plus laisserait une compétence oubliée.
      book.Clear();
      for (const proto::Skilldata& data : packet.skills) {
        book.Learn(SkillFrom(data));
      }
      return FrameOutcome::kApplied;
    }

    case proto::ZcAddSkill::kOpcode: {
      proto::ZcAddSkill packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // ⚠ Le serveur envoie aussi ce paquet avec un identifiant NUL, pour dire
      // qu'il n'y a rien à ajouter. L'accepter mettrait une ligne vide au haut
      // du grimoire, qu'aucun clic ne pourrait lancer.
      if (packet.skill.id == 0) {
        return FrameOutcome::kIgnored;
      }
      book.Learn(SkillFrom(packet.skill));
      return FrameOutcome::kApplied;
    }

    case proto::ZcSkillinfoUpdate2::kOpcode: {
      proto::ZcSkillinfoUpdate2 packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // Une mise à jour d'une compétence qu'on ne connaît pas encore l'ajoute :
      // le serveur fait suivre l'ajout de la mise à jour, et rien ne garantit
      // l'ordre d'arrivée.
      sim::KnownSkill skill;
      skill.id = packet.id;
      skill.level = packet.level;
      skill.sp_cost = packet.sp;
      skill.range_cells = packet.range2;
      skill.targeting = static_cast<std::uint16_t>(packet.inf);
      skill.upgradable = packet.upFlag != 0;
      book.Learn(std::move(skill));
      return FrameOutcome::kApplied;
    }

    case proto::ZcAutorunSkill::kOpcode: {
      proto::ZcAutorunSkill packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // Même lecture qu'une ligne du grimoire, mais elle n'y entre pas : voir
      // `SkillBook::OfferItemSkill`.
      sim::KnownSkill skill;
      skill.id = packet.skill_id;
      skill.level = packet.skill_lv;
      skill.sp_cost = packet.skill_sp;
      skill.range_cells = packet.skill_range;
      skill.targeting = static_cast<std::uint16_t>(packet.skill_type);
      skill.name = core::Utf8FromCp1252(packet.skill_name);
      book.OfferItemSkill(std::move(skill));
      return FrameOutcome::kApplied;
    }

    case proto::ZcSkillinfoDelete::kOpcode: {
      proto::ZcSkillinfoDelete packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      return book.Forget(packet.skillID) ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
    }

    default:
      return FrameOutcome::kUnhandled;
  }
}

std::optional<std::uint16_t> RefusedSkillOf(const Frame& frame) {
  if (frame.opcode != proto::ZcAckTouseskill::kOpcode) {
    return std::nullopt;
  }
  proto::ZcAckTouseskill packet;
  if (!proto::Decode(frame.bytes, packet) || packet.flag != kSkillUseFailed) {
    return std::nullopt;
  }
  return packet.skillId;
}

}  // namespace ro::net
