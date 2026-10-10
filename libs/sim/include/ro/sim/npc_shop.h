#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace ro::sim {

// LA BOUTIQUE D'UN PNJ : ce que le serveur en dit, et rien de ce qu'on en montre.
//
// Quatre sortes de boutiques partagent la même fenêtre, et chacune a ses
// paquets :
//
// - la boutique À ZENY (`shop`), qui achète ET vend. Un clic sur le PNJ fait
//   proposer le choix (`ZC_SELECT_DEALTYPE`) ; un script qui appelle
//   `callshop "…",1` ou `,2` envoie DIRECTEMENT la liste d'achat ou de vente,
//   sans jamais nommer le PNJ ;
// - le MARKET, à stock limité, qui ne fait qu'acheter ;
// - la boutique À POINTS ou À OBJET-DEVISE (`pointshop`, `itemshop`), qui ne
//   fait qu'acheter, contre autre chose que des zeny.
//
// 🔴 LE CLIENT NE CHOISIT PAS : c'est le paquet reçu qui dit quelle boutique est
// ouverte, et c'est lui qui l'ouvre. Il n'y a pas de fenêtre sans paquet.
//
// 🔴 ET TOUTE FERMETURE SE DIT AU SERVEUR. Tant qu'il tient une boutique pour
// ouverte, il refuse de laisser marcher le personnage ; un market ouvert arme en
// plus son état d'échange, qui bloque toute autre boutique. D'où
// `TakeFarewell`, que le modèle ne peut pas envoyer lui-même.

// Quelle boutique est ouverte.
enum class NpcShopKind : std::uint8_t { kNone, kZeny, kMarket, kPoints };

// Ce que la boutique à zeny offre : acheter, vendre, ou les deux. Une ouverture
// par liste seule — `callshop` — n'offre que ce qu'elle a envoyé.
enum class NpcShopDeals : std::uint8_t { kBuyAndSell, kBuyOnly, kSellOnly };

// La liste en jeu dans une demande ou un résultat.
enum class NpcShopSide : std::uint8_t { kBuy, kSell };

// Le paquet de fermeture que la boutique doit au serveur.
enum class NpcShopFarewell : std::uint8_t {
  kNone,
  // `CZ_NPC_TRADE_QUIT` : boutique à zeny, à points ou à objet-devise.
  kTradeQuit,
  // `CZ_NPC_MARKET_CLOSE` : le market, qui a armé l'état d'échange.
  kMarketClose,
};

// Le stock d'une ligne qu'aucune borne ne limite : le market l'écrit −1, les
// autres boutiques n'en ont pas.
inline constexpr std::int64_t kNpcShopUnlimitedStock = -1;

// Ce qu'un seul objet peut peser en pile, et ce qu'on peut en acheter en une
// ligne : `MAX_AMOUNT` du serveur. Au-delà, `pc_checkadditem` refuse l'achat
// ENTIER en « poids ».
inline constexpr std::int64_t kNpcShopMostPerStack = 30000;

// Le résultat qui dit « réussi », pour les trois familles de résultats.
inline constexpr std::uint16_t kNpcShopDealSucceeded = 0;

// UNE LIGNE À ACHETER.
struct NpcShopOffer {
  std::uint32_t item_id = 0;
  // Le prix d'AFFICHE, avant remise. Zéro quand la boutique n'en donne pas
  // d'autre que celui qu'elle facture.
  std::uint32_t base_price = 0;
  // 🔴 LE PRIX QUE LE SERVEUR FACTURERA, remise comprise. C'est lui qui se
  // montre et se totalise : le serveur calcule la remise, et un client qui la
  // recalculerait se tromperait dès qu'une règle change là-bas.
  std::uint32_t price = 0;
  std::uint8_t item_type = 0;
  std::uint16_t view_sprite = 0;
  std::uint32_t equip_location = 0;
  // Ce qui reste en rayon : `kNpcShopUnlimitedStock` hors market.
  std::int64_t stock = kNpcShopUnlimitedStock;
  // Le poids d'UNE unité, en dixièmes. ⚠ SEUL LE MARKET LE DONNE : zéro veut
  // dire « inconnu », pas « ne pèse rien ».
  std::uint32_t unit_weight = 0;
};

