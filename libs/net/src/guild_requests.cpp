// La guilde : ce qu'on demande au serveur. Voir l'en-tête pour ce qu'il
// revalide et pour l'encodage des textes.

#include <array>
#include <vector>

#include "ro/core/text.h"
#include "ro/net/guild.h"
#include "ro/proto/guild_packets.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

// L'ordre de la fenêtre : voir `RequestGuildWindow`.
constexpr std::array<proto::GuildMenuRequest::Part, 5> kWindowParts = {
    proto::GuildMenuRequest::kMembers, proto::GuildMenuRequest::kPositions,
    proto::GuildMenuRequest::kSkills, proto::GuildMenuRequest::kBans,
    proto::GuildMenuRequest::kBasics};

// Une invitation acceptée ou refusée, telle que le serveur la lit.
constexpr std::int32_t kInvitationAccepted = 1;
constexpr std::int32_t kInvitationRefused = 0;

core::Result<void> SendEncoded(Connection& link, core::Result<std::vector<std::uint8_t>> frame) {
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

}  // namespace

core::Result<void> RequestGuildWindow(Connection& link) {
  for (const proto::GuildMenuRequest::Part part : kWindowParts) {
    proto::GuildMenuRequest request;
    request.part = part;
    if (auto sent = SendEncoded(link, proto::Encode(request)); !sent) {
      return sent;
    }
  }
  return SendEncoded(link, proto::Encode(proto::GuildMenuInterfaceRequest{}));
}

core::Result<void> RequestGuildLeave(Connection& link, std::uint32_t guild_id,
                                     std::uint32_t account_id, std::uint32_t char_id,
                                     std::string_view reason) {
  proto::CzReqLeaveGuild request;
  request.guild_id = guild_id;
  request.AID = account_id;
  request.CID = char_id;
  request.message = core::WireFromUtf8Text(reason);
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestGuildExpel(Connection& link, std::uint32_t guild_id,
                                     std::uint32_t account_id, std::uint32_t char_id,
                                     std::string_view reason) {
  proto::CzReqBanGuild request;
  request.guild_id = guild_id;
  request.AID = account_id;
  request.CID = char_id;
  request.message = core::WireFromUtf8Text(reason);
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestGuildMemberPosition(Connection& link, std::uint32_t account_id,
                                              std::uint32_t char_id, std::int32_t position) {
  proto::CzReqChangeMemberpos request;
  request.list.push_back({account_id, char_id, position});
  const std::vector<std::uint8_t> frame = proto::Encode(request);
  if (frame.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "changement de poste inencodable");
  }
  link.Send(frame);
  return {};
}

core::Result<void> RequestGuildNotice(Connection& link, std::uint32_t guild_id,
                                      std::string_view subject, std::string_view body) {
  proto::GuildNoticeRequest request;
  request.guild_id = guild_id;
  request.subject = core::WireFromUtf8Text(subject);
  request.notice = core::WireFromUtf8Text(body);
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestGuildPositions(Connection& link,
                                         std::span<const GuildPositionEdit> edits) {
  proto::GuildPositionsRequest request;
  request.entries.reserve(edits.size());
  for (const GuildPositionEdit& edit : edits) {
    proto::GuildPositionEntry entry;
    entry.position = edit.position;
    // ⚠ LES SEULS DROITS QUE LE SERVEUR CONNAISSE : un bit de plus ne voudrait
    // rien dire chez lui, et sa part d'expérience est bornée de même.
    entry.rights = edit.rights & sim::kGuildRightsMask;
    entry.ranking = edit.position;
    entry.pay_rate = edit.pay_rate < 0                        ? 0
                     : edit.pay_rate > sim::kGuildPayRateMost ? sim::kGuildPayRateMost
                                                              : edit.pay_rate;
    entry.name = core::Cp1252FromUtf8(edit.name);
    request.entries.push_back(std::move(entry));
  }
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestGuildRelationEnd(Connection& link, std::int32_t guild_id,
                                           sim::GuildRelationKind kind) {
  proto::GuildRelationDeleteRequest request;
  request.guild_id = static_cast<std::uint32_t>(guild_id);
  request.relation = static_cast<std::uint32_t>(kind);
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestGuildCreation(Connection& link, std::uint32_t char_id,
                                        std::string_view name) {
  proto::GuildCreateRequest request;
  request.character_id = char_id;
  request.name = core::Cp1252FromUtf8(name);
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestGuildInvite(Connection& link, std::string_view name) {
  proto::CzReqJoinGuild2 request;
  request.name = core::Cp1252FromUtf8(name);
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestGuildAlliance(Connection& link, std::uint32_t target_account_id,
                                        std::uint32_t own_account_id, std::uint32_t own_char_id) {
  proto::GuildAllianceRequest request;
  request.target_account_id = target_account_id;
  request.own_account_id = own_account_id;
  request.own_char_id = own_char_id;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestGuildOpposition(Connection& link, std::uint32_t target_account_id) {
  proto::GuildOppositionRequest request;
  request.target_account_id = target_account_id;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> AnswerGuildInvitation(Connection& link, std::int32_t guild_id, bool accept) {
  proto::CzJoinGuild request;
  request.guild_id = static_cast<std::uint32_t>(guild_id);
  request.answer = accept ? kInvitationAccepted : kInvitationRefused;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> AnswerGuildAlliance(Connection& link, std::uint32_t inviter_account_id,
                                       bool accept) {
  proto::GuildAllianceAnswer request;
  request.inviter_account_id = inviter_account_id;
  request.answer =
      accept ? proto::GuildAllianceAnswer::kAccept : proto::GuildAllianceAnswer::kRefuse;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestGuildEmblemImage(Connection& link, std::int32_t guild_id) {
  proto::CzReqGuildEmblemImg1 request;
  request.guild_id = guild_id;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> AnnounceGuildEmblem(Connection& link, std::int32_t guild_id,
                                       std::int32_t version) {
  proto::CzReqAddNewEmblem request;
  request.guild_id = static_cast<std::uint32_t>(guild_id);
  request.version = static_cast<std::uint32_t>(version);
  link.Send(proto::Encode(request));
  return {};
}

}  // namespace ro::net
