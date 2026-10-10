#pragma once

#include <cstdint>
#include <string_view>

#include "ro/vfs/item_words.h"

namespace moonclient {

// ── LES NOMS DES TRAITS D'UNE ENTITÉ ────────────────────────────────────────
//
// Élément, race, taille, classe de monstre : les quatre familles que le serveur
// dit en INDICES, et que l'écran doit dire en mots.
//
// 🔴 UN SEUL FOYER, PARCE QU'IL Y A DÉJÀ DEUX SURFACES. La fiche de personnage
// les lit pour ses bonus conditionnels (« Résist. vs Undead ») et le HUD de
// cible pour l'espèce de ce qu'on vise. Deux aiguillages recopiés divergeraient
// au premier épisode qui ajoute une race — et le second à diverger serait celui
// que personne ne pense à relire.
//
// 🔴 LES MOTS D'UNE RACE, D'UN ÉLÉMENT ET D'UNE TAILLE SONT CEUX DE LA TABLE DE
// MESSAGES du client (`ro::vfs::MonsterRaceWord` & co.), dans la langue de
// l'installation : la plaque de cible et la fiche du monstre disent la même
// chose. Une clé que la table ne porte pas rend un texte VIDE, jamais un repli.
//
// ⚠ CE QUE LA TABLE NE NOMME PAS EST À NOUS, et passe par `core::Tr` : les
// races « joueur » et « Doram » et les jokers « tous » que le serveur range
// dans les mêmes familles. Le client d'origine n'écrit rien pour eux.
//
// ⚠ LA CLASSE DE MONSTRE (Boss, Guardian…) N'A PAS DE MOT DANS LA TABLE : elle
// reste en littéraux passant par `core::Tr`, qui veut voir sa chaîne à l'appel.

// Ce qu'on écrit d'un index dont aucune famille ne connaît le nom.
inline constexpr std::string_view kUnnamedTarget = "?";

[[nodiscard]] std::string_view ElementName(const ro::vfs::ItemWords& words, std::int16_t index);
[[nodiscard]] std::string_view RaceName(const ro::vfs::ItemWords& words, std::int16_t index);
[[nodiscard]] std::string_view SizeName(const ro::vfs::ItemWords& words, std::int16_t index);
[[nodiscard]] std::string_view MonsterClassName(std::int16_t index);

}  // namespace moonclient
