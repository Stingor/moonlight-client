#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/frame_tally.h"
#include "ro/render/skin_textures.h"
#include "ro/sim/bank.h"
#include "ro/sim/npc_dialog.h"
#include "ro/ui/afk_clock.h"
#include "ro/ui/align_grid.h"
#include "ro/ui/bank_window.h"
#include "ro/ui/book_window.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/camera_hud.h"
#include "ro/ui/card_album_window.h"
#include "ro/ui/card_insert_window.h"
#include "ro/ui/cart_choice_window.h"
#include "ro/ui/cart_window.h"
#include "ro/ui/cash_shop_window.h"
#include "ro/ui/cast_bars.h"
#include "ro/ui/character_create_window.h"
#include "ro/ui/character_delete_window.h"
#include "ro/ui/character_select_window.h"
#include "ro/ui/character_window.h"
#include "ro/ui/chat_bubble.h"
#include "ro/ui/chat_macros.h"
#include "ro/ui/chat_window.h"
#include "ro/ui/companion_ai_window.h"
#include "ro/ui/companion_tactic_window.h"
#include "ro/ui/craft_atlas_window.h"
#include "ro/ui/damage_meter_window.h"
#include "ro/ui/damage_texts.h"
#include "ro/ui/description_window.h"
#include "ro/ui/entity_bars.h"
#include "ro/ui/entity_menu.h"
#include "ro/ui/escape_stack.h"
#include "ro/ui/frame_meter.h"
#include "ro/ui/game_menu.h"
#include "ro/ui/hotkey_window.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/item_obtain_toast.h"
#include "ro/ui/link_notice.h"
#include "ro/ui/log_window.h"
#include "ro/ui/login_window.h"
#include "ro/ui/make_item_window.h"
#include "ro/ui/map_sign_banner.h"
#include "ro/ui/menu_icons.h"
#include "ro/ui/minimap_window.h"
#include "ro/ui/navigation_window.h"
#include "ro/ui/npc_dialog_window.h"
#include "ro/ui/npc_illustration_window.h"
#include "ro/ui/options_window.h"
#include "ro/ui/packet_window.h"
#include "ro/ui/palette_editor.h"
#include "ro/ui/patch_screen.h"
#include "ro/ui/pet_hatch_window.h"
#include "ro/ui/pet_window.h"
#include "ro/ui/pick_boxes.h"
#include "ro/ui/player_report_window.h"
#include "ro/ui/pvp_hud_frames.h"
#include "ro/ui/repair_window.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skill_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/sound_zones.h"
#include "ro/ui/spell_list_window.h"
#include "ro/ui/staff_tools.h"
#include "ro/ui/stat_bars.h"
#include "ro/ui/status_bar.h"
#include "ro/ui/status_preview.h"
#include "ro/ui/storage_window.h"
#include "ro/ui/style_preview.h"
#include "ro/ui/talkbox_window.h"
#include "ro/ui/ui_context.h"
#include "ro/ui/view_equip_window.h"
#include "ro/ui/warp_list_window.h"
#include "ro/ui/weapon_refine_window.h"
#include "ro/ui/zone_capture.h"

#include "chat/chat_tab_view.h"
#include "game/achievement_stage.h"
#include "game/battleground_stage.h"
#include "game/chat_room_stage.h"
#include "game/database_windows.h"
#include "game/hud_rounds.h"
#include "game/mail_stage.h"
#include "game/memorial_stage.h"
#include "game/navigation_windows.h"
#include "game/over_world_paint.h"
#include "game/party_windows.h"
#include "game/quest_stage.h"
#include "game/staff_window_rounds.h"
#include "game/trade_windows.h"
#include "game/whisper_stage.h"
#include "mvp/mvp_tracker_stage.h"
#include "party/party_booking_board.h"
#include "ui_backend_gl.h"
#include "world/broadcast_balloons.h"
#include "world/music_player_stage.h"
#include "world/ranking_board.h"

struct SDL_Window;
union SDL_Event;

