#include "ro/ui/navigation_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "navigation_parts.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

// Les mesures de la fenêtre, en pixels d'interface.
constexpr float kFirstLeftPixels = 40.0F;
constexpr float kFirstTopPixels = 90.0F;
constexpr float kFirstWidthPixels = 430.0F;
constexpr float kFirstHeightPixels = 470.0F;
// La place laissée sous les deux volets à l'itinéraire, et la largeur du
// volet de détail.
constexpr float kFooterPixels = 120.0F;
constexpr float kDetailWidthPixels = 220.0F;
constexpr float kCurrentIconPixels = 24.0F;
constexpr float kPickedIconPixels = 32.0F;
// Quatre vignettes par rangée : les huit tiennent en deux.
constexpr int kIconsPerRow = 4;
constexpr float kHelpWrapEm = 24.0F;

// Le rouge d'un échec, soutenu pour se lire sur le corps clair.
constexpr ImU32 kNoPathColor = IM_COL32(158, 38, 26, 255);
// Le cadre de la trace choisie : sans lui, rien ne dit laquelle des huit est
// active une fois le choix ouvert.
constexpr ImU32 kPickedIconFrame = IM_COL32(217, 184, 89, 140);
constexpr ImU32 kClearButton = IM_COL32(0, 0, 0, 0);
constexpr float kUnderlineDrop = 1.0F;

constexpr const char* kIconPopupId = "##navi_icon_pick";
constexpr const char* kStepSeparator = "  >  ";
constexpr std::size_t kIdBytes = 32;

[[nodiscard]] std::string Owned(std::string_view text) { return std::string(text); }

void ForgetSelection(NavigationWindowState& state) {
  state.selected_group = -1;
  state.selected_entry = -1;
}

void SetTerm(NavigationWindowState& state, std::string_view term) {
  state.term.fill('\0');
  std::copy_n(term.begin(), std::min(term.size(), state.term.size() - 1), state.term.begin());
}

// ── LE HAUT : saisie, pastilles, aide, raccourcis de la map courante ─────────

void DrawSearchBar(NavigationWindowState& state, const SkinArt& art, NavigationAnswer& answer) {
  ImGui::SetNextItemWidth(-1.0F);
  if (ImGui::InputTextWithHint("##navi_term", core::Tr("Chercher une carte, un NPC, un monstre..."),
                               state.term.data(), state.term.size())) {
    // Le terme part à la frappe. Et taper LIBÈRE le bornage : la question
    // devient celle du joueur, et un bornage hérité lui cacherait des
    // résultats sans dire pourquoi.
    answer.term = std::string(state.term.data());
    state.bound_map.clear();
  }

  // ⚠ « Maps » ET « NPC » NE SE TRADUISENT PAS : ce sont les termes du jeu, et
  // « Cartes » se lirait « Cards » — les cartes-items.
  struct Pill {
    const char* label;
    NavigationFilter filter;
  };
  const std::array<Pill, 4> pills = {Pill{core::Tr("Tout"), NavigationFilter::kAll},
                                     Pill{"Maps", NavigationFilter::kMaps},
                                     Pill{"NPC", NavigationFilter::kNpcs},
                                     Pill{core::Tr("Monstres"), NavigationFilter::kMonsters}};
  for (std::size_t rank = 0; rank < pills.size(); ++rank) {
    if (rank != 0) {
      ImGui::SameLine();
    }
    // Un filtre d'AFFICHAGE : rien ne se relance.
    if (SkinToggleButton(pills[rank].label, state.filter == pills[rank].filter, art)) {
      state.filter = pills[rank].filter;
    }
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered() && BeginSkinTooltip()) {
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * kHelpWrapEm);
    ImGui::TextUnformatted(core::Tr(
        "Tapez un nom de map, de NPC ou de monstre : la recherche se relance à la frappe, sans "
        "rien demander au serveur.\n\nChoisissez un résultat pour voir ses détails, puis « Y "
        "aller » pour être guidé. Le chemin passe par le Warp Agent quand la marche ne suffit "
        "pas.\n\nClic droit sur un résultat : le menu. Maj+clic : le lien dans le chat."));
    ImGui::PopTextWrapPos();
    EndSkinTooltip();
  }
}

