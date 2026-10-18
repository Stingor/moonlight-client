#include "options/damage_meter_options.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "ro/core/i18n.h"

#include "options/option_row_kinds.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kShownKey = "meter_damage";
constexpr const char* kLockedKey = "meter_damage_locked";
constexpr const char* kRecordsKey = "meter_damage_record";
constexpr const char* kTextColorKey = "meter_damage_color_text";
constexpr const char* kPlotColorKey = "meter_damage_color_curve";
constexpr const char* kOpacityKey = "meter_damage_opacity";
constexpr const char* kSlotMsKey = "meter_damage_column_ms";
constexpr const char* kWindowSecsKey = "meter_damage_window_s";
constexpr const char* kCombatTimeoutKey = "meter_damage_end_combat_s";
constexpr const char* kAreaInChatKey = "meter_damage_zone_chat";
constexpr const char* kModeKey = "meter_damage_mode";
constexpr const char* kSegmentKey = "meter_damage_segment";
constexpr const char* kPaneKey = "meter_damage_view";
constexpr const char* kCaptionRateKey = "meter_damage_title_rhythm";
constexpr const char* kCaptionPeakKey = "meter_damage_title_peak";
constexpr const char* kCaptionAverageKey = "meter_damage_title_average";
constexpr const char* kCaptionTotalKey = "meter_damage_title_total";

constexpr const char* kMillisecondsFormat = "%.0f ms";
constexpr const char* kSecondsFormat = "%.0f s";
constexpr const char* kShareFormat = "%.2f";

[[nodiscard]] OptionRowView Toggle(DamageMeterRow row, const char* label, const char* help,
                                   bool on) {
  return OptionRowView{.id = DamageMeterRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kToggle,
                       .on = on};
}

[[nodiscard]] int Rounded(float value, int lowest, int highest) {
  return std::clamp(static_cast<int>(std::lround(value)), lowest, highest);
}

}  // namespace

ro::ui::DamageMeterLook DamageMeterOptions::look() const noexcept {
  ro::ui::DamageMeterLook shown = look_;
  shown.background_opacity = std::clamp(shown.background_opacity, 0.0F, 1.0F);
  return shown;
}

ro::sim::DamageMeter::Settings DamageMeterOptions::meter() const noexcept {
  return ro::sim::DamageMeter::Settings{
      .slot_ms = static_cast<std::uint32_t>(std::clamp(slot_ms_, kFinestSlotMs, kCoarsestSlotMs)),
      .window_secs = static_cast<std::uint32_t>(
          std::clamp(window_secs_, kShortestWindowSecs, kLongestWindowSecs)),
      .combat_timeout_secs = static_cast<std::uint32_t>(
          std::clamp(combat_timeout_secs_, kShortestCombatTimeoutSecs, kLongestCombatTimeoutSecs))};
}

ro::ui::DamageMeterMode DamageMeterOptions::mode() const noexcept {
  return static_cast<ro::ui::DamageMeterMode>(
      std::clamp(mode_, 0, static_cast<int>(ro::ui::kDamageMeterModeCount) - 1));
}

ro::ui::DamageMeterSegment DamageMeterOptions::segment() const noexcept {
  return static_cast<ro::ui::DamageMeterSegment>(
      std::clamp(segment_, 0, static_cast<int>(ro::ui::kDamageMeterSegmentCount) - 1));
}

ro::ui::DamageMeterPane DamageMeterOptions::pane() const noexcept {
  return pane_ == static_cast<int>(ro::ui::DamageMeterPane::kGroup)
             ? ro::ui::DamageMeterPane::kGroup
             : ro::ui::DamageMeterPane::kSelf;
}

bool DamageMeterOptions::TakeResetFight() noexcept {
  return std::exchange(reset_fight_asked_, false);
}

bool DamageMeterOptions::TakeForgetRecords() noexcept {
  return std::exchange(forget_records_asked_, false);
}

