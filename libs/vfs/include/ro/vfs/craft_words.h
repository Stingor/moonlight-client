#pragma once

#include <string>
#include <string_view>

#include "ro/vfs/item_words.h"

namespace ro::vfs {

// LES MOTS DES ATELIERS : des textes de l'interface, posés par `FillCraftWords`
// (ADR 0090). Un mot qu'aucun remplissage n'a posé est vide.

// « Vous ne pouvez pas encore fabriquer d'items. »
[[nodiscard]] std::string_view CantMakeText(const ItemWords& words);
// « Liste de fabrication »
[[nodiscard]] std::string_view MakeListTitle(const ItemWords& words);
// « : matériaux requis », qui suit le nom du produit.
[[nodiscard]] std::string_view RequiredForText(const ItemWords& words);
// Le gabarit d'un résultat de fabrication, qui porte un `%s`.
[[nodiscard]] std::string_view MakeResultTemplate(const ItemWords& words, bool succeeded);

// Le gabarit d'une réponse d'affinage, dans l'ordre du serveur : réussite,
// échec, skill trop bas, minerai manquant. Vide hors de ces quatre.
[[nodiscard]] std::string_view RefineResultTemplate(const ItemWords& words, int outcome);

// 🔴 LE GABARIT EST UNE DONNÉE, PAS UN FORMAT : il vient d'un catalogue de
// langue, et le passer à `snprintf` confierait la pile à qui l'a écrit. Le
// PREMIER `%s` est remplacé à la main ; faux s'il n'y en a pas, et `into` n'est
// alors pas touché.
[[nodiscard]] bool FillFirstString(std::string_view pattern, std::string_view value,
                                   std::string* into);

// Pose les mots des ateliers dans la langue courante.
void FillCraftWords(CraftWords& words);

}  // namespace ro::vfs
