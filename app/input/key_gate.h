#pragma once

#include <SDL3/SDL.h>

namespace moonclient {

// ── QUI REÇOIT LA TOUCHE : LE JEU, OU LA SAISIE ? ───────────────────────────
//
// 🔴 UNE SAISIE NE RÉCLAME QUE LES TOUCHES QUI ÉCRIVENT. Tout rendre à
// l'interface dès qu'elle a le clavier éteint le jeu entier : plus de F1 à F9,
// plus d'Alt+E, plus d'Inser tant que la barre de conversation a le focus. Le
// joueur le lit « la chatbox bloque mon clavier », et ne devine pas qu'il doit
// cliquer ailleurs pour le récupérer. Le client d'origine ne fait pas cela.
//
// ⚠ ET IMGUI DIT « J'AI LE CLAVIER » BIEN PLUS SOUVENT QU'IL N'EN A BESOIN :
// il lève sa capture dès qu'un élément est ACTIF — un bouton tenu, une poignée
// tirée, un objet qu'on GLISSE. Aucun de ces gestes n'attend de touche. C'est
// pourquoi la boucle ne décide pas sur cette capture seule.

// Ce qu'une saisie de texte ne peut PAS utiliser, et qui revient donc au jeu
// même quand un champ a le clavier :
//
//   - LES TOUCHES DE FONCTION, qui sont les cases de la barre de raccourcis et
//     ne veulent rien dire dans un champ de texte ;
//   - INSER, qui assied et relève. L'interface n'a pas de mode « refrappe », la
//     touche ne lui sert donc à rien — et l'avaler empêcherait de s'asseoir
//     tant que la conversation a le clavier, c'est-à-dire précisément quand on
//     discute assis ;
//   - LES COMBINAISONS AVEC ALT GAUCHE, qui sont les raccourcis de fenêtres et
//     les macros de conversation du jeu.
//
// 🔴 ALTGR EST DE LA SAISIE, PAS UN ALT : sur un clavier français il écrit
// @ # [ ] { } \ | €, et l'arobase (AltGr+à) se lirait Alt+0 — une macro de
// conversation partirait au milieu de la phrase. AltGr arrive sous trois
// formes, et aucune ne compte comme Alt :
//   - Alt DROIT seul : SDL, sous Windows, jette le faux Ctrl gauche qu'AltGr
//     envoie avant lui ;
//   - Ctrl+Alt, quand ce faux Ctrl passe quand même ;
//   - MODE, la touche de sélection de niveau des claviers sous X11.
// Le prix : sur un clavier sans AltGr, Alt droit ne sert de raccourci qu'hors
// saisie. Alt gauche reste la touche des raccourcis partout.
//
// Ctrl seul reste avalé de toute façon — c'est copier, coller, tout
// sélectionner.
//
// 🔴 LE CRITÈRE, POUR LA PROCHAINE TOUCHE QU'ON VOUDRA AJOUTER : est-ce qu'une
// saisie de texte peut s'en servir ? Suppr, Origine, Fin, les flèches, Page
// haut et bas, Tab, Entrée et Échap servent tous à l'édition. Les rendre au jeu
// rendrait le champ inutilisable.
[[nodiscard]] bool GameOnlyKey(SDL_Keycode key, SDL_Keymod held);

// UNE TOUCHE DE FONCTION, de F1 à F12, quels que soient les modificateurs tenus.
[[nodiscard]] bool FunctionKey(SDL_Keycode key);

// UNE TOUCHE QUI NE SERT QU'À EN MODIFIER UNE AUTRE — Ctrl, Maj, Alt, la touche
// système — n'est pas un raccourci : tenir Alt pour faire « Alt+E » ne doit
// pas, à lui seul, déclencher quoi que ce soit.
[[nodiscard]] bool IsModifierKey(SDL_Keycode key);

// UN PRESET D'ÉQUIPEMENT SE REMET-IL PENDANT UNE SAISIE ?
//
// 🔴 SUR UNE TOUCHE DE FONCTION, OUI : F1 à F12 appartiennent au jeu même quand
// la barre de discussion a le clavier, et c'est vrai de TOUT ce que le joueur y
// a lié — une case de barre comme un preset. Une touche qui agit ou non selon
// l'endroit où l'on a cliqué en dernier se lit « mon raccourci est cassé ».
//
// ⚠ ALT ET INSER, NON : leur passage sert les raccourcis de fenêtres de la
// table, pas un changement d'équipement au milieu d'une phrase.
[[nodiscard]] bool PresetKeyLive(bool text_input, SDL_Keycode key);

// LA BARRE DE CONVERSATION RÉCLAME LES FRAPPES SANS AVOIR LE CLAVIER.
//
// 🔴 C'EST L'ÉTAT INTERMÉDIAIRE DU BATTLE MODE : la barre est dépliée, le
// joueur l'a ouverte exprès, mais personne n'écrit — il a cliqué ailleurs, ou
// le champ n'a pas encore pris le focus. Une lettre tapée là doit s'écrire dans
// la barre, pas déplacer le personnage. Voir `ro/ui/chat_bar.h`, où vit la
// règle, et pourquoi ImGui interdit de garder le clavier en permanence.
//
// ⚠ ENTRÉE ET ÉCHAP CONTINUENT D'ALLER AU JEU, et c'est nécessaire : ce sont
// elles qui ouvrent, envoient et referment la barre, et c'est `HandleKey` qui
// les lui transmet. Les avaler ici la rendrait sourde à ses propres commandes.
[[nodiscard]] bool ChatBarKeepsKey(SDL_Keycode key, SDL_Keymod held);

// LE DIALOGUE D'UN PNJ GARDE CE QU'IL LIT, et il le garde sans avoir le
// clavier.
//
// 🔴 ENTRÉE EST À LUI, ET C'EST LA MÊME TOUCHE QUI OUVRE LA CONVERSATION.
// Sinon, valider « Suivant » au clavier déplierait la barre de chat par-dessus
// la page : la frappe suivante s'écrirait dans une phrase au lieu de répondre
// au PNJ.
// Espace suit Entrée — le pied de fenêtre lit les deux —, et les flèches ne
// valent que sous un MENU, où elles déplacent la visée ; hors menu le dialogue
// n'en fait rien, et les retenir priverait le joueur de ce qu'il y a lié.
//
// ⚠ LA RÈGLE EST « CE QU'IL LIT, IL LE GARDE », et c'est ce qui dit quoi
// ajouter ici le jour où la fenêtre lira une touche de plus. Une touche qu'elle
// lit sans la retenir agit DEUX fois : une dans la fenêtre, une dans le jeu.
//
// ⚠ ÉCHAP N'EST PAS DE LA LISTE. Il passe par la pile d'échappement, qui ne
// ferme le dialogue que là où fermer a un sens pour le serveur ; pendant un
// « suivant », la touche doit filer au jeu et y ouvrir le menu.
//
// ⚠ ET SEULEMENT SANS MODIFICATEUR : Ctrl, Alt ou Maj + une touche est un
// raccourci du jeu, que le dialogue n'a jamais réclamé. La barre de compétences
// et les fenêtres restent utilisables pendant qu'un PNJ parle, comme en natif.
[[nodiscard]] bool NpcDialogKeepsKey(bool menu_open, SDL_Keycode key, SDL_Keymod held);

// CE QUI RÉCLAME LE CLAVIER À CET INSTANT.
//
// ⚠ CHAQUE CHAMP NOMMÉ, et non cinq booléens à la file : deux voisins de même
// type y échangent leur place sans qu'aucun compilateur ne s'en aperçoive.
struct KeyClaims {
  // La capture d'ImGui, pour quelque raison que ce soit.
  bool interface_has_keyboard = false;
  // Vrai quand cette prise est celle d'une SAISIE — ou d'un élément qu'on
  // tient, qu'ImGui compte pareil. Une prise TOTALE, elle, garde tout : une
  // modale attend qu'on lui réponde avant quoi que ce soit d'autre.
  bool typing = false;
  // L'état intermédiaire de la barre de conversation : dépliée, sans le
  // clavier.
  bool chat_bar_takes_typing = false;
  // Un PNJ parle, et sa fenêtre est à l'écran.
  bool npc_dialog_open = false;
  // Cette fenêtre montre une liste de choix.
  bool npc_menu_open = false;
};

// LE VERDICT DE LA BOUCLE, en une ligne : la touche part-elle au jeu ?
//
// 🔴 UN SEUL VERDICT, ET C'EST VOULU. Chaque prétendant testé séparément dans
// la boucle ferait que le suivant s'ajouterait sans qu'on voie plus dans quel
// ORDRE ils passent — or l'ordre est tout ce qui les départage.
[[nodiscard]] bool KeyGoesToGame(const KeyClaims& claims, SDL_Keycode key, SDL_Keymod held);

}  // namespace moonclient
