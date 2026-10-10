#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/sim/inventory.h"

namespace ro::sim {

// L'ÉCHANGE ENTRE JOUEURS : une proposition, puis deux offres qui se
// verrouillent, puis deux validations.
//
// 🔴 LE SERVEUR TIENT CERTAINES ERREURS DU CLIENT POUR DE LA TRICHE. Au moment
// d'exécuter, il recompte le sac de chacun (`impossible_trade_check`) : un item
// ÉQUIPÉ, ou une pile qui a fondu depuis son dépôt — une gemme consommée par une
// compétence, une flèche tirée —, fait de l'offre « plus que ce que le joueur
// possède ». Il prévient alors les maîtres du jeu, et `ban_hack_trade` BANNIT le
// compte pour autant de minutes qu'il en vaut. Un zeny offert qui a fondu pose le
// drapeau `ZENY_HACKER` sur le personnage. Ces erreurs-là, le client doit les
// rendre IMPOSSIBLES : c'est le rôle des verdicts de ce fichier, et de ceux
// que les autres gestes consultent pendant un échange (`DealEngages`,
// `DealForbidsSkills`, `DealForbidsZenySpending`).
//
// 🔴 CE QUI EST DÉPOSÉ QUITTE LE SAC À L'ACCUSÉ, et c'est le protocole qui
// l'exige, pas l'affichage : à l'annulation, le serveur RENVOIE chaque dépôt
// comme un gain ; à l'exécution, il retire les items SANS LE DIRE. Ne pas les
// retirer à l'accusé les doublerait dans un cas et les laisserait en fantôme
// dans l'autre. `TellAddResult` rend donc ce qui doit sortir du sac.
//
// ⚠ L'ACCUSÉ D'UN DÉPÔT NE PORTE QUE LA CASE : la quantité n'est connue que de
// nous, d'où la file des dépôts en route. Et le serveur se TAIT sur plusieurs
// refus — quantité au-delà de la pile, onzième item, œuf éclos, item rangé
// dans le changement d'équipement — : un dépôt sans réponse se relâche au bout
// de `kDealAddSilenceMs`.
//
// ⚠ LE ZENY N'A PAS D'ACCUSÉ. Le montant est ABSOLU, et le serveur l'accepte
// ou ANNULE tout l'échange — au-delà de la bourse, ou de ce que celle du
// partenaire peut encore recevoir. Le partenaire, lui, le voit aussitôt.

// Dix items distincts au plus de chaque côté.
inline constexpr std::size_t kDealMostItems = 10;

// La plus grande quantité d'un dépôt : `MAX_AMOUNT`. ⚠ LE SERVEUR LA LIT SUR
// SEIZE BITS SIGNÉS, quoique le fil en porte trente-deux : au-delà de 32 767,
// elle devient négative et le dépôt est jeté sans réponse.
inline constexpr std::int32_t kDealMostItemAmount = 30000;

// Ce que la bourse peut tenir : `MAX_ZENY`.
inline constexpr std::int64_t kDealMostZeny = 2147483647;

// La distance, en cases, au-delà de laquelle le serveur refuse de proposer ou
// d'accepter (`TRADE_DISTANCE`), mesurée sur chaque axe.
inline constexpr int kDealReachCells = 2;

// Le silence au bout duquel un dépôt sans accusé est tenu pour refusé.
inline constexpr std::uint32_t kDealAddSilenceMs = 3000;

// ── LES RÉPONSES À UNE PROPOSITION (`e_ack_trade_response`) ─────────────────
inline constexpr std::uint8_t kDealAnswerTooFar = 0;
inline constexpr std::uint8_t kDealAnswerNoCharacter = 1;
inline constexpr std::uint8_t kDealAnswerFailed = 2;
inline constexpr std::uint8_t kDealAnswerAccepted = 3;
inline constexpr std::uint8_t kDealAnswerCancelled = 4;
inline constexpr std::uint8_t kDealAnswerBusy = 5;

// ── LES ACCUSÉS D'UN DÉPÔT (`e_exitem_add_result`) ─────────────────────────
inline constexpr std::uint8_t kDealAddSucceeded = 0;
inline constexpr std::uint8_t kDealAddOverWeight = 1;
// Refus de l'item lui-même : non échangeable, loué, lié.
inline constexpr std::uint8_t kDealAddRefused = 2;
inline constexpr std::uint8_t kDealAddBagFull = 3;
inline constexpr std::uint8_t kDealAddStackOverflow = 4;
// Le nôtre, jamais sur le fil : le serveur n'a rien répondu.
inline constexpr std::uint8_t kDealAddSilent = 0xFF;

// LA CASE QUE L'ACCUSÉ DU VERROU PORTE : zéro, qui n'est la case d'aucun item.
inline constexpr std::uint16_t kDealZenySlot = 0;

enum class DealPhase : std::uint8_t {
  kNone,
  // J'ai proposé ; la réponse n'est pas venue.
  kProposing,
  // On me propose ; je n'ai pas répondu.
  kProposed,
  // Les deux fenêtres sont ouvertes.
  kOpen,
};

struct DealPartner {
  std::uint32_t account_id = 0;
  std::string name;
  std::uint16_t base_level = 0;
};

// UNE LIGNE D'OFFRE : l'item, `amount` étant ce qui est offert, pas la pile.
struct DealLine {
  InventoryItem item;
  // Arrivée ou grossie APRÈS mon verrou : c'est là qu'une offre peut changer
  // sous mes yeux, et la fenêtre la signale.
  bool changed_after_my_lock = false;
};

// UN DÉPÔT EN ROUTE : la case, la quantité, et l'item tel qu'il était.
struct DealPendingAdd {
  InventoryItem item;
  std::int16_t amount = 0;
  std::uint32_t sent_ms = 0;
};

// Ce que l'accusé d'un dépôt retire du sac.
struct DealTaken {
  std::uint16_t client_slot = 0;
  std::int16_t amount = 0;
};

// CE QUE LA FENÊTRE ET LA CONVERSATION DOIVENT DIRE, une fois chacun.
enum class DealNewsKind : std::uint8_t {
  // La proposition n'a pas abouti : `code` est la réponse du serveur.
  kAnswered,
  // Un dépôt refusé : `code` est l'accusé, `item` ce qui était déposé.
  kAddRefused,
  kPartnerLocked,
  // Le partenaire a changé son offre après mon verrou.
  kPartnerChangedAfterMyLock,
  kCancelled,
  // Annulé alors que j'avais validé : le serveur a refusé l'exécution — sac
  // plein, poids, bourse pleine chez l'un des deux.
  kRefusedAtExecution,
  kExecuted,
};

struct DealNews {
  std::uint32_t seq = 0;
  DealNewsKind kind = DealNewsKind::kCancelled;
  std::uint8_t code = 0;
  InventoryItem item;
};

// Les nouvelles gardées : plus qu'une session n'en produit entre deux images.
inline constexpr std::size_t kDealNewsKept = 32;

// POURQUOI UN DÉPÔT NE PARTIRAIT PAS.
enum class DealAddFault : std::uint8_t {
  kNone,
  kNotOpen,
  kLocked,
  kBadAmount,
  // 🔴 LE BANNISSEMENT : un item porté compte pour zéro au recompte du serveur.
  // Les munitions portées échappent à son recompte, pas au tir qui les
  // consomme : elles se déséquipent d'abord, elles aussi.
  kEquipped,
  // Le serveur le refuse.
  kRented,
  // Le serveur le jette sans réponse.
  kHatchedEgg,
  // Tout ce que la pile contient est déjà offert, ou en route.
  kAllOffered,
  // Dix items distincts déjà offerts : le serveur jette le onzième sans réponse.
  kOfferFull,
};

// POURQUOI UN MONTANT DE ZENY NE PARTIRAIT PAS. Tous ANNULERAIENT l'échange.
enum class DealZenyFault : std::uint8_t {
  kNone,
  kNotOpen,
  kLocked,
  kNegative,
  kAbovePurse,
  kAboveMost,
};

// POURQUOI LA VALIDATION NE PARTIRAIT PAS.
enum class DealExecuteFault : std::uint8_t {
  kNone,
  kNotOpen,
  kNotBothLocked,
  kAlreadySent,
  // Un dépôt est encore en route : ce que je verrais n'est pas ce qui part.
  kAddInFlight,
  // L'offre du partenaire a changé depuis le récapitulatif confirmé.
  kPartnerChanged,
  // Le zeny offert dépasse ce que la bourse contient À PRÉSENT : le serveur
  // poserait `ZENY_HACKER`.
  kZenyMelted,
};

class Deal {
 public:
  [[nodiscard]] DealPhase phase() const noexcept { return phase_; }
  [[nodiscard]] bool open() const noexcept { return phase_ == DealPhase::kOpen; }
  [[nodiscard]] const DealPartner& partner() const noexcept { return partner_; }
  // Le rang de l'ouverture : une ouverture neuve remet la fenêtre à zéro.
  [[nodiscard]] std::uint32_t opening_seq() const noexcept { return opening_seq_; }
  // Le rang de la proposition reçue : une neuve rouvre la boîte.
  [[nodiscard]] std::uint32_t proposal_seq() const noexcept { return proposal_seq_; }
  // L'instant où ma proposition est partie.
  [[nodiscard]] std::uint32_t proposal_sent_ms() const noexcept { return proposal_sent_ms_; }

