#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/ui/entity_sample.h"
#include "ro/ui/options_window.h"
#include "ro/ui/path_canvas.h"

#include "options/afk_options.h"
#include "options/battleground_options.h"
#include "options/bug_report_options.h"
#include "options/call_options.h"
#include "options/card_album_options.h"
#include "options/cast_bar_options.h"
#include "options/chat_web_options.h"
#include "options/context_menu_options.h"
#include "options/craft_atlas_options.h"
#include "options/damage_meter_options.h"
#include "options/description_options.h"
#include "options/graphics_options.h"
#include "options/grey_world_options.h"
#include "options/grimoire_key_options.h"
#include "options/make_item_options.h"
#include "options/map_sign_options.h"
#include "options/menu_icons_options.h"
#include "options/minimap_panel_options.h"
#include "options/monster_sheet_options.h"
#include "options/mvp_tracker_options.h"
#include "options/option_sections.h"
#include "options/pet_options.h"
#include "options/pick_precision_options.h"
#include "options/quick_cast_options.h"
#include "options/shop_options.h"
#include "options/skill_view_options.h"
#include "options/skin_look_options.h"
#include "options/skin_options.h"
#include "options/sprite_sound_options.h"
#include "options/target_options.h"
#include "options/view_equip_memory.h"
#include "options/weapon_refine_options.h"
#include "options/whisper_options.h"
#include "options/window_title_options.h"
#include "party/party_options.h"

namespace moonclient {

// CE QUE REND « RÉTABLIR CETTE PAGE ». ⚠ LES RÉGLAGES DU LOGIN N'EN MONTRENT
// QU'UNE PARTIE, et un rétablissement n'y touche qu'à ce qu'on voit.
enum class PageRestore : std::uint8_t {
  kWhole,
  // Sans les sons des monstres et des pets : voir `kSpriteSoundsSettingsPage`.
  kShownAtLogin,
};

static_assert(kPartyGridRowBase + static_cast<int>(PartyGridRow::kCount) <= kMakeItemRowBase,
              "la plage de la grille de groupe chevauche celle de la fabrication");
static_assert(kMakeItemRowBase + static_cast<int>(MakeItemRow::kCount) <= kWeaponRefineRowBase,
              "la plage de la fabrication chevauche celle du refine");
static_assert(kMapSignRowBase + static_cast<int>(MapSignRow::kCount) <= kPartyFriendRowBase,
              "la plage du bandeau de carte chevauche celle de la fenêtre Groupe / Amis");
static_assert(kGreyWorldRowBase + static_cast<int>(GreyWorldRow::kCount) <= kCallRowBase,
              "la plage de l'appel d'urgence chevauche celle de GreyWorld");
static_assert(kSpriteSoundRowBase + static_cast<int>(SpriteSoundRow::kCount) <= kMvpTrackerRowBase,
              "la plage du carnet MVP chevauche celle des sons de sprite");
static_assert(kMvpTrackerRowBase + static_cast<int>(MvpTrackerRow::kCount) <= kBattlegroundRowBase,
              "la plage du champ de bataille chevauche celle du carnet MVP");
static_assert(kBattlegroundRowBase + static_cast<int>(BattlegroundRow::kCount) <= kPetRowBase,
              "la plage du pet chevauche celle du champ de bataille");
static_assert(kSkinLookRowBase + static_cast<int>(SkinLookRow::kCount) <= kChatWebRowBase,
              "la plage des liens du chat chevauche celle de l'habillage");

// LES PAGES DE RÉGLAGES QUI TIENNENT LEUR PROPRE CLASSE — leurs réglages, leurs
// lignes, leurs listes.
//
// ⚠ UN SEUL MEMBRE DE `GameOptions`, et c'est la raison de ce fichier : la
// classe touche ses huit cents lignes. Une page de plus s'ajoute ici, sans elle.
struct OptionPages {
  AfkOptions afk;
  TargetOptions targeting;
  ContextMenuOptions context_menu;
  ViewEquipMemory view_equip;
  GraphicsOptions graphics;
  MonsterSheetOptions monster_sheet;
  CraftAtlasOptions craft_atlas;
  MakeItemOptions make_item;
  WeaponRefineOptions weapon_refine;
  QuickCastOptions quick_cast;
  CardAlbumOptions card_album;
  DescriptionOptions descriptions;
  BugReportOptions bug_report;
  MenuIconsOptions menu_icons;
  MapSignOptions map_sign;
  WindowTitleOptions window_title;
  MinimapPanelOptions minimap_panel;
  SkinOptions skin;
  SkinLookOptions skin_look;
  PartyOptions party;
  CastBarOptions cast_bars;
  PickPrecisionOptions pick_precision;
  DamageMeterOptions damage_meter;
  ShopOptions shops;
  WhisperOptions whispers;
  GreyWorldOptions grey_world;
  GrimoireKeyOptions grimoire_keys;
  SkillViewOptions skill_views;
  SpriteSoundOptions sprite_sounds;
  MvpTrackerOptions mvp_tracker;
  BattlegroundOptions battleground;
  PetOptions pet;
  // ⚠ SANS DÉCLARATION AU FICHIER : c'est le serveur qui tient ce réglage.
  CallOptions call;
  // Les liens, les images et les mentions du chat.
  ChatWebOptions chat_web;
  // L'ENTITÉ DU CADRE D'ESSAI des barres d'entité, composée par la partie avant
  // la page — voir `world/entity_sample.h`. ⚠ PAS UN RÉGLAGE : une vignette.
  ro::ui::EntitySample entity_sample;
  // Le cadre qui le montre, épinglé en haut de la page « Entités ». ⚠ ICI ET
  // NON SUR LA PILE : la section n'en garde qu'une vue.
  std::vector<ro::ui::OptionRowView> entity_pinned_rows;
  // LE CADRE DES TRAJECTOIRES DE DÉGÂTS, composé par la partie quand son onglet
  // est sous les yeux — voir `world/damage_path_sample.h` —, et les rangées du
  // volet de droite qui le montrent. ⚠ PAS UN RÉGLAGE.
  ro::ui::PathSample damage_path_sample;
  std::vector<ro::ui::OptionRowView> damage_side_rows;
  // LE CORPS DE LA PAGE « COMMANDS SETTINGS », posé par la partie : c'est elle
  // qui tient les réglages du serveur et la liaison. ⚠ PAS UN RÉGLAGE.
  ro::ui::OptionSectionBody commands_body;
  // LES PROFILS DE DISPOSITION ET DE RÉGLAGES, sous les réglages de la page
  // « Interface », posés par la partie. Voir `UiLayouts` et `SettingsProfiles`.
  // ⚠ PAS UN RÉGLAGE.
  ro::ui::OptionSectionBody profiles_body;

