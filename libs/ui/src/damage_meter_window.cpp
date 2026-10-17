#include "ro/ui/damage_meter_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"

#include "damage_meter_parts.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

using damage_meter::kBigTextScale;
using damage_meter::kLabelTextScale;
using damage_meter::kUnitTextScale;
using damage_meter::NumberFont;
using damage_meter::PaintText;
using damage_meter::TextWidth;
using damage_meter::Tint;

constexpr float kMsPerSecond = 1000.0F;
constexpr float kTwoPi = 6.2831853F;

// ── LES SEUILS DE L'ÉCRITURE ABRÉGÉE ────────────────────────────────────────
constexpr std::int64_t kAbbreviateFrom = 100000;
constexpr std::int64_t kMillion = 1000000;
// Un dixième de millier, un centième de million : les unités de l'arrondi.
constexpr std::int64_t kTenthOfThousand = 100;
constexpr std::int64_t kHundredthOfMillion = 10000;
constexpr std::int64_t kDecimalsOfThousand = 10;
constexpr std::int64_t kDecimalsOfMillion = 100;
constexpr std::size_t kDigitsPerGroup = 3;
constexpr std::uint64_t kDecimalBase = 10;

// ── LA FENÊTRE, en pixels à l'échelle 100 % ─────────────────────────────────
constexpr float kFirstWidthPixels = 286.0F;
constexpr float kFirstHeightPixels = 196.0F;
constexpr float kWindowRoundingPixels = 8.0F;
constexpr float kFrameRoundingPixels = 3.0F;
constexpr float kPadXPixels = 9.0F;
constexpr float kPadYPixels = 8.0F;
constexpr float kBandGapPixels = 5.0F;
// Le plancher de largeur : le graphique se soumet en bouton invisible, dont
// ImGui refuse une taille nulle — et une fenêtre resserrée jusqu'à sa taille
// minimale peut rendre une région disponible négative.
constexpr float kLeastWidthPixels = 24.0F;
// La courbe ne descend pas plus bas ; en deçà, les tuiles s'effacent pour elle.
constexpr float kLeastGraphPixels = 30.0F;
constexpr float kFloorGraphPixels = 18.0F;
constexpr float kHeaderRoomPixels = 6.0F;
constexpr float kStatsRoomPixels = 8.0F;

// Le fond, quasi noir : le doré du compteur ne tient pas sur le corps clair du
// skin. La barre de titre prend la couleur du joueur, assombrie.
constexpr ImVec4 kWindowBackground(0.055F, 0.055F, 0.070F, 1.0F);
constexpr float kTitleDarkening = 0.16F;
constexpr float kActiveTitleDarkening = 0.26F;
constexpr float kCollapsedTitleOpacity = 0.85F;
// Le liseré suit l'opacité du fond : une bordure pleine autour d'une fenêtre
// presque transparente dessinerait un rectangle flottant dans le décor.
constexpr float kBorderDarkening = 0.55F;
constexpr float kBorderOpacity = 0.45F;

// ── L'EN-TÊTE ───────────────────────────────────────────────────────────────
constexpr float kDotRadiusPixels = 3.5F;
constexpr float kDotGapPixels = 7.0F;
constexpr float kUnitGapPixels = 4.0F;
// La pastille BAT en combat : le regard apprend l'état sans lire.
constexpr float kPulseMs = 1100.0F;
constexpr float kPulseGrowth = 1.1F;
constexpr ImU32 kFightDotColor = IM_COL32(255, 110, 70, 255);
constexpr ImU32 kRestDotColor = IM_COL32(125, 125, 130, 150);
constexpr int kHaloRed = 255;
constexpr int kHaloGreen = 95;
constexpr int kHaloBlue = 60;
constexpr float kHaloLeastAlpha = 20.0F;
constexpr float kHaloAlphaSwing = 45.0F;
// Le chevron du palmarès, à droite de l'en-tête.
constexpr float kChevronPixels = 9.0F;
constexpr float kChevronGapPixels = 6.0F;
// Le repère d'insertion, quand le compteur survole une bande d'onglets.
constexpr float kDockMarkerPixels = 2.0F;