// « Ici », en deux boutons : ce sont deux questions. Chercher le nom de la map
// rend la map ; voir ce qu'il y a dessus rend ses NPC et ses monstres.
void DrawHereButtons(NavigationWindowState& state, const NavigationView& view, const SkinArt& art,
                     NavigationAnswer& answer) {
  const bool here_known = !view.current_map.empty();
  ImGui::BeginDisabled(!here_known);
  if (SkinButton(core::Tr("Rechercher la map actuelle"), art)) {
    // 🔴 LE NOM INTERNE, jamais le lisible : c'est lui que la recherche est
    // sûre de trouver.
    SetTerm(state, view.current_map);
    state.filter = NavigationFilter::kAll;
    state.bound_map.clear();
    ForgetSelection(state);
    answer.term = std::string(view.current_map);
    answer.search_here = true;
  }
  ImGui::EndDisabled();
  if (here_known && ImGui::IsItemHovered()) {
    SkinTooltipFormat("%s  (%s)", Owned(view.current_map_label).c_str(),
                      Owned(view.current_map).c_str());
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!here_known);
  if (SkinButton(core::Tr("Voir ce qu'il y a ici"), art)) {
    // La saisie se vide : elle ne correspondrait à rien de ce qui s'affiche.
    state.term.fill('\0');
    state.filter = NavigationFilter::kAll;
    state.bound_map.clear();
    ForgetSelection(state);
    answer.show_here = true;
  }
  ImGui::EndDisabled();
  if (here_known) {
    SkinTooltip(core::Tr("Les PNJ et les monstres déclarés sur cette carte."));
  }
}

// 🔴 UNE LISTE TRONQUÉE SANS EXPLICATION EST UN BUG aux yeux du joueur : on
// dit pourquoi, et l'on offre de lever le bornage sur place.
void DrawBoundBanner(NavigationWindowState& state, const NavigationView& view, const SkinArt& art,
                     NavigationAnswer& answer) {
  if (state.bound_map.empty()) {
    return;
  }
  ImGui::TextDisabled("%s", core::Tr("Limité à cette carte :"));
  ImGui::SameLine();
  const std::string shown =
      view.bound_map_label.empty() ? state.bound_map : Owned(view.bound_map_label);
  ImGui::TextUnformatted(shown.c_str());
  ImGui::SameLine();
  if (SkinButton(core::Tr("Partout"), art)) {
    state.bound_map.clear();
    answer.unbound = true;
  }
  SkinTooltip(core::Tr("Chercher ce nom sur toutes les cartes."));
}

// ── LE BAS : l'itinéraire ───────────────────────────────────────────────────

// Les étapes « a  >  b  >  c », repliées sur la largeur. L'étape COURANTE est
// soulignée et de la couleur du jeu, et chaque étape dit au survol comment on
// la quitte.
void DrawSteps(const NavigationRouteView& route) {
  const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
  const NavigationRouteStep* current = nullptr;
  for (std::size_t rank = 0; rank < route.steps.size(); ++rank) {
    const NavigationRouteStep& step = route.steps[rank];
    const std::string label = Owned(step.map_label);
    if (rank != 0) {
      ImGui::SameLine(0.0F, 0.0F);
      const float needed =
          ImGui::CalcTextSize(kStepSeparator).x + ImGui::CalcTextSize(label.c_str()).x;
      if (ImGui::GetCursorScreenPos().x + needed > right) {
        ImGui::NewLine();
      }
      ImGui::TextUnformatted(kStepSeparator);
      ImGui::SameLine(0.0F, 0.0F);
    }
    if (step.current) {
      current = &step;
      ImGui::PushStyleColor(ImGuiCol_Text, PaintingLook().good_text);
      ImGui::TextUnformatted(label.c_str());
      ImGui::PopStyleColor();
      const ImVec2 low = ImGui::GetItemRectMin();
      const ImVec2 high = ImGui::GetItemRectMax();
      ImGui::GetWindowDrawList()->AddLine(ImVec2(low.x, high.y - kUnderlineDrop),
                                          ImVec2(high.x, high.y - kUnderlineDrop),
                                          PaintingLook().good_text);
    } else {
      ImGui::TextUnformatted(label.c_str());
    }
    if (!step.passage.empty() && ImGui::IsItemHovered()) {
      SkinTooltipFormat("%s", Owned(step.passage).c_str());
    }
  }
  if (current != nullptr && !current->passage.empty()) {
    ImGui::TextWrapped(core::Tr("Prochain passage : %s"), Owned(current->passage).c_str());
  }
}

// Une vignette de trace en bouton ; un numéro faute d'art — un trou dans une
// rangée ne se clique pas.
bool TrailIconButton(const NavigationWindowState& state, const NavigationView& view,
                     const SkinArt& art, int icon, float side, const char* id) {
  const SkinImage& image = view.trail_icons[static_cast<std::size_t>(icon - 1)];
  ImGui::PushID(id);
  bool clicked = false;
  if (image.usable()) {
    ImGui::PushStyleColor(ImGuiCol_Button,
                          icon == state.trail_icon ? kPickedIconFrame : kClearButton);
    clicked = ImGui::ImageButton(id, image.id, ImVec2(side, side));
    ImGui::PopStyleColor();
  } else {
    std::array<char, kIdBytes> label{};
    std::snprintf(label.data(), label.size(), "%d", icon);
    clicked = SkinButton(label.data(), art, ImVec2(side, side));
  }
  ImGui::PopID();
  return clicked;
}

