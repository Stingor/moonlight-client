#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/sim/inventory.h"

namespace ro::sim {

// L'ENCHANTEMENT : un PNJ ouvre une fenêtre où une pièce du sac reçoit, dans une case
// libre au-delà de ses cases de carte, un enchantement — tiré au sort ou choisi —,
// voit le sien monter d'un cran, ou les perd tous.
//
// 🔴 LE SERVEUR N'ENVOIE QUE LE NUMÉRO DU GROUPE : les pièces admises, l'ordre des
// cases, les coûts et les chances viennent d'une table du client
// (`ItemEnchantGroup`). Sans elle, rien ne se peut montrer.
//
// 🔴 SES REFUS SONT MUETS : une demande qu'il rejette ne reçoit aucune réponse. D'où
// le jugement complet AVANT l'envoi (`CheckItemEnchantRequest`).
//
// 🔴 CHAQUE RÉPONSE CLÔT LA SÉANCE : le serveur oublie le groupe ouvert dès qu'il a
// répondu, et toute demande suivante tomberait dans le vide. La fenêtre se ferme ;
// seul le PNJ la rouvre.
//
// 🔴 LE SERVEUR NE RENVOIE PAS LA PIÈCE : il pose l'enchantement dans la case et dit
// lequel. Le sac se corrige depuis la demande et la réponse (`EnchantedCards`).

// Le type d'interface que le serveur ouvre pour l'enchantement (`OUT_UI_ENCHANT`).
inline constexpr std::uint8_t kItemEnchantUiType = 10;

// Les chances du serveur, en cent-millièmes : celle-ci est la certitude.
inline constexpr std::uint32_t kItemEnchantCertain = 100000;

// Ce que le serveur répond (`MSI_ENCHANT_SUCCESS`, `MSI_ENCHANT_FAILED`).
inline constexpr std::int32_t kItemEnchantSucceededMessage = 3857;
inline constexpr std::int32_t kItemEnchantFailedMessage = 3858;

struct ItemEnchantMaterial {
  std::uint32_t item_id = 0;
  std::uint16_t amount = 1;
};

// CE QU'UNE OPÉRATION COÛTE : des zenys, et des matériaux.
struct ItemEnchantCost {
  std::int64_t zeny = 0;
  std::vector<ItemEnchantMaterial> materials;
};

// UN ENCHANTEMENT QU'UN TIRAGE PEUT POSER, et la probabilité qu'il le pose, en
// cent-millièmes — celle que le serveur tire, pas la chance écrite dans sa table.
struct ItemEnchantDraw {
  std::uint32_t item_id = 0;
  std::uint32_t odds = 0;
};

// CE QU'UN TIRAGE POSE SUR UNE PIÈCE DE CE GRADE.
struct ItemEnchantDrawGrade {
  std::uint8_t grade = 0;
  std::vector<ItemEnchantDraw> enchants;
};

// UN ENCHANTEMENT QUE LE JOUEUR CHOISIT, sûr, et son prix.
struct ItemEnchantPerfect {
  std::uint32_t item_id = 0;
  ItemEnchantCost cost;
};

// CE QU'UN ENCHANTEMENT DÉJÀ POSÉ DEVIENT, et ce qu'il en coûte.
struct ItemEnchantUpgrade {
  std::uint32_t from_item_id = 0;
  std::uint32_t to_item_id = 0;
  ItemEnchantCost cost;
};

// UNE CASE DU GROUPE : son tirage, ses choix sûrs, ses améliorations.
struct ItemEnchantSlot {
  std::uint8_t slot = 0;
  // Le tirage coûte ceci, et réussit avec cette chance.
  ItemEnchantCost cost;
  std::uint32_t chance = kItemEnchantCertain;
  std::vector<ItemEnchantDrawGrade> draws;
  std::vector<ItemEnchantPerfect> perfect;
  std::vector<ItemEnchantUpgrade> upgrades;