namespace moonclient {

// L'INTERFACE DU JEU : son contexte, son dos, et la trame qu'ils composent.
//
// ⚠ C'est ici, et nulle part ailleurs, que `ui` rencontre le reste du jeu. La
// couche ne connaît ni `net` ni `sim` — le DAG l'interdit —, donc c'est cet
// étage qui RECOPIE ce qu'une fenêtre montre en données pures. C'est la même
// discipline que la `FrameScene` du renderer, et elle achète la même chose :
// une fenêtre se fabrique et se vérifie dans un test, sans serveur.
//
// ⚠ IL NE POSSÈDE AUCUN ÉTAT DE FENÊTRE. En mode immédiat, une fenêtre se
// redessine entière à chaque image : ce qui doit durer vit chez l'appelant,
// exactement comme l'interrupteur de l'inspecteur de trames. C'est aussi ce qui permet
// à une commande de chat de l'ouvrir sans rien connaître de cet étage.
class UiStage {
 public:
  // LE DIALOGUE À DESSINER : ce qu'il montre, et de quoi peindre ce qu'il porte.
  //
  // ⚠ LES DEUX ENSEMBLE, parce qu'ils ne se lisent pas séparément : une page qui
  // appelle des icônes sans savoir les charger montrerait des trous. `view` nul
  // veut dire qu'aucune conversation n'est ouverte.
  struct NpcTalk {
    const ro::ui::NpcDialogView* view = nullptr;
    ro::ui::NpcDialogArt art;
    // L'ÉTAT QUE LE JOUEUR SURVOLAIT À L'IMAGE PRÉCÉDENTE, composé par `app`.
    //
    // ⚠ IL RETARDE D'UNE IMAGE, et c'est sans conséquence : la fenêtre apprend
    // ce qu'elle survole par la RÉPONSE qu'elle vient de rendre, et une bulle
    // n'apparaît pas avant que la souris se soit arrêtée. Même mécanique que
    // l'aperçu d'un objet.
    ro::ui::StatusPreviewView status_preview;
    // L'ILLUSTRATION QUE LE PNJ MONTRE, s'il en montre une. Elle voyage avec le
    // dialogue parce qu'elle se dessine avec lui, juste avant : il passe devant.
    std::optional<ro::ui::NpcIllustrationView> illustration;
  };

  // CE QUE LA FICHE MONTRE, groupé : trois vues qui ne se lisent qu'ensemble,
  // et qui viendraient sinon s'ajouter une à une à une signature déjà longue.
  struct CharacterSheetView {
    ro::ui::CharacterHeaderView header;
    std::span<const ro::ui::EquipSlotView> slots;
    ro::ui::CharacterStatsView stats;
    ro::ui::PresetTabView presets;
    ro::ui::CharacterTitlesView titles;
    ro::ui::GuildView guild;
    // Les onglets de l'homoncule et du mercenaire.
    ro::ui::CompanionTabsView companions;
    // L'INVITATION DANS UNE GUILDE, et LA GUILDE qui propose une alliance à la
    // nôtre, en attente de réponse, ou vides. ⚠ HORS DE LA FICHE : elles
    // arrivent fenêtre fermée, et doivent se voir quand même.
    std::string_view guild_invitation;
    std::string_view guild_alliance;
  };

  // Ce qu'une fenêtre de conversation a demandé, et laquelle.
  struct ChatWindowAsk {
    std::uint32_t group = 0;
    ro::ui::ChatWindowAction asked;
  };

  // Ce que les fenêtres gardent d'une image à l'autre. Groupé plutôt qu'énuméré
  // en paramètres : chaque fenêtre nouvelle en ajouterait un, et l'ordre de
  // quatre booléens voisins devient vite un piège.
  struct Windows {
    std::optional<ro::ui::MapSignView> map_sign;  // le bandeau de la carte, composé par `app`
    // La liaison au serveur : bandeau et boîte de perte, composés par `app`.
    ro::ui::LinkNoticeState link_notice;
    BalloonBoards balloons;  // les bulles d'annonce, composées à chaque image
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
      // La place de chaque icône, tenue par les réglages. Nulle, rien ne se
      // dessine.
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
    // LA BOUTIQUE DE VOTE, même aller-retour. Vue nulle, rien ne se dessine :
    // c'est le serveur qui ouvre.
    struct CashShop {
      ro::ui::CashShopWindowState state;
      const ro::ui::CashShopView* view = nullptr;
      ro::ui::CashShopWindowAction answer;
    };
    CashShop cash_shop;
    // LES COMMERCES — boutiques de PNJ, échoppes, troc —, groupés à part : voir
    // `game/trade_windows.h` et `ui_stage_trade.cpp`.
    TradeWindows trade;
    // LE COURRIER : la boîte et l'écriture. Voir `ui_stage_mail.cpp`.
    MailWindows mail;
    // LES SALONS : le formulaire, la salle, l'invite. Voir
    // `ui_stage_chat_room.cpp`.
    ChatRoomWindows chat_room;
    WhisperWindows whispers;            // Les conversations 1:1 : `ui_stage_whisper.cpp`.
    AchievementWindows achievements;    // La fenêtre, la bannière : `ui_stage_achievements.cpp`.
    QuestWindows quests;                // Le journal : `ui_stage_quests.cpp`.
    TrackerWindows tracker;             // Le suivi commun aux succès, de même.
    MvpTrackerWindows mvp_tracker;      // Le carnet MVP : `ui_stage_mvp_tracker.cpp`.
    MemorialWindows memorial;           // L'instance : `game/memorial_stage.h`.
    BattlegroundWindows battleground;   // Voir `ui_stage_battleground.cpp`.
    RankingBoardWindows ranking_board;  // Voir `ui_stage_ranking.cpp`.
    PartyBookingWindows party_booking;  // Voir `ui_stage_party_booking.cpp`.
    PartyWindows party;                 // Groupe, amis et grille : `game/party_windows.h`.
    MusicPlayerWindows music_player;    // Le lecteur : `world/music_player_stage.h`.
    // Les éléments déverrouillés à l'écran, déjà traduits : l'avis de pose
    // (`game/hud_lock.h`) paraît tant qu'il y en a un, et les nomme.
    std::vector<const char*> unlocked_hud;
  };

