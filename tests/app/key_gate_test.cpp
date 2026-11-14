// QUI RECOIT LA TOUCHE quand l'interface a le clavier.

#include "input/key_gate.h"

#include <array>

#include <catch2/catch_test_macros.hpp>

using moonclient::AltGrMacroKey;
using moonclient::ChatBarKeepsKey;
using moonclient::FunctionKey;
using moonclient::GameOnlyKey;
using moonclient::KeyGoesToGame;
using moonclient::NpcDialogKeepsKey;
using moonclient::PresetKeyLive;

namespace {

// Les defauts du client : ce sont EUX que la barre de conversation avalait.
constexpr SDL_Keymod kNothingHeld = SDL_KMOD_NONE;

}  // namespace

TEST_CASE("Clavier : la saisie ne prend que ce qu'elle peut ECRIRE", "[app][clavier]") {
  // 🔴 LES TROIS FAMILLES QUI REVIENNENT AU JEU. Un champ de texte ne sait rien
  // en faire, et les lui laisser eteignait le client entier tant qu'il avait le
  // focus.
  CHECK(GameOnlyKey(SDLK_F1, kNothingHeld));
  CHECK(GameOnlyKey(SDLK_F9, kNothingHeld));
  CHECK(GameOnlyKey(SDLK_F12, kNothingHeld));
  // INSER assied et releve, et c'est assis qu'on discute.
  CHECK(GameOnlyKey(SDLK_INSERT, kNothingHeld));
  // Les raccourcis de fenetres du jeu : le sac, la charrette, la fiche.
  CHECK(GameOnlyKey(SDLK_E, SDL_KMOD_LALT));
  CHECK(GameOnlyKey(SDLK_W, SDL_KMOD_LALT));
  // Les macros de conversation, Alt+0 a Alt+9.
  CHECK(GameOnlyKey(SDLK_0, SDL_KMOD_LALT));
}

TEST_CASE("Clavier : ce qu'une saisie utilise lui reste", "[app][clavier]") {
  // 🔴 LE CRITERE : est-ce qu'un champ de texte peut s'en servir ? Ces
  // touches-la servent toutes a ECRIRE ou a EDITER, et les rendre au jeu
  // rendrait le champ inutilisable.
  for (const SDL_Keycode key :
       {SDLK_A, SDLK_S, SDLK_R, SDLK_N, SDLK_SPACE, SDLK_RETURN, SDLK_ESCAPE, SDLK_BACKSPACE,
        SDLK_DELETE, SDLK_TAB, SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN, SDLK_HOME, SDLK_END,
        SDLK_PAGEUP, SDLK_PAGEDOWN, SDLK_LEFTBRACKET}) {
    CHECK_FALSE(GameOnlyKey(key, kNothingHeld));
  }
  // CTRL EST A LA SAISIE : copier, coller, tout selectionner.
  CHECK_FALSE(GameOnlyKey(SDLK_C, SDL_KMOD_LCTRL));
  CHECK_FALSE(GameOnlyKey(SDLK_A, SDL_KMOD_LCTRL));
}

