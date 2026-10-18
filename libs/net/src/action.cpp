#include "ro/net/action.h"

#include "ro/core/text.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/stop_attack_packet.h"

#include "send_encoded.h"

namespace ro::net {
namespace {

core::Result<void> SendAction(Connection& link, std::uint32_t target_id, std::uint8_t action) {
  proto::ActionRequest request;
  request.target_id = target_id;
  request.action = action;
  return SendEncoded(link, proto::Encode(request));
}

}  // namespace

core::Result<void> RequestAttack(Connection& link, std::uint32_t target_id, bool repeat) {
  return SendAction(
      link, target_id,
      repeat ? proto::ActionRequest::kAttackRepeatedly : proto::ActionRequest::kAttackOnce);
}

core::Result<void> RequestStopAttack(Connection& link) {
  return SendEncoded(link, proto::EncodeStopAttack());
}

core::Result<void> RequestTick(Connection& link, std::uint32_t client_tick) {
  proto::TickRequest request;
  request.client_tick = client_tick;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestPickup(Connection& link, std::uint32_t item_id) {
  proto::PickupRequest request;
  request.item_id = item_id;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestChat(Connection& link, std::string_view speaker, std::string_view text) {
  proto::ChatRequest request;
  // Le serveur relaie des OCTETS, et compare le nom à celui qu'il garde : le
  // nom comme le message repartent dans la page de codes du jeu, sans quoi un
  // nom accentué ne correspondrait plus et tout le message serait rejeté.
  request.speaker = core::Cp1252FromUtf8(speaker);
  // ⚠ LA PHRASE BASCULE, LE NOM JAMAIS : le serveur RELAIE l'une et COMPARE
  // l'autre, octet par octet, à ce que sa base contient.
  request.text = core::WireFromUtf8Text(text);
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestEmotion(Connection& link, std::uint16_t emotion) {
  proto::EmotionRequest request;
  request.pack_id = proto::EmotionRequest::kBasePack;
  request.emotion_id = emotion;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestRespawn(Connection& link) {
  proto::RestartRequest request;
  request.what = proto::RestartRequest::kSavePoint;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestAutoRevive(Connection& link) {
  return SendEncoded(link, proto::Encode(proto::AutoReviveRequest{}));
}

core::Result<void> RequestSit(Connection& link, bool sit) {
  // S'asseoir ne vise personne : le serveur ne lit pas la cible pour ces
  // deux actions-là (clif_parse_ActionRequest_sub, dans src/map/clif.cpp).
  return SendAction(link, 0, sit ? proto::ActionRequest::kSitDown : proto::ActionRequest::kStandUp);
}

}  // namespace ro::net
