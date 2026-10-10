#pragma once

// L'ÉCHOPPE D'ACHAT, VUE DE L'ÉCRAN : ce qu'on propose au joueur, ce que les
// codes du serveur font dire, et les mots qui disent un refus avant le clic.
//
// ⚠ DES RÈGLES PURES, sans fil, sans fenêtre et sans monde : elles reçoivent le
// sac, l'échoppe et les mots du jeu, et rendent des données. C'est ce qui les
// laisse s'essayer hors jeu — une ligne de conversation fausse ne se voit
// qu'au moment où l'on vend.

#include <cstdint>
#include <ctime>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/buying_store.h"
#include "ro/sim/buying_store_checks.h"
#include "ro/sim/inventory.h"
#include "ro/vfs/trade_words.h"

namespace moonclient {

// ── LES COULEURS DE LA CONVERSATION, celles du client d'origine ─────────────
// Rouge pour ce qui échoue, cyan pâle pour une ligne vendue.
inline constexpr std::uint32_t kBuyingStoreFailedColor = 0xFF0000;
inline constexpr std::uint32_t kBuyingStoreSoldColor = 0x64FFFF;

// Deux refus d'ouverture que le client d'origine sait dire et que ce serveur
// n'envoie pas : ils restent lus, pour un serveur qui les enverrait.
inline constexpr std::uint16_t kBuyingStoreOpenNoStore = 9;
inline constexpr std::uint16_t kBuyingStoreOpenBadPosition = 10;

// Le plus d'exemplaires qu'une ligne de vente porte : le client d'origine
// refuse d'en proposer davantage d'un coup.
inline constexpr int kBuyingStoreMostPerSaleLine = 30'000;

// UN CODE DU SERVEUR, ET LE MOT DU JEU QUI LE DIT. Vide pour un code que le
// client d'origine ne dit pas : l'appelant écrit alors le code lui-même, pour
// qu'un refus ne passe jamais sous silence.
[[nodiscard]] std::optional<ro::vfs::TradeWord> WordOfOpenRefusal(std::uint16_t code) noexcept;
[[nodiscard]] std::optional<ro::vfs::TradeWord> WordOfEnd(std::uint16_t code) noexcept;
[[nodiscard]] std::optional<ro::vfs::TradeWord> WordOfSaleRefusal(std::uint16_t code) noexcept;

// LE MOT IMPRIMÉ, qu'il porte une conversion ou non : un gabarit à un entier
// reçoit `number`, un texte sans conversion se montre tel quel. Vide si
// l'installation n'a pas le mot, ou un gabarit d'une autre forme.
[[nodiscard]] std::string PrintedWord(const ro::vfs::ItemWords& words, ro::vfs::TradeWord word,
                                      std::int64_t number);

// LA LIGNE D'UNE VENTE CONFIRMÉE : le nom composé de l'objet vendu, la
// quantité, et ce qu'elle a rapporté — prix × quantité.
[[nodiscard]] std::string SoldLineText(const ro::vfs::ItemWords& words, std::string_view item_name,
                                       const ro::sim::BuyingStoreSale& sale);

// L'HEURE D'UN ACHAT, en heure locale : « jj/mm - hh:mm:ss ».
[[nodiscard]] std::string ClockText(const std::tm& local);
[[nodiscard]] std::string LocalClockText(std::uint32_t unix_s);

// ── CE QU'ON PEUT METTRE EN ACHAT ───────────────────────────────────────────
//
// 🔴 LE CLIENT NE SAIT PAS CE QUI S'ACHÈTE EN ÉCHOPPE : c'est un drapeau de la
// base du serveur. Mais le serveur exige d'en posséder déjà un — c'est la pile
// qu'il complète. Le sac est donc la liste la plus juste qu'on puisse offrir :
// un objet par identifiant, pris dans sa PREMIÈRE pile, celle que le serveur
// compte.
struct BuyingStoreDraftCandidate {
  std::uint32_t item_id = 0;
  // La première pile de l'objet, dans l'ordre du sac.
  std::uint16_t client_slot = 0;
  int owned = 0;
};

// Les objets du sac, non portés, un par identifiant, dans l'ordre du sac.
[[nodiscard]] std::vector<BuyingStoreDraftCandidate> BuyingStoreDraftCandidates(
    std::span<const ro::sim::InventoryItem> bag);

// ── CE QU'ON PEUT VENDRE À UN ACHETEUR ──────────────────────────────────────
//
// Les piles du sac dont l'acheteur veut l'objet, non portées. ⚠ UNE PILE QUE LE
// SERVEUR REFUSERAIT RESTE LISTÉE, avec sa raison : un objet serti qui
// disparaîtrait de la liste passerait pour un objet que l'acheteur ne veut pas.
struct BuyingStoreSaleCandidate {
  std::uint16_t client_slot = 0;
  std::uint32_t item_id = 0;
  int owned = 0;
  std::uint32_t unit_price = 0;
  // Ce qui interdit la vente de cette pile, s'il y a lieu.
  std::optional<ro::sim::BuyingStoreSaleFault> barred;
};

[[nodiscard]] std::vector<BuyingStoreSaleCandidate> BuyingStoreSaleCandidates(
    const ro::sim::VisitedBuyingStore& store, std::span<const ro::sim::InventoryItem> bag);

// ── LES MOTS D'UN REFUS DIT AVANT LE CLIC ───────────────────────────────────
// Les nôtres, traduits : le jeu n'en a pas pour dire « avant ».
[[nodiscard]] const char* DraftFaultText(ro::sim::BuyingStoreDraftFault fault);
[[nodiscard]] const char* SaleFaultText(ro::sim::BuyingStoreSaleFault fault);

}  // namespace moonclient
