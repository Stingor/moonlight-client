#include "character_select_customize.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include "ro/core/decimal_text.h"
#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// ── LES POIGNÉES ─────────────────────────────────────────────────────────────
constexpr float kAnchorGrabPixels = 18.0F;
constexpr float kAnchorDotRadius = 5.0F;
constexpr ImU32 kAnchorColor = IM_COL32(120, 200, 255, 230);
constexpr ImU32 kAnchorHeldColor = IM_COL32(255, 255, 150, 255);
constexpr ImU32 kAnchorLabelColor = IM_COL32(150, 210, 255, 255);
// Le nom s'écrit à droite de la pastille, calé sur son milieu.
constexpr ImVec2 kAnchorLabelOffset(8.0F, -6.0F);

// ── LE CADRE D'UN SIÈGE ──────────────────────────────────────────────────────
constexpr ImU32 kSeatFrameColor = IM_COL32(255, 210, 90, 220);
constexpr ImU32 kSeatNumberColor = IM_COL32(255, 220, 120, 255);
constexpr float kSeatFrameRounding = 3.0F;
constexpr float kSeatFrameStroke = 1.5F;
// Le numéro, au-dessus du cadre et à peu près centré sur lui.
constexpr ImVec2 kSeatNumberOffset(-4.0F, -14.0F);
// Ctrl+molette : un cran vaut cinq millièmes de hauteur d'écran, et un
// personnage ne descend pas sous cinq centièmes — plus bas, il n'est plus
// qu'un point qu'on ne rattrape pas.
constexpr float kSeatHeightWheelStep = 0.005F;
constexpr float kSeatHeightWheelFloor = 0.05F;

// ── LES REPÈRES ──────────────────────────────────────────────────────────────
constexpr int kThirds = 3;
constexpr ImU32 kGuideColor = IM_COL32(255, 255, 255, 26);
constexpr ImU32 kAxisColor = IM_COL32(255, 210, 90, 45);
constexpr ImU32 kHintPlateColor = IM_COL32(20, 30, 50, 225);
constexpr ImU32 kHintBorderColor = IM_COL32(120, 200, 255, 200);
constexpr ImU32 kHintTextColor = IM_COL32(215, 235, 255, 255);
constexpr float kHintTopPixels = 6.0F;
constexpr ImVec2 kHintPadding(12.0F, 4.0F);
constexpr float kHintRounding = 4.0F;
constexpr float kHintStroke = 1.5F;

// ── LE PANNEAU ───────────────────────────────────────────────────────────────
//
// ⚠ UNE TAILLE FIXE, redimensionnable à la main, et non un ajustement au
// contenu : le panneau est fait de textes enroulés et d'enfants à largeur
// libre, dont la mise en page dépend de la largeur — les laisser décider de
// cette même largeur fait osciller la fenêtre d'une image à l'autre.
constexpr ImVec2 kPanelSize(370.0F, 480.0F);
// Sa place à la première ouverture : à droite, sous le rappel des gestes.
constexpr float kPanelRightPixels = 400.0F;
constexpr float kPanelMarginPixels = 20.0F;
constexpr float kPanelTopPixels = 70.0F;
constexpr float kBackgroundListPoints = 132.0F;
constexpr float kPresetListPoints = 96.0F;
// Les largeurs des boutons, en points.
constexpr float kRescanWidth = 110.0F;
constexpr float kOpenFolderWidth = 160.0F;
constexpr float kResetPlacementWidth = 200.0F;
constexpr float kResetAllWidth = 150.0F;
constexpr float kSavePresetWidth = 130.0F;
constexpr float kPresetButtonWidth = 120.0F;
constexpr float kFinishWidth = 120.0F;

// 🔴 DES COULEURS EXPLICITES : le corps d'une fenêtre du jeu est CLAIR, et le
// gris « désactivé » d'ImGui y est illisible.
constexpr ImU32 kFailureColor = IM_COL32(180, 40, 40, 255);
constexpr ImU32 kMisplacedColor = IM_COL32(150, 90, 0, 255);
constexpr ImVec4 kQuietColor(0.36F, 0.38F, 0.42F, 1.0F);
constexpr ImVec4 kArmedColor(0.55F, 0.15F, 0.10F, 1.0F);
constexpr ImVec4 kUnsavedColor(0.45F, 0.30F, 0.05F, 1.0F);
constexpr ImVec4 kSavedColor(0.15F, 0.42F, 0.18F, 1.0F);

