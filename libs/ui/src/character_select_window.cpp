#include "ro/ui/character_select_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "character_select_amend.h"
#include "character_select_bars.h"
#include "character_select_customize.h"

namespace ro::ui {
namespace {

// LES COULEURS DE LA SCÈNE LUI SONT PROPRES, et non tirées de
// `skin_palette.h`.
//
// ⚠ ET C'EST LA RÈGLE, PAS UNE ENTORSE : cette palette-là habille le CORPS
// d'une fenêtre du jeu, qui est presque blanc. Ici il n'y a pas de corps — on
// écrit sur une salle peinte, sombre, et un gris-bleu très sombre y serait
// illisible. Ces valeurs ont été calées sur ce décor-là.
constexpr ImU32 kTitleColor = IM_COL32(245, 236, 210, 255);
constexpr ImU32 kTitleReliefColor = IM_COL32(0, 0, 0, 160);
constexpr ImU32 kNameColor = IM_COL32(235, 235, 235, 255);
constexpr ImU32 kChosenNameColor = IM_COL32(255, 236, 190, 255);
constexpr ImU32 kLevelColor = IM_COL32(200, 205, 220, 220);
constexpr ImU32 kPlateColor = IM_COL32(0, 0, 0, 150);
constexpr ImU32 kChosenHaloColor = IM_COL32(255, 210, 110, 70);
constexpr ImU32 kHoverHaloColor = IM_COL32(200, 210, 235, 40);
// L'arrivée choisie d'un déplacement : un bleu, pour ne pas la confondre avec le
// convive qu'on déplace, qui garde son halo doré.
constexpr ImU32 kMoveTargetHaloColor = IM_COL32(120, 200, 255, 110);
constexpr ImU32 kRefusalColor = IM_COL32(255, 185, 185, 255);
constexpr ImU32 kRefusalPlateColor = IM_COL32(70, 20, 20, 235);
constexpr ImU32 kWaitingDollColor = IM_COL32(40, 44, 58, 120);
constexpr ImU32 kFreeSeatColor = IM_COL32(210, 200, 180, 150);
constexpr ImU32 kFreeSeatHoverColor = IM_COL32(255, 230, 150, 230);
constexpr ImU32 kDoomedColor = IM_COL32(255, 150, 150, 255);
constexpr ImU32 kDoomedPlateColor = IM_COL32(40, 0, 0, 205);
// Le texte du voile de l'entrée en jeu.
constexpr ImU32 kEnterFadeTextColor = IM_COL32(235, 230, 220, 255);
constexpr float kOpaqueAlpha = 255.0F;

// LE PANTIN D'UN CONDAMNÉ BAT EN ROUGE. Les canaux vert et bleu descendent puis
// remontent en cosinus, sur une seconde : c'est
// le seul signe qui se voie depuis l'autre bout de la table.
constexpr double kDoomedPulseSeconds = 1.0;
constexpr int kDoomedPulseFloor = 70;
constexpr int kDoomedPulseRange = 150;

// LES PLACES D'USINE DES BLOCS, en pixels depuis le bord.
// ⚠ CE NE SONT QUE DES DÉFAUTS : chaque bloc passe par une ancre que le mode
// « Personnaliser » déplace, et c'est elle qui fait foi ensuite.
constexpr float kTitleTopPixels = 24.0F;
constexpr float kActionBarBottomPixels = 52.0F;
constexpr float kExitBarRightPixels = 24.0F;
constexpr float kPageBarBottomPixels = 96.0F;

// Le halo au sol : une fraction de la largeur du cadre cliquable.
constexpr float kChosenHaloWidthRatio = 0.55F;
constexpr float kHoverHaloWidthRatio = 0.50F;
constexpr int kHaloSegments = 28;

// L'étiquette du nom : son fond déborde de ceci autour du texte, et elle est
// remontée d'autant vers le sprite.
constexpr float kPlatePaddingPixels = 6.0F;
constexpr float kPlateLiftPixels = 3.0F;
constexpr float kPlateRounding = 3.0F;

// LE MARQUEUR D'UNE PLACE LIBRE : un cercle barré d'une croix, posé à
// mi-hauteur du cadre du siège. Son rayon est une fraction de la largeur de ce
// cadre, donc il suit la perspective comme les convives.
constexpr float kFreeSeatRadiusRatio = 0.30F;
constexpr float kFreeSeatArmRatio = 0.50F;
constexpr int kFreeSeatSegments = 24;
constexpr float kFreeSeatStroke = 2.0F;
// L'opacité d'une place libre AU REPOS, avant le réglage du joueur.
constexpr float kFreeSeatIdleAlpha = 150.0F;

// UNE PLACE LIBRE : le « + » qui invite à créer.
//
// ⚠ L'OPACITÉ DU JOUEUR NE VAUT QU'AU REPOS : au survol, le marqueur reprend
// toute sa présence, sans quoi une place très atténuée deviendrait impossible
// à viser avec assurance.
void DrawFreeSeat(ImDrawList* list, const SeatFrame& seat, bool hovered, float idle_alpha) {
  const float grab_width = seat.grab_right - seat.grab_left;
  const float radius = grab_width * kFreeSeatRadiusRatio;
  const float middle_y = seat.chair_y - seat.standard_height * 0.5F;
  const ImVec2 middle(seat.centre_x, middle_y);
  const auto idle = static_cast<int>(kFreeSeatIdleAlpha * std::clamp(idle_alpha, 0.0F, 1.0F));
  const ImU32 color =
      hovered
          ? kFreeSeatHoverColor
          : ((kFreeSeatColor & ~IM_COL32_A_MASK) | (static_cast<ImU32>(idle) << IM_COL32_A_SHIFT));
  const float arm = radius * kFreeSeatArmRatio;
  list->AddCircle(middle, radius, color, kFreeSeatSegments, kFreeSeatStroke);
  list->AddLine(ImVec2(middle.x - arm, middle.y), ImVec2(middle.x + arm, middle.y), color,
                kFreeSeatStroke);
  list->AddLine(ImVec2(middle.x, middle.y - arm), ImVec2(middle.x, middle.y + arm), color,
                kFreeSeatStroke);
}

// UN TEXTE EN RELIEF : son ombre d'un pixel, puis lui. Sans elle, une lettre
// claire posée sur une zone claire du décor disparaît — et le décor n'est pas
// le nôtre, il change avec la carte.
void AddTextRelief(ImDrawList* list, ImVec2 at, ImU32 color, const char* text) {
  list->AddText(ImVec2(at.x + 1.0F, at.y + 1.0F), kTitleReliefColor, text);
  list->AddText(at, color, text);
}

// Un bandeau centré sous le titre : le seul endroit où la scène PARLE.
void DrawBanner(ImDrawList* list, float centre_x, float top, const char* text, ImU32 text_color,
                ImU32 plate_color) {
  const ImVec2 size = ImGui::CalcTextSize(text);
  const float left = centre_x - size.x * 0.5F;
  list->AddRectFilled(ImVec2(left - kPlatePaddingPixels * 2.0F, top - 4.0F),
                      ImVec2(left + size.x + kPlatePaddingPixels * 2.0F, top + size.y + 4.0F),
                      plate_color, 4.0F);
  AddTextRelief(list, ImVec2(left, top), text_color, text);
}

// L'ÉTIQUETTE D'UN CONVIVE : son nom, puis ses deux niveaux en retrait.
//
// ⚠ SUR UNE SEULE LIGNE, sous les pieds : c'est ce qui la garde collée au
// personnage quel que soit ce qu'il porte. Deux lignes la feraient descendre
// sur le convive du rang suivant.
void DrawNamePlate(ImDrawList* list, const SeatFrame& seat, const CharacterSlotView& who,
                   bool chosen) {
  std::array<char, 32> levels = {};
  std::snprintf(levels.data(), levels.size(), "  %d/%d", who.base_level, who.job_level);
  const ImVec2 name_size = ImGui::CalcTextSize(who.name.c_str());
  const ImVec2 level_size = ImGui::CalcTextSize(levels.data());
  const float total = name_size.x + level_size.x;
  const float left = seat.centre_x - total * 0.5F;
  const float top = seat.chair_y - kPlateLiftPixels;

  list->AddRectFilled(ImVec2(left - kPlatePaddingPixels, top - 1.0F),
                      ImVec2(left + total + kPlatePaddingPixels, top + name_size.y + 1.0F),
                      kPlateColor, kPlateRounding);
  list->AddText(ImVec2(left, top), chosen ? kChosenNameColor : kNameColor, who.name.c_str());
  list->AddText(ImVec2(left + name_size.x, top), kLevelColor, levels.data());

  // ⚠ LES RETOUCHES SE TAISENT SOUS UN CONDAMNÉ : la barre ne les propose plus,
  // et deux étiquettes empilées descendraient sur le convive du rang suivant.
  if (!who.doomed()) {
    DrawAmendTag(list, seat, who, top + name_size.y + 3.0F);
    return;
  }
  // ⚠ SOUS LE NOM, avec le temps qu'il reste : « suppression programmée » sans
  // échéance laisse croire qu'elle est imminente, alors qu'elle attend une
  // heure. Et c'est ce compte à rebours qui dit quand le bouton s'allumera.
  std::array<char, 64> doom = {};
  if (who.delete_in_seconds > 0) {
    const int hours = who.delete_in_seconds / 3600;
    const int minutes = (who.delete_in_seconds % 3600) / 60;
    const int seconds = who.delete_in_seconds % 60;
    std::snprintf(doom.data(), doom.size(), core::Tr("Suppression dans %02d:%02d:%02d"), hours,
                  minutes, seconds);
  } else {
    std::snprintf(doom.data(), doom.size(), "%s", core::Tr("Suppression possible"));
  }
  const ImVec2 doom_size = ImGui::CalcTextSize(doom.data());
  const float doom_left = seat.centre_x - doom_size.x * 0.5F;
  const float doom_top = top + name_size.y + 3.0F;
  list->AddRectFilled(
      ImVec2(doom_left - kPlatePaddingPixels, doom_top + 2.0F),
      ImVec2(doom_left + doom_size.x + kPlatePaddingPixels, doom_top + doom_size.y + 1.0F),
      kDoomedPlateColor, kPlateRounding);
  list->AddText(ImVec2(doom_left, doom_top), kDoomedColor, doom.data());
}

// LE PANTIN, ou son attente.
//
// 🔴 SON BAS EST LA LIGNE DU SIÈGE ET SON HAUT DÉBORDE : la boîte est plus
// haute qu'un personnage standard, parce que toute la tablée est à UNE échelle
// et que ce qui est plus grand doit sortir du cadre plutôt que rétrécir. Voir
// `kSeatHeadroom`.
void DrawDoll(ImDrawList* list, const SeatFrame& seat, const CharacterSlotView& who) {
  const float left = seat.centre_x - seat.doll_width * 0.5F;
  const float top = seat.chair_y - seat.doll_height;
  if (who.doll.usable()) {
    // ⚠ LA TEINTE SE POSE AU DESSIN, et non sur la vignette : celle-ci est
    // gardée en cache et ne se recompose que quand l'image change — la
    // repeindre à chaque battement la referait soixante fois par seconde.
    ImU32 tint = IM_COL32_WHITE;
    if (who.doomed()) {
      const auto phase = static_cast<float>(std::fmod(ImGui::GetTime(), kDoomedPulseSeconds) /
                                            kDoomedPulseSeconds);
      const float pulse = 0.5F - 0.5F * std::cos(phase * 2.0F * std::numbers::pi_v<float>);
      const auto channel = static_cast<int>(kDoomedPulseFloor + (1.0F - pulse) * kDoomedPulseRange);
      tint = IM_COL32(255, channel, channel, 255);
    }
    list->AddImage(who.doll.id, ImVec2(left, top), ImVec2(left + seat.doll_width, seat.chair_y),
                   ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F), tint);
    return;
  }
  // ⚠ MÊME ENCOMBREMENT QUE LE PANTIN, et c'est tout l'objet de cette
  // silhouette : les fichiers arrivent en quelques images, et une place vide
  // qui se remplirait ferait sauter la scène sous les yeux du joueur.
  list->AddRectFilled(ImVec2(seat.grab_left, seat.grab_top), ImVec2(seat.grab_right, seat.chair_y),
                      kWaitingDollColor, 4.0F);
}

// LE VOILE NOIR DE L'ENTRÉE EN JEU, et son texte, qui montent ensemble.
//
// ⚠ SUR LA LISTE DE PREMIER PLAN : il couvre tout ce que l'image porte, y
// compris ce qui a été dessiné après la scène.
void DrawEnterFade(float entering_seconds) {
  const float opacity = std::clamp(entering_seconds / kEnterFadeSeconds, 0.0F, 1.0F);
  const auto alpha = static_cast<int>(opacity * kOpaqueAlpha);
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImDrawList* list = ImGui::GetForegroundDrawList();
  list->AddRectFilled(
      viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
      IM_COL32(0, 0, 0, alpha));
  const char* label = core::Tr("Entrée en jeu…");
  const ImVec2 size = ImGui::CalcTextSize(label);
  const ImVec2 centre = viewport->GetCenter();
  list->AddText(
      ImVec2(centre.x - size.x * 0.5F, centre.y - size.y * 0.5F),
      (kEnterFadeTextColor & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT),
      label);
}

}  // namespace

