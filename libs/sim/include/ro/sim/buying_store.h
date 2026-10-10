#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ro::sim {

// L'ÉCHOPPE D'ACHAT : celle que le joueur tient, et celle qu'il visite pour y
// vendre. Ce que le serveur en dit, et rien de ce qu'on en montre.
//
// Un acheteur annonce ce qu'il veut, combien, à quel prix, et une LIMITE de zeny
// à dépenser en tout. Les autres joueurs lui vendent ; chaque vente décrémente
// la ligne et la limite. Quand plus rien n'est voulu, ou que la limite est
// épuisée, le serveur ferme l'échoppe de lui-même.
//
// 🔴 LE CLIENT N'OUVRE RIEN LUI-MÊME. La compétence (ou l'objet) fait répondre au
// serveur « prépare » ; la demande part ; c'est la liste renvoyée par le serveur
// qui ouvre l'échoppe. De même, la liste d'une échoppe visitée est la réponse à
// un clic sur son enseigne.
//
// 🔴 ET LE PROPRIÉTAIRE NE REÇOIT NI SON ENSEIGNE NI SA DISPARITION : le serveur
// les envoie à la zone, SAUF à lui. Ce modèle garde donc le titre demandé, que
// l'enseigne du joueur lui-même ne peut montrer que d'ici.

// ── LES BORNES DU SERVEUR ───────────────────────────────────────────────────

// Le plus de lignes qu'une échoppe puisse tenir : MAX_BUYINGSTORE_SLOTS. Le
// serveur rabat la préparation sur cette valeur (`buyingstore_setup`).
inline constexpr std::size_t kBuyingStoreMostSlots = 5;

// Le prix unitaire le plus haut : BUYINGSTORE_MAX_PRICE (`buyingstore_create`).
// ⚠ ET LE PLUS BAS EST UN : contrairement à l'échoppe de vente, un prix nul est
// refusé.
inline constexpr std::uint32_t kBuyingStoreMostPrice = 99'990'000;

// Ce que le joueur peut POSSÉDER d'un objet voulu, une fois tout acheté :
// BUYINGSTORE_MAX_AMOUNT (`buyingstore_create`). La possession se lit dans LA
// PREMIÈRE PILE de l'objet, comme le serveur la cherche.
inline constexpr std::uint32_t kBuyingStoreMostOwned = 9'999;

// Ce que la bourse peut tenir : MAX_ZENY du serveur.
inline constexpr std::int64_t kBuyingStoreMostZeny = 2'147'483'647;

// ── LES CODES DU SERVEUR ────────────────────────────────────────────────────
//
// Ils restent des nombres dans le modèle : un code que ce client ne connaît pas
// doit arriver jusqu'à la fenêtre, qui dira qu'elle ne le connaît pas.

// Refus d'ouverture (`clif_buyingstore_open_failed`).
inline constexpr std::uint16_t kBuyingStoreOpenFailed = 1;
// Le poids projeté atteindrait la surcharge : le paquet porte ce poids.
inline constexpr std::uint16_t kBuyingStoreOpenOverweight = 2;
inline constexpr std::uint16_t kBuyingStoreOpenNoInformation = 8;

// Fin de mon échoppe (`clif_buyingstore_trade_failed_buyer`).
inline constexpr std::uint16_t kBuyingStoreEndLimitReached = 3;
inline constexpr std::uint16_t kBuyingStoreEndAllBought = 4;

// Refus d'une vente, au vendeur (`clif_buyingstore_trade_failed_seller`).
// ⚠ LE PREMIER EST FOURRE-TOUT : échoppe disparue ou rouverte, hors de portée,
// case changée, objet serti, lié, loué, non voulu, surcharge de l'acheteur.
inline constexpr std::uint16_t kBuyingStoreSaleFailed = 5;
// Plus que ce que l'acheteur veut encore de cet objet.
inline constexpr std::uint16_t kBuyingStoreSaleOverCount = 6;
// L'acheteur n'a plus de quoi payer. ⚠ Sa limite est d'abord rabattue sur son
// zeny réel : la limite affichée peut promettre plus qu'il n'a.
inline constexpr std::uint16_t kBuyingStoreSaleBuyerLacksZeny = 7;

// ── LES DONNÉES ─────────────────────────────────────────────────────────────