// ── LA COURBE ───────────────────────────────────────────────────────────────
// De l'air au-dessus de la plus haute colonne : un sommet collé au bord se
// lirait comme une valeur tronquée.
constexpr float kGraphHeadroom = 1.18F;
constexpr int kGridLines = 3;
constexpr ImU32 kGraphBackground = IM_COL32(0, 0, 0, 55);
constexpr ImU32 kGridColor = IM_COL32(255, 255, 255, 13);
constexpr ImU32 kGraphBorder = IM_COL32(255, 255, 255, 20);
constexpr ImU32 kPeakLineColor = IM_COL32(255, 255, 255, 60);
constexpr float kPeakDashPixels = 4.0F;
constexpr float kPeakGapPixels = 3.0F;
constexpr float kCurveWidthPixels = 1.3F;
constexpr float kHeadHaloPixels = 3.5F;
constexpr float kHeadDotPixels = 1.8F;

// Les quatre bandes du compteur, en hauteur.
struct Bands {
  float header = 0.0F;
  float graph = 0.0F;
  float stats = 0.0F;
  float records = 0.0F;
  // Les rangs du palmarès qu'on montre : zéro, le volet ne paraît pas.
  std::size_t record_rows = 0;
  // La barre d'outils, en tête ; et la place du groupe, quand c'est lui qu'on
  // montre — la courbe, les tuiles et le palmarès lui cèdent tout.
  float toolbar = 0.0F;
  float group = 0.0F;
  bool with_stats = false;
};

[[nodiscard]] std::uint32_t LowerOf(std::uint32_t at, std::uint32_t period) {
  return period == 0 ? 0 : at % period;
}

