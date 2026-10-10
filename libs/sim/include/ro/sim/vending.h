#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/inventory.h"

namespace ro::sim {

// L'ÉCHOPPE DE VENTE : la mienne, de la préparation au journal des ventes, et
// celle d'un autre joueur que je visite.
//
// 🔴 LE CLIENT N'OUVRE RIEN LUI-MÊME. La compétence ouvre la PRÉPARATION (le
// serveur dit combien de lignes elle permet) ; la demande d'ouverture part ;
// c'est la liste renvoyée par le serveur qui ouvre l'échoppe. De même, une
// visite ne s'ouvre qu'à l'arrivée de la liste du vendeur.
//
// 🔴 ET LE SERVEUR SE TAIT SOUVENT. Un titre vide, une carte ou une case
// interdite, un vendeur trop loin, une ligne d'achat inconnue : rien ne revient,
// ou seulement un message. D'où les contrôles PURS plus bas, qui disent à la
// fenêtre, avant l'envoi, ce que le serveur refuserait.

// ── LES BORNES DU SERVEUR ──────────────────────────────────────────────────────

// `MAX_VENDING` : au-delà, `vending_openvending` fait échouer l'ouverture, et
// `vending_purchasereq` ignore l'achat sans un mot.
inline constexpr std::size_t kVendingMostLines = 12;

// `vending_max_value` : ⚠ `vending_openvending` RABAT EN SILENCE un prix plus
// haut à cette valeur. Le client refuse de l'envoyer plutôt que de laisser
// partir un prix qui ne sera pas celui affiché.
inline constexpr std::uint32_t kVendingMostPriceZeny = 1'000'000'000;

// `MAX_ZENY`, soit `INT_MAX` : zeny possédés plus recette de l'échoppe ne
// doivent pas le dépasser, sans quoi l'ouverture échoue.
inline constexpr std::int64_t kVendingMostZeny = std::numeric_limits<std::int32_t>::max();

// Le titre sur le fil, NUL compris (`MESSAGE_SIZE`) : il en reste un de moins
// pour le texte.
inline constexpr std::size_t kVendingTitleBytes = 80;
inline constexpr std::size_t kVendingTitleMostTextBytes = kVendingTitleBytes - 1;

// ── LES RÉPONSES DU SERVEUR ───────────────────────────────────────────────────

// Le résultat de l'ouverture. Tout autre que « réussi » est un ÉCHEC, et la
// préparation est alors perdue : le serveur l'a remise à zéro, il faut relancer
// la compétence.
inline constexpr std::uint8_t kVendingOpenSucceeded = 0;
inline constexpr std::uint8_t kVendingOpenFailed = 1;
// Position interdite : déclarée par le protocole, jamais émise par ce serveur.
inline constexpr std::uint8_t kVendingOpenForbiddenPosition = 3;

// Les refus d'achat. 🔴 UN ACHAT RÉUSSI N'EN ENVOIE AUCUN : il n'existe pas de
// code « réussi » sur le fil.
inline constexpr std::uint8_t kVendingBuyNoZeny = 1;
inline constexpr std::uint8_t kVendingBuyOverweight = 2;
// Aussi quand la recette ferait déborder les zeny du VENDEUR : la quantité
// renvoyée est alors le stock de la ligne.
inline constexpr std::uint8_t kVendingBuyOutOfStock = 4;
inline constexpr std::uint8_t kVendingBuyTrading = 5;
// L'échoppe a été refermée puis rouverte depuis la liste : ses offres ne sont
// plus celles qu'on a vues. La case renvoyée est alors 2, sans signification.
inline constexpr std::uint8_t kVendingBuyStoreChanged = 6;
inline constexpr std::uint8_t kVendingBuyNoSaleInfo = 7;

// ── LES DONNÉES ──────────────────────────────────────────────────────────────

// Une ligne d'échoppe : la mienne ouverte, ou une offre de celle qu'on visite.
//
// ⚠ `item.client_slot` EST LA CASE DE CHARRETTE DU VENDEUR vue du client (index
// du serveur plus deux) : c'est elle qu'un achat renvoie, et elle que les
// rapports de vente nomment. `item.amount` est CE QUI RESTE EN VENTE.
// Pour une offre visitée, `item.wearable_at` et `item.view_id` sont remplis ;
// pour ma propre échoppe, le serveur ne les envoie pas.
//
// Le prix est le prix UNITAIRE, et c'est exactement ce que le serveur facture :
// ni remise ni taxe côté acheteur.
struct VendingLine {
  InventoryItem item;
  std::uint32_t price_zeny = 0;
};

// Une vente inscrite au journal.
struct VendingSale {
  // L'objet tel que la ligne le décrivait, `item.amount` étant la quantité
  // VENDUE. Une case que l'échoppe ne connaît pas donne un objet d'identifiant
  // zéro : la vente reste comptée.
  InventoryItem item;
  // Ce que la ligne a rapporté en tout — pas un prix unitaire —, taxe déduite
  // (nulle sur ce serveur).
  std::int32_t zeny = 0;
  std::uint32_t buyer_char_id = 0;
  // L'heure du serveur, en secondes Unix tronquées à 32 bits.
  std::uint32_t sold_at_unix_s = 0;
};

// Pourquoi une visite a pris fin : ce que la fenêtre peut en dire.
enum class VendingVisitEnd : std::uint8_t {
  kNone,
  // Le joueur a fermé.
  kClosed,
  // La liste est revenue vide après un achat : tout est acheté.
  kEmptied,
  // L'enseigne du vendeur a disparu.
  kVendorGone,
  // Changement de carte ou de serveur.
  kLeftMap,
};

// ── L'ÉTAT ───────────────────────────────────────────────────────────────────

class Vending {
 public:
  // ── LA PRÉPARATION ────────────────────────────────────────────────────────
  //
  // Ouverte par la compétence ; close par l'abandon, par le résultat de
  // l'ouverture, ou par l'échoppe qui s'ouvre.
  //
  // 🔴 TANT QU'ELLE EST OUVERTE, LE SERVEUR FIGE LA CHARRETTE : ni dépôt ni
  // retrait. Seul l'abandon dit au serveur la lève — d'où `TakeFarewell`.
  [[nodiscard]] bool preparing() const noexcept { return preparing_; }
  // Le nombre de lignes que le serveur annonce (deux plus le niveau de la
  // compétence), et celui qu'il acceptera vraiment.
  [[nodiscard]] std::uint16_t offered_lines() const noexcept { return offered_lines_; }
  [[nodiscard]] std::size_t permitted_lines() const noexcept;
  // Change à chaque préparation neuve : c'est lui qui remet la fenêtre à zéro.
  [[nodiscard]] std::uint32_t preparation_seq() const noexcept { return preparation_seq_; }

