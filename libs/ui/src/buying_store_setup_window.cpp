#include "ro/ui/buying_store_setup_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// ── LES MESURES (composition), passées à l'échelle ──────────────────────────
constexpr float kFirstWidthPixels = 560.0F;
constexpr float kFirstHeightPixels = 430.0F;
constexpr float kLeastWidthPixels = 430.0F;
constexpr float kLeastHeightPixels = 300.0F;
// Le champ du nom commence après son libellé.
constexpr float kNameLabelPixels = 45.0F;
// Les colonnes.
constexpr float kOwnedColumnPixels = 46.0F;
constexpr float kAmountColumnPixels = 56.0F;
constexpr float kPriceColumnPixels = 96.0F;
constexpr float kTotalColumnPixels = 88.0F;
// Le champ de la limite.
constexpr float kLimitFieldPixels = 160.0F;
// La liste des items à poser garde au moins quatre lignes, les lignes posées
// trois : le surplus va à la liste, qui est la plus longue.
constexpr float kLeastCandidateRows = 4.0F;
constexpr float kLeastLineRows = 3.0F;
// Les identités des deux tables, loin l'une de l'autre.
constexpr int kCandidateIdBase = 10000;

// Le fil porte la limite sur quatre octets, que le serveur lit signés.
constexpr std::int64_t kWireMostZeny = std::numeric_limits<std::int32_t>::max();

constexpr const char* kDearConfirmId = "##buying_store_dear_confirm";
constexpr const char* kItemMenuId = "##buying_store_setup_item_menu";

bool Matches(const ImGuiTextFilter& filter, std::string_view name) {
  return filter.PassFilter(name.data(), name.data() + name.size());
}

const BuyingStoreCandidateView* CandidateOf(const BuyingStoreSetupView& view,
                                            std::uint32_t item_id) {
  const auto found = std::ranges::find_if(view.candidates, [item_id](const auto& candidate) {
    return candidate.item.item_id == item_id;
  });
  return found == view.candidates.end() ? nullptr : &*found;
}

bool Placed(const BuyingStoreSetupState& state, std::uint32_t item_id) {
  return std::ranges::find(state.lines, item_id, &BuyingStoreSetupLine::item_id) !=
         state.lines.end();
}

void NoteItem(const TradeItemGesture& gesture, const TradeItemView& item, std::uint32_t item_id,
              ItemLinkMenu& menu, BuyingStoreSetupAction& action) {
  if (gesture.hovered) {
    action.hovered_id = item_id;
  }
  TradeItemView aimed = item;
  aimed.item_id = item_id;
  if (NoteTradeItemLink(gesture, aimed, 0, menu)) {
    action.describe_id = item_id;
    action.item_gesture = ChatLinkGesture::kRepost;
  }
}

std::string JoinedReasons(std::span<const std::string_view> reasons) {
  std::string joined;
  for (const std::string_view reason : reasons) {
    if (!joined.empty()) {
      joined.push_back('\n');
    }
    joined.append(reason);
  }
  return joined;
}

void DrawName(BuyingStoreSetupState& state, const BuyingStoreSetupView& view) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Nom"));
  ImGui::SameLine(kNameLabelPixels * Scale());
  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputText("##buying_store_name", state.title.data(), state.title.size());
  // 🔴 LE FIL COMPTE DES OCTETS, pas des lettres : un accent en prend deux dès
  // que le nom n'est plus en 1252. Ce que le serveur coupera se dit ici, et
  // c'est le nom coupé que les autres liront.
  if (view.title_bytes_left >= 0) {
    ImGui::TextDisabled(core::Tr("%d octet(s) restant(s)"), view.title_bytes_left);
  } else {
    ImGui::TextColored(kTradeWarningText, core::Tr("Trop long : il sera coupé en « %.*s »"),
                       static_cast<int>(view.title_on_wire.size()), view.title_on_wire.data());
  }
}