TEST_CASE("Clavier : ALTGR ECRIT, il ne declenche rien", "[app][clavier]") {
  // 🔴 SUR UN CLAVIER FRANCAIS, ALTGR VAUT CTRL+ALT. C'est ce qui ecrit @ # [ ]
  // { } \ | € -- de la SAISIE, et non les raccourcis du jeu. Sans le test de
  // Ctrl, taper une arobase dans un champ aurait aussi ouvert une fenetre.
  CHECK_FALSE(GameOnlyKey(SDLK_0, static_cast<SDL_Keymod>(SDL_KMOD_RALT | SDL_KMOD_LCTRL)));
  CHECK_FALSE(GameOnlyKey(SDLK_E, static_cast<SDL_Keymod>(SDL_KMOD_RALT | SDL_KMOD_RCTRL)));
  // 🔴 SOUS WINDOWS, SDL JETTE LE FAUX CTRL : AltGr arrive en Alt droit SEUL.
  // C'est la forme que prend l'arobase en vrai, et elle lancait la macro Alt+0.
  CHECK_FALSE(GameOnlyKey(SDLK_0, SDL_KMOD_RALT));
  CHECK_FALSE(GameOnlyKey(SDLK_E, SDL_KMOD_RALT));
  // Sous X11, AltGr est la touche MODE.
  CHECK_FALSE(GameOnlyKey(SDLK_0, static_cast<SDL_Keymod>(SDL_KMOD_LALT | SDL_KMOD_MODE)));
  // Tenu avec Alt gauche, il reste de la saisie.
  CHECK_FALSE(GameOnlyKey(SDLK_0, static_cast<SDL_Keymod>(SDL_KMOD_LALT | SDL_KMOD_RALT)));

  // Toute saisie suit la regle, la barre depliee du battle mode comprise.
  CHECK_FALSE(KeyGoesToGame(
      {.interface_has_keyboard = true, .typing = true, .chat_bar_takes_typing = false}, SDLK_0,
      SDL_KMOD_RALT));
  CHECK_FALSE(KeyGoesToGame(
      {.interface_has_keyboard = false, .typing = false, .chat_bar_takes_typing = true}, SDLK_0,
      SDL_KMOD_RALT));
}

TEST_CASE("Clavier : ALTGR lance la macro depuis la conversation VIDE", "[app][clavier]") {
  constexpr moonclient::KeyClaims kEmptyChatTyping{
      .interface_has_keyboard = true, .typing = true, .empty_chat_holds_macro = true};
  constexpr moonclient::KeyClaims kEmptyChatUnfolded{.chat_bar_takes_typing = true,
                                                     .empty_chat_holds_macro = true};
  CHECK(KeyGoesToGame(kEmptyChatTyping, SDLK_1, SDL_KMOD_RALT));
  CHECK(KeyGoesToGame(kEmptyChatTyping, SDLK_9, SDL_KMOD_RALT));
  CHECK(KeyGoesToGame(kEmptyChatUnfolded, SDLK_1, SDL_KMOD_RALT));

  // 🔴 L'AROBASE RESTE ECRITE : une commande commence par elle, dans un champ vide.
  CHECK_FALSE(KeyGoesToGame(kEmptyChatTyping, SDLK_0, SDL_KMOD_RALT));
  CHECK_FALSE(KeyGoesToGame(kEmptyChatUnfolded, SDLK_0, SDL_KMOD_RALT));

  // Une phrase commencee, une case vide ou un autre champ : Alt Gr ecrit.
  CHECK_FALSE(
      KeyGoesToGame({.interface_has_keyboard = true, .typing = true}, SDLK_1, SDL_KMOD_RALT));

  // ⚠ UNE MODALE GARDE TOUT.
  CHECK_FALSE(KeyGoesToGame({.interface_has_keyboard = true, .empty_chat_holds_macro = true},
                            SDLK_1, SDL_KMOD_RALT));

  // ⚠ ALT DROIT SEUL : ses autres formes ne sont pas lues comme Alt par la table.
  CHECK_FALSE(AltGrMacroKey(SDLK_1, static_cast<SDL_Keymod>(SDL_KMOD_RALT | SDL_KMOD_LCTRL)));
  CHECK_FALSE(AltGrMacroKey(SDLK_1, static_cast<SDL_Keymod>(SDL_KMOD_RALT | SDL_KMOD_LSHIFT)));
  CHECK_FALSE(AltGrMacroKey(SDLK_1, static_cast<SDL_Keymod>(SDL_KMOD_RALT | SDL_KMOD_LALT)));
  CHECK_FALSE(AltGrMacroKey(SDLK_1, SDL_KMOD_LALT));
  CHECK_FALSE(AltGrMacroKey(SDLK_RALT, SDL_KMOD_RALT));
  // Verr. Num. et Verr. Maj. ne changent rien au geste.
  CHECK(AltGrMacroKey(SDLK_1, static_cast<SDL_Keymod>(SDL_KMOD_RALT | SDL_KMOD_NUM)));
  CHECK(AltGrMacroKey(SDLK_1, static_cast<SDL_Keymod>(SDL_KMOD_RALT | SDL_KMOD_CAPS)));
}