// Une ligne d'échoppe d'achat, la mienne comme celle d'un autre.
struct BuyingStoreLine {
  std::uint32_t item_id = 0;
  // La famille de l'objet, telle que le serveur la replie (voir `InventoryItem`).
  std::uint8_t item_type = 0;
  // Combien l'acheteur en veut ENCORE. ⚠ ZÉRO EST UNE LIGNE PLEINE, et le
  // serveur la liste quand même : elle reste, et plus personne ne peut y vendre.
  std::uint16_t wanted = 0;
  std::uint32_t price = 0;
};

// Une ligne que le joueur PRÉPARE, avant l'envoi. Les quantités sont larges pour
// que la validation voie une saisie démesurée au lieu d'une valeur déjà repliée.
struct BuyingStoreWish {
  std::uint32_t item_id = 0;
  std::uint32_t amount = 0;
  std::uint32_t price = 0;
};

// Un achat conclu par MON échoppe.
struct BuyingStorePurchase {
  std::uint32_t item_id = 0;
  std::uint16_t amount = 0;
  // Ce que la ligne a coûté, en tout.
  std::int64_t zeny = 0;
  // Le PERSONNAGE qui m'a vendu — pas son compte.
  std::uint32_t seller_char_id = 0;
  // L'heure du serveur, en secondes depuis l'époque UNIX.
  std::uint32_t unix_s = 0;
};

// Une ligne que le joueur VEND à une échoppe visitée.
struct BuyingStoreSaleLine {
  // La case du sac, vue du client.
  std::uint16_t client_slot = 0;
  std::uint32_t item_id = 0;
  std::uint16_t amount = 0;
};

// Une ligne vendue, confirmée par le serveur.
struct BuyingStoreSale {
  std::uint16_t client_slot = 0;
  // Retrouvé dans la demande partie : le serveur ne le répète pas. Zéro si la
  // confirmation ne correspond à aucune ligne demandée.
  std::uint32_t item_id = 0;
  std::uint16_t amount = 0;
  std::uint32_t unit_price = 0;
  std::int64_t zeny = 0;
};

// L'échoppe d'un autre, telle que sa liste l'a dite.
struct VisitedBuyingStore {
  // Le COMPTE de l'acheteur, qui est aussi son identifiant d'acteur dans le monde.
  std::uint32_t buyer_account_id = 0;
  // Le numéro de CETTE ouverture : il change si l'acheteur ferme et rouvre, et
  // une vente qui en rappelle un ancien échoue.
  std::uint32_t store_id = 0;
  // Ce qui reste à dépenser, tel qu'annoncé puis décrémenté par nos ventes.
  std::int64_t zeny_limit = 0;
  std::vector<BuyingStoreLine> lines;
};

// Où en est l'échoppe du joueur.
enum class BuyingStorePhase : std::uint8_t {
  kClosed,
  // Le serveur a dit « prépare » : le joueur compose.
  kPreparing,
  // La demande est partie ; la réponse ouvrira ou refusera.
  kRequested,
  kOpen,
};

class BuyingStore {
 public:
  // ══ MON ÉCHOPPE ═══════════════════════════════════════════════════════════

  [[nodiscard]] BuyingStorePhase phase() const noexcept { return phase_; }
  [[nodiscard]] bool open() const noexcept { return phase_ == BuyingStorePhase::kOpen; }

  // ── LA PRÉPARATION ──────────────────────────────────────────────────────
  //
  // Le serveur dit combien de lignes sont permises. ⚠ UNE PRÉPARATION ARRIVÉE
  // ÉCHOPPE OUVERTE EST IGNORÉE : le serveur ne l'envoie pas dans ce cas, et la
  // prendre fermerait une échoppe qui court toujours.
  void TellPreparation(std::uint8_t slots);
  [[nodiscard]] std::uint8_t slots() const noexcept { return slots_; }
  // Le rang de la préparation : il change à chaque préparation neuve, et c'est
  // lui qui remet la fenêtre de composition à zéro.
  [[nodiscard]] std::uint32_t preparation_seq() const noexcept { return preparation_seq_; }

  // Le joueur renonce. Le serveur n'a rien à défaire : une renonciation ne lui
  // change rien, et il ne répond pas.
  void CancelPreparation() noexcept;

  // La demande d'ouverture part, avec ce titre tel qu'il sera MONTRÉ — coupé
  // comme le fil le coupera. Faux, et rien ne change, hors préparation : c'est
  // ce qui empêche d'émettre deux fois.
  bool TellRequested(std::string title, std::int64_t zeny_limit) noexcept;
  // La limite demandée, que la réponse du serveur confirmera.
  [[nodiscard]] std::int64_t requested_zeny_limit() const noexcept { return requested_zeny_limit_; }