void DamageMeterOptions::Declare(SettingsFile& into) {
  into.Declare(kShownKey, &look_.shown);
  into.Declare(kLockedKey, &look_.locked);
  into.Declare(kRecordsKey, &look_.records);
  into.Declare(kTextColorKey, &look_.text_color);
  into.Declare(kPlotColorKey, &look_.plot_color);
  into.Declare(kOpacityKey, &look_.background_opacity);
  into.Declare(kSlotMsKey, &slot_ms_);
  into.Declare(kWindowSecsKey, &window_secs_);
  into.Declare(kCombatTimeoutKey, &combat_timeout_secs_);
  into.Declare(kAreaInChatKey, &area_in_chat_);
  into.Declare(kModeKey, &mode_);
  into.Declare(kSegmentKey, &segment_);
  into.Declare(kPaneKey, &pane_);
  into.Declare(kCaptionRateKey, &caption_.rate);
  into.Declare(kCaptionPeakKey, &caption_.peak);
  into.Declare(kCaptionAverageKey, &caption_.average);
  into.Declare(kCaptionTotalKey, &caption_.total);
}

void DamageMeterOptions::Compose() {
  const ro::sim::DamageMeter::Settings bounded = meter();
  rows_.clear();
  rows_.push_back(OptionRowView{
      .id = DamageMeterRowId(DamageMeterRow::kIntro),
      .label = Tr("Ce que toi et ton groupe infligez, soignez, encaissez et recevez, comme "
                  "Skada ou Recount : le mode se choisit en haut du compteur, et « Groupe » "
                  "montre une barre par membre, avec le détail par skill au clic. Le combat "
                  "s'ouvre au premier coup et se ferme après un silence. Seuls comptent les "
                  "membres à portée de vue : le serveur ne dit rien des autres."),
      .kind = OptionKind::kNote});
  rows_.push_back(Toggle(DamageMeterRow::kShown, Tr("Afficher"), nullptr, look_.shown));
  rows_.push_back(Toggle(DamageMeterRow::kLocked, Tr("Verrouiller (fige + clic-traversant)"),
                         Tr("Fige la fenêtre DPS (position/taille) et laisse passer les clics au "
                            "jeu en dessous.\nSa barre de titre reste vive : la flèche de repli "
                            "et le menu du clic droit y restent à portée."),
                         look_.locked));
  rows_.push_back(Toggle(DamageMeterRow::kRecords, Tr("Palmarès des 10 plus gros coups"),
                         Tr("Un volet sous le bilan : les dix plus gros coups portés depuis le "
                            "début de la session, avec leur skill, leur cible et leur âge. Ils "
                            "survivent aux combats ; ceux du combat en cours portent un liseré.\n"
                            "Le chevron à droite du compteur le déplie aussi."),
                         look_.records));

  rows_.push_back(OptionRowView{.id = DamageMeterRowId(DamageMeterRow::kTextColor),
                                .label = Tr("Couleur texte"),
                                .kind = OptionKind::kColor,
                                .color = look_.text_color});
  rows_.back().heading = Tr("Apparence");
  rows_.push_back(OptionRowView{.id = DamageMeterRowId(DamageMeterRow::kPlotColor),
                                .label = Tr("Couleur graphe"),
                                .kind = OptionKind::kColor,
                                .color = look_.plot_color});
  rows_.push_back(OptionRowView{.id = DamageMeterRowId(DamageMeterRow::kOpacity),
                                .label = Tr("Opacité fond"),
                                .kind = OptionKind::kSlider,
                                .value = look().background_opacity,
                                .lowest = 0.0F,
                                .highest = 1.0F,
                                .format = kShareFormat});

  rows_.push_back(WholeSlider(DamageMeterRowId(DamageMeterRow::kSlotMs), Tr("Résolution (ms/slot)"),
                              Tr("Largeur de chaque colonne du graphique en millisecondes.\nValeur "
                                 "plus basse = graphique plus précis mais moins lisse.\nLe changer "
                                 "remet le graphique à zéro."),
                              static_cast<int>(bounded.slot_ms), kFinestSlotMs, kCoarsestSlotMs,
                              kMillisecondsFormat));
  rows_.back().heading = Tr("Mesure");
  rows_.push_back(WholeSlider(DamageMeterRowId(DamageMeterRow::kWindowSecs), Tr("Fenêtre DPS (s)"),
                              Tr("Fenêtre de temps pour calculer le DPS courant affiché."),
                              static_cast<int>(bounded.window_secs), kShortestWindowSecs,
                              kLongestWindowSecs, kSecondsFormat));
  rows_.push_back(
      WholeSlider(DamageMeterRowId(DamageMeterRow::kCombatTimeout), Tr("Timeout combat (s)"),
                  Tr("Secondes sans dégâts avant de quitter le mode combat."),
                  static_cast<int>(bounded.combat_timeout_secs), kShortestCombatTimeoutSecs,
                  kLongestCombatTimeoutSecs, kSecondsFormat));
  rows_.push_back(OptionRowView{.id = DamageMeterRowId(DamageMeterRow::kResetFight),
                                .label = Tr("Reset graphique"),
                                .help = Tr("Oublie le combat en cours et son graphique. Le "
                                           "palmarès reste."),
                                .kind = OptionKind::kAction});
  rows_.push_back(OptionRowView{.id = DamageMeterRowId(DamageMeterRow::kForgetRecords),
                                .label = Tr("Vider le palmarès"),
                                .kind = OptionKind::kAction,
                                .confirm = Tr("Oublier les dix plus gros coups de la session ?")});

  rows_.push_back(Toggle(DamageMeterRow::kAreaInChat,
                         Tr("Afficher dommages de sorts de zone dans le chat"),
                         Tr("Affiche chaque coup de Storm Gust / Meteor Storm / LoV etc. dans le "
                            "chat, parmi les messages de combat.\nMessage propre au serveur "
                            "Moonlight — le serveur ne montre pas ces dégâts dans le chat "
                            "habituel."),
                         area_in_chat_));
  rows_.back().heading = Tr("Conversation");

  rows_.push_back(Toggle(DamageMeterRow::kCaptionRate, Tr("Rythme courant"),
                         Tr("Le rythme sur la fenêtre glissante — DPS, HPS… selon le mode. Il "
                            "ne s'écrit qu'en combat."),
                         caption_.rate));
  rows_.back().heading = Tr("Titre et languette d'onglet");
  rows_.push_back(Toggle(DamageMeterRow::kCaptionPeak, Tr("Pic"),
                         Tr("Le plus haut rythme atteint pendant le combat."), caption_.peak));
  rows_.push_back(Toggle(DamageMeterRow::kCaptionAverage, Tr("Moyenne"),
                         Tr("Le total du combat divisé par sa durée."), caption_.average));
  rows_.push_back(Toggle(DamageMeterRow::kCaptionTotal, Tr("Total"),
                         Tr("Le total du combat. Ce que tu coches ici se lit sur la barre de "
                            "titre du compteur, même replié, et sur sa languette quand il est "
                            "rangé dans la conversation."),
                         caption_.total));
}