TEST_CASE("Clavier : tenir ALT ne reveille pas le jeu a lui seul", "[app][clavier]") {
  // Tenir Alt pour faire « Alt+E » ne doit pas, a lui seul, passer pour un
  // raccourci : sinon la premiere moitie du geste declenche quelque chose.
  CHECK_FALSE(GameOnlyKey(SDLK_LALT, SDL_KMOD_LALT));
  CHECK_FALSE(GameOnlyKey(SDLK_RALT, SDL_KMOD_RALT));
  CHECK_FALSE(GameOnlyKey(SDLK_LCTRL, SDL_KMOD_LCTRL));
}

TEST_CASE("Clavier : deux avalages, et pas un seul", "[app][clavier]") {
  // L'interface n'a pas le clavier : tout passe, y compris ce qu'elle aurait
  // pris.
  CHECK(KeyGoesToGame(
      {.interface_has_keyboard = false, .typing = false, .chat_bar_takes_typing = false}, SDLK_A,
      kNothingHeld));

  // 🔴 UNE SAISIE : elle garde ce qu'elle ecrit, elle rend le reste. C'est le
  // defaut signale -- la barre de conversation avalait F1 a F9 et Alt+E.
  CHECK_FALSE(KeyGoesToGame(
      {.interface_has_keyboard = true, .typing = true, .chat_bar_takes_typing = false}, SDLK_A,
      kNothingHeld));
  CHECK(KeyGoesToGame(
      {.interface_has_keyboard = true, .typing = true, .chat_bar_takes_typing = false}, SDLK_F1,
      kNothingHeld));
  CHECK(KeyGoesToGame(
      {.interface_has_keyboard = true, .typing = true, .chat_bar_takes_typing = false}, SDLK_E,
      SDL_KMOD_LALT));
  CHECK(KeyGoesToGame(
      {.interface_has_keyboard = true, .typing = true, .chat_bar_takes_typing = false}, SDLK_INSERT,
      kNothingHeld));

  // 🔴 UNE PRISE TOTALE -- une modale -- GARDE TOUT : le joueur doit lui
  // repondre avant quoi que ce soit d'autre.
  CHECK_FALSE(KeyGoesToGame(
      {.interface_has_keyboard = true, .typing = false, .chat_bar_takes_typing = false}, SDLK_F1,
      kNothingHeld));
  CHECK_FALSE(KeyGoesToGame(
      {.interface_has_keyboard = true, .typing = false, .chat_bar_takes_typing = false}, SDLK_E,
      SDL_KMOD_LALT));
}

