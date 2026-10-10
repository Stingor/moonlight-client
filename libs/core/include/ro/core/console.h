#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace ro::core {

// La valeur d'une variable d'environnement, COPIÉE.
//
// Une copie plutôt que le pointeur rendu par getenv : ce dernier vise un tampon
// que la bibliothèque peut réutiliser à la lecture suivante, et deux lectures de
// suite rendraient donc deux fois la même valeur.
[[nodiscard]] std::optional<std::string> Environment(const char* name);

// Pose une question au terminal et lit la réponse SANS l'afficher.
//
// C'est la seule façon d'obtenir un secret sans le laisser derrière soi : un
// argument de ligne de commande se retrouve dans l'historique du shell et dans
// la liste des processus, une variable d'environnement se transmet aux enfants,
// et une saisie affichée reste à l'écran.
//
// Rend nullopt si l'entrée standard n'est pas un terminal, ou si elle est déjà
// close : un programme lancé sans console ne doit pas attendre une saisie qui ne
// viendra jamais.
[[nodiscard]] std::optional<std::string> ReadSecret(std::string_view question);

// DIT À LA CONSOLE QUE CE QU'ON LUI ÉCRIT EST DE L'UTF-8.
//
// 🔴 SANS ELLE, LE JOURNAL À L'ÉCRAN EST ILLISIBLE. Nos sources sont en UTF-8
// et nos messages sont en français ; une console de Windows, elle, décode par
// défaut dans la page OEM de la machine (437, 850…). « décor » y devient
// « d├®cor » et le tiret cadratin « ÔÇö ». Le FICHIER, lui, était juste depuis
// le début — c'est l'affichage seul qui mentait, ce qui rend le défaut
// d'autant plus facile à prendre pour un défaut d'encodage du journal.
//
// ⚠ ELLE CHANGE LA CONSOLE, PAS LE PROGRAMME, et donc elle survit à notre
// sortie quand on a été lancé depuis un terminal déjà ouvert. C'est le prix
// convenu : une console qui reste en UTF-8 gêne bien moins qu'une sortie
// qu'on ne peut pas lire.
//
// ⚠ SANS EFFET AILLEURS QUE SOUS WINDOWS, où il n'y a rien à corriger.
void UseUtf8Console();

}  // namespace ro::core
