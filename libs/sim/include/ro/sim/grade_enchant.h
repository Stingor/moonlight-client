#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace ro::sim {

class Inventory;
struct InventoryItem;

// L'AMÉLIORATION DE GRADE : un PNJ ouvre une fenêtre où une arme du sac monte d'un
// grade — D, C, B puis A — contre un joyau et des zenys, avec une chance que de la
// poussière bénie relève.
//
// 🔴 LE SERVEUR DIT CE QU'UNE PIÈCE COÛTE, PAS CE QUI SE CHOISIT : la pièce choisie
// part sur le fil (`TakeSelection`), et c'est sa réponse (`TellOffer`) qui porte la
// chance, la poussière et les deux mises possibles.
//
// 🔴 SES REFUS SONT MUETS : une pièce qu'il n'admet pas, une demande qu'il rejette,
// ne reçoivent aucune réponse. D'où le tri des pièces (`JudgeGradePiece`) et le
// jugement complet AVANT l'envoi (`CheckGradeEnchantRequest`).
//
// 🔴 UNE RÉUSSITE RAMÈNE LE RAFFINEMENT À ZÉRO, et le serveur ne renvoie pas la
// pièce : le sac se corrige depuis le résultat (`ro::net`).
//
// 🔴 TOUTE FERMETURE DU JOUEUR SE DIT AU SERVEUR, sans quoi il garde la fenêtre
// pour ouverte. Un changement de carte ferme en silence.

// Le type d'interface que le serveur ouvre pour les grades (`OUT_UI_ENCHANTGRADE`).
inline constexpr std::int8_t kGradeEnchantUiType = 8;

// Le type d'une arme sur le fil (`IT_WEAPON`). ⚠ PAS LA FAMILLE DU SAC, qui range
// d'autres types avec les armes.
inline constexpr std::uint8_t kGradeWeaponType = 5;

// Les grades : aucun, D, C, B, A. A est le dernier.
inline constexpr std::uint8_t kGradeNone = 0;
inline constexpr std::uint8_t kGradeTop = 4;

// Ce que le serveur répond à une tentative.
enum class GradeEnchantResult : std::uint8_t {
  kSucceeded = 0,
  kFailed = 1,
  kDowngraded = 2,
  kBroken = 3,
  kProtected = 4,
};

// UNE MISE : le joyau, sa quantité, le prix, et ce qu'un échec coûte.
struct GradeEnchantStake {
  std::uint32_t item_id = 0;
  std::uint32_t amount = 0;
  std::int64_t zeny = 0;
  bool downgrades = false;
  bool breaks = false;
};

// LA POUSSIÈRE QUI RELÈVE LA CHANCE : tant par dose, au plus tant de doses, et ce
// que chaque dose ajoute, en points de pourcentage. `item_id` nul : aucune.
struct GradeEnchantBlessing {
  std::uint32_t item_id = 0;
  std::uint32_t per_step = 0;
  std::uint32_t most_steps = 0;
  std::uint32_t bonus_percent = 0;
};

// CE QUE LE SERVEUR PROPOSE POUR LA PIÈCE CHOISIE.
struct GradeEnchantOffer {
  std::uint16_t client_slot = 0;
  // Faux : le serveur ne gère pas ce niveau d'arme.
  bool supported = false;
  // La chance de base, en pourcentage ; zéro sous le raffinement exigé.
  std::uint32_t chance_percent = 0;
  GradeEnchantBlessing blessing;
  std::vector<GradeEnchantStake> stakes;
};

// POURQUOI UNE PIÈCE OU UNE DEMANDE NE PART PAS.
enum class GradeEnchantRefusal : std::uint8_t {
  kNone,
  kNotOpen,
  // Une demande attend déjà sa réponse.
  kWaiting,
  kNothingChosen,
  // La pièce choisie a quitté le sac entre-temps.
  kPieceGone,
  // Ce n'est pas une arme : seules les armes prennent un grade.
  kNotGradable,
  kWorn,
  kUnidentified,
  kTopGrade,
  // La proposition du serveur n'est pas encore arrivée.
  kAwaitingOffer,
  kLevelUnsupported,
  // Sous le raffinement exigé : la chance du serveur est nulle.
  kRefineTooLow,
  kNoSuchStake,
  kMissingStake,
  kMissingBlessing,
  kNotEnoughZeny,
};