// UNE LIGNE À VENDRE : une case du sac que le serveur accepte d'acheter.
//
// ⚠ ELLE NE DIT PAS DE QUEL OBJET IL S'AGIT : le serveur n'envoie que la case et
// deux prix. L'objet se lit dans le sac, à cette case — et c'est le serveur qui
// a décidé ce qui est vendable, filtres de ce serveur compris : la liste ne se
// déduit pas du sac.
struct NpcShopSellOffer {
  // La case du sac, telle que le client la voit : voir `InventoryItem`.
  std::uint16_t client_slot = 0;
  std::uint32_t base_price = 0;
  // 🔴 CE QUE LE SERVEUR CRÉDITERA, surcharge comprise.
  std::uint32_t price = 0;
};

// Ce que le dernier résultat a dit.
struct NpcShopOutcome {
  // ⚠ LA BOUTIQUE QUI L'A REÇU, même fermée depuis : c'est elle qui dit quel
  // message un code désigne — le même chiffre ne dit pas la même chose au
  // market et à la boutique à points.
  NpcShopKind kind = NpcShopKind::kNone;
  NpcShopSide side = NpcShopSide::kBuy;
  // Le code du serveur, tel quel. Voir `docs/commerces-protocole.md` §A.
  std::uint16_t result = kNpcShopDealSucceeded;
  // ⚠ ZÉRO VEUT DIRE « AUCUN RÉSULTAT DEPUIS L'OUVERTURE ».
  std::uint32_t seq = 0;
};

class NpcShop {
 public:
  // ── L'OUVERTURE ───────────────────────────────────────────────────────────
  [[nodiscard]] bool open() const noexcept { return kind_ != NpcShopKind::kNone; }
  [[nodiscard]] NpcShopKind kind() const noexcept { return kind_; }
  [[nodiscard]] NpcShopDeals deals() const noexcept { return deals_; }
  // Le PNJ boutique. ⚠ ZÉRO QUAND LE SERVEUR NE L'A PAS NOMMÉ — toute ouverture
  // par liste seule —, et c'est ce qui interdit alors de réarmer la boutique.
  [[nodiscard]] std::uint32_t npc_id() const noexcept { return npc_id_; }
  // Le rang de l'ouverture : il change à chaque ouverture neuve, et c'est lui
  // qui remet la fenêtre à zéro.
  [[nodiscard]] std::uint32_t opening_seq() const noexcept { return opening_seq_; }

  // LE CHOIX « ACHETER OU VENDRE » : la boutique à zeny, nommée. La fenêtre
  // s'ouvre sur l'achat, et c'est à `app` de demander la liste.
  void TellDealChoice(std::uint32_t npc_id);
  // LA LISTE D'ACHAT. Sans choix préalable, elle OUVRE une boutique d'achat
  // seul — le `callshop …,1` d'un script.
  void TellBuyList(std::span<const NpcShopOffer> offers);
  // LA LISTE DE VENTE. Sans choix préalable, elle ouvre une boutique de vente
  // seule.
  void TellSellList(std::span<const NpcShopSellOffer> offers);
  // LE MARKET. ⚠ CHAQUE LISTE ROUVRE : le serveur n'en envoie qu'à l'ouverture.
  void TellMarket(std::span<const NpcShopOffer> offers);
  // LA BOUTIQUE À POINTS : sa liste, et ce que le joueur possède de sa monnaie
  // — et de la seconde, pour la boutique cash d'un PNJ.
  void TellPointShop(std::span<const NpcShopOffer> offers, std::uint32_t points,
                     std::uint32_t secondary_points);

  // ── LES LISTES ────────────────────────────────────────────────────────────
  [[nodiscard]] std::span<const NpcShopOffer> buy_offers() const noexcept { return buy_offers_; }
  [[nodiscard]] std::span<const NpcShopSellOffer> sell_offers() const noexcept {
    return sell_offers_;
  }
  // Les rangs des deux listes, qui changent à chaque liste reçue. Zéro : pas
  // encore reçue depuis l'ouverture.
  [[nodiscard]] std::uint32_t buy_seq() const noexcept { return buy_seq_; }
  [[nodiscard]] std::uint32_t sell_seq() const noexcept { return sell_seq_; }
  [[nodiscard]] std::uint32_t points() const noexcept { return points_; }
  [[nodiscard]] std::uint32_t secondary_points() const noexcept { return secondary_points_; }