  // CE QU'UNE IMAGE D'INTERFACE REND À LA BOUCLE.
  //
  // Groupé plutôt que rendu par un seul type : deux fenêtres émettent déjà, et
  // la troisième ajouterait un second paramètre de sortie. Ce qui est vide ne
  // coûte rien.
  struct Answers {
    // Les réponses à l'invitation et à l'alliance : accepter, ou refuser.
    std::optional<bool> guild_invitation;
    std::optional<bool> guild_alliance;
    // Vrai quand une jauge libre vient d'être déplacée ou retaillée : la
    // boucle persiste alors le fichier, UNE fois.
    bool stat_bars_moved = false;
    // La case de l'avis de pose vient d'être cochée : tout verrouiller.
    bool lock_hud = false;
    // Son autre case : ne plus montrer l'avis du tout.
    bool hide_unlock_notice = false;
    bool pvp_hud_moved = false;
    bool menu_icons_moved = false;
    // Ce que le compteur de dégâts a demandé.
    ro::ui::DamageMeterAction damage_meter;
    // Ce que la fenêtre des macros a demandé : une ligne à envoyer, l'écran des
    // raccourcis, ou le fichier à mettre à jour.
    ro::ui::ChatMacroAsk macros;
    ro::ui::NpcDialogAnswer npc;
    // Ce que CHAQUE fenêtre de conversation a demandé, avec le groupe qui la
    // désigne.
    //
    // 🔴 UNE LISTE, ET APPLIQUÉE APRÈS LE DESSIN DE TOUTES. Un retrait, un
    // déplacement ou un détachement réordonne la liste des onglets : agir entre
    // deux fenêtres ferait lire à la suivante des rangs périmés, et elle
    // toucherait l'onglet d'à côté — ou celui d'une autre fenêtre.
    std::vector<ChatWindowAsk> chat;
    // Où une languette arrachée vient d'être déposée. ⚠ À PART DU RESTE : elle ne
    // vient d'aucune fenêtre en particulier, elle vient de l'écran.
    ro::ui::ChatDrop chat_drop;
    // Ce que le joueur a demandé à sa fiche : retirer une pièce, monter une
    // caractéristique — et, dans son onglet, ce qu'il a demandé au grimoire.
    ro::ui::CharacterWindowAction character;
    ro::ui::PaletteEditorAction style;
    // Ce que l'aperçu d'un style reçu a demandé : l'essayer, ou le copier.
    ro::ui::StylePreviewVerb style_preview = ro::ui::StylePreviewVerb::kNothing;
    // Ce qu'il a survole dans son sac. La fenetre est en LECTURE : elle ne
    // rend pas de verbe, et son absence se voit mieux qu'une moitie de geste.
    ro::ui::InventoryWindowAction inventory;
    // Ce qu'il a sorti de sa charrette.
    ro::ui::CartWindowAction cart;
    // Ce qu'il a sorti de son coffre — ou sa demande de le refermer.
    ro::ui::StorageWindowAction storage;
    // Ce qu'il a déposé, retiré — ou sa demande de refermer la banque.
    ro::ui::BankWindowAction bank;
    // Ce qu'il a demandé à l'album de cartes.
    ro::ui::CardAlbumWindowAction card_album;
    // La pièce sur laquelle il sertit — ou sa demande de refermer.
    ro::ui::CardInsertWindowAction card_insert;
    // Ce qu'il a demandé aux deux ateliers.
    ro::ui::MakeItemWindowAction make_item;
    ro::ui::WeaponRefineWindowAction weapon_refine;
    // La destination qu'il a choisie — ou son annulation.
    ro::ui::WarpListWindowAction warp_list;
    // La charrette qu'il a choisie — ou la croix.
    ro::ui::CartChoiceWindowAction cart_choice;
    // Le sort qu'il arme — ou le refus.
    ro::ui::SpellListWindowAction spell_list;
    // La pièce à réparer — ou le refus.
    ro::ui::RepairWindowAction repair;
    // Ce que la fiche du familier et la liste d'éclosion ont demandé.
    ro::ui::PetWindowAction pet;
    ro::ui::PetHatchWindowAction pet_hatch;
    // Le message de Talkie Box qu'il a écrit — ou son abandon.
    ro::ui::TalkboxWindowAction talkbox;
    // Les réglages avancés d'une tactique, changés ou refermés.
    ro::ui::CompanionTacticWindowAction companion_tactic;
    ro::ui::CompanionAiWindowAction companion_ai;
    // L'illustration du PNJ, qu'il a fermée ou non.
    ro::ui::NpcIllustrationAction npc_illustration;
    // Ce qu'il a choisi dans le menu du jeu, et dans la configuration des
    // raccourcis.
    ro::ui::GameMenuChoice menu = ro::ui::GameMenuChoice::kNothing;
    ro::ui::HotkeyWindowAction hotkeys;
    ro::ui::OptionsWindowAction options;
    // Ou la poignee du bandeau d'obtention vient d'etre posee. ⚠ ELLE NE
    // REND QUE LA POSITION : le bandeau lui-meme ne capte aucun clic, il se
    // peint sur la liste de premier plan.
    ro::ui::ItemObtainToastAsk obtained;
    // Ce qu'il a fait de sa barre : lancer, vider, déposer.
    ro::ui::ShortcutBarsAction shortcuts;
    // Le lien qu'il vient de suivre dans une description — d'objet, puis de
    // compétence. ⚠ DEUX RÉPONSES : les deux fenêtres portent des liens, et une
    // seule réponse ferait perdre celui de la seconde dessinée.
    ro::ui::DescriptionWindowAction description;
    // ⚠ CELLE D'UNE COMPÉTENCE PORTE DEUX DEMANDES DE PLUS : le cast et les
    // dégâts se demandent au serveur, et seule cette fenêtre sait quand.
    ro::ui::SkillDescriptionAction skill_description;
    ro::ui::BugReportAction bug_report;
    ro::ui::PlayerReportAnswer player_report;
    ro::ui::BookWindowAction book;
    // L'OBJET LÂCHÉ SUR LE MONDE, hors de toute fenêtre. ⚠ À PART DU RESTE,
    // comme la languette de conversation : il ne vient d'aucune fenêtre en
    // particulier — il vient de l'écran.
    bool dropped_on_nothing = false;
    ro::ui::ItemDrag dropped_item;
    // Le rectangle qu'il vient de tracer pour la capture, ou son abandon.
    ro::ui::ZoneCaptureAsk capture;
  };

