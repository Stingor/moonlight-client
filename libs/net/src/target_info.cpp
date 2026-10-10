// L'état de l'entité désignée. Voir l'en-tête pour ce que le générateur ne
// porte pas, et pour la porte PVP qu'on ne comble pas.

#include "ro/net/target_info.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/net/member_statuses.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/member_statuses.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/target_detail.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

[[nodiscard]] sim::TargetKind KindOf(std::uint8_t told) {
  // ⚠ UN RANG QU'ON NE CONNAÎT PAS RESTE INCONNU. Le convertir de force
  // rangerait une entité d'une version plus récente dans la mauvaise espèce, et
  // rien à l'écran ne le dirait.
  if (told > sim::kLastTargetKind) {
    return sim::TargetKind::kUnknown;
  }
  return static_cast<sim::TargetKind>(told);
}

}  // namespace

FrameOutcome ApplyTargetInfoFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightTargetInfo::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonlightTargetInfo packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }

  sim::TargetDetail told;
  told.id = packet.gid;
  told.told = true;
  told.reachable = packet.status == kTargetAnswerOk;
  told.kind = KindOf(packet.type);

  // 🔴 UN CHAMP DONT LE BIT EST À ZÉRO NE SE POSE PAS. Le serveur envoie zéro
  // dans l'octet, et zéro est une valeur légitime ailleurs : c'est le masque,
  // et lui seul, qui sépare « aucun point de vie » de « on ne me le dit pas ».
  if ((packet.known & kTargetKnowsHealth) != 0) {
    told.health = packet.hp;
    told.max_health = packet.maxhp;
  }
  if ((packet.known & kTargetKnowsMagic) != 0) {
    told.magic = packet.sp;
    told.max_magic = packet.maxsp;
  }
  if ((packet.known & kTargetKnowsLevel) != 0) {
    told.level = packet.level;
  }
  if ((packet.known & kTargetKnowsNature) != 0) {
    sim::TargetDetail::Nature nature;
    nature.race = packet.race;
    nature.element = packet.element;
    nature.element_level = packet.element_lv;
    nature.size = packet.size;
    // ⚠ RECOPIÉ TEL QUEL : c'est une classe, pas un drapeau. Voir `Nature`.
    nature.monster_class = packet.boss;
    told.nature = nature;
  }

  // ⚠ LE MONDE REFUSE UNE RÉPONSE QUI NE NOMME PAS LA DÉSIGNATION EN COURS, et
  // ce n'est pas une trame illisible pour autant : elle est arrivée après un
  // changement de cible, et la jeter est la bonne conduite.
  static_cast<void>(world.TellTarget(std::move(told)));
  return FrameOutcome::kApplied;
}

FrameOutcome ApplyTargetStatusFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != proto::ZcMoonlightStatusList::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  proto::ZcMoonlightStatusList packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  if (packet.status != kTargetAnswerOk) {
    // Hors de vue, ou refusée. ⚠ ON NE POSE AUCUNE LISTE : une liste vide dirait
    // « aucun état », ce qui est l'inverse de « je ne sais pas ». Seul le refus
    // se compte.
    static_cast<void>(world.RefuseTargetStatuses(packet.gid));
    return FrameOutcome::kApplied;
  }

  const std::optional<std::vector<sim::StatusReport>> reports =
      ReadStatusReports(frame, packet.count);
  if (!reports.has_value()) {
    return FrameOutcome::kUndecodable;
  }

  // 🔴 LE SERVEUR ÉCRIT LE RESTANT DANS LE CHAMP « TOTAL », et la cible se
  // redemande sans cesse : pris au mot, le total repartirait du restant à chaque
  // réponse, la part écoulée resterait nulle et le voile ne bougerait jamais. La
  // durée de référence est donc le plus grand restant vu pour CETTE cible.
  const sim::TargetDetail& before = world.target_detail();
  const std::span<const sim::ActiveStatus> previous =
      before.id == packet.gid && before.statuses_told
          ? std::span<const sim::ActiveStatus>(before.statuses)
          : std::span<const sim::ActiveStatus>();

  std::vector<sim::ActiveStatus> statuses;
  statuses.reserve(reports->size());
  for (const sim::StatusReport& report : *reports) {
    sim::ActiveStatus status;
    status.effect = report.effect;
    std::uint32_t previous_total = 0;
    std::uint32_t previous_left = 0;
    for (const sim::ActiveStatus& old : previous) {
      if (old.effect == report.effect) {
        previous_total = old.total_ms;
        previous_left = old.expires_ms > now_ms ? old.expires_ms - now_ms : 0;
      }
    }
    status.total_ms = sim::KeepLongestDuration(std::max(report.remaining_ms, report.total_ms),
                                               previous_total, previous_left);
    // ⚠ UN RESTE DEVIENT UNE ÉCHÉANCE, parce que le monde ne lit aucune horloge.
    // Et zéro reste zéro : c'est « sans échéance », non « déjà fini ».
    status.expires_ms = report.remaining_ms == 0 ? 0 : now_ms + report.remaining_ms;
    statuses.push_back(status);
  }

  static_cast<void>(world.TellTargetStatuses(packet.gid, std::move(statuses)));
  return FrameOutcome::kApplied;
}

core::Result<void> RequestTargetInfo(Connection& link, std::uint32_t gid) {
  proto::CzMoonlightTargetInfo request;
  // ⚠ LA LONGUEUR S'ÉCRIT : le serveur la LIT, et le générateur la recopie telle
  // quelle. La laisser à zéro ferait rejeter la trame sans un mot.
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightTargetInfo::kWireSize);
  request.gid = gid;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestTargetStatuses(Connection& link, std::uint32_t gid) {
  proto::CzMoonlightReqStatusList request;
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightReqStatusList::kWireSize);
  request.gid = gid;
  link.Send(proto::Encode(request));
  return {};
}

}  // namespace ro::net