  // Le serveur refuse. ⚠ LA PRÉPARATION EST PERDUE : il a remis à zéro le nombre
  // de lignes permises, et une nouvelle demande serait refusée à son tour. Il
  // faut relancer la compétence.
  void TellOpenRefused(std::uint16_t result, std::uint32_t weight) noexcept;
  [[nodiscard]] std::uint16_t open_refusal() const noexcept { return open_refusal_; }
  // Le poids projeté, en dixièmes ; n'a de sens que pour la surcharge.
  [[nodiscard]] std::uint32_t open_refusal_weight() const noexcept { return open_refusal_weight_; }
  [[nodiscard]] std::uint32_t open_refusal_seq() const noexcept { return open_refusal_seq_; }

  // ── L'ÉCHOPPE OUVERTE ───────────────────────────────────────────────────
  //
  // La liste du serveur OUVRE l'échoppe. ⚠ ELLE REMET LE JOURNAL À ZÉRO : il
  // parle de l'échoppe d'avant, qu'on vient de remplacer.
  void TellOpened(std::uint32_t owner_account_id, std::int64_t zeny_limit,
                  std::span<const BuyingStoreLine> lines);
  [[nodiscard]] std::uint32_t opening_seq() const noexcept { return opening_seq_; }
  [[nodiscard]] std::uint32_t owner_account_id() const noexcept { return owner_account_id_; }
  // Le titre tel qu'envoyé. Vide si le serveur a ouvert une échoppe qu'on n'a
  // pas demandée dans cette session.
  [[nodiscard]] const std::string& title() const noexcept { return title_; }
  // Ce qui reste à dépenser.
  [[nodiscard]] std::int64_t zeny_limit() const noexcept { return zeny_limit_; }
  // La limite à l'ouverture.
  [[nodiscard]] std::int64_t opening_zeny_limit() const noexcept { return opening_zeny_limit_; }
  [[nodiscard]] std::span<const BuyingStoreLine> lines() const noexcept { return lines_; }

  // ── LES ACHATS ──────────────────────────────────────────────────────────
  //
  // Un vendeur m'a vendu. La ligne de l'objet perd la quantité — sans passer
  // sous zéro —, la limite prend la valeur que le serveur annonce, et l'achat
  // s'inscrit au journal. Un achat hors échoppe ouverte s'inscrit quand même :
  // c'est de l'argent dépensé.
  void TellPurchase(const BuyingStorePurchase& purchase, std::int64_t zeny_limit_left);
  // 🔴 LE JOURNAL SURVIT À LA FERMETURE : c'est à la fin qu'on le relit.
  [[nodiscard]] std::span<const BuyingStorePurchase> journal() const noexcept { return journal_; }
  [[nodiscard]] std::int64_t spent_zeny() const noexcept { return spent_zeny_; }
  [[nodiscard]] std::uint32_t journal_seq() const noexcept { return journal_seq_; }
  // Oublie le journal, quand le joueur le referme.
  void ClearJournal() noexcept;

  // ── LA FIN ──────────────────────────────────────────────────────────────
  //
  // Le serveur a fermé l'échoppe : tout est acheté, ou la limite est épuisée.
  // Il n'y a rien à lui dire.
  void TellEnded(std::uint16_t result) noexcept;
  [[nodiscard]] std::uint16_t end_result() const noexcept { return end_result_; }
  [[nodiscard]] std::uint32_t end_seq() const noexcept { return end_seq_; }

  // Le joueur ferme son échoppe. Ouverte, la fermeture se dit au serveur — voir
  // `TakeFarewell` ; en préparation, c'est une renonciation.
  //
  // ⚠ DEMANDÉE MAIS PAS ENCORE OUVERTE, ELLE SE DIT AUSSI : le serveur traite
  // les paquets dans l'ordre, la fermeture défera donc l'ouverture qu'il est en
  // train d'accorder. Et la liste qui arrivera ensuite n'ouvre plus rien.
  void Close() noexcept;
  // Vrai UNE fois après chaque fermeture qui doit se dire au serveur.
  [[nodiscard]] bool TakeFarewell() noexcept;

  // ══ L'ÉCHOPPE VISITÉE ═════════════════════════════════════════════════════

  // La liste d'un acheteur. ⚠ ELLE REMPLACE la visite en cours, et vide les
  // ventes de la précédente si l'acheteur ou le numéro change.
  void TellVisit(VisitedBuyingStore store);
  [[nodiscard]] bool visiting() const noexcept { return visiting_; }
  [[nodiscard]] const VisitedBuyingStore& visited() const noexcept { return visited_; }
  [[nodiscard]] std::uint32_t visit_seq() const noexcept { return visit_seq_; }