  // Échoue si l'un des deux dos d'ImGui refuse de s'ouvrir. Le jeu se passe
  // alors d'interface plutôt que de ne pas démarrer.
  //
  // ⚠ `windows` EST RETENU D'UNE PARTIE À L'AUTRE — place, taille et ouverture.
  // Le contexte en garde l'ADRESSE pour les écrire dans ses réglages : il doit
  // donc survivre à cet étage, ce que la boucle assure en le déclarant AVANT.
  // `font_family` est le rang de la police choisie, moins de zéro pour celle
  // d'ImGui. Il vient des réglages, déjà lus. `loose_font_folder` est le
  // dossier où le joueur dépose ses propres polices — voir `FindLooseFonts`.
  // `settings_file` est le fichier où l'interface retient la place et
  // l'ouverture de ses fenêtres ; vide, elle s'en passe plutôt que d'écrire au
  // hasard.
  [[nodiscard]] static ro::core::Result<UiStage> Open(
      SDL_Window* window, Windows& windows, int font_family,
      const std::filesystem::path& loose_font_folder, const std::filesystem::path& settings_file);

  // Les polices du dossier du jeu que l'atlas a su lire, par leur nom.
  [[nodiscard]] std::vector<std::string> LooseFontNames() const { return ui_->LooseFontNames(); }

  // L'HABILLAGE, recopié en identifiants et tailles.
  //
  // ⚠ POSÉ APRÈS L'OUVERTURE, et il le faut : l'interface s'ouvre avec le
  // contexte graphique, les pièces se lisent plus tard avec le reste des
  // accessoires. Tant qu'il n'est pas posé, les fenêtres gardent leur aspect
  // ordinaire — ce qui est exactement ce qu'on veut d'une installation sans art.
  //
  // C'est ici, et nulle part ailleurs, que la texture devient un identifiant :
  // `ui` ne connaît pas OpenGL.
  void UseSkin(const ro::render::SkinTextures& textures);
  // L'habillage que le joueur règle : posé à chaque image, il change en jeu.
  void SetSkinLook(const ro::ui::SkinLook& look) { skin_.SetLook(look); }

