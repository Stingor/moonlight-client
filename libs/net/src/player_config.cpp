#include "ro/net/player_config.h"

#include <cstdint>

#include "ro/proto/config_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/equip_location.h"

#include "frame_apply.h"
#include "send_encoded.h"

namespace ro::net {
namespace {

[[nodiscard]] bool IsSetting(const proto::ConfigAck& ack, proto::ConfigType setting) {
  return ack.type == static_cast<std::uint32_t>(setting);
}

[[nodiscard]] core::Result<void> Request(Connection& link, proto::ConfigType setting,
                                         std::uint32_t value) {
  return SendEncoded(link, proto::Encode(proto::ConfigRequest{.type = setting, .value = value}));
}

}  // namespace

FrameOutcome ApplyConfigFrame(const Frame& frame, sim::PlayerConfig& config) {
  // L'ANNONCE DE L'ENTRÉE, un octet.
  if (frame.opcode == proto::ZcConfigNotify::kOpcode) {
    return Decoded<proto::ZcConfigNotify>(frame, [&config](const auto& packet) {
      config.AnnounceEquipmentShown(packet.flag != 0);
      return FrameOutcome::kApplied;
    });
  }
  if (frame.opcode != proto::ConfigAck::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  const core::Result<proto::ConfigAck> ack = proto::DecodeConfigAck(frame.bytes);
  if (!ack) {
    return FrameOutcome::kUndecodable;
  }
  if (IsSetting(*ack, proto::ConfigType::kShowEquipment)) {
    config.AnnounceEquipmentShown(ack->value != 0);
    return FrameOutcome::kApplied;
  }
  // ⚠ LE SERVEUR TIENT « MASQUÉS », et c'est zéro qui les montre.
  if (IsSetting(*ack, proto::ConfigType::kHideCostumes)) {
    config.AnnounceCostumesShown(ack->value == 0);
    return FrameOutcome::kApplied;
  }
  // ⚠ LE SERVEUR TIENT « REFUSÉ », et c'est zéro qui laisse l'appel passer.
  if (IsSetting(*ack, proto::ConfigType::kCall)) {
    config.AnnounceCallable(ack->value == 0);
    return FrameOutcome::kApplied;
  }
  return FrameOutcome::kUnhandled;
}

core::Result<void> RequestShowEquipment(Connection& link, bool shown) {
  return Request(link, proto::ConfigType::kShowEquipment, shown ? 1U : 0U);
}

core::Result<void> RequestShowCostumes(Connection& link, bool shown) {
  // 🔴 LE SENS S'INVERSE ICI, ET NULLE PART AILLEURS : le serveur tient
  // « masqués ». Et masquer demande les quatre places d'apparat, non un simple
  // un — c'est ce que la case fait partir, et le serveur n'y lit que « non nul ».
  return Request(link, proto::ConfigType::kHideCostumes, shown ? 0U : sim::kEquipCostumeMask);
}

core::Result<void> RequestCallable(Connection& link, bool callable) {
  // ⚠ LE SENS S'INVERSE, comme pour les costumes : le serveur tient « refusé ».
  return Request(link, proto::ConfigType::kCall, callable ? 0U : 1U);
}

}  // namespace ro::net