constexpr const char* kPanelTitle = core::TrText("Personnaliser l'écran");
constexpr const char* kPanelId = "table_customize";
constexpr const char* kPoseMenu = "##pose_siege";

// UN CHEMIN LISIBLE : les antislashs deviennent des barres pour l'affichage
// seul. La police de l'interface est coréenne, et y dessine U+005C comme le
// symbole won « ₩ » — l'octet est juste, mais « lobby\x.bmp » se lirait
// « lobby₩x.bmp ». Le chemin STOCKÉ garde ses antislashs.
[[nodiscard]] std::string DisplayPath(std::string_view path) {
  std::string shown(path);
  std::ranges::replace(shown, '\\', '/');
  return shown;
}

[[nodiscard]] const char* OriginText(BackgroundOrigin origin) {
  switch (origin) {
    case BackgroundOrigin::kDisk:
      return core::Tr("ton fichier");
    case BackgroundOrigin::kPacked:
      return core::Tr("livré par le serveur");
    case BackgroundOrigin::kFactory:
      break;
  }
  return core::Tr("livré avec le client");
}

void TextInColor(ImU32 color, const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextWrapped("%s", text);
  ImGui::PopStyleColor();
}

// LA SECTION DES DÉCORS.
void DrawBackgroundSection(CharacterSelectState& state, const CharacterSelectView& view,
                           const SkinArt& art, CharacterSelectAction& action) {
  SeatLayout& layout = state.layout;
  ImGui::TextUnformatted(core::Tr("Décor"));
  ImGui::Separator();
  // ⚠ UNE COPIE : choisir un décor réécrit `layout.background` au milieu du
  // parcours, et les lignes suivantes se compareraient au nouveau.
  const std::string current = layout.background;
  if (ImGui::BeginChild("##decors", ImVec2(0.0F, kBackgroundListPoints * Scale()),
                        ImGuiChildFlags_Borders)) {
    for (const LayoutBackground& background : view.backgrounds) {
      const bool chosen = background.path == current;
      const char* label = BackgroundLabel(background);
      ImGui::PushID(background.path.c_str());
      // ⚠ APPLIQUÉ TOUT DE SUITE : le joueur voit son décor derrière le
      // panneau sans rien valider.
      if (ImGui::Selectable(label, chosen) && !chosen) {
        layout.background = background.path;
        state.layout_dirty = true;
      }
      if (ImGui::IsItemHovered()) {
        SkinTooltipFormat("%s\n%s", DisplayPath(background.path).c_str(),
                          OriginText(background.origin));
      }
      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  if (!view.background_loaded) {
    TextInColor(kFailureColor, core::Tr("Ce décor n'a pas pu être chargé (fichier absent ou "
                                        "format refusé) — fond uni en attendant."));
  }
  if (SkinButton(core::Tr("Rafraîchir"), art, ImVec2(kRescanWidth * Scale(), 0.0F))) {
    action.verb = CharacterSelectVerb::kRescanBackgrounds;
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Ouvrir le dossier"), art, ImVec2(kOpenFolderWidth * Scale(), 0.0F))) {
    action.verb = CharacterSelectVerb::kOpenBackgroundFolder;
  }
  ImGui::TextWrapped("%s", core::Tr("Dépose tes images .bmp dans data/texture/lobby/ (24 ou 32 "
                                    "bits, nom sans accent), puis « Rafraîchir ». Le bouton "
                                    "ci-dessus ouvre le bon dossier et le crée au besoin."));
  if (view.packed_backgrounds > 0) {
    ImGui::TextWrapped(core::Tr("%d décor(s) sont livrés avec le jeu : ils apparaissent tout "
                                "seuls, rien à installer."),
                       view.packed_backgrounds);
  }
  if (view.misplaced_backgrounds > 0) {
    std::array<char, 512> misplaced = {};
    std::snprintf(misplaced.data(), misplaced.size(),
                  core::Tr("%d image(s) se trouvent dans %s : le client ne lit pas ce dossier, "
                           "déplace-les vers data/texture/lobby/."),
                  view.misplaced_backgrounds, DisplayPath(view.misplaced_folder).c_str());
    TextInColor(kMisplacedColor, misplaced.data());
  }
}

// LA SECTION DES PLACES LIBRES.
void DrawFreeSeatSection(CharacterSelectState& state, const SkinArt& art) {
  SeatLayout& layout = state.layout;
  ImGui::Spacing();
  ImGui::TextUnformatted(core::Tr("Places libres"));
  ImGui::Separator();
  bool hide = layout.hide_free_seats;
  if (SkinCheckBox(core::Tr("Masquer les places libres"), &hide, art)) {
    layout.hide_free_seats = hide;
    state.layout_dirty = true;
  }
  if (!hide) {
    float alpha = layout.free_seat_alpha;
    if (SkinSliderFloat(core::Tr("Opacité"), &alpha, 0.0F, 1.0F, art)) {
      layout.free_seat_alpha = alpha;
      state.layout_dirty = true;
    }
  }
  ImGui::TextWrapped("%s", core::Tr("Masquées, elles réapparaissent au survol : créer un "
                                    "personnage reste possible. Elles restent toujours "
                                    "visibles pendant la personnalisation."));
}

// LA SECTION DU PLACEMENT.
void DrawPlacementSection(CharacterSelectState& state, const SkinArt& art) {
  ImGui::Spacing();
  ImGui::TextUnformatted(core::Tr("Placement"));
  ImGui::Separator();
  ImGui::TextWrapped("%s",
                     core::Tr("Glisse un personnage pour déplacer sa place. Molette dessus : le "
                              "tourner ; Ctrl+molette : sa taille ; clic droit : sa pose (assis, "
                              "debout, touché, mort) et l'orientation de sa tête.\nLes pastilles "
                              "bleues déplacent le titre, la barre de boutons, la pagination et la "
                              "barre de sortie."));
  if (SkinButton(core::Tr("Replacer les personnages"), art,
                 ImVec2(kResetPlacementWidth * Scale(), 0.0F))) {
    ResetSeatPlacement(state.layout);
    state.layout_dirty = true;
  }
  ImGui::SameLine();
  // ⚠ RETOUR À LA MISE EN PAGE LIVRÉE, décor compris. Les enregistrements du
  // joueur ne sont pas touchés : c'est une remise à zéro de l'écran, pas un
  // effacement de son travail.
  if (SkinButton(core::Tr("Tout restaurer"), art, ImVec2(kResetAllWidth * Scale(), 0.0F))) {
    ResetSeatLayout(state.layout);
    state.preset_selected = -1;
    state.layout_dirty = true;
  }
}

// LA SECTION DES MISES EN PAGE ENREGISTRÉES.
void DrawPresetSection(CharacterSelectState& state, const CharacterSelectView& view,
                       const SkinArt& art, CharacterSelectAction& action) {
  ImGui::Spacing();
  ImGui::TextUnformatted(core::Tr("Mises en page enregistrées"));
  ImGui::Separator();
  const auto count = static_cast<int>(view.presets.size());
  if (ImGui::BeginChild("##mises_en_page", ImVec2(0.0F, kPresetListPoints * Scale()),
                        ImGuiChildFlags_Borders)) {
    if (count == 0) {
      ImGui::TextColored(kQuietColor, "%s", core::Tr("Aucune pour l'instant."));
    }
    for (int rank = 0; rank < count; ++rank) {
      const std::string& name = view.presets[static_cast<std::size_t>(rank)];
      ImGui::PushID(rank);
      if (ImGui::Selectable(name.c_str(), state.preset_selected == rank)) {
        state.preset_selected = rank;
        // ⚠ LE CHAMP SUIT LA SÉLECTION : « Enregistrer » écrase alors CE nom,
        // ce que le joueur attend après avoir cliqué dessus.
        std::snprintf(state.preset_name.data(), state.preset_name.size(), "%s", name.c_str());
        state.preset_delete_armed = false;
      }
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        action.verb = CharacterSelectVerb::kApplyPreset;
        action.preset = rank;
      }
      ImGui::PopID();
    }
  }
  ImGui::EndChild();

  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputTextWithHint("##nom_mise_en_page", core::Tr("Nom (ex. « Ma taverne »)"),
                           state.preset_name.data(), state.preset_name.size());
  const bool has_name = state.preset_name[0] != '\0';
  ImGui::BeginDisabled(!has_name);
  if (SkinButton(core::Tr("Enregistrer"), art, ImVec2(kSavePresetWidth * Scale(), 0.0F))) {
    action.verb = CharacterSelectVerb::kSavePreset;
    state.preset_delete_armed = false;
  }
  ImGui::EndDisabled();

  const bool has_choice = state.preset_selected >= 0 && state.preset_selected < count;
  ImGui::SameLine();
  ImGui::BeginDisabled(!has_choice);
  if (SkinButton(core::Tr("Appliquer"), art, ImVec2(kPresetButtonWidth * Scale(), 0.0F))) {
    action.verb = CharacterSelectVerb::kApplyPreset;
    action.preset = state.preset_selected;
    state.preset_delete_armed = false;
  }
  ImGui::SameLine();
  // 🔴 EN DEUX TEMPS plutôt qu'une boîte : un enregistrement effacé par erreur
  // est irrécupérable, mais une fenêtre de plus sur cet écran, déjà peuplé de
  // création et de suppression, coûterait plus qu'elle ne protège.
  const char* remove_label =
      state.preset_delete_armed ? core::Tr("Confirmer") : core::Tr("Supprimer");
  if (SkinButton(remove_label, art, ImVec2(kPresetButtonWidth * Scale(), 0.0F))) {
    if (state.preset_delete_armed) {
      action.verb = CharacterSelectVerb::kDeletePreset;
      action.preset = state.preset_selected;
      state.preset_selected = -1;
      state.preset_delete_armed = false;
    } else {
      state.preset_delete_armed = true;
    }
  }
  ImGui::EndDisabled();

  if (state.preset_delete_armed && has_choice) {
    ImGui::TextColored(kArmedColor, core::Tr("Clique « Confirmer » pour supprimer « %s »."),
                       view.presets[static_cast<std::size_t>(state.preset_selected)].c_str());
  } else {
    ImGui::TextWrapped("%s", core::Tr("Double-clic sur un nom = l'appliquer. « Enregistrer » "
                                      "range l'écran tel qu'il est sous ce nom."));
  }
}

}  // namespace

