#include "ro/ui/chat_picture_viewer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "ui_text.h"

namespace ro::ui {
namespace {

// Un cran de molette multiplie ou divise le zoom d'autant.
constexpr float kViewerZoomStep = 1.25F;
// Les bornes du zoom : en deçà l'image n'est plus qu'un point, au-delà un
// pixel couvre l'écran.
constexpr float kViewerLeastZoom = 1.0F / 16.0F;
constexpr float kViewerMostZoom = 16.0F;
// La taille réelle : un pixel de l'image pour un pixel d'écran.
constexpr float kActualSize = 1.0F;
constexpr double kMsPerSecond = 1000.0;
constexpr float kHalf = 0.5F;

// LE ZOOM « AJUSTÉE » : l'image entière dans la toile, sans jamais l'agrandir.
// Une emote de trente pixels étirée sur tout l'écran ne serait qu'un flou ;
// la molette est là pour qui veut s'approcher.
[[nodiscard]] float FitZoom(const ChatPictureViewerView& view, ImVec2 canvas) {
  if (view.width_px <= 0.0F || view.height_px <= 0.0F) {
    return kActualSize;
  }
  return std::min({canvas.x / view.width_px, canvas.y / view.height_px, kActualSize});
}

// L'image à montrer à cet instant : la fixe, ou celle de l'animation.
[[nodiscard]] ImTextureID ShownTexture(const ChatPictureViewerView& view) {
  if (view.frames.empty()) {
    return view.texture;
  }
  const auto elapsed_ms = static_cast<std::uint64_t>(ImGui::GetTime() * kMsPerSecond);
  return view.frames[ChatPictureFrameAt(view.frames, elapsed_ms)].texture;
}

// PASSE À `next` EN GARDANT IMMOBILE LE POINT SOUS `anchor` : c'est ce que fait
// tout visionneur, et sans cela chaque cran ferait fuir ce qu'on regardait.
void ZoomAround(ChatPictureViewerState& state, float from, float next, ImVec2 middle,
                ImVec2 anchor) {
  const ImVec2 offset(anchor.x - middle.x - state.pan.x, anchor.y - middle.y - state.pan.y);
  const float ratio = next / from;
  state.pan =
      ImVec2(anchor.x - middle.x - offset.x * ratio, anchor.y - middle.y - offset.y * ratio);
  state.zoom = next;
}

// LES GESTES DE LA TOILE : molette, double-clic, glisser.
//
// ⚠ TANT QUE L'IMAGE EST AJUSTÉE, ELLE SUIT LA FENÊTRE : `zoom` nul veut dire
// « recalculé à chaque image ». Le premier geste fige le zoom courant, et c'est
// de là qu'il part.
void Handle(ChatPictureViewerState& state, float fit, ImVec2 middle, bool hovered) {
  const ImGuiIO& io = ImGui::GetIO();
  const float from = state.zoom > 0.0F ? state.zoom : fit;
  if (state.zoom <= 0.0F) {
    state.pan = ImVec2(0.0F, 0.0F);
  }
  if (hovered && io.MouseWheel != 0.0F) {
    const float next = std::clamp(from * std::pow(kViewerZoomStep, io.MouseWheel), kViewerLeastZoom,
                                  kViewerMostZoom);
    ZoomAround(state, from, next, middle, io.MousePos);
    return;
  }
  // LE DOUBLE-CLIC BASCULE : taille réelle, puis de nouveau ajustée.
  if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    if (state.zoom == kActualSize) {
      state.zoom = 0.0F;
      state.pan = ImVec2(0.0F, 0.0F);
    } else {
      ZoomAround(state, from, kActualSize, middle, io.MousePos);
    }
    return;
  }
  // ⚠ LE GLISSER SE LIT SUR L'ÉLÉMENT ACTIF, PAS SURVOLÉ : la souris sort de la
  // toile en cours de geste, et l'image ne doit pas lâcher en route.
  //
  // ⚠ ET SEULEMENT QUAND LA SOURIS BOUGE : un simple clic figerait sinon le
  // zoom ajusté, et le double-clic qui suit le prendrait pour la taille réelle.
  const bool moved = io.MouseDelta.x != 0.0F || io.MouseDelta.y != 0.0F;
  if (ImGui::IsItemActive() && moved) {
    state.zoom = from;
    state.pan = ImVec2(state.pan.x + io.MouseDelta.x, state.pan.y + io.MouseDelta.y);
  }
}

// Ce que la toile montre quand l'image n'est pas là.
[[nodiscard]] const char* StatusOf(ChatPictureState state) {
  return state == ChatPictureState::kFailed ? core::Tr("Aperçu indisponible.")
                                            : core::Tr("Chargement de l'aperçu…");
}

void DrawCanvas(ChatPictureViewerState& state, const ChatPictureViewerView& view, ImVec2 size,
                const SkinArt& art) {
  const ImVec2 at = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##toile", size);
  const ImVec2 middle(at.x + size.x * kHalf, at.y + size.y * kHalf);
  const bool ready =
      view.state == ChatPictureState::kReady && view.width_px > 0.0F && view.height_px > 0.0F;
  const float fit = FitZoom(view, size);
  if (ready) {
    // LA TOILE SURVOLÉE SE RÉSERVE LA MOLETTE : Ctrl+molette y zoome l'image,
    // et la caméra du jeu derrière la fenêtre n'en reçoit rien.
    if (ImGui::IsItemHovered()) {
      ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    }
    Handle(state, fit, middle, ImGui::IsItemHovered());
  }
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 far(at.x + size.x, at.y + size.y);
  into->PushClipRect(at, far, true);
  if (ready) {
    const float zoom = state.zoom > 0.0F ? state.zoom : fit;
    const ImVec2 pan = state.zoom > 0.0F ? state.pan : ImVec2(0.0F, 0.0F);
    const ImVec2 shown(view.width_px * zoom, view.height_px * zoom);
    const ImVec2 top_left(middle.x + pan.x - shown.x * kHalf, middle.y + pan.y - shown.y * kHalf);
    const ImTextureID texture = ShownTexture(view);
    if (texture != 0) {
      into->AddImage(texture, top_left, ImVec2(top_left.x + shown.x, top_left.y + shown.y));
    }
  } else {
    const char* status = StatusOf(view.state);
    const ImVec2 text = ImGui::CalcTextSize(status);
    into->AddText(ImVec2(middle.x - text.x * kHalf, middle.y - text.y * kHalf),
                  art.look().faint_text, status);
  }
  into->PopClipRect();
}

}  // namespace

