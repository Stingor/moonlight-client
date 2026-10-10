#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'onglet des presets de la fiche : les jeux d'équipement nommés du personnage.
// Il montre des lignes déjà composées et rend ce que le joueur demande ; c'est
// `app` qui enregistre, remet et retire.

// Le nom qu'on tape, terminateur compris : vingt-trois caractères.
inline constexpr std::size_t kEquipPresetNameBytes = 24;
inline constexpr std::size_t kEquipPresetNoteBytes = 160;

enum class PresetPieceKind : std::uint8_t { kEquipment, kCostume, kAmmo };

struct PresetPieceView {
  SkinImage icon;
  // Le nom au survol, raffinement compris — « +7 Épée ».
  std::string_view name;
  int refine_level = 0;
  PresetPieceKind kind = PresetPieceKind::kEquipment;
};

struct PresetRowView {
  std::string_view name;
  std::span<const PresetPieceView> pieces;
  bool with_costumes = false;
  // « Ctrl+F5 », ou vide.
  std::string_view hotkey;
  // Qui d'autre tient la touche — « l'action « Inventaire » » —, ou vide. Une
  // touche de la table des raccourcis passe avant celle du preset.
  std::string_view taken_by;
};

struct PresetTabView {
  std::span<const PresetRowView> presets;
  // Le plafond par personnage.
  std::size_t capacity = 0;
  // La touche de l'éditeur de style, ou vide.
  std::string_view style_hotkey;
};

struct PresetTabState {
  std::array<char, kEquipPresetNameBytes> name = {};
  // Le preset dont on attend la touche, ou moins de zéro. La touche est lue par
  // `app`, qui l'attribue ou écrit dans `refusal` pourquoi il ne le peut pas ;
  // Échap se lit ici. L'attente s'annule dès que la ligne n'est plus à l'écran.
  int capturing = -1;
  std::array<char, kEquipPresetNoteBytes> refusal = {};
  // Le compte rendu du dernier geste, écrit par `app` — y compris pour une
  // touche de preset pressée fiche fermée.
  std::array<char, kEquipPresetNoteBytes> status = {};
};

struct PresetAction {
  // Remettre ou supprimer ce preset, par son rang dans la liste montrée.
  std::optional<std::size_t> load;
  std::optional<std::size_t> remove;
  // La case « Piloter aussi les costumes » de ce preset, et son nouvel état.
  std::optional<std::size_t> costumes;
  bool with_costumes = false;
  std::optional<std::size_t> clear_hotkey;
  // Enregistrer ce qui est porté sous ce nom ; vide, rien à enregistrer.
  std::string save_as;
  // « Tout déséquiper » — avec les costumes si vrai.
  std::optional<bool> undress;
  bool toggle_style = false;
  // Échap vient d'annuler une attente : la même frappe ne doit rien fermer.
  bool escape_spent = false;
};

// À appeler dans un enfant de la fiche : l'onglet en prend toute la largeur.
[[nodiscard]] PresetAction DrawPresetTab(PresetTabState& state, const PresetTabView& view,
                                         const SkinArt& art);

}  // namespace ro::ui