CharacterSelectVerb ToggleCustomizing(CharacterSelectState& state) {
  state.customizing = !state.customizing;
  if (!state.customizing) {
    return CharacterSelectVerb::kEndCustomizing;
  }
  state.preset_selected = -1;
  state.preset_delete_armed = false;
  return CharacterSelectVerb::kBeginCustomizing;
}

ImVec2 SceneAnchor(CharacterSelectState& state, std::string_view name, float default_x,
                   float default_y, ImVec2 display) {
  SeatAnchor* anchor = AnchorOf(state.layout, name, default_x, default_y);
  if (anchor == nullptr) {
    return ImVec2(default_x * display.x, default_y * display.y);
  }
  ImVec2 point(anchor->x * display.x, anchor->y * display.y);
  if (!state.customizing) {
    return point;
  }
  const float half = kAnchorGrabPixels * 0.5F;
  ImGui::PushID(name.data(), name.data() + name.size());
  ImGui::SetCursorScreenPos(ImVec2(point.x - half, point.y - half));
  ImGui::InvisibleButton("##ancre", ImVec2(kAnchorGrabPixels, kAnchorGrabPixels));
  const bool held = ImGui::IsItemActive();
  if (held && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    anchor->x += ImGui::GetIO().MouseDelta.x / display.x;
    anchor->y += ImGui::GetIO().MouseDelta.y / display.y;
    point = ImVec2(anchor->x * display.x, anchor->y * display.y);
    state.layout_dirty = true;
  }
  ImDrawList* list = ImGui::GetWindowDrawList();
  list->AddCircleFilled(point, kAnchorDotRadius, held ? kAnchorHeldColor : kAnchorColor);
  const std::string label(name);
  list->AddText(ImVec2(point.x + kAnchorLabelOffset.x, point.y + kAnchorLabelOffset.y),
                kAnchorLabelColor, label.c_str());
  ImGui::PopID();
  return point;
}

