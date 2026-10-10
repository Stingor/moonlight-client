#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/item_icons.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/preset_tab.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "character/equip_presets.h"
#include "input/hotkeys.h"

namespace moonclient {

// L'onglet des presets vu de la partie : le carnet du personnage de la session
// et son fichier, les lignes que l'onglet montre, et les demandes que ses gestes
// et ses touches envoient. Le sac ne change qu'aux accusés du serveur.
class PresetView {
 public:
  PresetView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
             ro::render::ItemIcons* icons, std::filesystem::path client_root,
             std::uint32_t character_id);

  // Refait les lignes de l'onglet, fiche ouverte, quand le carnet, la table des
  // raccourcis ou la langue ont changé.
  void Refresh(bool shown, const Hotkeys& table);
  [[nodiscard]] ro::ui::PresetTabView view() const;

  // Ce que l'onglet a demandé, sauf l'éditeur de style : son interrupteur est à
  // la boucle.
  void Apply(const ro::ui::PresetAction& asked, ro::ui::PresetTabState& state,
             ro::net::Connection& link, const ro::sim::World& world);

  // La touche qu'attend l'onglet devient celle du preset, ou `state` reçoit
  // pourquoi elle ne le peut pas.
  void Capture(const Hotkeys& table, ro::ui::PresetTabState& state, Binding wanted);

  // Remet le preset que cette touche désigne. Rend faux s'il n'y en a pas.
  bool TriggerHotkey(Binding pressed, ro::ui::PresetTabState& state, ro::net::Connection& link,
                     const ro::sim::World& world);

  // La fenêtre des raccourcis vient de donner cette touche à une action : le
  // preset qui la tenait la perd. Rend son nom tel qu'une phrase le cite, ou
  // vide.
  [[nodiscard]] std::string ReleaseHotkey(Binding taken);

 private:
  void Wear(std::size_t rank, ro::ui::PresetTabState& state, ro::net::Connection& link,
            const ro::sim::World& world);
  // Réécrit le fichier quand le carnet a changé depuis la dernière fois.
  void SaveIfChanged();
  [[nodiscard]] std::string PieceName(const PresetPiece& piece) const;
  [[nodiscard]] ro::ui::SkinImage PieceIcon(std::uint32_t item_id) const;

  const ro::vfs::ItemTable* items_;
  const ro::vfs::ItemWords* words_;
  ro::render::ItemIcons* icons_;
  std::filesystem::path client_root_;
  std::uint32_t character_id_;
  EquipPresetBook book_;
  std::uint32_t saved_revision_ = 0;

  // Ce sur quoi les lignes ont été composées ; rien : elles sont à composer.
  struct Composed {
    std::uint32_t book = 0;
    std::uint32_t table = 0;
    std::size_t language = 0;

    [[nodiscard]] bool operator==(const Composed&) const = default;
  };
  std::optional<Composed> composed_;

  // Les vues pointent dans ces listes : elles sont remplies entières avant,
  // car une liste qui grandit déplace ce qu'elle porte.
  std::vector<std::string> piece_names_;
  std::vector<ro::ui::PresetPieceView> pieces_;
  std::vector<std::string> hotkeys_;
  std::vector<std::string> holders_;
  std::vector<ro::ui::PresetRowView> rows_;
  std::string style_hotkey_;
};

}  // namespace moonclient