  // ── LES DEMANDES ──────────────────────────────────────────────────────────
  //
  // 🔴 LE SERVEUR OUBLIE LA BOUTIQUE APRÈS CHAQUE ACHAT OU VENTE. Un second
  // achat demande donc un nouveau choix d'achat : c'est ce que dit `armed`, et
  // `net` le réarme avant d'émettre — ce qui n'est possible que pour une
  // boutique NOMMÉE. Ouverte par une liste seule, une boutique à zeny ne sert
  // qu'UNE transaction : `spent` la ferme au résultat.
  //
  // ⚠ C'EST LA LISTE REÇUE QUI ARME, pas le choix envoyé : le serveur ne retient
  // la boutique qu'en répondant par sa liste.
  [[nodiscard]] bool armed() const noexcept { return armed_; }
  // Une transaction vient de partir : plus d'autre avant son résultat, sans
  // quoi deux demandes se croiseraient sur une boutique désarmée.
  void NoteDealSent() noexcept;
  // ⚠ SURVIT À LA FERMETURE : un résultat qui arrive boutique fermée se lit
  // encore, et le joueur qui ferme aussitôt après un achat en voit l'issue.
  [[nodiscard]] bool awaiting_result() const noexcept { return awaiting_result_; }
  // Une boutique ouverte par liste seule ne sert qu'une fois. Voir `armed`.
  [[nodiscard]] bool single_use() const noexcept {
    return npc_id_ == 0 && kind_ == NpcShopKind::kZeny;
  }

  // ── LES RÉSULTATS ─────────────────────────────────────────────────────────
  //
  // Achat à zeny, vente, achat au market, achat à points. ⚠ UNE BOUTIQUE À
  // USAGE UNIQUE SE FERME AU RÉSULTAT, réussi ou non : le serveur l'a déjà
  // oubliée, et il n'y a plus rien à demander.
  void TellBuyResult(std::uint8_t result);
  void TellSellResult(std::uint8_t result);
  // ⚠ LE MARKET NE DIT QU'« ÉCHEC » (0xFFFF) OU « RÉUSSI » AVEC CE QUI A ÉTÉ
  // ACHETÉ : c'est au client de retirer ces quantités de son stock.
  struct MarketBought {
    std::uint32_t item_id = 0;
    std::uint16_t amount = 0;
  };
  void TellMarketResult(std::uint16_t result, std::span<const MarketBought> bought);
  void TellPointShopResult(std::uint16_t result, std::uint32_t points,
                           std::uint32_t secondary_points);
  [[nodiscard]] const NpcShopOutcome& outcome() const noexcept { return outcome_; }

  // ── LA FERMETURE ──────────────────────────────────────────────────────────
  //
  // Le joueur ferme. La fermeture se dit au serveur : voir `TakeFarewell`.
  void Close() noexcept;
  // Le personnage change de carte sur le même serveur : la boutique se ferme,
  // et ⚠ LA FERMETURE SE DIT QUAND MÊME. Le serveur ne désarme pas l'état
  // d'échange d'un market au changement de carte ; le mot de congé ne coûte
  // rien aux autres boutiques et leur garantit la marche rendue.
  void CloseForMapChange() noexcept;
  // Le personnage change de serveur : la session repart de zéro de l'autre
  // côté, et il n'y a plus personne à qui dire au revoir.
  void CloseSilently() noexcept;

  // Le paquet de fermeture dû, UNE fois après chaque fermeture qui doit se
  // dire au serveur ; `kNone` sinon.
  [[nodiscard]] NpcShopFarewell TakeFarewell() noexcept;

 private:
  void Open(NpcShopKind kind, NpcShopDeals deals, std::uint32_t npc_id);
  void RecordOutcome(NpcShopSide side, std::uint16_t result);
  // Cette sorte de boutique est ouverte, ou une de ses transactions attend
  // encore son résultat.
  [[nodiscard]] bool Awaits(NpcShopKind kind) const noexcept;

  NpcShopKind kind_ = NpcShopKind::kNone;
  NpcShopDeals deals_ = NpcShopDeals::kBuyAndSell;
  std::uint32_t npc_id_ = 0;
  std::uint32_t opening_seq_ = 0;
  std::vector<NpcShopOffer> buy_offers_;
  std::vector<NpcShopSellOffer> sell_offers_;
  std::uint32_t buy_seq_ = 0;
  std::uint32_t sell_seq_ = 0;
  std::uint32_t points_ = 0;
  std::uint32_t secondary_points_ = 0;
  bool armed_ = false;
  bool awaiting_result_ = false;
  // La sorte de boutique dont une transaction est partie : son résultat se
  // reconnaît encore après la fermeture.
  NpcShopKind awaited_kind_ = NpcShopKind::kNone;
  NpcShopOutcome outcome_;
  // Le rang des résultats, qui ne recule jamais : `outcome_.seq` le recopie.
  std::uint32_t outcome_count_ = 0;
  NpcShopFarewell farewell_due_ = NpcShopFarewell::kNone;
};

}  // namespace ro::sim
