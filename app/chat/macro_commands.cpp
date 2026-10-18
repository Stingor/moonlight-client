// Les valeurs d'usine des macros. Voir l'en-tête.

#include "chat/macro_commands.h"

#include <array>
#include <cstdint>

#include "ro/ui/chat_macros.h"

#include "chat/emotion_commands.h"

namespace moonclient {
namespace {

// LES ÉMOTICÔNES DONT LE CLIENT REMPLIT SES DIX LIGNES, relevées dans SA table
// de messages (`data\msgstringtable.csv` de `moonlight.grf`) :
//
//     0x220 /!    0x221 /?    0x222 /ho   0x223 /lv   0x224 /lv2  ← SAUTÉ
//     0x225 /swt  0x226 /ic   0x227 /an   0x228 /ag   0x229 /$    0x22A /...
//
// 🔴 LE RANG SAUTÉ N'EST PAS LE CINQUIÈME SPRITE, C'EST `/lv2`, qui est
// l'émoticône 14 chez nous. Déduire « dix rangs consécutifs moins le
// quatrième » donnait `/gawi` à la place de `/swt` — une commande qui n'est pas
// un défaut du client, et un défaut du client manquant. La table du client
// tranche : les dix valeurs sont simplement les dix PREMIÈRES émoticônes.
constexpr std::array<std::uint16_t, ro::ui::kChatMacroCount> kDefaultEmotions = {0, 1, 2, 3, 4,
                                                                                 5, 6, 7, 8, 9};

}  // namespace

std::string ChatMacroDefault(std::size_t slot) {
  if (slot >= kDefaultEmotions.size()) {
    return {};
  }
  return std::string(EmotionCommandOf(kDefaultEmotions[slot]));
}

}  // namespace moonclient