  // L'échelle de l'interface. Voir `UiContext::SetInterfaceScale` : c'est un
  // réglage du joueur, retenu d'une partie à l'autre.
  void SetInterfaceScale(float scale) { ui_->SetInterfaceScale(scale); }
  [[nodiscard]] float interface_scale() const { return ui_->interface_scale(); }

  // LA DISPOSITION DES FENÊTRES, lue et posée à chaud. Voir
  // `UiContext::CurrentLayout` et `ui::WindowPlacer`.
  [[nodiscard]] ro::ui::LayoutProfile CurrentLayout() const { return ui_->CurrentLayout(); }
  [[nodiscard]] std::map<std::string, bool> LayoutToggles() const { return ui_->LayoutToggles(); }
  void UseLayout(const ro::ui::LayoutProfile& layout) { ui_->UseLayout(layout); }
  [[nodiscard]] std::uint64_t layout_revision() const { return ui_->layout_revision(); }

  // Change la police de toute l'interface, TOUT DE SUITE.
  //
  // ⚠ AUCUN ATLAS N'EST REBÂTI : toutes les familles y sont déjà, bakées à
  // l'ouverture, et la bascule ne coûte qu'un changement de pointeur. C'est
  // précisément pourquoi elles sont toutes chargées d'avance.
  // `family` est un rang de `SystemFontFamilies()` — celui du réglage — et non de
  // l'atlas : voir `UiContext::ChooseFont`.
  void ChooseFont(int family);

  // La police des dégâts écrits en police : celle de ce nom, ou celle de
  // l'interface quand le nom est vide ou inconnu.
  [[nodiscard]] ImFont* DamageFont(std::string_view name, bool bold) const;

  // Écrit les réglages s'ils ont bougé. À appeler une fois par image : sans
  // cela, une fenêtre déplacée puis le jeu fermé perdrait sa place.
  void SaveSettings() { ui_->SaveSettingsIfDirty(); }

  // Donne l'événement à l'interface. À appeler sur CHAQUE événement, avant
  // toute autre décision : ImGui ne sait ce qu'il y a sous le curseur que s'il
  // a vu la souris bouger.
  void TakeEvent(const SDL_Event& event) const;

  // Qui reçoit le geste. Se lit APRÈS `TakeEvent`, et porte le verdict de
  // l'image précédente — c'est le fonctionnement normal d'une interface en mode
  // immédiat, et non un retard à corriger.
  [[nodiscard]] ro::ui::InputGate gate() const { return ui_->gate(); }

  // ÉCHAP A-T-IL UNE FENÊTRE À FERMER ? Tant que oui, la touche ne doit pas
  // quitter le jeu : elle ferme, une par une, la plus au-dessus d'abord.
  [[nodiscard]] bool escape_has_work() const { return escape_.has_work(); }

  // Ouvre la trame, dessine ce qui est ouvert, la ferme et la peint.
  //
  // ⚠ EN DERNIER DANS L'IMAGE : rien de ce que l'interface dessine ne doit être
  // masqué par le monde.
  //
  // `dialog` est le dialogue en cours, ou nul. Ce qu'il rend est ce que le
  // joueur a DEMANDÉ : la boucle le route vers le serveur, cet étage ne touche à
  // rien lui-même.
  //
  // UNE IMAGE D'INTERFACE DE L'ÉCRAN DE CONNEXION.
  //
  // ⚠ À LA PLACE DE `Draw`, jamais avec : les deux ouvrent et ferment la même
  // image d'ImGui, et un formulaire n'a rien à dire aux `Answers` du JEU.
  // `map_sign` : le bandeau de la ville du décor, ou nul. `settings` : les
  // réglages du login, sur `pages`, qui répondent dans `settings_asked`.
  [[nodiscard]] ro::ui::LoginWindowAction DrawLobby(
      ro::ui::LoginWindowState& login, const ro::ui::LoginWindowView& view,
      const ro::ui::MapSignView* map_sign, ro::ui::OptionsWindowState& settings,
      std::span<const ro::ui::OptionSectionView> pages,
      ro::ui::OptionsWindowAction* settings_asked);

  // CE QUE LA SCÈNE DE SÉLECTION A RENDU : la tablée, et la création qui se
  // pose dessus.
  //
  // ⚠ LES DEUX ENSEMBLE, contrairement au formulaire : la création n'est pas
  // un autre ÉCRAN mais une fenêtre POSÉE sur la salle, qui reste visible
  // derrière elle. Les séparer reviendrait à effacer la tablée le temps de
  // choisir un nom.
  struct LobbySeatsAnswers {
    ro::ui::CharacterSelectAction seats;
    ro::ui::CharacterCreateAction create;
    ro::ui::CharacterDeleteAction remove;
  };