// LE CHOIX DE LA TRACE AU SOL : un bouton qui porte la trace courante et
// déroule les huit.
void DrawTrailPicker(NavigationWindowState& state, const NavigationView& view, const SkinArt& art,
                     NavigationAnswer& answer) {
  state.trail_icon = std::clamp(state.trail_icon, 1, kNavigationTrailIconCount);
  if (TrailIconButton(state, view, art, state.trail_icon, Px(kCurrentIconPixels),
                      "##navi_icon_cur")) {
    ImGui::OpenPopup(kIconPopupId);
  }
  SkinTooltip(core::Tr("Trace du guidage au sol"));
  if (!BeginSkinPopup(kIconPopupId)) {
    return;
  }
  for (int icon = 1; icon <= kNavigationTrailIconCount; ++icon) {
    std::array<char, kIdBytes> id{};
    std::snprintf(id.data(), id.size(), "##navi_icon%d", icon);
    if (TrailIconButton(state, view, art, icon, Px(kPickedIconPixels), id.data())) {
      state.trail_icon = icon;
      answer.trail_icon = icon;
      ImGui::CloseCurrentPopup();
    }
    if (icon % kIconsPerRow != 0) {
      ImGui::SameLine();
    }
  }
  EndSkinPopup();
}

void DrawRoute(NavigationWindowState& state, const NavigationView& view, const SkinArt& art,
               NavigationAnswer& answer) {
  const NavigationRouteView& route = view.route;
  if (route.already_here) {
    // Ni rouge ni alarmant : la demande a abouti, elle n'avait nulle part où
    // mener.
    ImGui::TextDisabled("%s", core::Tr("Vous êtes déjà sur cette carte."));
  } else if (route.steps.empty() && route.no_path) {
    ImGui::PushStyleColor(ImGuiCol_Text, kNoPathColor);
    ImGui::TextWrapped("%s", core::Tr("Aucun chemin jusqu'à cette destination."));
    ImGui::PopStyleColor();
    ImGui::TextWrapped("%s", core::Tr("Aucune liaison connue ne mène jusque-là."));
  } else if (route.steps.empty()) {
    // 🔴 « PAS D'ÉTAPE » N'EST PAS « PAS DE GUIDAGE » : une destination sur la
    // map courante n'en a aucune, et c'est le cas ordinaire.
    ImGui::TextDisabled("%s", route.guiding ? core::Tr("Guidage en cours, sans étape à afficher.")
                                            : core::Tr("Aucun itinéraire en cours."));
  } else {
    DrawSteps(route);
  }
  if (route.distance_cells >= 0) {
    ImGui::TextDisabled(core::Tr("Distance estimée : %d cellules"), route.distance_cells);
  }
  if (route.guiding) {
    if (SkinButton(core::Tr("Arrêter le guidage"), art)) {
      answer.stop_guidance = true;
    }
    SkinTooltip(core::Tr("Efface l'itinéraire et les traces au sol."));
    ImGui::SameLine();
  }
  DrawTrailPicker(state, view, art, answer);
}

// La place de l'itinéraire : une hauteur de base, plus une ligne par ajout.
[[nodiscard]] float FooterHeight(const NavigationView& view) {
  float height = Px(kFooterPixels);
  const float line = ImGui::GetTextLineHeightWithSpacing();
  if (view.route.distance_cells >= 0) {
    height += line;
  }
  const bool passage_shown = std::ranges::any_of(
      view.route.steps,
      [](const NavigationRouteStep& s) { return s.current && !s.passage.empty(); });
  if (passage_shown) {
    height += line;
  }
  return height;
}

void DrawBody(NavigationWindowState& state, const NavigationView& view, const SkinArt& art,
              NavigationAnswer& answer) {
  if (view.graph_map_count <= 0) {
    ImGui::TextUnformatted(core::Tr("Les données de navigation ne sont pas chargées."));
    return;
  }
  // Diagnostic discret : le nombre de maps dit d'un coup d'œil si les données
  // chargées sont celles qu'on croit.
  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
      ImGui::IsKeyDown(ImGuiKey_LeftAlt)) {
    SkinTooltipFormat(core::Tr("%d maps dans le graphe de navigation."), view.graph_map_count);
  }
  DrawSearchBar(state, art, answer);
  DrawHereButtons(state, view, art, answer);
  DrawBoundBanner(state, view, art, answer);
  ImGui::Separator();

  const float footer = FooterHeight(view);
  const float detail_width = Px(kDetailWidthPixels);
  if (ImGui::BeginChild("##navi_results", ImVec2(-detail_width, -footer),
                        ImGuiChildFlags_Borders)) {
    DrawNavigationResults(state, view, answer);
  }
  ImGui::EndChild();
  ImGui::SameLine();
  state.preview_side = Px(kNavigationPreviewPixels);
  if (ImGui::BeginChild("##navi_detail", ImVec2(0.0F, -footer), ImGuiChildFlags_Borders)) {
    DrawNavigationDetail(state, view, art, answer);
  }
  ImGui::EndChild();
  DrawRoute(state, view, art, answer);
  DrawNavigationRowMenu(state, view, answer);
}

}  // namespace

