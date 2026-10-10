#pragma once

#include <cstdint>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA BOÎTE DE CONFIRMATION DU JEU.
//
// ⚠ C'EST ELLE QUI PORTE LE CADRE EN NEUF MORCEAUX. Les huit `sysbox_*` de
// l'installation n'appartiennent PAS aux fenêtres — leur corps est un aplat
// clair bordé d'un filet, dessiné à partir de la palette. Ils appartiennent aux
// boîtes modales et aux panneaux de description, qui n'ont pas de barre de
// titre du tout. Les avoir posés sur les fenêtres leur donnait l'aspect d'un
// inventaire ; c'est ici qu'ils vont.
//
// ⚠ ELLE EST MODALE POUR DE BON : tant qu'elle est ouverte, rien d'autre ne
// reçoit ni clic ni touche. Une question qu'on peut ignorer en cliquant à côté
// n'est pas une question, et le geste destructeur qu'elle garde partirait quand
// même.
//
// S'emploie en deux temps, comme un popup d'ImGui — le geste OUVRE, l'image
// suivante DEMANDE :
//
//   if (SkinButton(tout_effacer, art)) { AskModal(kEffacerTout); }
//   switch (DrawModalBox(kEffacerTout, question, art)) {
//     case ModalAnswer::kYes: EffacerVraiment(); break;
//     case ModalAnswer::kNo:
//     case ModalAnswer::kWaiting: break;
//   }

enum class ModalAnswer : std::uint8_t {
  // Personne n'a répondu — la boîte est fermée, ou elle attend encore.
  kWaiting,
  kYes,
  kNo,
};

// Ouvre la boîte. Sûre à appeler pendant le dessin d'autre chose : elle ne fait
// que poser l'état que l'appel suivant consomme.
void AskModal(const char* id);

// Dessine la boîte si elle est ouverte, et rend ce que le joueur a répondu.
//
// `question`, `yes` et `no` sont DÉJÀ TRADUITS par l'appelant — même frontière
// que partout dans cette couche.
[[nodiscard]] ModalAnswer DrawModalBox(const char* id, const char* question, const char* yes,
                                       const char* no, const SkinArt& art);

// LA BOÎTE QUI SE RÉPOND « NON » TOUTE SEULE.
//
// 🔴 CE N'EST PAS UNE CONFIRMATION, C'EST UN FILET — et la différence décide de
// tout. Une confirmation demande la permission AVANT d'agir ; celle-ci arrive
// APRÈS, parce que le geste dont il s'agit peut rendre l'écran illisible. Un
// moniteur qui refuse un mode n'affiche rien du tout : la question est là, sous
// les yeux de personne.
//
// C'est donc le SILENCE qui annule, jamais le clic. Le joueur qui voit la boîte
// répond « oui » ; celui qui ne la voit pas ne fait rien, et le réglage revient
// de lui-même.
//
// ⚠ ET LE COMPTE À REBOURS N'EST PAS TENU ICI : l'appelant passe les secondes
// qui restent, parce que c'est lui qui sait défaire, et qu'il doit pouvoir le
// faire même si cette boîte n'est pas dessinée — une fenêtre refermée, un
// panneau hors champ. À zéro, la boîte rend « non » et se ferme.
//
// ⚠ ELLE S'OUVRE COMME L'AUTRE, PAR `AskModal`. Un surgissant d'ImGui ne
// paraît pas parce qu'on le dessine : sans la demande, `BeginPopupModal` rend
// faux à chaque image et la question reste invisible — donc sans réponse
// possible, donc refusée par le temps qui passe.
//
// `note` dit CE QUE LE GESTE A DÉJÀ FAIT, sous la question et au-dessus du
// compte à rebours. Une chaîne vide n'écrit rien.
[[nodiscard]] ModalAnswer DrawRevertBox(const char* id, const char* question, const char* note,
                                        const char* yes, const char* no, int seconds_left,
                                        const SkinArt& art);

// QUELQUE CHOSE DE SURGISSANT PREND-IL ÉCHAP CETTE IMAGE ?
//
// ⚠ L'ÉTAGE D'INTERFACE LE DEMANDE POUR FAIRE TAIRE LA PILE D'ÉCHAP. Sans
// cela, une seule touche fermerait la boîte ET la fenêtre qui est dessous —
// le joueur répondrait « non » à une question et perdrait au passage la
// fenêtre où il travaillait.
//
// ⚠ PAS SEULEMENT LES BOÎTES : une liste déroulante ouverte est un surgissant
// elle aussi, et Échap doit la refermer sans rien emporter d'autre.
[[nodiscard]] bool PopupTakesEscape();

// RELÈVE LES SURGISSANTS OUVERTS, une fois par image, juste après `NewFrame`.
//
// 🔴 C'EST CE RELEVÉ QUE `PopupTakesEscape` REND, et non l'état de la fin de
// l'image : la touche Échap arrive avant que rien ne soit dessiné, et le
// surgissant qu'elle ferme a disparu de la pile d'ImGui bien avant qu'on
// demande. Sans ce souvenir, une seule touche ferme le surgissant ET la fenêtre
// derrière lui.
void NotePopupsAtFrameStart();

}  // namespace ro::ui
