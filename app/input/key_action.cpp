#include "input/key_action.h"

#include <array>
#include <cstdio>

#include "ro/core/build_flavor.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/action.h"
#include "ro/render/actor_sprite.h"
#include "ro/sim/world.h"

#include "game/achievement_stage.h"
#include "game/bug_report_context.h"
#include "game/cash_shop_stage.h"
#include "game/chat_room_stage.h"
#include "game/companion_ai_stage.h"
#include "game/mail_stage.h"
#include "game/market_stage.h"
#include "game/memorial_stage.h"
#include "game/quest_stage.h"
#include "input/key_gate.h"
#include "mvp/mvp_tracker_stage.h"
#include "world/click_action.h"

namespace moonclient {
namespace {

// L'ÉMOTICÔNE DE CHAQUE DRAPEAU, de Ctrl+1 à Ctrl+9, telle que le client du
// joueur la choisit : les drapeaux coréen, indonésien, philippin, malais,
// singapourien, brésilien et indien, puis les huitième et neuvième. Ce sont les
// numéros du protocole — l'énumération `ET_*` des fichiers de l'installation.
constexpr std::array<std::uint16_t, kFlagEmoteSlots> kFlagEmotions = {13, 35, 48, 49, 50,
                                                                      51, 64, 66, 67};

// OUVRE LA FICHE SUR UN ONGLET, ou la referme quand elle y est déjà. Voir les
// trois règles au site d'appel.
void OpenSheetTab(ro::ui::CharacterWindowState* sheet, ro::ui::CharacterTab wanted) {
  if (sheet == nullptr) {
    return;
  }
  if (sheet->open && sheet->tab == wanted) {
    sheet->open = false;
    return;
  }
  sheet->open = true;
  // ⚠ LES DEUX, ET PAS SEULEMENT LA DEMANDE. `tab` sert AVANT la barre — c'est
  // lui qui décide des crans de largeur —, et la demande sert PENDANT, parce que
  // c'est ImGui qui possède la sélection. N'écrire que `tab` ne changeait
  // d'ailleurs rien du tout : la barre le réécrivait dans la foulée.
  sheet->tab = wanted;
  sheet->tab_request = wanted;
}

// La même bascule pour la fenêtre Groupe / Amis : ouverte sur cet onglet, elle
// se ferme ; sinon elle s'ouvre, ou change d'onglet.
void OpenPartyTab(ro::ui::PartyFriendWindowState* party, ro::ui::PartyFriendTab wanted) {
  if (party == nullptr) {
    return;
  }
  if (party->open && party->tab == wanted) {
    party->open = false;
    return;
  }
  party->open = true;
  party->tab = wanted;
}

// LA CIBLE D'UNE ACTION EST-ELLE BRANCHÉE ?
//
// ⚠ UN JOURNAL PLUTÔT QU'UN SILENCE. Une action dont la cible manque ne fait
// rien du tout, et cela se lit exactement comme un raccourci qui n'aurait pas
// pris : le joueur refait sa liaison, change de touche, et conclut que la
// configuration est cassée. Une ligne dans le journal nomme le vrai coupable.
[[nodiscard]] bool Wired(const void* target, const char* what) {
  if (target != nullptr) {
    return true;
  }
  ro::core::LogError("touche muette : la cible de « {} » n'est pas branchée", what);
  return false;
}

// Ce que le joueur tient en même temps que la touche, réduit à ce qui compte.
struct Modifiers {
  bool ctrl = false;
  bool shift = false;
  bool alt = false;
};

[[nodiscard]] Modifiers HeldIn(const SDL_KeyboardEvent& key) {
  return Modifiers{.ctrl = (key.mod & SDL_KMOD_CTRL) != 0,
                   .shift = (key.mod & SDL_KMOD_SHIFT) != 0,
                   .alt = (key.mod & SDL_KMOD_ALT) != 0};
}

// Une touche qui ne sert QU'À en modifier une autre ne fait pas un raccourci.
// Sans ce test, poser un raccourci deviendrait impossible : appuyer sur Alt pour
// faire « Alt+S » lierait l'action à Alt tout seul.
[[nodiscard]] bool IsModifierKey(SDL_Keycode key) {
  switch (key) {
    case SDLK_LCTRL:
    case SDLK_RCTRL:
    case SDLK_LSHIFT:
    case SDLK_RSHIFT:
    case SDLK_LALT:
    case SDLK_RALT:
    case SDLK_LGUI:
    case SDLK_RGUI:
      return true;
    default:
      return false;
  }
}

void PoseCaptured(ro::ui::HotkeyWindowState* hotkeys, Hotkeys* bindings, ActionRef action,
                  Binding wanted, PresetView* presets) {
  const std::optional<ActionRef> robbed = bindings->Bind(action, wanted);
  // Un preset qui tenait la touche la perd aussi : la table passe avant lui.
  const std::string preset_robbed =
      presets != nullptr ? presets->ReleaseHotkey(wanted) : std::string();
  // 🔴 CE QU'ON VIENT DE PRENDRE SE DIT. La touche est VOLÉE à son détenteur —
  // c'est ce que fait le jeu, et refuser laisserait le joueur retrouver la ligne
  // coupable, la délier, revenir, recommencer. Mais la table compte plus de cent
  // lignes : la victime est presque toujours hors de l'écran, et sans cette
  // phrase le joueur perdrait un raccourci sans jamais l'apprendre.
  std::string victims = robbed.has_value() ? NameOfAction(*robbed) : std::string();
  if (!preset_robbed.empty()) {
    victims += victims.empty() ? "" : ", ";
    victims += preset_robbed;
  }
  if (!victims.empty()) {
    std::snprintf(hotkeys->robbed, sizeof(hotkeys->robbed), "%s %s",
                  ro::core::Tr("Touche reprise à :"), victims.c_str());
  } else {
    hotkeys->robbed[0] = '\0';
  }
  hotkeys->capturing = -1;
}

// LA CAPTURE D'UN RACCOURCI. Rend vrai quand la touche lui était destinée — elle
// ne doit alors déclencher aucune action.
[[nodiscard]] bool CaptureBinding(const KeyPress& to, const SDL_KeyboardEvent& key) {
  if (to.hotkeys == nullptr || to.bindings == nullptr || to.hotkeys->capturing < 0) {
    return false;
  }
  const auto actions = AllBindableActions();
  const auto rank = static_cast<std::size_t>(to.hotkeys->capturing);
  if (rank >= actions.size()) {
    to.hotkeys->capturing = -1;
    return true;
  }
  if (IsModifierKey(key.key)) {
    return true;  // on attend encore la vraie touche
  }
  // Échap renonce, Suppr retire le raccourci : les deux gestes qu'on essaie
  // quand on s'est trompé de ligne.
  if (key.key == SDLK_ESCAPE) {
    to.hotkeys->capturing = -1;
    to.hotkeys->escape_spent = true;
    return true;
  }
  const Modifiers held = HeldIn(key);
  const Binding wanted =
      key.key == SDLK_DELETE
          ? Binding{}
          : Binding{.key = key.key, .ctrl = held.ctrl, .shift = held.shift, .alt = held.alt};
  // Avalée, et la capture attend toujours le geste.
  if (!BindingFits(actions[rank], wanted)) {
    return true;
  }
  PoseCaptured(to.hotkeys, to.bindings, actions[rank], wanted, to.presets);
  return true;
}

// La touche qu'attend l'onglet des presets. Toute touche est avalée pendant
// l'attente, même refusée, sinon Entrée ouvrirait la conversation ; Échap, lui,
// est lu par l'onglet, qui seul peut empêcher la même frappe de fermer la fiche.
[[nodiscard]] bool CapturePresetKey(const KeyPress& to, const SDL_KeyboardEvent& key) {
  if (to.presets == nullptr || to.sheet == nullptr || to.bindings == nullptr ||
      to.sheet->presets.capturing < 0) {
    return false;
  }
  if (IsModifierKey(key.key) || !PresetKeyCapturable(key.key)) {
    return true;  // on attend encore la vraie touche
  }
  const Modifiers held = HeldIn(key);
  to.presets->Capture(
      *to.bindings, to.sheet->presets,
      Binding{.key = key.key, .ctrl = held.ctrl, .shift = held.shift, .alt = held.alt});
  return true;
}

// Une touche de preset, fiche fermée comprise. Jamais sur une répétition du
// clavier, qui renverrait le lot ; pendant une saisie, sur F1 à F12 seulement —
// voir `PresetKeyLive`.
void TriggerPreset(const KeyPress& to, const SDL_KeyboardEvent& key) {
  if (to.presets == nullptr || to.sheet == nullptr || to.link == nullptr || to.world == nullptr ||
      key.repeat || !PresetKeyLive(to.text_input, key.key)) {
    return;
  }
  const Modifiers held = HeldIn(key);
  static_cast<void>(to.presets->TriggerHotkey(
      Binding{.key = key.key, .ctrl = held.ctrl, .shift = held.shift, .alt = held.alt},
      to.sheet->presets, *to.link, *to.world));
}

}  // namespace

bool CaptureGesture(ro::ui::HotkeyWindowState* hotkeys, Hotkeys* bindings, MouseGesture gesture,
                    SDL_Keymod held) {
  if (hotkeys == nullptr || bindings == nullptr || hotkeys->capturing < 0) {
    return false;
  }
  const auto actions = AllBindableActions();
  const auto rank = static_cast<std::size_t>(hotkeys->capturing);
  if (rank >= actions.size()) {
    return false;
  }
  const Binding wanted{.key = KeyOfGesture(gesture),
                       .ctrl = (held & SDL_KMOD_CTRL) != 0,
                       .shift = (held & SDL_KMOD_SHIFT) != 0,
                       .alt = (held & SDL_KMOD_ALT) != 0};
  if (!BindingFits(actions[rank], wanted)) {
    return false;
  }
  // Un geste de souris n'est jamais la touche d'un preset : rien à leur reprendre.
  PoseCaptured(hotkeys, bindings, actions[rank], wanted, nullptr);
  return true;
}

std::optional<std::string> HandleKey(const KeyPress& to, const SDL_KeyboardEvent& key,
                                     const ro::render::Camera& framed_map) {
  // ⚠ IL N'Y A PLUS DE LIGNE DE SAISIE ICI. Le clavier ne se partage plus à la
  // main : tant que le champ de la conversation l'a, le banc d'entrée ne nous
  // donne même pas les touches — écrire « r » ne peut plus recadrer la caméra au
  // milieu d'un mot, et ce n'est plus une précaution à tenir mais une propriété.
  //
  // ⚠ LA CAPTURE PASSE AVANT TOUT LE RESTE : tant que la configuration attend une
  // touche, aucune ne doit déclencher son action — sinon lier « Menu du jeu » à
  // Échap ouvrirait le menu au moment même où on le lie.
  if (CaptureBinding(to, key) || CapturePresetKey(to, key)) {
    return std::nullopt;
  }
  if (to.bindings == nullptr) {
    return std::nullopt;
  }

  // 🔴 QUICK CAST RETIENT TOUTE FRAPPE FRAÎCHE, AVANT QU'ELLE N'AGISSE : c'est
  // elle qui distingue ensuite un sort armé au clavier d'un sort armé au clic,
  // et seul le premier se relance tant que la touche est tenue.
  if (!key.repeat && to.quick_cast != nullptr) {
    to.quick_cast->NotePress(static_cast<std::uint32_t>(key.scancode), to.now_ms);
  }
  const Modifiers held = HeldIn(key);
  const std::optional<ActionRef> action =
      to.bindings->ActionFor(key.key, held.ctrl, held.shift, held.alt);
  // TOUTE TOUCHE FRAÎCHE SE NOTE, case de barre ou non : c'est la dernière
  // enfoncée qui décide laquelle pourra relancer sa compétence en restant
  // tenue. Voir `ShortcutBarView::Cast`.
  if (!key.repeat && to.shortcuts != nullptr && to.skills != nullptr) {
    to.shortcuts->NotePress(action.has_value() && action->is_shortcut()
                                ? std::optional<std::size_t>(action->shortcut_index())
                                : std::nullopt,
                            *to.skills);
  }
  // La table passe avant les presets : une touche qu'elle tient ne déclenche
  // que son action, et la ligne du preset le dit.
  if (!action.has_value()) {
    TriggerPreset(to, key);
    return std::nullopt;
  }
  return TriggerAction(to, *action, key.repeat, framed_map);
}

std::optional<std::string> TriggerAction(const KeyPress& to, ActionRef action_ref, bool repeat,
                                         const ro::render::Camera& framed_map) {
  const std::optional<ActionRef> action = action_ref;

  // ⚠ LES CASES DE LA BARRE D'ABORD, ET ELLES NE SONT PLUS DANS LE `switch`.
  // Elles sont quatre-vingt-seize : les énumérer pour appeler la même fonction
  // avec un index différent serait quatre-vingt-seize occasions de se tromper
  // d'index — et c'est pour cela qu'elles ont quitté l'énumération.
  if (action->is_shortcut()) {
    if (to.shortcuts != nullptr && to.link != nullptr && to.world != nullptr &&
        to.skills != nullptr) {
      // ⚠ LA TOUCHE TENUE EST TRANSMISE, et la barre décide : elle seule sait si
      // la case porte un objet — qu'on répète — ou une compétence, qu'on ne
      // répète pas.
      to.shortcuts->Cast(action->shortcut_index(), *to.skills, *to.link, *to.world, to.now_ms,
                         repeat);
    }
    return std::nullopt;
  }

  // 🔴 LES DEUX COMPTES DE MACROS SE RECOUPENT ICI, et c'est le seul endroit qui
  // voit les deux. `moonclient_hotkeys` ne lie pas `ro_ui` — il ne connaît que
  // SDL et `sim`, et c'est une propriété qu'on tient —, si bien que la table des
  // raccourcis porte son propre compte. Leur divergence serait dix touches qui
  // désignent onze lignes, ou l'inverse : une erreur de compilation, pas une
  // remarque en revue.
  static_assert(kChatMacroSlots == ro::ui::kChatMacroCount,
                "le compte de macros de la table des raccourcis et celui de la fenêtre doivent "
                "rester d'accord");

  // ── UNE MACRO DE CHAT ──────────────────────────────────────────────────────
  // Elle rend sa ligne comme la saisie rend la sienne : la boucle seule sait à
  // qui la remettre, et une macro ne doit pas suivre un autre chemin d'envoi
  // que ce qu'on tape — sinon `/commande` et `@commande` s'y comporteraient
  // autrement, et personne ne saurait dire pourquoi.
  if (action->is_macro()) {
    // 🔴 JAMAIS SUR UNE RÉPÉTITION DE TOUCHE. Alt+1 tenu une seconde enverrait
    // trente fois la même ligne — et le serveur coupe la parole à qui inonde.
    // Une case de barre se répète, une phrase non.
    if (repeat || to.macros == nullptr || action->macro_slot() >= ro::ui::kChatMacroCount) {
      return std::nullopt;
    }
    const std::array<char, ro::ui::kChatMacroSize>& row = to.macros->rows[action->macro_slot()];
    if (row[0] == '\0') {
      // ⚠ UNE MACRO VIDE NE DIT RIEN, et n'envoie pas une ligne vide : c'est
      // une touche qu'on n'a pas encore remplie, pas une erreur.
      return std::nullopt;
    }
    return std::string(row.data());
  }

  // ── UN DRAPEAU ─────────────────────────────────────────────────────────────
  // Une émoticône, et rien d'autre : ni sur une répétition, ni sans lien.
  if (action->is_flag()) {
    if (!repeat && to.link != nullptr && to.link->is_open() &&
        action->flag_slot() < kFlagEmotions.size()) {
      if (auto sent = ro::net::RequestEmotion(*to.link, kFlagEmotions[action->flag_slot()]);
          !sent) {
        ro::core::LogWarning("drapeau non émis — {}", ro::core::Describe(sent.error()));
      }
    }
    return std::nullopt;
  }

  switch (*action->named()) {
    case GameAction::kOpenChat:
      // 🔴 ENTRÉE NE DÉCIDE PAS ICI, ELLE DÉPOSE L'INTENTION. Ce qu'elle fait
      // dépend de l'état de la barre — repliée on la déplie, pleine on envoie,
      // vide on sort — et cet état-là appartient au DESSIN : en battle mode la
      // rangée n'est pas soumise du tout, et son texte n'a de sens qu'au regard
      // de ce que l'image précédente a montré. La chatbox tranche en tête
      // d'image, voir `ro/ui/chat_bar.h`.
      //
      // ⚠ SAUF SI UN ATELIER LA RÉCLAME, et qu'aucun champ n'écrit : la touche
      // est alors à lui — c'est le prix du réglage « Entrée lance… ».
      if (to.chat != nullptr && !(to.enter_claimed && !to.text_input)) {
        to.chat->open = true;
        to.chat->input.enter_pending = true;
      }
      break;
    case GameAction::kGameMenu:
      // 🔴 LA BARRE DÉPLIÉE D'ABORD, et elle AVALE la touche. Échap est la
      // sortie à une frappe du battle mode, et elle vaut que la barre ait le
      // clavier ou non — c'est ce qui l'empêche de devenir un piège une fois
      // qu'on a cliqué ailleurs. Sans cette priorité, une seule frappe
      // refermerait la barre ET ouvrirait le menu du jeu.
      if (to.chat != nullptr &&
          ro::ui::ChatBarFoldsOnEscape(to.chat->input.battle_mode, to.chat->input.unfolded)) {
        to.chat->input.escape_pending = true;
        break;
      }
      // 🔴 LE SORT ARMÉ ENSUITE, ET IL AVALE LA TOUCHE. Un curseur armé est le
      // seul état qui change ce que le CLIC SUIVANT fera : tant qu'il dure, on
      // ne peut plus ni marcher ni frapper d'un clic. C'est donc lui qu'Échap
      // doit défaire en premier, avant toute fenêtre — celles-ci n'empêchent
      // rien, elles occupent seulement de la place.
      //
      // ⚠ SANS RIEN DEMANDER D'AUTRE : désarmer et fermer une fenêtre d'une
      // seule frappe ferait disparaître deux choses pour un geste, et le joueur
      // ne saurait pas laquelle il visait.
      if (to.skills != nullptr && to.skills->armed() != nullptr) {
        to.skills->Disarm();
        break;
      }
      if (to.companion_ai != nullptr && to.companion_ai->aim().has_value()) {
        to.companion_ai->Disarm();
        break;
      }
      // ⚠ LA PILE ENSUITE, et le menu en dernier. Échap ferme d'abord ce qui est
      // ouvert ; il n'ouvre le menu que lorsqu'il ne reste rien à fermer. Quitter
      // le jeu n'est plus sur cette touche du tout : c'est un bouton du menu.
      if (!to.escape_closes_a_window && to.menu != nullptr) {
        // Échap lâche la cible avant d'ouvrir le menu, si le joueur l'a demandé.
        // Sans cible, la touche repart intacte vers le menu.
        if (!to.menu->open && to.targeting != nullptr && to.world != nullptr &&
            to.targeting->settings().escape_clears && to.targeting->ClearTarget(*to.world)) {
          break;
        }
        to.menu->open = !to.menu->open;
      }
      break;
    // 🔴 LA TOUCHE VISE UN ONGLET, PAS LA FENÊTRE. Trois règles, et chacune
    // répond à un appui qu'on fait vraiment :
    //
    //   fermée            → elle s'ouvre SUR cet onglet ;
    //   ouverte, ailleurs → elle CHANGE d'onglet, elle ne se ferme pas — c'est
    //                       un geste de navigation, et la refermer obligerait à
    //                       appuyer deux fois pour aller voir à côté ;
    //   ouverte, ici      → elle se ferme, parce qu'un second appui sur la même
    //                       touche est ce qu'on fait pour s'en débarrasser.
    case GameAction::kOpenCharacterEquipment:
      OpenSheetTab(to.sheet, ro::ui::CharacterTab::kEquipment);
      break;
    case GameAction::kOpenCharacterCostume:
      OpenSheetTab(to.sheet, ro::ui::CharacterTab::kCostume);
      break;
    case GameAction::kOpenCharacterGuild:
      OpenSheetTab(to.sheet, ro::ui::CharacterTab::kGuild);
      break;
    case GameAction::kOpenCharacterGrimoire:
      OpenSheetTab(to.sheet, ro::ui::CharacterTab::kGrimoire);
      break;
    case GameAction::kOpenCharacterHomunculus:
      if (to.homunculus_present) {
        OpenSheetTab(to.sheet, ro::ui::CharacterTab::kHomunculus);
      }
      break;
    case GameAction::kOpenCharacterMercenary:
      if (to.mercenary_present) {
        OpenSheetTab(to.sheet, ro::ui::CharacterTab::kMercenary);
      }
      break;
    case GameAction::kOpenStatus:
      // Une bascule, sans rien demander au serveur : les nombres sont déjà là.
      if (Wired(to.status, "fenêtre de statut")) {
        to.status->open = !to.status->open;
      }
      break;
    case GameAction::kOpenPetWindow:
      if (to.pet != nullptr) {
        to.pet->toggle_requested = true;
      }
      break;
    case GameAction::kOpenInventory:
      if (to.inventory != nullptr) {
        to.inventory->open = !to.inventory->open;
      }
      break;
    case GameAction::kOpenCart:
      // 🔴 SANS CHARRETTE, LA DEMANDE EST AVALÉE. On n'ouvre pas une fenêtre pour
      // y écrire qu'elle est vide : cela ferait faire deux gestes — l'ouvrir, la
      // refermer — pour une phrase d'une ligne. Elle se dit dans la conversation,
      // là où le joueur lit déjà tous les refus.
      //
      // ⚠ ET SANS CHARRETTE, ELLE NE REFERME RIEN : la fenêtre ne paraît alors
      // pas, et l'ouverture que garde le profil de disposition est celle d'un
      // autre personnage qui, lui, en pousse une.
      if (to.cart != nullptr && to.world != nullptr) {
        if (to.world->cart().capacity() > 0) {
          to.cart->open = !to.cart->open;
        } else {
          to.world->RecordChat(ro::sim::ChatKind::kServer, 0,
                               ro::core::Tr("Vous ne possédez pas de Cart."), to.now_ms);
        }
      }
      break;
    case GameAction::kOpenParty:
      OpenPartyTab(to.party, ro::ui::PartyFriendTab::kParty);
      break;
    case GameAction::kOpenFriends:
      OpenPartyTab(to.party, ro::ui::PartyFriendTab::kFriends);
      break;
    case GameAction::kOpenBank:
      // 🔴 ELLE NE BASCULE PAS UN BOOLÉEN, ELLE PARLE AU SERVEUR. La banque
      // ouverte se ferme chez nous, sans un octet ; fermée, on la DEMANDE, et
      // c'est la réponse qui ouvre. Ouvrir sans aller-retour montrerait le
      // solde de la dernière visite.
      if (to.bank != nullptr && to.link != nullptr && to.world != nullptr) {
        to.bank->ToggleSession(*to.link, *to.world, to.now_ms);
      }
      break;
    case GameAction::kOpenCardAlbum:
      // ⚠ OUVRIR DEMANDE L'ÉTAT AU SERVEUR : l'album est le sien, pas le nôtre.
      if (to.card_album != nullptr && to.card_album_window != nullptr && to.link != nullptr) {
        to.card_album->Toggle(*to.card_album_window, *to.link, to.now_ms);
      }
      break;
    case GameAction::kOpenSkillBook:
      if constexpr (ro::core::kDevTools) {
        to.skills->Toggle();
      }
      break;
    case GameAction::kOpenChatMacros:
      if (to.macros != nullptr) {
        to.macros->open = !to.macros->open;
      }
      break;
    case GameAction::kOpenStyleEditor:
      // ⚠ UNE BASCULE, comme les autres fenêtres : la même touche ouvre et
      // referme. ⛔ ET REFERMER N'ANNULE RIEN — le brouillon garde ce qui était
      // composé, et le personnage n'a de toute façon jamais changé.
      if (to.style != nullptr) {
        to.style->open = !to.style->open;
      }
      break;
    case GameAction::kOpenCashShop:
      // 🔴 ELLE PARLE AU SERVEUR : ouverte ou fermée, c'est sa réponse qui
      // bascule la boutique.
      if (to.cash_shop != nullptr && to.link != nullptr) {
        to.cash_shop->Toggle(*to.link);
      }
      break;
    case GameAction::kOpenAchievements:
      // Joué à l'image suivante, là où les succès composent leur vue.
      if (to.achievements != nullptr) {
        to.achievements->Toggle();
      }
      break;
    case GameAction::kOpenQuests:
      if (to.quests != nullptr) {
        to.quests->Toggle();
      }
      break;
    case GameAction::kOpenMemorialDungeon:
      // Joué à l'image suivante, là où l'instance compose sa vue.
      if (to.memorial != nullptr) {
        to.memorial->Toggle();
      }
      break;
    case GameAction::kOpenMarket:
      // Joué à l'image suivante, là où le Marché voit sa fenêtre.
      if (to.market != nullptr) {
        to.market->RequestToggle();
      }
      break;
    case GameAction::kOpenChatRoom:
      // Joué à l'image suivante, là où le salon a le fil en main.
      if (to.chat_rooms != nullptr) {
        to.chat_rooms->ToggleMake();
      }
      break;
    case GameAction::kOpenMail:
      // Ouvrir demande la liste au serveur ; refermer n'émet rien. Joué à
      // l'image suivante, là où le courrier a le fil en main.
      if (to.mail != nullptr) {
        to.mail->Toggle();
      }
      break;
    case GameAction::kOpenCraftAtlas:
      // ⚠ UNE BASCULE : c'est le geste d'un RACCOURCI, où la même touche fait
      // l'aller et le retour. La commande tapée, elle, ne fait qu'ouvrir.
      if (to.craft_atlas != nullptr) {
        to.craft_atlas->open = !to.craft_atlas->open;
        to.craft_atlas->raise = to.craft_atlas->open;
      }
      break;
    case GameAction::kOpenNavigation:
      if (to.navigation != nullptr) {
        to.navigation->open = !to.navigation->open;
        to.navigation->raise = to.navigation->open;
      }
      break;
    case GameAction::kOpenWorldMap:
      if (to.world_map != nullptr) {
        to.world_map->open = !to.world_map->open;
        to.world_map->raise = to.world_map->open;
      }
      break;
    case GameAction::kOpenMvpTracker:
      // ⚠ JOUÉ À L'IMAGE SUIVANTE, par l'étage : c'est lui qui sait si le
      // carnet est allumé, et éteint, la touche n'ouvre rien.
      if (to.mvp_tracker != nullptr) {
        to.mvp_tracker->RequestToggle();
      }
      break;
    case GameAction::kToggleMinimap:
      if (to.minimap_shown != nullptr) *to.minimap_shown = !*to.minimap_shown;
      break;
    case GameAction::kReportBug:
      // ⚠ LE RENONCEMENT DES OPTIONS ÉTEINT AUSSI LA TOUCHE, pas seulement les
      // boutons : qui ne signalera jamais rien n'a pas à tomber sur la boîte.
      if (to.bug_report != nullptr && to.bug_report->buttons) {
        AskBugReport(*to.bug_report, GenericBugReport());
      }
      break;
    // 🔴 LES TROIS GESTES DE LA CAPTURE, ET LEUR CIBLE RÉCLAMÉE PLUTÔT QUE
    // TESTÉE. Un pointeur qu'on oublie de brancher vaut ici une TOUCHE MUETTE, et
    // rien ne la distingue d'une touche mal liée : ni le compilateur, qui voit un
    // champ d'agrégat parfaitement valable à sa valeur par défaut, ni le joueur,
    // qui refait sa liaison trois fois avant de renoncer. C'est arrivé.
    case GameAction::kCaptureRecord:
      if (Wired(to.recorder, "capture : filmer")) {
        to.recorder->ToggleRecording(to.now_ms);
      }
      break;
    case GameAction::kCapturePicture:
      if (Wired(to.recorder, "capture : image fixe")) {
        to.recorder->AskPicture();
      }
      break;
    case GameAction::kCaptureZone:
      if (Wired(to.recorder, "capture : tracer la zone")) {
        to.recorder->AskZone();
      }
      break;
    case GameAction::kAfkScreen:
      // ⚠ ELLE N'A PAS À BASCULER : la moindre entrée met fin à la veille, et
      // la touche qui la lancerait en même temps qu'elle la termine ne ferait
      // jamais rien. Un aller simple, donc — le retour est le geste suivant,
      // quel qu'il soit.
      if (to.afk_now != nullptr) {
        *to.afk_now = true;
      }
      break;
    case GameAction::kFrameWholeMap:
      *to.camera = framed_map;
      break;
    // Le mode éteint, ces quatre-là ne font rien : c'est le prix d'un
    // interrupteur maître honnête.
    case GameAction::kTargetNearest:
      if (Wired(to.targeting, "cible la plus proche") && to.world != nullptr) {
        static_cast<void>(to.targeting->TargetNearest(*to.world, to.now_ms));
      }
      break;
    case GameAction::kTargetPrevious:
      if (Wired(to.targeting, "cible précédente") && to.world != nullptr) {
        static_cast<void>(to.targeting->CycleTarget(*to.world, /*forward=*/false, to.now_ms));
      }
      break;
    case GameAction::kTargetNext:
      if (Wired(to.targeting, "cible suivante") && to.world != nullptr) {
        static_cast<void>(to.targeting->CycleTarget(*to.world, /*forward=*/true, to.now_ms));
      }
      break;
    case GameAction::kTargetClear:
      if (Wired(to.targeting, "effacer la cible") && to.world != nullptr) {
        static_cast<void>(to.targeting->ClearTarget(*to.world));
      }
      break;
    case GameAction::kHomunculusStandby:
    case GameAction::kMercenaryStandby:
      // ⚠ PAS SUR UNE RÉPÉTITION : une bascule tenue clignoterait.
      if (!repeat && Wired(to.companion_ai, "attente du compagnon") && to.world != nullptr) {
        static_cast<void>(to.companion_ai->ToggleStandby(
            *action->named() == GameAction::kHomunculusStandby ? ro::sim::AiCompanion::kHomunculus
                                                               : ro::sim::AiCompanion::kMercenary,
            *to.world, to.now_ms));
      }
      break;
    case GameAction::kHomunculusOrder:
    case GameAction::kMercenaryOrder:
      // Un clic sur le monde, lu par la souris : aucune touche ne le porte.
      break;
    case GameAction::kCameraReset:
      if (Wired(to.rig, "caméra : remise")) {
        ResetCamera(*to.camera, to.rig->limits());
      }
      break;
    case GameAction::kCameraResetYaw:
      if (Wired(to.rig, "caméra : face au nord")) {
        static_cast<void>(ResetCameraYaw(*to.camera, to.rig->limits()));
      }
      break;
    case GameAction::kCameraResetTilt:
      if (Wired(to.rig, "caméra : inclinaison")) {
        static_cast<void>(ResetCameraTilt(*to.camera, to.rig->limits()));
      }
      break;
    case GameAction::kCameraResetZoom:
      if (Wired(to.rig, "caméra : zoom")) {
        static_cast<void>(ResetCameraZoom(*to.camera, to.rig->limits()));
      }
      break;
    case GameAction::kCameraResetTiltZoom:
      if (Wired(to.rig, "caméra : inclinaison et zoom")) {
        static_cast<void>(ResetCameraTilt(*to.camera, to.rig->limits()));
        static_cast<void>(ResetCameraZoom(*to.camera, to.rig->limits()));
      }
      break;
    // Appliqués au mouvement de la souris, dans le routage des événements.
    case GameAction::kCameraOrbit:
    case GameAction::kCameraTilt:
    case GameAction::kCameraOrbitZoom:
    case GameAction::kCameraTiltZoom:
    case GameAction::kCameraZoomWheel:
    case GameAction::kCameraTiltWheel:
      break;
    case GameAction::kSitOrStand:
      // Inser assied et relève, comme dans le jeu.
      if (to.link->is_open()) {
        if (const ro::sim::Actor* self = to.world->player(); self != nullptr) {
          ReportRefusal(ro::net::RequestSit(*to.link, self->posture != ro::sim::Posture::kSitting));
        }
      }
      break;
    case GameAction::kEffectBench:
      if constexpr (ro::core::kDevTools) {
        to.bench->Toggle();
      }
      break;
  }
  return std::nullopt;
}

}  // namespace moonclient