  // CE QUE LA SCÈNE DE SÉLECTION MONTRE, d'un seul tenant.
  //
  // ⚠ UNE STRUCTURE PLUTÔT QU'UNE FILE DE SIX PARAMÈTRES, dont trois vues et
  // deux états : un appelant qui en intervertit deux ne s'en aperçoit qu'à
  // l'écran.
  struct LobbySeatsFrame {
    const ro::ui::CharacterSelectView* seats = nullptr;
    const ro::ui::CharacterCreateView* create = nullptr;
    const ro::ui::CharacterDeleteView* remove = nullptr;
  };

  // UNE IMAGE D'INTERFACE DE LA SCÈNE DE SÉLECTION.
  //
  // ⚠ SA JUMELLE, ET JAMAIS EN MÊME TEMPS : les deux écrans du démarrage se
  // succèdent, et chacune de ces fonctions ouvre et ferme la même image
  // d'ImGui. Deux fonctions plutôt qu'un aiguillage à drapeau : ce que l'une
  // rend n'a rien à voir avec ce que rend l'autre, et les fondre demanderait
  // de rendre les deux réponses à chaque image.
  [[nodiscard]] LobbySeatsAnswers DrawCharacters(ro::ui::CharacterSelectState& seats,
                                                 ro::ui::CharacterCreateState& create,
                                                 ro::ui::CharacterDeleteState& remove,
                                                 const LobbySeatsFrame& frame);

  // UNE IMAGE OÙ L'INTERFACE NE SE DESSINE PAS : l'écran de veille.
  //
  // 🔴 ON SAUTE LE DESSIN, PAS L'ÉTAT, et c'est toute la différence avec le
  // « cacher l'interface » du client d'origine — lequel FERME les fenêtres pour
  // les rouvrir ensuite. Acceptable pour un geste volontaire, inacceptable pour
  // un basculement automatique : partir se faire un café ne doit pas ranger ce
  // qu'on avait sorti. Ici rien n'est touché ; l'image suivante redessine tout.
  //
  // ⚠ ELLE OUVRE ET FERME LA MÊME IMAGE D'IMGUI que ses deux jumelles, et
  // jamais en même temps qu'elles.
  void DrawAfkOnly(const ro::ui::AfkClockView& clock, const OverWorldPaint& over_world = {});

  // UNE IMAGE D'ÉCRAN DE CHARGEMENT, et rien d'autre : voir
  // `ro/ui/loading_screen.h`. Même règle que ses jumelles — jamais en même
  // temps qu'elles.
  void DrawLoadingOnly(const char* text);

  // UNE IMAGE D'ÉCRAN DE MISE À JOUR : voir `ro/ui/patch_screen.h`. Même règle
  // que ses jumelles.
  [[nodiscard]] ro::ui::PatchScreenAnswers DrawPatchOnly(const ro::ui::PatchScreenView& view);

