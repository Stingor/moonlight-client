#include "ro/ui/camera_hud.h"

#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "imgui.h"

namespace ro::ui {
namespace {

constexpr float kFirstX = 16.0F;
constexpr float kFirstY = 120.0F;

constexpr float kBackgroundAlpha = 0.6F;

void Row(const char* label, float ours, const char* ours_unit, float native,
         const char* native_unit) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::TextUnformatted(label);
  ImGui::TableNextColumn();
  // `+ 0` change un zéro négatif en zéro : « -0.00 » ne dit rien de plus.
  ImGui::Text("%.2f %s", static_cast<double>(ours + 0.0F), ours_unit);
  ImGui::TableNextColumn();
  ImGui::Text("%.2f %s", static_cast<double>(native + 0.0F), native_unit);
}

void Point(const char* label, const std::array<float, 3>& point) {
  ImGui::Text("%s  %.1f  %.1f  %.1f", label, static_cast<double>(point[0]),
              static_cast<double>(point[1]), static_cast<double>(point[2]));
}

}  // namespace

void DrawCameraHud(const CameraHudView& view, bool& open) {
  if (!open) {
    return;
  }
  static WindowName name(kCameraHudTitle, kCameraHudId);
  ImGui::SetNextWindowPos(ImVec2(kFirstX, kFirstY), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(kBackgroundAlpha);
  ConstrainNextWindowToScreen(name.Get());
  // Ne vole ni le clavier ni la souris au jeu pendant qu'on manipule la caméra.
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_AlwaysAutoResize |
                                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                      ImGuiWindowFlags_NoCollapse;
  if (!ImGui::Begin(name.Get(), &open, kFlags)) {
    ImGui::End();
    return;
  }

  ImGui::Text("%.*s — %.*s", static_cast<int>(view.map_name.size()), view.map_name.data(),
              static_cast<int>(view.map_kind.size()), view.map_kind.data());
  ImGui::TextUnformatted(view.bounded ? core::Tr("bornée") : core::Tr("SANS BORNES"));
  if (view.ctrl_held) {
    ImGui::SameLine();
    ImGui::TextUnformatted(core::Tr("· Ctrl tenu"));
  }

  if (ImGui::BeginTable("camera_hud_values", 3, ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("");
    ImGui::TableSetupColumn(core::Tr("client"));
    ImGui::TableSetupColumn(core::Tr("jeu"));
    ImGui::TableHeadersRow();
    Row(core::Tr("Lacet"), view.yaw_degrees, "°", view.native_yaw_degrees, "°");
    Row(core::Tr("Élévation"), view.pitch_degrees, "°", view.native_altitude_degrees, "°");
    Row(core::Tr("Recul"), view.distance, "", view.native_zoom, "");
    ImGui::EndTable();
  }
  ImGui::Text("%s %.1f°", core::Tr("Champ"), static_cast<double>(view.fov_degrees));

  ImGui::SeparatorText(core::Tr("Bornes (jeu)"));
  ImGui::Text("%s %.1f … %.1f", core::Tr("Recul"), static_cast<double>(view.zoom_nearest),
              static_cast<double>(view.zoom_farthest));
  ImGui::Text("%s %.1f … %.1f (Ctrl %.1f)", core::Tr("Altitude"),
              static_cast<double>(view.altitude_floor), static_cast<double>(view.altitude_ceiling),
              static_cast<double>(view.altitude_ceiling_ctrl));

  ImGui::SeparatorText(core::Tr("Position"));
  Point(core::Tr("Visé"), view.look_at);
  Point(core::Tr("Œil"), view.eye);
  // Sans bornes, le plancher vaut le plus petit flottant : rien à montrer.
  if (view.bounded) {
    ImGui::Text("%s %.1f %s", core::Tr("Plancher de l'œil"),
                static_cast<double>(view.eye_floor_height),
                view.eye_floored ? core::Tr("(posé)") : "");
  }

  ImGui::End();
}

}  // namespace ro::ui