  void TellPreparation(std::uint16_t offered_lines);

  // La demande d'ouverture est partie et attend sa réponse : elle ne repart pas.
  [[nodiscard]] bool opening_pending() const noexcept { return opening_pending_; }
  [[nodiscard]] bool CanSendOpening() const noexcept { return preparing_ && !opening_pending_; }
  // Retient le titre MONTRÉ (déjà coupé : voir `FitVendingTitle`) : le serveur
  // ne le renverra pas à son vendeur.
  void NoteOpeningSent(std::string title_utf8);
  // ⚠ LE SERVEUR PEUT NE JAMAIS RÉPONDRE : carte ou case interdite, joueur
  // muet, zone réservée. Il n'envoie alors qu'un message, et la préparation
  // reste ouverte chez lui. C'est à celui qui lit ces messages de libérer la
  // demande pour qu'on puisse réessayer.
  void ForgetPendingOpening() noexcept { opening_pending_ = false; }

  // Le joueur abandonne la préparation. L'abandon se dit : voir `TakeFarewell`.
  void CancelPreparation() noexcept;
  // Vrai UNE fois après chaque abandon qui doit se dire au serveur.
  [[nodiscard]] bool TakeFarewell() noexcept;

  // Le résultat de l'ouverture, et son rang. ⚠ LE RANG ZÉRO VEUT DIRE « AUCUN
  // RÉSULTAT ENCORE ».
  void TellOpeningResult(std::uint8_t result);
  [[nodiscard]] std::uint8_t opening_result() const noexcept { return opening_result_; }
  [[nodiscard]] std::uint32_t opening_result_seq() const noexcept { return opening_result_seq_; }