  // ── LA PROPOSITION ───────────────────────────────────────────────────────

  // Ma proposition part. ⚠ LE SERVEUR ANNULE TOUTE SESSION EN COURS du
  // proposant : le modèle repart de zéro.
  void NoteProposalSent(DealPartner partner, std::uint32_t now_ms);
  // Un joueur me propose un échange. Remplace une proposition précédente.
  void TellProposal(std::string name, std::uint32_t account_id, std::uint16_t base_level);
  // Ma réponse part. Un refus ferme aussitôt : la réponse du serveur, qui
  // suivra, n'aura rien à dire.
  void NoteAnswerSent(bool accept);
  [[nodiscard]] bool answer_sent() const noexcept { return answer_sent_; }
  // La réponse du serveur : `kDealAnswerAccepted` OUVRE l'échange, chez les
  // deux joueurs ; tout autre code le ferme.
  void TellAnswer(std::uint8_t code, std::uint32_t partner_aid, std::uint16_t partner_level);

  // ── MON OFFRE ────────────────────────────────────────────────────────────

  // Combien de cette pile peut encore partir : ce que le sac en montre, moins
  // ce qui est en route, au plus `kDealMostItemAmount`.
  [[nodiscard]] std::int32_t MostAddable(const InventoryItem& bag_item) const noexcept;
  [[nodiscard]] DealAddFault CheckAdd(const InventoryItem& bag_item,
                                      std::int32_t amount) const noexcept;
  // Le dépôt part, BORNÉ par l'appelant à `MostAddable`.
  void NoteAddSent(const InventoryItem& bag_item, std::int16_t amount, std::uint32_t now_ms);
  // L'accusé d'un dépôt. Au succès, rend ce que le sac doit perdre. ⚠ L'ACCUSÉ
  // DE LA CASE ZÉRO EST CELUI DU VERROU, ou d'un dépôt nul : il ne vise rien.
  [[nodiscard]] std::optional<DealTaken> TellAddResult(std::uint16_t client_slot,
                                                       std::uint8_t code);
  // Relâche les dépôts restés sans réponse depuis `kDealAddSilenceMs` ; en
  // rend le nombre.
  std::size_t ExpireSilentAdds(std::uint32_t now_ms);