void EditSeat(CharacterSelectState& state, int seat, const SeatFrame& frame, ImDrawList* list,
              ImVec2 display, bool hovered) {
  CharacterSeat& place = state.layout.seats[static_cast<std::size_t>(seat)];
  const ImGuiIO& io = ImGui::GetIO();
  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    place.x += io.MouseDelta.x / display.x;
    place.y += io.MouseDelta.y / display.y;
    state.layout_dirty = true;
  }
  // ⚠ PAR LE PORTAIL DE LA MOLETTE : rien n'est défilable ici, donc elle reste
  // franche — seul un curseur qui vient d'arriver sur le siège attend d'être
  // posé avant de faire tourner le convive.
  const float wheel = WheelOver(state.wheel, ImGui::GetItemID(), hovered);
  if (wheel != 0.0F) {
    if (io.KeyCtrl) {
      // ⚠ BORNÉE AUSSI PAR LE HAUT, à ce que la lecture accepte : au-delà, le
      // fichier rendrait le siège entier à l'usine au lancement suivant.
      place.height = std::clamp(place.height + wheel * kSeatHeightWheelStep, kSeatHeightWheelFloor,
                                kSeatHeightHighest);
    } else {
      // Huit orientations en boucle, un cran par huitième de tour.
      const int step = wheel > 0.0F ? 1 : -1;
      place.direction = (place.direction + step + kSeatDirections) % kSeatDirections;
    }
    state.layout_dirty = true;
  }
  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    state.pose_seat = seat;
    state.pose_menu_asked = true;
  }
  list->AddRect(ImVec2(frame.grab_left, frame.grab_top),
                ImVec2(frame.grab_right, frame.grab_bottom), kSeatFrameColor, kSeatFrameRounding, 0,
                kSeatFrameStroke);
  std::array<char, ro::core::kDecimalTextBytes<int>> number = {};
  std::snprintf(number.data(), number.size(), "%d", seat + 1);
  list->AddText(ImVec2(frame.centre_x + kSeatNumberOffset.x, frame.grab_top + kSeatNumberOffset.y),
                kSeatNumberColor, number.data());
}