// ── L'en-tête : l'état du combat, et le chiffre qu'on vient lire ────────────
void DrawHeader(const DamageMeterView& view, float width, float height, DamageMeterAction* asked) {
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImFont* regular = ImGui::GetFont();
  ImFont* bold = NumberFont(view);
  const float base_px = ImGui::GetFontSize();
  const float big_px = base_px * kBigTextScale;
  const float unit_px = base_px * kUnitTextScale;
  const ImU32 text = view.look.text_color;

  // La pastille d'état. En combat, elle bat.
  const float dot_r = Px(kDotRadiusPixels);
  const ImVec2 dot(origin.x + dot_r, origin.y + height * 0.5F);
  if (view.in_combat) {
    const float phase =
        static_cast<float>(LowerOf(view.now_ms, static_cast<std::uint32_t>(kPulseMs))) / kPulseMs;
    const float swing = 0.5F + 0.5F * std::sin(phase * kTwoPi);
    into->AddCircleFilled(
        dot, dot_r * (1.0F + kPulseGrowth * swing),
        IM_COL32(kHaloRed, kHaloGreen, kHaloBlue,
                 static_cast<int>(kHaloLeastAlpha + kHaloAlphaSwing * (1.0F - swing))));
    into->AddCircleFilled(dot, dot_r, kFightDotColor);
  } else {
    into->AddCircleFilled(dot, dot_r, kRestDotColor);
  }

  // Le nombre. Éteint hors combat : ce qu'il affiche alors est un zéro qui vient
  // de tomber, pas une mesure.
  const std::string number = FormatDamageAmount(std::llround(view.dps));
  const float number_x = dot.x + dot_r + Px(kDotGapPixels);
  const float number_y = origin.y + (height - big_px) * 0.5F;
  PaintText(into, bold, big_px, ImVec2(number_x, number_y),
            Tint(text, view.in_combat ? 1.0F : 0.45F), number.c_str());

  // L'unité, calée sur la LIGNE DE BASE du nombre et non sur son sommet : elle
  // se lit comme un suffixe plutôt que comme un second texte.
  PaintText(into, regular, unit_px,
            ImVec2(number_x + TextWidth(bold, big_px, number.c_str()) + Px(kUnitGapPixels),
                   number_y + big_px - unit_px - Px(1.0F)),
            Tint(text, view.in_combat ? 0.60F : 0.30F), DamageMeterRateUnit(view.mode));

  // Le chevron du volet, tout à droite : il dit où le palmarès se déplie.
  const float chevron = Px(kChevronPixels);
  const ImVec2 chevron_min(origin.x + width - chevron, origin.y + (height - chevron) * 0.5F);
  ImGui::SetCursorScreenPos(chevron_min);
  if (ImGui::InvisibleButton("##volet_palmares", ImVec2(chevron, chevron))) {
    asked->records = !view.look.records;
  }
  const bool chevron_hovered = ImGui::IsItemHovered();
  if (chevron_hovered) {
    SkinTooltipFormat("%s", view.look.records
                                ? core::Tr("Replier le palmarès")
                                : core::Tr("Déplier le palmarès des 10 plus gros coups"));
  }
  const ImU32 chevron_color = chevron_hovered ? Tint(text, 1.0F) : damage_meter::kQuietColor;
  const float third = chevron / 3.0F;
  if (view.look.records) {
    into->AddTriangleFilled(ImVec2(chevron_min.x, chevron_min.y + 2.0F * third),
                            ImVec2(chevron_min.x + chevron, chevron_min.y + 2.0F * third),
                            ImVec2(chevron_min.x + chevron * 0.5F, chevron_min.y + third),
                            chevron_color);
  } else {
    into->AddTriangleFilled(ImVec2(chevron_min.x, chevron_min.y + third),
                            ImVec2(chevron_min.x + chevron, chevron_min.y + third),
                            ImVec2(chevron_min.x + chevron * 0.5F, chevron_min.y + 2.0F * third),
                            chevron_color);
  }

  // À sa gauche, la durée du combat en cours — ou son absence.
  std::array<char, 48> right{};
  if (view.in_combat) {
    std::snprintf(right.data(), right.size(), core::Tr("%.0f s"),
                  static_cast<double>(static_cast<float>(view.combat_ms) / kMsPerSecond));
  } else {
    // Un soin n'a pas de combat : il est au repos.
    const bool healing =
        view.mode == DamageMeterMode::kHealingDone || view.mode == DamageMeterMode::kHealingTaken;
    std::snprintf(right.data(), right.size(), "%s",
                  healing ? core::Tr("au repos") : core::Tr("hors combat"));
  }
  PaintText(
      into, regular, base_px,
      ImVec2(chevron_min.x - Px(kChevronGapPixels) - TextWidth(regular, base_px, right.data()),
             origin.y + (height - base_px) * 0.5F),
      view.in_combat ? damage_meter::kSubColor : damage_meter::kQuietColor, right.data());

  // Le filet de séparation, dégradé vers la droite : il pose l'en-tête sans
  // couper la fenêtre en deux comme le ferait un trait plein.
  const float line_y = origin.y + height - Px(1.0F);
  into->AddRectFilledMultiColor(ImVec2(origin.x, line_y),
                                ImVec2(origin.x + width, line_y + Px(1.0F)), Tint(text, 0.50F),
                                Tint(text, 0.0F), Tint(text, 0.0F), Tint(text, 0.50F));

  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(width, height));
}

