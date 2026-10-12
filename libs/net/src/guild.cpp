// La guilde : ce que le serveur en dit. Voir l'en-tête pour le carnet qui vit
// à part du monde.

#include "ro/net/guild.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/guild_packets.h"
#include "ro/proto/map_packets.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// La cellule que le serveur annonce pour dire « ce membre n'est plus sur la
// carte » : les deux coordonnées valent moins un.
constexpr std::int16_t kNoCell = -1;

// Ce que `ZC_UPDATE_CHARSTAT` écrit d'un membre qui vient de se connecter.
constexpr std::uint32_t kMemberOnline = 1;
// Le verdict d'une dissolution réussie.
constexpr std::int32_t kDisbanded = 0;
// Le dernier verdict de création et de réponse que le serveur connaisse.
constexpr std::uint8_t kLastCreation =
    static_cast<std::uint8_t>(sim::GuildCreation::kEmperiumMissing);
constexpr std::uint8_t kLastInviteAnswer =
    static_cast<std::uint8_t>(sim::GuildInviteAnswer::kGuildFull);

// ⚠ LES NOMS SONT DES IDENTIFIANTS DU JEU, en 1252 ; les phrases peuvent
// revenir en UTF-8 quand c'est un client comme le nôtre qui les a écrites.
[[nodiscard]] std::string Name(std::string_view wire) { return core::Utf8FromCp1252(wire); }
[[nodiscard]] std::string Phrase(std::string_view wire) { return core::Utf8FromWire(wire); }

// LE TEXTE, ses mots interdits masqués quand on tient la liste.
[[nodiscard]] std::string Masked(const sim::BannedWords* banned, const std::string& utf8_text) {
  return sim::BannedWords::MaskWith(banned, utf8_text);
}

// LES DEUX DRAPEAUX DE GUILDE — l'étendard d'une carte de siège, et celui que
// pose une compétence de guilde — inscrivent leur version d'emblème même nulle.
constexpr std::int16_t kGuildFlagJob = 722;
constexpr std::int16_t kGuildSkillFlagJob = 20269;

// LA VERSION D'EMBLÈME QU'UNE APPARITION DÉCLARE, pour la guilde de l'unité.
//
// 🔴 C'EST LE SEUL ENDROIT OÙ L'ON APPREND CELLE DES AUTRES GUILDES : le
// serveur ne la redit qu'au changement d'emblème. Sans elle, aucune étiquette
// ne saurait quel emblème demander.
//
// ⚠ UNE VERSION NULLE D'UN JOUEUR NE S'ÉCRIT PAS : elle effacerait celle qu'un
// autre membre vient d'annoncer. Le monde lit les mêmes trames, pour l'acteur.
template <typename Packet>
[[nodiscard]] FrameOutcome LearnEmblemVersion(const Packet& packet, sim::GuildBook& guild) {
  const bool flag = packet.job == kGuildFlagJob || packet.job == kGuildSkillFlagJob;
  if (packet.GUID == 0 || (packet.GEmblemVer <= 0 && !flag)) {
    return FrameOutcome::kIgnored;
  }
  guild.TellEmblemVersion(static_cast<std::int32_t>(packet.GUID), packet.GEmblemVer);
  return FrameOutcome::kApplied;
}

[[nodiscard]] sim::GuildRelationKind KindOf(std::int64_t relation) {
  return relation == 0 ? sim::GuildRelationKind::kAlly : sim::GuildRelationKind::kEnemy;
}

// Le départ comme l'expulsion : c'est nous, ou c'est un autre.
FrameOutcome MemberGone(sim::GuildBook& guild, std::uint32_t char_id, std::uint32_t own_char_id) {
  if (char_id == own_char_id) {
    guild.Leave();
  } else {
    guild.RemoveMember(char_id);
  }
  return FrameOutcome::kApplied;
}

FrameOutcome ApplyMembers(const proto::ZcMembermgrInfo& packet, sim::GuildBook& guild) {
  std::vector<sim::GuildMember> members;
  members.reserve(packet.guildMemberInfo.size());
  for (const proto::GuildMemberInfo& wire : packet.guildMemberInfo) {
    sim::GuildMember member;
    member.account_id = wire.AID;
    member.char_id = wire.GID;
    member.name = Name(wire.char_name);
    member.hair = wire.head;
    member.hair_palette = wire.headPalette;
    member.sex = wire.sex;
    member.job = wire.job;
    member.level = wire.level;
    member.contribution = wire.contributionExp;
    member.online = wire.currentState != 0;
    member.position = wire.positionID;
    member.last_login = wire.lastLoginTime;
    members.push_back(std::move(member));
  }
  guild.ReplaceMembers(std::move(members));
  return FrameOutcome::kApplied;
}

