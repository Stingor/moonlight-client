#include "ro/ui/wheel_gate.h"

// La possession d'une touche et le témoin de défilement vivent dans l'en-tête
// interne d'ImGui : ce sont ses mécaniques, pas les nôtres.
#include <imgui_internal.h>

namespace ro::ui {
namespace {

// Temps de pose du curseur avant qu'une zone puisse prendre la molette, et
// durée d'une salve : deux crans plus rapprochés que cela appartiennent au même
// geste, et un geste ne change pas de destinataire en cours de route. Réglés au
// ressenti — plus court, le survol de passage repasse ; plus
// long, le réglage se fait attendre.
constexpr double kHoverSettleSeconds = 0.30;
constexpr double kWheelBurstSeconds = 0.35;

}  // namespace

float WheelOver(WheelGateState& state, ImGuiID zone, bool hovered, bool engaged) {
  if (zone == 0 || (!hovered && !engaged)) {
    return 0.0F;
  }
  const double now = ImGui::GetTime();
  const int frame = ImGui::GetFrameCount();

  // 🔴 UN DÉFILEMENT A-T-IL VRAIMENT EU LIEU CETTE IMAGE ? ImGui le note
  // lui-même en APPLIQUANT le défilement, dans son début d'image. Compter les
  // crans ne le dirait pas : un cran au-dessus d'une liste qui n'a rien à faire
  // défiler ne doit rien verrouiller du tout.
  const ImGuiContext* context = ImGui::GetCurrentContext();
  const bool scrolled_now =
      context != nullptr && context->WheelingWindowScrolledFrame == context->FrameCount;
  if (scrolled_now) {
    // Une salve de crans reste au défilement jusqu'à son terme, y compris une
    // fois la liste arrivée en butée — c'est LÀ que le curseur s'immobilise sur
    // une zone et que le réglage accidentel se produirait.
    state.scroll_until = now + kWheelBurstSeconds;
  }

  if (hovered) {
    // ⚠ LE SURVOL EST NEUF SI LA ZONE CHANGE, MAIS AUSSI SI L'IMAGE PRÉCÉDENTE
    // NE SURVOLAIT PAS : sans ce second test, revenir sur une zone quittée
    // depuis longtemps hériterait de son ancien chrono.
    //
    // 🔴 SAUF QUAND C'EST LE DÉFILEMENT QUI A CHANGÉ LA ZONE, sans quoi le
    // portail serait INFRANCHISSABLE. Une grille glisse sous un curseur
    // immobile : chaque cran amène une case différente ; si le chrono
    // repartait de zéro, le délai de pose ne serait jamais atteint — la molette
    // ne ferait QUE défiler, pour toujours. Le joueur n'a pas visé ailleurs,
    // on ne repart pas de zéro.
    const bool moved_away = zone != state.hovered && !scrolled_now;
    if (moved_away || frame != state.hovered_frame + 1) {
      state.hovered_since = now;
    }
    state.hovered = zone;
    state.hovered_frame = frame;
  }

  const bool tuning = state.tuning == zone && now < state.tuning_until;
  if (!engaged && !tuning &&
      (now < state.scroll_until || now - state.hovered_since < kHoverSettleSeconds)) {
    return 0.0F;
  }
  ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, zone);
  const float wheel = ImGui::GetIO().MouseWheel;
  if (wheel == 0.0F) {
    return 0.0F;
  }
  state.tuning = zone;
  state.tuning_until = now + kWheelBurstSeconds;
  // Consommée : plus personne ne la relit.
  ImGui::GetIO().MouseWheel = 0.0F;
  return wheel;
}

float WheelOverItem() {
  const ImGuiContext* context = ImGui::GetCurrentContext();
  const ImGuiID item = ImGui::GetItemID();
  if (context == nullptr || item == 0 || !ImGui::IsItemHovered() || context->HoveredId != item) {
    return 0.0F;
  }
  // UNE SALVE EN COURS FAIT DÉFILER UNE FENÊTRE : ImGui retient la fenêtre à
  // laquelle il applique un défilement, et la relâche après une courte pause ou
  // un mouvement de la souris. Une salve sur une page qui n'a rien à faire
  // défiler ne lui en fait retenir aucune.
  const bool page_scrolling = context->WheelingWindow != nullptr;
  if (page_scrolling || static_cast<double>(context->HoveredIdTimer) < kHoverSettleSeconds) {
    return 0.0F;
  }
  ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, item);
  const float wheel = ImGui::GetIO().MouseWheel;
  if (wheel == 0.0F) {
    return 0.0F;
  }
  ImGui::GetIO().MouseWheel = 0.0F;
  return wheel;
}

}  // namespace ro::ui