// ── La courbe : une AIRE, pas un fil ────────────────────────────────────────
// Une ligne d'un pixel sur un fond translucide se perd dans le décor. L'aire
// dégradée donne la masse : on voit la forme du combat — montée, plateau,
// chute — avant d'avoir lu un chiffre.
void DrawGraph(const DamageMeterView& view, float width, float height) {
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 low(ImGui::GetCursorScreenPos());
  const ImVec2 high(low.x + width, low.y + height);
  const float rounding = Px(kFrameRoundingPixels);
  const ImU32 plot_color = view.look.plot_color;
  const std::size_t slots = view.plot.size();

  into->AddRectFilled(low, high, kGraphBackground, rounding);

  // L'échelle suit la plus haute colonne VISIBLE. Elle est propre au graphique :
  // une colonne couvre bien moins que la fenêtre du rythme affiché, donc elle
  // monte plus haut que lui.
  float highest = 1.0F;
  for (const float value : view.plot) {
    highest = std::max(highest, value);
  }
  const float scale_top = highest * kGraphHeadroom;

  // Trois repères horizontaux, assez pâles pour ne jamais concurrencer la
  // courbe.
  for (int line = 1; line <= kGridLines; ++line) {
    const float y =
        low.y + height * (static_cast<float>(line) / static_cast<float>(kGridLines + 1));
    into->AddLine(ImVec2(low.x + rounding, y), ImVec2(high.x - rounding, y), kGridColor);
  }

  std::vector<ImVec2> curve(slots);
  if (slots != 0) {
    const float step = width / static_cast<float>(slots);
    const ImU32 top = Tint(plot_color, 0.55F);
    const ImU32 bottom = Tint(plot_color, 0.05F);
    for (std::size_t rank = 0; rank < slots; ++rank) {
      // Le tampon est circulaire : `plot_offset` est la colonne la plus ANCIENNE.
      const float value = view.plot[(view.plot_offset + rank) % slots];
      const float x = low.x + step * static_cast<float>(rank);
      const float y = std::max(low.y, high.y - (value / scale_top) * height);
      if (value > 0.0F) {
        into->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + step, high.y), top, top, bottom,
                                      bottom);
      }
      curve[rank] = ImVec2(x + step * 0.5F, y);
    }
    into->AddPolyline(curve.data(), static_cast<int>(slots), Tint(plot_color, 0.95F), 0,
                      std::max(1.0F, Px(kCurveWidthPixels)));
  }

  // Le pic, en pointillés : le trait qui RELIE la tuile « pic » à la courbe.
  // Seulement s'il tombe dans l'échelle — une salve courte peut la dépasser.
  if (view.peak_dps > 0.0F && view.peak_dps <= scale_top) {
    const float y = high.y - (view.peak_dps / scale_top) * height;
    const float dash = Px(kPeakDashPixels);
    const float gap = Px(kPeakGapPixels);
    for (float x = low.x; x < high.x; x += dash + gap) {
      into->AddLine(ImVec2(x, y), ImVec2(std::min(x + dash, high.x), y), kPeakLineColor);
    }
  }

  // La tête de courbe : le seul endroit qui bouge, qu'on retrouve d'un regard.
  if (view.in_combat && slots != 0) {
    into->AddCircleFilled(curve.back(), Px(kHeadHaloPixels), Tint(plot_color, 0.22F));
    into->AddCircleFilled(curve.back(), Px(kHeadDotPixels), Tint(plot_color, 1.0F));
  }

  into->AddRect(low, high, kGraphBorder, rounding);

  // L'item qui porte le survol — et qui avance le curseur de la fenêtre.
  ImGui::InvisibleButton("##courbe_degats", ImVec2(width, height));
  if (ImGui::IsItemHovered() && slots != 0) {
    const float across =
        std::clamp((ImGui::GetIO().MousePos.x - low.x) / width, 0.0F, 1.0F - FLT_EPSILON);
    const auto rank = static_cast<std::size_t>(across * static_cast<float>(slots));
    const std::string amount =
        FormatDamageAmount(std::llround(view.plot[(view.plot_offset + rank) % slots]));
    const float ago_s = static_cast<float>((slots - 1 - rank) * view.slot_ms) / kMsPerSecond;
    SkinTooltipFormat(core::Tr("%s %s  ·  il y a %.1f s"), amount.c_str(),
                      DamageMeterRateUnit(view.mode), static_cast<double>(ago_s));
  }
}

