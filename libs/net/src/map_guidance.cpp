// Ce qu'un script montre sur la carte : les repères et les demandes de
// navigation. Voir l'en-tête pour leur place hors du monde.

#include "ro/net/map_guidance.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/net/map_name.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/navigation_packets.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// Les trois actions de la commande `viewpoint`, telles que le serveur les écrit.
constexpr std::uint32_t kViewpointEphemeral = 0;
constexpr std::uint32_t kViewpointPermanent = 1;
constexpr std::uint32_t kViewpointRemove = 2;

// Le dernier type de cible que la navigation connaisse.
constexpr std::uint8_t kLastNavigationTarget =
    static_cast<std::uint8_t>(sim::NavigationTarget::kMonster);

// Le drapeau de services est écrit en décimal : un chiffre par service.
constexpr std::uint8_t kFlagBase = 10;
constexpr std::uint8_t kFlagTransportPlace = 100;
constexpr std::uint8_t kFlagAirshipPlace = 10;
constexpr std::uint8_t kFlagScrollsPlace = 1;

[[nodiscard]] bool DigitSet(std::uint8_t flag, std::uint8_t place) {
  return (flag / place) % kFlagBase != 0;
}

FrameOutcome ApplyViewpoint(const proto::ZcCompass& packet, sim::MapViewpoints& viewpoints,
                            std::uint32_t now_ms) {
  const auto point_id = static_cast<std::int32_t>(packet.id);
  switch (packet.type) {
    case kViewpointRemove:
      return viewpoints.Remove(packet.npcId, point_id) ? FrameOutcome::kApplied
                                                       : FrameOutcome::kIgnored;
    case kViewpointEphemeral:
    case kViewpointPermanent:
      viewpoints.Place(sim::Viewpoint{
          .npc_id = packet.npcId,
          .point_id = point_id,
          .cell_x = static_cast<std::int16_t>(packet.xPos),
          .cell_y = static_cast<std::int16_t>(packet.yPos),
          .color_argb = sim::OpaqueViewpointColor(packet.color),
          .permanent = packet.type == kViewpointPermanent,
          .placed_ms = now_ms,
      });
      return FrameOutcome::kApplied;
    default:
      return FrameOutcome::kIgnored;
  }
}

FrameOutcome ApplyNavigation(const proto::NavigationActive& packet,
                             sim::NavigationRequests& navigation) {
  if (packet.type > kLastNavigationTarget) {
    return FrameOutcome::kIgnored;
  }
  sim::NavigationRequest request;
  request.target = static_cast<sim::NavigationTarget>(packet.type);
  request.services = sim::NavigationServices{
      .transport = DigitSet(packet.flag, kFlagTransportPlace),
      .airship = DigitSet(packet.flag, kFlagAirshipPlace),
      .scrolls = DigitSet(packet.flag, kFlagScrollsPlace),
  };
  request.hide_window = packet.hide_window != 0;
  request.map = MapBaseName(packet.map);
  request.cell_x = packet.x;
  request.cell_y = packet.y;
  request.monster_id = packet.mob_id;
  navigation.Push(std::move(request));
  return FrameOutcome::kApplied;
}

// Les quatre annonces du boss, telles que le serveur les numérote.
enum class BossAnnounce : std::uint8_t { kNone = 0, kAlive = 1, kAliveSaid = 2, kDead = 3 };

// Le retour du boss qui s'annonce « dans moins d'une minute » : zéro heure, une
// minute — le serveur arrondit à la minute supérieure.
constexpr std::uint16_t kLastMinute = 1;

// Le blanc des annonces du boss.
constexpr std::uint32_t kBossNoticeColor = 0xFFFFFF;
constexpr std::size_t kBossNoticeCapacity = 256;

void SayBoss(sim::World& world, const char* text, std::uint32_t now_ms) {
  world.RecordChat(sim::ChatKind::kServer, sim::ChatType::kPublic, 0, text, now_ms,
                   kBossNoticeColor);
}

FrameOutcome ApplyBossInfo(const proto::ZcBossInfo& packet, sim::MapGuidance& guidance,
                           sim::World& world, std::uint32_t now_ms) {
  guidance.boss.reset();
  const std::string name = core::Utf8FromCp1252(packet.name);
  std::array<char, kBossNoticeCapacity> line{};
  switch (static_cast<BossAnnounce>(packet.type)) {
    case BossAnnounce::kNone:
      SayBoss(world, core::Tr("Aucune trace de boss dans cette zone."), now_ms);
      return FrameOutcome::kApplied;
    case BossAnnounce::kAlive:
    case BossAnnounce::kAliveSaid:
      guidance.boss = sim::BossCell{.x = static_cast<std::int32_t>(packet.x),
                                    .y = static_cast<std::int32_t>(packet.y)};
      if (static_cast<BossAnnounce>(packet.type) == BossAnnounce::kAliveSaid) {
        std::snprintf(line.data(), line.size(),
                      core::Tr("La position du boss [%s] s'affiche sur la minimap."), name.c_str());
        SayBoss(world, line.data(), now_ms);
      }
      return FrameOutcome::kApplied;
    case BossAnnounce::kDead:
      if (packet.minHours == 0 && packet.minMinutes == kLastMinute) {
        std::snprintf(line.data(), line.size(),
                      core::Tr("Le boss [%s] apparaîtra dans moins d'une minute."), name.c_str());
      } else {
        std::snprintf(line.data(), line.size(),
                      core::Tr("Le boss [%s] apparaîtra dans %02d heure(s) et %02d minute(s)."),
                      name.c_str(), static_cast<int>(packet.minHours),
                      static_cast<int>(packet.minMinutes));
      }
      SayBoss(world, line.data(), now_ms);
      return FrameOutcome::kApplied;
  }
  return FrameOutcome::kIgnored;
}

}  // namespace

FrameOutcome ApplyBossInfoFrame(const Frame& frame, sim::MapGuidance& guidance, sim::World& world,
                                std::uint32_t now_ms) {
  if (frame.opcode != proto::ZcBossInfo::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::ZcBossInfo>(frame, [&guidance, &world, now_ms](const auto& packet) {
    return ApplyBossInfo(packet, guidance, world, now_ms);
  });
}

FrameOutcome ApplyMapGuidanceFrame(const Frame& frame, sim::MapGuidance& guidance,
                                   std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::ZcCompass::kOpcode:
      return Decoded<proto::ZcCompass>(frame, [&guidance, now_ms](const auto& packet) {
        return ApplyViewpoint(packet, guidance.viewpoints, now_ms);
      });

    case proto::NavigationActive::kOpcode:
      return Decoded<proto::NavigationActive>(frame, [&guidance](const auto& packet) {
        return ApplyNavigation(packet, guidance.navigation);
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

}  // namespace ro::net
