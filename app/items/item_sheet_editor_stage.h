#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "ro/formats/icon_tint.h"
#include "ro/render/item_icons.h"
#include "ro/ui/item_sheet_editor.h"
#include "ro/ui/staff_tools.h"
#include "ro/vfs/item_info_text.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/vfs.h"

#include "items/item_info_delta.h"

namespace moonclient {

// Ce que la pièce reçoit à chaque image, le temps de l'appel.
struct ItemSheetEditorContext {
  // Le droit du staff, relu à chaque image.
  bool allowed = false;
  const std::filesystem::path* client_root = nullptr;
  ro::vfs::Vfs* vfs = nullptr;
  ro::render::ItemIcons* icons = nullptr;
};

// LA FICHE D'UN OBJET ÉDITÉE PAR LE STAFF, vue de la partie : la vue que
// l'établi dessine, et les bons de travail qu'elle pose.
//
// 🔴 LA SAISIE SE COMPARE À LA LIGNE BRUTE DE LA TABLE, pas à l'objet que le
// client montre : celui-ci porte en tête de description les trois lignes du
// script de chargement, et ses `#` de fin changés en traits. Un bon tiré de lui
// les écrirait dans la table.
class ItemSheetEditorStage {
 public:
  // L'IMAGE : ce que la fenêtre a demandé à l'image d'avant, puis la vue de la
  // suivante, posée dans l'état de l'établi.
  void Advance(ro::ui::StaffToolsState& tools, const ItemSheetEditorContext& context);

 private:
  // La ligne de la table pour cet objet, relue au changement d'objet ; nulle
  // quand la table ne l'a pas.
  [[nodiscard]] const ro::vfs::ItemInfoRecord* TableRecord(std::uint32_t item_id,
                                                           ro::vfs::Vfs* vfs);
  void Save(ro::ui::ItemSheetEditorState& state, const ro::vfs::ItemInfoRecord& table);
  void Compose(const ro::ui::ItemSheetEditorState& state, const ItemSheetEditorContext& context);
  void Notice(std::string text, bool ok);

  std::optional<ItemInfoDelta> delta_;

  std::uint32_t record_for_ = 0;
  std::optional<ro::vfs::ItemInfoRecord> record_;

  // Les libellés des teintes, en français : « aucune », puis chaque teinte
  // dans l'ordre de `kIconTintShades`.
  std::array<const char*, ro::formats::kIconTintShades.size() + 1> tint_labels_{};
  // Ce que la vue désigne, gardé ici le temps de l'image.
  ro::vfs::ItemInfoRecord start_;
  std::string start_resource_;
  std::string notice_;
  bool notice_ok_ = true;
  std::uint32_t notice_for_ = 0;
  ro::ui::ItemSheetEditorView view_;
};

// LE RANG D'UNE TEINTE dans la liste de la fenêtre : zéro pour « aucune » ou
// un mot inconnu, puis le rang dans `kIconTintShades` plus un.
[[nodiscard]] int ItemSheetTintRank(std::string_view word);
// Le mot du rang ; vide pour « aucune ».
[[nodiscard]] std::string_view ItemSheetTintWord(int rank);

// LE BON QUE DONNE UNE SAISIE : les seuls champs qui diffèrent de la ligne de
// la table. Vide quand la saisie redit la table.
[[nodiscard]] ItemInfoDeltaEntry ItemSheetEdit(std::uint32_t item_id,
                                               const ro::vfs::ItemInfoRecord& table,
                                               const ro::vfs::ItemInfoRecord& typed);

}  // namespace moonclient