  // ── MON ÉCHOPPE ───────────────────────────────────────────────────────────
  [[nodiscard]] bool shop_open() const noexcept { return shop_open_; }
  // Ouverte, mais plus rien à vendre. Le serveur la garde ouverte : seul le
  // joueur la ferme.
  [[nodiscard]] bool sold_out() const noexcept { return shop_open_ && shop_lines_.empty(); }
  // Change à chaque ouverture neuve.
  [[nodiscard]] std::uint32_t shop_opening_seq() const noexcept { return shop_opening_seq_; }
  // Change à chaque nouvelle de l'échoppe : ouverture, vente, fermeture.
  [[nodiscard]] std::uint32_t shop_seq() const noexcept { return shop_seq_; }
  // Le titre retenu à l'envoi ; vide pour une échoppe que le serveur a ouverte
  // sans demande de ce client.
  [[nodiscard]] const std::string& shop_title() const noexcept { return shop_title_; }
  [[nodiscard]] std::uint32_t shop_owner_aid() const noexcept { return shop_owner_aid_; }
  [[nodiscard]] std::span<const VendingLine> shop_lines() const noexcept { return shop_lines_; }

  // La liste de l'échoppe ouverte. Elle clôt la préparation et remet le
  // journal à zéro.
  void TellShopOpened(std::uint32_t owner_aid, std::vector<VendingLine> lines);
  // Une vente : la ligne décroît, disparaît à zéro, et la vente s'inscrit au
  // journal. Une vente sans échoppe ouverte s'inscrit quand même.
  void TellSale(std::uint16_t cart_slot, std::uint16_t amount, std::uint32_t buyer_char_id,
                std::uint32_t sold_at_unix_s, std::int32_t zeny);
  // La fermeture. ⚠ LE SERVEUR N'Y RÉPOND PAS : c'est l'envoi qui ferme.
  void CloseShop() noexcept;

  // ── LE JOURNAL DES VENTES ─────────────────────────────────────────────────
  //
  // Il SURVIT à la fermeture de l'échoppe — c'est après qu'on le lit —, jusqu'à
  // la prochaine ouverture ou jusqu'à ce qu'on l'efface.
  [[nodiscard]] std::span<const VendingSale> journal() const noexcept { return journal_; }
  // Le cumul encaissé. ⚠ SUR 64 BITS : douze lignes à un milliard dépassent
  // ce que 32 bits tiennent.
  [[nodiscard]] std::int64_t earned_zeny() const noexcept { return earned_zeny_; }
  // Change à chaque vente et à chaque effacement.
  [[nodiscard]] std::uint32_t journal_seq() const noexcept { return journal_seq_; }
  void ClearJournal() noexcept;

  // ── L'ÉCHOPPE VISITÉE ─────────────────────────────────────────────────────
  //
  // 🔴 AUCUN PAQUET NE LA FERME : ni le client ne le dit au serveur, ni le
  // serveur au client. Elle se ferme ici, par le joueur ou par ce qu'on voit.
  [[nodiscard]] bool visiting() const noexcept { return visiting_; }
  // Change à chaque visite neuve (un autre vendeur, ou après une fermeture).
  [[nodiscard]] std::uint32_t visit_opening_seq() const noexcept { return visit_opening_seq_; }
  // Change à chaque liste reçue, rafraîchissement compris.
  [[nodiscard]] std::uint32_t visit_seq() const noexcept { return visit_seq_; }
  [[nodiscard]] VendingVisitEnd visit_end() const noexcept { return visit_end_; }
  // L'identifiant de compte du vendeur, et celui de SON ÉCHOPPE, qu'un achat
  // doit rappeler : une échoppe rouverte en porte un autre.
  [[nodiscard]] std::uint32_t vendor_aid() const noexcept { return vendor_aid_; }
  [[nodiscard]] std::uint32_t vender_id() const noexcept { return vender_id_; }
  [[nodiscard]] std::span<const VendingLine> offers() const noexcept { return offers_; }

  // La liste d'un vendeur. ⚠ ARRIVÉE VIDE APRÈS UN ACHAT, elle clôt la visite
  // (`kEmptied`).
  void TellVisit(std::uint32_t vendor_aid, std::uint32_t vender_id,
                 std::vector<VendingLine> offers);

  // 🔴 UN ACHAT RÉUSSI N'A PAS DE RÉPONSE, et le serveur ne renvoie pas la
  // liste : l'achat part donc SUIVI d'une nouvelle demande de liste, et c'est
  // elle qui montre ce qui reste. Tant qu'elle n'est pas revenue, un second
  // achat ne part pas.
  void NotePurchaseSent() noexcept;
  [[nodiscard]] bool purchase_pending() const noexcept { return purchase_pending_; }