  [[nodiscard]] DealZenyFault CheckZeny(std::int64_t amount, std::int64_t purse) const noexcept;
  void NoteZenySent(std::int64_t amount) noexcept;

  // ── LE PARTENAIRE ───────────────────────────────────────────────────────

  // Un dépôt du partenaire, en DELTA : le serveur envoie ce qui s'ajoute, pas
  // le total. Les piles identiques se fondent — l'équipement jamais, chaque
  // pièce est un item.
  void TellPartnerItem(InventoryItem item);
  void TellPartnerZeny(std::int64_t amount);

  // ── LES VERROUS ET LA FIN ───────────────────────────────────────────────

  void NoteLockSent() noexcept;
  [[nodiscard]] bool lock_sent() const noexcept { return lock_sent_; }
  // `partner` : c'est le verrou de l'autre.
  void TellLocked(bool partner);
  // ⚠ L'APPELANT A VÉRIFIÉ `CheckExecute` : la validation est sans retour.
  void NoteExecuteSent() noexcept;
  [[nodiscard]] DealExecuteFault CheckExecute(std::uint32_t confirmed_partner_revision,
                                              std::int64_t purse) const noexcept;

  // Mon annulation part : le modèle ferme, l'annulation du serveur suivra.
  void NoteCancelSent();
  void TellCancelled();
  void TellExecuted(std::uint8_t result);
  // Un changement de carte : le serveur a annulé de lui-même.
  void CloseForMapChange();

  // ── LES LECTURES ────────────────────────────────────────────────────────

