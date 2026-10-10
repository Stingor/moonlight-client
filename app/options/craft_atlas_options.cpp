#include "options/craft_atlas_options.h"

#include <array>
#include <cstdio>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kPreviewKey = "atlas_recipes_preview";
constexpr const char* kOnlyCraftableKey = "atlas_recipes_doable";
constexpr const char* kShowUnlearnableKey = "atlas_recipes_unplayable";

constexpr std::size_t kTallyBytes = 192;

[[nodiscard]] OptionRowView Toggle(CraftAtlasRow row, const char* label, const char* help,
                                   bool on) {
  return OptionRowView{.id = CraftAtlasRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kToggle,
                       .on = on};
}

[[nodiscard]] OptionRowView Note(CraftAtlasRow row, const char* text) {
  return OptionRowView{.id = CraftAtlasRowId(row), .label = text, .kind = OptionKind::kNote};
}

}  // namespace

void CraftAtlasOptions::SetOnlyCraftable(bool on, SettingsFile* file) {
  only_craftable_ = on;
  if (file != nullptr) {
    file->Touch();
  }
}

void CraftAtlasOptions::Declare(SettingsFile& into) {
  into.Declare(kPreviewKey, &preview_);
  into.Declare(kOnlyCraftableKey, &only_craftable_);
  into.Declare(kShowUnlearnableKey, &show_unlearnable_);
}

void CraftAtlasOptions::Compose() {
  rows_.clear();
  rows_.push_back(
      Note(CraftAtlasRow::kIntro,
           Tr("L'Atlas répertorie tout ce que le serveur sait fabriquer : par skill, par produit, "
              "par matériau, et les flèches. Le jeu ne peut pas le faire seul — le serveur "
              "n'envoie une liste qu'après le lancement d'un skill.")));
  // ⚠ UN BOUTON ET NON UNE CASE : une case liée à l'ouverture se décochait
  // toute seule dès qu'on fermait l'Atlas par sa croix. On clique ici pour VOIR
  // l'Atlas, jamais pour le refermer — il est derrière le panneau, et un clic
  // qui le fermerait ressemblerait à un clic sans effet.
  rows_.push_back(OptionRowView{
      .id = CraftAtlasRowId(CraftAtlasRow::kOpen),
      .label = Tr("Ouvrir l'Atlas"),
      .help = Tr("Aussi par la commande /atlas, ou par la touche qu'on lui donne dans la "
                 "configuration des raccourcis."),
      .command = "/atlas",
      .kind = OptionKind::kAction});
  rows_.push_back(
      Toggle(CraftAtlasRow::kPreview, Tr("Aperçu de l'item au survol"), nullptr, preview_));
  rows_.push_back(Toggle(CraftAtlasRow::kOnlyCraftable, Tr("Ne montrer que ce qui est réalisable"),
                         nullptr, only_craftable_));
  rows_.push_back(Toggle(
      CraftAtlasRow::kShowUnlearnable, Tr("Montrer aussi les recettes injouables sur ce serveur"),
      Tr("Le serveur a des recettes dont aucune classe n'apprend le skill. Elles sont masquées "
         "par défaut : il les refuserait. Le critère est l'arbre des skills du serveur, pas le "
         "mode renewal."),
      show_unlearnable_));
  rows_.push_back(Note(CraftAtlasRow::kGestures,
                       Tr("Dans les listes : clic gauche pour suivre un item, clic droit pour son "
                          "menu, Maj + clic pour poser son lien dans le chat.")));
  if (tally_.available) {
    std::array<char, kTallyBytes> text{};
    std::snprintf(text.data(), text.size(),
                  Tr("%zu recettes, %zu matériaux, %zu transformations de flèches."),
                  tally_.recipes, tally_.materials, tally_.arrows);
    tally_text_ = text.data();
  } else {
    tally_text_ = Tr("Fichier de recettes absent : SystemEN\\craft_recipes.yml");
  }
  // ⚠ LA VUE APRÈS L'ÉCRITURE DE LA CHAÎNE : elle pointe dedans.
  rows_.push_back(Note(CraftAtlasRow::kTally, tally_text_.c_str()));
}

bool CraftAtlasOptions::Change(const ro::ui::OptionsWindowAction& asked,
                               ro::ui::CraftAtlasWindowState* window) {
  if (asked.changed < kCraftAtlasRowBase ||
      asked.changed >= kCraftAtlasRowBase + static_cast<int>(CraftAtlasRow::kCount)) {
    return false;
  }
  switch (static_cast<CraftAtlasRow>(asked.changed - kCraftAtlasRowBase)) {
    case CraftAtlasRow::kOpen:
      if (window != nullptr) {
        window->open = true;
        window->raise = true;
      }
      break;
    case CraftAtlasRow::kPreview:
      preview_ = asked.on;
      break;
    case CraftAtlasRow::kOnlyCraftable:
      only_craftable_ = asked.on;
      break;
    case CraftAtlasRow::kShowUnlearnable:
      show_unlearnable_ = asked.on;
      break;
    case CraftAtlasRow::kIntro:
    case CraftAtlasRow::kGestures:
    case CraftAtlasRow::kTally:
    case CraftAtlasRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
