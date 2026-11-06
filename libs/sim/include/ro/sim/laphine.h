#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace ro::sim {

class Inventory;
struct InventoryItem;

// LE LAPHINE : un objet qu'on utilise ouvre une fenêtre, et ce qu'on y dépose en
// sort transformé.
//
// ⚠ DEUX ESPÈCES, que le serveur ouvre chacune par son paquet. La SYNTHÈSE consomme
// un nombre fixe de piles choisies et rend une récompense tirée au sort.
// L'AMÉLIORATION prend une seule pièce, lui retire ses options au sort et la rend.
//
// 🔴 LE SERVEUR N'ENVOIE QUE L'OBJET DÉCLENCHEUR : ce qu'il accepte vient d'une
// table du client (`LaphineRecipe`). Sans recette, rien ne se peut montrer.
//
// 🔴 TOUTE FERMETURE DU JOUEUR SE DIT AU SERVEUR, succès compris. L'ouverture
// lui arme un état qui compte dans `pc_cant_act` : tant qu'il n'a pas reçu la
// fermeture, le personnage ne peut plus agir. Seul un changement de carte le
// désarme sans paquet (`pc_setpos`). D'où `TakeFarewell`.

enum class LaphineKind : std::uint8_t {
  kSynthesis,
  kUpgrade,
};

// Les codes du résultat d'une synthèse (`e_laphine_synthesis_result`).
inline constexpr std::uint16_t kLaphineSynthesisSucceeded = 0;
inline constexpr std::uint16_t kLaphineSynthesisWrongAmount = 5;
inline constexpr std::uint16_t kLaphineSynthesisWrongItem = 7;
// Le résultat d'une amélioration : zéro réussie, tout le reste refusée.
inline constexpr std::uint16_t kLaphineUpgradeSucceeded = 0;

// Un objet accepté. En amélioration, la quantité vaut toujours un.
struct LaphineMaterial {
  std::uint32_t item_id = 0;
  std::uint16_t amount = 1;
};

// CE QU'UN OBJET DÉCLENCHEUR ACCEPTE, tel que le serveur le juge.
struct LaphineRecipe {
  std::uint32_t trigger_item_id = 0;
  // Combien de piles DISTINCTES il faut déposer. Toujours un en amélioration.
  std::uint16_t needed_piles = 1;
  std::uint8_t least_refine = 0;
  std::uint8_t most_refine = 0;
  // Amélioration : le nombre d'options qu'une pièce doit déjà porter.
  std::uint8_t least_options = 0;
  // Amélioration : une pièce sertie est-elle admise ?
  bool cards_allowed = false;
  std::vector<LaphineMaterial> materials;

  [[nodiscard]] const LaphineMaterial* FindMaterial(std::uint32_t item_id) const noexcept;
};

// POURQUOI UN OBJET OU UNE DEMANDE NE PART PAS.
enum class LaphineRefusal : std::uint8_t {
  kNone,
  kNotOpen,
  // Une demande attend déjà sa réponse.
  kWaiting,
  // Pas autant de piles déposées que la recette en demande.
  kWrongPileCount,
  // Une case déposée s'est vidée entre-temps.
  kPileGone,
  kNotAMaterial,
  kTooFewInPile,
  kWorn,
  // Amélioration seulement : le serveur refuse une pièce non identifiée.
  kUnidentified,
  kRefineTooLow,
  kRefineTooHigh,
  kTooFewOptions,
  // Amélioration seulement : une pièce sertie, que la recette n'admet pas.
  kCardsForbidden,
};

// Les mises en garde qui n'empêchent rien, mais que le joueur doit lire : ce qu'il
// dépose perdra plus que nécessaire.
struct LaphineWarnings {
  bool refined_above_need = false;
  bool has_cards = false;

  [[nodiscard]] bool any() const noexcept { return refined_above_need || has_cards; }
};

// Ce que le serveur dirait de cet objet, avant tout envoi.
[[nodiscard]] LaphineRefusal JudgeLaphineItem(const LaphineRecipe& recipe, LaphineKind kind,
                                              const InventoryItem& item);

[[nodiscard]] LaphineWarnings WarnLaphineItem(const LaphineRecipe& recipe,
                                              const InventoryItem& item);