// ── Les trois tuiles du bas ─────────────────────────────────────────────────
// Le chiffre qui compte est celui d'en haut ; ceux-ci sont le bilan. Plus
// petits, sous un intitulé plus petit encore : la hiérarchie tient à la taille,
// pas à la couleur, qui reste celle du réglage.
void DrawStats(const DamageMeterView& view, float width, float height) {
  ImDrawList* into = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImFont* regular = ImGui::GetFont();
  ImFont* bold = NumberFont(view);
  const float base_px = ImGui::GetFontSize();
  const float label_px = base_px * kLabelTextScale;

  const float span_s = static_cast<float>(view.fight_span_ms) / kMsPerSecond;
  const float average = span_s > 0.0F ? static_cast<float>(view.total_damage) / span_s : 0.0F;

  struct Tile {
    const char* label;
    std::int64_t value;
  };
  const std::array<Tile, 3> tiles = {{
      {core::Tr("PIC"), std::llround(view.peak_dps)},
      {core::Tr("MOY"), std::llround(average)},
      {core::Tr("TOTAL"), view.total_damage},
  }};

  const float gap = Px(damage_meter::kTileGapPixels);
  const float tile_width =
      (width - gap * static_cast<float>(tiles.size() - 1)) / static_cast<float>(tiles.size());
  const float pad = Px(damage_meter::kTilePadPixels);
  for (std::size_t rank = 0; rank < tiles.size(); ++rank) {
    const ImVec2 low(origin.x + (tile_width + gap) * static_cast<float>(rank), origin.y);
    const ImVec2 high(low.x + tile_width, origin.y + height);
    into->AddRectFilled(low, high, damage_meter::kTileColor, Px(damage_meter::kTileRoundingPixels));
    PaintText(into, regular, label_px,
              ImVec2(low.x + (tile_width - TextWidth(regular, label_px, tiles[rank].label)) * 0.5F,
                     low.y + pad),
              damage_meter::kLabelColor, tiles[rank].label);
    const std::string value = FormatDamageAmount(tiles[rank].value);
    PaintText(into, bold, base_px,
              ImVec2(low.x + (tile_width - TextWidth(bold, base_px, value.c_str())) * 0.5F,
                     low.y + pad + label_px + Px(1.0F)),
              Tint(view.look.text_color, 0.90F), value.c_str());
  }
  ImGui::Dummy(ImVec2(width, height));
}

// ── Le menu du clic droit ───────────────────────────────────────────────────
// ⚠ OUVERT HORS DE LA FENÊTRE, après ses styles : le fond sombre et les marges
// du compteur ne doivent pas déteindre sur un menu du jeu.
constexpr const char* kMenuScope = "compteur_degats_menu";
constexpr const char* kMenuId = "##menu";

void DrawMenu(const DamageMeterView& view, bool open_now, DamageMeterAction* asked) {
  ImGui::PushID(kMenuScope);
  if (open_now) {
    ImGui::OpenPopup(kMenuId);
  }
  if (BeginSkinPopup(kMenuId)) {
    if (ImGui::MenuItem(core::Tr("Palmarès des 10 plus gros coups"), nullptr, view.look.records)) {
      asked->records = !view.look.records;
    }
    if (ImGui::MenuItem(core::Tr("Verrouiller (figé et traversant)"), nullptr, view.look.locked)) {
      asked->lock = !view.look.locked;
    }
    ImGui::Separator();
    if (ImGui::MenuItem(core::Tr("Remettre le combat à zéro"))) {
      asked->reset_fight = true;
    }
    if (ImGui::MenuItem(core::Tr("Vider le palmarès"), nullptr, false, !view.records.empty())) {
      asked->forget_records = true;
    }
    ImGui::Separator();
    if (ImGui::MenuItem(core::Tr("Ranger dans la conversation"))) {
      asked->dock = DamageMeterDock{};
    }
    if (ImGui::MenuItem(core::Tr("Réglages du compteur…"))) {
      asked->open_settings = true;
    }
    if (ImGui::MenuItem(core::Tr("Fermer"))) {
      asked->close = true;
    }
    EndSkinPopup();
  }
  ImGui::PopID();
}

