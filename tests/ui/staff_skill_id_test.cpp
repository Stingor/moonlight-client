// L'IDENTIFIANT D'UNE COMPÉTENCE AU SURVOL, RÉSERVÉ AU STAFF : la ligne
// « ID : 5 (SM_BASH) » paraît dans l'infobulle du grimoire et dans celle de
// l'onglet du mercenaire quand le droit est là, et nulle part sans lui.

#include "ro/ui/staff_skill_id.h"

#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/mercenary_tab.h"
#include "ro/ui/skill_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 30.0F;
constexpr float kBenchWidthPixels = 640.0F;
constexpr float kBenchHeightPixels = 900.0F;
constexpr float kSweepStepXPixels = 12.0F;
constexpr float kSweepStepYPixels = 6.0F;

constexpr std::uint16_t kBashId = 5;
constexpr const char* kBashIdLine = "ID : 5 (SM_BASH)";
// Une ligne que l'infobulle de chaque banc porte toujours : elle prouve que
// l'infobulle s'est ouverte.
constexpr const char* kGrimoireTooltipMark = "Clic droit : menu";
constexpr const char* kMercenaryTooltipMark = "Clic droit : description";

struct Swept {
  bool tooltip_seen = false;
  bool id_seen = false;
};

// Promène la souris sur le banc, chaque image écrite au journal d'ImGui —
// infobulles comprises. S'arrête à la première image qui montre l'infobulle.
template <typename Draw>
[[nodiscard]] Swept Sweep(ro::ui::UiContext& ui, const char* tooltip_mark, Draw draw) {
  ImGuiIO& io = ImGui::GetIO();
  Swept swept;
  for (float y = 0.0F; y < kBenchHeightPixels; y += kSweepStepYPixels) {
    for (float x = 0.0F; x < kBenchWidthPixels; x += kSweepStepXPixels) {
      io.AddMousePosEvent(x, y);
      ui.BeginFrame(kViewport, kFrameSeconds);
      ImGui::LogToBuffer();
      ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
      ImGui::SetNextWindowSize(ImVec2(kBenchWidthPixels, kBenchHeightPixels));
      ImGui::Begin("banc", nullptr, ImGuiWindowFlags_NoTitleBar);
      draw();
      ImGui::End();
      const std::string logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
      ImGui::LogFinish();
      ui.EndFrame();
      if (logged.find(tooltip_mark) != std::string::npos) {
        swept.tooltip_seen = true;
        swept.id_seen = logged.find(kBashIdLine) != std::string::npos;
        return swept;
      }
    }
  }
  return swept;
}

[[nodiscard]] Swept SweepGrimoire(bool staff_allowed) {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::SkillWindowState state;
  const std::vector<ro::ui::SkillCellView> cells = {
      {.skill_id = kBashId, .position = 0, .name = "Bash", .idname = "SM_BASH", .learned = 1}};
  const std::vector<ro::ui::SkillGroupView> groups = {{.label = "Swordman", .cells = cells}};
  return Sweep(ui, kGrimoireTooltipMark, [&]() {
    static_cast<void>(ro::ui::DrawGrimoireTab(state, groups, {}, staff_allowed, art));
  });
}

[[nodiscard]] Swept SweepMercenary(bool staff_allowed) {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::MercenaryTabState state;
  state.skills_open = true;
  const std::vector<ro::ui::MercenarySkillRow> skills = {
      {.skill_id = kBashId, .level = 1, .name = "Bash", .idname = "SM_BASH"}};
  ro::ui::MercenaryTabView view;
  view.present = true;
  view.staff_allowed = staff_allowed;
  view.skills = skills;
  return Sweep(ui, kMercenaryTooltipMark,
               [&]() { static_cast<void>(ro::ui::DrawMercenaryTab(state, view, art)); });
}

}  // namespace

TEST_CASE("Identifiant de skill du staff : le numéro, puis le nom interne s'il est connu",
          "[ui][staff_skill_id]") {
  CHECK(ro::ui::StaffSkillIdText({.skill_id = kBashId, .idname = "SM_BASH"}) == kBashIdLine);
  CHECK(ro::ui::StaffSkillIdText({.skill_id = kBashId}) == "ID : 5");
  // Une compétence nulle ne désigne rien.
  CHECK(ro::ui::StaffSkillIdText({}).empty());
}

TEST_CASE("Grimoire : le staff lit l'identifiant au survol d'une case, pas un joueur",
          "[ui][staff_skill_id]") {
  const Swept staff = SweepGrimoire(/*staff_allowed=*/true);
  REQUIRE(staff.tooltip_seen);
  CHECK(staff.id_seen);
  const Swept player = SweepGrimoire(/*staff_allowed=*/false);
  REQUIRE(player.tooltip_seen);
  CHECK_FALSE(player.id_seen);
}

TEST_CASE("Mercenaire : le staff lit l'identifiant au survol d'un skill, pas un joueur",
          "[ui][staff_skill_id]") {
  const Swept staff = SweepMercenary(/*staff_allowed=*/true);
  REQUIRE(staff.tooltip_seen);
  CHECK(staff.id_seen);
  const Swept player = SweepMercenary(/*staff_allowed=*/false);
  REQUIRE(player.tooltip_seen);
  CHECK_FALSE(player.id_seen);
}
