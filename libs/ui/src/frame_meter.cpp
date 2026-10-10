#include "ro/ui/frame_meter.h"

#include <array>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_text.h"

namespace ro::ui {
namespace {

// Le fond : noir à peine posé. ⚠ PAS `SetNextWindowBgAlpha`, qui ne règle que
// la transparence : la couleur entière se choisit ici.
constexpr ImU32 kMeterBackground = IM_COL32(0, 0, 0, 115);
constexpr ImU32 kMeterText = IM_COL32_WHITE;
// L'ombre du texte. ⚠ TOUJOURS LÀ, et ce n'est pas un réglage de plus : le
// compteur se pose sur le JEU, dont la couleur change à chaque pas. Un chiffre
// blanc sur un sol clair disparaît, et c'est précisément quand tout rame qu'on
// vient le lire.
constexpr ImU32 kMeterShadow = IM_COL32(0, 0, 0, 200);

constexpr float kMillisecondsPerSecond = 1000.0F;
// La courbe, en pixels d'art.
constexpr float kGraphWidthPixels = 140.0F;
constexpr float kGraphHeightPixels = 32.0F;

// ⚠ JAMAIS TRADUITE : c'est une clé ImGui, et la changer de langue en ferait un
// autre panneau — qui repartirait au coin de l'écran.
constexpr const char* kMeterId = "##frame_meter";

void ShadowedLine(const char* text) {
  const ImVec2 at = ImGui::GetCursorScreenPos();
  AddTextHalo(ImGui::GetWindowDrawList(), at, kMeterText, text, kMeterShadow);
  ImGui::Dummy(ImGui::CalcTextSize(text));
}

}  // namespace

void DrawFrameMeter(FrameMeterState& state, std::int32_t round_trip_ms) {
  const ImGuiIO& io = ImGui::GetIO();
  // ⚠ L'HISTORIQUE SE TIENT MÊME CACHÉ. Le perdre à la fermeture le ferait
  // repartir vide à la réouverture — juste au moment où l'on vient voir ce qui
  // s'est passé.
  state.history[state.head] = io.DeltaTime * kMillisecondsPerSecond;
  state.head = (state.head + 1) % kFrameHistoryLength;
  if (!state.shown) {
    return;
  }

  ImGui::PushStyleColor(ImGuiCol_WindowBg, kMeterBackground);
  // ⚠ `NoDecoration` NE RETIRE PAS LA BORDURE : il enlève le titre, la poignée
  // et l'ascenseur, mais le liseré vient de `WindowBorderSize`, qui est un
  // style. Sans cette ligne le compteur porte un cadre que personne n'a demandé,
  // et qui reste visible même le fond rendu transparent.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                           ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;
  if (state.locked) {
    flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
  }
  if (ImGui::Begin(kMeterId, nullptr, flags)) {
    std::array<char, 64> line = {};
    std::snprintf(line.data(), line.size(), core::Tr("%.0f images/s"),
                  static_cast<double>(io.Framerate));
    ShadowedLine(line.data());

    if (state.ping) {
      // 🔴 DIRE L'ABSENCE PLUTÔT QU'UN ZÉRO. Le battement d'horloge n'a lieu que
      // toutes les quelques secondes : avant le premier, il n'y a rien à
      // montrer — et « 0 ms » se lirait comme une mesure, qui plus est
      // vraisemblable sur un serveur local.
      if (round_trip_ms < 0) {
        ShadowedLine(core::Tr("-- ms au serveur"));
      } else {
        std::snprintf(line.data(), line.size(), core::Tr("%d ms au serveur"), round_trip_ms);
        ShadowedLine(line.data());
      }
    }

    if (state.graph) {
      // 🔴 « ms/image » ET NON « ms » TOUT COURT. C'est la seule chose qui
      // empêche de le lire comme une latence réseau, et les deux se mesurent
      // dans la même unité.
      std::snprintf(line.data(), line.size(), core::Tr("%.1f ms/image"),
                    static_cast<double>(kMillisecondsPerSecond / io.Framerate));
      ImGui::PushStyleColor(ImGuiCol_Text, kMeterText);
      ImGui::PlotLines("##courbe", state.history.data(), static_cast<int>(kFrameHistoryLength),
                       static_cast<int>(state.head), line.data(), 0.0F, kFrameGraphCeilingMs,
                       ImVec2(kGraphWidthPixels, kGraphHeightPixels));
      ImGui::PopStyleColor();
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

}  // namespace ro::ui
