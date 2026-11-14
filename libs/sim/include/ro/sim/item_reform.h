#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace ro::sim {

class Inventory;
struct InventoryItem;

// LA RÉFORME : un PNJ ou un objet ouvre une fenêtre où une pièce de la liste se
// change en une autre, contre des matériaux. Le raffinement bouge d'un pas fixe,
// cartes et options restent.
//
// 🔴 LE SERVEUR N'ENVOIE QUE L'OBJET DÉCLENCHEUR : les pièces admises, ce qu'elles
// deviennent et ce qu'elles coûtent viennent d'une table du client
// (`ItemReformTrigger`). Sans elle, rien ne se peut montrer.
//
// 🔴 SES REFUS SONT MUETS : une demande qu'il rejette ne reçoit aucune réponse.
// D'où le jugement complet AVANT l'envoi (`CheckItemReformRequest`).
//
// 🔴 TOUTE FERMETURE DU JOUEUR SE DIT AU SERVEUR. L'ouverture lui arme un état qui
// compte dans `pc_cant_act` : tant qu'il n'a pas reçu la fermeture, le personnage
// ne peut plus agir. ⚠ SAUF APRÈS UNE RÉUSSITE, que le serveur désarme seul, et un
// changement de carte (`pc_setpos`).

// Le résultat de la réforme : zéro réussie, tout le reste refusée.
inline constexpr std::uint8_t kItemReformSucceeded = 0;

struct ItemReformMaterial {
  std::uint32_t item_id = 0;
  std::uint16_t amount = 1;
};

// UNE PIÈCE ADMISE, ce qu'elle devient et ce qu'elle coûte, tel que le serveur le juge.
struct ItemReformRecipe {
  std::uint32_t base_item_id = 0;
  std::uint32_t result_item_id = 0;
  std::uint8_t least_refine = 0;
  std::uint8_t most_refine = 0;
  // Le nombre d'options qu'une pièce doit déjà porter.
  std::uint8_t least_options = 0;
  // ⚠ ADMISES PAR DÉFAUT, à l'inverse du Laphine.
  bool cards_allowed = true;
  // Le pas du raffinement, négatif pour une baisse.
  std::int8_t refine_change = 0;
  std::vector<ItemReformMaterial> materials;
};

// CE QU'UN OBJET DÉCLENCHEUR RÉFORME.
struct ItemReformTrigger {
  std::uint32_t trigger_item_id = 0;
  // Le plafond du raffinement du serveur : le résultat s'y arrête.
  std::uint8_t refine_cap = 0;
  std::vector<ItemReformRecipe> recipes;

  [[nodiscard]] const ItemReformRecipe* FindRecipe(std::uint32_t base_item_id) const noexcept;
};

// POURQUOI UNE PIÈCE OU UNE DEMANDE NE PART PAS.
enum class ItemReformRefusal : std::uint8_t {
  kNone,
  kNotOpen,
  // Une demande attend déjà sa réponse.
  kWaiting,
  kNothingChosen,
  // La pièce choisie a quitté le sac entre-temps.
  kPieceGone,
  kNotReformable,
  kWorn,
  kUnidentified,
  kRefineTooLow,
  kRefineTooHigh,
  kTooFewOptions,
  kCardsForbidden,
  kMissingMaterial,
};

// Ce que le serveur dirait de cette pièce, matériaux mis à part.
[[nodiscard]] ItemReformRefusal JudgeItemReformPiece(const ItemReformRecipe& recipe,
                                                     const InventoryItem& item);

// ⚠ LE SERVEUR NE COMPTE QUE LA PREMIÈRE PILE de chaque matériau, dans l'ordre
// des cases : deux piles de vingt ne paient pas trente. Zéro quand il n'en tient pas.
[[nodiscard]] std::uint32_t CountedMaterial(const Inventory& inventory, std::uint32_t item_id);