  // Ce qu'un tirage pose sur une pièce de ce grade, nul si rien : le serveur refuse.
  [[nodiscard]] const ItemEnchantDrawGrade* DrawsFor(std::uint8_t grade) const noexcept;
  [[nodiscard]] const ItemEnchantPerfect* FindPerfect(std::uint32_t item_id) const noexcept;
  [[nodiscard]] const ItemEnchantUpgrade* FindUpgrade(std::uint32_t from_item_id) const noexcept;
};

// LA REMISE À ZÉRO : elle ôte tous les enchantements de la pièce, avec cette chance.
struct ItemEnchantReset {
  std::uint32_t chance = 0;
  ItemEnchantCost cost;
};

// UN GROUPE : le numéro que le serveur envoie, et tout ce qu'il admet.
struct ItemEnchantGroup {
  std::uint64_t group_id = 0;
  std::vector<std::uint32_t> target_item_ids;
  std::uint8_t least_refine = 0;
  std::uint8_t least_grade = 0;
  // ⚠ ADMISES PAR DÉFAUT : une pièce aux options aléatoires ne s'écarte que si le
  // groupe le dit.
  bool random_options_allowed = true;
  // L'ordre où les cases se remplissent : la première libre reçoit le tirage.
  std::vector<std::uint8_t> order;
  std::optional<ItemEnchantReset> reset;
  std::vector<ItemEnchantSlot> slots;

  [[nodiscard]] bool Targets(std::uint32_t item_id) const noexcept;
  [[nodiscard]] const ItemEnchantSlot* FindSlot(std::uint8_t slot) const noexcept;
};

// CE QUE LE JOUEUR DEMANDE.
enum class ItemEnchantMode : std::uint8_t {
  // Un enchantement tiré au sort dans la première case libre.
  kRandom,
  // L'enchantement choisi, dans la première case libre.
  kPerfect,
  // L'enchantement d'une case monte d'un cran.
  kUpgrade,
  // Tous les enchantements s'en vont.
  kReset,
};

// POURQUOI UNE PIÈCE OU UNE DEMANDE NE PART PAS.
enum class ItemEnchantRefusal : std::uint8_t {
  kNone,
  kNotOpen,
  // Une demande attend déjà sa réponse.
  kWaiting,
  kNothingChosen,
  // La pièce choisie a quitté le sac entre-temps.
  kPieceGone,
  kNotTargeted,
  kWorn,
  kDamaged,
  kRefineTooLow,
  kGradeTooLow,
  kRandomOptionsForbidden,
  // Toutes les cases de l'ordre sont prises.
  kNoFreeSlot,
  // La case n'a ni tirage, ni choix, ni amélioration pour cette demande.
  kSlotClosed,
  // Le groupe ne tire rien pour le grade de la pièce.
  kNoDrawForGrade,
  kNoSuchEnchant,
  // La case à améliorer est vide, ou son enchantement ne monte pas.
  kNothingToUpgrade,
  // Aucun enchantement à ôter, ou le groupe n'ôte rien.
  kNothingToReset,
  kMissingMaterial,
  kNotEnoughZeny,
};

// Ce que le serveur dirait de cette pièce, demande mise à part.
[[nodiscard]] ItemEnchantRefusal JudgeItemEnchantPiece(const ItemEnchantGroup& group,
                                                       const InventoryItem& item);

// La case que le tirage et le choix sûr rempliraient : la première de l'ordre qui
// est libre. ⚠ LE SERVEUR REFUSE UNE CASE DE CARTE, celles qu'en compte la pièce
// (`card_slots`) : rien alors. Un objet forgé n'a pas de case libre.
[[nodiscard]] std::optional<std::uint8_t> NextEnchantSlot(const ItemEnchantGroup& group,
                                                          const InventoryItem& item,
                                                          std::uint8_t card_slots) noexcept;

// Les cases au-delà des cases de carte qui portent déjà un enchantement.
[[nodiscard]] std::vector<std::uint8_t> EnchantedSlots(const InventoryItem& item,
                                                       std::uint8_t card_slots);

// Le premier matériau qui manque, nul quand tout y est.
// ⚠ LE SERVEUR NE COMPTE QUE LA PREMIÈRE PILE (`CountedMaterial`).
[[nodiscard]] const ItemEnchantMaterial* MissingEnchantMaterial(const ItemEnchantCost& cost,
                                                                const Inventory& inventory);

// LA DEMANDE : quoi, sur quelle case (amélioration), et quel enchantement (choix sûr).
struct ItemEnchantRequest {
  ItemEnchantMode mode = ItemEnchantMode::kRandom;
  std::uint8_t slot = 0;
  std::uint32_t enchant_item_id = 0;
};

// La séance d'enchantement, telle que le client la tient.
class ItemEnchant {
 public:
  [[nodiscard]] bool open() const noexcept { return open_; }
  [[nodiscard]] std::uint64_t group_id() const noexcept { return group_id_; }
  // Change à chaque ouverture : c'est lui qui remet la fenêtre à zéro.
  [[nodiscard]] std::uint32_t opening_seq() const noexcept { return opening_seq_; }