  [[nodiscard]] std::span<const DealLine> my_offer() const noexcept { return my_offer_; }
  [[nodiscard]] std::span<const DealLine> partner_offer() const noexcept { return partner_offer_; }
  [[nodiscard]] std::span<const DealPendingAdd> pending_adds() const noexcept {
    return pending_adds_;
  }
  [[nodiscard]] std::int64_t my_zeny() const noexcept { return my_zeny_; }
  [[nodiscard]] std::int64_t partner_zeny() const noexcept { return partner_zeny_; }
  [[nodiscard]] bool partner_zeny_changed_after_my_lock() const noexcept {
    return partner_zeny_changed_after_my_lock_;
  }
  [[nodiscard]] bool my_locked() const noexcept { return my_locked_; }
  [[nodiscard]] bool partner_locked() const noexcept { return partner_locked_; }
  [[nodiscard]] bool execute_sent() const noexcept { return execute_sent_; }
  // Avance à chaque changement de l'offre du partenaire : la validation
  // confirmée porte le rang qu'elle a vu.
  [[nodiscard]] std::uint32_t partner_revision() const noexcept { return partner_revision_; }
  // L'offre du partenaire a changé depuis que j'ai verrouillé la mienne.
  [[nodiscard]] bool partner_changed_after_my_lock() const noexcept;

  // Combien de cette case est en route.
  [[nodiscard]] std::int32_t InFlightOf(std::uint16_t client_slot) const noexcept;
  // La case est-elle dans mon offre, ou en route vers elle ?
  [[nodiscard]] bool Engages(std::uint16_t client_slot) const noexcept;
  // Ai-je quelque chose en jeu : un item offert ou en route, du zeny ?
  [[nodiscard]] bool HasStake() const noexcept;

  [[nodiscard]] std::span<const DealNews> news() const noexcept { return news_; }
  [[nodiscard]] std::uint32_t last_news_seq() const noexcept { return news_seq_; }

 private:
  void Reset();
  void Close();
  void Tell(DealNewsKind kind, std::uint8_t code = 0, const InventoryItem* item = nullptr);
  void NotePartnerChange();
  [[nodiscard]] std::size_t DistinctOffered() const noexcept;

  DealPhase phase_ = DealPhase::kNone;
  DealPartner partner_;
  std::uint32_t opening_seq_ = 0;
  std::uint32_t proposal_seq_ = 0;
  std::uint32_t proposal_sent_ms_ = 0;
  bool answer_sent_ = false;

  std::vector<DealLine> my_offer_;
  std::vector<DealLine> partner_offer_;
  std::vector<DealPendingAdd> pending_adds_;
  // ⚠ UN DÉPÔT TENU POUR MUET RESTE ICI : un accusé tardif doit encore retirer
  // l'item du sac, sans quoi l'annulation le doublerait.
  std::vector<DealPendingAdd> silent_adds_;
  std::int64_t my_zeny_ = 0;
  std::int64_t partner_zeny_ = 0;
  bool partner_zeny_changed_after_my_lock_ = false;
  bool lock_sent_ = false;
  bool my_locked_ = false;
  bool partner_locked_ = false;
  bool execute_sent_ = false;
  std::uint32_t partner_revision_ = 0;
  std::uint32_t partner_revision_at_my_lock_ = 0;

  std::vector<DealNews> news_;
  std::uint32_t news_seq_ = 0;
};

// ── CE QUE LES AUTRES GESTES CONSULTENT PENDANT UN ÉCHANGE ─────────────────

// LES DEUX JOUEURS SONT-ILS À PORTÉE ? Sur chaque axe, comme le serveur.
[[nodiscard]] bool DealReachable(int my_x, int my_y, int partner_x, int partner_y) noexcept;

// L'item de cette case peut-il être utilisé, équipé, jeté ou rangé ? Non s'il
// est offert ou en route : sa pile fondrait sous l'offre.
[[nodiscard]] bool DealEngages(const Deal& deal, std::uint16_t client_slot) noexcept;

// 🔴 UNE COMPÉTENCE PEUT CONSOMMER UN ITEM OU DU ZENY — une gemme, une pièce
// de Mammonite — sans que le client le sache d'avance. Tant que j'ai quelque
// chose en jeu, aucune ne part.
[[nodiscard]] bool DealForbidsSkills(const Deal& deal) noexcept;

// Une dépense de zeny — achat chez un marchand, dépôt en banque — ferait
// fondre le zeny offert.
[[nodiscard]] bool DealForbidsZenySpending(const Deal& deal) noexcept;

}  // namespace ro::sim