// La répartition de la hauteur. L'en-tête, les tuiles et le palmarès ont une
// hauteur DICTÉE par leur texte ; la courbe prend tout ce qui reste — c'est
// elle qu'on agrandit en tirant la fenêtre, et c'est bien elle qu'on veut plus
// grande.
//
// `fit_records` : DANS UN ONGLET, la place n'est pas la sienne — c'est celle
// de la conversation. La courbe garde alors son plancher, et le palmarès ne
// montre que les rangs qui tiennent dans le reste.
[[nodiscard]] Bands SplitHeight(const DamageMeterView& view, float available, float spacing,
                                bool fit_records) {
  const float base_px = ImGui::GetFontSize();
  Bands bands;
  bands.toolbar = damage_meter::ToolbarHeight();
  available -= bands.toolbar + spacing;
  bands.header = base_px * kBigTextScale + Px(kHeaderRoomPixels);
  if (view.pane == DamageMeterPane::kGroup) {
    bands.group = std::max(available - bands.header - spacing, Px(kFloorGraphPixels));
    return bands;
  }
  bands.stats = base_px * (1.0F + kLabelTextScale) + Px(kStatsRoomPixels);
  if (view.look.records) {
    bands.record_rows =
        fit_records ? damage_meter::RecordRowsFitting(available - bands.header - bands.stats -
                                                      Px(kLeastGraphPixels) - spacing * 3.0F)
                    : kDamageMeterRecordRows;
  }
  bands.records =
      bands.record_rows != 0 ? damage_meter::RecordsHeight(bands.record_rows) + spacing : 0.0F;
  // Les tuiles s'effacent quand la fenêtre devient trop basse pour les porter
  // SANS écraser la courbe : un compteur réduit à sa plus simple expression
  // garde son chiffre et sa forme, pas trois bilans sur un trait.
  bands.with_stats = available - bands.header - bands.records - bands.stats - spacing * 2.0F >=
                     Px(kLeastGraphPixels);
  bands.graph = std::max(available - bands.header - spacing - bands.records -
                             (bands.with_stats ? bands.stats + spacing : 0.0F),
                         Px(kFloorGraphPixels));
  return bands;
}

// LES BANDES SE POSENT À LA MAIN, à leur écart, depuis le curseur : l'espacement
// des items n'est pas poussé, pour ne pas déteindre sur les infobulles.
void DrawBands(const DamageMeterView& view, float width, const Bands& bands, float gap,
               DamageMeterAction* asked) {
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  float y = origin.y;
  const auto next_band = [&origin, &y, gap](float height) {
    ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
    y += height + gap;
  };
  next_band(bands.toolbar);
  damage_meter::DrawToolbar(view, width, asked);
  next_band(bands.header);
  DrawHeader(view, width, bands.header, asked);
  if (view.pane == DamageMeterPane::kGroup) {
    next_band(bands.group);
    damage_meter::DrawGroup(view, ImVec2(width, bands.group), asked);
    return;
  }
  next_band(bands.graph);
  DrawGraph(view, width, bands.graph);
  if (bands.with_stats) {
    next_band(bands.stats);
    DrawStats(view, width, bands.stats);
  }
  if (bands.record_rows != 0) {
    next_band(damage_meter::RecordsHeight(bands.record_rows));
    damage_meter::DrawRecords(view, width, bands.record_rows);
  }
}

// LE COMPTEUR EN ROUTE VERS UNE BANDE D'ONGLETS. Tant que sa barre de titre est
// tirée, un repère montre où il se rangerait ; lâché sur une bande, il le
// demande.
void FollowTitleDrag(DamageMeterState& state, const DamageMeterView& view,
                     const ChatDragBoard& board, DamageMeterAction* asked) {
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const bool over_title = mouse.x >= state.title_min.x && mouse.x < state.title_max.x &&
                          mouse.y >= state.title_min.y && mouse.y < state.title_max.y;
  if (over_title && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    state.title_dragging = true;
  }
  if (!state.title_dragging) {
    return;
  }
  const std::optional<ChatStripSpot> spot = ChatStripUnder(board, mouse);
  if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    if (spot.has_value()) {
      // ⚠ AU PREMIER PLAN : la fenêtre qu'on traîne couvre la bande.
      ImGui::GetForegroundDrawList()->AddLine(ImVec2(spot->marker_x, spot->top),
                                              ImVec2(spot->marker_x, spot->bottom),
                                              view.look.text_color, Px(kDockMarkerPixels));
      SkinTooltipFormat("%s", core::Tr("Lâcher pour ranger le compteur dans cette conversation"));
    }
    return;
  }
  state.title_dragging = false;
  if (spot.has_value()) {
    asked->dock = DamageMeterDock{.group = spot->group, .rank = spot->rank};
  }
}

}  // namespace

