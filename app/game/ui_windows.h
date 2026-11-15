#pragma once

// CE QUE LES FENÊTRES DU JEU GARDENT D'UNE IMAGE À L'AUTRE : `UiStage::Windows`.
//
// ⚠ CET EN-TÊTE NE TIRE AUCUNE FENÊTRE, et il ne doit pas se remettre à le faire :
// presque toute la partie le lit. Chaque fenêtre y est tenue par `Held`, donc
// seulement déclarée ; une source qui s'en sert inclut l'en-tête de la fenêtre,
// et une retouche de celui-ci ne recompile qu'elle. Les aller-retours imbriqués
// (`EntityMenu`, `MonsterSheet`…) se définissent dans `game/ui_window_rounds.h`.
//
// ⚠ UN MEMBRE NEUF D'UN TYPE DE FENÊTRE SE TIENT PAR `Held` : par valeur, il
// ferait relire son en-tête à toute la partie.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game/held.h"

namespace ro::ui {
struct BankWindowState;
struct BookView;
struct BookWindowState;
struct BugReportWindowState;
struct CameraHudView;
struct CardAlbumView;
struct CardAlbumWindowState;
struct CardInsertView;
struct CardInsertWindowState;
struct CartChoiceView;
struct CartChoiceWindowState;
struct CartWindowState;
struct CartWindowView;
struct CharacterWindowState;
struct CharacterWindowView;
struct ChatMacroState;
struct ChatWindows;
struct CompanionAiView;
struct CompanionAiWindowState;
struct CompanionTacticView;
struct CompanionTacticWindowState;
struct DamageMeterState;
struct DamageMeterView;
struct DescriptionWindowState;
struct FrameMeterState;
struct GameMenuState;
struct HotkeyWindowState;
struct InventoryWindowState;
struct InventoryWindowView;
struct LinkNoticeState;
struct LogWindowState;
struct MakeItemView;
struct MakeItemWindowState;
struct MapSignView;
struct NpcDialogWindowState;
struct OptionsWindowState;
struct PacketWindowState;
struct PaletteEditorState;
struct PetHatchWindowState;
struct PetHatchWindowView;
struct PetWindowState;
struct PetWindowView;
struct PickBoxesView;
struct PlayerReportWindowState;
struct RepairView;
struct RepairWindowState;
struct ShortcutWheelView;
struct SkillDescriptionWindowState;
struct SoundZoneView;
struct SpellListView;
struct SpellListWindowState;
struct StaffToolsState;
struct StorageWindowState;
struct StorageWindowView;
struct StylePreviewState;
struct TalkboxView;
struct TalkboxWindowState;
struct VoiceOverlayState;
struct WarpListView;
struct WarpListWindowState;
struct WeaponRefineView;
struct WeaponRefineWindowState;
}  // namespace ro::ui

namespace moonclient {

struct AchievementWindows;
struct BalloonBoards;
struct BasicInfoRound;
struct BattlegroundWindows;
struct CashShopWindows;
struct ChatBlockLogRound;
struct ChatRoomWindows;
struct EntityInspectorRound;
struct MailWindows;
struct MemorialWindows;
struct MinigameWindows;
struct MusicPlayerWindows;
struct MvpTrackerWindows;
struct NavigationWindows;
struct PartyBookingWindows;
struct PartyWindows;
struct QuestWindows;
struct RankingBoardWindows;
struct ReputationWindows;
struct StatPresetRound;
struct StatusPortraitRound;
struct StatusWindowRound;
struct TargetHudRound;
struct TrackerWindows;
struct TradeWindows;
struct WhisperWindows;

// Ce que les fenêtres gardent d'une image à l'autre. Groupé plutôt qu'énuméré
// en paramètres : chaque fenêtre nouvelle en ajouterait un, et l'ordre de
// quatre booléens voisins devient vite un piège.
struct UiWindows {
  UiWindows();
  ~UiWindows();
  UiWindows(const UiWindows&) = delete;
  UiWindows& operator=(const UiWindows&) = delete;