  // Le paquet du serveur. Une ouverture par-dessus une autre la remplace.
  void TellOpening(std::uint64_t group_id);

  // ── LA PIÈCE CHOISIE, PAR CASE DU SAC ─────────────────────────────────────
  [[nodiscard]] std::optional<std::uint16_t> chosen_slot() const noexcept { return chosen_; }
  // Change à chaque choix : une confirmation se rapporte à un rang.
  [[nodiscard]] std::uint32_t choice_seq() const noexcept { return choice_seq_; }
  // Faux si la fenêtre est fermée ou en attente.
  bool Choose(std::uint16_t client_slot);
  // Le choix tombe : la pièce a quitté le sac.
  void Forget() noexcept;

  // ── LA DEMANDE ET SA RÉPONSE ──────────────────────────────────────────────
  [[nodiscard]] bool waiting() const noexcept { return pending_.has_value(); }
  // La demande partie, la case qu'elle vise et les cases de carte de la pièce : la
  // réponse ne redit ni la case ni la demande.
  void MarkRequestSent(ItemEnchantRequest request, std::uint8_t target_slot,
                       std::uint8_t card_slots) noexcept;
  // La réponse : le mot du serveur et l'enchantement posé. Elle clôt la séance —
  // le serveur ne garde rien — sans fermeture à dire.
  void TellResult(std::int32_t message_id, std::uint32_t enchant_item_id);

  // LE DERNIER RÉSULTAT, une fois la séance close.
  struct Result {
    // La demande qu'il conclut, et la case qu'elle visait.
    ItemEnchantRequest request;
    std::uint8_t target_slot = 0;
    std::uint8_t card_slots = 0;
    std::uint16_t client_slot = 0;
    std::int32_t message_id = 0;
    std::uint32_t enchant_item_id = 0;

    [[nodiscard]] bool succeeded() const noexcept {
      return message_id == kItemEnchantSucceededMessage;
    }
  };
  [[nodiscard]] const Result& result() const noexcept { return result_; }
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

  struct Pending {
    ItemEnchantRequest request;
    std::uint8_t target_slot = 0;
    std::uint8_t card_slots = 0;
    std::uint16_t client_slot = 0;
  };

  bool open_ = false;
  std::uint64_t group_id_ = 0;
  std::uint32_t opening_seq_ = 0;
  std::optional<std::uint16_t> chosen_;
  std::uint32_t choice_seq_ = 0;
  std::optional<Pending> pending_;
  Result result_;
  std::uint32_t result_seq_ = 0;
  bool farewell_due_ = false;
};

// Ce qui retient la demande, l'objet en cause (le matériau qui manque), et la case
// qu'elle viserait.
struct ItemEnchantCheck {
  ItemEnchantRefusal refusal = ItemEnchantRefusal::kNone;
  std::uint32_t item_id = 0;
  std::uint8_t target_slot = 0;

  [[nodiscard]] bool passed() const noexcept { return refusal == ItemEnchantRefusal::kNone; }
};

// Ce que la demande coûterait, nul quand elle ne vise rien.
[[nodiscard]] const ItemEnchantCost* ItemEnchantCostOf(const ItemEnchantGroup& group,
                                                       const InventoryItem& item,
                                                       ItemEnchantRequest request,
                                                       std::uint8_t card_slots);

// Toute la demande, contre le sac et la bourse : la garde du verbe qui l'envoie.
// `card_slots` : les cases de carte de la pièce, que la table d'objets du client dit.
[[nodiscard]] ItemEnchantCheck CheckItemEnchantRequest(const ItemEnchant& enchant,
                                                       const ItemEnchantGroup& group,
                                                       ItemEnchantRequest request,
                                                       const Inventory& inventory,
                                                       std::uint8_t card_slots, std::int64_t zeny);

// Les cartes de la pièce après une réussite : l'enchantement posé dans la case
// visée, ou toutes les cases au-delà des cases de carte vidées par une remise à zéro.
[[nodiscard]] std::array<std::uint32_t, kItemCardSlots> EnchantedCards(
    const std::array<std::uint32_t, kItemCardSlots>& cards,
    const ItemEnchant::Result& result) noexcept;

}  // namespace ro::sim