TEST_CASE("Clavier : la barre depliee du battle mode prend les frappes", "[app][clavier]") {
  // 🔴 L'ETAT INTERMEDIAIRE : la barre est la, le joueur l'a ouverte expres,
  // et personne n'ecrit -- il a clique ailleurs. Une lettre doit s'ecrire
  // dedans, pas faire marcher le personnage.
  CHECK(ChatBarKeepsKey(SDLK_A, kNothingHeld));
  CHECK_FALSE(KeyGoesToGame(
      {.interface_has_keyboard = false, .typing = false, .chat_bar_takes_typing = true}, SDLK_A,
      kNothingHeld));

  // 🔴 SES TROIS TOUCHES LUI REVIENNENT PAR LE JEU, et il le faut : ce sont
  // elles qui l'ouvrent, envoient et referment, et c'est `HandleKey` qui les
  // lui transmet. Les avaler ici la rendrait sourde a ses propres commandes.
  for (const SDL_Keycode key : {SDLK_RETURN, SDLK_KP_ENTER, SDLK_ESCAPE}) {
    CHECK(KeyGoesToGame(
        {.interface_has_keyboard = false, .typing = false, .chat_bar_takes_typing = true}, key,
        kNothingHeld));
  }
  // Et ce que le jeu garde MEME pendant une saisie continue de passer : la
  // barre depliee n'eteint pas les raccourcis.
  CHECK(KeyGoesToGame(
      {.interface_has_keyboard = false, .typing = false, .chat_bar_takes_typing = true}, SDLK_F1,
      kNothingHeld));
  CHECK(KeyGoesToGame(
      {.interface_has_keyboard = false, .typing = false, .chat_bar_takes_typing = true}, SDLK_E,
      SDL_KMOD_LALT));
  CHECK(KeyGoesToGame(
      {.interface_has_keyboard = false, .typing = false, .chat_bar_takes_typing = true},
      SDLK_INSERT, kNothingHeld));
}

TEST_CASE("Clavier : le dialogue d'un PNJ garde ce qu'il LIT", "[app][clavier][npc]") {
  // 🔴 ENTREE EST LA MEME TOUCHE QUE CELLE QUI OUVRE LA CONVERSATION. Sans cette
  // regle, valider « Suivant » au clavier depliait la barre de chat par-dessus la
  // page, et la frappe suivante partait dans une phrase au lieu de repondre.
  CHECK(NpcDialogKeepsKey(/*menu_open=*/false, SDLK_RETURN, kNothingHeld));
  CHECK(NpcDialogKeepsKey(/*menu_open=*/false, SDLK_KP_ENTER, kNothingHeld));
  // Espace suit Entree : le pied de fenetre lit les deux.
  CHECK(NpcDialogKeepsKey(/*menu_open=*/false, SDLK_SPACE, kNothingHeld));

  // 🔴 LES FLECHES NE VALENT QUE SOUS UN MENU, ou elles deplacent la visee. Hors
  // menu la fenetre n'en fait rien, et les retenir priverait le joueur de ce
  // qu'il y a lie.
  CHECK_FALSE(NpcDialogKeepsKey(/*menu_open=*/false, SDLK_UP, kNothingHeld));
  CHECK_FALSE(NpcDialogKeepsKey(/*menu_open=*/false, SDLK_DOWN, kNothingHeld));
  CHECK(NpcDialogKeepsKey(/*menu_open=*/true, SDLK_UP, kNothingHeld));
  CHECK(NpcDialogKeepsKey(/*menu_open=*/true, SDLK_DOWN, kNothingHeld));
  // Gauche et droite, jamais : la liste est verticale.
  CHECK_FALSE(NpcDialogKeepsKey(/*menu_open=*/true, SDLK_LEFT, kNothingHeld));

  // ⚠ ECHAP N'EST PAS DE LA LISTE : il passe par la pile d'echappement, qui ne
  // ferme le dialogue que la ou fermer a un sens pour le serveur.
  CHECK_FALSE(NpcDialogKeepsKey(/*menu_open=*/true, SDLK_ESCAPE, kNothingHeld));

  // ⚠ UN RACCOURCI DU JEU PASSE : la barre de competences et les fenetres
  // restent utilisables pendant qu'un PNJ parle, comme en natif.
  CHECK_FALSE(NpcDialogKeepsKey(/*menu_open=*/true, SDLK_RETURN, SDL_KMOD_LALT));
  CHECK_FALSE(NpcDialogKeepsKey(/*menu_open=*/true, SDLK_UP, SDL_KMOD_LCTRL));
  CHECK_FALSE(NpcDialogKeepsKey(/*menu_open=*/false, SDLK_SPACE, SDL_KMOD_LSHIFT));
}

