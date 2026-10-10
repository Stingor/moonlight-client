#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "ro/vfs/item_words.h"

namespace ro::vfs {

// ── LES MOTS DE LA FICHE D'UN MONSTRE ────────────────────────────────────────
//
// Ceux de la fenêtre du jeu — « Demi-Human », « Shadow » en anglais —, devenus
// des textes de l'interface, posés par `FillMonsterWords` (ADR 0090). Une
// valeur hors bornes rend « ? », comme la fiche de référence.

// La race du serveur, 0 à 9 : « Formless », « Undead », « Brute »…
[[nodiscard]] std::string_view MonsterRaceWord(const ItemWords& words, std::uint8_t race);

// L'élément du serveur, 0 à 9. ⚠ INDEXÉ MODULO VINGT, comme le client : c'est
// le reste du codage « élément + 20 × niveau ». Le serveur n'envoie que 0 à 9,
// l'opération est donc neutre.
[[nodiscard]] std::string_view MonsterElementWord(const ItemWords& words, std::uint8_t element);

// La taille du serveur, 0 à 2 : « Small », « Medium », « Large ».
[[nodiscard]] std::string_view MonsterSizeWord(const ItemWords& words, std::uint8_t size);

// Les libellés des lignes de la fiche.
enum class MonsterLabel : std::uint8_t { kLevel, kRace, kElement, kSize, kDef, kMdef };

[[nodiscard]] std::string_view MonsterLabelWord(const ItemWords& words, MonsterLabel label);

// Pose ces mots dans la langue courante.
void FillMonsterWords(MonsterWords& words);

}  // namespace ro::vfs
