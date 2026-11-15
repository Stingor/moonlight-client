#pragma once

// CE QU'UNE IMAGE D'INTERFACE REND À LA BOUCLE : `UiStage::Answers`.
//
// ⚠ À PART DE `game/ui_stage.h`, pour la même raison que `game/ui_windows.h` :
// elle porte par valeur la réponse de chaque fenêtre, donc l'en-tête de chacune.
// Seules les sources qui LISENT une réponse l'incluent.

#include <cstdint>
#include <optional>
#include <vector>

#include "ro/ui/bank_window.h"
#include "ro/ui/book_window.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/card_album_window.h"
#include "ro/ui/card_insert_window.h"
#include "ro/ui/cart_choice_window.h"
#include "ro/ui/cart_window.h"
#include "ro/ui/character_window.h"
#include "ro/ui/chat_macros.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/chat_window.h"
#include "ro/ui/companion_ai_window.h"
#include "ro/ui/companion_tactic_window.h"
#include "ro/ui/damage_meter_window.h"
#include "ro/ui/description_window.h"
#include "ro/ui/game_menu.h"
#include "ro/ui/hotkey_window.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/item_obtain_toast.h"
#include "ro/ui/make_item_window.h"
#include "ro/ui/npc_dialog_window.h"
#include "ro/ui/npc_illustration_window.h"
#include "ro/ui/options_window.h"
#include "ro/ui/palette_editor.h"
#include "ro/ui/pet_hatch_window.h"
#include "ro/ui/pet_window.h"
#include "ro/ui/player_report_window.h"
#include "ro/ui/repair_window.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/spell_list_window.h"
#include "ro/ui/storage_window.h"
#include "ro/ui/style_preview.h"
#include "ro/ui/talkbox_window.h"
#include "ro/ui/warp_list_window.h"
#include "ro/ui/weapon_refine_window.h"
#include "ro/ui/zone_capture.h"

namespace moonclient {

// Ce qu'une fenêtre de conversation a demandé, et laquelle.
struct UiChatWindowAsk {
  std::uint32_t group = 0;
  ro::ui::ChatWindowAction asked;
};

// CE QU'UNE IMAGE D'INTERFACE REND À LA BOUCLE.
//
// Groupé plutôt que rendu par un seul type : deux fenêtres émettent déjà, et
// la troisième ajouterait un second paramètre de sortie. Ce qui est vide ne
// coûte rien.
struct UiAnswers {
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
  std::vector<UiChatWindowAsk> chat;
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

}  // namespace moonclient