std::string GroupDamageDigits(std::int64_t amount) {
  // ⚠ PAR LE NON SIGNÉ : le plus petit entier n'a pas d'opposé.
  const bool negative = amount < 0;
  std::uint64_t left =
      negative ? static_cast<std::uint64_t>(-(amount + 1)) + 1 : static_cast<std::uint64_t>(amount);
  std::string digits;
  std::size_t written = 0;
  do {
    if (written != 0 && written % kDigitsPerGroup == 0) {
      digits.push_back(' ');
    }
    digits.push_back(static_cast<char>('0' + left % kDecimalBase));
    ++written;
    left /= kDecimalBase;
  } while (left != 0);
  if (negative) {
    digits.push_back('-');
  }
  return {digits.rbegin(), digits.rend()};
}

std::string FormatDamageAmount(std::int64_t amount) {
  amount = std::max<std::int64_t>(amount, 0);
  if (amount < kAbbreviateFrom) {
    return GroupDamageDigits(amount);
  }
  std::array<char, 32> text{};
  // ⚠ L'ARRONDI DÉCIDE DE L'UNITÉ, et non le montant brut : 999 950 arrondi au
  // dixième de millier ferait « 1000,0 k », qui est un million.
  const std::int64_t tenths = (amount + kTenthOfThousand / 2) / kTenthOfThousand;
  if (tenths < kMillion / kTenthOfThousand) {
    std::snprintf(text.data(), text.size(), core::Tr("%lld,%lld k"),
                  static_cast<long long>(tenths / kDecimalsOfThousand),
                  static_cast<long long>(tenths % kDecimalsOfThousand));
  } else {
    const std::int64_t hundredths = (amount + kHundredthOfMillion / 2) / kHundredthOfMillion;
    std::snprintf(text.data(), text.size(), core::Tr("%lld,%02lld M"),
                  static_cast<long long>(hundredths / kDecimalsOfMillion),
                  static_cast<long long>(hundredths % kDecimalsOfMillion));
  }
  return text.data();
}