  // `skills` porte les onglets du grimoire, DÉJÀ composés : `ui` ne connaît ni
  // l'arbre du client ni le livre du serveur.
  [[nodiscard]] Answers Draw(
      const ro::net::FrameTally& tally, Windows& windows, const NpcTalk& talk,
      const ro::ui::ChatLinePreview& chat_preview, const ChatTabsView& chat_tabs,
      // LES LIENS DÉJÀ POSÉS DANS LA SAISIE, à peindre en pastilles. ⚠ SEULE LA
      // FENÊTRE PRINCIPALE EN A : elle seule porte une saisie.
      std::span<const ro::ui::ChatInputLink> chat_links,
      std::span<const ro::ui::ChatEmotion> emotions, std::span<const ro::ui::ChatEmojiGroup> emojis,
      std::span<const ro::ui::SkillGroupView> skills, ro::ui::SkillPointTally points,
      const CharacterSheetView& sheet,
      // CE QUE L'ÉDITEUR DE STYLE MONTRE : les pièces du corps porté, leurs
      // pastilles, et le pantin que `app` a composé.
      const ro::ui::PaletteEditorView& style,
      // ET CE QUE MONTRE L'APERÇU D'UN STYLE REÇU PAR LE FIL. ⚠ UNE AUTRE
      // FENÊTRE, sur un autre pantin : les deux peuvent être ouvertes en même
      // temps sur deux recettes différentes.
      const ro::ui::StylePreviewView& style_preview, std::span<const ro::ui::InventoryCellView> bag,
      ro::ui::InventoryTally bag_tally, std::span<const ro::ui::InventoryCellView> cart,
      ro::ui::CartTally cart_tally, std::span<const ro::ui::InventoryCellView> storage,
      ro::ui::StorageTally storage_tally,
      // LES DEUX SOLDES DE LA BANQUE, déjà recopiés du monde.
      const ro::ui::BankWindowView& bank, const ro::ui::HotkeyWindowView& hotkeys,
      std::span<const ro::ui::ShortcutSlotView> shortcuts,
      const ro::ui::ShortcutBarsLayout& shortcut_bars,
      // LES ÉTATS DU JOUEUR, DÉJÀ CROISÉS : `ui` ne connaît ni les EFST, ni les
      // tables de l'installation, ni les textures.
      std::span<const ro::ui::StatusCellView> statuses, const ro::ui::StatusBarLayout& status_bar,
      // LES SIX JAUGES LIBRES. ⚠ `look` N'EST PAS CONST : le joueur les
      // déplace à la souris, et c'est le dessin qui écrit leur géométrie.
      // `Answers::stat_bars_moved` dit quand il faut persister.
      const ro::ui::StatBarValues& stat_bar_values,
      const ro::ui::StatBarWarnings& stat_bar_warnings, ro::ui::StatBarsLook& stat_bars,
      // LES DEUX CADRES DU CLASSEMENT PVP. ⚠ PAS CONST NON PLUS, et pour la même
      // raison que les jauges : le joueur les pose à la souris.
      ro::ui::PvpHudLook& pvp_hud, std::span<const ro::ui::OptionSectionView> options,
      const ro::ui::DescriptionView& described, const ro::ui::DescriptionView& previewed,
      const ro::ui::SkillDescriptionView& skill_described, const ro::ui::AlignGrid& grid,
      std::span<const ro::ui::ChatBubbleView> bubbles, const ro::ui::ChatBubbleLook& bubble_look,
      // LES BARRES DE VIE ET DE MAGIE DES ENTITÉS, déjà projetées en pixels par
      // `app` : `ui` ne connaît ni caméra ni monde. Le FILTRE — l'espèce, le
      // survol, la cible, le seuil d'entame — se joue au dessin, contre le
      // réglage, et non à la récolte.
      std::span<const ro::ui::EntityBarView> entity_bars,
      const ro::ui::EntityBarsLook& entity_bar_look,
      // LES BARRES D'INCANTATION, projetées de même. Le filtre se joue au dessin.
      std::span<const ro::ui::CastBarView> cast_bars, const ro::ui::CastBarsLook& cast_bar_look,
      // LES NOMBRES DE DÉGÂTS QUE LE RÉGLAGE ÉCRIT EN POLICE, projetés de même,
      // et la police qu'il leur choisit.
      const ro::ui::DamageTexts& damage_texts,
      // CE QUE LE SERVEUR AUTORISE. ⚠ Relu à chaque image, jamais mémorisé : un
      // droit se perd en cours de session comme il s'accorde.
      ro::ui::SessionRights rights,
      // CE QUE LE MENU D'ÉCHAP PROPOSE : la disposition dépend de l'état du
      // personnage, et se relit à chaque image — on peut mourir menu ouvert.
      const ro::ui::GameMenuFacts& menu_facts,
      // Ce que le menu du clic droit des macros propose, DÉJÀ TRADUIT : `ui` ne
      // connaît aucune commande de serveur.
      std::span<const ro::ui::ChatMacroCommandGroup> macro_commands,
      // LE BANDEAU D'OBTENTION, déjà composé : `ui` ne connaît ni sac, ni table
      // d'objets. ⚠ APRÈS LES FENÊTRES DANS LE DESSIN — une notification ne se
      // range pas derrière celle qu'on vient d'ouvrir — et sa poignée n'est pas
      // const : le joueur la déplace, et `Answers::obtained` dit quand
      // persister.
      std::span<const ro::ui::ItemObtainToastEntry> obtained,
      const ro::ui::ItemObtainToastLook& obtained_look, ro::ui::ItemObtainAnchorGrip& obtained_grip,
      // CE QUE L'ENREGISTREUR DE ZONE MONTRE : le tracé, le décompte, le témoin.
      // ⚠ TOUT EN DERNIER DANS LE DESSIN, et par-dessus le reste : le tracé
      // recouvre l'écran, et le témoin doit rester visible au-dessus des
      // fenêtres qu'on est justement en train de filmer.
      const ro::ui::ZoneCaptureView& capture, std::int32_t round_trip_ms,
      const OverWorldPaint& over_world = {});

