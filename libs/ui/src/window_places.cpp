#include "ro/ui/window_places.h"

#include <cfloat>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/ui/window_bounds.h"

namespace ro::ui {
namespace {

// L'espace LIBRE se coupe en trois sur chaque axe : une fenêtre dont l'écart à
// un bord tient dans le tiers de cet espace s'ancre à ce bord.
constexpr float kThirds = 3.0F;
constexpr float kHalf = 0.5F;

// LE BORD LE PLUS PROCHE, voir l'en-tête. Une fenêtre qui remplit l'axe n'a
// pas d'espace libre : elle s'ancre au départ, comme le bornage l'y colle.
[[nodiscard]] AnchorSide SideOf(float start_px, float extent_px, float screen_px) {
  const float free_px = screen_px - extent_px;
  if (free_px <= 0.0F) {
    return AnchorSide::kStart;
  }
  const float gap_start_px = start_px;
  const float gap_end_px = free_px - start_px;
  const float third_px = free_px / kThirds;
  if (gap_start_px <= third_px && gap_start_px <= gap_end_px) {
    return AnchorSide::kStart;
  }
  if (gap_end_px <= third_px) {
    return AnchorSide::kEnd;
  }
  return AnchorSide::kCenter;
}

[[nodiscard]] float OffsetOf(AnchorSide side, float start_px, float extent_px, float screen_px) {
  switch (side) {
    case AnchorSide::kStart:
      return start_px;
    case AnchorSide::kEnd:
      return screen_px - (start_px + extent_px);
    case AnchorSide::kCenter:
      return start_px + extent_px * kHalf - screen_px * kHalf;
  }
  return start_px;
}

[[nodiscard]] float StartOf(AnchorSide side, float offset_px, float extent_px, float screen_px) {
  switch (side) {
    case AnchorSide::kStart:
      return offset_px;
    case AnchorSide::kEnd:
      return screen_px - offset_px - extent_px;
    case AnchorSide::kCenter:
      return screen_px * kHalf + offset_px - extent_px * kHalf;
  }
  return offset_px;
}

[[nodiscard]] float UsableScale(float interface_scale) {
  return interface_scale > 0.0F ? interface_scale : 1.0F;
}

// LA CLÉ D'UNE FENÊTRE : ce qu'ImGui hache de son nom. Voir `LayoutProfile`.
[[nodiscard]] std::string KeyOf(const ImGuiWindow& window) {
  return ImHashSkipUncontributingPrefix(window.Name);
}

// LA TAILLE VOULUE VAUT POUR CETTE FENÊTRE, et suit donc l'échelle. ⚠ SAUF
// QUAND SON CONTENU LA DICTE : elle se recalcule à chaque image.
[[nodiscard]] bool SizeFollowsPlace(const ImGuiWindow& window) {
  return (window.Flags & ImGuiWindowFlags_AlwaysAutoResize) == 0;
}

// LA TAILLE SE PLAFONNE À UNE PART D'ÉCRAN. ⚠ PAS CELLE QUE L'APPELANT OU LE
// VERROU DU JOUEUR FIGE : la couper tronquerait ce qu'elle montre.
[[nodiscard]] bool SizeCapped(const ImGuiWindow& window) {
  return (window.Flags & kWindowSizeNotOurs) == 0;
}

// La taille qu'on donne : celle voulue, au plus `kMaxScreenFraction` de l'écran.
[[nodiscard]] ImVec2 Bounded(ImVec2 size, ImVec2 screen) {
  return ImVec2(ImMin(size.x, screen.x * kMaxScreenFraction),
                ImMin(size.y, screen.y * kMaxScreenFraction));
}

// ⚠ ARRONDI ET NON TRONQUÉ : une place passée par l'échelle et revenue tombe
// parfois une poussière sous l'entier, et la tronquer décalerait d'un pixel.
[[nodiscard]] ImVec2 Rounded(ImVec2 value) {
  return ImVec2(ImFloor(value.x + kHalf), ImFloor(value.y + kHalf));
}

// La place que tient la fenêtre à l'écran : sa seule barre de titre, repliée.
// ⚠ C'EST ELLE QUI SE BORNE : replier une fenêtre posée en bas d'écran ne la
// fait pas remonter.
[[nodiscard]] ImVec2 Occupied(ImVec2 size, bool collapsed, float title_bar_height_px) {
  return collapsed ? ImVec2(size.x, title_bar_height_px) : size;
}

struct Shown {
  ImVec2 at;
  ImVec2 size;
};

// LA FENÊTRE SOUS LA POIGNÉE DU JOUEUR, bornée : la taille s'arrête au bord de
// l'écran et à sa part, et le bord que le geste ne tire pas reste où il est.
// Voir `HeldResize`.
[[nodiscard]] Shown UnderTheHand(const ImGuiWindow& window, const HeldResize& resize, ImVec2 screen,
                                 bool capped) {
  ImVec2 size = capped ? Bounded(window.SizeFull, screen) : window.SizeFull;
  size = ImMin(size, resize.largest_px);
  ImVec2 at = window.Pos;
  if (resize.moves_left) {
    at.x += window.SizeFull.x - size.x;
  }
  if (resize.moves_top) {
    at.y += window.SizeFull.y - size.y;
  }
  return Shown{.at = at, .size = size};
}

// LA FENÊTRE N'A CHANGÉ QUE DE TAILLE, et personne ne la tient : son contenu,
// son appelant ou une contrainte l'ont redimensionnée. Voir l'en-tête.
[[nodiscard]] bool OnlyResized(const ImGuiWindow& window, float posed_x_px, float posed_y_px,
                               bool posed_collapsed) {
  return window.Pos.x == posed_x_px && window.Pos.y == posed_y_px &&
         window.Collapsed == posed_collapsed;
}

// CE QU'UNE PLACE VOULUE DONNE À L'ÉCRAN, bornée. `measured` : la taille que
// la fenêtre a quand ce n'est pas la place qui la dit.
[[nodiscard]] Shown ShownOf(WindowPlace place, ImVec2 screen, float scale, const ImVec2* measured,
                            bool capped, float title_bar_height_px) {
  ImVec2 size =
      measured != nullptr ? *measured : Rounded(ImVec2(place.width * scale, place.height * scale));
  if (capped) {
    size = ImFloor(Bounded(size, screen));
  }
  // ⚠ L'ANCRE SE CALCULE AVEC LA TAILLE AFFICHÉE : c'est le bord ancré qui doit
  // tenir quand la taille change.
  place.width = size.x / scale;
  place.height = size.y / scale;
  const ScreenRect rect = RectFromPlace(place, screen.x, screen.y, scale);
  const ImVec2 at = Rounded(ClampWindowPosToScreen(
      ImVec2(rect.x_px, rect.y_px), Occupied(size, place.collapsed, title_bar_height_px)));
  return Shown{.at = at, .size = size};
}

}  // namespace

WindowPlace PlaceFromRect(const ScreenRect& rect, float screen_width_px, float screen_height_px,
                          float interface_scale) {
  const float scale = UsableScale(interface_scale);
  WindowPlace place;
  place.across = SideOf(rect.x_px, rect.width_px, screen_width_px);
  place.down = SideOf(rect.y_px, rect.height_px, screen_height_px);
  place.offset_x = OffsetOf(place.across, rect.x_px, rect.width_px, screen_width_px) / scale;
  place.offset_y = OffsetOf(place.down, rect.y_px, rect.height_px, screen_height_px) / scale;
  place.width = rect.width_px / scale;
  place.height = rect.height_px / scale;
  return place;
}

ScreenRect RectFromPlace(const WindowPlace& place, float screen_width_px, float screen_height_px,
                         float interface_scale) {
  const float scale = UsableScale(interface_scale);
  ScreenRect rect;
  rect.width_px = place.width * scale;
  rect.height_px = place.height * scale;
  rect.x_px = StartOf(place.across, place.offset_x * scale, rect.width_px, screen_width_px);
  rect.y_px = StartOf(place.down, place.offset_y * scale, rect.height_px, screen_height_px);
  return rect;
}

bool WindowPlacer::Differs(const Posed& posed, const ImGuiWindow& window) {
  return window.Pos.x != posed.x_px || window.Pos.y != posed.y_px ||
         window.Collapsed != posed.collapsed ||
         (SizeFollowsPlace(window) &&
          (window.SizeFull.x != posed.width_px || window.SizeFull.y != posed.height_px));
}

void WindowPlacer::Use(const std::map<std::string, WindowPlace>& places) {
  places_ = places;
  // 🔴 CE QUE LA DISPOSITION NE NOMME PAS REPREND SA PLACE D'ORIGINE : sans
  // cela, un profil hériterait des places du précédent, et « réinitialiser »
  // ne remettrait rien. Une fenêtre vivante rouvre la consigne de première
  // ouverture que son code lui donne ; une fenêtre à naître perd ses réglages.
  if (ImGuiContext* context = ImGui::GetCurrentContext(); context != nullptr) {
    for (ImGuiWindow* window : context->Windows) {
      if (window != nullptr && (window->Flags & kWindowOwnedByImGui) == 0 &&
          !places_.contains(KeyOf(*window))) {
        window->SetWindowPosAllowFlags |= ImGuiCond_FirstUseEver;
        window->SetWindowSizeAllowFlags |= ImGuiCond_FirstUseEver;
      }
    }
    for (ImGuiWindowSettings* settings = context->SettingsWindows.begin(); settings != nullptr;
         settings = context->SettingsWindows.next_chunk(settings)) {
      if (!places_.contains(ImHashSkipUncontributingPrefix(settings->GetName())) &&
          ImGui::FindWindowByID(settings->ID) == nullptr) {
        settings->WantDelete = true;
      }
    }
  }
  // ⚠ CE QUI A ÉTÉ POSÉ NE VAUT PLUS : sans cela, la passe prendrait l'écart
  // entre l'ancienne place et la nouvelle pour un geste du joueur.
  posed_.clear();
  prepared_.clear();
  unborn_stale_ = true;
  ++revision_;
}

void WindowPlacer::Place(float interface_scale) {
  ImGuiContext* context = ImGui::GetCurrentContext();
  if (context == nullptr) {
    return;
  }
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  // 🔴 UN ÉCRAN NUL NE POSE RIEN : fenêtre du jeu réduite. Tout s'y rabattrait
  // en (0,0), et cette place-là serait retenue comme voulue.
  if (screen.x <= 0.0F || screen.y <= 0.0F) {
    return;
  }
  const float scale = UsableScale(interface_scale);
  if (screen.x != prepared_width_px_ || screen.y != prepared_height_px_ ||
      scale != prepared_scale_) {
    unborn_stale_ = true;
  }

  for (ImGuiWindow* window : context->Windows) {
    if (window == nullptr || (window->Flags & kWindowOwnedByImGui) != 0) {
      continue;
    }
    // Centrage à pivot en attente : c'est `Begin` qui placera la fenêtre cette
    // image, à partir de `SetWindowPosVal`. La place se lira à la suivante.
    if (window->SetWindowPosVal.x != FLT_MAX) {
      continue;
    }
    // Jamais dessinée : sa taille n'est pas encore mesurée.
    if (window->SizeFull.x <= 0.0F || window->SizeFull.y <= 0.0F) {
      continue;
    }
    const bool size_follows = SizeFollowsPlace(*window);
    const bool capped = SizeCapped(*window);
    const HeldResize resize = HeldResizeOf(window);

    // UNE FENÊTRE QU'ON NE RETIENT PAS n'a pas de place voulue : elle se borne
    // dans l'écran, là où elle est.
    if ((window->Flags & ImGuiWindowFlags_NoSavedSettings) != 0) {
      if (resize.held) {
        const Shown held = UnderTheHand(*window, resize, screen, capped);
        window->Pos = held.at;
        window->SizeFull = held.size;
        window->Size = held.size;
        continue;
      }
      const ImVec2 size = capped ? ImFloor(Bounded(window->SizeFull, screen)) : window->SizeFull;
      window->SizeFull = size;
      if (!window->Collapsed) {
        window->Size = size;
      }
      window->Pos = Rounded(ClampWindowPosToScreen(window->Pos, window->Size));
      continue;
    }

    const std::string key = KeyOf(*window);
    const auto posed = posed_.find(window->ID);
    auto wanted = places_.find(key);
    bool moved_elsewhere = false;
    const bool known = posed != posed_.end() && wanted != places_.end();
    if (known && resize.held) {
      // LE JOUEUR TIRE UNE POIGNÉE : la place suit sa main, bornée par la
      // taille et sans déplacer le bord qu'il ne tire pas.
      const Shown held = UnderTheHand(*window, resize, screen, capped);
      WindowPlace adopted = PlaceFromRect(ScreenRect{.x_px = held.at.x,
                                                     .y_px = held.at.y,
                                                     .width_px = held.size.x,
                                                     .height_px = held.size.y},
                                          screen.x, screen.y, scale);
      adopted.collapsed = window->Collapsed;
      if (wanted->second != adopted) {
        wanted->second = adopted;
        ++revision_;
      }
    } else if (known && Differs(posed->second, *window) &&
               OnlyResized(*window, posed->second.x_px, posed->second.y_px,
                           posed->second.collapsed)) {
      // 🔴 REDIMENSIONNÉE SANS GESTE DU JOUEUR : l'ancre et l'écart restent,
      // seule la taille change. Voir l'en-tête.
      const ImVec2 size = capped ? Bounded(window->SizeFull, screen) : window->SizeFull;
      WindowPlace resized = wanted->second;
      resized.width = size.x / scale;
      resized.height = size.y / scale;
      if (wanted->second != resized) {
        wanted->second = resized;
        ++revision_;
      }
    } else if (posed != posed_.end()) {
      moved_elsewhere = Differs(posed->second, *window);
    } else if (wanted == places_.end()) {
      // ⚠ UNE FENÊTRE QUE LA DISPOSITION POSÉE NE NOMME PAS attend d'être
      // redessinée : sa consigne de première ouverture la replace alors. L'adopter
      // avant retiendrait la place du profil d'avant — pour toujours, si elle
      // reste fermée.
      if (!window->WasActive) {
        continue;
      }
      moved_elsewhere = true;
    } else if (const auto prepared = prepared_.find(window->ID); prepared != prepared_.end()) {
      // 🔴 NÉE À UNE AUTRE PLACE QUE CELLE PRÉPARÉE : son appelant l'a posée à
      // l'apparition — sous le curseur, au centre —, et cette place-là gagne.
      moved_elsewhere = Differs(prepared->second, *window);
      prepared_.erase(prepared);
    }

    if (moved_elsewhere) {
      // LA PLACE ADOPTÉE EST CELLE QUI TIENT DANS L'ÉCRAN : une fenêtre tirée
      // au-delà du bord droit retient le bord, pas l'au-delà.
      const ImVec2 size = capped ? Bounded(window->SizeFull, screen) : window->SizeFull;
      const ImVec2 at = ClampWindowPosToScreen(
          window->Pos, Occupied(size, window->Collapsed, window->TitleBarHeight));
      WindowPlace adopted = PlaceFromRect(
          ScreenRect{.x_px = at.x, .y_px = at.y, .width_px = size.x, .height_px = size.y}, screen.x,
          screen.y, scale);
      adopted.collapsed = window->Collapsed;
      if (wanted == places_.end() || wanted->second != adopted) {
        wanted = places_.insert_or_assign(key, adopted).first;
        ++revision_;
      }
    }

    const Shown shown =
        ShownOf(wanted->second, screen, scale, size_follows ? nullptr : &window->SizeFull, capped,
                window->TitleBarHeight);
    // ⚠ ÉCRITURE DIRECTE, et surtout pas `ImGui::SetWindowPos(window, ...)` :
    // cette surcharge interne efface au passage `SetWindowPosVal` et les bits
    // `Once`/`FirstUseEver`/`Appearing`.
    window->Pos = shown.at;
    window->Collapsed = wanted->second.collapsed;
    if (size_follows) {
      window->SizeFull = shown.size;
      if (!window->Collapsed) {
        window->Size = shown.size;
      }
    }
    posed_.insert_or_assign(window->ID, Posed{.x_px = shown.at.x,
                                              .y_px = shown.at.y,
                                              .width_px = window->SizeFull.x,
                                              .height_px = window->SizeFull.y,
                                              .collapsed = window->Collapsed});
  }

  if (unborn_stale_) {
    PrepareUnborn(scale);
    prepared_width_px_ = screen.x;
    prepared_height_px_ = screen.y;
    prepared_scale_ = scale;
    unborn_stale_ = false;
  }
}

void WindowPlacer::PrepareUnborn(float interface_scale) {
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  // La barre de titre d'une fenêtre qui n'existe pas encore : celle qu'ImGui
  // lui donnera, une ligne de cadre.
  const float title_bar_height_px = ImGui::GetFrameHeight();
  prepared_.clear();
  for (const auto& [key, place] : places_) {
    const ImGuiID window_id = ImHashStr(key.c_str());
    if (ImGui::FindWindowByID(window_id) != nullptr) {
      continue;
    }
    ImGuiWindowSettings* settings = ImGui::FindWindowSettingsByID(window_id);
    if (settings == nullptr) {
      settings = ImGui::CreateNewWindowSettings(key.c_str());
    }
    const Shown shown = ShownOf(place, screen, interface_scale, nullptr, true, title_bar_height_px);
    settings->Pos = ImVec2ih(static_cast<short>(shown.at.x), static_cast<short>(shown.at.y));
    settings->Size = ImVec2ih(static_cast<short>(shown.size.x), static_cast<short>(shown.size.y));
    settings->Collapsed = place.collapsed;
    prepared_.insert_or_assign(window_id, Posed{.x_px = shown.at.x,
                                                .y_px = shown.at.y,
                                                .width_px = shown.size.x,
                                                .height_px = shown.size.y,
                                                .collapsed = place.collapsed});
  }
}

}  // namespace ro::ui
