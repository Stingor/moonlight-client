// Le panneau de suivi commun. Voir l'en-tête.

#include "game/tracker_stage.h"

#include <string>

#include "ro/core/i18n.h"

#include "quest/quest_rules.h"

namespace moonclient {
namespace {

// Le panneau des succès séparés naît sous celui des quêtes, pas sur lui.
constexpr float kSecondPanelFirstTop = 200.0F;
constexpr std::size_t kMainPanel = 0;
constexpr std::size_t kSecondPanel = 1;

}  // namespace

void TrackerStage::StartSession() {
  memory_ = TrackerMemory{};
  LoadTrackerMemory(client_root_, memory_);
  places_pending_ = true;
}

void TrackerStage::Save() {
  // Le fichier est commun à toute l'installation, pas à un personnage : il
  // s'écrit tel que ce client le tient, et le dernier qui règle a raison.
  static_cast<void>(SaveTrackerMemory(client_root_, memory_));
}

void TrackerStage::Settle(TrackerWindows& tracker, const ro::ui::TrackerSettingsAnswer& asked) {
  if (asked.look.has_value()) {
    memory_.look = *asked.look;
  }
  if (asked.reset_place) {
    for (std::size_t panel = 0; panel < kTrackerPanelCount; ++panel) {
      memory_.places[panel].reset();
      tracker.panels[panel].place_reset = true;
    }
  }
  Save();
}

void TrackerStage::FollowPanel(std::size_t panel, TrackerWindows& tracker, QuestStage& quests,
                               QuestWindows& quest_windows,
                               AchievementWindows& achievement_windows) {
  const ro::ui::TrackerPanelAction asked = std::exchange(tracker.answers[panel], {});
  quests.FollowTracker(quest_windows, asked);
  if (asked.open.has_value() && asked.open->kind == ro::ui::TrackerKind::kAchievement) {
    achievement_windows.tracker_open = asked.open->key;
  }
  if (asked.untrack.has_value() && asked.untrack->kind == ro::ui::TrackerKind::kAchievement) {
    achievement_windows.tracker_untrack = asked.untrack->key;
  }
  if (asked.look.has_value() || asked.reset_place) {
    // ⚠ LE RETOUR À L'ORIGINE DEPUIS UN PANNEAU ne vaut que pour lui.
    if (asked.look.has_value()) {
      memory_.look = *asked.look;
    }
    if (asked.reset_place) {
      memory_.places[panel].reset();
      tracker.panels[panel].place_reset = true;
    }
    Save();
  }
  if (asked.moved.has_value()) {
    memory_.places[panel] = *asked.moved;
    Save();
  }
  if (asked.hide) {
    tracker.shown_asked = false;
  }
}

void TrackerStage::Advance(TrackerWindows& tracker, QuestStage& quests, QuestWindows& quest_windows,
                           AchievementWindows& achievement_windows) {
  for (std::size_t panel = 0; panel < kTrackerPanelCount; ++panel) {
    FollowPanel(panel, tracker, quests, quest_windows, achievement_windows);
  }
  // LA PUCE DES SUCCÈS RÈGLE LE MÊME SUIVI que la page des options.
  if (std::optional<ro::ui::TrackerSettingsAnswer> asked =
          std::exchange(achievement_windows.tracker_settings, std::nullopt);
      asked.has_value()) {
    tracker.settings_asked = std::move(asked);
  }
  if (const std::optional<ro::ui::TrackerSettingsAnswer> asked =
          std::exchange(tracker.settings_asked, std::nullopt);
      asked.has_value()) {
    Settle(tracker, *asked);
  }
  if (const std::optional<bool> shown = std::exchange(tracker.shown_asked, std::nullopt);
      shown.has_value() && *shown != memory_.shown) {
    memory_.shown = *shown;
    Save();
  }
  tracker.look = memory_.look;
  tracker.shown = memory_.shown;
  achievement_windows.tracker_look = memory_.look;
  // LES PLACES RETENUES SE PRENNENT UNE FOIS PAR PARTIE ; ensuite, c'est celle
  // où le joueur les traîne.
  if (std::exchange(places_pending_, false)) {
    for (std::size_t panel = 0; panel < kTrackerPanelCount; ++panel) {
      if (memory_.places[panel].has_value()) {
        tracker.panels[panel].move_to = *memory_.places[panel];
      }
    }
  }

  // ── LES SECTIONS ─────────────────────────────────────────────────────────
  const std::span<const ro::ui::TrackerEntry> quest_entries = quests.tracker_entries();
  const std::span<const ro::ui::TrackerEntry> achievement_entries =
      achievement_windows.tracker_entries;
  quest_label_ = QuestFill(ro::core::Tr("Quêtes ({})"), {std::to_string(quests.tracker_total())});
  achievement_label_ =
      QuestFill(ro::core::Tr("Succès ({})"), {std::to_string(achievement_entries.size())});
  const ro::ui::TrackerSection quest_section{.kind = ro::ui::TrackerKind::kQuest,
                                             .label = quest_label_,
                                             .entries = quest_entries,
                                             .more = quests.tracker_more()};
  const ro::ui::TrackerSection achievement_section{.kind = ro::ui::TrackerKind::kAchievement,
                                                   .label = achievement_label_,
                                                   .entries = achievement_entries};
  std::array<std::size_t, kTrackerPanelCount> counts{};
  const auto place = [this, &counts](std::size_t panel, const ro::ui::TrackerSection& section) {
    if (!section.entries.empty()) {
      sections_[panel][counts[panel]++] = section;
    }
  };
  place(kMainPanel, quest_section);
  place(memory_.look.split ? kSecondPanel : kMainPanel, achievement_section);
  for (std::size_t panel = 0; panel < kTrackerPanelCount; ++panel) {
    tracker.sections[panel] =
        std::span<const ro::ui::TrackerSection>(sections_[panel]).first(counts[panel]);
    tracker.panels[panel].shown = memory_.shown && counts[panel] > 0;
  }
}

float TrackerPanelFirstTop(std::size_t panel) {
  return panel == kSecondPanel ? kSecondPanelFirstTop : 0.0F;
}

}  // namespace moonclient
