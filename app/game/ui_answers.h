#pragma once

// CE QU'UNE IMAGE D'INTERFACE REND À LA BOUCLE : `UiStage::Answers`.
//
// ⚠ CET EN-TÊTE NE TIRE AUCUNE FENÊTRE, pour la même raison que
// `game/ui_windows.h` : chaque réponse y est tenue par `Held`, et une source qui
// en lit une inclut l'en-tête de sa fenêtre. Constructeur, destructeur et
// déplacement vivent dans `game/ui_answers.cpp`.

#include <cstdint>
#include <optional>
#include <vector>

#include "game/held.h"
#include "game/ui_stage_fwd.h"

namespace ro::ui {
enum class GameMenuChoice : std::uint8_t;
enum class StylePreviewVerb : std::uint8_t;
struct BankWindowAction;
struct BookWindowAction;
struct BugReportAction;
struct CardAlbumWindowAction;
struct CardInsertWindowAction;
struct CartChoiceWindowAction;
struct CartWindowAction;
struct CharacterWindowAction;
struct ChatDrop;
struct ChatMacroAsk;
struct CompanionAiWindowAction;
struct CompanionTacticWindowAction;
struct DamageMeterAction;
struct DescriptionWindowAction;
struct HotkeyWindowAction;
struct InventoryWindowAction;
struct ItemDrag;
struct ItemObtainToastAsk;
struct MakeItemWindowAction;
struct NpcDialogAnswer;
struct NpcIllustrationAction;
struct OptionsWindowAction;
struct PaletteEditorAction;
struct PetHatchWindowAction;
struct PetWindowAction;
struct PlayerReportAnswer;
struct RepairWindowAction;
struct ShortcutBarsAction;
struct SkillDescriptionAction;
struct SpellListWindowAction;
struct StorageWindowAction;
struct TalkboxWindowAction;
struct WarpListWindowAction;
struct WeaponRefineWindowAction;
struct ZoneCaptureAsk;
}  // namespace ro::ui

