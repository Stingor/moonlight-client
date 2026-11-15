#pragma once

// CE QUE LES FENÊTRES DU JEU GARDENT D'UNE IMAGE À L'AUTRE : `UiStage::Windows`.
//
// ⚠ À PART DE `game/ui_stage.h`, et c'est ce qui borne une reconstruction. Cette
// structure porte PAR VALEUR l'état de chaque fenêtre, donc elle tire l'en-tête
// de chacune : la nommer suffit à `ui_stage.h`, mais une source qui la LIT les
// relit toutes. Une source qui ne fait que la passer inclut
// `game/ui_stage_fwd.h`, et une retouche d'une fenêtre ne la recompile pas.

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/bank_window.h"
#include "ro/ui/book_window.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/camera_hud.h"
#include "ro/ui/card_album_window.h"
#include "ro/ui/card_insert_window.h"
#include "ro/ui/cart_choice_window.h"
#include "ro/ui/cart_window.h"
#include "ro/ui/character_window.h"
#include "ro/ui/chat_macros.h"
#include "ro/ui/chat_window.h"
#include "ro/ui/companion_ai_window.h"
#include "ro/ui/companion_tactic_window.h"
#include "ro/ui/craft_atlas_window.h"
#include "ro/ui/damage_meter_window.h"
#include "ro/ui/description_window.h"
#include "ro/ui/entity_menu.h"
#include "ro/ui/frame_meter.h"
#include "ro/ui/game_menu.h"
#include "ro/ui/hotkey_window.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/link_notice.h"
#include "ro/ui/log_window.h"
#include "ro/ui/make_item_window.h"
#include "ro/ui/map_sign_banner.h"
#include "ro/ui/menu_icons.h"
#include "ro/ui/minimap_settings.h"
#include "ro/ui/minimap_window.h"
#include "ro/ui/monster_sheet_window.h"
#include "ro/ui/npc_dialog_window.h"
#include "ro/ui/options_window.h"
#include "ro/ui/packet_window.h"
#include "ro/ui/palette_editor.h"
#include "ro/ui/pet_hatch_window.h"
#include "ro/ui/pet_window.h"
#include "ro/ui/pick_boxes.h"
#include "ro/ui/player_report_window.h"
#include "ro/ui/repair_window.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/sound_zones.h"
#include "ro/ui/spell_list_window.h"
#include "ro/ui/staff_tools.h"
#include "ro/ui/storage_window.h"
#include "ro/ui/style_preview.h"
#include "ro/ui/talkbox_window.h"
#include "ro/ui/view_equip_window.h"
#include "ro/ui/voice_overlay.h"
#include "ro/ui/warp_list_window.h"
#include "ro/ui/weapon_refine_window.h"

#include "game/achievement_stage.h"
#include "game/battleground_stage.h"
#include "game/cash_shop_windows.h"
#include "game/chat_room_stage.h"
#include "game/database_windows.h"
#include "game/hud_rounds.h"
#include "game/mail_stage.h"
#include "game/memorial_stage.h"
#include "game/minigame_stage.h"
#include "game/navigation_windows.h"
#include "game/party_windows.h"
#include "game/quest_stage.h"
#include "game/staff_window_rounds.h"
#include "game/trade_windows.h"
#include "game/whisper_stage.h"
#include "mvp/mvp_tracker_stage.h"
#include "party/party_booking_board.h"
#include "world/broadcast_balloons.h"
#include "world/music_player_stage.h"
#include "world/ranking_board.h"
#include "world/reputation_board.h"