  // Un refus d'achat, et son rang. ⚠ LE RANG ZÉRO VEUT DIRE « AUCUN REFUS
  // ENCORE ».
  void TellPurchaseRefused(std::uint16_t shop_slot, std::uint16_t amount, std::uint8_t result);
  [[nodiscard]] std::uint8_t purchase_result() const noexcept { return purchase_result_; }
  [[nodiscard]] std::uint16_t purchase_result_slot() const noexcept {
    return purchase_result_slot_;
  }
  [[nodiscard]] std::uint16_t purchase_result_amount() const noexcept {
    return purchase_result_amount_;
  }
  [[nodiscard]] std::uint32_t purchase_result_seq() const noexcept { return purchase_result_seq_; }

  void CloseVisit() noexcept;
  // L'enseigne d'un vendeur a disparu : si c'est celui qu'on visite, la visite
  // se ferme.
  void TellVendorGone(std::uint32_t vendor_aid) noexcept;

  // ── LES CHANGEMENTS DE CARTE ET DE SERVEUR ────────────────────────────────

  // Même serveur : la visite se ferme ; la préparation aussi, ET L'ABANDON SE
  // DIT, sans quoi la charrette resterait figée chez le serveur.
  // ⚠ MON ÉCHOPPE OUVERTE RESTE OUVERTE : le serveur ne la ferme que si la carte
  // ou la case d'arrivée l'interdit, et sans le dire au vendeur. La garder
  // laisse au joueur de quoi la fermer ; la fermer à tort le laisserait figé sur
  // place sans bouton.
  void CloseForMapChange() noexcept;
  // Autre serveur : le serveur de départ a tout fermé, échoppe comprise, et il
  // n'y a plus personne à qui dire au revoir. Le journal reste.
  void CloseSilently() noexcept;

 private:
  void EndVisit(VendingVisitEnd why) noexcept;

  bool preparing_ = false;
  std::uint16_t offered_lines_ = 0;
  std::uint32_t preparation_seq_ = 0;
  bool opening_pending_ = false;
  std::string pending_title_;
  bool farewell_due_ = false;
  std::uint8_t opening_result_ = kVendingOpenSucceeded;
  std::uint32_t opening_result_seq_ = 0;

  bool shop_open_ = false;
  std::uint32_t shop_opening_seq_ = 0;
  std::uint32_t shop_seq_ = 0;
  std::string shop_title_;
  std::uint32_t shop_owner_aid_ = 0;
  std::vector<VendingLine> shop_lines_;

  std::vector<VendingSale> journal_;
  std::int64_t earned_zeny_ = 0;
  std::uint32_t journal_seq_ = 0;