namespace moonclient {

// CE QU'UNE IMAGE D'INTERFACE REND À LA BOUCLE.
//
// Groupé plutôt que rendu par un seul type : deux fenêtres émettent déjà, et
// la troisième ajouterait un second paramètre de sortie. Ce qui est vide ne
// coûte rien.
struct UiAnswers {
  UiAnswers();
  ~UiAnswers();
  UiAnswers(UiAnswers&&) noexcept;
  UiAnswers(const UiAnswers&) = delete;
  UiAnswers& operator=(const UiAnswers&) = delete;
  UiAnswers& operator=(UiAnswers&&) = delete;

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
  Held<ro::ui::DamageMeterAction> damage_meter;
  // Ce que la fenêtre des macros a demandé : une ligne à envoyer, l'écran des
  // raccourcis, ou le fichier à mettre à jour.
  Held<ro::ui::ChatMacroAsk> macros;
  Held<ro::ui::NpcDialogAnswer> npc;
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
  Held<ro::ui::ChatDrop> chat_drop;
  // Ce que le joueur a demandé à sa fiche : retirer une pièce, monter une
  // caractéristique — et, dans son onglet, ce qu'il a demandé au grimoire.
  Held<ro::ui::CharacterWindowAction> character;
  Held<ro::ui::PaletteEditorAction> style;
  // Ce que l'aperçu d'un style reçu a demandé : l'essayer, ou le copier.
  ro::ui::StylePreviewVerb style_preview{};  // {} : kNothing, le premier.
  // Ce qu'il a survole dans son sac. La fenetre est en LECTURE : elle ne
  // rend pas de verbe, et son absence se voit mieux qu'une moitie de geste.
  Held<ro::ui::InventoryWindowAction> inventory;
  // Ce qu'il a sorti de sa charrette.
  Held<ro::ui::CartWindowAction> cart;
  // Ce qu'il a sorti de son coffre — ou sa demande de le refermer.
  Held<ro::ui::StorageWindowAction> storage;
  // Ce qu'il a déposé, retiré — ou sa demande de refermer la banque.
  Held<ro::ui::BankWindowAction> bank;
  // Ce qu'il a demandé à l'album de cartes.
  Held<ro::ui::CardAlbumWindowAction> card_album;
  // La pièce sur laquelle il sertit — ou sa demande de refermer.
  Held<ro::ui::CardInsertWindowAction> card_insert;
  // Ce qu'il a demandé aux deux ateliers.
  Held<ro::ui::MakeItemWindowAction> make_item;
  Held<ro::ui::WeaponRefineWindowAction> weapon_refine;
  // La destination qu'il a choisie — ou son annulation.
  Held<ro::ui::WarpListWindowAction> warp_list;
  // La charrette qu'il a choisie — ou la croix.
  Held<ro::ui::CartChoiceWindowAction> cart_choice;
  // Le sort qu'il arme — ou le refus.
  Held<ro::ui::SpellListWindowAction> spell_list;
  // La pièce à réparer — ou le refus.
  Held<ro::ui::RepairWindowAction> repair;
  // Ce que la fiche du familier et la liste d'éclosion ont demandé.
  Held<ro::ui::PetWindowAction> pet;
  Held<ro::ui::PetHatchWindowAction> pet_hatch;
  // Le message de Talkie Box qu'il a écrit — ou son abandon.
  Held<ro::ui::TalkboxWindowAction> talkbox;
  // Les réglages avancés d'une tactique, changés ou refermés.
  Held<ro::ui::CompanionTacticWindowAction> companion_tactic;
  Held<ro::ui::CompanionAiWindowAction> companion_ai;
  // L'illustration du PNJ, qu'il a fermée ou non.
  Held<ro::ui::NpcIllustrationAction> npc_illustration;
  // Ce qu'il a choisi dans le menu du jeu, et dans la configuration des
  // raccourcis.
  ro::ui::GameMenuChoice menu{};  // {} : kNothing, le premier.
  Held<ro::ui::HotkeyWindowAction> hotkeys;
  Held<ro::ui::OptionsWindowAction> options;
  // Ou la poignee du bandeau d'obtention vient d'etre posee. ⚠ ELLE NE
  // REND QUE LA POSITION : le bandeau lui-meme ne capte aucun clic, il se
  // peint sur la liste de premier plan.
  Held<ro::ui::ItemObtainToastAsk> obtained;
  // Ce qu'il a fait de sa barre : lancer, vider, déposer.
  Held<ro::ui::ShortcutBarsAction> shortcuts;
  // Le lien qu'il vient de suivre dans une description — d'objet, puis de
  // compétence. ⚠ DEUX RÉPONSES : les deux fenêtres portent des liens, et une
  // seule réponse ferait perdre celui de la seconde dessinée.
  Held<ro::ui::DescriptionWindowAction> description;
  // ⚠ CELLE D'UNE COMPÉTENCE PORTE DEUX DEMANDES DE PLUS : le cast et les
  // dégâts se demandent au serveur, et seule cette fenêtre sait quand.
  Held<ro::ui::SkillDescriptionAction> skill_description;
  Held<ro::ui::BugReportAction> bug_report;
  Held<ro::ui::PlayerReportAnswer> player_report;
  Held<ro::ui::BookWindowAction> book;
  // L'OBJET LÂCHÉ SUR LE MONDE, hors de toute fenêtre. ⚠ À PART DU RESTE,
  // comme la languette de conversation : il ne vient d'aucune fenêtre en
  // particulier — il vient de l'écran.
  bool dropped_on_nothing = false;
  Held<ro::ui::ItemDrag> dropped_item;
  // Le rectangle qu'il vient de tracer pour la capture, ou son abandon.
  Held<ro::ui::ZoneCaptureAsk> capture;
};

}  // namespace moonclient