void DrawSeatPoseMenu(CharacterSelectState& state, const CharacterSelectView& view,
                      ImVec2 padding) {
  if (state.pose_menu_asked) {
    ImGui::OpenPopup(kPoseMenu);
    state.pose_menu_asked = false;
  }
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
  const bool open = BeginSkinPopup(kPoseMenu);
  ImGui::PopStyleVar();
  if (!open) {
    return;
  }
  if (state.pose_seat >= 0 && state.pose_seat < kSeatCount) {
    CharacterSeat& place = state.layout.seats[static_cast<std::size_t>(state.pose_seat)];
    ImGui::Text(core::Tr("Place %d"), state.pose_seat + 1);
    ImGui::Separator();
    for (const SeatPoseChoice& choice : kSeatPoseChoices) {
      const bool on = place.animation == choice.animation && place.animated == choice.animated;
      if (ImGui::MenuItem(PoseLabel(choice), nullptr, on)) {
        place.animation = choice.animation;
        place.animated = choice.animated;
        // ⚠ L'IMAGE CHOISIE APPARTENAIT À L'ACTION PRÉCÉDENTE, et la nouvelle
        // n'en a pas forcément autant : on repart d'« Automatique ».
        place.frame = kAutomaticFrame;
        state.layout_dirty = true;
      }
    }
    // ⚠ SEULEMENT LES IMAGES QUI EXISTENT : leur nombre est relevé au rendu de
    // cette place, et dépend de la pose ET de la classe.
    const int frames = view.seat_frames[static_cast<std::size_t>(state.pose_seat)];
    if (frames > 1 && ImGui::BeginMenu(core::Tr("Image"))) {
      if (ImGui::MenuItem(core::Tr("Automatique"), nullptr, place.frame < 0)) {
        place.frame = kAutomaticFrame;
        state.layout_dirty = true;
      }
      ImGui::Separator();
      for (int image = 0; image < frames; ++image) {
        std::array<char, 32> label = {};
        std::snprintf(label.data(), label.size(), core::Tr("Image %d / %d"), image + 1, frames);
        if (ImGui::MenuItem(label.data(), nullptr, place.frame == image)) {
          place.frame = image;
          state.layout_dirty = true;
        }
      }
      ImGui::EndMenu();
    }

    ImGui::Separator();
    if (ImGui::BeginMenu(core::Tr("Tête (doridori)"))) {
      for (const HeadTurnChoice& head : kHeadTurnChoices) {
        // ⚠ « DANS L'AXE » S'ÉCRIT DE DEUX FAÇONS, -1 et 0 : la coche suit ce
        // sens et non la valeur.
        const bool on =
            head.head_turn == 0 ? place.head_turn <= 0 : place.head_turn == head.head_turn;
        if (ImGui::MenuItem(HeadTurnLabel(head), nullptr, on)) {
          place.head_turn = head.head_turn;
          state.layout_dirty = true;
        }
      }
      ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::TextColored(kQuietColor, "%s", core::Tr("Molette : tourner — Ctrl+molette : taille"));
    if (ImGui::MenuItem(core::Tr("Appliquer cette pose à toutes les places"))) {
      ApplyPoseEverywhere(state.layout, place);
      state.layout_dirty = true;
    }
  }
  EndSkinPopup();
}

void DrawLayoutGuides(ImDrawList* list, ImVec2 origin, ImVec2 display) {
  for (int cut = 1; cut < kThirds; ++cut) {
    const float x = origin.x + display.x * static_cast<float>(cut) / kThirds;
    const float y = origin.y + display.y * static_cast<float>(cut) / kThirds;
    list->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + display.y), kGuideColor);
    list->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + display.x, y), kGuideColor);
  }
  const float middle = origin.x + display.x * 0.5F;
  list->AddLine(ImVec2(middle, origin.y), ImVec2(middle, origin.y + display.y), kAxisColor);

  // ⚠ LE RAPPEL DES GESTES EN HAUT, et non dans le panneau : celui-ci se
  // déplace n'importe où, et le joueur ne doit pas avoir à le retrouver pour se
  // souvenir de la molette.
  const char* hint = core::Tr(
      "Personnalisation — glisser : placer  •  molette : tourner  •  Ctrl+molette : "
      "taille  •  clic droit : pose  •  pastilles bleues : blocs d'interface");
  const ImVec2 size = ImGui::CalcTextSize(hint);
  const ImVec2 at(origin.x + (display.x - size.x) * 0.5F, origin.y + kHintTopPixels);
  const ImVec2 plate_min(at.x - kHintPadding.x, at.y - kHintPadding.y);
  const ImVec2 plate_max(at.x + size.x + kHintPadding.x, at.y + size.y + kHintPadding.y);
  list->AddRectFilled(plate_min, plate_max, kHintPlateColor, kHintRounding);
  list->AddRect(plate_min, plate_max, kHintBorderColor, kHintRounding, 0, kHintStroke);
  list->AddText(at, kHintTextColor, hint);
}

