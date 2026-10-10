#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/inventory.h"

#include "input/hotkeys.h"

namespace moonclient {

// Les presets d'équipement : un jeu de pièces nommé, propre à un personnage,
// qu'on remet d'un geste. Une pièce se retient par son identité — objet,
// raffinement, grade, cartes —, pas par sa place dans le sac, qui change.

inline constexpr std::size_t kPresetsPerCharacter = 5;

// L'équipement suit le preset ; les costumes ne bougent que si le preset les
// pilote ; la munition se remplace mais ne se retire jamais.
enum class PresetFamily : std::uint8_t { kEquipment, kCostume, kAmmo };

struct PresetPiece {
  std::uint32_t item_id = 0;
  int refine_level = 0;
  int grade = 0;
  std::array<std::uint32_t, ro::sim::kItemCardSlots> cards{};
  // Une arme tenue de la main gauche, qui se redemande à gauche.
  bool left_hand = false;
  PresetFamily family = PresetFamily::kEquipment;

  [[nodiscard]] bool operator==(const PresetPiece&) const = default;
};

struct EquipPreset {
  std::string name;
  std::vector<PresetPiece> pieces;
  Binding hotkey;
  // Faux par défaut : un preset de combat ne touche pas à l'apparence. Les
  // costumes portés sont retenus quand même, pour qu'il suffise de cocher.
  bool with_costumes = false;
};

// Ce qui est porté, une entrée par pièce — une arme à deux mains ou une coiffe
// à deux étages ne compte qu'une fois —, dans l'ordre des dix places, puis les
// costumes, puis la munition. Ni l'équipement d'ombre.
[[nodiscard]] std::vector<PresetPiece> WornPresetPieces(const ro::sim::Inventory& bag);

struct PresetWear {
  std::uint16_t client_slot = 0;
  std::uint32_t position = 0;
};

// Ce qu'il faut demander pour remettre un preset : les retraits, puis les poses.
struct PresetPlan {
  std::vector<std::uint16_t> take_off;
  std::vector<PresetWear> wear;
  // Les pièces que le sac ne contient plus.
  int missing = 0;
};

// Rapproche le preset du sac :
// - chaque pièce désigne un objet distinct, l'exemplaire exact d'abord, et
//   celui qu'on porte déjà plutôt que son jumeau du sac ;
// - une pièce que le preset remplace ne se retire pas à part — le serveur la
//   retire en posant l'autre, sans faire tomber la munition d'un arc ;
// - une arme se demande dans la main retenue, une pièce portée ailleurs se
//   retire d'abord, et une pièce gardée qu'une pose déloge se redemande après ;
// - les poses dont le serveur choisit la place partent en dernier.
[[nodiscard]] PresetPlan PlanPreset(const EquipPreset& preset, const ro::sim::Inventory& bag);

// « Tout déséquiper » : une demande par pièce portée. Ni la munition, ni
// l'équipement d'ombre — le serveur retire toutefois la munition avec un arc.
[[nodiscard]] std::vector<std::uint16_t> PlanUndress(const ro::sim::Inventory& bag,
                                                     bool with_costumes);

// Une lettre, un chiffre de la rangée du haut, F1 à F12 ou l'espace.
[[nodiscard]] bool PresetKeyCapturable(std::uint32_t key);

// Les presets d'un personnage, dans l'ordre où il les a créés.
class EquipPresetBook {
 public:
  EquipPresetBook() = default;
  explicit EquipPresetBook(std::vector<EquipPreset> presets) : presets_(std::move(presets)) {}

  [[nodiscard]] std::span<const EquipPreset> presets() const noexcept { return presets_; }
  [[nodiscard]] const EquipPreset* At(std::size_t rank) const noexcept;

  enum class Saved : std::uint8_t { kAdded, kReplaced, kFull };

  // Un nom déjà pris remplace les pièces de son preset et garde ses costumes et
  // sa touche — ce qui reste possible au plafond.
  Saved Save(std::string_view name, std::vector<PresetPiece> pieces);
  bool Erase(std::size_t rank);
  bool SetWithCostumes(std::size_t rank, bool with_costumes);
  bool SetHotkey(std::size_t rank, Binding hotkey);

  [[nodiscard]] std::optional<std::size_t> Holding(
      Binding hotkey, std::optional<std::size_t> except = std::nullopt) const;

  // Retire cette touche au preset qui la tient, et rend son rang.
  std::optional<std::size_t> Release(Binding hotkey);

  // Change à chaque modification : qui enregistre compare ce compte au sien.
  [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }

 private:
  std::vector<EquipPreset> presets_;
  std::uint32_t revision_ = 0;
};

// « le preset « PvM » » : un preset tel qu'une phrase le cite.
[[nodiscard]] std::string PresetOwnerLabel(std::string_view name);

// Qui tient déjà cette touche — un autre preset du personnage, ou une action de
// la table des raccourcis —, prêt à suivre « Déjà utilisé par ».
[[nodiscard]] std::optional<std::string> PresetKeyHolder(const Hotkeys& table,
                                                         const EquipPresetBook& book,
                                                         std::optional<std::size_t> self,
                                                         Binding wanted);

}  // namespace moonclient