 private:
  // Glisse le rappel dans la liste de fond, puis rend à l'interface l'état
  // qu'il a changé.
  static void PaintOverWorld(const OverWorldPaint& over_world);
  // LES COMMERCES : leur dessin, ce que le menu d'un item sait d'avance, et
  // leur place dans la pile d'Échap. Voir `ui_stage_trade.cpp`.
  void DrawTradeWindows(TradeWindows& trade, ro::ui::ItemLinkFacts link_facts);
  void RegisterTradeEscapes(TradeWindows& trade);
  // LE COURRIER, de même : voir `ui_stage_mail.cpp`.
  void DrawMailWindows(MailWindows& mail, ro::ui::ItemLinkFacts link_facts);
  void RegisterMailEscapes(MailWindows& mail);
  // LES SALONS, de même : voir `ui_stage_chat_room.cpp`. La salle porte la barre
  // de la conversation : ce qu'elle rend s'ajoute à celui des conversations.
  void DrawChatRoomWindows(ChatRoomWindows& chat_room, ro::ui::ChatWindowState& chat,
                           const ro::ui::ChatLinePreview& preview, const ChatTabsView& tabs,
                           std::span<const ro::ui::ChatInputLink> links,
                           ro::ui::ItemLinkFacts link_facts, Answers& answers);
  void RegisterChatRoomEscapes(ChatRoomWindows& chat_room);
  void DrawWhisperWindows(WhisperWindows& whispers, const ChatTabsView& tabs,
                          std::span<const ro::ui::ChatEmotion> emotions,
                          std::span<const ro::ui::ChatEmojiGroup> emojis,
                          ro::ui::ItemLinkFacts link_facts, Answers& answers);
  void RegisterWhisperEscapes(WhisperWindows& whispers);  // 1:1 : `ui_stage_whisper.cpp`
  // LA CONVERSATION, ses salons et ses 1:1 : voir `ui_stage_chat.cpp`.
  void DrawConversation(Windows& windows, const ro::ui::ChatLinePreview& preview,
                        const ChatTabsView& tabs, std::span<const ro::ui::ChatInputLink> links,
                        std::span<const ro::ui::ChatEmotion> emotions,
                        std::span<const ro::ui::ChatEmojiGroup> emojis, Answers& answers);
  void DrawAchievementWindows(AchievementWindows& achievements);  // `ui_stage_achievements.cpp`
  void RegisterAchievementEscapes(AchievementWindows& achievements);
  void DrawQuestWindows(QuestWindows& quests, TrackerWindows& tracker);  // `ui_stage_quests.cpp`
  void RegisterQuestEscapes(QuestWindows& quests);
  void DrawMvpTrackerWindows(MvpTrackerWindows& mvp);  // `ui_stage_mvp_tracker.cpp`
  void RegisterMvpTrackerEscapes(MvpTrackerWindows& mvp);
  void DrawBattlegroundWindows(BattlegroundWindows& battleground);  // `ui_stage_battleground.cpp`
  void RegisterBattlegroundEscapes(BattlegroundWindows& battleground);
  void DrawRankingWindows(RankingBoardWindows& ranking);  // `ui_stage_ranking.cpp`
  void RegisterRankingEscapes(RankingBoardWindows& ranking);
  void DrawPartyBookingWindows(PartyBookingWindows& booking);  // `ui_stage_party_booking.cpp`
  void RegisterPartyBookingEscapes(PartyBookingWindows& booking);
  // LA NAVIGATION ET SES DEUX INFOBULLES : voir `ui_stage_navigation.cpp`.
  void DrawNavigationWindows(Windows::Navigation& navigation);
  void RegisterNavigationEscapes(Windows::Navigation& navigation);
  UiStage(std::unique_ptr<ro::ui::UiContext> ui, ro::ui::UiBackendGl backend);

  // 🔴 UN SEUL ÉTAT DE GESTE POUR TOUS LES CADRES DE HUD, parce qu'il n'y a
  // qu'une souris. Un par surface autoriserait « deux cadres se croient
  // attrapés », que rien ne signalerait.
  ro::ui::HudGrip hud_grip_;

  // ⚠ PAR POINTEUR, parce que `UiContext` ne se déplace pas : il se désigne
  // lui-même auprès d'ImGui. Cet étage, lui, doit rester déplaçable — `Open`
  // le rend par valeur —, et c'est le pointeur qui bouge alors, pas l'objet.
  std::unique_ptr<ro::ui::UiContext> ui_;
  // ⚠ APRÈS le contexte, et c'est une contrainte de destruction : ImGui range
  // l'état du dos DANS le contexte, si bien que détruire celui-ci d'abord
  // ferait écrire la fermeture du dos dans de la mémoire libérée. L'ordre des
  // membres est l'ordre de construction, et l'inverse à la destruction.
  ro::ui::UiBackendGl backend_;
  ro::ui::SkinArt skin_;
  ro::ui::EscapeStack escape_;
  // Le rectangle de capture en cours de tracé. Voir `ro::ui::ZoneTraceState`.
  ro::ui::ZoneTraceState zone_trace_;
};

}  // namespace moonclient