CharacterSelectAction DrawLayoutPanel(CharacterSelectState& state, const CharacterSelectView& view,
                                      const SkinArt& art) {
  CharacterSelectAction action;
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowSize(ImVec2(kPanelSize.x * Scale(), kPanelSize.y * Scale()),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(
      ImVec2(viewport->Pos.x +
                 std::max(kPanelMarginPixels, viewport->Size.x - kPanelRightPixels * Scale()),
             viewport->Pos.y + kPanelTopPixels),
      ImGuiCond_FirstUseEver);

  bool open = true;
  static WindowName name(kPanelTitle, kPanelId);
  if (BeginSkinnedWindow(name.Get(), &open, art)) {
    DrawBackgroundSection(state, view, art, action);
    DrawFreeSeatSection(state, art);
    DrawPlacementSection(state, art);
    DrawPresetSection(state, view, art, action);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (SkinButton(core::Tr("Terminer"), art, ImVec2(kFinishWidth * Scale(), 0.0F))) {
      open = false;
    }
    ImGui::SameLine();
    if (state.layout_dirty) {
      ImGui::TextColored(kUnsavedColor, "%s", core::Tr("Sera enregistré en quittant"));
    } else {
      ImGui::TextColored(kSavedColor, "%s", core::Tr("Enregistré"));
    }
  }
  EndSkinnedWindow();
  // « Terminer » ou la croix : la sortie du mode, donc l'écriture.
  if (!open) {
    action.verb = ToggleCustomizing(state);
  }
  return action;
}

}  // namespace ro::ui
