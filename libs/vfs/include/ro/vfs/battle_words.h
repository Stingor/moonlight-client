#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "ro/vfs/item_words.h"

namespace ro::vfs {

// ── LES LIGNES DE COMBAT DE LA CONVERSATION ──────────────────────────────────
//
// Le client écrit une ligne quand un coup me touche ou touche un membre
// connecté de mon groupe, et quand je porte un coup ou qu'un membre en porte
// un. Les phrases sont des textes de l'interface, posés par `FillBattleWords`
// (ADR 0090) : quand un gabarit n'a pas la forme attendue, la ligne est VIDE et
// l'appelant ne l'écrit pas.

enum class BattleLine : std::uint8_t {
  // « [attaquant] inflicts 'N' damage points on you. »
  kIReceived,
  // « [membre] attacked by [attaquant] with 'N' damage points. »
  kPartyReceived,
  // « [cible] receives 'N' damage points. »
  kIGave,
  // « [membre] attacks [cible] with 'N' damage points. »
  kPartyGave,
};

// Le nom de qui n'en a pas.
[[nodiscard]] std::string_view SomeoneText(const ItemWords& words);

// La ligne composée, ou une chaîne vide quand le gabarit manque ou n'a pas la
// forme attendue. `first` et `second` sont les noms dans l'ordre où la phrase
// les écrit ; `second` n'est lu que par les deux lignes du groupe.
[[nodiscard]] std::string BattleLineText(const ItemWords& words, BattleLine line,
                                         std::string_view first, std::string_view second,
                                         int damage);

// Vrai si un gabarit PRINTF porte exactement ces conversions, dans cet ordre :
// `s` pour une chaîne, `d` pour un entier (`%d` ou `%i`), et rien d'autre que
// des `%%`. C'est le crible qui rend sûr d'imprimer un gabarit venu d'un
// catalogue de langue.
[[nodiscard]] bool FormatTakesExactly(std::string_view format, std::string_view conversions);

// Pose ces phrases dans la langue courante.
void FillBattleWords(BattleWords& words);

}  // namespace ro::vfs