DamageMeterAction DrawDamageMeter(DamageMeterState& state, const DamageMeterView& view,
                                  const ChatDragBoard* board) {
  DamageMeterAction asked;
  // ⚠ CACHÉ OU RANGÉ, IL GARDE CE QU'IL SAVAIT DU VOLET : basculé pendant ce
  // temps, le volet fera grandir ou rapetisser la fenêtre à sa réapparition.
  if (!view.look.shown || view.docked) {
    state.title_dragging = false;
    return asked;
  }

  // Le titre porte ce que le joueur a choisi d'y lire, même replié.
  std::string title(view.title.empty() ? std::string_view(DamageMeterModeName(view.mode))
                                       : view.title);
  title += kDamageMeterWindowId;

  // ── Le chrome, accordé à la couleur du joueur ──────────────────────────────
  //
  // ⚠ IL NE VAUT QUE POUR LE CADRE, et se dépile dès `Begin` : le fond, la barre
  // de titre, le liseré et les marges y sont peints. Gardé plus longtemps, il
  // déteindrait sur les infobulles du contenu.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Px(kWindowRoundingPixels));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(kPadXPixels), Px(kPadYPixels)));
  constexpr int kStyleVars = 3;
  const ImVec4 accent = ImGui::ColorConvertU32ToFloat4(view.look.text_color);
  const float opacity = view.look.background_opacity;
  const auto darker = [&accent](float times, float alpha) {
    return ImVec4(accent.x * times, accent.y * times, accent.z * times, alpha);
  };
  ImGui::PushStyleColor(ImGuiCol_WindowBg, kWindowBackground);
  ImGui::PushStyleColor(ImGuiCol_TitleBg, darker(kTitleDarkening, opacity));
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, darker(kActiveTitleDarkening, opacity));
  ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed,
                        darker(kTitleDarkening, opacity * kCollapsedTitleOpacity));
  ImGui::PushStyleColor(ImGuiCol_Border, darker(kBorderDarkening, opacity * kBorderOpacity));
  // Le texte du titre : blanc, quel que soit le skin — il se pose sur une barre
  // sombre.
  ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32_WHITE);
  constexpr int kStyleColors = 6;

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoFocusOnAppearing;
  if (view.look.locked) {
    flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    // TRAVERSANT, SAUF SUR LA BARRE DE TITRE, relevée à l'image précédente :
    // c'est là que restent la flèche de repli et le menu du clic droit.
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool over_title = mouse.x >= state.title_min.x && mouse.x < state.title_max.x &&
                            mouse.y >= state.title_min.y && mouse.y < state.title_max.y;
    if (!over_title) {
      flags |= ImGuiWindowFlags_NoInputs;
    }
  }
  const float gap = Px(kBandGapPixels);
  const float pane = damage_meter::RecordsHeight() + gap;
  ImGui::SetNextWindowSize(
      ImVec2(Px(kFirstWidthPixels), Px(kFirstHeightPixels) + damage_meter::ToolbarHeight() + gap +
                                        (view.look.records ? pane : 0.0F)),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowBgAlpha(opacity);
  if (state.place_at.has_value()) {
    ImGui::SetNextWindowPos(*state.place_at, ImGuiCond_Always);
    state.place_at.reset();
  }
  bool open = true;
  const bool expanded = ImGui::Begin(title.c_str(), view.look.locked ? nullptr : &open, flags);
  ImGui::PopStyleColor(kStyleColors);
  ImGui::PopStyleVar(kStyleVars);
  if (!open) {
    asked.close = true;
  }
  const ImVec2 window_at = ImGui::GetWindowPos();
  state.title_min = window_at;
  state.title_max =
      ImVec2(window_at.x + ImGui::GetWindowSize().x, window_at.y + ImGui::GetFrameHeight());
  // ⚠ VERROUILLÉ, IL NE SE DÉPLACE PAS : il ne se range pas non plus en glissant
  // — le menu le fait.
  if (board != nullptr && !view.look.locked) {
    FollowTitleDrag(state, view, *board, &asked);
  }

  // LE MENU S'OUVRE SUR UN CLIC DROIT COMMENCÉ ICI : un clic droit qui tourne
  // la caméra et se relâche par hasard sur le compteur n'est pas une demande.
  const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    state.right_press_here = true;
  }
  const bool open_menu =
      state.right_press_here && hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right);
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    state.right_press_here = false;
  }

  if (expanded) {
    // LE VOLET SE DÉPLIE EN GRANDISSANT LA FENÊTRE, et se replie en la
    // rapetissant : la courbe garde la hauteur que le joueur lui a donnée.
    //
    // ⚠ SEULEMENT DÉPLIÉE : une fenêtre repliée ne mesure que sa barre de
    // titre. Basculé pendant ce temps, le volet attend le dépliage.
    //
    // ⚠ ET LA HAUTEUR DISPONIBLE SUIT DANS LA MÊME IMAGE : la nouvelle taille
    // ne vaut qu'à la suivante, et la courbe s'écraserait une image durant.
    //
    // ⚠ ET SEULEMENT DANS LA VUE « MOI », la seule qui le peint : basculé
    // pendant la vue du groupe, il attend le retour.
    float grown = 0.0F;
    if (view.pane == DamageMeterPane::kSelf) {
      if (state.records_were_shown.has_value() && *state.records_were_shown != view.look.records) {
        grown = view.look.records ? pane : -pane;
        const ImVec2 size = ImGui::GetWindowSize();
        ImGui::SetWindowSize(ImVec2(size.x, std::max(size.y + grown, ImGui::GetFrameHeight())));
      }
      state.records_were_shown = view.look.records;
    }

    const float width = std::max(ImGui::GetContentRegionAvail().x, Px(kLeastWidthPixels));
    DrawBands(view, width, SplitHeight(view, ImGui::GetContentRegionAvail().y + grown, gap, false),
              gap, &asked);
  }
  ImGui::End();

  DrawMenu(view, open_menu, &asked);
  return asked;
}

void DrawDamageMeterBody(const DamageMeterView& view, ImVec2 size, DamageMeterAction* asked) {
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float gap = Px(kBandGapPixels);
  const float width = std::max(size.x, Px(kLeastWidthPixels));
  DrawBands(view, width, SplitHeight(view, size.y, gap, true), gap, asked);
  // La place entière est prise, quoi que les bandes aient laissé.
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(width, std::max(size.y, 1.0F)));
}

}  // namespace ro::ui