bool DamageMeterOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kDamageMeterRowBase ||
      asked.changed >= kDamageMeterRowBase + static_cast<int>(DamageMeterRow::kCount)) {
    return false;
  }
  switch (static_cast<DamageMeterRow>(asked.changed - kDamageMeterRowBase)) {
    case DamageMeterRow::kShown:
      look_.shown = asked.on;
      break;
    case DamageMeterRow::kLocked:
      look_.locked = asked.on;
      break;
    case DamageMeterRow::kRecords:
      look_.records = asked.on;
      break;
    case DamageMeterRow::kTextColor:
      look_.text_color = asked.color;
      break;
    case DamageMeterRow::kPlotColor:
      look_.plot_color = asked.color;
      break;
    case DamageMeterRow::kOpacity:
      look_.background_opacity = std::clamp(asked.value, 0.0F, 1.0F);
      break;
    case DamageMeterRow::kSlotMs:
      slot_ms_ = Rounded(asked.value, kFinestSlotMs, kCoarsestSlotMs);
      break;
    case DamageMeterRow::kWindowSecs:
      window_secs_ = Rounded(asked.value, kShortestWindowSecs, kLongestWindowSecs);
      break;
    case DamageMeterRow::kCombatTimeout:
      combat_timeout_secs_ =
          Rounded(asked.value, kShortestCombatTimeoutSecs, kLongestCombatTimeoutSecs);
      break;
    case DamageMeterRow::kResetFight:
      reset_fight_asked_ = true;
      break;
    case DamageMeterRow::kForgetRecords:
      forget_records_asked_ = true;
      break;
    case DamageMeterRow::kAreaInChat:
      area_in_chat_ = asked.on;
      break;
    case DamageMeterRow::kCaptionRate:
      caption_.rate = asked.on;
      break;
    case DamageMeterRow::kCaptionPeak:
      caption_.peak = asked.on;
      break;
    case DamageMeterRow::kCaptionAverage:
      caption_.average = asked.on;
      break;
    case DamageMeterRow::kCaptionTotal:
      caption_.total = asked.on;
      break;
    case DamageMeterRow::kIntro:
    case DamageMeterRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
