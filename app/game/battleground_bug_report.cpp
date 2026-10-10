#include "game/battleground_bug_report.h"

#include <array>
#include <cstdio>
#include <format>
#include <string>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::sim::BattleSide;
using ro::sim::EntryQueuePhase;

// Le libellé d'un rapport — « Champ de bataille : file « Flavius » » —, jamais
// plus long.
constexpr std::size_t kLabelBytes = 192;

[[nodiscard]] const char* PhaseWord(EntryQueuePhase phase) {
  switch (phase) {
    case EntryQueuePhase::kIdle:
      return "idle";
    case EntryQueuePhase::kApplying:
      return "applying";
    case EntryQueuePhase::kQueued:
      return "queued";
    case EntryQueuePhase::kCancelling:
      return "cancelling";
    case EntryQueuePhase::kInvited:
      return "invited";
    case EntryQueuePhase::kAccepted:
      return "accepted";
  }
  return "?";
}

[[nodiscard]] const char* SideWord(BattleSide side) {
  switch (side) {
    case BattleSide::kUnknown:
      return "unknown";
    case BattleSide::kLion:
      return "lion";
    case BattleSide::kEagle:
      return "eagle";
  }
  return "?";
}

[[nodiscard]] const char* Flag(bool value) { return value ? "true" : "false"; }

// La file : ce que le serveur en a dit, et ce que le client en a déduit.
[[nodiscard]] std::string QueueFields(const ro::sim::EntryQueue& queue, std::uint32_t now_ms) {
  return std::format(
      R"("queue":{{"phase":"{}","field":"{}","lobby":"{}","count":{},"waited_ms":{},)"
      R"("invitation_ms_left":{}}})",
      PhaseWord(queue.phase()), JsonEscaped(queue.field_name()), JsonEscaped(queue.lobby_name()),
      queue.count(), queue.waited_ms(now_ms), queue.invitation_ms_left(now_ms));
}

// ⚠ LES DEUX NOMS : celui qui part au serveur et celui que le joueur lit. Un
// terrain refusé comme inconnu se comprend à leur écart.
[[nodiscard]] std::string FieldFields(const BattlegroundField& field) {
  return std::format(
      R"("field":{{"server_name":"{}","display_name":"{}","team":{{"min":{},"max":{}}},)"
      R"("apply":{{"solo":{},"party":{},"guild":{}}},"level":{{"min":{},"max":{}}}}})",
      JsonEscaped(field.server_name), JsonEscaped(field.name.In(ro::core::CurrentLanguage())),
      field.team_min, field.team_max, Flag(field.solo_apply), Flag(field.party_apply),
      Flag(field.guild_apply), field.level_min, field.level_max);
}

[[nodiscard]] std::string BattleFields(const ro::sim::Battlefield& battlefield) {
  const ro::sim::BattleScore& score = battlefield.score();
  return std::format(
      R"("battle":{{"own_camp":{},"side":"{}","score":{{"lion":{},"eagle":{}}},"mates":{}}})",
      battlefield.own_camp(), SideWord(battlefield.own_side()), score.lion, score.eagle,
      battlefield.mates().size());
}

}  // namespace

BugReportContext BattlegroundBugReport(const BattlegroundBugScene& scene) {
  std::string fields =
      std::format(R"("battleground":{{"map":"{}","on_battlefield":{},"link_ready":{},)"
                  R"("table":{{"loaded":{},"fields":{}}})",
                  JsonEscaped(scene.map_name), Flag(scene.on_battlefield), Flag(scene.link_ready),
                  Flag(scene.table_loaded), scene.table_fields);
  if (scene.queue != nullptr) {
    fields += ',';
    fields += QueueFields(*scene.queue, scene.now_ms);
  }
  if (scene.field != nullptr) {
    fields += ',';
    fields += FieldFields(*scene.field);
  }
  if (scene.on_battlefield && scene.battlefield != nullptr) {
    fields += ',';
    fields += BattleFields(*scene.battlefield);
  }
  fields += '}';

  // Le libellé : la bataille en cours, sinon la file, sinon le terrain choisi.
  std::array<char, kLabelBytes> label{};
  const bool in_queue = scene.queue != nullptr && scene.queue->phase() != EntryQueuePhase::kIdle;
  if (scene.on_battlefield) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Champ de bataille : en bataille (%s)"),
                  std::string(scene.map_name).c_str());
  } else if (in_queue) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Champ de bataille : file « %s »"),
                  scene.queue->field_name().c_str());
  } else if (scene.field != nullptr) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Champ de bataille : « %s »"),
                  scene.field->server_name.c_str());
  } else {
    std::snprintf(label.data(), label.size(), "%s", ro::core::Tr("Champ de bataille"));
  }
  return FieldsBugReport(label.data(), fields);
}

}  // namespace moonclient