  Held<std::optional<ro::ui::MapSignView>> map_sign;  // le bandeau de la carte, composé par `app`
  Held<ro::ui::ShortcutWheelView> shortcut_wheel;     // la roue d'utilisation, composée par `app`
  // La liaison au serveur : bandeau et boîte de perte, composés par `app`.
  Held<ro::ui::LinkNoticeState> link_notice;
  Held<ro::ui::VoiceOverlayState> voice;  // le témoin de la voix, composé par `app`
  Held<BalloonBoards> balloons;           // les bulles d'annonce, composées à chaque image
  Held<ro::ui::PacketWindowState> packets;
  Held<ro::ui::LogWindowState> log;
  Held<ro::ui::ChatWindows> chat;
  // LA FICHE DE PERSONNAGE, grimoire compris : c'est UNE fenetre et un
  // seul interrupteur. Le grimoire y garde son propre souvenir d'onglet
  // et de filtre, mais il ne s'ouvre plus tout seul.
  Held<ro::ui::CharacterWindowState> character;
  Held<ro::ui::CharacterWindowView> character_view;
  // L'ÉDITEUR DE STYLE. ⚠ SON OUVERTURE N'APPARTIENT PAS QU'AU JOUEUR : le
  // styliste du serveur peut la demander — voir `ZC_MOONLIGHT_STYLE_OPEN`.
  Held<ro::ui::PaletteEditorState> style;
  // L'APERÇU D'UN STYLE REÇU. ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR
  // NON PLUS : elle vient d'un clic sur un lien de la conversation.
  Held<ro::ui::StylePreviewState> style_preview;
  Held<ro::ui::InventoryWindowState> inventory;
  // Ce que le sac sait du monde et de ses sœurs, recomposé à chaque image.
  Held<ro::ui::InventoryWindowView> inventory_view;
  // LE DIALOGUE D'UN PNJ. ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : elle
  // suit le monde, qui la tient du serveur. Cet état ne porte que ce qu'une
  // conversation garde d'une image à l'autre — la mise en page en cache, le
  // filtre du menu, ce qu'on est en train de taper.
  Held<ro::ui::NpcDialogWindowState> npc_dialog;
  // LA CHARRETTE, sœur du sac : même grille, six onglets, pied sans bourse.
  Held<ro::ui::CartWindowState> cart;
  Held<ro::ui::CartWindowView> cart_view;
  // LE COFFRE, la troisième sœur — mais une TABLE À COLONNES, et une session
  // du serveur : son interrupteur n'appartient pas au joueur.
  Held<ro::ui::StorageWindowState> storage;
  Held<ro::ui::StorageWindowView> storage_view;
  // LA BANQUE DE ZENY. ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : elle
  // recopie celle du monde, que le serveur bascule.
  Held<ro::ui::BankWindowState> bank;
  // L'ALBUM DE CARTES. ⚠ SA VUE EST COMPOSÉE PAR `app`, fenêtre ouverte, et
  // elle descend par ce pointeur : nul, la fenêtre ne se dessine pas.
  Held<ro::ui::CardAlbumWindowState> card_album;
  // LE RAPPORT DE BUG : sa boîte, son contexte, son accusé. Voir l'ADR 0052.
  Held<ro::ui::BugReportWindowState> bug_report;
  // « SIGNALER CE JOUEUR » : voir `world/player_report.h`.
  Held<ro::ui::PlayerReportWindowState> player_report;
  // LE LIVRE QU'ON LIT, et ce que `app` en montre : ses lignes, et la ligne
  // que le personnage récite.
  Held<ro::ui::BookWindowState> book;
  const ro::ui::BookView* book_view = nullptr;
  std::string_view book_recital;
  const ro::ui::CardAlbumView* card_album_view = nullptr;
  // LE SERTISSAGE D'UNE CARTE. ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR :
  // c'est la liste du serveur qui l'ouvre. Sa vue descend avec lui, recomposée
  // à chaque image par `app` — comme la rangée des coffres.
  Held<ro::ui::CardInsertWindowState> card_insert;
  Held<ro::ui::CardInsertView> card_insert_view;
  // LES DEUX ATELIERS — fabrication et refine d'arme. ⚠ LEUR OUVERTURE
  // N'APPARTIENT PAS AU JOUEUR : c'est la liste du serveur qui les ouvre. Leurs
  // vues vivent chez `CraftStage`, et descendent par pointeur.
  Held<ro::ui::MakeItemWindowState> make_item;
  const ro::ui::MakeItemView* make_item_view = nullptr;
  Held<ro::ui::WeaponRefineWindowState> weapon_refine;
  const ro::ui::WeaponRefineView* weapon_refine_view = nullptr;
  // LES DESTINATIONS D'UNE COMPÉTENCE DE DÉPLACEMENT. ⚠ SON OUVERTURE
  // N'APPARTIENT PAS AU JOUEUR : elle recopie la question du monde. Les
  // lignes vivent ici, et la vue les désigne.
  Held<ro::ui::WarpListWindowState> warp_list;
  Held<ro::ui::WarpListView> warp_list_view;
  std::vector<std::string> warp_list_rows;
  // LA LISTE DES CHARRETTES. ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : la
  // compétence, ou la liste du serveur, l'ouvre. Sa vue descend avec elle.
  Held<ro::ui::CartChoiceWindowState> cart_choice;
  Held<ro::ui::CartChoiceView> cart_choice_view;
  // LA LISTE DES SORTS D'AUTOSPELL, ouverte par le serveur.
  Held<ro::ui::SpellListWindowState> spell_list;
  Held<ro::ui::SpellListView> spell_list_view;
  // LA LISTE DE REPAIR WEAPON, ouverte par le serveur.
  Held<ro::ui::RepairWindowState> repair;
  Held<ro::ui::RepairView> repair_view;
  // LA FICHE DU FAMILIER, et la liste d'éclosion que le serveur ouvre.
  Held<ro::ui::PetWindowState> pet;
  Held<ro::ui::PetWindowView> pet_view;
  Held<ro::ui::PetHatchWindowState> pet_hatch;
  Held<ro::ui::PetHatchWindowView> pet_hatch_view;
  // LA SAISIE DE TALKIE BOX. ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : la
  // pose de la compétence l'ouvre.
  Held<ro::ui::TalkboxWindowState> talkbox;
  Held<ro::ui::TalkboxView> talkbox_view;
  // LES RÉGLAGES AVANCÉS D'UNE TACTIQUE DE COMPAGNON, ouverts du menu d'entité.
  Held<ro::ui::CompanionTacticWindowState> companion_tactic;
  Held<ro::ui::CompanionTacticView> companion_tactic_view;
  // LA FENÊTRE « IA DU COMPAGNON », ouverte depuis l'onglet d'un compagnon.
  Held<ro::ui::CompanionAiWindowState> companion_ai;
  Held<ro::ui::CompanionAiView> companion_ai_view;
  // LA DESCRIPTION D'UN OBJET. ⚠ SA CLÉ EST UN IDENTIFIANT, pas une place :
  // elle survit à la consommation de l'objet qui l'a ouverte.
  Held<ro::ui::DescriptionWindowState> description;
  // LA DESCRIPTION D'UNE COMPÉTENCE. ⚠ UN ÉTAT À PART, et deux fenêtres qui
  // s'ouvrent ensemble : on compare une compétence à l'objet qui la donne.
  Held<ro::ui::SkillDescriptionWindowState> skill_description;
  Held<ro::ui::GameMenuState> menu;
  Held<ro::ui::OptionsWindowState> options;
  Held<ro::ui::HotkeyWindowState> hotkeys;
  // LE COMPTEUR D'IMAGES. ⚠ IL N'EST PAS DANS LA PILE D'ÉCHAP : c'est un
  // instrument posé sur le jeu, pas une fenêtre qu'on ouvre et qu'on referme.
  Held<ro::ui::FrameMeterState> meter;
  // LE COMPTEUR DE DÉGÂTS, hors de la pile d'Échap pour la même raison. Sa
  // vue descend avec lui, composée par `DamageMeterStage`.
  Held<ro::ui::DamageMeterState> damage_meter;
  Held<ro::ui::DamageMeterView> damage_meter_view;
  // L'ÉTABLI DU STAFF. ⚠ IL SORT DE LA PILE D'ÉCHAP : c'est une fenêtre qu'on
  // garde ouverte en travaillant, et chaque Échap destiné au menu du jeu la
  // refermerait par surprise.
  Held<ro::ui::StaffToolsState> staff_tools;
  Held<ro::ui::CameraHudView> camera_hud;
  std::vector<ro::ui::SoundZoneView> sound_zones;
  // LES ZONES CLIQUABLES de l'image, récoltées par le dessin du monde.
  Held<ro::ui::PickBoxesView> pick_boxes;
  // LES DIX MACROS DE CHAT, et leur fenêtre. ⚠ LES TEXTES VIVENT ICI PENDANT
  // LA PARTIE : le fichier les descend au démarrage, et les reprend quand
  // elles changent — voir `GameSettings::NoteChatMacros`.
  Held<ro::ui::ChatMacroState> macros;
  // LES DIX LIBELLÉS DE TOUCHE DES MACROS, composés par `app`.
  //
  // ⚠ `ui` NE SAIT PAS NOMMER UN CODE DE TOUCHE : seule la table des
  // raccourcis dit ce qu'ils valent, même frontière que pour la fenêtre de
  // configuration. Remplis seulement quand la fenêtre est ouverte.
  struct MacroKeyLabels;
  Held<MacroKeyLabels> macro_keys;
  // LA CIBLE, LE PORTRAIT, LE STATUT ET SES PRÉRÉGLAGES : `game/hud_rounds.h`.
  Held<TargetHudRound> target_hud;
  Held<StatusPortraitRound> status_portrait;
  Held<StatusWindowRound> status;
  Held<StatPresetRound> stat_presets;
  Held<BasicInfoRound> basic_info;
  // LE MENU DU CLIC DROIT SUR UNE ENTITÉ, même aller-retour : sa vue descend
  // d'`app` à l'ouverture, sa réponse remonte à l'image suivante.
  struct EntityMenu;
  Held<EntityMenu> entity_menu;
  // « VOIR L'ÉQUIPEMENT » D'UN AUTRE JOUEUR, même aller-retour. Vue nulle,
  // rien ne se dessine.
  struct ViewEquip;
  Held<ViewEquip> view_equip;
  // LA FICHE D'UN MONSTRE, même aller-retour ; vue nulle, rien ne se dessine.
  // Et LA BASE DE DONNÉES, qui l'héberge quand elle est ouverte (ADR 0116).
  struct MonsterSheet;
  Held<MonsterSheet> monster_sheet;
  // LES FICHES DU STAFF, même aller-retour : voir `game/staff_window_rounds.h`.
  Held<EntityInspectorRound> entity_inspector;
  Held<ChatBlockLogRound> chat_block_log;
  // LA GRILLE D'ICÔNES DE MENU, même aller-retour : ses icônes descendent
  // d'`app` à chaque image, la commande cliquée remonte à la suivante.
  struct MenuIcons;
  Held<MenuIcons> menu_icons;
  // L'ATLAS DES RECETTES, même aller-retour. Vue nulle, rien ne se dessine.
  struct CraftAtlas;
  Held<CraftAtlas> craft_atlas;
  // LA MINIMAP, même aller-retour. Vue nulle, rien ne se dessine : hors du
  // monde, ou masquée par le joueur. Ses réglages sont à la session de
  // navigation, qui les enregistre.
  struct Minimap;
  Held<Minimap> minimap;
  // LA NAVIGATION ET LA CARTE DU MONDE : voir `NavigationWindows`.
  using Navigation = NavigationWindows;
  Held<Navigation> navigation;
  Held<CashShopWindows> cash_shop;  // La boutique de vote : `game/cash_shop_windows.h`.
  Held<TradeWindows> trade;         // Les commerces : `game/trade_windows.h`, `ui_stage_trade.cpp`.
  // LE COURRIER : la boîte et l'écriture. Voir `ui_stage_mail.cpp`.
  Held<MailWindows> mail;
  // LES SALONS : le formulaire, la salle, l'invite. Voir `ui_stage_chat_room.cpp`.
  Held<ChatRoomWindows> chat_room;
  Held<WhisperWindows> whispers;           // Les conversations 1:1 : `ui_stage_whisper.cpp`.
  Held<AchievementWindows> achievements;   // La fenêtre, la bannière : `ui_stage_achievements.cpp`.
  Held<QuestWindows> quests;               // Le journal : `ui_stage_quests.cpp`.
  Held<TrackerWindows> tracker;            // Le suivi commun aux succès, de même.
  Held<MvpTrackerWindows> mvp_tracker;     // Le carnet MVP : `ui_stage_mvp_tracker.cpp`.
  Held<MemorialWindows> memorial;          // L'instance : `game/memorial_stage.h`.
  Held<BattlegroundWindows> battleground;  // Voir `ui_stage_battleground.cpp`.
  Held<RankingBoardWindows> ranking_board;  // Voir `ui_stage_ranking.cpp`.
  Held<ReputationWindows> reputation;       // Voir `world/reputation_board.h`.
  Held<PartyBookingWindows> party_booking;  // Voir `ui_stage_party_booking.cpp`.
  Held<PartyWindows> party;                 // Groupe, amis et grille : `game/party_windows.h`.
  Held<MusicPlayerWindows> music_player;    // Le lecteur : `world/music_player_stage.h`.
  Held<MinigameWindows> minigames;          // Les mini-jeux : `game/minigame_stage.h`.
  // Les éléments déverrouillés à l'écran, déjà traduits : l'avis de pose
  // (`game/hud_lock.h`) paraît tant qu'il y en a un, et les nomme.
  std::vector<const char*> unlocked_hud;
};

}  // namespace moonclient