void DrawChatPictureViewer(ChatPictureViewerState& state, const ChatPictureViewerView& view,
                           const SkinArt& art) {
  if (!state.open) {
    return;
  }
  // ⚠ SON IDENTITÉ EST `kChatPictureViewerId` : c'est par elle que la pile
  // d'Échap la retrouve et la ferme.
  static WindowName name("Image", kChatPictureViewerId);
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  const ImVec2 most(screen->Size.x * kMaxScreenFraction, screen->Size.y * kMaxScreenFraction);
  // 🔴 CHAQUE OUVERTURE REPART AJUSTÉE, à 90 % de l'écran et au centre : une
  // autre image n'a rien à faire du zoom ni du cadrage de la précédente.
  if (state.raise) {
    state.raise = false;
    state.zoom = 0.0F;
    state.pan = ImVec2(0.0F, 0.0F);
    ImGui::SetNextWindowSize(most, ImGuiCond_Always);
    ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Always, ImVec2(kHalf, kHalf));
    ImGui::SetNextWindowFocus();
  } else {
    ImGui::SetNextWindowSize(most, ImGuiCond_FirstUseEver);
  }
  ConstrainNextWindowToScreen(name.Get());
  // ⚠ LA MOLETTE APPARTIENT AU ZOOM : la fenêtre ne défile pas.
  if (!BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                              ImGuiWindowFlags_NoCollapse)) {
    EndSkinnedWindow();
    return;
  }
  ImVec2 canvas = ImGui::GetContentRegionAvail();
  canvas.y -= SkinFooterHeight(art);
  canvas = ImVec2(std::max(canvas.x, 1.0F), std::max(canvas.y, 1.0F));
  DrawCanvas(state, view, canvas, art);

  if (SkinButton(core::Tr("Enregistrer"), art)) {
    state.answer = ChatViewerAnswer::kSave;
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Ouvrir dans le navigateur"), art)) {
    state.answer = ChatViewerAnswer::kOpenInBrowser;
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Copier l'adresse"), art)) {
    state.answer = ChatViewerAnswer::kCopyAddress;
  }
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ColoredText(art.look().faint_text, core::Tr("Molette : zoom · double-clic : taille réelle"));
  EndSkinnedWindow();
}

}  // namespace ro::ui
