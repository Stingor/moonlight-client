#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/target_options.h"
#include "world/blocked_npcs.h"
#include "world/entity_menu.h"

namespace moonclient {

// ── LA PAGE « MENU CONTEXTUEL » ─────────────────────────────────────────────
//
// Sur quoi le menu du clic droit s'ouvre, et la liste des PNJ rendus sourds au
// clic gauche.
//
// ⚠ LES LIGNES PORTENT UN IDENTIFIANT COMPOSÉ, hors de `GameOption`, comme
// celles de la fenêtre de cible : la liste des PNJ bloqués compte une ligne par
// PNJ.
inline constexpr int kContextMenuRowBase = 5200;
inline constexpr int kBlockedNpcRowBase = 5300;
inline constexpr int kBlockedNpcRowsMost = static_cast<int>(kFixedNpcLastId - kFixedNpcFirstId + 1);

enum class ContextMenuRow : int {
  kIntro = 0,
  kAllEntities,
  kSelfMenu,
  kNpcBlock,
  kNoBlockedNpc,
  kStaffExtras,
  kCount,
};

static_assert(kTargetPartRowBase +
                      static_cast<int>(ro::ui::kTargetPartCount) * kTargetFieldsPerPart <
                  kContextMenuRowBase,
              "les lignes des cadres de cible déborderaient sur celles du menu contextuel");
static_assert(kContextMenuRowBase + static_cast<int>(ContextMenuRow::kCount) < kBlockedNpcRowBase,
              "les lignes du menu contextuel déborderaient sur celles des PNJ bloqués");

[[nodiscard]] constexpr int ContextMenuRowId(ContextMenuRow row) {
  return kContextMenuRowBase + static_cast<int>(row);
}

class ContextMenuOptions {
 public:
  [[nodiscard]] const EntityMenuSettings& settings() const noexcept { return settings_; }

  // LA LISTE DES PNJ BLOQUÉS. ⚠ ELLE VIT ICI parce que c'est cette page qui la
  // montre et la défait ; son fichier, lui, est écrit par `GameSettings`, comme
  // les favoris du coffre.
  [[nodiscard]] BlockedNpcs& blocked() noexcept { return blocked_; }
  [[nodiscard]] const BlockedNpcs& blocked() const noexcept { return blocked_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Le compte est-il du staff ? Sans cela, la case « Outils du staff » ne se
  // compose pas : elle ne pourrait rien faire.
  void NoteStaff(bool staff) noexcept { staff_ = staff; }

  // Refait les lignes, à chaque image de panneau ouvert.
  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

  // Un PNJ vient d'être débloqué depuis la page : la liste est à réécrire.
  [[nodiscard]] bool TakeBlockedChanged() noexcept;

 private:
  EntityMenuSettings settings_;
  BlockedNpcs blocked_;
  bool staff_ = false;
  bool blocked_changed_ = false;
  std::vector<ro::ui::OptionRowView> rows_;
  // Le GID de chaque ligne de la liste, dans l'ordre des lignes.
  std::vector<std::uint32_t> listed_;
  // Les textes de la liste. ⚠ REMPLIS EN ENTIER AVANT QU'UNE VUE NE SE POSE :
  // une vue ne survit pas à la réallocation de ce qu'elle désigne.
  std::vector<std::string> names_;
  std::vector<std::string> notes_;
};

}  // namespace moonclient