  // Le joueur quitte l'échoppe visitée. Le serveur n'en sait rien et n'a pas à
  // le savoir : il n'a rien réservé.
  void LeaveVisit() noexcept;

  // La vente part. Faux, et rien ne change, hors visite, sans ligne, ou tant
  // qu'une vente attend sa réponse : c'est ce qui empêche d'émettre deux fois.
  bool TellSaleRequested(std::span<const BuyingStoreSaleLine> lines);
  [[nodiscard]] bool sale_pending() const noexcept { return !pending_sale_.empty(); }

  // Une ligne vendue. La ligne voulue de l'objet perd la quantité et la limite
  // perd le montant, comme le serveur vient de le faire chez l'acheteur : il ne
  // renvoie pas la liste.
  //
  // ⚠ LA VENTE EST ATOMIQUE : le serveur valide TOUTES les lignes avant d'en
  // traiter une. Une ligne confirmée annonce donc les autres, et l'attente se
  // lève à la dernière.
  void TellSold(std::uint16_t client_slot, std::uint16_t amount, std::uint32_t unit_price);
  [[nodiscard]] std::span<const BuyingStoreSale> sales() const noexcept { return sales_; }
  [[nodiscard]] std::int64_t earned_zeny() const noexcept { return earned_zeny_; }
  [[nodiscard]] std::uint32_t sale_seq() const noexcept { return sale_seq_; }

  // Le serveur refuse la vente — toute la vente : aucune ligne n'est passée.
  void TellSaleRefused(std::uint16_t result, std::uint32_t item_id) noexcept;
  [[nodiscard]] std::uint16_t sale_refusal() const noexcept { return sale_refusal_; }
  // L'objet en cause ; zéro quand le refus ne vise aucune ligne.
  [[nodiscard]] std::uint32_t sale_refusal_item() const noexcept { return sale_refusal_item_; }
  [[nodiscard]] std::uint32_t sale_refusal_seq() const noexcept { return sale_refusal_seq_; }

  // ══ LES CHANGEMENTS DE CARTE ══════════════════════════════════════════════

  // Même serveur, autre carte. ⚠ MON ÉCHOPPE RESTE OUVERTE : le serveur ne la
  // ferme que si la carte l'interdit, et le dit alors par un message. La
  // préparation et la visite, elles, sont perdues — l'acheteur n'est plus en vue.
  void CloseForMapChange() noexcept;
  // Autre serveur : le serveur de départ a tout fermé de lui-même, et il n'y a
  // plus personne à qui dire au revoir. Le journal reste.
  void CloseSilently() noexcept;

 private:
  void ForgetMyStore() noexcept;

  BuyingStorePhase phase_ = BuyingStorePhase::kClosed;
  std::uint8_t slots_ = 0;
  std::uint32_t preparation_seq_ = 0;
  std::string requested_title_;
  std::int64_t requested_zeny_limit_ = 0;
  std::uint16_t open_refusal_ = 0;
  std::uint32_t open_refusal_weight_ = 0;
  std::uint32_t open_refusal_seq_ = 0;

  std::uint32_t opening_seq_ = 0;
  std::uint32_t owner_account_id_ = 0;
  std::string title_;
  std::int64_t zeny_limit_ = 0;
  std::int64_t opening_zeny_limit_ = 0;
  std::vector<BuyingStoreLine> lines_;

  std::vector<BuyingStorePurchase> journal_;
  std::int64_t spent_zeny_ = 0;
  std::uint32_t journal_seq_ = 0;

  std::uint16_t end_result_ = 0;
  std::uint32_t end_seq_ = 0;
  bool farewell_due_ = false;
  // Une demande abandonnée avant sa réponse : la liste qui la suivra est celle
  // d'une échoppe que la fermeture a déjà défaite.
  bool request_abandoned_ = false;

  bool visiting_ = false;
  VisitedBuyingStore visited_;
  std::uint32_t visit_seq_ = 0;
  // Les lignes de la vente partie, que les confirmations consomment.
  std::vector<BuyingStoreSaleLine> pending_sale_;
  std::vector<BuyingStoreSale> sales_;
  std::int64_t earned_zeny_ = 0;
  std::uint32_t sale_seq_ = 0;
  std::uint16_t sale_refusal_ = 0;
  std::uint32_t sale_refusal_item_ = 0;
  std::uint32_t sale_refusal_seq_ = 0;
};

}  // namespace ro::sim