FrameOutcome ApplySkills(const proto::ZcGuildSkillinfo& packet, sim::GuildBook& guild) {
  std::vector<sim::GuildSkill> skills;
  skills.reserve(packet.skillInfo.size());
  for (const proto::GuildSkilldata& wire : packet.skillInfo) {
    if (wire.id == 0) {
      continue;
    }
    sim::GuildSkill skill;
    skill.id = wire.id;
    skill.inf = wire.inf;
    skill.level = wire.level;
    skill.sp = wire.sp;
    skill.range = wire.range2;
    skill.name = wire.name;
    skill.upgradable = wire.upFlag != 0;
    skills.push_back(std::move(skill));
  }
  guild.ReplaceSkills(packet.skillPoint, std::move(skills));
  return FrameOutcome::kApplied;
}

// Les deux paquets que le générateur ne décode pas : les noms des postes, et
// l'accusé d'un changement de postes.
FrameOutcome ApplyPositionNames(const Frame& frame, sim::GuildBook& guild,
                                const sim::BannedWords* banned) {
  const auto names = proto::DecodeGuildPositionNames(frame.bytes);
  if (!names) {
    return FrameOutcome::kUndecodable;
  }
  for (const proto::GuildPositionName& entry : *names) {
    guild.TellPositionName(entry.position, Masked(banned, Phrase(entry.name)));
  }
  return FrameOutcome::kApplied;
}

FrameOutcome ApplyPositionsChanged(const Frame& frame, sim::GuildBook& guild,
                                   const sim::BannedWords* banned) {
  const auto entries = proto::DecodeGuildPositionsChanged(frame.bytes);
  if (!entries) {
    return FrameOutcome::kUndecodable;
  }
  for (const proto::GuildPositionEntry& entry : *entries) {
    guild.TellPositionName(entry.position, Masked(banned, Phrase(entry.name)));
    guild.TellPositionRights(entry.position, entry.rights, entry.pay_rate);
  }
  return FrameOutcome::kApplied;
}

FrameOutcome ApplyEmblemImage(const Frame& frame, sim::GuildBook& guild) {
  auto image = proto::DecodeGuildEmblemImage(frame.bytes);
  if (!image) {
    return FrameOutcome::kUndecodable;
  }
  guild.TellPushedEmblem(image->guild_id,
                         sim::PushedEmblem{image->version, std::move(image->compressed)});
  return FrameOutcome::kApplied;
}

}  // namespace

FrameOutcome ApplyGuildFrame(const Frame& frame, sim::GuildBook& guild, std::uint32_t own_char_id) {
  return ApplyGuildFrame(frame, guild, own_char_id, nullptr);
}

