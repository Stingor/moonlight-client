#include "character/stat_preset_desk.h"

#include <algorithm>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/net/character.h"
#include "ro/sim/player_status.h"

#include "character/stat_preset_file.h"

namespace moonclient {
namespace {

static_assert(ro::ui::kStatPresetStatCount == ro::sim::kBaseStatCount,
              "la fenêtre et le carnet ne comptent pas les mêmes caractéristiques");
static_assert(ro::ui::kStatPresetNameInputBytes == ro::sim::kStatPresetNameBytes,
              "la saisie et le carnet ne bornent pas le nom à la même taille");

[[nodiscard]] ro::sim::BaseStats BaseOf(const ro::sim::PlayerStatus& status) {
  return {status.str, status.agi, status.vit, status.int_, status.dex, status.luk};
}

[[nodiscard]] ro::sim::BaseStats CostOf(const ro::sim::PlayerStatus& status) {
  return {status.str_cost, status.agi_cost, status.vit_cost,
          status.int_cost, status.dex_cost, status.luk_cost};
}

// LES SIX VALEURS ET LES POINTS SONT-ILS DITS ? Sans eux, ni l'enregistrement
// ni le plan n'ont de sens : « pas encore dit » vaut moins un.
[[nodiscard]] bool Known(const ro::sim::PlayerStatus& status) {
  const ro::sim::BaseStats base = BaseOf(status);
  return status.status_point >= 0 &&
         std::all_of(base.begin(), base.end(), [](std::int64_t value) { return value >= 0; });
}

[[nodiscard]] ro::sim::StatPlanInput InputOf(const ro::sim::World& world) {
  const ro::sim::PlayerStatus& status = world.player_status();
  return ro::sim::StatPlanInput{
      .current = BaseOf(status), .next_cost = CostOf(status), .status_point = status.status_point};
}

[[nodiscard]] ro::sim::StatRaiseSight SightOf(const ro::sim::World& world, std::uint32_t now_ms) {
  const ro::sim::PlayerStatus& status = world.player_status();
  return ro::sim::StatRaiseSight{.base = BaseOf(status),
                                 .next_cost = CostOf(status),
                                 .status_point = status.status_point,
                                 .awaited = world.stat_raise_awaited(),
                                 .refusals = world.stat_raise_refusals(),
                                 .now_ms = now_ms};
}

[[nodiscard]] ro::ui::StatPresetRowVerdict VerdictOf(ro::sim::StatPlanVerdict verdict) {
  switch (verdict) {
    case ro::sim::StatPlanVerdict::kReached:
      return ro::ui::StatPresetRowVerdict::kReached;
    case ro::sim::StatPlanVerdict::kAboveTarget:
      return ro::ui::StatPresetRowVerdict::kAboveTarget;
    case ro::sim::StatPlanVerdict::kShort:
      return ro::ui::StatPresetRowVerdict::kShort;
    case ro::sim::StatPlanVerdict::kCeiling:
      return ro::ui::StatPresetRowVerdict::kCeiling;
  }
  return ro::ui::StatPresetRowVerdict::kReached;
}

[[nodiscard]] std::string RefusalText(ro::sim::StatPresetRefusal refusal) {
  switch (refusal) {
    case ro::sim::StatPresetRefusal::kNone:
      break;
    case ro::sim::StatPresetRefusal::kEmptyName:
      return ro::core::Tr("Donne un nom au préréglage.");
    case ro::sim::StatPresetRefusal::kNameTaken:
      return ro::core::Tr("Un autre préréglage porte déjà ce nom.");
    case ro::sim::StatPresetRefusal::kFull: {
      return ro::core::Printed(ro::core::Tr("Le carnet est plein (%zu préréglages)."),
                               ro::sim::kStatPresetLimit);
    }
    case ro::sim::StatPresetRefusal::kUnknown:
      return ro::core::Tr("Ce préréglage n'existe plus.");
  }
  return {};
}

// Une montée à la fois, comme la flèche.
constexpr std::uint8_t kOneStep = 1;

// LA RAISON D'UN ARRÊT, et où la file en était.
[[nodiscard]] std::string StopText(const ro::sim::StatRaiseRun& run, const std::string& name) {
  std::string text;
  const std::size_t done = run.done();
  const std::size_t total = run.total();
  switch (run.stop()) {
    case ro::sim::StatRunStop::kNone:
      return {};
    case ro::sim::StatRunStop::kFinished:
      text = ro::core::Printed(ro::core::Tr("Préréglage « %s » appliqué : %zu montées."),
                               name.c_str(), done);
      break;
    case ro::sim::StatRunStop::kRefused:
      text = ro::core::Printed(
          ro::core::Tr("Le serveur a refusé une montée : arrêt après %zu / %zu."), done, total);
      break;
    case ro::sim::StatRunStop::kPointsChanged:
      text = ro::core::Printed(
          ro::core::Tr("Les points de statut ont changé en cours : arrêt après %zu / %zu."), done,
          total);
      break;
    case ro::sim::StatRunStop::kStatsChanged:
      text = ro::core::Printed(ro::core::Tr("Une stat a changé en cours : arrêt après %zu / %zu."),
                               done, total);
      break;
    case ro::sim::StatRunStop::kShort:
      text = ro::core::Printed(
          ro::core::Tr("Plus assez de points pour la montée suivante : arrêt après %zu / "
                       "%zu."),
          done, total);
      break;
    case ro::sim::StatRunStop::kNoReply:
      text = ro::core::Printed(ro::core::Tr("Le serveur ne répond pas : arrêt après %zu / %zu."),
                               done, total);
      break;
    case ro::sim::StatRunStop::kNotSent:
      text = ro::core::Printed(
          ro::core::Tr("Montée non envoyée (hors ligne ou personnage mort) : arrêt après "
                       "%zu / %zu."),
          done, total);
      break;
    case ro::sim::StatRunStop::kCancelled:
      text = ro::core::Printed(ro::core::Tr("Application arrêtée à ta demande après %zu / %zu."),
                               done, total);
      break;
  }
  return text;
}

}  // namespace

StatPresetDesk::StatPresetDesk(std::filesystem::path client_root, std::uint32_t account_id)
    : client_root_(std::move(client_root)),
      account_id_(account_id),
      book_(LoadStatPresets(client_root_, account_id_)) {}

void StatPresetDesk::Keep() {
  if (!SaveStatPresets(client_root_, account_id_, book_.all())) {
    report_ = ro::core::Tr("Le fichier des préréglages n'a pas pu être écrit.");
  }
}

void StatPresetDesk::Say(ro::sim::World& world, std::uint32_t now_ms) {
  report_ = StopText(run_, applied_name_);
  if (!report_.empty()) {
    world.RecordChat(ro::sim::ChatKind::kServer, 0, report_, now_ms);
  }
}

void StatPresetDesk::Tick(ro::net::Connection& link, ro::sim::World& world, std::uint32_t now_ms) {
  if (run_.running()) {
    if (const std::optional<std::uint8_t> rank = run_.Tick(SightOf(world, now_ms))) {
      bool emitted = false;
      if (link.is_open()) {
        // UN CRAN, comme la flèche : la file attend la réponse avant le suivant.
        const ro::core::Result<void> sent = ro::net::RequestStatUp(
            link, world, static_cast<std::uint16_t>(ro::sim::kFirstBaseStatParameter + *rank),
            kOneStep);
        if (!sent) {
          ro::core::LogWarning("préréglage de stats : montée non émise — {}",
                               ro::core::Describe(sent.error()));
        }
        // ⚠ UNE DEMANDE TUE SE RECONNAÎT À L'ATTENTE RESTÉE BASSE : celle d'un
        // personnage mort part sans rien émettre.
        emitted = sent.has_value() && world.stat_raise_awaited();
      }
      run_.Sent(emitted, now_ms);
    }
  }
  if (was_running_ && !run_.running()) {
    Say(world, now_ms);
  }
  was_running_ = run_.running();
}

void StatPresetDesk::Apply(const ro::ui::StatPresetAction& asked, const ro::sim::World& world,
                           std::uint32_t now_ms) {
  const ro::sim::PlayerStatus& status = world.player_status();
  switch (asked.verb) {
    case ro::ui::StatPresetVerb::kNothing:
      return;
    case ro::ui::StatPresetVerb::kSave: {
      if (!Known(status)) {
        report_ = ro::core::Tr("Les valeurs ne sont pas encore connues.");
        return;
      }
      const ro::sim::StatPresetRefusal refusal = book_.Save(asked.name, BaseOf(status));
      if (refusal != ro::sim::StatPresetRefusal::kNone) {
        report_ = RefusalText(refusal);
        return;
      }
      report_ = ro::core::Printed(ro::core::Tr("Préréglage « %s » enregistré."),
                                  ro::sim::TidyStatPresetName(asked.name).c_str());
      Keep();
      return;
    }
    case ro::ui::StatPresetVerb::kRename: {
      const ro::sim::StatPresetRefusal refusal = book_.Rename(asked.index, asked.name);
      if (refusal != ro::sim::StatPresetRefusal::kNone) {
        report_ = RefusalText(refusal);
        return;
      }
      previewed_.reset();
      report_ = ro::core::Printed(ro::core::Tr("Préréglage renommé en « %s »."),
                                  ro::sim::TidyStatPresetName(asked.name).c_str());
      Keep();
      return;
    }
    case ro::ui::StatPresetVerb::kRemove: {
      const ro::sim::StatPreset* gone = book_.at(asked.index);
      const std::string name = gone != nullptr ? gone->name : std::string();
      const ro::sim::StatPresetRefusal refusal = book_.Remove(asked.index);
      if (refusal != ro::sim::StatPresetRefusal::kNone) {
        report_ = RefusalText(refusal);
        return;
      }
      previewed_.reset();
      report_ = ro::core::Printed(ro::core::Tr("Préréglage « %s » supprimé."), name.c_str());
      Keep();
      return;
    }
    case ro::ui::StatPresetVerb::kPreview:
      if (const ro::sim::StatPreset* preset = book_.at(asked.index); preset != nullptr) {
        previewed_ = preset->name;
        report_.clear();
      }
      return;
    case ro::ui::StatPresetVerb::kCancelPreview:
      previewed_.reset();
      return;
    case ro::ui::StatPresetVerb::kConfirm: {
      if (!previewed_.has_value() || run_.running() || !Known(status)) {
        return;
      }
      const auto shown = std::find_if(
          book_.all().begin(), book_.all().end(),
          [&](const ro::sim::StatPreset& preset) { return preset.name == *previewed_; });
      if (shown == book_.all().end()) {
        previewed_.reset();
        return;
      }
      const ro::sim::StatPlan plan = ro::sim::PlanStatPreset(shown->stats, InputOf(world));
      if (!run_.Start(plan, SightOf(world, now_ms))) {
        report_ = ro::core::Tr("Rien à monter : aucun point ne rapproche de la cible.");
        return;
      }
      applied_name_ = shown->name;
      previewed_.reset();
      report_.clear();
      return;
    }
    case ro::ui::StatPresetVerb::kStop:
      run_.Cancel();
      return;
  }
}

void StatPresetDesk::Compose(const ro::sim::World& world, ro::ui::StatPresetWindowView& into) {
  const ro::sim::PlayerStatus& status = world.player_status();
  rows_.clear();
  for (const ro::sim::StatPreset& preset : book_.all()) {
    rows_.push_back(ro::ui::StatPresetRowView{.name = preset.name, .stats = preset.stats});
  }
  into = ro::ui::StatPresetWindowView{};
  into.presets = rows_;
  into.capacity = ro::sim::kStatPresetLimit;
  into.current = BaseOf(status);
  into.stats_known = Known(status);
  into.running = run_.running();
  into.done = run_.done();
  into.total = run_.total();
  into.report = report_;
  if (!previewed_.has_value() || !into.stats_known) {
    return;
  }
  const auto shown =
      std::find_if(book_.all().begin(), book_.all().end(),
                   [&](const ro::sim::StatPreset& preset) { return preset.name == *previewed_; });
  if (shown == book_.all().end()) {
    previewed_.reset();
    return;
  }
  const ro::sim::StatPlan plan = ro::sim::PlanStatPreset(shown->stats, InputOf(world));
  preview_name_ = shown->name;
  ro::ui::StatPresetPreviewView& preview = into.preview;
  preview.shown = true;
  preview.name = preview_name_;
  for (std::size_t rank = 0; rank < ro::sim::kBaseStatCount; ++rank) {
    const ro::sim::StatPlanRow& row = plan.rows[rank];
    preview.rows[rank] = ro::ui::StatPresetPreviewRow{.current = row.current,
                                                      .after = row.after,
                                                      .target = row.target,
                                                      .missing_steps = row.missing_steps,
                                                      .missing_points = row.missing_points,
                                                      .verdict = VerdictOf(row.verdict)};
  }
  preview.points_before = plan.points_before;
  preview.points_after = plan.points_after;
  preview.raises = plan.raises();
}

}  // namespace moonclient
