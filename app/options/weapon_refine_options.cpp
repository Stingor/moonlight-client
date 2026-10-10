#include "options/weapon_refine_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES.
constexpr const char* kConfirmKey = "refine_confirm";
constexpr const char* kShowCardsKey = "refine_maps";
constexpr const char* kShowFilterKey = "refine_filter";
constexpr const char* kPreviewKey = "refine_preview";
constexpr const char* kEnterKey = "refine_entry";
constexpr const char* kAutoRecastKey = "refine_retry_skill";
constexpr const char* kAutoRefineKey = "refine_chain";
constexpr const char* kHistoryKey = "refine_log";
constexpr const char* kLogTimeKey = "refine_log_hour";

[[nodiscard]] OptionRowView Toggle(WeaponRefineRow row, const char* label, const char* help,
                                   bool on, bool indented = false) {
  return OptionRowView{.id = WeaponRefineRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kToggle,
                       .on = on,
                       .indented = indented};
}

[[nodiscard]] OptionRowView Note(WeaponRefineRow row, const char* text) {
  return OptionRowView{.id = WeaponRefineRowId(row), .label = text, .kind = OptionKind::kNote};
}

}  // namespace

void WeaponRefineOptions::Declare(SettingsFile& into) {
  into.Declare(kConfirmKey, &settings_.confirm);
  into.Declare(kShowCardsKey, &settings_.show_cards);
  into.Declare(kShowFilterKey, &settings_.show_filter);
  into.Declare(kPreviewKey, &settings_.hover_preview);
  into.Declare(kEnterKey, &settings_.enter_key);
  into.Declare(kAutoRecastKey, &settings_.auto_recast);
  into.Declare(kAutoRefineKey, &settings_.auto_refine);
  into.Declare(kHistoryKey, &settings_.show_history);
  into.Declare(kLogTimeKey, &settings_.log_time);
}

