#include "options/option_pages.h"

#include "options/option_sections.h"

namespace moonclient {
namespace {

// Déclare les réglages d'une page sous la section du panneau qui les montre.
template <typename Page>
void DeclareOn(SettingsFile& into, OptionSection section, Page& page) {
  const SettingsFile::PageScope scope(into, SettingsPage(section));
  page.Declare(into);
}

}  // namespace

void OptionPages::Declare(SettingsFile& into) {
  into.Section("Les graphismes");
  DeclareOn(into, OptionSection::kGraphics, graphics);
  into.Section("L'écran de veille");
  DeclareOn(into, OptionSection::kAfkScreen, afk);
  into.Section("Le mode Ciblage et son HUD");
  DeclareOn(into, OptionSection::kTarget, targeting);
  into.Section("Le menu du clic droit sur une entité");
  DeclareOn(into, OptionSection::kContextMenu, context_menu);
  // AUCUNE PAGE : ce que la fenêtre retient d'une ouverture à l'autre.
  into.Section("La fenêtre « Voir l'équipement »");
  view_equip.Declare(into);
  into.Section("La fiche d'un monstre");
  DeclareOn(into, OptionSection::kMonsterSheet, monster_sheet);
  into.Section("L'atlas des recettes");
  DeclareOn(into, OptionSection::kCraftAtlas, craft_atlas);
  into.Section("La fabrication");
  DeclareOn(into, OptionSection::kMakeItem, make_item);
  into.Section("Le refine d'arme");
  DeclareOn(into, OptionSection::kWeaponRefine, weapon_refine);
  into.Section("Quick cast");
  DeclareOn(into, OptionSection::kGameplay, quick_cast);
  into.Section("L'album de cartes");
  DeclareOn(into, OptionSection::kCardAlbum, card_album);
  into.Section("Les descriptions");
  DeclareOn(into, OptionSection::kDescriptions, descriptions);
  into.Section("Le rapport de bug");
  DeclareOn(into, OptionSection::kInterface, bug_report);
  into.Section("Les icônes du menu");
  DeclareOn(into, OptionSection::kMenuIcons, menu_icons);
  into.Section("Le bandeau du nom de la carte");
  DeclareOn(into, OptionSection::kInterface, map_sign);
  into.Section("Le titre de la fenêtre");
  DeclareOn(into, OptionSection::kInterface, window_title);
  // ⚠ LA PAGE DU SKIN TIENT AUSSI L'HABILLAGE, rangé dans un autre fichier :
  // « Rétablir cette page » le rend avec elle, voir `RestorePage`.
  into.Section("Le skin");
  DeclareOn(into, OptionSection::kSkin, skin);
  into.Section("Le groupe, les amis et la grille de groupe");
  {
    const SettingsFile::PageScope scope(into, SettingsPage(OptionSection::kPartyFriend));
    party.DeclareWindow(into);
  }
  {
    const SettingsFile::PageScope scope(into, SettingsPage(OptionSection::kPartyGrid));
    party.DeclareGrid(into);
  }
  into.Section("Les barres d'incantation");
  DeclareOn(into, OptionSection::kEntityBars, cast_bars);
  into.Section("La précision du ciblage");
  DeclareOn(into, OptionSection::kEntityBars, pick_precision);
  into.Section("Le compteur de dégâts");
  DeclareOn(into, OptionSection::kDamageMeter, damage_meter);
  into.Section("Les échoppes des autres");
  DeclareOn(into, OptionSection::kGameplay, shops);
  into.Section("Les conversations 1:1");
  DeclareOn(into, OptionSection::kChat, whispers);
  into.Section("Les liens et les images du chat");
  DeclareOn(into, OptionSection::kChat, chat_web);
  into.Section("GreyWorld");
  DeclareOn(into, OptionSection::kGameplay, grey_world);
  // AUCUNE PAGE : le mode et la vue d'un arbre se choisissent dans l'arbre.
  into.Section("Le grimoire");
  grimoire_keys.Declare(into);
  skill_views.Declare(into);
  into.Section("Les sons des monstres et des pets");
  {
    const SettingsFile::PageScope scope(into, kSpriteSoundsSettingsPage);
    sprite_sounds.Declare(into);
  }
  into.Section("Le carnet de chasse MVP");
  DeclareOn(into, OptionSection::kMvpTracker, mvp_tracker);
  into.Section("Le champ de bataille");
  DeclareOn(into, OptionSection::kGameplay, battleground);
  into.Section("La fiche du pet");
  DeclareOn(into, OptionSection::kPet, pet);
}

ro::ui::OptionSectionView OptionPages::ComposeSkinSection() {
  skin_rows.clear();
  skin.ComposeInto(skin_rows);
  skin_look.ComposeInto(skin_rows);
  skin_pinned_rows.clear();
  skin_look.ComposeSample(skin_pinned_rows);
  return ro::ui::OptionSectionView{.title = ro::core::Tr("Skin"),
                                   .rows = skin_rows,
                                   .pinned_rows = skin_pinned_rows,
                                   .body = skin_look.ProfilesBody()};
}

void OptionPages::NoteRestorable(const SettingsFile& file) {
  for (std::size_t rank = 0; rank < restorable.size(); ++rank) {
    restorable[rank] = file.HasPage(static_cast<int>(rank));
  }
}

void OptionPages::MarkRestorable(std::span<ro::ui::OptionSectionView> sections) const {
  for (std::size_t rank = 0; rank < sections.size() && rank < restorable.size(); ++rank) {
    sections[rank].restorable = restorable[rank];
  }
}

void OptionPages::RestorePage(int page, SettingsFile& file, PageRestore reach) {
  file.RestoreDefaults(page);
  if (page == SettingsPage(OptionSection::kSound) && reach == PageRestore::kWhole) {
    file.RestoreDefaults(kSpriteSoundsSettingsPage);
  }
  if (page == SettingsPage(OptionSection::kSkin)) {
    skin_look.RestoreDefaults();
  }
}

bool OptionPages::ChangeSetting(const ro::ui::OptionsWindowAction& asked) {
  return menu_icons.Change(asked) || map_sign.Change(asked) || window_title.Change(asked) ||
         minimap_panel.Change(asked) || skin.Change(asked) || skin_look.Change(asked) ||
         party.Change(asked) || cast_bars.Change(asked) || pick_precision.Change(asked) ||
         damage_meter.Change(asked) || shops.Change(asked) || whispers.Change(asked) ||
         grey_world.Change(asked) || sprite_sounds.Change(asked) || mvp_tracker.Change(asked) ||
         battleground.Change(asked) || pet.Change(asked) || quest_tracker.Change(asked) ||
         chat_web.Change(asked);
}

void OptionPages::ComposeChatExtras(std::vector<ro::ui::OptionRowView>& rows) {
  whispers.ComposeInto(rows);
  chat_web.ComposeInto(rows);
}

bool OptionPages::ChangeServerHeld(const ro::ui::OptionsWindowAction& asked) {
  return call.Change(asked);
}

}  // namespace moonclient
