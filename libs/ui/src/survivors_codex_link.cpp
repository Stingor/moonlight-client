#include "survivors_codex_link.h"

#include <algorithm>

#include "survivors_codex_text.h"

namespace ro::ui {
namespace {

constexpr ImU32 kShownTint = IM_COL32(255, 255, 255, 255);
// Une silhouette : l'image peinte en noir, sa forme seule.
constexpr ImU32 kSilhouetteTint = IM_COL32(0, 0, 0, 255);

}  // namespace

void PaintCodexImage(const SkinImage& image, ImVec2 box, bool seen) {
  const ImVec2 corner = ImGui::GetCursorScreenPos();
  ImGui::Dummy(box);
  if (!image.usable()) return;
  const float scale = std::min(box.x / image.width, box.y / image.height);
  const ImVec2 size(image.width * scale, image.height * scale);
  const ImVec2 from(corner.x + (box.x - size.x) * 0.5F, corner.y + (box.y - size.y) * 0.5F);
  ImGui::GetWindowDrawList()->AddImage(image.id, from, ImVec2(from.x + size.x, from.y + size.y),
                                       ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                                       seen ? kShownTint : kSilhouetteTint);
}

void DrawCodexLink(const SurvivorsCodexLink& link, float icon_side) {
  PaintCodexImage(link.icon, ImVec2(icon_side, icon_side), link.seen);
  ImGui::SameLine();
  ImGui::TextUnformatted(CodexLinkName(link));
}

}  // namespace ro::ui