bool NavigationGroupShown(NavigationFilter filter, NavigationKind kind) {
  switch (filter) {
    case NavigationFilter::kAll:
      return true;
    case NavigationFilter::kMaps:
      return kind == NavigationKind::kMap;
    case NavigationFilter::kNpcs:
      return kind == NavigationKind::kNpc;
    case NavigationFilter::kMonsters:
      return kind == NavigationKind::kMonster;
  }
  return true;
}

const NavigationEntryView* NavigationSelection(const NavigationWindowState& state,
                                               const NavigationView& view) {
  if (state.selected_group < 0 || state.selected_entry < 0) {
    return nullptr;
  }
  const auto group = static_cast<std::size_t>(state.selected_group);
  const auto entry = static_cast<std::size_t>(state.selected_entry);
  if (group >= view.groups.size() || entry >= view.groups[group].entries.size()) {
    return nullptr;
  }
  return &view.groups[group].entries[entry];
}

const char* NavigationElementName(int element) {
  switch (element) {
    case 0:
      return core::Tr("Neutre");
    case 1:
      return core::Tr("Eau");
    case 2:
      return core::Tr("Terre");
    case 3:
      return core::Tr("Feu");
    case 4:
      return core::Tr("Vent");
    case 5:
      return core::Tr("Poison");
    case 6:
      return core::Tr("Sacré");
    case 7:
      return core::Tr("Ombre");
    case 8:
      return core::Tr("Fantôme");
    case 9:
      return core::Tr("Mort-vivant");
    default:
      return "?";
  }
}

const char* NavigationSizeName(int size) {
  switch (size) {
    case 0:
      return core::Tr("Petite");
    case 1:
      return core::Tr("Moyenne");
    case 2:
      return core::Tr("Grande");
    default:
      return "?";
  }
}

const char* NavigationRaceName(int race) {
  switch (race) {
    case 0:
      return core::Tr("Informe");
    case 1:
      return core::Tr("Mort-vivant");
    case 2:
      return core::Tr("Bête");
    case 3:
      return core::Tr("Plante");
    case 4:
      return core::Tr("Insecte");
    case 5:
      return core::Tr("Poisson");
    case 6:
      return core::Tr("Démon");
    case 7:
      return core::Tr("Demi-humain");
    case 8:
      return core::Tr("Ange");
    case 9:
      return core::Tr("Dragon");
    default:
      return "?";
  }
}

void OpenNavigationSearch(NavigationWindowState& state, std::string_view term, bool monsters_only,
                          std::string_view bound_map) {
  if (term.empty()) {
    return;
  }
  state.open = true;
  state.raise = true;
  SetTerm(state, term);
  // Sans la pastille Monstres, un nom courant comme « Poring » se noierait sous
  // les maps et les NPC homonymes.
  state.filter = monsters_only ? NavigationFilter::kMonsters : NavigationFilter::kAll;
  state.bound_map = std::string(bound_map);
  ForgetSelection(state);
}

NavigationAnswer DrawNavigationWindow(NavigationWindowState& state, const NavigationView& view,
                                      const SkinArt& art) {
  NavigationAnswer answer;
  if (!state.open) {
    return answer;
  }
  // Les résultats ont changé : la sélection d'avant appartient aux résultats
  // d'avant, et le détail montrerait une entrée qui n'est plus dans la liste.
  if (view.results_serial != state.seen_serial) {
    state.seen_serial = view.results_serial;
    ForgetSelection(state);
  }
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
      ImVec2(screen->WorkPos.x + Px(kFirstLeftPixels), screen->WorkPos.y + Px(kFirstTopPixels)),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(Px(kFirstWidthPixels), Px(kFirstHeightPixels)),
                           ImGuiCond_FirstUseEver);
  // ⚠ L'IDENTITÉ, PAS LE TITRE : la place se retient sous un nom qui ne dépend
  // pas de la langue.
  const std::string title = std::string(core::Tr("Navigation")) + "###" + kNavigationWindowId;
  PoseBugReportButton(state.bug_report);
  if (BeginSkinnedWindow(title.c_str(), &state.open, art)) {
    answer.report_bug = state.bug_report && TitleActionClicked(0);
    DrawBody(state, view, art, answer);
  }
  EndSkinnedWindow();
  return answer;
}

}  // namespace ro::ui
