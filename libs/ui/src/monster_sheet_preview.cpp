// L'aperçu au survol d'un lien de monstre : ce qui décide si l'on ouvre la
// fiche.

#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/monster_sheet_window.h"
#include "ro/ui/skinned_window.h"

#include "monster_sheet_parts.h"

namespace ro::ui {
namespace {

// Le cadre du sprite de l'aperçu, en pixels d'interface.
constexpr float kPreviewSpriteWidthPixels = 72.0F;
constexpr float kPreviewSpriteHeightPixels = 78.0F;

}  // namespace

void DrawMonsterSheetPreview(MonsterSheetPreviewState& state, const MonsterSheetPreview& preview,
                             const SkinArt& art) {
  using namespace monster_sheet;
  if (preview.shown_id == 0 || !BeginSkinnedTooltip(art)) {
    return;
  }
  switch (preview.phase) {
    case MonsterSheetPhase::kUnknown:
      Label(Printed(core::Tr("Monstre inconnu du serveur (#%u)."), preview.shown_id).c_str());
      break;
    case MonsterSheetPhase::kAsking:
      Label(core::Tr("Chargement de la fiche…"));
      break;
    case MonsterSheetPhase::kReady: {
      // Le sprite, VIVANT : son attente, comme dans la fiche, et selon le même
      // réglage.
      const ImVec2 box(Px(kPreviewSpriteWidthPixels), Px(kPreviewSpriteHeightPixels));
      state.sprite_box_width = box.x;
      state.sprite_box_height = box.y;
      if (preview.sprite.usable()) {
        ImGui::Image(preview.sprite.id, box);
        ImGui::SameLine();
      }
      ImGui::BeginGroup();
      Colored(kTitleColor, preview.name.empty() ? core::Tr("(sans nom)") : preview.name.c_str());
      DrawBossBadge(preview.boss_rank);
      const std::string size(preview.size_value);
      const std::string element(preview.element_value);
      ImGui::Text(core::Tr("Niv. %u   %s   %s niv. %u"), static_cast<unsigned>(preview.level),
                  size.c_str(), element.c_str(), static_cast<unsigned>(preview.element_level));
      const std::string race(preview.race_value);
      ImGui::TextUnformatted(race.c_str());
      // Les PV décident de tout ; le SP ne se lit que sur un monstre qui lance
      // quelque chose, d'où la ligne conditionnelle plutôt qu'un « SP 0 ».
      if (preview.sp > 0) {
        ImGui::Text(core::Tr("PV %u   SP %u"), preview.hp, preview.sp);
      } else {
        ImGui::Text(core::Tr("PV %u"), preview.hp);
      }
      ImGui::EndGroup();
      break;
    }
  }
  if (!preview.hint.empty()) {
    ImGui::Separator();
    ImGui::TextDisabled("%s", preview.hint.c_str());
  }
  EndSkinnedTooltip(art);
}

}  // namespace ro::ui
