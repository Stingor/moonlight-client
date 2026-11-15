#pragma once

// ⚠ CET EN-TÊTE NE TIRE AUCUNE PAGE : chacune est tenue par `Held`, et une
// source qui en lit une inclut son en-tête. Constructeur et destructeur vivent
// dans `option_pages.cpp`, seule source où elles sont toutes complètes.

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "ro/ui/entity_sample.h"
#include "ro/ui/options_search.h"
#include "ro/ui/options_window.h"
#include "ro/ui/path_canvas.h"

#include "game/held.h"
#include "options/option_sections.h"

namespace moonclient {

class SettingsFile;
class AfkOptions;
class BattlegroundOptions;
class BroadcastBalloonOptions;
class BugReportOptions;
class CallOptions;
class CardAlbumOptions;
class CastBarOptions;
class ChatWebOptions;
struct CompanionZonesMemory;
class ContextMenuOptions;
class CraftAtlasOptions;
class DamageMeterOptions;
class DealRefusalOptions;
class DescriptionOptions;
class GraphicsOptions;
class GreyWorldOptions;
class GrimoireKeyOptions;
class MakeItemOptions;
class MapSignOptions;
class MenuIconsOptions;
class MinimapPanelOptions;
class MonsterSheetOptions;
class MvpTrackerOptions;
class PartyOptions;
class PetOptions;
class PickPrecisionOptions;
class QuestTrackerOptions;
class QuickCastOptions;
class ShopOptions;
class SignboardOptions;
class SkillViewOptions;
class SkinLookOptions;
class SkinOptions;
class SpriteSoundOptions;
class StatusPortraitOptions;
class TalkFilterOptions;
class TargetOptions;
struct ViewEquipMemory;
class VoiceOptions;
class WeaponRefineOptions;
class WhisperOptions;
class WindowTitleOptions;

// CE QUE REND « RÉTABLIR CETTE PAGE ». ⚠ LES RÉGLAGES DU LOGIN N'EN MONTRENT
// QU'UNE PARTIE, et un rétablissement n'y touche qu'à ce qu'on voit.
enum class PageRestore : std::uint8_t {
  kWhole,
  // Sans les sons des monstres et des pets : voir `kSpriteSoundsSettingsPage`.
  kShownAtLogin,
};

// LES PAGES DE RÉGLAGES QUI TIENNENT LEUR PROPRE CLASSE — leurs réglages, leurs
// lignes, leurs listes.
//
// ⚠ UN SEUL MEMBRE DE `GameOptions`, et c'est la raison de ce fichier : la
// classe touche ses huit cents lignes. Une page de plus s'ajoute ici, sans elle.
struct OptionPages {
  OptionPages();
  ~OptionPages();
  OptionPages(const OptionPages&) = delete;
  OptionPages& operator=(const OptionPages&) = delete;

  Held<AfkOptions> afk;
  Held<TargetOptions> targeting;
  // Les onglets « Portrait » et « Étiquettes » de la page « Basic Info »,
  // nés dans la disposition d'usine.
  Held<StatusPortraitOptions> portrait;
  Held<ContextMenuOptions> context_menu;
  Held<ViewEquipMemory> view_equip;
  Held<CompanionZonesMemory> companion_zones;
  Held<GraphicsOptions> graphics;
  Held<MonsterSheetOptions> monster_sheet;
  Held<CraftAtlasOptions> craft_atlas;
  Held<MakeItemOptions> make_item;
  Held<WeaponRefineOptions> weapon_refine;
  Held<QuickCastOptions> quick_cast;
  Held<CardAlbumOptions> card_album;
  Held<DescriptionOptions> descriptions;
  Held<BugReportOptions> bug_report;
  Held<MenuIconsOptions> menu_icons;
  Held<MapSignOptions> map_sign;
  Held<WindowTitleOptions> window_title;
  // Les deux bandeaux de bulles du haut de l'écran : annonces et chasses.
  Held<BroadcastBalloonOptions> balloons;
  Held<MinimapPanelOptions> minimap_panel;
  Held<SkinOptions> skin;
  Held<SkinLookOptions> skin_look;
  Held<PartyOptions> party;
  Held<CastBarOptions> cast_bars;
  Held<PickPrecisionOptions> pick_precision;
  Held<DamageMeterOptions> damage_meter;
  Held<ShopOptions> shops;
  // L'onglet « Enseignes » de la page « Entités ».
  Held<SignboardOptions> signboards;
  Held<WhisperOptions> whispers;
  Held<GreyWorldOptions> grey_world;
  Held<GrimoireKeyOptions> grimoire_keys;
  Held<SkillViewOptions> skill_views;
  Held<SpriteSoundOptions> sprite_sounds;
  Held<MvpTrackerOptions> mvp_tracker;
  Held<BattlegroundOptions> battleground;
  Held<PetOptions> pet;
  // La voix en jeu, dans la page Son.
  Held<VoiceOptions> voice;
  Held<QuestTrackerOptions> quest_tracker;
  // ⚠ SANS DÉCLARATION AU FICHIER : c'est le serveur qui tient ce réglage.
  Held<CallOptions> call;
  // `/notrade` : les demandes d'échange refusées d'office.
  Held<DealRefusalOptions> deal_refusal;
  // Les liens, les images et les mentions du chat.
  Held<ChatWebOptions> chat_web;
  // `/notalkmsg` et `/notalkmsg2`.
  Held<TalkFilterOptions> talk_filter;
  // L'ENTITÉ DU CADRE D'ESSAI des barres d'entité, composée par la partie avant
  // la page — voir `world/entity_sample.h`. ⚠ PAS UN RÉGLAGE : une vignette.
  ro::ui::EntitySample entity_sample;
  // Le cadre qui le montre, épinglé en haut de la page « Entités ». ⚠ ICI ET
  // NON SUR LA PILE : la section n'en garde qu'une vue.
  std::vector<ro::ui::OptionRowView> entity_pinned_rows;
  // L'aperçu épinglé en haut de la page des bulles de chat. ⚠ ICI ET NON SUR LA
  // PILE, pour la même raison.
  std::vector<ro::ui::OptionRowView> bubble_pinned_rows;
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
  // CE QUE CES PROFILS DONNENT À LA RECHERCHE : leurs titres, traduits. Posé
  // par la partie avec le corps. ⚠ PAS UN RÉGLAGE.
  std::function<void(std::vector<ro::ui::OptionBodyTerm>&)> profiles_terms;

  // ── CE QUE LE FILTRE DU PANNEAU LIT, refait à chaque image où l'on cherche ──
  //
  // Les mots des pages dessinées par elles-mêmes, et les rangées des onglets
  // fermés recopiées : voir `ro/ui/options_search.h`. ⚠ ICI ET NON SUR LA PILE :
  // les sections n'en gardent qu'une vue. Vides quand rien n'est cherché.
  std::vector<ro::ui::OptionBodyTerm> interface_terms;
  std::vector<ro::ui::OptionBodyTerm> skin_terms;
  std::vector<ro::ui::OptionBodyTerm> commands_terms;
  ro::ui::SearchedTabs shortcut_search;
  ro::ui::SearchedTabs stat_bar_search;
  ro::ui::SearchedTabs entity_search;
  ro::ui::SearchedTabs damage_search;

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

  // CE QUE LA SECTION CONVERSATION PORTE APRÈS SES RÉGLAGES À ELLE : la parole
  // tue, les conversations 1:1, le relai Discord, puis les liens et les images.
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
