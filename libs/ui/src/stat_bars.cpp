// Les sept jauges libres. Voir l'en-tête pour ce que chaque unité impose à son
// texte, et pour la teinte de surcharge.

#include "ro/ui/stat_bars.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/gauge_fill.h"
#include "ro/ui/inventory_window.h"

#include "item_grid.h"

namespace ro::ui {
namespace {

// De quoi écrire un nom court, deux nombres de soixante-quatre bits groupés et
// leur pour cent.
constexpr std::size_t kCaptionRoom = 112;

// Cent, pour une part en pour cent.
constexpr std::int64_t kPercent = 100;

// L'identité ImGui de chaque jauge. ⚠ STABLE ET UNIQUE : c'est elle qui tient la
// prise d'un geste d'une image à l'autre.
constexpr std::array<const char*, kStatBarCount> kBarIds = {
    "##stat_bar_health", "##stat_bar_magic", "##stat_bar_base_exp", "##stat_bar_job_exp",
    "##stat_bar_weight", "##stat_bar_zeny",  "##stat_bar_cast",
};

constexpr float kShadowShare = 0.08F;
constexpr float kLeastShadow = 1.0F;
constexpr ImU32 kCaptionShadow = IM_COL32(0, 0, 0, 190);
constexpr float kBorderThickness = 1.0F;

// Ce que la teinte de surcharge remplace.
constexpr ImU32 kOverweightFill = IM_COL32(200, 55, 55, 255);

// La disposition d'origine, en pixels d'écran.
constexpr float kDefaultBarLeft = 100.0F;
constexpr float kDefaultBarWidth = 220.0F;
constexpr float kDefaultBarHeight = 16.0F;

// Le tour complet et le quart de tour, écrits en clair plutôt que par `IM_PI` :
// une constante nommée dit ce qu'elle mesure, un produit par pi ne le dit pas.
constexpr float kQuarterTurn = 1.5707963F;
constexpr float kHalfTurn = 3.1415927F;
constexpr float kFullTurn = 6.2831853F;

// ── LE RELIEF, EN NOMBRES ───────────────────────────────────────────────────

// Ce qu'une teinte gagne vers la surface et perd vers le fond, en part du chemin
// vers le blanc et vers le noir.
constexpr float kSurfaceLift = 0.22F;
constexpr float kDepthShade = 0.38F;

// Le trait clair qui marque la surface du liquide, et le fil de lumière sur
// l'arête haute d'une barre.
constexpr ImU32 kSurfaceLine = IM_COL32(255, 255, 255, 90);
constexpr float kSurfaceLineThickness = 1.5F;
constexpr ImU32 kBarHighlight = IM_COL32(255, 255, 255, 45);

// L'ombre de profondeur d'une sphère : quelques anneaux de plus en plus sombres
// vers le tour, chacun épais d'une part du rayon.
constexpr int kDepthRings = 4;
constexpr float kDepthRingShare = 0.055F;
constexpr int kDeepestShadowAlpha = 110;

// Le bord de verre : un tour sombre, et un fil clair juste dedans.
constexpr float kOrbRimThickness = 2.0F;
constexpr ImU32 kOrbGlassEdge = IM_COL32(255, 255, 255, 40);

// Le reflet : une ellipse plate en haut du dôme, et une étincelle en haut à
// gauche — en parts du rayon.
constexpr ImU32 kOrbGloss = IM_COL32(255, 255, 255, 60);
constexpr float kGlossHeightShare = 0.55F;
constexpr float kGlossWidthShare = 0.55F;
constexpr float kGlossThicknessShare = 0.22F;
constexpr ImU32 kOrbSpark = IM_COL32(255, 255, 255, 95);
constexpr float kSparkOffsetShare = 0.42F;
constexpr float kSparkRadiusShare = 0.09F;

// Le biseau d'un anneau : un fil clair sur son bord extérieur, un fil sombre
// sur son bord intérieur.
constexpr ImU32 kRingOuterLight = IM_COL32(255, 255, 255, 55);
constexpr ImU32 kRingInnerShade = IM_COL32(0, 0, 0, 100);

// Le plus de points qu'un chemin à dégrader porte : un tour complet et ses
// deux bouts, ou un rectangle à quatre coins arrondis. Au-delà, le dégradé cède
// la place au remplissage uni plutôt que d'écrire hors du tampon.
constexpr int kMostPathPoints = 2 * kRoundSegments;

// Une teinte poussée vers le blanc (part positive) ou vers le noir (négative).
// La transparence ne bouge pas.
[[nodiscard]] ImU32 Shaded(ImU32 color, float toward_white) {
  const auto moved = [color, toward_white](int shift) {
    const float raw = static_cast<float>((color >> shift) & 0xFF);
    const float value =
        toward_white >= 0.0F ? raw + (255.0F - raw) * toward_white : raw * (1.0F + toward_white);
    return static_cast<int>(std::clamp(value, 0.0F, 255.0F) + 0.5F);
  };
  return IM_COL32(moved(IM_COL32_R_SHIFT), moved(IM_COL32_G_SHIFT), moved(IM_COL32_B_SHIFT),
                  (color >> IM_COL32_A_SHIFT) & 0xFF);
}

// Le mélange de deux teintes, canal par canal.
[[nodiscard]] ImU32 Mixed(ImU32 from, ImU32 to, float part) {
  const float held = std::clamp(part, 0.0F, 1.0F);
  const auto channel = [from, to, held](int shift) {
    const float start = static_cast<float>((from >> shift) & 0xFF);
    const float end = static_cast<float>((to >> shift) & 0xFF);
    return static_cast<int>(start + (end - start) * held + 0.5F);
  };
  return IM_COL32(channel(IM_COL32_R_SHIFT), channel(IM_COL32_G_SHIFT), channel(IM_COL32_B_SHIFT),
                  channel(IM_COL32_A_SHIFT));
}

// REMPLIT LE CHEMIN COURANT D'UN DÉGRADÉ VERTICAL, et le vide.
//
// 🔴 ImGui NE SAIT PAS DÉGRADER UN POLYGONE : `AddRectFilledMultiColor` ne
// connaît ni l'arrondi ni l'arc. On écrit donc les sommets soi-même, chacun avec
// sa teinte selon sa hauteur, en éventail depuis le premier — ce que
// `PathFillConvex` fait aussi, sans la teinte. ⚠ CONVEXE SEULEMENT, pour la même
// raison que lui.
//
// ⚠ ET LE LISSAGE DU BORD EST CELUI D'UN REMPLISSAGE UNI POSÉ DESSOUS, dans la
// teinte du fond : l'éventail n'en a pas, et son bord dentelé se verrait sur un
// cercle. Le liseré d'un pixel qui reste dans la teinte sombre passe pour de
// l'ombre.
void FillPathVertically(ImDrawList& into, ImU32 top, ImU32 bottom) {
  const int count = into._Path.Size;
  if (count < 3) {
    into.PathClear();
    return;
  }
  if (count > kMostPathPoints) {
    into.PathFillConvex(bottom);
    return;
  }
  std::array<ImVec2, kMostPathPoints> points{};
  std::copy_n(into._Path.Data, count, points.begin());
  into.PathFillConvex(bottom);

  float highest = points[0].y;
  float lowest = points[0].y;
  for (int rank = 1; rank < count; ++rank) {
    highest = std::min(highest, points[static_cast<std::size_t>(rank)].y);
    lowest = std::max(lowest, points[static_cast<std::size_t>(rank)].y);
  }
  const float span = std::max(lowest - highest, 1.0F);

  into.PrimReserve((count - 2) * 3, count);
  const ImVec2 white = into._Data->TexUvWhitePixel;
  const auto first = static_cast<ImDrawIdx>(into._VtxCurrentIdx);
  for (int rank = 0; rank < count; ++rank) {
    const ImVec2& point = points[static_cast<std::size_t>(rank)];
    into.PrimWriteVtx(point, white, Mixed(top, bottom, (point.y - highest) / span));
  }
  for (int rank = 2; rank < count; ++rank) {
    into.PrimWriteIdx(first);
    into.PrimWriteIdx(static_cast<ImDrawIdx>(first + rank - 1));
    into.PrimWriteIdx(static_cast<ImDrawIdx>(first + rank));
  }
}

// Le chemin courant, rempli uni ou en relief.
void FillPath(ImDrawList& into, ImU32 color, bool relief) {
  if (relief) {
    FillPathVertically(into, Shaded(color, kSurfaceLift), Shaded(color, -kDepthShade));
  } else {
    into.PathFillConvex(color);
  }
}

// La demi-largeur du disque à une hauteur donnée depuis son centre.
[[nodiscard]] float HalfChordAt(const OrbSurface& disc, float above) {
  return std::sqrt(std::max(0.0F, disc.radius * disc.radius - above * above));
}

// Le LIQUIDE d'un orbe : le disque coupé par une droite horizontale.
//
// 🔴 UN SEGMENT CIRCULAIRE EST CONVEXE, et c'est ce qui autorise un remplissage
// en éventail ici alors que le balayage horaire l'interdisait. Un disque coupé
// par une droite le reste quelle que soit la hauteur de la coupe ; un secteur de
// plus d'un demi-tour, non. Les deux se ressemblent, et la différence décide de
// la méthode.
void PaintOrbLiquid(ImDrawList& into, const OrbSurface& disc, float fill, ImU32 color,
                    bool relief) {
  if (fill <= 0.0F || disc.radius <= 0.0F) {
    return;
  }
  if (fill >= 1.0F) {
    into.PathArcTo(disc.middle, disc.radius, 0.0F, kFullTurn, kRoundSegments);
    FillPath(into, color, relief);
    return;
  }
  const float above = OrbLevelY(disc, fill) - disc.middle.y;
  const float half = HalfChordAt(disc, above);
  // ⚠ UN SEUL CALCUL POUR LES DEUX MOITIÉS : l'arc va du point de droite à celui
  // de gauche en passant par le BAS, que la coupe soit au-dessus ou au-dessous du
  // centre. Deux formules selon le côté seraient deux occasions de se tromper de
  // sens, et l'orbe se remplirait par le haut une fois sur deux.
  const float right = std::atan2(above, half);
  const float sweep = kHalfTurn - 2.0F * right;
  const auto used = static_cast<int>(static_cast<float>(kRoundSegments) * sweep / kFullTurn) + 2;
  into.PathArcTo(disc.middle, disc.radius, right, kHalfTurn - right, used);
  FillPath(into, color, relief);

  // LA SURFACE : un trait clair sur la corde. C'est lui qui fait lire un
  // NIVEAU, et non une teinte qui s'arrête.
  if (relief) {
    const float level = disc.middle.y + above;
    into.AddLine(ImVec2(disc.middle.x - half, level), ImVec2(disc.middle.x + half, level),
                 kSurfaceLine, kSurfaceLineThickness);
  }
}

// La profondeur d'une sphère : des anneaux concentriques de plus en plus
// sombres vers le tour. ⚠ CONCENTRIQUES, donc leurs bords adoucis se
// superposent en cercles et non en rayons — ce qui interdit les quadrilatères
// de l'anneau ne gêne pas ici.
//
// 🔴 ET LA PREMIÈRE BANDE SE POSE PAR SON BORD OPAQUE, non par son milieu. Un
// trait d'épaisseur `e` centré sur `R − e/2` semble border un disque de rayon R,
// mais son opacité s'arrête un demi-pixel plus tôt : l'ombre commençait donc
// APRÈS le bord du liquide, et le laissait vif. Voir `OpaqueOuterRadius`.
void PaintOrbDepth(ImDrawList& into, const OrbSurface& liquid) {
  for (int rank = 0; rank < kDepthRings; ++rank) {
    const OrbStroke band = OrbShadowStroke(liquid, rank);
    const int alpha = kDeepestShadowAlpha * (kDepthRings - rank) / kDepthRings;
    into.AddCircle(liquid.middle, band.center_radius, IM_COL32(0, 0, 0, alpha), kRoundSegments,
                   band.thickness);
  }
}

// Le reflet qui fait d'un disque un dôme : une ellipse plate en haut, et une
// étincelle en haut à gauche.
void PaintOrbGloss(ImDrawList& into, const OrbSurface& disc) {
  const ImVec2 dome(disc.middle.x, disc.middle.y - disc.radius * kGlossHeightShare);
  into.AddEllipseFilled(dome,
                        ImVec2(disc.radius * kGlossWidthShare, disc.radius * kGlossThicknessShare),
                        kOrbGloss, 0.0F, kRoundSegments);
  const ImVec2 spark(disc.middle.x - disc.radius * kSparkOffsetShare,
                     disc.middle.y - disc.radius * kSparkOffsetShare);
  into.AddCircleFilled(spark, disc.radius * kSparkRadiusShare, kOrbSpark, kRoundSegments);
}

// Le tour d'un orbe : le filet, rond ici, et le fil de verre en relief.
void PaintOrbRim(ImDrawList& into, const OrbSurface& disc, bool border, ImU32 border_color,
                 bool relief) {
  if (border) {
    const OrbStroke rim = OrbRimStroke(disc);
    into.AddCircle(disc.middle, rim.center_radius, border_color, kRoundSegments, rim.thickness);
  }
  if (relief) {
    into.AddCircle(disc.middle, disc.radius - kOrbRimThickness - kBorderThickness, kOrbGlassEdge,
                   kRoundSegments, kBorderThickness);
  }
}

// L'angle d'un pas d'anneau, depuis midi, dans le sens demandé.
[[nodiscard]] float RingAngleAt(int step, bool reversed) {
  const float turned = kFullTurn * static_cast<float>(step) / static_cast<float>(kRoundSegments);
  return -kQuarterTurn + (reversed ? -turned : turned);
}

// Le REMPLISSAGE d'un anneau, depuis midi.
//
// 🔴 UN ANNEAU N'EST JAMAIS CONVEXE — c'est un RUBAN, pas une surface pleine. Il
// se pose donc en quadrilatères, et l'adoucissement du remplissage se coupe le
// temps de la boucle : deux quadrilatères voisins poseraient chacun leur bord
// adouci au même endroit, et leur superposition dessinerait des rayons clairs.
// C'est la leçon du balayage horaire, payée une fois.
void PaintRingFill(ImDrawList& into, const OrbSurface& disc, float fill, float thickness,
                   ImU32 color, bool reversed) {
  const int used =
      static_cast<int>(static_cast<float>(kRoundSegments) * std::clamp(fill, 0.0F, 1.0F) + 0.5F);
  if (used <= 0 || disc.radius <= 0.0F) {
    return;
  }
  const float inner = disc.radius * (1.0F - std::clamp(thickness, kThinnestRing, kThickestRing));

  const ImDrawListFlags kept = into.Flags;
  into.Flags &= ~ImDrawListFlags_AntiAliasedFill;
  for (int step = 0; step < used; ++step) {
    const float from = RingAngleAt(step, reversed);
    const float to = RingAngleAt(step + 1, reversed);
    const ImVec2 outer_from(disc.middle.x + std::cos(from) * disc.radius,
                            disc.middle.y + std::sin(from) * disc.radius);
    const ImVec2 outer_to(disc.middle.x + std::cos(to) * disc.radius,
                          disc.middle.y + std::sin(to) * disc.radius);
    const ImVec2 inner_to(disc.middle.x + std::cos(to) * inner,
                          disc.middle.y + std::sin(to) * inner);
    const ImVec2 inner_from(disc.middle.x + std::cos(from) * inner,
                            disc.middle.y + std::sin(from) * inner);
    into.AddQuadFilled(inner_from, outer_from, outer_to, inner_to, color);
  }
  into.Flags = kept;
}

// Le biseau de la part remplie d'un anneau : un fil clair dehors, un fil sombre
// dedans, sur l'arc rempli seulement.
void PaintRingBevel(ImDrawList& into, const OrbSurface& disc, float fill, float thickness,
                    bool reversed) {
  const int used =
      static_cast<int>(static_cast<float>(kRoundSegments) * std::clamp(fill, 0.0F, 1.0F) + 0.5F);
  if (used <= 0 || disc.radius <= 0.0F) {
    return;
  }
  const float inner = disc.radius * (1.0F - std::clamp(thickness, kThinnestRing, kThickestRing));
  const float from = RingAngleAt(0, reversed);
  const float to = RingAngleAt(used, reversed);
  into.PathArcTo(disc.middle, disc.radius - kBorderThickness * 0.5F, from, to, used);
  into.PathStroke(kRingOuterLight, ImDrawFlags_None, kBorderThickness);
  into.PathArcTo(disc.middle, inner + kBorderThickness * 0.5F, from, to, used);
  into.PathStroke(kRingInnerShade, ImDrawFlags_None, kBorderThickness);
}

// Les deux bords d'un anneau, dans la teinte du filet.
void PaintRingBorder(ImDrawList& into, const OrbSurface& disc, float thickness, ImU32 color) {
  const float inner = disc.radius * (1.0F - std::clamp(thickness, kThinnestRing, kThickestRing));
  into.AddCircle(disc.middle, disc.radius - kBorderThickness * 0.5F, color, kRoundSegments,
                 kBorderThickness);
  into.AddCircle(disc.middle, inner + kBorderThickness * 0.5F, color, kRoundSegments,
                 kBorderThickness);
}

// Le sens de remplissage d'une barre. ⚠ L'ARRIÈRE DÉPEND DU SENS : une barre
// inversée part de la droite, une barre verticale du bas — inversée, du haut, ce
// qui est le seul cas où elle se lit comme une vidange, et c'est le joueur qui
// l'aura demandé.
[[nodiscard]] GaugeGrowth BarGrowth(bool vertical, bool reversed) {
  if (vertical) {
    return reversed ? GaugeGrowth::kDownward : GaugeGrowth::kUpward;
  }
  return reversed ? GaugeGrowth::kLeftward : GaugeGrowth::kRightward;
}

// Le remplissage d'une BARRE, dans l'intérieur de son cadre.
//
// ⚠ LA FORME VIENT DE `gauge_fill.h` : l'intérieur arrondi découpé par la part
// remplie. Le front reste une arête franche au milieu du creux, ne suit la
// courbe que là où elle le rattrape, et une part minuscule ne dépasse pas des
// coins.
void PaintBarFill(ImDrawList& into, const RoundedBox& interior, float fill, GaugeGrowth growth,
                  ImU32 color, bool relief) {
  if (!PathGaugeFill(into, interior, fill, growth)) {
    return;
  }
  FillPath(into, color, relief);

  // LE FIL DE LUMIÈRE SUR L'ARÊTE HAUTE : c'est lui qui fait lire un tube de
  // verre et non une bande de couleur. ⚠ RENTRÉ DE CE QUE LA COURBE MANGE à sa
  // hauteur, et borné à la part remplie : il ne doit dépasser ni l'arrondi ni
  // le front.
  if (relief) {
    const AxisBox box = GaugeFillBox(interior, fill, growth);
    const float line_y = box.min.y + kBorderThickness;
    const float inset = RoundedBoxInsetAt(interior, line_y - kBorderThickness * 0.5F);
    const float left = std::max(box.min.x, interior.min.x + inset);
    const float right = std::min(box.max.x, interior.max.x - inset);
    if (right > left && line_y < box.max.y) {
      into.AddLine(ImVec2(left, line_y), ImVec2(right, line_y), kBarHighlight, kBorderThickness);
    }
  }
}

// Texte centré, mesuré à la taille où il se dessine.
//
// ⚠ CHAQUE LIGNE CENTRÉE POUR ELLE-MÊME : ImGui aligne à gauche les lignes
// d'un bloc, et « (25 %) » partirait sous le début de « 300 / 1200 ».
void PaintCentered(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right, const char* text,
                   float pixels, ImU32 color) {
  ImFont* font = ImGui::GetFont();
  if (font == nullptr || text == nullptr || text[0] == '\0') {
    return;
  }
  const std::string_view whole(text);
  const float block_height = font->CalcTextSizeA(pixels, FLT_MAX, 0.0F, text).y;
  const float shadow = std::max(kLeastShadow, pixels * kShadowShare);
  float line_top = (top_left.y + bottom_right.y - block_height) * 0.5F;
  std::size_t begin = 0;
  while (begin <= whole.size()) {
    const std::size_t end = std::min(whole.find('\n', begin), whole.size());
    const char* first = whole.data() + begin;
    const char* last = whole.data() + end;
    const ImVec2 size = font->CalcTextSizeA(pixels, FLT_MAX, 0.0F, first, last);
    const ImVec2 at((top_left.x + bottom_right.x - size.x) * 0.5F, line_top);
    into.AddText(font, pixels, ImVec2(at.x + shadow, at.y + shadow), kCaptionShadow, first, last);
    into.AddText(font, pixels, at, color, first, last);
    line_top += pixels;
    begin = end + 1;
  }
}

// La part en pour cent, ou zéro quand il n'y a rien à diviser.
[[nodiscard]] std::int64_t PercentOf(const StatBarValue& value) {
  if (!value.known || value.most <= 0) {
    return 0;
  }
  return std::clamp<std::int64_t>(value.current * kPercent / value.most, 0, kPercent);
}

}  // namespace

void PaintStatusOrb(ImDrawList& into, const OrbSurface& disc, const OrbPaint& how) {
  // ⚠ LE FOND GARDE LE DISQUE PLEIN — c'est lui la silhouette —, le liquide
  // rentre d'un pixel dessous. Voir `OrbLiquidOf`.
  const OrbSurface liquid = OrbLiquidOf(disc);
  into.AddCircleFilled(disc.middle, disc.radius, how.background, kRoundSegments);
  PaintOrbLiquid(into, liquid, how.fill, how.ink, how.glossy);
  // ⚠ LE RELIEF PASSE PAR-DESSUS LE LIQUIDE, jamais dessous : un reflet sous la
  // teinte disparaîtrait dès que l'orbe se remplit, et la sphère redeviendrait
  // une pastille au moment où elle est pleine. Et l'ombre AVANT le reflet, pour
  // ne pas le ternir.
  if (how.glossy) {
    PaintOrbDepth(into, liquid);
    PaintOrbGloss(into, liquid);
  }
  PaintOrbRim(into, disc, how.border, how.border_color, how.glossy);
}

OrbStroke OrbRimStroke(const OrbSurface& disc) {
  return OrbStroke{.center_radius = disc.radius - kOrbRimThickness * 0.5F,
                   .thickness = kOrbRimThickness};
}

OrbStroke OrbShadowStroke(const OrbSurface& liquid, int rank) {
  const float step = liquid.radius * kDepthRingShare;
  // 🔴 POSÉE PAR SON BORD OPAQUE, NON PAR SON MILIEU. Centrée sur
  // `rayon − épaisseur / 2`, la première bande semble border le liquide : son
  // opacité s'arrête en fait un demi-pixel plus tôt, et c'est ce demi-pixel de
  // couleur vive qui faisait un liseré tout autour de l'orbe.
  return OrbStroke{.center_radius = liquid.radius - (step - kAntiAliasedEdge) * 0.5F -
                                    static_cast<float>(rank) * step,
                   .thickness = step};
}

OrbSurface OrbLiquidOf(const OrbSurface& disc) {
  OrbSurface liquid;
  liquid.middle = disc.middle;
  liquid.radius = std::max(0.0F, disc.radius - kOrbEdgeInset);
  return liquid;
}

OrbSurface OrbSurfaceOf(ImVec2 top_left, ImVec2 bottom_right) {
  OrbSurface disc;
  const float wide = bottom_right.x - top_left.x;
  const float tall = bottom_right.y - top_left.y;
  disc.middle = ImVec2((top_left.x + bottom_right.x) * 0.5F, (top_left.y + bottom_right.y) * 0.5F);
  // ⚠ LE PLUS PETIT DES DEUX CÔTÉS. Le prendre sur la largeur ferait déborder un
  // orbe d'un cadre plus haut que large, et la moitié du remplissage sortirait de
  // ce que le joueur a posé.
  disc.radius = std::max(0.0F, std::min(wide, tall) * 0.5F);
  return disc;
}

float OrbLevelY(const OrbSurface& disc, float fill) {
  const float part = std::clamp(fill, 0.0F, 1.0F);
  // Du bas du disque vers son sommet : à zéro le niveau est au bas, à un au
  // sommet. Jamais l'inverse, et jamais rapporté au CADRE — un cadre plus haut
  // que large laisserait alors du remplissage hors du disque.
  return disc.middle.y + disc.radius - 2.0F * disc.radius * part;
}

StatBarShape StatBarShapeOf(int stored) {
  if (stored < static_cast<int>(StatBarShape::kBar) ||
      stored > static_cast<int>(StatBarShape::kRing)) {
    return StatBarShape::kBar;
  }
  return static_cast<StatBarShape>(stored);
}

std::array<StatBarLook, kStatBarCount> DefaultStatBars() {
  // ⚠ L'ORDRE EST CELUI DE `StatBar`, et les ordonnées les empilent ainsi :
  // vie et magie en haut, les deux expériences dessous, la bourse et le poids en
  // bas. Elles se touchent de deux pixels, ce qui les rend saisissables en BLOC
  // au CTRL dès la première ouverture.
  std::array<StatBarLook, kStatBarCount> bars{};
  const auto place = [&bars](StatBar which, float y, ImU32 fill) {
    StatBarLook& look = bars[static_cast<std::size_t>(which)];
    look.rect.x = kDefaultBarLeft;
    look.rect.y = y;
    look.rect.width = kDefaultBarWidth;
    look.rect.height = kDefaultBarHeight;
    look.fill = fill;
  };
  place(StatBar::kHealth, 40.0F, IM_COL32(217, 69, 69, 255));
  place(StatBar::kMagic, 58.0F, IM_COL32(77, 158, 242, 255));
  place(StatBar::kBaseExp, 76.0F, IM_COL32(92, 199, 255, 255));
  place(StatBar::kJobExp, 94.0F, IM_COL32(255, 209, 77, 255));
  place(StatBar::kZeny, 112.0F, IM_COL32(250, 186, 51, 255));
  place(StatBar::kWeight, 130.0F, IM_COL32(166, 140, 204, 255));
  // Le bleu de la barre d'incantation, sous le poids.
  place(StatBar::kCast, 148.0F, IM_COL32(107, 173, 255, 255));
  return bars;
}

ImVec2 StatBarTextShift(const StatBarLook& look, ImVec2 size) {
  const auto bounded = [](float share) {
    return std::clamp(share, -kLargestTextShiftShare, kLargestTextShiftShare);
  };
  return ImVec2(bounded(look.text_shift_x) * size.x, bounded(look.text_shift_y) * size.y);
}

void CopyStatBarStyle(std::array<StatBarLook, kStatBarCount>& bars, std::size_t from) {
  if (from >= bars.size()) {
    return;
  }
  const StatBarLook& model = bars[from];
  for (StatBarLook& other : bars) {
    if (&other == &model) {
      continue;
    }
    // ⚠ CHAMP PAR CHAMP, ET NON `other = model`. Une affectation entière
    // emporterait la place, la visibilité, la forme, la couleur et le sens —
    // c'est-à-dire tout ce qui distingue une jauge d'une autre, et le geste
    // deviendrait « rendre les six identiques » au lieu de « leur donner la même
    // manière ».
    other.vertical = model.vertical;
    other.glossy = model.glossy;
    other.text = model.text;
    other.font_pixels = model.font_pixels;
    other.label = model.label;
    other.rounding = model.rounding;
    other.ring_thickness = model.ring_thickness;
    other.border = model.border;
    other.background = model.background;
    other.text_color = model.text_color;
    other.border_color = model.border_color;
  }
}

const char* StatBarShortName(StatBar bar) {
  // ⚠ UN AIGUILLAGE DE LITTÉRAUX, et non une table : `Tr` veut voir sa chaîne à
  // l'appel. HP, SP, Base et Job passent quand même par lui : ils ne changent
  // pas de langue, mais leur entrée les garde hors de la liste des manquants.
  switch (bar) {
    case StatBar::kHealth:
      return core::Tr("HP");
    case StatBar::kMagic:
      return core::Tr("SP");
    case StatBar::kBaseExp:
      return core::Tr("Base");
    case StatBar::kJobExp:
      return core::Tr("Job");
    case StatBar::kWeight:
      return core::Tr("Poids");
    case StatBar::kCast:
      return core::Tr("Cast");
    case StatBar::kZeny:
    default:
      return "";
  }
}

int ZenyReferenceOf(float asked) {
  // ⚠ LE `std::clamp` SE FAIT EN DOUBLE, ET C'EST LE POINT. Comparer puis
  // convertir en `int` ne suffirait pas : c'est la CONVERSION d'un flottant trop
  // grand qui est indéfinie, donc elle ne doit avoir lieu qu'une fois la valeur
  // ramenée dans la plage.
  const double held =
      std::clamp(static_cast<double>(asked), static_cast<double>(kSmallestZenyReference),
                 static_cast<double>(kLargestZenyReference));
  return static_cast<int>(held);
}

StatBarText StatBarTextOf(int stored) {
  if (stored < static_cast<int>(StatBarText::kNothing) ||
      stored > static_cast<int>(StatBarText::kBothStacked)) {
    return StatBarText::kPercent;
  }
  return static_cast<StatBarText>(stored);
}

std::string StatBarCaption(StatBar bar, const StatBarValue& value, StatBarText mode,
                           bool labelled) {
  if (mode == StatBarText::kNothing) {
    return {};
  }
  if (!value.known) {
    // ⚠ RIEN PLUTÔT QU'UN ZÉRO. Tant que le serveur n'a pas parlé, « 0 / 0 »
    // annoncerait une bourse vide ou une mort — les deux faux, et les deux
    // crédibles.
    return {};
  }
  // UN TEXTE COMPOSÉ PASSE DEVANT LE MODE, qui ne décide plus que s'il s'écrit.
  if (!value.composed.empty()) {
    return value.composed;
  }

  // 🔴 LA BOURSE N'A PAS DE FRACTION QUI VEUILLE DIRE QUELQUE CHOSE. Écrire
  // « 4 210 / 2 147 483 647 » est exact et illisible : ce qu'on veut savoir,
  // c'est COMBIEN on a. Elle ignore donc le mode et donne son montant groupé —
  // le même que le pied du sac, par le même assembleur.
  if (bar == StatBar::kZeny) {
    // ⚠ LE SUFFIXE EST CELUI DU SAC, et par le même foyer : deux « z » écrits à
    // deux endroits finiraient par diverger le jour où le serveur change d'unité.
    // Et il tient lieu de nom : « Zeny 1 234 z » le dirait deux fois.
    return GroupThousands(value.current) + std::string(kZenySuffix);
  }

  std::array<char, kCaptionRoom> written{};
  const std::int64_t part = PercentOf(value);
  // ⚠ LE POIDS SE STOCKE EN DIXIÈMES D'UNITÉ, comme chez le serveur : l'écrire
  // brut le multiplierait par dix sous les yeux du joueur.
  const std::int64_t divisor = bar == StatBar::kWeight ? kWeightTenths : 1;
  const long long now = static_cast<long long>(value.current / divisor);
  const long long full = static_cast<long long>(value.most / divisor);
  // Le nom court et son espace, ou rien du tout.
  const char* name = labelled ? StatBarShortName(bar) : "";
  const char* gap = (labelled && name[0] != '\0') ? " " : "";

  switch (mode) {
    case StatBarText::kPercent:
      std::snprintf(written.data(), written.size(), "%s%s%lld %%", name, gap,
                    static_cast<long long>(part));
      break;
    case StatBarText::kValues:
      std::snprintf(written.data(), written.size(), "%s%s%lld / %lld", name, gap, now, full);
      break;
    case StatBarText::kBothStacked:
      std::snprintf(written.data(), written.size(), "%s%s%lld / %lld\n(%lld %%)", name, gap, now,
                    full, static_cast<long long>(part));
      break;
    case StatBarText::kBoth:
    default:
      std::snprintf(written.data(), written.size(), "%s%s%lld / %lld (%lld %%)", name, gap, now,
                    full, static_cast<long long>(part));
      break;
  }
  return std::string(written.data());
}

float StatBarFill(const StatBarValue& value) {
  if (!value.known || value.most <= 0) {
    return 0.0F;
  }
  const float part = static_cast<float>(value.current) / static_cast<float>(value.most);
  return std::clamp(part, 0.0F, 1.0F);
}

bool StatBarOverweight(const StatBarValue& weight, const StatBarWarnings& warnings) {
  // ⚠ LE SEUIL VIENT DU SERVEUR, ET SON ABSENCE NE SE DEVINE PAS. Tant qu'il ne
  // l'a pas annoncé, on ne colore rien — un seuil codé en dur mentirait sur une
  // configuration qui ne l'emploie pas.
  if (!weight.known || weight.most <= 0 || warnings.overweight_percent <= 0) {
    return false;
  }
  return PercentOf(weight) >= warnings.overweight_percent;
}

bool StatBarPresent(StatBar bar, const StatBarValue& value, bool keep_maxed_exp) {
  const auto at_max_level = [&value, keep_maxed_exp](std::int64_t ceiling) {
    return !keep_maxed_exp && value.known && value.most == ceiling;
  };
  switch (bar) {
    case StatBar::kCast:
      return value.known;
    case StatBar::kBaseExp:
      return !at_max_level(kBaseExpCeiling);
    case StatBar::kJobExp:
      return !at_max_level(kJobExpCeiling);
    default:
      return true;
  }
}

void DrawStatBars(const StatBarValues& values, const StatBarWarnings& warnings,
                  const AlignGrid& grid, StatBarsLook& look, HudGrip& grip, bool* moved) {
  if (!look.shown) {
    return;
  }

  std::array<HudRect, kStatBarCount> rects{};
  std::array<bool, kStatBarCount> shown{};
  for (std::size_t rank = 0; rank < kStatBarCount; ++rank) {
    rects[rank] = look.bars[rank].rect;
    shown[rank] = look.bars[rank].shown &&
                  StatBarPresent(static_cast<StatBar>(rank), values[rank], look.keep_maxed_exp);
  }

  for (std::size_t rank = 0; rank < kStatBarCount; ++rank) {
    // ⚠ `shown` ET NON LE SEUL RÉGLAGE : une jauge absente de l'écran ne doit
    // pas non plus aimanter ses sœurs.
    if (!shown[rank]) {
      continue;
    }
    const auto bar = static_cast<StatBar>(rank);
    const StatBarValue& value = values[rank];
    // ⚠ L'APPARENCE EST CELLE DE CETTE JAUGE-LÀ. Seuls le verrou, l'aimantation
    // et le repère de pose viennent du lot : ce sont les trois faits du GESTE.
    const StatBarLook& mine = look.bars[rank];

    HudFrameLook frame;
    frame.locked = look.locked;
    // 🔴 LE CADRE NE PEINT NI FILET NI FOND ROND. Son filet passe AVANT le
    // contenu, et une jauge pleine et arrondie le recouvrait ; son fond est
    // carré, et se verrait aux quatre coins d'un orbe. Chaque forme peint donc
    // les siens, après son remplissage.
    frame.border = false;
    frame.rounding = mine.rounding;
    const StatBarShape shape = mine.shape;
    if (shape == StatBarShape::kBar) {
      frame.background = mine.background;
    } else {
      frame.background.reset();
    }
    frame.grid = &grid;
    frame.sticky = look.sticky;
    frame.center_mark = look.center_mark;

    HudSiblings siblings;
    siblings.frames = rects;
    siblings.shown = shown;
    siblings.mine = static_cast<int>(rank);

    // ⚠ C'EST `rects[rank]` QUI EST PASSÉ : le cadre écrit dedans, et dans celui
    // de ses frères en déplacement de bloc.
    if (BeginHudFrame(kBarIds[rank], rects[rank], frame, siblings, grip, moved)) {
      ImDrawList* into = ImGui::GetWindowDrawList();
      if (into != nullptr) {
        const ImVec2 top_left = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        const ImVec2 bottom_right(top_left.x + size.x, top_left.y + size.y);

        const float fill = StatBarFill(value);
        ImU32 ink = mine.fill;
        if (bar == StatBar::kWeight && look.warn_overweight && StatBarOverweight(value, warnings)) {
          ink = kOverweightFill;
        }
        const bool reversed = mine.reversed;

        switch (shape) {
          case StatBarShape::kBar:
            PaintBarFill(*into,
                         GaugeInterior(top_left, bottom_right, mine.rounding,
                                       mine.border ? kBorderThickness : 0.0F),
                         fill, BarGrowth(mine.vertical, reversed), ink, mine.glossy);
            if (mine.border) {
              into->AddRect(top_left, bottom_right, mine.border_color, mine.rounding,
                            ImDrawFlags_None, kBorderThickness);
            }
            break;
          case StatBarShape::kOrb:
            PaintStatusOrb(*into, OrbSurfaceOf(top_left, bottom_right),
                           OrbPaint{.fill = fill,
                                    .ink = ink,
                                    .background = mine.background,
                                    .border_color = mine.border_color,
                                    .glossy = mine.glossy,
                                    .border = mine.border});
            break;
          case StatBarShape::kRing: {
            const OrbSurface disc = OrbSurfaceOf(top_left, bottom_right);
            // Le tour complet en fond : c'est lui qui montre ce qui MANQUE.
            PaintRingFill(*into, disc, 1.0F, mine.ring_thickness, mine.background, reversed);
            PaintRingFill(*into, disc, fill, mine.ring_thickness, ink, reversed);
            if (mine.glossy) {
              PaintRingBevel(*into, disc, fill, mine.ring_thickness, reversed);
            }
            if (mine.border) {
              PaintRingBorder(*into, disc, mine.ring_thickness, mine.border_color);
            }
            break;
          }
        }

        const std::string caption = StatBarCaption(bar, value, mine.text, mine.label);
        if (!caption.empty()) {
          const ImVec2 shift = StatBarTextShift(mine, size);
          // ⚠ DÉCOUPÉ À L'ÉCRAN ET NON AU CADRE : un texte décalé sort de la jauge.
          into->PushClipRectFullScreen();
          PaintCentered(*into, ImVec2(top_left.x + shift.x, top_left.y + shift.y),
                        ImVec2(bottom_right.x + shift.x, bottom_right.y + shift.y), caption.c_str(),
                        mine.font_pixels, mine.text_color);
          into->PopClipRect();
        }
      }
    }
    EndHudFrame(grip);
  }

  for (std::size_t rank = 0; rank < kStatBarCount; ++rank) {
    look.bars[rank].rect = rects[rank];
  }
}

}  // namespace ro::ui