// Le premier matériau qui manque, nul quand tout y est.
[[nodiscard]] const ItemReformMaterial* MissingMaterial(const ItemReformRecipe& recipe,
                                                        const Inventory& inventory);

// Le raffinement de la pièce réformée, borné à zéro et au plafond.
[[nodiscard]] std::uint8_t ReformedRefine(const ItemReformTrigger& trigger,
                                          const ItemReformRecipe& recipe,
                                          std::uint8_t refine_level) noexcept;

class ItemReform {
 public:
  [[nodiscard]] bool open() const noexcept { return open_; }
  [[nodiscard]] std::uint32_t trigger_item_id() const noexcept { return trigger_item_id_; }
  // Change à chaque ouverture : c'est lui qui remet la fenêtre à zéro.
  [[nodiscard]] std::uint32_t opening_seq() const noexcept { return opening_seq_; }

  // Le paquet du serveur. Une ouverture par-dessus une autre la remplace : il ne
  // garde qu'un déclencheur, sans fermeture à dire.
  void TellOpening(std::uint32_t trigger_item_id);

  // ── LA PIÈCE CHOISIE, PAR CASE DU SAC ─────────────────────────────────────
  [[nodiscard]] std::optional<std::uint16_t> chosen_slot() const noexcept { return chosen_; }
  // Change à chaque choix : une confirmation se rapporte à un rang.
  [[nodiscard]] std::uint32_t choice_seq() const noexcept { return choice_seq_; }
  // Faux si la fenêtre est fermée ou en attente.
  bool Choose(std::uint16_t client_slot);

  // ── LA DEMANDE ET SA RÉPONSE ──────────────────────────────────────────────
  [[nodiscard]] bool waiting() const noexcept { return waiting_; }
  void MarkRequestSent() noexcept { waiting_ = true; }
  // Le code du serveur et la case de la pièce. Une réussite ferme sans fermeture
  // due ; un refus laisse la fenêtre ouverte.
  void TellResult(std::uint8_t code, std::uint16_t client_slot);
  [[nodiscard]] std::uint8_t result() const noexcept { return result_; }
  // ⚠ ZÉRO VEUT DIRE « AUCUN RÉSULTAT ENCORE », là où le code zéro dit la réussite.
  [[nodiscard]] std::uint32_t result_seq() const noexcept { return result_seq_; }

  // La pièce vient d'être réformée : sa description s'ouvre dessus. Une fois.
  [[nodiscard]] std::optional<std::uint16_t> TakeReformed() noexcept;

  // ── LA FERMETURE ──────────────────────────────────────────────────────────
  // Le joueur ferme : la fermeture se dit au serveur.
  void Close();
  // Changement de carte ou de serveur : RIEN À DIRE, le serveur a déjà désarmé.
  void CloseSilently() noexcept;
  // Vrai une fois quand une fermeture est due.
  [[nodiscard]] bool TakeFarewell() noexcept;

 private:
  void Reset() noexcept;

  bool open_ = false;
  std::uint32_t trigger_item_id_ = 0;
  std::uint32_t opening_seq_ = 0;
  std::optional<std::uint16_t> chosen_;
  std::uint32_t choice_seq_ = 0;
  bool waiting_ = false;
  std::uint8_t result_ = 0;
  std::uint32_t result_seq_ = 0;
  std::optional<std::uint16_t> reformed_slot_;
  bool farewell_due_ = false;
};

// Ce qui retient la demande, et le matériau en cause.
struct ItemReformCheck {
  ItemReformRefusal refusal = ItemReformRefusal::kNone;
  std::uint32_t material_id = 0;

  [[nodiscard]] bool passed() const noexcept { return refusal == ItemReformRefusal::kNone; }
};

// Toute la demande, contre le sac : la garde du verbe qui l'envoie.
[[nodiscard]] ItemReformCheck CheckItemReformRequest(const ItemReform& reform,
                                                     const ItemReformTrigger& trigger,
                                                     const Inventory& inventory);

}  // namespace ro::sim