void WeaponRefineOptions::Compose() {
  rows_.clear();
  rows_.push_back(
      Note(WeaponRefineRow::kIntro,
           Tr("La fenêtre du skill Upgrade Weapon : les armes que le serveur propose d'affiner.")));
  rows_.push_back(Note(WeaponRefineRow::kGestures,
                       Tr("Clic droit : description · double-clic ou Entrée : refine.")));
  rows_.push_back(Note(WeaponRefineRow::kSorting,
                       Tr("En-têtes de colonne : trier (3e clic = ordre d'inventory).")));
  rows_.push_back(Toggle(WeaponRefineRow::kConfirm, Tr("Confirmer avant un refine"),
                         Tr("Un échec DÉTRUIT l'arme, et le client affiche le MÊME message pour un "
                            "succès et un échec. La confirmation rappelle l'arme visée et le "
                            "risque."),
                         settings_.confirm));
  rows_.push_back(Toggle(WeaponRefineRow::kShowCards, Tr("Cartes et emplacements"),
                         Tr("Le paquet du serveur porte les 4 cartes de chaque arme — la fenêtre "
                            "du jeu les jette, deux armes identiques dont une sertie y sont donc "
                            "indistinguables."),
                         settings_.show_cards));
  rows_.push_back(Toggle(WeaponRefineRow::kShowFilter, Tr("Champ de filtre"),
                         Tr("Utile sur un gros inventory ; sur deux ou trois armes il ne fait que "
                            "prendre une ligne. Le décocher efface aussi le filtre en cours, pour "
                            "qu'aucune arme ne reste masquée par un champ invisible."),
                         settings_.show_filter));
  rows_.push_back(Toggle(WeaponRefineRow::kHoverPreview, Tr("Description au survol"), nullptr,
                         settings_.hover_preview));
  rows_.push_back(Toggle(WeaponRefineRow::kEnterKey, Tr("Entrée lance le refine"),
                         Tr("Cochée, Entrée refine l'arme sélectionnée quand la fenêtre a le "
                            "clavier, et la maintenir enchaîne ; la fenêtre garde alors la touche "
                            "tant qu'elle est ouverte, et le chat ne s'ouvre plus. Décoché "
                            "(défaut), la touche Entrée reste au CHAT. La fenêtre de CONFIRMATION "
                            "garde Entrée dans tous les cas : elle valide une action qui peut "
                            "détruire l'arme."),
                         settings_.enter_key));
  rows_.push_back(Toggle(
      WeaponRefineRow::kAutoRecast, Tr("Relancer le skill automatiquement"),
      Tr("Après chaque tentative, relance Upgrade Weapon pour rouvrir la liste — le serveur n'en "
         "autorise qu'une par lancement. Ne refine RIEN tout seul : le choix de l'arme et le "
         "déclenchement restent des clics. La chaîne s'arrête d'elle-même quand la liste revient "
         "vide, quand il n'y a plus de minerai, ou quand le serveur refuse une condition. Si le "
         "skill est jeté par son délai de lancement, la relance est réessayée un peu plus "
         "tard (3 fois au plus) au lieu de s'arrêter en silence."),
      settings_.auto_recast));
  rows_.push_back(Toggle(
      WeaponRefineRow::kAutoRefine, Tr("Refiner automatiquement (chaîne complète)"),
      Tr("Enchaîne TOUT SEUL : la première arme de la liste est jouée, le skill relancée, et "
         "ainsi de suite jusqu'à ne plus pouvoir — c'est le SP qui borne la chaîne. Implique la "
         "relance automatique. La confirmation est IGNORÉE. L'arme visée est celle qui est "
         "sélectionnée si elle est encore là, sinon la première ligne AFFICHÉE — filtre et tri "
         "compris. S'arrête seule : plus de SP, plus de minerai, plus d'arme dans la liste, ou "
         "refus du serveur. Le bouton « Chaîne auto » de la fenêtre coche ce réglage, et "
         "« Arrêter » le décoche. Coché ou non, la chaîne ne part qu'après une confirmation "
         "dans la fenêtre ouverte : rouverte, elle redemande."),
      settings_.auto_refine));
  if (settings_.auto_refine) {
    OptionRowView warning = Note(WeaponRefineRow::kAutoRefineWarning,
                                 Tr("Chaque tentative peut DÉTRUIRE l'arme, et elles partent sans "
                                    "confirmation. La chaîne joue les armes de la liste jusqu'à "
                                    "épuisement du SP."));
    warning.indented = true;
    warning.warning = true;
    rows_.push_back(warning);
  }
  rows_.push_back(Toggle(WeaponRefineRow::kHistory, Tr("Journal de session"),
                         Tr("Garde la trace des tentatives de la session, avec le libellé EXACT du "
                            "serveur. C'est le seul endroit où succès et échec se distinguent : "
                            "le client leur donne le MÊME texte."),
                         settings_.show_history));
  if (settings_.show_history) {
    rows_.push_back(Toggle(WeaponRefineRow::kLogTime, Tr("Horodater les lignes"), nullptr,
                           settings_.log_time, true));
  }
}

bool WeaponRefineOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kWeaponRefineRowBase ||
      asked.changed >= kWeaponRefineRowBase + static_cast<int>(WeaponRefineRow::kCount)) {
    return false;
  }
  switch (static_cast<WeaponRefineRow>(asked.changed - kWeaponRefineRowBase)) {
    case WeaponRefineRow::kConfirm:
      settings_.confirm = asked.on;
      break;
    case WeaponRefineRow::kShowCards:
      settings_.show_cards = asked.on;
      break;
    case WeaponRefineRow::kShowFilter:
      settings_.show_filter = asked.on;
      break;
    case WeaponRefineRow::kHoverPreview:
      settings_.hover_preview = asked.on;
      break;
    case WeaponRefineRow::kEnterKey:
      settings_.enter_key = asked.on;
      break;
    case WeaponRefineRow::kAutoRecast:
      settings_.auto_recast = asked.on;
      break;
    case WeaponRefineRow::kAutoRefine:
      settings_.auto_refine = asked.on;
      break;
    case WeaponRefineRow::kHistory:
      settings_.show_history = asked.on;
      break;
    case WeaponRefineRow::kLogTime:
      settings_.log_time = asked.on;
      break;
    case WeaponRefineRow::kIntro:
    case WeaponRefineRow::kGestures:
    case WeaponRefineRow::kSorting:
    case WeaponRefineRow::kAutoRefineWarning:
    case WeaponRefineRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
