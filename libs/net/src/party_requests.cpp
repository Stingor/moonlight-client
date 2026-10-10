// Le groupe : ce qu'on demande au serveur. Voir l'en-tête pour ce qu'il
// revalide et pour l'encodage des noms.

#include <vector>

#include "ro/core/text.h"
#include "ro/net/party.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/social_packets.h"

namespace ro::net {
namespace {

constexpr std::uint8_t kWireYes = 1;
constexpr std::uint8_t kWireNo = 0;

[[nodiscard]] std::uint8_t Flag(bool value) { return value ? kWireYes : kWireNo; }

core::Result<void> SendEncoded(Connection& link, core::Result<std::vector<std::uint8_t>> frame) {
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

}  // namespace

core::Result<void> RequestPartyCreation(Connection& link, std::string_view name, bool pickup_shared,
                                        bool loot_shared) {
  proto::CzMakeGroup2 request;
  request.name = core::Cp1252FromUtf8(name);
  request.item_pickup = Flag(pickup_shared);
  request.item_share = Flag(loot_shared);
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestPartyInvite(Connection& link, std::string_view name) {
  proto::CzPartyJoinReq request;
  request.name = core::Cp1252FromUtf8(name);
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> AnswerPartyInvitation(Connection& link, std::uint32_t party_id, bool accept) {
  proto::CzPartyJoinReqAck request;
  request.party_id = party_id;
  request.flag = Flag(accept);
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestPartyLeave(Connection& link) {
  link.Send(proto::Encode(proto::CzReqLeaveGroup{}));
  return {};
}

core::Result<void> RequestPartyExpel(Connection& link, std::uint32_t account_id,
                                     std::string_view name) {
  proto::CzReqExpelGroupMember request;
  request.AID = account_id;
  request.name = core::Cp1252FromUtf8(name);
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestPartyLeader(Connection& link, std::uint32_t account_id) {
  proto::PartyLeaderRequest request;
  request.account_id = account_id;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestPartyOptions(Connection& link, const sim::PartyOptions& options) {
  proto::PartyOptionsRequest request;
  request.exp_share = Flag(options.exp_shared);
  request.item_pickup = Flag(options.pickup_shared);
  request.item_share = Flag(options.loot_shared);
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestPartyInviteRefusal(Connection& link, bool refuse) {
  proto::CzPartyConfig request;
  request.refuseInvite = Flag(refuse);
  link.Send(proto::Encode(request));
  return {};
}

}  // namespace ro::net
