#include "ro/ui/status_preview.h"

#include <array>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

// Le jour entre l'icône et le texte qui la suit.
constexpr float kIconGapPixels = 8.0F;

// Le nom montré, ou le numéro à défaut.
//
// ⚠ LE NUMÉRO N'EST PAS UN PIS-ALLER MUET : c'est la clé qu'on retrouve dans
// l'énumération `efst_type` du serveur, donc ce qu'un joueur peut redire à
// quelqu'un qui saura.
[[nodiscard]] std::string ShownName(const StatusPreviewView& view) {
  if (!view.name.empty()) {
    return std::string(view.name);
  }
  std::array<char, 64> numbered = {};
  std::snprintf(numbered.data(), numbered.size(), core::Tr("État %u"), view.effect_id);
  return numbered.data();
}

}  // namespace

void DrawStatusPreview(const StatusPreviewView& view, const SkinArt& art) {
  if (view.effect_id == 0) {
    return;
  }
  if (!BeginSkinnedTooltip(art)) {
    return;
  }
  if (view.icon.usable()) {
    const float side = kStatusPreviewIconPixels * ImGui::GetStyle().FontScaleMain;
    ImGui::Image(view.icon.id, ImVec2(side, side));
    ImGui::SameLine(0.0F, kIconGapPixels);
  }
  ImGui::BeginGroup();
  ImGui::TextUnformatted(ShownName(view).c_str());
  if (!view.description.empty()) {
    ImGui::Separator();
    for (const std::string_view line : view.description) {
      ImGui::TextUnformatted(line.data(), line.data() + line.size());
    }
  }
  ImGui::EndGroup();
  EndSkinnedTooltip(art);
}

}  // namespace ro::ui
