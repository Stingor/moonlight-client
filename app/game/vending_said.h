#pragma once

// CE QUE L'ÉCHOPPE DE VENTE DIT AU JOUEUR : les phrases du jeu pour chaque
// réponse du serveur, et les nôtres pour ce que le client refuse avant l'envoi.
//
// ⚠ PURES : elles ne lisent que ce qu'on leur donne, et s'éprouvent sur un banc.
// La correspondance code → mot vient de `docs/commerces-messages.md`.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/sim/vending.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/trade_words.h"

namespace moonclient {

// Les couleurs des lignes de conversation, celles du client (0xRRGGBB).
inline constexpr std::uint32_t kVendingFailedColor = 0xFF0000;
inline constexpr std::uint32_t kVendingLightRedColor = 0xFF6464;
inline constexpr std::uint32_t kVendingSoldColor = 0x64FFFF;
inline constexpr std::uint32_t kVendingSoldOutColor = 0x00FF00;

// UNE LIGNE À ÉCRIRE : le texte, déjà imprimé, et sa couleur. Un texte vide ne
// s'écrit pas — la table de l'installation n'a pas la clé.
struct VendingChatLine {
  std::string text;
  std::uint32_t color = kVendingFailedColor;
};

// LE REFUS D'UN ACHAT (`0x0135`). `item_name` est le nom COMPOSÉ de l'offre à
// la case renvoyée ; ⚠ VIDE, c'est-à-dire l'offre introuvable, le client se
// tait pour les codes qui le nomment. `amount` est la quantité du paquet.
[[nodiscard]] VendingChatLine SaidOfPurchaseRefusal(const ro::vfs::ItemWords& words,
                                                    std::uint8_t result, std::string_view item_name,
                                                    std::int64_t amount);

// CE QUE DIT LE RÉSULTAT D'UNE OUVERTURE (`0x0A28`) : une boîte, une ligne, ou
// rien.
enum class VendingOpeningSaid : std::uint8_t { kNothing, kNotice, kLine };

struct VendingOpeningNews {
  VendingOpeningSaid said = VendingOpeningSaid::kNothing;
  std::string text;
};

[[nodiscard]] VendingOpeningNews SaidOfOpeningResult(const ro::vfs::ItemWords& words,
                                                     std::uint8_t result);

// Une vente, écrite au vendeur : cyan pâle, nom composé et quantité VENDUE.
[[nodiscard]] VendingChatLine SaidOfSale(const ro::vfs::ItemWords& words,
                                         std::string_view item_name, std::int64_t amount);

// NOS MOTS pour ce que le client refuse avant l'envoi : le jeu n'en a pas pour
// dire « avant ».
[[nodiscard]] const char* VendingSetupFaultText(ro::sim::VendingSetupFault fault);
[[nodiscard]] const char* VendingBasketFaultText(ro::sim::VendingBasketFault fault);

// L'heure d'une vente, dans le fuseau du joueur : « 31/12 23:59:59 ».
[[nodiscard]] std::string VendingSoldAtText(std::uint32_t sold_at_unix_s);

}  // namespace moonclient