class Laphine {
 public:
  [[nodiscard]] bool open() const noexcept { return open_; }
  // L'espèce de la dernière ouverture, qui reste lisible après la fermeture : le
  // résultat se lit avec elle.
  [[nodiscard]] LaphineKind kind() const noexcept { return kind_; }
  [[nodiscard]] std::uint32_t trigger_item_id() const noexcept { return trigger_item_id_; }
  // Change à chaque ouverture : c'est lui qui remet la fenêtre à zéro.
  [[nodiscard]] std::uint32_t opening_seq() const noexcept { return opening_seq_; }

  // Le paquet du serveur. ⚠ UNE OUVERTURE DE L'AUTRE ESPÈCE laisse armé chez lui
  // l'état de la première : sa fermeture est donc due.
  void TellOpening(LaphineKind kind, std::uint32_t trigger_item_id);

  // ── CE QUI EST DÉPOSÉ, PAR CASE DU SAC ────────────────────────────────────
  [[nodiscard]] std::span<const std::uint16_t> piles() const noexcept { return piles_; }
  // Change à chaque dépôt ou retrait : une confirmation se rapporte à un rang.
  [[nodiscard]] std::uint32_t piles_seq() const noexcept { return piles_seq_; }
  [[nodiscard]] bool Deposited(std::uint16_t client_slot) const noexcept;
  // Faux si la fenêtre est fermée, en attente, ou si la case y est déjà.
  bool Deposit(std::uint16_t client_slot);
  // Faux si la case n'y est pas, ou si une réponse est attendue.
  bool Withdraw(std::uint16_t client_slot);

  // ── LA DEMANDE ET SA RÉPONSE ──────────────────────────────────────────────
  [[nodiscard]] bool waiting() const noexcept { return waiting_; }
  void MarkRequestSent() noexcept { waiting_ = true; }
  // Le code du serveur. Un succès FERME, et sa fermeture est due ; un refus laisse
  // la fenêtre ouverte, prête pour un autre essai.
  void TellResult(std::uint16_t code);
  [[nodiscard]] std::uint16_t result() const noexcept { return result_; }
  // ⚠ ZÉRO VEUT DIRE « AUCUN RÉSULTAT ENCORE », là où le code zéro dit la réussite.
  [[nodiscard]] std::uint32_t result_seq() const noexcept { return result_seq_; }

  // L'amélioration vient de changer cette pièce : la fenêtre de description s'ouvre
  // dessus. Une fois.
  void NoteUpgraded(std::uint16_t client_slot) noexcept { upgraded_slot_ = client_slot; }
  [[nodiscard]] std::optional<std::uint16_t> TakeUpgraded() noexcept;

  // ── LA FERMETURE ──────────────────────────────────────────────────────────
  // Le joueur ferme : la fermeture se dit au serveur.
  void Close();
  // Le personnage change de carte ou de serveur : ⚠ RIEN À DIRE, le serveur a déjà
  // désarmé, et les fermetures encore dues tombent.
  void CloseSilently() noexcept;
  // La prochaine espèce dont la fermeture est due, une fois chacune.
  [[nodiscard]] std::optional<LaphineKind> TakeFarewell() noexcept;

 private:
  bool open_ = false;
  LaphineKind kind_ = LaphineKind::kSynthesis;
  std::uint32_t trigger_item_id_ = 0;
  std::uint32_t opening_seq_ = 0;
  std::vector<std::uint16_t> piles_;
  std::uint32_t piles_seq_ = 0;
  bool waiting_ = false;
  std::uint16_t result_ = 0;
  std::uint32_t result_seq_ = 0;
  std::optional<std::uint16_t> upgraded_slot_;
  bool synthesis_farewell_due_ = false;
  bool upgrade_farewell_due_ = false;
};

// Ce qui retient la demande, et la case en cause (zéro si la demande entière).
struct LaphineCheck {
  LaphineRefusal refusal = LaphineRefusal::kNone;
  std::uint16_t client_slot = 0;

  [[nodiscard]] bool passed() const noexcept { return refusal == LaphineRefusal::kNone; }
};

// Toute la demande, contre le sac : la garde du verbe qui l'envoie.
[[nodiscard]] LaphineCheck CheckLaphineRequest(const Laphine& laphine, const LaphineRecipe& recipe,
                                               const Inventory& inventory);

}  // namespace ro::sim