CharacterSelectAction DrawCharacterSelect(CharacterSelectState& state,
                                          const CharacterSelectView& view, const SkinArt& art) {
  CharacterSelectAction action;

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 display = viewport->Size;
  if (display.x <= 0.0F || display.y <= 0.0F) {
    return action;
  }

  // PLEIN CADRE, SANS FOND NI CHROME. ⚠ `NoBringToFrontOnFocus` : sans lui,
  // cliquer un siège remonterait la scène par-dessus tout ce qu'on pose
  // dessus — une confirmation, un formulaire de création, le panneau de
  // personnalisation.
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(display);
  // 🔴 L'HABILLAGE SE POSE ICI, ET PAS SEULEMENT DANS LES FENÊTRES. `ImGuiCol_Text`
  // vaut BLANC chez ImGui ; le jeu, lui, écrit en sombre (`kSkinTextColor`) parce
  // que ses boutons sont clairs. `BeginSkinnedWindow` pose cette couleur, mais
  // cette scène-ci ouvre une fenêtre NUE : sans cette poussée, ses boutons
  // écriraient en blanc sur du clair, c'est-à-dire pas du tout — la barre
  // d'action et les flèches de page seraient des rectangles vides.
  //
  // ⚠ AVANT LE FOND TRANSPARENT, et l'ordre fait tout : `PushSkinStyle` pose le
  // corps opaque d'une fenêtre du jeu, et c'est la poussée SUIVANTE qui doit
  // l'emporter — sinon la scène se peint un fond et cache la ville.
  const SkinStylePushed skin = PushSkinStyle(art);
  // ⚠ LE RETRAIT DE L'HABILLAGE SE RETIENT AVANT D'ÊTRE ANNULÉ : le menu de
  // pose s'ouvre dans cette scène, et hériterait sinon de ses bords à zéro.
  const ImVec2 skin_padding = ImGui::GetStyle().WindowPadding;
  ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  const bool open =
      ImGui::Begin("###character_select", nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
  if (!open) {
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    PopSkinStyle(skin);
    return action;
  }

  ImDrawList* list = ImGui::GetWindowDrawList();
  const bool editing = state.customizing;

  // LA SALLE, ÉTIRÉE PLEIN CADRE. C'est ce qui rend les sièges portables tels
  // quels : une table calée en 1920 × 1080 vaut en 1280 × 720.
  if (view.hall.usable()) {
    list->AddImage(view.hall.id, viewport->Pos,
                   ImVec2(viewport->Pos.x + display.x, viewport->Pos.y + display.y));
  }

  const auto slot_count = static_cast<int>(view.slots.size());
  const int pages = SeatPages(slot_count);
  state.page = std::clamp(state.page, 0, pages - 1);
  // ⚠ LES EMPLACEMENTS NE SONT PAS LES PERSONNAGES : la vue porte AUSSI les
  // places libres, parce qu'elles sont des sièges de la tablée. Compter les
  // cases pour dire « aucun personnage » rendrait ce message impossible.
  const auto peopled =
      static_cast<int>(std::count_if(view.slots.begin(), view.slots.end(),
                                     [](const CharacterSlotView& who) { return who.occupied; }));

  // 🔴 LE CHOIX EST PARTI : LA SCÈNE RESTE, MAIS PLUS RIEN NE RÉPOND. Un siège
  // qui accepterait un second clic pendant que la carte se monte enverrait un
  // second choix sur une liaison qui n'écoute plus. Et l'effacer laisserait un
  // écran noir de plusieurs secondes.
  ImGui::BeginDisabled(view.busy);

  // ── Les sièges, du fond vers l'avant ──────────────────────────────────────
  //
  // ⚠ L'ORDRE DE LA TABLE EST CELUI DE LA PROFONDEUR : le premier plan se
  // dessine en dernier, donc il passe par-dessus — et son bouton invisible,
  // soumis en dernier lui aussi, gagne les clics là où deux cadres se
  // recouvrent. C'est le bon arbitrage : on vise ce qu'on voit.
  const CharacterSlotView nobody;
  // Les convives de cette page, pour les flèches.
  std::vector<SeatPoint> peopled_seats;
  for (int seat = 0; seat < kSeatCount; ++seat) {
    const int slot = SeatedSlot(seat, state.page);
    const bool has_slot = slot < slot_count;
    // ⚠ PENDANT LA PERSONNALISATION, LES VINGT-CINQ SIÈGES SE MONTRENT, même
    // ceux qu'aucun emplacement ne remplit : sans cela, un compte de trois
    // personnages ne pourrait placer que trois sièges.
    if (!has_slot && !editing) {
      continue;
    }
    const CharacterSlotView& who = has_slot ? view.slots[static_cast<std::size_t>(slot)] : nobody;
    // ⚠ UNE PLACE LIBRE NE SE MONTRE QUE SI ELLE S'OUVRE. Au-delà de ce que le
    // compte peut remplir, un « + » inviterait à un geste que le serveur
    // refuse — et son refus, « cet emplacement n'est pas ouvert », arriverait
    // après coup.
    const bool creatable = !who.occupied && slot < view.creatable_slots;
    if (!who.occupied && !creatable && !editing) {
      continue;
    }
    const SeatFrame seating =
        SeatFrameAt(state.layout.seats[static_cast<std::size_t>(seat)], display.x, display.y);

    ImGui::PushID(seat);
    ImGui::SetCursorScreenPos(ImVec2(seating.grab_left, seating.grab_top));
    ImGui::InvisibleButton("siege", ImVec2(seating.grab_right - seating.grab_left,
                                           seating.grab_bottom - seating.grab_top));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    if (editing) {
      // ⚠ PENDANT LA PERSONNALISATION, LE CLIC PLACE ET NE CHOISIT PAS : ni
      // sélection, ni double clic qui entrerait en jeu, ni création.
      EditSeat(state, seat, seating, list, display, hovered);
    } else if (state.moving()) {
      // ⚠ PENDANT UN DÉPLACEMENT, LE CLIC DÉSIGNE L'ARRIVÉE : ni sélection, ni
      // entrée en jeu, ni création. Et rien ne part encore — voir
      // `moving_target`.
      if (clicked && IsMoveTarget(state, view, slot)) {
        state.moving_target = slot;
      }
    } else {
      if (clicked && who.occupied) {
        state.selected = slot;
      }
      // ⚠ LE DOUBLE CLIC ENTRE, et c'est le geste qu'on fait sans y penser sur
      // une liste de personnages. Le bouton reste, pour qui ne l'essaie pas.
      if (hovered && who.occupied && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        action.verb = CharacterSelectVerb::kEnter;
        action.slot = slot;
      }
      // ⚠ UN SEUL CLIC SUR UNE PLACE LIBRE, et non deux : il n'y a rien à
      // désigner d'abord, et le geste n'entre pas en jeu — il ouvre une
      // fenêtre, d'où l'on peut toujours revenir.
      if (clicked && creatable) {
        action.verb = CharacterSelectVerb::kCreate;
        action.slot = slot;
      }
    }
    ImGui::PopID();

    const bool move_target = IsMoveTarget(state, view, slot);
    const float grab_width = seating.grab_right - seating.grab_left;
    if (move_target && (state.moving_target == slot || hovered)) {
      list->AddCircleFilled(
          ImVec2(seating.centre_x, seating.chair_y), grab_width * kChosenHaloWidthRatio,
          state.moving_target == slot ? kMoveTargetHaloColor : kHoverHaloColor, kHaloSegments);
    }
    if (move_target && hovered) {
      MoveTargetTooltip(who, slot);
    }

    if (!who.occupied) {
      // 🔴 MASQUÉE, ELLE RESTE CLIQUABLE : le bouton invisible est déjà
      // soumis, seul le dessin est sauté — et elle reparaît au survol. Jamais
      // masquée pendant la personnalisation, où il faut pouvoir la placer, ni
      // pendant un déplacement, où elle est une arrivée possible.
      if (state.layout.hide_free_seats && !editing && !state.moving() && !hovered) {
        continue;
      }
      DrawFreeSeat(list, seating, hovered, state.layout.free_seat_alpha);
      if (hovered && !state.moving()) {
        SkinTooltipFormat(core::Tr("Créer un personnage (emplacement %d)"), slot + 1);
      }
      continue;
    }

    peopled_seats.push_back({slot, seating.centre_x, seating.chair_y});
    const bool chosen = state.selected == slot;
    // ⚠ UNE ARRIVÉE POSSIBLE A DÉJÀ SON HALO : deux disques empilés ne diraient
    // plus lequel est choisi.
    if (chosen && !move_target) {
      list->AddCircleFilled(ImVec2(seating.centre_x, seating.chair_y),
                            grab_width * kChosenHaloWidthRatio, kChosenHaloColor, kHaloSegments);
    } else if (hovered && !move_target) {
      list->AddCircleFilled(ImVec2(seating.centre_x, seating.chair_y),
                            grab_width * kHoverHaloWidthRatio, kHoverHaloColor, kHaloSegments);
    }
    DrawDoll(list, seating, who);
    DrawNamePlate(list, seating, who, chosen);
  }

  // ── Les flèches, d'un convive à son voisin ────────────────────────────────
  //
  // ⚠ AVANT LA BARRE D'ACTION : Entrée, qu'elle lit, porte alors sur le
  // convive que la flèche vient de désigner. Et pas pendant une saisie, sous
  // une fenêtre ni pendant la personnalisation, pour les mêmes raisons que F10.
  const bool arrows_ours =
      !editing && !state.moving() && !view.busy && !view.dialog_open && !state.rename.open() &&
      !ImGui::GetIO().WantTextInput &&
      !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
  if (arrows_ours) {
    constexpr std::array<std::pair<ImGuiKey, SeatStep>, 4> kArrows = {{
        {ImGuiKey_LeftArrow, SeatStep::kLeft},
        {ImGuiKey_RightArrow, SeatStep::kRight},
        {ImGuiKey_UpArrow, SeatStep::kUp},
        {ImGuiKey_DownArrow, SeatStep::kDown},
    }};
    for (const auto& [key, step] : kArrows) {
      if (!ImGui::IsKeyPressed(key, true)) {
        continue;
      }
      const int next_slot = NearestSeatToward(peopled_seats, state.selected, step);
      if (next_slot >= 0) {
        state.selected = next_slot;
      }
    }
  }

  // ── Le titre, et ce que la scène a à dire ─────────────────────────────────
  const char* title = core::Tr("Choisis ton personnage");
  const ImVec2 title_size = ImGui::CalcTextSize(title);
  const float centre_x = viewport->Pos.x + display.x * 0.5F;
  // Le point du titre est son milieu-haut.
  const ImVec2 title_at =
      SceneAnchor(state, kTitleAnchor, 0.5F, kTitleTopPixels / display.y, display);
  AddTextRelief(list, ImVec2(title_at.x - title_size.x * 0.5F, title_at.y), kTitleColor, title);

  float banner_top = title_at.y + title_size.y + 10.0F;
  if (!view.message.empty()) {
    DrawBanner(list, centre_x, banner_top, view.message.c_str(), kRefusalColor, kRefusalPlateColor);
    banner_top += title_size.y + 14.0F;
  }
  if (state.moving()) {
    const std::string moving = MoveBannerText(state, view);
    DrawBanner(list, centre_x, banner_top, moving.c_str(), kTitleColor, kPlateColor);
    banner_top += title_size.y + 14.0F;
  }
  // 🔴 AUCUN PERSONNAGE EST UNE RÉPONSE, ET NON UNE ATTENTE. C'est l'état d'un
  // compte qui vient d'être créé, et une salle vide sans un mot laisse croire
  // que la liste n'est pas arrivée.
  if (peopled == 0 && !view.busy) {
    DrawBanner(list, centre_x, banner_top, core::Tr("Ce compte n'a aucun personnage."), kTitleColor,
               kPlateColor);
  }

  // ── La barre de sortie, puis la barre d'action ─────────────────────────────
  //
  // ⚠ LA SORTIE SE MESURE AVANT QUE L'ACTION NE SE DESSINE : c'est la
  // comparaison des deux rectangles qui décide s'ils tiennent sur la même
  // rangée.
  const std::array<const char*, 2> exit_labels = {core::Tr("Revenir au login"),
                                                  core::Tr("Quitter le jeu")};
  const float exit_width = SkinWidestButton(exit_labels, art);
  const float exit_bar_width = exit_width * 2.0F + kBarGapPixels;
  const float bars_top = (display.y - kActionBarBottomPixels) / display.y;
  const ImVec2 exit_at = SceneAnchor(state, kExitBarAnchor,
                                     (display.x - exit_bar_width - kExitBarRightPixels) / display.x,
                                     bars_top, display);
  const ImVec2 action_at = SceneAnchor(state, kActionBarAnchor, 0.5F, bars_top, display);

  // 🔴 SUR UN ÉCRAN ÉTROIT, LES DEUX BARRES SE CHEVAUCHENT : la sortie monte
  // alors d'une rangée au-dessus de l'action, dont le côté droit est libre. Pas
  // pendant la personnalisation : le joueur doit voir ses pastilles là où il
  // les pose.
  ImVec2 exit_pos = exit_at;
  if (!editing) {
    const float row_height = BarRowHeight(art);
    // 🔴 LA SORTIE TIENT ENTIÈRE DANS L'ÉCRAN. Son ancre est une FRACTION de
    // l'écran, fixée pour la largeur de ses libellés à la taille du moment :
    // sur un écran plus étroit, ou dans une langue aux libellés plus longs,
    // « Quitter le jeu » sortirait par la droite — et c'est le seul bouton de
    // la tablée qui ferme le client.
    exit_pos.x = std::max(std::min(exit_pos.x, display.x - exit_bar_width), 0.0F);
    exit_pos.y = std::max(std::min(exit_pos.y, display.y - row_height), 0.0F);

    int widest_count = 0;
    const BarShape widest = WidestActionBar(art, &widest_count);
    const int widest_per_row = widest.rows > 1 ? (widest_count + 1) / 2 : widest_count;
    const float widest_width =
        static_cast<float>(widest_per_row) * (widest.width + kBarGapPixels) - kBarGapPixels;
    const float action_left = action_at.x - widest_width * 0.5F;
    const float action_right = action_left + widest_width;
    const bool same_row = std::fabs(exit_pos.y - action_at.y) < row_height;
    const bool overlap = action_right + kBarGapPixels > exit_pos.x &&
                         exit_pos.x + exit_bar_width + kBarGapPixels > action_left;
    if (same_row && overlap) {
      exit_pos.y = action_at.y - static_cast<float>(widest.rows) * row_height;
    }
  }

  const CharacterSlotView* designated =
      state.selected >= 0 && state.selected < slot_count &&
              view.slots[static_cast<std::size_t>(state.selected)].occupied
          ? &view.slots[static_cast<std::size_t>(state.selected)]
          : nullptr;
  const CharacterSelectAction from_bar = DrawActionBar(state, view, designated, art, action_at);
  const CharacterSelectVerb from_exit = DrawExitBar(art, exit_pos, exit_width);
  // ⚠ LE DOUBLE CLIC L'EMPORTE S'IL A EU LIEU : il est parti du siège, avant
  // la barre, et écraser son verbe ferait perdre le geste le plus naturel de
  // l'écran.
  if (action.verb == CharacterSelectVerb::kNothing) {
    action = from_bar;
  }
  if (action.verb == CharacterSelectVerb::kNothing) {
    action.verb = from_exit;
  }

  // ── Les pages ─────────────────────────────────────────────────────────────
  //
  // ⚠ SEULEMENT S'IL Y EN A PLUSIEURS : ce serveur en donne rarement assez
  // pour déborder des vingt-cinq sièges, et deux flèches grisées en
  // permanence annonceraient un ailleurs qui n'existe pas.
  if (pages > 1) {
    const ImVec2 pages_at = SceneAnchor(state, kPageBarAnchor, 0.5F,
                                        (display.y - kPageBarBottomPixels) / display.y, display);
    DrawPageBar(state, pages, art, list, pages_at);
  }

  // ── La personnalisation ───────────────────────────────────────────────────
  //
  // F10 EN DOUBLON DU BOUTON. ⚠ PAS PENDANT UNE SAISIE NI SOUS UNE FENÊTRE :
  // basculer le mode sous un formulaire de création laisserait le joueur
  // devant deux interactions concurrentes.
  const bool any_popup =
      ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
  if (!view.busy && !view.dialog_open && !state.moving() && !state.rename.open() && !any_popup &&
      !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F10, false) &&
      action.verb == CharacterSelectVerb::kNothing) {
    action.verb = ToggleCustomizing(state);
  }
  DrawSeatPoseMenu(state, view, skin_padding);
  if (state.customizing) {
    DrawLayoutGuides(list, viewport->Pos, display);
  }

  ImGui::EndDisabled();
  ImGui::End();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
  PopSkinStyle(skin);

  // ⚠ LE PANNEAU APRÈS LA SCÈNE, donc au-dessus d'elle : voir
  // `DrawLayoutPanel`.
  if (state.customizing) {
    const CharacterSelectAction from_panel = DrawLayoutPanel(state, view, art);
    if (from_panel.verb != CharacterSelectVerb::kNothing) {
      action = from_panel;
    }
  }
  // ⚠ LA FENÊTRE DU NOUVEAU NOM AUSSI, pour la même raison.
  const CharacterSelectAction from_rename = DrawRenameWindow(state, view, art);
  if (from_rename.verb != CharacterSelectVerb::kNothing) {
    action = from_rename;
  }

  // LE VOILE DE L'ENTRÉE EN JEU, par-dessus TOUT — la scène, le panneau, une
  // fenêtre restée ouverte : c'est la dernière image avant la carte.
  if (view.busy) {
    DrawEnterFade(view.entering_seconds);
  }
  return action;
}

}  // namespace ro::ui