void DrawCandidates(BuyingStoreSetupState& state, const BuyingStoreSetupView& view,
                    const SkinArt& art, float height, BuyingStoreSetupAction& action) {
  const float scale = Scale();
  const bool full = state.lines.size() >= view.slots;
  ImGui::BeginChild("##buying_store_candidates", ImVec2(-1.0F, height), ImGuiChildFlags_Borders);
  if (view.candidates.empty()) {
    ImGui::TextDisabled("%s", core::Tr("L'inventory est vide."));
  } else if (ImGui::BeginTable("##buying_store_candidate_table", 3,
                               ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(core::Tr("Possédé"), ImGuiTableColumnFlags_WidthFixed,
                            kOwnedColumnPixels * scale);
    ImGui::TableSetupColumn("##place", ImGuiTableColumnFlags_WidthFixed,
                            SkinSmallButtonWidth(core::Tr("Poser"), art));
    ImGui::TableHeadersRow();
    for (std::size_t index = 0; index < view.candidates.size(); ++index) {
      const BuyingStoreCandidateView& candidate = view.candidates[index];
      if (!Matches(state.filter, candidate.item.name)) {
        continue;
      }
      ImGui::PushID(kCandidateIdBase + static_cast<int>(index));
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      NoteItem(DrawTradeItem(candidate.item), candidate.item, candidate.item.item_id,
               state.item_menu, action);
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(kTradeBlackText, "%d", candidate.owned);
      ImGui::TableNextColumn();
      const bool placed = Placed(state, candidate.item.item_id);
      const bool barred = full || placed || view.awaiting;
      if (barred) {
        ImGui::BeginDisabled();
      }
      if (SkinSmallButton(core::Tr("Poser"), art)) {
        AddBuyingStoreSetupLine(state, candidate.item.item_id, view.slots);
      }
      if (barred) {
        ImGui::EndDisabled();
      }
      if ((full || placed) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        SkinTooltip(placed ? core::Tr("Déjà posé : un item ne tient qu'une ligne.")
                           : core::Tr("Toutes les lignes sont prises."));
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  PaintWindowScrollbar(art);
  ImGui::EndChild();
}

void DrawLines(BuyingStoreSetupState& state, const BuyingStoreSetupView& view, const SkinArt& art,
               float height, BuyingStoreSetupAction& action) {
  const float scale = Scale();
  ImGui::BeginChild("##buying_store_lines", ImVec2(-1.0F, height));
  if (state.lines.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Choisis un item ci-dessus et clique « Poser »."));
    PaintWindowScrollbar(art);
    ImGui::EndChild();
    return;
  }
  std::size_t removed = state.lines.size();
  if (ImGui::BeginTable("##buying_store_line_table", 6,
                        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(core::Tr("Possédé"), ImGuiTableColumnFlags_WidthFixed,
                            kOwnedColumnPixels * scale);
    ImGui::TableSetupColumn(core::Tr("Qté"), ImGuiTableColumnFlags_WidthFixed,
                            kAmountColumnPixels * scale);
    ImGui::TableSetupColumn(core::Tr("Prix"), ImGuiTableColumnFlags_WidthFixed,
                            kPriceColumnPixels * scale);
    ImGui::TableSetupColumn(core::Tr("Total"), ImGuiTableColumnFlags_WidthFixed,
                            kTotalColumnPixels * scale);
    ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed,
                            SkinSmallButtonWidth("x", art));
    ImGui::TableHeadersRow();
    for (std::size_t rank = 0; rank < state.lines.size(); ++rank) {
      BuyingStoreSetupLine& line = state.lines[rank];
      const BuyingStoreCandidateView* candidate = CandidateOf(view, line.item_id);
      ImGui::PushID(static_cast<int>(rank));
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      const TradeItemView item =
          candidate != nullptr ? candidate->item : TradeItemView{.item_id = line.item_id};
      NoteItem(DrawTradeItem(item), item, line.item_id, state.item_menu, action);
      // LA LIGNE EN CAUSE SE DÉSIGNE : le serveur refuserait l'échoppe entière
      // sans dire laquelle.
      if (rank < view.line_notes.size() && !view.line_notes[rank].empty()) {
        ImGui::SameLine();
        ImGui::TextColored(kTradeRefusedText, "(!)");
        if (ImGui::IsItemHovered()) {
          const std::string note(view.line_notes[rank]);
          SkinTooltip(note.c_str());
        }
      }
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(kTradeBlackText, "%d", candidate != nullptr ? candidate->owned : 0);
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(-1.0F);
      if (ImGui::InputInt("##amount", &line.amount, 0, 0)) {
        line.amount = std::clamp(line.amount, 0, std::max(view.most_amount, 0));
      }
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(-1.0F);
      if (ImGui::InputInt("##price", &line.price, 0, 0)) {
        line.price = std::max(line.price, 0);
      }
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::TextColored(
          kTradeBlackText, "%s",
          TradeAmountText(static_cast<std::int64_t>(line.price) * line.amount, "z").c_str());
      ImGui::TableNextColumn();
      if (view.awaiting) {
        ImGui::BeginDisabled();
      }
      if (SkinSmallButton("x", art)) {
        removed = rank;
      }
      if (view.awaiting) {
        ImGui::EndDisabled();
      }
      if (ImGui::IsItemHovered()) {
        SkinTooltip(core::Tr("Retirer du buying store"));
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  PaintWindowScrollbar(art);
  ImGui::EndChild();
  if (removed < state.lines.size()) {
    state.lines.erase(state.lines.begin() + static_cast<std::ptrdiff_t>(removed));
  }
}

void DrawFooter(BuyingStoreSetupState& state, const BuyingStoreSetupView& view, const SkinArt& art,
                BuyingStoreSetupAction& action) {
  const float scale = Scale();
  ImGui::Separator();
  ImGui::Text(core::Tr("Coût total : %s"), TradeAmountText(view.total_cost, "z").c_str());
  if (view.zeny != kTradeNoPurse) {
    ImGui::Text(core::Tr("Zeny : %s"), TradeAmountText(view.zeny, "z").c_str());
    ImGui::SameLine();
  }
  ImGui::TextDisabled(core::Tr("| %zu/%zu ligne(s)"), state.lines.size(), view.slots);

  ImGui::SetNextItemWidth(kLimitFieldPixels * scale);
  if (ImGui::InputScalar(core::Tr("Limite de zeny"), ImGuiDataType_S64, &state.zeny_limit)) {
    state.zeny_limit = std::max<std::int64_t>(state.zeny_limit, 0);
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("= total"), art)) {
    state.zeny_limit = std::min(view.total_cost, kWireMostZeny);
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("La limite prend le coût total : tout ce qui est voulu sera payé."));
  }

  // UNE LIGNE TOUJOURS ÉMISE, pour que le pied garde sa hauteur : le premier
  // refus en rouge, sinon l'avertissement, sinon rien.
  if (!view.blockers.empty()) {
    ImGui::TextColored(kTradeRefusedText, "%.*s", static_cast<int>(view.blockers[0].size()),
                       view.blockers[0].data());
  } else if (!view.warnings.empty()) {
    ImGui::TextColored(kTradeWarningText, "%.*s", static_cast<int>(view.warnings[0].size()),
                       view.warnings[0].data());
  } else if (view.awaiting) {
    ImGui::TextDisabled("%s", core::Tr("En attente du serveur..."));
  } else {
    ImGui::TextDisabled(" ");
  }
  ImGui::Separator();

  const bool blocked = !view.blockers.empty() || view.awaiting;
  if (blocked) {
    ImGui::BeginDisabled();
  }
  if (SkinButton(core::Tr("Ouvrir le buying store"), art)) {
    if (BuyingStoreSetupHasDearPrice(state)) {
      AskModal(kDearConfirmId);
    } else {
      action.open_store = true;
    }
  }
  if (blocked) {
    ImGui::EndDisabled();
  }
  if (!view.blockers.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(JoinedReasons(view.blockers).c_str());
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art)) {
    state.open = false;
  }
}

void DrawDearConfirmation(const SkinArt& art, BuyingStoreSetupAction& action) {
  std::array<char, 256> question{};
  std::snprintf(question.data(), question.size(),
                core::Tr("Un prix atteint %s par item.\nOuvrir le buying store quand même ?"),
                TradeAmountText(kBuyingStoreDearPrice, "z").c_str());
  if (DrawModalBox(kDearConfirmId, question.data(), core::Tr("Ouvrir"), core::Tr("Annuler"), art) ==
      ModalAnswer::kYes) {
    action.open_store = true;
  }
}

}  // namespace

void OpenBuyingStoreSetupOn(BuyingStoreSetupState& state, std::uint32_t preparation_seq) {
  state.open = true;
  state.preparation_seen = preparation_seq;
  state.lines.clear();
  state.zeny_limit = 0;
  state.filter.Clear();
}

bool AddBuyingStoreSetupLine(BuyingStoreSetupState& state, std::uint32_t item_id,
                             std::size_t slots) {
  if (state.lines.size() >= slots || Placed(state, item_id)) {
    return false;
  }
  state.lines.push_back(BuyingStoreSetupLine{.item_id = item_id});
  return true;
}

bool BuyingStoreSetupHasDearPrice(const BuyingStoreSetupState& state) {
  return std::ranges::any_of(state.lines, [](const BuyingStoreSetupLine& line) {
    return line.price >= kBuyingStoreDearPrice;
  });
}

BuyingStoreSetupAction DrawBuyingStoreSetupWindow(BuyingStoreSetupState& state,
                                                  const BuyingStoreSetupView& view,
                                                  const SkinArt& art, ItemLinkFacts link_facts) {
  BuyingStoreSetupAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels * scale, kFirstHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(kLeastWidthPixels * scale, kLeastHeightPixels * scale),
      ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
  const std::string_view shown =
      view.window_title.empty() ? std::string_view(core::Tr("Buying store")) : view.window_title;
  const std::string title = std::string(shown) + kBuyingStoreSetupWindowName;
  const bool body_shown =
      BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse);
  if (body_shown && state.open) {
    DrawName(state, view);
    ImGui::Separator();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(core::Tr("Items achetables"));
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu)", view.candidates.size());
    ImGui::SameLine();
    SkinHelpMarker(
        core::Tr("Ce sont les items de ton inventory : le serveur exige d'en posséder déjà un.\n"
                 "Il n'accepte que les items marqués achetables en buying store, et refusera "
                 "l'ouverture sinon, sans dire lequel."));
    ImGui::SetNextItemWidth(-1.0F);
    if (ImGui::InputTextWithHint("##buying_store_filter", core::Tr("Filtrer..."),
                                 state.filter.InputBuf, IM_ARRAYSIZE(state.filter.InputBuf))) {
      state.filter.Build();
    }

    // LA LISTE À POSER PREND LE SURPLUS, les lignes posées ce qu'il leur faut.
    const float line_height = ImGui::GetTextLineHeightWithSpacing();
    const float row_height = ImGui::GetFrameHeightWithSpacing();
    const float panes = ImGui::GetContentRegionAvail().y - state.footer_pixels - line_height;
    const float lines_wanted =
        std::max(kLeastLineRows, static_cast<float>(state.lines.size()) + 1.0F) * row_height;
    const float lines_height =
        std::clamp(lines_wanted, kLeastLineRows * row_height,
                   std::max(panes - kLeastCandidateRows * row_height, kLeastLineRows * row_height));
    const float candidates_height =
        std::max(panes - lines_height, kLeastCandidateRows * row_height);
    DrawCandidates(state, view, art, candidates_height, action);

    ImGui::TextUnformatted(core::Tr("Dans le buying store"));
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu/%zu)", state.lines.size(), view.slots);
    DrawLines(state, view, art,
              std::max(ImGui::GetContentRegionAvail().y - state.footer_pixels,
                       kLeastLineRows * row_height),
              action);

    const float footer_top = ImGui::GetCursorPosY();
    DrawFooter(state, view, art, action);
    state.footer_pixels = ImGui::GetCursorPosY() - footer_top;
    DrawDearConfirmation(art, action);
    if (const ChatLinkAsk chosen = DrawTradeItemMenu(kItemMenuId, state.item_menu, link_facts);
        chosen.gesture != ChatLinkGesture::kNothing) {
      action.describe_id = chosen.item_id;
      action.item_gesture = chosen.gesture;
    }
  }
  EndSkinnedWindow();
  if (!state.open) {
    action.cancel = true;
    action.open_store = false;
  }
  return action;
}

}  // namespace ro::ui
