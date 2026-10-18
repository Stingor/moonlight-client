#include "options/context_menu_options.h"

#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kAllEntitiesKey = "menu_entity_everywhere";
constexpr const char* kSelfMenuKey = "menu_entity_on_oneself";
constexpr const char* kStaffExtrasKey = "menu_entity_staff";
constexpr const char* kNpcBlockKey = "menu_entity_blocking_npc";

[[nodiscard]] OptionRowView Toggle(ContextMenuRow row, const char* label, const char* help,
                                   bool on) {
  return OptionRowView{.id = ContextMenuRowId(row),
                       .label = label,
                       .help = help,
                       .kind = OptionKind::kToggle,
                       .on = on};
}

[[nodiscard]] OptionRowView Note(ContextMenuRow row, const char* text) {
  return OptionRowView{.id = ContextMenuRowId(row), .label = text, .kind = OptionKind::kNote};
}

}  // namespace

void ContextMenuOptions::Declare(SettingsFile& into) {
  into.Declare(kAllEntitiesKey, &settings_.all_entities);
  into.Declare(kSelfMenuKey, &settings_.self_menu);
  into.Declare(kStaffExtrasKey, &settings_.staff_extras);
  into.Declare(kNpcBlockKey, &settings_.npc_block);
}

void ContextMenuOptions::Compose() {
  rows_.clear();
  listed_.clear();
  names_.clear();
  notes_.clear();

  rows_.push_back(Note(ContextMenuRow::kIntro,
                       Tr("Le menu du clic droit sur une entité. Le client d'origine ne l'ouvrait "
                          "que sur un joueur, son pet, son homoncule et son mercenaire — ici il "
                          "peut aussi s'ouvrir sur les monstres et les NPC.")));
  rows_.push_back(Toggle(ContextMenuRow::kAllEntities, Tr("Sur toutes les entités"),
                         Tr("Le client d'origine n'ouvrait de menu que sur un joueur, son pet, son "
                            "homoncule ou son mercenaire. Coché, le menu s'ouvre aussi sur les "
                            "monstres, les NPC et les items au sol (ramasser, description)."),
                         settings_.all_entities));
  // SOUS-RÉGLAGE : sans « toutes les entités », le menu ne s'ouvre de toute
  // façon jamais sur soi. Grisé plutôt que retiré — une case qui disparaît ne
  // dit pas ce qu'elle attendait.
  OptionRowView self = Toggle(
      ContextMenuRow::kSelfMenu, Tr("Sur soi-même"),
      Tr("Le menu sur son propre personnage ne porte que « Copier mon nom » (plus les "
         "identifiants sous « Outils du staff »). Décoché, le clic droit sur soi-même repart au "
         "jeu sans rien ouvrir ni rien avaler.\n"
         "Sans « Sur toutes les entités », ce menu ne s'ouvre de toute façon jamais : le client "
         "n'en a jamais eu sur soi."),
      settings_.self_menu);
  self.indented = true;
  self.disabled = !settings_.all_entities;
  rows_.push_back(self);

  // LE BLOCAGE : l'interrupteur général et la LISTE, pour reprendre d'un geste
  // ce qu'on a bloqué au fil du jeu — la case, elle, se coche dans le menu.
  rows_.push_back(Toggle(
      ContextMenuRow::kNpcBlock, Tr("Blocage du clic sur les NPC de service"),
      Tr("Les NPC de la capitale (kafra, job master, styliste, warpers…) se tiennent au milieu "
         "du passage et s'ouvrent quand on cherchait à marcher. Clic droit sur l'un d'eux, puis "
         "« Bloquer le clic gauche » : il devient une zone morte, le clic ne déclenche plus rien.\n"
         "Décoché ici, la case n'est plus proposée et les NPC déjà bloqués redeviennent "
         "cliquables — la liste, elle, est conservée."),
      settings_.npc_block));
  if (blocked_.empty()) {
    OptionRowView none = Note(ContextMenuRow::kNoBlockedNpc, Tr("Aucun NPC bloqué."));
    none.indented = true;
    rows_.push_back(none);
  } else {
    // ⚠ LES TEXTES D'ABORD, LES VUES ENSUITE : voir `names_`.
    for (const auto& [id, name] : blocked_.all()) {
      if (listed_.size() >= static_cast<std::size_t>(kBlockedNpcRowsMost)) {
        break;
      }
      listed_.push_back(id);
      // Le GID n'est dit à part que si le nom le cache : sinon il est déjà écrit.
      std::string note;
      if (name.empty()) {
        names_.emplace_back(ro::core::Printed(Tr("NPC %u"), id));
      } else {
        names_.push_back(name);
        note = ro::core::Printed("(%u)", id);
      }
      notes_.emplace_back(note);
    }
    // Une ligne par PNJ, avec de quoi le débloquer sans avoir à le retrouver en
    // jeu : il peut être sur une autre carte.
    for (std::size_t rank = 0; rank < listed_.size(); ++rank) {
      rows_.push_back(
          OptionRowView{.id = kBlockedNpcRowBase + static_cast<int>(rank),
                        .label = Tr("Débloquer"),
                        .kind = OptionKind::kAction,
                        .object = names_[rank].c_str(),
                        .object_note = notes_[rank].empty() ? nullptr : notes_[rank].c_str(),
                        .small_button = true,
                        .indented = true});
    }
  }

  // ⚠ SEULEMENT POUR LE STAFF : ailleurs, la case ne pourrait rien faire.
  if (staff_) {
    rows_.push_back(
        Toggle(ContextMenuRow::kStaffExtras, Tr("Outils du staff"),
               Tr("Ajoute au menu les actions et les identifiants réservés au staff.\n"
                  "Coché, le menu s'ouvre alors sur TOUTES les entités sans attendre le réglage du "
                  "dessus : monstres, NPC, unités de skill, items au sol. C'est justement sur "
                  "les cibles inattendues qu'on en a besoin."),
               settings_.staff_extras));
  }
}

bool ContextMenuOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed >= kBlockedNpcRowBase &&
      asked.changed < kBlockedNpcRowBase + kBlockedNpcRowsMost) {
    const auto rank = static_cast<std::size_t>(asked.changed - kBlockedNpcRowBase);
    if (rank < listed_.size() && blocked_.Remove(listed_[rank])) {
      blocked_changed_ = true;
    }
    return true;
  }
  if (asked.changed < kContextMenuRowBase ||
      asked.changed >= kContextMenuRowBase + static_cast<int>(ContextMenuRow::kCount)) {
    return false;
  }
  switch (static_cast<ContextMenuRow>(asked.changed - kContextMenuRowBase)) {
    case ContextMenuRow::kAllEntities:
      settings_.all_entities = asked.on;
      break;
    case ContextMenuRow::kSelfMenu:
      settings_.self_menu = asked.on;
      break;
    case ContextMenuRow::kNpcBlock:
      settings_.npc_block = asked.on;
      break;
    case ContextMenuRow::kStaffExtras:
      settings_.staff_extras = asked.on;
      break;
    case ContextMenuRow::kIntro:
    case ContextMenuRow::kNoBlockedNpc:
    case ContextMenuRow::kCount:
      break;
  }
  return true;
}

bool ContextMenuOptions::TakeBlockedChanged() noexcept {
  return std::exchange(blocked_changed_, false);
}

}  // namespace moonclient