namespace moonclient {

// Ce que les fenêtres gardent d'une image à l'autre. Groupé plutôt qu'énuméré
// en paramètres : chaque fenêtre nouvelle en ajouterait un, et l'ordre de
// quatre booléens voisins devient vite un piège.
struct UiWindows {
  std::optional<ro::ui::MapSignView> map_sign;  // le bandeau de la carte, composé par `app`
  ro::ui::ShortcutWheelView shortcut_wheel;     // la roue d'utilisation, composée par `app`
  // La liaison au serveur : bandeau et boîte de perte, composés par `app`.
  ro::ui::LinkNoticeState link_notice;
  ro::ui::VoiceOverlayState voice;  // le témoin de la voix, composé par `app`
  BalloonBoards balloons;           // les bulles d'annonce, composées à chaque image
  ro::ui::PacketWindowState packets;
  ro::ui::LogWindowState log;
  ro::ui::ChatWindows chat;
  // LA FICHE DE PERSONNAGE, grimoire compris : c'est UNE fenetre et un
  // seul interrupteur. Le grimoire y garde son propre souvenir d'onglet
  // et de filtre, mais il ne s'ouvre plus tout seul.
  ro::ui::CharacterWindowState character;
  ro::ui::CharacterWindowView character_view;
  // L'ÉDITEUR DE STYLE. ⚠ SON OUVERTURE N'APPARTIENT PAS QU'AU JOUEUR : le
  // styliste du serveur peut la demander — voir `ZC_MOONLIGHT_STYLE_OPEN`.
  ro::ui::PaletteEditorState style;
  // L'APERÇU D'UN STYLE REÇU. ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR
  // NON PLUS : elle vient d'un clic sur un lien de la conversation.
  ro::ui::StylePreviewState style_preview;
  ro::ui::InventoryWindowState inventory;
  // Ce que le sac sait du monde et de ses sœurs, recomposé à chaque image.
  ro::ui::InventoryWindowView inventory_view;
  // LE DIALOGUE D'UN PNJ. ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : elle
  // suit le monde, qui la tient du serveur. Cet état ne porte que ce qu'une
  // conversation garde d'une image à l'autre — la mise en page en cache, le
  // filtre du menu, ce qu'on est en train de taper.
  ro::ui::NpcDialogWindowState npc_dialog;
  // LA CHARRETTE, sœur du sac : même grille, six onglets, pied sans bourse.
  ro::ui::CartWindowState cart;
  ro::ui::CartWindowView cart_view;
  // LE COFFRE, la troisième sœur — mais une TABLE À COLONNES, et une session
  // du serveur : son interrupteur n'appartient pas au joueur.
  ro::ui::StorageWindowState storage;
  ro::ui::StorageWindowView storage_view;
  // LA BANQUE DE ZENY. ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : elle
  // recopie celle du monde, que le serveur bascule.
  ro::ui::BankWindowState bank;
  // L'ALBUM DE CARTES. ⚠ SA VUE EST COMPOSÉE PAR `app`, fenêtre ouverte, et
  // elle descend par ce pointeur : nul, la fenêtre ne se dessine pas.
  ro::ui::CardAlbumWindowState card_album;
  // LE RAPPORT DE BUG : sa boîte, son contexte, son accusé. Voir l'ADR 0052.
  ro::ui::BugReportWindowState bug_report;
  // « SIGNALER CE JOUEUR » : voir `world/player_report.h`.
  ro::ui::PlayerReportWindowState player_report;
  // LE LIVRE QU'ON LIT, et ce que `app` en montre : ses lignes, et la ligne
  // que le personnage récite.
  ro::ui::BookWindowState book;
  const ro::ui::BookView* book_view = nullptr;
  std::string_view book_recital;
  const ro::ui::CardAlbumView* card_album_view = nullptr;
  // LE SERTISSAGE D'UNE CARTE. ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR :
  // c'est la liste du serveur qui l'ouvre. Sa vue descend avec lui, recomposée
  // à chaque image par `app` — comme la rangée des coffres.
  ro::ui::CardInsertWindowState card_insert;
  ro::ui::CardInsertView card_insert_view;
  // LES DEUX ATELIERS — fabrication et refine d'arme. ⚠ LEUR OUVERTURE
  // N'APPARTIENT PAS AU JOUEUR : c'est la liste du serveur qui les ouvre. Leurs
  // vues vivent chez `CraftStage`, et descendent par pointeur.
  ro::ui::MakeItemWindowState make_item;
  const ro::ui::MakeItemView* make_item_view = nullptr;
  ro::ui::WeaponRefineWindowState weapon_refine;
  const ro::ui::WeaponRefineView* weapon_refine_view = nullptr;
  // LES DESTINATIONS D'UNE COMPÉTENCE DE DÉPLACEMENT. ⚠ SON OUVERTURE
  // N'APPARTIENT PAS AU JOUEUR : elle recopie la question du monde. Les
  // lignes vivent ici, et la vue les désigne.
  ro::ui::WarpListWindowState warp_list;
  ro::ui::WarpListView warp_list_view;
  std::vector<std::string> warp_list_rows;
  // LA LISTE DES CHARRETTES. ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : la
  // compétence, ou la liste du serveur, l'ouvre. Sa vue descend avec elle.
  ro::ui::CartChoiceWindowState cart_choice;
  ro::ui::CartChoiceView cart_choice_view;
  // LA LISTE DES SORTS D'AUTOSPELL, ouverte par le serveur.
  ro::ui::SpellListWindowState spell_list;
  ro::ui::SpellListView spell_list_view;
  // LA LISTE DE REPAIR WEAPON, ouverte par le serveur.
  ro::ui::RepairWindowState repair;
  ro::ui::RepairView repair_view;
  // LA FICHE DU FAMILIER, et la liste d'éclosion que le serveur ouvre.
  ro::ui::PetWindowState pet;
  ro::ui::PetWindowView pet_view;
  ro::ui::PetHatchWindowState pet_hatch;
  ro::ui::PetHatchWindowView pet_hatch_view;
  // LA SAISIE DE TALKIE BOX. ⚠ SON OUVERTURE N'APPARTIENT PAS AU JOUEUR : la
  // pose de la compétence l'ouvre.
  ro::ui::TalkboxWindowState talkbox;
  ro::ui::TalkboxView talkbox_view;
  // LES RÉGLAGES AVANCÉS D'UNE TACTIQUE DE COMPAGNON, ouverts du menu d'entité.
  ro::ui::CompanionTacticWindowState companion_tactic;
  ro::ui::CompanionTacticView companion_tactic_view;
  // LA FENÊTRE « IA DU COMPAGNON », ouverte depuis l'onglet d'un compagnon.
  ro::ui::CompanionAiWindowState companion_ai;
  ro::ui::CompanionAiView companion_ai_view;
  // LA DESCRIPTION D'UN OBJET. ⚠ SA CLÉ EST UN IDENTIFIANT, pas une place :
  // elle survit à la consommation de l'objet qui l'a ouverte.
  ro::ui::DescriptionWindowState description;
  // LA DESCRIPTION D'UNE COMPÉTENCE. ⚠ UN ÉTAT À PART, et deux fenêtres qui
  // s'ouvrent ensemble : on compare une compétence à l'objet qui la donne.
  ro::ui::SkillDescriptionWindowState skill_description;
  ro::ui::GameMenuState menu;
  ro::ui::OptionsWindowState options;
  ro::ui::HotkeyWindowState hotkeys;
  // LE COMPTEUR D'IMAGES. ⚠ IL N'EST PAS DANS LA PILE D'ÉCHAP : c'est un
  // instrument posé sur le jeu, pas une fenêtre qu'on ouvre et qu'on referme.
  ro::ui::FrameMeterState meter;
  // LE COMPTEUR DE DÉGÂTS, hors de la pile d'Échap pour la même raison. Sa
  // vue descend avec lui, composée par `DamageMeterStage`.
  ro::ui::DamageMeterState damage_meter;
  ro::ui::DamageMeterView damage_meter_view;
  // L'ÉTABLI DU STAFF. ⚠ IL SORT DE LA PILE D'ÉCHAP : c'est une fenêtre qu'on
  // garde ouverte en travaillant, et chaque Échap destiné au menu du jeu la
  // refermerait par surprise.
  ro::ui::StaffToolsState staff_tools;
  ro::ui::CameraHudView camera_hud;
  std::vector<ro::ui::SoundZoneView> sound_zones;
  // LES ZONES CLIQUABLES de l'image, récoltées par le dessin du monde.
  ro::ui::PickBoxesView pick_boxes;
  // LES DIX MACROS DE CHAT, et leur fenêtre. ⚠ LES TEXTES VIVENT ICI PENDANT
  // LA PARTIE : le fichier les descend au démarrage, et les reprend quand
  // elles changent — voir `GameSettings::NoteChatMacros`.
  ro::ui::ChatMacroState macros;
  // LES DIX LIBELLÉS DE TOUCHE DES MACROS, composés par `app`.
  //
  // ⚠ `ui` NE SAIT PAS NOMMER UN CODE DE TOUCHE : seule la table des
  // raccourcis dit ce qu'ils valent, même frontière que pour la fenêtre de
  // configuration. Remplis seulement quand la fenêtre est ouverte.
  std::array<std::string, ro::ui::kChatMacroCount> macro_keys;
  // LA CIBLE, LE PORTRAIT, LE STATUT ET SES PRÉRÉGLAGES : `game/hud_rounds.h`.
  TargetHudRound target_hud;
  StatusPortraitRound status_portrait;
  StatusWindowRound status;
  StatPresetRound stat_presets;
  BasicInfoRound basic_info;
  // LE MENU DU CLIC DROIT SUR UNE ENTITÉ, même aller-retour : sa vue descend
  // d'`app` à l'ouverture, sa réponse remonte à l'image suivante.
  struct EntityMenu {
    ro::ui::EntityMenuState state;
    const ro::ui::EntityMenuView* view = nullptr;
    ro::ui::EntityMenuAnswer answer;
  };
  EntityMenu entity_menu;
  // « VOIR L'ÉQUIPEMENT » D'UN AUTRE JOUEUR, même aller-retour. Vue nulle,
  // rien ne se dessine.
  struct ViewEquip {
    ro::ui::ViewEquipWindowState state;
    const ro::ui::ViewEquipView* view = nullptr;
    ro::ui::ViewEquipAnswer answer;
  };
  ViewEquip view_equip;
  // LA FICHE D'UN MONSTRE, même aller-retour ; vue nulle, rien ne se dessine.
  // Et LA BASE DE DONNÉES, qui l'héberge quand elle est ouverte (ADR 0116).
  struct MonsterSheet {
    ro::ui::MonsterSheetWindowState state;
    const ro::ui::MonsterSheetView* view = nullptr;
    ro::ui::MonsterSheetAnswer answer;
    const ro::ui::MonsterSheetPreview* preview = nullptr;  // un lien survolé, ou nul
    ro::ui::MonsterSheetPreviewState preview_state;
    DatabaseWindows database;
  };
  MonsterSheet monster_sheet;
  // LES FICHES DU STAFF, même aller-retour : voir `game/staff_window_rounds.h`.
  EntityInspectorRound entity_inspector;
  ChatBlockLogRound chat_block_log;
  // LA GRILLE D'ICÔNES DE MENU, même aller-retour : ses icônes descendent
  // d'`app` à chaque image, la commande cliquée remonte à la suivante.
  struct MenuIcons {
    ro::ui::MenuIconsState state;
    std::vector<ro::ui::MenuIconView> icons;
    std::optional<ro::ui::MenuCommand> clicked;
    // La place de chaque icône, tenue par les réglages. Nulle, rien ne se dessine.
    ro::ui::MenuIconsLook* look = nullptr;
  };
  MenuIcons menu_icons;
  // L'ATLAS DES RECETTES, même aller-retour. Vue nulle, rien ne se dessine.
  struct CraftAtlas {
    ro::ui::CraftAtlasWindowState state;
    const ro::ui::CraftAtlasView* view = nullptr;
    ro::ui::CraftAtlasAnswer answer;
    // L'aperçu d'un lien de recette survolé : nul quand rien ne l'est.
    const ro::ui::CraftRecipePreview* preview = nullptr;
  };
  CraftAtlas craft_atlas;
  // LA MINIMAP, même aller-retour. Vue nulle, rien ne se dessine : hors du
  // monde, ou masquée par le joueur. Ses réglages sont à la session de
  // navigation, qui les enregistre.
  struct Minimap {
    ro::ui::MinimapSettings* settings = nullptr;
    ro::ui::MinimapWindowState state;
    const ro::ui::MinimapView* view = nullptr;
    ro::ui::MinimapAnswer answer;
  };
  Minimap minimap;
  // LA NAVIGATION ET LA CARTE DU MONDE : voir `NavigationWindows`.
  using Navigation = NavigationWindows;
  Navigation navigation;
  CashShopWindows cash_shop;  // La boutique de vote : `game/cash_shop_windows.h`.
  TradeWindows trade;         // Les commerces : `game/trade_windows.h`, `ui_stage_trade.cpp`.
  // LE COURRIER : la boîte et l'écriture. Voir `ui_stage_mail.cpp`.
  MailWindows mail;
  // LES SALONS : le formulaire, la salle, l'invite. Voir `ui_stage_chat_room.cpp`.
  ChatRoomWindows chat_room;
  WhisperWindows whispers;            // Les conversations 1:1 : `ui_stage_whisper.cpp`.
  AchievementWindows achievements;    // La fenêtre, la bannière : `ui_stage_achievements.cpp`.
  QuestWindows quests;                // Le journal : `ui_stage_quests.cpp`.
  TrackerWindows tracker;             // Le suivi commun aux succès, de même.
  MvpTrackerWindows mvp_tracker;      // Le carnet MVP : `ui_stage_mvp_tracker.cpp`.
  MemorialWindows memorial;           // L'instance : `game/memorial_stage.h`.
  BattlegroundWindows battleground;   // Voir `ui_stage_battleground.cpp`.
  RankingBoardWindows ranking_board;  // Voir `ui_stage_ranking.cpp`.
  ReputationWindows reputation;       // Voir `world/reputation_board.h`.
  PartyBookingWindows party_booking;  // Voir `ui_stage_party_booking.cpp`.
  PartyWindows party;                 // Groupe, amis et grille : `game/party_windows.h`.
  MusicPlayerWindows music_player;    // Le lecteur : `world/music_player_stage.h`.
  MinigameWindows minigames;          // Les mini-jeux : `game/minigame_stage.h`.
  // Les éléments déverrouillés à l'écran, déjà traduits : l'avis de pose
  // (`game/hud_lock.h`) paraît tant qu'il y en a un, et les nomme.
  std::vector<const char*> unlocked_hud;
};

}  // namespace moonclient