TEST_CASE("Clavier : le dialogue passe AVANT la barre depliee", "[app][clavier][npc]") {
  // 🔴 L'ORDRE EST TOUT CE QUI LES DEPARTAGE : les deux reclament sans avoir le
  // clavier, et Entree les interesse tous les deux. Si la barre passait la
  // premiere, repondre au PNJ ouvrirait le chat.
  CHECK_FALSE(KeyGoesToGame({.chat_bar_takes_typing = true, .npc_dialog_open = true}, SDLK_RETURN,
                            kNothingHeld));
  // Et sans dialogue, Entree lui revient : c'est elle qui ouvre et envoie.
  CHECK(KeyGoesToGame({.chat_bar_takes_typing = true}, SDLK_RETURN, kNothingHeld));

  // Le dialogue n'eteint pas le jeu pour autant : une touche qu'il ne lit pas
  // part comme avant.
  CHECK(KeyGoesToGame({.npc_dialog_open = true}, SDLK_A, kNothingHeld));
  CHECK(KeyGoesToGame({.npc_dialog_open = true, .npc_menu_open = true}, SDLK_F1, kNothingHeld));
}

TEST_CASE("Clavier : F1 a F12 traversent la saisie, modificateurs compris", "[app][clavier]") {
  // 🔴 LES DOUZE TOUCHES, ET AVEC CE QU'ON TIENT : Ctrl+F1, Maj+F5 ou Alt+F12
  // sont des raccourcis du joueur au meme titre que F1 seule. La barre de
  // discussion qui a le clavier n'en garde aucune.
  constexpr moonclient::KeyClaims kChatTyping{.interface_has_keyboard = true, .typing = true};
  constexpr moonclient::KeyClaims kChatUnfolded{.chat_bar_takes_typing = true};
  for (SDL_Keycode key = SDLK_F1; key <= SDLK_F12; ++key) {
    CHECK(FunctionKey(key));
    for (const SDL_Keymod held : std::to_array<SDL_Keymod>(
             {kNothingHeld, SDL_KMOD_LCTRL, SDL_KMOD_LSHIFT, SDL_KMOD_LALT, SDL_KMOD_RCTRL})) {
      CHECK(KeyGoesToGame(kChatTyping, key, held));
      CHECK(KeyGoesToGame(kChatUnfolded, key, held));
    }
  }
  // Les voisines restent a la saisie : ce ne sont pas des touches de fonction.
  CHECK_FALSE(FunctionKey(SDLK_F13));
  CHECK_FALSE(FunctionKey(SDLK_ESCAPE));
  CHECK_FALSE(KeyGoesToGame(kChatTyping, SDLK_1, kNothingHeld));
  CHECK_FALSE(KeyGoesToGame(kChatTyping, SDLK_A, kNothingHeld));
}

TEST_CASE("Clavier : un preset lie a F1-F12 se remet pendant une saisie", "[app][clavier]") {
  // 🔴 TOUT CE QUE LE JOUEUR A LIE A UNE TOUCHE DE FONCTION REPOND, la barre de
  // discussion focalisee ou non.
  CHECK(PresetKeyLive(/*text_input=*/true, SDLK_F1));
  CHECK(PresetKeyLive(/*text_input=*/true, SDLK_F12));
  // ⚠ LES AUTRES TOUCHES D'UN PRESET ATTENDENT LA FIN DE LA PHRASE : Alt et Inser
  // traversent la saisie pour la table, pas pour changer d'equipement.
  CHECK_FALSE(PresetKeyLive(/*text_input=*/true, SDLK_E));
  CHECK_FALSE(PresetKeyLive(/*text_input=*/true, SDLK_INSERT));
  // Hors saisie, toute touche de preset repond.
  CHECK(PresetKeyLive(/*text_input=*/false, SDLK_E));
  CHECK(PresetKeyLive(/*text_input=*/false, SDLK_F3));
}
