#include "options/make_item_options.h"

#include <algorithm>
#include <cmath>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kShowOwnedKey = "crafting_stock";
constexpr const char* kShowFilterKey = "crafting_filter";
constexpr const char* kPreviewKey = "crafting_preview";
constexpr const char* kHistoryKey = "crafting_log";
constexpr const char* kLogTimeKey = "crafting_log_hour";
constexpr const char* kEnterKey = "crafting_entry";
constexpr const char* kAutoRecastKey = "crafting_retry_skill";
constexpr const char* kAutoReuseKey = "crafting_retry_item";
constexpr const char* kReuseMaxKey = "crafting_retry_item_ceiling";

// Le plafond qu'on peut choisir, en exemplaires.
constexpr int kLeastReuseCap = 1;
constexpr int kMostReuseCap = 50;

[[nodiscard]] OptionRowView Toggle(MakeItemRow row, const char* label, const char* help, bool on,
                                   bool indented = false) {
  return OptionRowView{.id = MakeItemRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kToggle,
                       .on = on,
                       .indented = indented};
}

[[nodiscard]] OptionRowView Note(MakeItemRow row, const char* text) {
  return OptionRowView{.id = MakeItemRowId(row), .label = text, .kind = OptionKind::kNote};
}

}  // namespace

void MakeItemOptions::Declare(SettingsFile& into) {
  into.Declare(kShowOwnedKey, &settings_.show_owned);
  into.Declare(kShowFilterKey, &settings_.show_filter);
  into.Declare(kPreviewKey, &settings_.hover_preview);
  into.Declare(kHistoryKey, &settings_.show_history);
  into.Declare(kLogTimeKey, &settings_.log_time);
  into.Declare(kEnterKey, &settings_.enter_key);
  into.Declare(kAutoRecastKey, &settings_.auto_recast);
  into.Declare(kAutoReuseKey, &settings_.auto_reuse_item);
  into.Declare(kReuseMaxKey, &settings_.auto_reuse_max);
}

