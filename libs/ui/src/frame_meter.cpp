#include "ro/ui/frame_meter.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
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
// La poignée de redimensionnement : à peine visible au repos, pour ne pas salir
// le jeu, franche sous la souris.
constexpr ImU32 kGripColor = IM_COL32(255, 255, 255, 50);
constexpr ImU32 kGripHoveredColor = IM_COL32(255, 255, 255, 140);
constexpr ImU32 kGripActiveColor = IM_COL32(255, 255, 255, 220);
constexpr int kGripColorCount = 3;

constexpr float kMillisecondsPerSecond = 1000.0F;
// La courbe à l'échelle un, en pixels d'art.
constexpr float kGraphWidthPixels = 140.0F;
constexpr float kGraphHeightPixels = 32.0F;

// L'échelle que la poignée donne au compteur, texte et courbe ensemble.
constexpr float kLeastMeterScale = 0.75F;
constexpr float kMostMeterScale = 4.0F;

// Les valeurs les plus larges qu'on s'attend à lire. La largeur du compteur
// s'en déduit, pour qu'elle ne bouge pas à chaque chiffre qui change.
constexpr double kWidestFrameRate = 999.0;
constexpr int kWidestRoundTripMs = 999;

constexpr std::size_t kLineCapacity = 64;
using MeterLine = std::array<char, kLineCapacity>;

// ⚠ JAMAIS TRADUITE : c'est une clé ImGui, et la changer de langue en ferait un
// autre panneau — qui repartirait au coin de l'écran.
constexpr const char* kMeterId = "##frame_meter";

// Le contenu à l'échelle un, et la marge de la fenêtre qui ne s'échelonne pas.
struct MeterFit {
  ImVec2 content;
  ImVec2 padding;
};

float ScaleOf(const MeterFit& fit, ImVec2 window_size) {
  return std::clamp((window_size.y - fit.padding.y * 2.0F) / fit.content.y, kLeastMeterScale,
                    kMostMeterScale);
}

// 🔴 LA POIGNÉE NE CHOISIT QU'UNE ÉCHELLE, jamais une forme : le compteur garde
// ses proportions, et le geste dans l'un ou l'autre sens l'agrandit. Une
// fenêtre étirée sur un seul axe ne ferait que montrer du fond.
void KeepProportions(ImGuiSizeCallbackData* data) {
  const MeterFit& fit = *static_cast<const MeterFit*>(data->UserData);
  const float scale_across = (data->DesiredSize.x - fit.padding.x * 2.0F) / fit.content.x;
  const float scale_down = (data->DesiredSize.y - fit.padding.y * 2.0F) / fit.content.y;
  // ⚠ L'AXE QUI A LE PLUS BOUGÉ L'EMPORTE, et la hauteur hors geste. C'est
  // elle qui porte l'échelle : la largeur change avec les lignes qu'on montre,
  // et la relire ferait grandir le compteur à chaque ligne retirée.
  const float moved_across = std::abs(data->DesiredSize.x - data->CurrentSize.x);
  const float moved_down = std::abs(data->DesiredSize.y - data->CurrentSize.y);
  const float scale = std::clamp(moved_across > moved_down ? scale_across : scale_down,
                                 kLeastMeterScale, kMostMeterScale);
  data->DesiredSize = ImVec2(fit.content.x * scale + fit.padding.x * 2.0F,
                             fit.content.y * scale + fit.padding.y * 2.0F);
}

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

  // 🔴 LA TAILLE DE LA FENÊTRE EST LE RÉGLAGE. Elle se garde comme la place, et
  // l'échelle s'en relit à chaque image : il n'y a pas de second chemin à
  // tenir d'accord avec elle.
  const ImGuiStyle& style = ImGui::GetStyle();
  MeterLine sample = {};
  std::snprintf(sample.data(), sample.size(), core::Tr("%.0f images/s"), kWidestFrameRate);
  const float line_height = ImGui::GetTextLineHeight();
  MeterFit fit{ImVec2(ImGui::CalcTextSize(sample.data()).x, line_height), style.WindowPadding};
  if (state.ping) {
    std::snprintf(sample.data(), sample.size(), core::Tr("%d ms au serveur"), kWidestRoundTripMs);
    fit.content.x = std::max(fit.content.x, ImGui::CalcTextSize(sample.data()).x);
    fit.content.y += style.ItemSpacing.y + line_height;
  }
  if (state.graph) {
    fit.content.x = std::max(fit.content.x, kGraphWidthPixels);
    fit.content.y += style.ItemSpacing.y + kGraphHeightPixels;
  }
  ImGui::SetNextWindowSize(
      ImVec2(fit.content.x + fit.padding.x * 2.0F, fit.content.y + fit.padding.y * 2.0F),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(0.0F, 0.0F), ImVec2(FLT_MAX, FLT_MAX), KeepProportions,
                                      &fit);

  ImGui::PushStyleColor(ImGuiCol_WindowBg, kMeterBackground);
  ImGui::PushStyleColor(ImGuiCol_ResizeGrip, kGripColor);
  ImGui::PushStyleColor(ImGuiCol_ResizeGripHovered, kGripHoveredColor);
  ImGui::PushStyleColor(ImGuiCol_ResizeGripActive, kGripActiveColor);
  // ⚠ LE LISERÉ VIENT DE `WindowBorderSize`, qui est un style et non un
  // drapeau. Sans cette ligne le compteur porte un cadre que personne n'a
  // demandé, et qui reste visible même le fond rendu transparent.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  // ⚠ PAS `NoDecoration` : il porte `NoResize`, qui retire la poignée.
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
                           ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;
  if (state.locked) {
    // Figé, il perd aussi sa poignée : un geste qu'on ne peut plus faire ne se
    // montre pas.
    flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoInputs;
  }
  if (ImGui::Begin(kMeterId, nullptr, flags)) {
    const float scale = ScaleOf(fit, ImGui::GetWindowSize());
    // ⚠ L'ÉCART ENTRE LIGNES S'ÉCHELONNE AVEC ELLES : resté fixe, il ferait
    // déborder le contenu de la hauteur calculée pour lui.
    ImGui::PushFont(nullptr, style.FontSizeBase * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(style.ItemSpacing.x * scale, style.ItemSpacing.y * scale));

    MeterLine line = {};
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
                       ImVec2(ImGui::GetContentRegionAvail().x, kGraphHeightPixels * scale));
      ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();
    ImGui::PopFont();
  }
  ImGui::End();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(1 + kGripColorCount);
}

}  // namespace ro::ui