  bool visiting_ = false;
  std::uint32_t visit_opening_seq_ = 0;
  std::uint32_t visit_seq_ = 0;
  VendingVisitEnd visit_end_ = VendingVisitEnd::kNone;
  std::uint32_t vendor_aid_ = 0;
  std::uint32_t vender_id_ = 0;
  std::vector<VendingLine> offers_;
  bool purchase_pending_ = false;
  // Le vendeur dont une liste demandée APRÈS UN ACHAT est encore en route alors
  // que la visite est close : elle ne doit pas la rouvrir. Zéro pour aucun.
  std::uint32_t stale_refresh_aid_ = 0;
  std::uint8_t purchase_result_ = 0;
  std::uint16_t purchase_result_slot_ = 0;
  std::uint16_t purchase_result_amount_ = 0;
  std::uint32_t purchase_result_seq_ = 0;
};

// ── LES CONTRÔLES AVANT ENVOI ─────────────────────────────────────────────────
//
// PURS : ils ne lisent que ce qu'on leur donne. La fenêtre les appelle pour dire
// au joueur, avant l'envoi, ce que le serveur refuserait ; les verbes de `net`
// les rejouent et n'envoient rien qui échoue.

// Le rang qui désigne l'ensemble plutôt qu'une ligne.
inline constexpr std::size_t kVendingWholeRequest = std::numeric_limits<std::size_t>::max();

// Le TITRE TEL QU'IL PARTIRA : le plus long début du texte dont la forme sur le
// fil tient en `kVendingTitleMostTextBytes` octets, coupé entre deux caractères.
// C'est lui qu'on montre, et lui qu'on retient.
[[nodiscard]] std::string FitVendingTitle(std::string_view title_utf8);

// Une ligne que le joueur veut mettre en vente.
struct VendingSetupLine {
  // La case de charrette vue du client, telle que la charrette la donne.
  std::uint16_t cart_slot = 0;
  std::uint16_t amount = 0;
  std::uint32_t price_zeny = 0;
};

enum class VendingSetupFault : std::uint8_t {
  // Le serveur l'ignore sans un mot.
  kNoTitle,
  kNoLines,
  // Plus que la préparation n'en permet : échec de l'ouverture.
  kTooManyLines,
  // Deux lignes pour la même case : le serveur ne le vérifie pas, et
  // compterait deux fois la même pile.
  kDuplicateSlot,
  // Le serveur l'accepte : l'objet serait donné.
  kZeroPrice,
  // Rabattu EN SILENCE par le serveur.
  kPriceAboveMost,
  // Le serveur l'accepte, et met en vente une ligne vide.
  kZeroAmount,
  // Les suivants font échouer TOUTE l'échoppe, avec un message.
  kNotInCart,
  kAmountAboveStack,
  kUnidentified,
  kDamaged,
  kRented,
  // Zeny possédés plus recette au-delà de `kVendingMostZeny` : échec.
  kTakingsOverflow,
};

struct VendingSetupRefusal {
  VendingSetupFault fault = VendingSetupFault::kNoLines;
  // Le rang de la ligne fautive, ou `kVendingWholeRequest`.
  std::size_t line = kVendingWholeRequest;
};

// La recette de l'échoppe si tout se vend, prix rabattus comme le serveur les
// rabat.
[[nodiscard]] std::int64_t VendingSetupTakings(std::span<const VendingSetupLine> lines) noexcept;

// Tout ce qui ne va pas, dans l'ordre des lignes ; vide si l'ouverture peut
// partir. `cart` est le contenu de la charrette, `zeny` ce que le joueur
// possède.
[[nodiscard]] std::vector<VendingSetupRefusal> CheckVendingSetup(
    std::string_view title_utf8, std::span<const VendingSetupLine> lines,
    std::size_t permitted_lines, std::span<const InventoryItem> cart, std::int64_t zeny);

// Une ligne du panier chez un vendeur.
struct VendingBasketLine {
  // La case de l'offre (`VendingLine::item.client_slot`), renvoyée telle quelle.
  std::uint16_t shop_slot = 0;
  std::uint16_t amount = 0;
};

// Les lignes d'une même offre FUSIONNÉES, dans l'ordre de leur première
// apparition. Le serveur les cumule de toute façon ; fusionnées, elles comptent
// une seule ligne sur les douze permises.
[[nodiscard]] std::vector<VendingBasketLine> MergeVendingBasket(
    std::span<const VendingBasketLine> basket);

enum class VendingBasketFault : std::uint8_t {
  kEmpty,
  // Au-delà de `kVendingMostLines` lignes après fusion : ignoré sans un mot.
  kTooManyLines,
  // Ignoré sans un mot.
  kZeroAmount,
  // Ignoré sans un mot.
  kUnknownOffer,
  // Refusé, code « stock ».
  kAmountAboveStock,
  // Refusé, code « zeny ».
  kTotalAboveZeny,
};

struct VendingBasketRefusal {
  VendingBasketFault fault = VendingBasketFault::kEmpty;
  // Le rang dans le panier FUSIONNÉ, ou `kVendingWholeRequest`.
  std::size_t line = kVendingWholeRequest;
};

// Le coût du panier, fusionné ou non ; une offre inconnue ne coûte rien.
[[nodiscard]] std::int64_t VendingBasketCost(std::span<const VendingBasketLine> basket,
                                             std::span<const VendingLine> offers) noexcept;

// Tout ce qui ne va pas dans le panier, APRÈS FUSION ; vide si l'achat peut
// partir. ⚠ Ni le poids ni la place dans l'inventaire ne se contrôlent ici :
// le serveur refuse le premier avec un code, la seconde en silence.
[[nodiscard]] std::vector<VendingBasketRefusal> CheckVendingBasket(
    std::span<const VendingBasketLine> basket, std::span<const VendingLine> offers,
    std::int64_t zeny);

}  // namespace ro::sim