// Ce que le serveur dirait de cette pièce, mises mises à part.
[[nodiscard]] GradeEnchantRefusal JudgeGradePiece(const InventoryItem& item);

// La chance d'une tentative, en pourcentage, doses de poussière comprises.
[[nodiscard]] std::uint32_t GradeEnchantChance(const GradeEnchantOffer& offer,
                                               std::uint32_t blessing_steps) noexcept;

class GradeEnchant {
 public:
  [[nodiscard]] bool open() const noexcept { return open_; }
  // Change à chaque ouverture : c'est lui qui remet la fenêtre à zéro.
  [[nodiscard]] std::uint32_t opening_seq() const noexcept { return opening_seq_; }

  // Le paquet du serveur. Une ouverture par-dessus une autre la remplace.
  void TellOpening();

  // ── LA PIÈCE CHOISIE, PAR CASE DU SAC ─────────────────────────────────────
  [[nodiscard]] std::optional<std::uint16_t> chosen_slot() const noexcept { return chosen_; }
  // Change à chaque choix et à chaque proposition : une confirmation s'y rapporte.
  [[nodiscard]] std::uint32_t choice_seq() const noexcept { return choice_seq_; }
  // Faux si la fenêtre est fermée ou en attente. La proposition précédente tombe.
  bool Choose(std::uint16_t client_slot);
  // Le choix tombe : la pièce a quitté le sac.
  void Forget() noexcept;
  // La case à demander au serveur, une fois par choix.
  [[nodiscard]] std::optional<std::uint16_t> TakeSelection() noexcept;

  // ── LA PROPOSITION ────────────────────────────────────────────────────────
  [[nodiscard]] const std::optional<GradeEnchantOffer>& offer() const noexcept { return offer_; }
  // Celle d'une autre pièce que la choisie est périmée, et s'oublie.
  void TellOffer(GradeEnchantOffer offer);

  // ── LA DEMANDE ET SA RÉPONSE ──────────────────────────────────────────────
  [[nodiscard]] bool waiting() const noexcept { return waiting_; }
  void MarkRequestSent() noexcept { waiting_ = true; }
  // Le résultat. La pièce change ou disparaît : sa proposition se redemande.
  void TellResult(GradeEnchantResult result);
  [[nodiscard]] GradeEnchantResult result() const noexcept { return result_; }
  // ⚠ ZÉRO VEUT DIRE « AUCUN RÉSULTAT ENCORE ».
  [[nodiscard]] std::uint32_t result_seq() const noexcept { return result_seq_; }

  // ── LA FERMETURE ──────────────────────────────────────────────────────────
  // Le joueur ferme : la fermeture se dit au serveur.
  void Close();
  // Changement de carte ou de serveur : RIEN À DIRE.
  void CloseSilently() noexcept;
  // Vrai une fois quand une fermeture est due.
  [[nodiscard]] bool TakeFarewell() noexcept;

 private:
  void Reset() noexcept;

  bool open_ = false;
  std::uint32_t opening_seq_ = 0;
  std::optional<std::uint16_t> chosen_;
  std::uint32_t choice_seq_ = 0;
  bool selection_due_ = false;
  std::optional<GradeEnchantOffer> offer_;
  bool waiting_ = false;
  GradeEnchantResult result_ = GradeEnchantResult::kSucceeded;
  std::uint32_t result_seq_ = 0;
  bool farewell_due_ = false;
};

// LA TENTATIVE QUE LE JOUEUR VEUT : quelle mise, combien de doses de poussière.
struct GradeEnchantAttempt {
  std::uint32_t stake = 0;
  std::uint32_t blessing_steps = 0;
};

// Ce qui retient la demande, et l'objet en cause.
struct GradeEnchantCheck {
  GradeEnchantRefusal refusal = GradeEnchantRefusal::kNone;
  std::uint32_t item_id = 0;

  [[nodiscard]] bool passed() const noexcept { return refusal == GradeEnchantRefusal::kNone; }
};

// Toute la demande, contre le sac et la bourse : la garde du verbe qui l'envoie.
// ⚠ LE SERVEUR NE COMPTE QUE LA PREMIÈRE PILE de chaque objet (`CountedMaterial`).
[[nodiscard]] GradeEnchantCheck CheckGradeEnchantRequest(const GradeEnchant& grade,
                                                         GradeEnchantAttempt attempt,
                                                         const Inventory& inventory,
                                                         std::int64_t zeny);

}  // namespace ro::sim