void MakeItemOptions::Compose() {
  rows_.clear();
  rows_.push_back(
      Note(MakeItemRow::kIntro,
           Tr("S'ouvre sur les DEUX listes de fabrication du serveur : « LIST » (flèches, "
              "convertisseurs, poison, leurres, cuisine, bombes) et « Manufacturing "
              "List » (pharmacie, runes, forge).")));
  rows_.push_back(Note(MakeItemRow::kGestures,
                       Tr("Clic droit : description · double-clic ou Entrée : fabriquer.")));
  rows_.push_back(
      Note(MakeItemRow::kSorting, Tr("En-têtes de colonne : trier (3e clic = ordre du serveur).")));
  rows_.push_back(Toggle(
      MakeItemRow::kShowOwned, Tr("Colonnes « Possédé » et « Faisable »"),
      Tr("Le stock en inventory, et le nombre de fabrications possibles déduit de la recette. "
         "« — » signifie « on ne sait pas » : la recette manque au fichier, ou un matériau n'a "
         "pas pu être résolu. Jamais « 0 » — le serveur n'aurait pas proposé le produit s'il n'y "
         "avait pas de quoi en faire au moins un."),
      settings_.show_owned));
  rows_.push_back(
      Toggle(MakeItemRow::kShowFilter, Tr("Champ de filtre"), nullptr, settings_.show_filter));
  rows_.push_back(
      Toggle(MakeItemRow::kHoverPreview, Tr("Aperçu au survol"), nullptr, settings_.hover_preview));
  rows_.push_back(
      Toggle(MakeItemRow::kHistory, Tr("Journal de session"), nullptr, settings_.show_history));
  if (settings_.show_history) {
    rows_.push_back(Toggle(MakeItemRow::kLogTime, Tr("Horodater le journal"), nullptr,
                           settings_.log_time, true));
  }
  rows_.push_back(Toggle(
      MakeItemRow::kEnterKey, Tr("Entrée lance la fabrication"),
      Tr("Décoché (défaut), la touche Entrée reste au CHAT. Cochée, elle déclenche la fabrication "
         "du produit sélectionné — et la maintenir enchaîne. Mais la fenêtre garde alors la "
         "touche tant qu'elle est ouverte : impossible d'ouvrir la saisie du chat. Depuis que le "
         "champ Quantité existe, marteler Entrée n'a plus grand intérêt : « ×20 » fait le même "
         "travail sans occuper le clavier."),
      settings_.enter_key));
  rows_.push_back(Toggle(
      MakeItemRow::kAutoRecast, Tr("Relancer le skill automatiquement"),
      Tr("Le serveur n'autorise qu'UNE fabrication par lancement de skill. Ce qui se relance "
         "seul, c'est le LANCEMENT DU SKILL — jamais la fabrication : le choix du produit "
         "et le déclenchement restent des clics. La chaîne s'arrête d'elle-même sur une liste "
         "vide ou sur un refus de skill. NE S'APPLIQUE PAS aux listes ouvertes par un ITEM "
         "(Mini Furnace, marteaux de forge) : ces items sont CONSOMMÉS à chaque usage, et ont "
         "leur propre réglage, juste en dessous."),
      settings_.auto_recast));
  rows_.push_back(Toggle(
      MakeItemRow::kAutoReuseItem, Tr("Ré-utiliser l'ITEM automatiquement (le consomme)"),
      Tr("Pour les listes ouvertes par un item — Mini Furnace, marteaux de forge. /!\\ CHAQUE "
         "RELANCE DÉTRUIT UN EXEMPLAIRE : le serveur supprime l'item AVANT d'exécuter son "
         "script, même si tu annules ensuite, et même si la fabrication rate. Ce qui se relance "
         "est l'OUVERTURE de la liste, jamais la fabrication. La chaîne s'arrête sur une liste "
         "vide, sur un refus du serveur, dès qu'il ne te reste plus l'item, ou si tu fermes la "
         "fenêtre. Le nombre d'exemplaires consommés reste affiché pendant toute la chaîne."),
      settings_.auto_reuse_item));
  if (settings_.auto_reuse_item) {
    const bool unlimited = settings_.auto_reuse_max == 0;
    rows_.push_back(Toggle(
        MakeItemRow::kUnlimited, Tr("Sans limite (jusqu'à épuisement du stock)"),
        Tr("Sans limite, la chaîne va jusqu'à épuisement du stock. C'est le défaut — consommer "
           "toute une pile est ton choix. Le plafond n'existe que pour le seul cas qu'il couvre "
           "vraiment : avoir oublié ce réglage coché. Une chaîne illimitée ne peut pas "
           "s'emballer : chaque tour exige un résultat du serveur pour armer le suivant."),
        unlimited, true));
    if (!unlimited) {
      rows_.push_back(OptionRowView{.id = MakeItemRowId(MakeItemRow::kReuseCap),
                                    .label = Tr("Exemplaires au maximum"),
                                    .kind = OptionKind::kSlider,
                                    .value = static_cast<float>(settings_.auto_reuse_max),
                                    .lowest = static_cast<float>(kLeastReuseCap),
                                    .highest = static_cast<float>(kMostReuseCap),
                                    .format = "%.0f",
                                    .indented = true});
    }
  }
}

bool MakeItemOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kMakeItemRowBase ||
      asked.changed >= kMakeItemRowBase + static_cast<int>(MakeItemRow::kCount)) {
    return false;
  }
  switch (static_cast<MakeItemRow>(asked.changed - kMakeItemRowBase)) {
    case MakeItemRow::kShowOwned:
      settings_.show_owned = asked.on;
      break;
    case MakeItemRow::kShowFilter:
      settings_.show_filter = asked.on;
      break;
    case MakeItemRow::kHoverPreview:
      settings_.hover_preview = asked.on;
      break;
    case MakeItemRow::kHistory:
      settings_.show_history = asked.on;
      break;
    case MakeItemRow::kLogTime:
      settings_.log_time = asked.on;
      break;
    case MakeItemRow::kEnterKey:
      settings_.enter_key = asked.on;
      break;
    case MakeItemRow::kAutoRecast:
      settings_.auto_recast = asked.on;
      break;
    case MakeItemRow::kAutoReuseItem:
      settings_.auto_reuse_item = asked.on;
      break;
    case MakeItemRow::kUnlimited:
      settings_.auto_reuse_max = asked.on ? 0 : reuse_cap_;
      break;
    case MakeItemRow::kReuseCap:
      settings_.auto_reuse_max =
          std::clamp(static_cast<int>(std::lround(asked.value)), kLeastReuseCap, kMostReuseCap);
      reuse_cap_ = settings_.auto_reuse_max;
      break;
    case MakeItemRow::kIntro:
    case MakeItemRow::kGestures:
    case MakeItemRow::kSorting:
    case MakeItemRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
