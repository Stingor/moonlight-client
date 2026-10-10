// L'issue d'une alliance ou d'une hostilité, écrite dans la conversation. Voir
// l'en-tête pour ce qu'un verdict ne dit pas.

#include <cstdint>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/guild.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// LES COULEURS, en `0xRRGGBB` : le blanc d'un succès, le rouge d'un refus.
constexpr std::uint32_t kSuccessColor = 0xFFFFFF;
constexpr std::uint32_t kRefusalColor = 0xFF0000;

// Les verdicts d'une alliance, tels que le serveur les numérote.
enum class AllianceVerdict : std::uint8_t {
  kAlreadyAllied = 0,
  kOfferPending = 1,
  kConcluded = 2,
  kNotConcluded = 3,
  kOwnGuildFull = 4,
  kMasterOnly = 5,
};

// Les verdicts d'une hostilité.
enum class HostilityVerdict : std::uint8_t {
  kDeclared = 0,
  kOwnGuildFull = 1,
  kAlreadyEnemy = 2,
  kDisabled = 3,
};

struct Verdict {
  const char* text = nullptr;
  std::uint32_t color = kRefusalColor;
};

// 🔴 QUI REÇOIT QUOI. L'auteur d'une proposition reçoit tous ces numéros ;
// celui qui y répond ne reçoit que « ne se fera pas » — l'auteur est parti, ou
// une des deux guildes n'a plus de place — et « trop d'alliances ».
[[nodiscard]] Verdict AllianceText(std::uint8_t flag) {
  switch (static_cast<AllianceVerdict>(flag)) {
    case AllianceVerdict::kAlreadyAllied:
      return {core::Tr("Ta guilde est déjà alliée à celle-ci.")};
    case AllianceVerdict::kOfferPending:
      return {core::Tr("Ce joueur examine déjà une autre proposition d'alliance.")};
    case AllianceVerdict::kConcluded:
      return {core::Tr("L'alliance est conclue."), kSuccessColor};
    case AllianceVerdict::kNotConcluded:
      return {
          core::Tr("L'alliance ne se fera pas : elle a été refusée, l'autre guilde n'a plus de "
                   "place, ou son joueur n'est plus là.")};
    case AllianceVerdict::kOwnGuildFull:
      return {core::Tr("Ta guilde compte déjà trop d'alliances.")};
    case AllianceVerdict::kMasterOnly:
      return {core::Tr("Seul le maître de l'autre guilde peut recevoir une alliance.")};
  }
  return {};
}

[[nodiscard]] Verdict HostilityText(std::uint8_t flag) {
  switch (static_cast<HostilityVerdict>(flag)) {
    case HostilityVerdict::kDeclared:
      return {core::Tr("La guilde est désormais ennemie."), kSuccessColor};
    case HostilityVerdict::kOwnGuildFull:
      return {core::Tr("Ta guilde compte déjà trop de guildes ennemies.")};
    case HostilityVerdict::kAlreadyEnemy:
      return {core::Tr("Cette guilde est déjà ennemie.")};
    case HostilityVerdict::kDisabled:
      return {core::Tr("Les hostilités entre guildes sont désactivées.")};
  }
  return {};
}

FrameOutcome Say(sim::World& world, std::uint16_t opcode, std::uint8_t flag, const Verdict& verdict,
                 std::uint32_t now_ms) {
  if (verdict.text == nullptr) {
    core::LogInfo("verdict de guilde n°{} (opcode {:#06x}) inconnu : rien d'écrit", flag, opcode);
    return FrameOutcome::kIgnored;
  }
  world.RecordChat(sim::ChatKind::kServer, sim::ChatType::kPublic, 0, verdict.text, now_ms,
                   verdict.color);
  return FrameOutcome::kApplied;
}

}  // namespace

FrameOutcome ApplyGuildRelationAnswerFrame(const Frame& frame, sim::World& world,
                                           std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::ZcAckReqAllyGuild::kOpcode:
      return Decoded<proto::ZcAckReqAllyGuild>(frame, [&world, now_ms](const auto& packet) {
        return Say(world, proto::ZcAckReqAllyGuild::kOpcode, packet.flag, AllianceText(packet.flag),
                   now_ms);
      });
    case proto::ZcAckReqHostileGuild::kOpcode:
      return Decoded<proto::ZcAckReqHostileGuild>(frame, [&world, now_ms](const auto& packet) {
        return Say(world, proto::ZcAckReqHostileGuild::kOpcode, packet.flag,
                   HostilityText(packet.flag), now_ms);
      });
    default:
      return FrameOutcome::kUnhandled;
  }
}

}  // namespace ro::net