FrameOutcome ApplyGuildFrame(const Frame& frame, sim::GuildBook& guild, std::uint32_t own_char_id,
                             const sim::BannedWords* banned) {
  switch (frame.opcode) {
    case proto::ZcNotifyPositionToGuildm::kOpcode:
      return Decoded<proto::ZcNotifyPositionToGuildm>(frame, [&guild](const auto& packet) {
        // Le serveur retire la position d'un membre en l'annonçant hors carte.
        std::optional<sim::GuildMemberCell> cell;
        if (packet.xPos != kNoCell || packet.yPos != kNoCell) {
          cell = sim::GuildMemberCell{.x = packet.xPos, .y = packet.yPos};
        }
        guild.TellMemberCell(packet.aid, cell);
        return FrameOutcome::kApplied;
      });

    case proto::ZcUpdateGdid::kOpcode:
      return Decoded<proto::ZcUpdateGdid>(frame, [&guild](const auto& packet) {
        guild.TellBelonging(sim::GuildBelonging{
            .guild_id = static_cast<std::int32_t>(packet.guildId),
            .emblem_version = packet.emblemVersion,
            .rights = static_cast<std::int32_t>(packet.mode),
            .master = packet.isMaster != 0,
            .name = Name(packet.guildName),
            .master_char_id = static_cast<std::int32_t>(packet.masterGID),
        });
        return FrameOutcome::kApplied;
      });

    case proto::ZcGuildInfo::kOpcode:
      return Decoded<proto::ZcGuildInfo>(frame, [&guild, own_char_id, banned](const auto& packet) {
        // 🔴 L'EN-TÊTE VAUT APPARTENANCE : le serveur ne l'envoie qu'aux membres
        // de la guilde qu'il décrit. Et c'est le SEUL témoin d'une adhésion en
        // cours de session — à qui vient d'accepter une invitation, le serveur
        // n'envoie jamais l'appartenance, son poste n'étant pas encore connu au
        // moment de l'accusé.
        //
        // ⚠ UNE APPARTENANCE DÉJÀ CONNUE DE LA MÊME GUILDE NE SE RÉÉCRIT PAS :
        // elle porte le masque de droits et le drapeau de maître, que l'en-tête
        // n'a pas, et ce paquet revient à chaque rafraîchissement de la fenêtre.
        //
        // ⚠ SON NOM SE MASQUE, celui de son maître non.
        const bool known = guild.in_guild() && guild.belonging()->guild_id == packet.GDID;
        const std::string name = Masked(banned, Name(packet.guildname));
        if (packet.GDID > 0 && !known) {
          guild.TellBelonging(sim::GuildBelonging{
              .guild_id = packet.GDID,
              .emblem_version = packet.emblemVersion,
              .master = static_cast<std::uint32_t>(packet.masterGID) == own_char_id,
              .name = name,
              .master_char_id = packet.masterGID,
          });
        }
        guild.TellSummary(sim::GuildSummary{
            .guild_id = packet.GDID,
            .level = packet.level,
            .online_count = packet.userNum,
            .member_most = packet.maxUserNum,
            .average_level = packet.userAverageLevel,
            .exp = packet.exp,
            .next_exp = packet.maxExp,
            .emblem_version = packet.emblemVersion,
            .name = name,
            .land = Phrase(packet.manageLand),
            .master_char_id = packet.masterGID,
            .master_name = Name(packet.masterName),
        });
        return FrameOutcome::kApplied;
      });

    case proto::ZcGuildNotice::kOpcode:
      return Decoded<proto::ZcGuildNotice>(frame, [&guild, banned](const auto& packet) {
        guild.TellNotice(Masked(banned, Phrase(packet.subject)),
                         Masked(banned, Phrase(packet.notice)));
        return FrameOutcome::kApplied;
      });

    case proto::ZcMembermgrInfo::kOpcode:
      return Decoded<proto::ZcMembermgrInfo>(
          frame, [&guild](const auto& packet) { return ApplyMembers(packet, guild); });

    case proto::kGuildPositionNamesOpcode:
      return ApplyPositionNames(frame, guild, banned);

    case proto::ZcPositionInfo::kOpcode:
      return Decoded<proto::ZcPositionInfo>(frame, [&guild](const auto& packet) {
        for (const proto::ZcPositionInfoSub& entry : packet.posInfo) {
          guild.TellPositionRights(entry.positionID, entry.right, entry.payRate);
        }
        return FrameOutcome::kApplied;
      });

    case proto::kGuildPositionsChangedOpcode:
      return ApplyPositionsChanged(frame, guild, banned);

    case proto::ZcAckReqChangeMembers::kOpcode:
      return Decoded<proto::ZcAckReqChangeMembers>(frame, [&guild](const auto& packet) {
        std::vector<sim::GuildPositionMove> moves;
        moves.reserve(packet.members.size());
        for (const proto::ZcAckReqChangeMembersSub& entry : packet.members) {
          moves.push_back({entry.accId, entry.charId, entry.positionID});
        }
        guild.TellPositionMoves(moves);
        return FrameOutcome::kApplied;
      });

    case proto::ZcUpdateCharstat::kOpcode:
      return Decoded<proto::ZcUpdateCharstat>(frame, [&guild](const auto& packet) {
        guild.TellMemberOnline(packet.aid, packet.cid, packet.status == kMemberOnline,
                               static_cast<std::int16_t>(packet.gender),
                               static_cast<std::int16_t>(packet.hairStyle),
                               static_cast<std::int16_t>(packet.hairColor));
        return FrameOutcome::kApplied;
      });

    case proto::kGuildMemberLeftOpcode:
      return Decoded<proto::ZcAckLeaveGuild2>(frame, [&guild, own_char_id](const auto& packet) {
        return MemberGone(guild, packet.GID, own_char_id);
      });

    case proto::kGuildMemberExpelledOpcode:
      return Decoded<proto::ZcAckBanGuild3>(frame, [&guild, own_char_id](const auto& packet) {
        return MemberGone(guild, packet.GID, own_char_id);
      });

    case proto::ZcAckDisorganizeGuildResult::kOpcode:
      return Decoded<proto::ZcAckDisorganizeGuildResult>(frame, [&guild](const auto& packet) {
        if (packet.result == kDisbanded) {
          guild.Leave();
        }
        return FrameOutcome::kApplied;
      });

    case proto::ZcGuildSkillinfo::kOpcode:
      return Decoded<proto::ZcGuildSkillinfo>(
          frame, [&guild](const auto& packet) { return ApplySkills(packet, guild); });

    case proto::ZcAckGuildMenuinterface::kOpcode:
      return Decoded<proto::ZcAckGuildMenuinterface>(frame, [&guild](const auto& packet) {
        guild.TellMenuFlags(static_cast<std::uint32_t>(packet.menuFlag));
        return FrameOutcome::kApplied;
      });

    case proto::ZcBanList::kOpcode:
      return Decoded<proto::ZcBanList>(frame, [&guild](const auto& packet) {
        std::vector<sim::GuildBan> bans;
        bans.reserve(packet.chars.size());
        for (const proto::ZcBanListSub& entry : packet.chars) {
          bans.push_back({entry.char_id, Name(entry.char_name), Phrase(entry.message)});
        }
        guild.ReplaceBans(std::move(bans));
        return FrameOutcome::kApplied;
      });

    case proto::ZcMyguildBasicInfo::kOpcode:
      return Decoded<proto::ZcMyguildBasicInfo>(frame, [&guild, banned](const auto& packet) {
        std::vector<sim::GuildRelation> relations;
        relations.reserve(packet.rgInfo.size());
        for (const proto::RelatedGuildInfo& entry : packet.rgInfo) {
          relations.push_back(
              {entry.GDID, KindOf(entry.relation), Masked(banned, Name(entry.guildname))});
        }
        guild.ReplaceRelations(std::move(relations));
        return FrameOutcome::kApplied;
      });

    case proto::ZcDeleteRelatedGuild::kOpcode:
      return Decoded<proto::ZcDeleteRelatedGuild>(frame, [&guild](const auto& packet) {
        guild.RemoveRelation(static_cast<std::int32_t>(packet.allyID), KindOf(packet.flag));
        return FrameOutcome::kApplied;
      });

    case proto::ZcResultMakeGuild::kOpcode:
      return Decoded<proto::ZcResultMakeGuild>(frame, [&guild](const auto& packet) {
        if (packet.result > kLastCreation) {
          return FrameOutcome::kIgnored;
        }
        guild.TellCreation(static_cast<sim::GuildCreation>(packet.result));
        return FrameOutcome::kApplied;
      });

    case proto::ZcReqJoinGuild::kOpcode:
      return Decoded<proto::ZcReqJoinGuild>(frame, [&guild, banned](const auto& packet) {
        guild.TellInvitation(sim::GuildInvitation{static_cast<std::int32_t>(packet.guild_id),
                                                  Masked(banned, Name(packet.guild_name))});
        return FrameOutcome::kApplied;
      });

    case proto::ZcReqAllyGuild::kOpcode:
      return Decoded<proto::ZcReqAllyGuild>(frame, [&guild, banned](const auto& packet) {
        guild.TellAllianceOffer(sim::GuildAllianceOffer{
            packet.inviterId, Masked(banned, Name(packet.inviterGuildName))});
        return FrameOutcome::kApplied;
      });

    case proto::ZcAckReqJoinGuild::kOpcode:
      return Decoded<proto::ZcAckReqJoinGuild>(frame, [&guild](const auto& packet) {
        if (packet.result > kLastInviteAnswer) {
          return FrameOutcome::kIgnored;
        }
        guild.TellInviteAnswer(static_cast<sim::GuildInviteAnswer>(packet.result));
        return FrameOutcome::kApplied;
      });

    case proto::ZcChangeGuild::kOpcode:
      return Decoded<proto::ZcChangeGuild>(frame, [&guild](const auto& packet) {
        guild.TellEmblemVersion(packet.guild_id, static_cast<std::int32_t>(packet.emblem_id));
        return FrameOutcome::kApplied;
      });

    case proto::kGuildEmblemImageOpcode:
      return ApplyEmblemImage(frame, guild);

    case proto::PacketIdleUnit::kOpcode:
      return Decoded<proto::PacketIdleUnit>(
          frame, [&guild](const auto& packet) { return LearnEmblemVersion(packet, guild); });
    case proto::PacketSpawnUnit::kOpcode:
      return Decoded<proto::PacketSpawnUnit>(
          frame, [&guild](const auto& packet) { return LearnEmblemVersion(packet, guild); });
    case proto::PacketUnitWalking::kOpcode:
      return Decoded<proto::PacketUnitWalking>(
          frame, [&guild](const auto& packet) { return LearnEmblemVersion(packet, guild); });

    default:
      return FrameOutcome::kUnhandled;
  }
}

}  // namespace ro::net
