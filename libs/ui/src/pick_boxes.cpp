#include "ro/ui/pick_boxes.h"

#include <array>
#include <cstdio>

#include <imgui.h>

#include "ro/core/i18n.h"

namespace ro::ui {
namespace {

constexpr std::array<std::uint8_t, 4> kHoveredColor = {255, 235, 60, 255};
constexpr std::array<std::uint8_t, 4> kActorColor = {90, 210, 120, 200};
constexpr std::array<std::uint8_t, 4> kGroundItemColor = {110, 170, 255, 200};
constexpr std::array<std::uint8_t, 4> kSkillUnitColor = {255, 170, 70, 200};
constexpr std::array<std::uint8_t, 4> kOtherColor = {180, 180, 180, 170};
// Le cadre de plus autour de la zone du personnage joué, un pixel au-dehors.
constexpr std::array<std::uint8_t, 4> kOwnColor = {255, 120, 220, 160};
constexpr float kOwnOutsetPixels = 1.0F;
// Le fond de l'étiquette, et l'éclat du compte près du curseur.
constexpr std::array<std::uint8_t, 4> kTagBackdropColor = {0, 0, 0, 170};
constexpr std::array<std::uint8_t, 4> kCountColor = {255, 255, 255, 210};

// L'épaisseur des traits : épais pour la zone désignée, fins pour les autres.
constexpr float kHoveredThicknessPixels = 2.5F;
constexpr float kThicknessPixels = 1.0F;
// L'étiquette se pose au-dessus de la zone, à cet écart ; son fond déborde du
// texte de ces marges.
constexpr float kTagGapPixels = 2.0F;
constexpr float kTagMarginLeftPixels = 2.0F;
constexpr float kTagMarginRightPixels = 3.0F;
constexpr float kTagMarginVerticalPixels = 1.0F;
// Le compte se pose en bas à droite du curseur, à cet écart.
constexpr float kCountOffsetPixels = 14.0F;
// La plus longue étiquette : trois nombres et leurs trois noms.
constexpr std::size_t kTagCapacity = 96;

ImU32 Packed(const std::array<std::uint8_t, 4>& color) {
  return IM_COL32(color[0], color[1], color[2], color[3]);
}

}  // namespace

std::array<std::uint8_t, 4> PickBoxColor(const PickBoxesView& view, std::size_t rank) {
  if (view.hovered.has_value() && *view.hovered == rank) {
    return kHoveredColor;
  }
  switch (view.boxes[rank].category) {
    case PickCategory::kActor:
      return kActorColor;
    case PickCategory::kGroundItem:
      return kGroundItemColor;
    case PickCategory::kSkillUnit:
      return kSkillUnitColor;
    case PickCategory::kPet:
    case PickCategory::kCompanion:
      break;
  }
  return kOtherColor;
}

std::string PickBoxTag(const PickBoxView& box) {
  std::array<char, kTagCapacity> tag{};
  std::snprintf(tag.data(), tag.size(), "AID %u  job %u  cat %u", box.id, box.job,
                static_cast<unsigned>(box.category));
  return tag.data();
}

std::string PickBoxCount(const PickBoxesView& view) {
  std::array<char, kTagCapacity> text{};
  std::snprintf(text.data(), text.size(),
                view.hovered.has_value() ? core::Tr("%d zones — 1 sous le curseur")
                                         : core::Tr("%d zones — aucune sous le curseur"),
                static_cast<int>(view.boxes.size()));
  return text.data();
}

void DrawPickBoxes(const PickBoxesView& view) {
  if (view.boxes.empty()) {
    return;
  }
  ImDrawList* list = ImGui::GetForegroundDrawList();
  for (std::size_t rank = 0; rank < view.boxes.size(); ++rank) {
    const PickBoxView& box = view.boxes[rank];
    if (box.max.x <= box.min.x || box.max.y <= box.min.y) {
      continue;
    }
    const bool hovered = view.hovered.has_value() && *view.hovered == rank;
    list->AddRect(ImVec2(box.min.x, box.min.y), ImVec2(box.max.x, box.max.y),
                  Packed(PickBoxColor(view, rank)), 0.0F, 0,
                  hovered ? kHoveredThicknessPixels : kThicknessPixels);
    if (box.own) {
      list->AddRect(ImVec2(box.min.x - kOwnOutsetPixels, box.min.y - kOwnOutsetPixels),
                    ImVec2(box.max.x + kOwnOutsetPixels, box.max.y + kOwnOutsetPixels),
                    Packed(kOwnColor), 0.0F, 0, kThicknessPixels);
    }
    // L'ÉTIQUETTE AU SEUL DÉSIGNÉ : tout étiqueter rendrait l'écran illisible dès
    // qu'il y a du monde, et c'est lui qu'on cherche à identifier.
    if (hovered) {
      const std::string tag = PickBoxTag(box);
      const float font = ImGui::GetFontSize();
      const ImVec2 at(box.min.x, box.min.y - font - kTagGapPixels);
      list->AddRectFilled(ImVec2(at.x - kTagMarginLeftPixels, at.y - kTagMarginVerticalPixels),
                          ImVec2(at.x + ImGui::CalcTextSize(tag.c_str()).x + kTagMarginRightPixels,
                                 at.y + font + kTagMarginVerticalPixels),
                          Packed(kTagBackdropColor));
      list->AddText(at, Packed(kHoveredColor), tag.c_str());
    }
  }
  // Le compte, près du curseur : « combien de zones se disputent ce clic » est
  // déjà une réponse.
  const std::string count = PickBoxCount(view);
  list->AddText(ImVec2(view.mouse.x + kCountOffsetPixels, view.mouse.y + kCountOffsetPixels),
                Packed(kCountColor), count.c_str());
}

}  // namespace ro::ui
