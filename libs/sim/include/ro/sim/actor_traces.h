#pragma once

#include "ro/sim/actor.h"

namespace ro::sim {

// CE QU'UN ACTEUR OUBLIE EN CHANGEANT DE CARTE.
//
// 🔴 LE MONDE DÉTRUIT TOUS SES ACTEURS À L'ARRIVÉE — SAUF LE JOUEUR, qu'il
// transporte parce que le serveur ne réannonce jamais son identité. Et un acteur
// transporté garde ses TRACES : sa dernière incantation, le dernier sort qu'il a
// posé au sol, le dernier coup qu'il a porté.
//
// 🔴 CE SONT DES ÉVÉNEMENTS PASSÉS, ET C'EST LA VUE QUI DÉCIDE DE LES MONTRER.
// Elle le fait en se souvenant de ce qu'elle a déjà montré — et ce souvenir-là
// part au changement de carte (`EffectsInPlay::Forget`). Les deux mémoires
// doivent donc partir ENSEMBLE : n'en effacer qu'une fait rejouer, à chaque
// `@load`, le dernier sort du joueur — son anneau d'incantation sur la case
// visée, son bruit d'incantation, et le bruit du sort lui-même. Vu en jeu.
//
// ⚠ ET LE CLIENT NATIF N'A PAS CE PROBLÈME parce qu'il n'a pas ce choix : il
// détruit son monde entier, joueur compris, et le rebâtit de ce que le serveur
// réannonce. Notre transport du joueur est un écart assumé — il doit donc porter
// lui-même l'oubli que la destruction donnait gratuitement.
//
// ⚠ CE QUI RESTE, ET POURQUOI : la place, l'apparence, la vie, les états et les
// effets de chapeau. Ce sont des ÉTATS, pas des événements ; le serveur ne les
// redit pas toujours, et les effacer ferait clignoter un personnage qui a
// simplement retraversé un portail.
void ForgetLastMapTraces(Actor& actor);

}  // namespace ro::sim
