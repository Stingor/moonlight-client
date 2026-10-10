#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ro/sim/chat_line.h"
#include "ro/sim/whisper.h"
#include "ro/vfs/item_words.h"

namespace moonclient {

// CE QU'UNE LIGNE DE CHUCHOTEMENT MONTRE EN TÊTE, selon l'endroit où elle se
// lit. Le texte du monde ne porte que la phrase : l'en-tête se compose ici.
//
// 🔴 DEUX FORMES POUR UNE MÊME LIGNE, et ce sont celles du client d'origine.
// Le FIL dit qui et dans quel sens — « ( From Nom ) : », « ( To Nom ) : » —
// parce qu'il mêle tous les correspondants ; la CONVERSATION à part ne parle
// qu'à un seul, et elle écrit « Nom : » comme une parole ordinaire, le nôtre
// en tête de ce qu'on a dit.
//
// ⚠ LES EN-TÊTES SONT EN ANGLAIS, et ne passent pas par `Tr()` : ce sont des
// formes figées que les joueurs lisent ainsi dans toutes les langues, comme un
// mot du jeu.

enum class WhisperSurface : std::uint8_t {
  // Le fil d'un onglet, et le journal sur le disque.
  kLog,
  // La fenêtre d'une conversation 1:1.
  kConversation,
};

// L'en-tête d'une ligne, et l'endroit où le pseudo s'y trouve.
struct WhisperHead {
  std::string text;
  // Le pseudo de l'autre dans `text`, cliquable. Une longueur nulle dit
  // qu'aucun ne l'est — notre propre nom en tête de ce qu'on a dit, ou un refus
  // dont l'envoi n'a pas été retrouvé.
  std::size_t name_offset = 0;
  std::size_t name_length = 0;
};

// L'en-tête de cette ligne sur cette surface. Vide pour une ligne qui n'est pas
// un chuchotement. `own_name` est le nom du personnage joué : la conversation
// le met en tête de ce qu'on a dit.
//
// ⚠ L'IDENTIFIANT DU COMPTE N'Y EST PAS : sa forme chiffrée ne dit rien à
// personne, et l'en-tête ne porte que le nom.
[[nodiscard]] WhisperHead ComposeWhisperHead(const ro::sim::WhisperFacts& facts,
                                             WhisperSurface surface, std::string_view own_name);

// LA PHRASE D'UNE LIGNE : celle du monde, sauf pour un refus, qui n'en a pas —
// celle que le client écrit pour son code, prise dans les mots de `words`.
// Sans eux, ou pour un code inconnu, une phrase générale.
[[nodiscard]] std::string_view WhisperSaid(const ro::sim::ChatLine& line,
                                           const ro::vfs::ItemWords* words);

// La ligne entière telle que le FIL la montre : l'en-tête, puis la phrase.
// C'est elle qu'écrivent le journal et l'historique — sans en-tête, un
// chuchotement s'y relirait comme une parole publique.
[[nodiscard]] std::string WhisperLogText(const ro::sim::ChatLine& line,
                                         const ro::vfs::ItemWords* words);

// LE SENS ET LE PAIR D'UNE LIGNE RELUE dans le journal, retrouvés dans
// l'en-tête que `WhisperLogText` y a écrit : le journal ne garde que le texte.
// Une ligne sans en-tête de chuchotement reçu ou envoyé rend `kNone` — un refus
// compris, qui n'a rien dit. La relation d'un écho entre crochets se lit
// « ami ».
//
// ⚠ L'EN-TÊTE D'UN MEMBRE DE L'ÉQUIPE, « [ Nom ] : », est le seul qui
// n'annonce pas sa forme : un nom de l'équipe qui commencerait par « Friend »,
// « Member » ou « To » se lirait comme celui d'un ami, d'un membre de la
// guilde ou d'un écho, qui passent d'abord.
[[nodiscard]] ro::sim::WhisperFacts WhisperFactsOfLogText(std::string_view text);

// LES COULEURS D'UNE CONVERSATION 1:1, en `0xRRGGBB` : celles du client
// d'origine, un bleu-gris pâle pour ce qu'on reçoit et un jaune pour ce qu'on
// dit. Le refus, que le client n'y écrit pas, prend un rouge doux.
inline constexpr std::uint32_t kConversationReceivedColor = 0xB6DDE8;
inline constexpr std::uint32_t kConversationSentColor = 0xFFFF00;
inline constexpr std::uint32_t kConversationUndeliveredColor = 0xF08C78;

// La couleur d'une ligne dans une conversation.
[[nodiscard]] std::uint32_t ConversationColor(ro::sim::WhisperSide side);

}  // namespace moonclient