  // LA SECTION « SKIN » : le choix du dossier, son habillage, puis les profils
  // d'habillage. ⚠ ICI ET NON SUR LA PILE : la section n'en garde qu'une vue.
  std::vector<ro::ui::OptionRowView> skin_rows;
  std::vector<ro::ui::OptionRowView> skin_pinned_rows;
  [[nodiscard]] ro::ui::OptionSectionView ComposeSkinSection();

  // Leurs réglages, chacun sous son intertitre du fichier. ⚠ AVANT LA LECTURE :
  // la valeur présente est le défaut.
  void Declare(SettingsFile& into);

  // LES PAGES DONT UNE LIGNE NE FAIT QU'ÉCRIRE SON RÉGLAGE : vrai quand la
  // ligne était l'une des leurs, et qu'il ne reste qu'à enregistrer.
  [[nodiscard]] bool ChangeSetting(const ro::ui::OptionsWindowAction& asked);

  // CE QUE LA SECTION CONVERSATION PORTE APRÈS SES RÉGLAGES À ELLE : les
  // conversations 1:1, le relai Discord, puis les liens et les images.
  void ComposeChatExtras(std::vector<ro::ui::OptionRowView>& rows);
  // LES PAGES DONT LE RÉGLAGE EST TENU PAR LE SERVEUR : vrai quand la ligne
  // était l'une des leurs. Rien à écrire : le geste est une demande.
  [[nodiscard]] bool ChangeServerHeld(const ro::ui::OptionsWindowAction& asked);

  // LES PAGES QUI PORTENT AU MOINS UN RÉGLAGE DU FICHIER, relevées une fois
  // tout déclaré : elles seules montrent « Rétablir cette page ».
  void NoteRestorable(const SettingsFile& file);
  void MarkRestorable(std::span<ro::ui::OptionSectionView> sections) const;
  std::array<bool, Rank(OptionSection::kCount)> restorable{};

  // REND UNE PAGE À L'ORIGINE : ses réglages du fichier, ce qu'elle tient hors
  // de lui — l'habillage que garde le look.yaml du skin — et, sous « Sound »,
  // les sons des monstres et des pets quand la page entière est montrée.
  void RestorePage(int page, SettingsFile& file, PageRestore reach);
};

}  // namespace moonclient
