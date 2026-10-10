#include "ro/ui/packet_window.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "imgui.h"

namespace ro::ui {
namespace {

// « 0x0284 » : la forme sous laquelle tous les relevés du projet nomment un
// opcode, donc celle qui se recopie telle quelle dans une recherche.
constexpr std::size_t kHexOpcodeSize = 7;

[[nodiscard]] std::array<char, kHexOpcodeSize> HexOpcode(std::uint16_t opcode) {
  std::array<char, kHexOpcodeSize> text = {};
  std::snprintf(text.data(), text.size(), "0x%04X", opcode);
  return text;
}

// Les colonnes du relevé, dans l'ordre où le décodeur les produit : ce qui a
// été appliqué, ce qui a été reconnu puis écarté, puis les deux manières de ne
// rien donner.
//
// ⚠ UNE FONCTION À LITTÉRAUX, ET NON UNE TABLE. `Tr` n'accepte qu'un littéral,
// et le relevé qui bâtit les catalogues ne voit que ce qui est écrit tel quel :
// `Tr(table[rang])` ne serait dans aucun catalogue et ne pourrait pas y entrer.
constexpr std::size_t kColumnCount = 5;

[[nodiscard]] const char* ColumnTitle(std::size_t column) {
  switch (column) {
    case 0:
      return core::Tr("opcode");
    case 1:
      return core::Tr("appliquées");
    case 2:
      return core::Tr("ignorées");
    case 3:
      return core::Tr("non traitées");
    default:
      return core::Tr("indécodables");
  }
}

}  // namespace

std::vector<PacketRow> SelectPacketRows(std::span<const PacketRow> rows,
                                        const PacketWindowState& state) {
  std::vector<PacketRow> kept;
  kept.reserve(rows.size());
  for (const PacketRow& row : rows) {
    if (state.missing_only && row.missing() == 0) {
      continue;
    }
    kept.push_back(row);
  }
  // `std::ranges::sort` et non `stable_sort` : le second critère est l'opcode,
  // qui est unique, donc l'ordre est déjà total et rien ne reste à départager.
  std::ranges::sort(kept, [](const PacketRow& left, const PacketRow& right) {
    if (left.missing() != right.missing()) {
      return left.missing() > right.missing();
    }
    return left.opcode < right.opcode;
  });
  return kept;
}

std::size_t DrawPacketWindow(PacketWindowState& state, std::span<const PacketRow> rows,
                             std::uint32_t frames) {
  if (!state.open) {
    return 0;
  }
  // Recomposé au changement de langue seulement : voir `WindowName`.
  static WindowName name(kPacketWindowTitle, kPacketWindowId);
  ConstrainNextWindowToScreen(name.Get());
  if (!ImGui::Begin(name.Get(), &state.open)) {
    // Repliée : ImGui n'a rien dessiné, et `End` reste dû.
    ImGui::End();
    return 0;
  }

  const std::vector<PacketRow> shown = SelectPacketRows(rows, state);
  std::uint32_t missing_frames = 0;
  for (const PacketRow& row : rows) {
    missing_frames += row.missing();
  }

  ImGui::Text(core::Tr("%u trame(s), dont %u perdue(s) sur %zu opcode(s)."), frames, missing_frames,
              rows.size());
  ImGui::Checkbox(core::Tr("Seulement ce qui s'est perdu"), &state.missing_only);
  ImGui::Separator();

  constexpr ImGuiTableFlags kTableFlags =
      ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp;
  if (ImGui::BeginTable("trames", static_cast<int>(kColumnCount), kTableFlags)) {
    for (std::size_t column = 0; column < kColumnCount; ++column) {
      ImGui::TableSetupColumn(ColumnTitle(column));
    }
    ImGui::TableHeadersRow();
    for (const PacketRow& row : shown) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(HexOpcode(row.opcode).data());
      ImGui::TableNextColumn();
      ImGui::Text("%u", row.applied);
      ImGui::TableNextColumn();
      ImGui::Text("%u", row.ignored);
      ImGui::TableNextColumn();
      ImGui::Text("%u", row.unhandled);
      ImGui::TableNextColumn();
      ImGui::Text("%u", row.undecodable);
    }
    ImGui::EndTable();
  }

  if (shown.empty()) {
    ImGui::TextUnformatted(state.missing_only ? core::Tr("Aucune trame ne s'est perdue.")
                                              : core::Tr("Aucune trame reçue."));
  }
  ImGui::End();
  return shown.size();
}

}  // namespace ro::ui
