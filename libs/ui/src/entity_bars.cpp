#include "ro/ui/entity_bars.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numbers>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/gauge_fill.h"

#include "ui_values.h"

namespace ro::ui {
namespace {

// L'ombre portée du texte : une part de sa hauteur, jamais moins d'un pixel.
constexpr float kShadowShare = 0.09F;
constexpr float kLeastShadow = 1.0F;
constexpr ImU32 kTextShadow = IM_COL32(0, 0, 0, 200);

// Ce qu'un mélange de couleurs fait d'un octet.
constexpr float kFullChannel = 255.0F;

[[nodiscard]] ImU32 Faded(ImU32 color, float opacity) {
  if (opacity >= 1.0F) {
    return color;
  }
  const auto alpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFFU);
  const auto faded = static_cast<ImU32>(std::clamp(alpha * opacity, 0.0F, kFullChannel));
  return (color & ~IM_COL32_A_MASK) | (faded << IM_COL32_A_SHIFT);
}

// Le mélange de deux couleurs, canal par canal. `part` à zéro rend la première.
[[nodiscard]] ImU32 Mixed(ImU32 from, ImU32 to, float part) {
  const float share = std::clamp(part, 0.0F, 1.0F);
  const auto channel = [from, to, share](int shift) {
    const auto left = static_cast<float>((from >> shift) & 0xFFU);
    const auto right = static_cast<float>((to >> shift) & 0xFFU);
    return static_cast<ImU32>(std::clamp(left + (right - left) * share, 0.0F, kFullChannel));
  };
  return (channel(IM_COL32_R_SHIFT) << IM_COL32_R_SHIFT) |
         (channel(IM_COL32_G_SHIFT) << IM_COL32_G_SHIFT) |
         (channel(IM_COL32_B_SHIFT) << IM_COL32_B_SHIFT) |
         (channel(IM_COL32_A_SHIFT) << IM_COL32_A_SHIFT);
}

// La part en pour cent, ou zéro quand il n'y a rien à diviser.
[[nodiscard]] std::int64_t PercentOf(const EntityGauge& gauge) {
  return GaugePercent(gauge.known, gauge.current, gauge.most);
}

// LE TEXTE D'UNE BARRE, CENTRÉ ET POSÉ SUR LE TEXEL.
//
// ⚠ `pixels` EST DÉJÀ ENTIER quand il vient de `EntityBarFontPixels` — c'est là
// que se joue la netteté des lettres. Ici on aligne ce qui reste : la PLACE.
void PaintCaption(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right, const char* text,
                  float pixels, ImU32 color) {
  ImFont* font = ImGui::GetFont();
  if (font == nullptr || text == nullptr || text[0] == '\0') {
    return;
  }
  // ⚠ SUR L'ENCRE, PAS SUR LA BOÎTE DE LIGNE, et aligné sur le pixel : voir
  // `EntityBarTextOrigin`. C'est aussi ce qui fait tomber juste l'OMBRE —
  // `ImFont::RenderText` tronque son point de départ à part, donc son écart au
  // texte vaudrait un pixel ou deux selon l'endroit du monde où se tient
  // l'entité, et elle se décollerait pendant qu'un monstre marche.
  const ImVec2 at = EntityBarTextOrigin(*font, pixels, top_left, bottom_right, text);
  // ⚠ ET L'OMBRE SE COMPTE EN PIXELS ENTIERS, pour la même raison : une part de
  // la hauteur donne 1,17 px à treize, ce qui n'est ni un ni deux.
  const float shadow = std::max(kLeastShadow, std::round(pixels * kShadowShare));
  into.AddText(font, pixels, ImVec2(at.x + shadow, at.y + shadow), kTextShadow, text);
  into.AddText(font, pixels, at, color, text);
}

// LES DEUX BARRES D'UNE ENTITÉ, du point d'ancrage vers le bas ou vers le haut.
//
// 🔴 LE SENS D'EMPILEMENT SUIT LE CÔTÉ. Sous les pieds, la vie est en haut et la
// magie dessous — l'ordre du client. Au-dessus de la tête, la vie reste la plus
// proche du sprite : c'est elle qu'on lit en visant, et la mettre au-dessus de
// la magie l'éloignerait de ce qu'elle décrit.
void PaintEntityBars(ImDrawList& into, const EntityBarView& what, const EntityBarLook& look) {
  // 🔴 L'ÉCHELLE NE TOUCHE QUE L'ÉCART, ET C'EST LA MÉTHODE DU CLIENT. Sa jauge
  // est un widget de soixante sur cinq PIXELS que rien n'agrandit ; seule son
  // ordonnée passe par une échelle. Voir `ScreenAnchorScale`, qui la porte.
  // 🔴 TOUT SUR LE PIXEL ENTIER : bords, largeur, hauteurs, écart. L'écart se
  // multiplie par une échelle qui suit le zoom, et un bord tombé entre deux
  // pixels se peint en fondu sur les deux — la barre paraissait floue à
  // certains crans de zoom, et nette aux autres.
  const float width = std::round(std::max(look.width_pixels, kSmallestEntityBarWidth));
  const float health_height = std::round(std::max(look.height_pixels, kSmallestEntityBarHeight));
  const float magic_height =
      std::round(std::max(look.magic_height_pixels, kSmallestEntityBarHeight));
  const float gap = std::round(std::max(look.gap_pixels, 0.0F));
  const float left = std::round(what.feet.x - width * 0.5F);
  const float right = left + width;

  const bool shows_health = look.health && what.health.known;
  const bool shows_magic = look.magic && what.magic.known;

  // Le haut de la PREMIÈRE barre. Sous les pieds, on descend ; au-dessus de la
  // tête, on remonte de toute la pile pour que le bas affleure le sprite.
  float top = 0.0F;
  const float offset = look.offset_pixels * what.scale;
  if (look.under_feet) {
    top = std::round(what.feet.y + offset);
  } else {
    top = std::round(what.head_y - offset - EntityBarsHeight(look, what));
  }

  if (shows_health) {
    const ImVec2 top_left(left, top);
    const ImVec2 bottom_right(right, top + health_height);
    const float fill = EntityGaugeFill(what.health);
    PaintEntityBar(into, top_left, bottom_right, fill, EntityBarFillColor(look, fill), look);
    const std::string caption = EntityBarCaption(what.health, look.text);
    if (!caption.empty()) {
      PaintCaption(into, top_left, bottom_right, caption.c_str(), EntityBarFontPixels(look),
                   Faded(look.text_color, std::clamp(look.opacity, 0.0F, 1.0F)));
    }
    top += health_height + gap;
  }
  if (shows_magic) {
    const ImVec2 top_left(left, top);
    const ImVec2 bottom_right(right, top + magic_height);
    PaintEntityBar(into, top_left, bottom_right, EntityGaugeFill(what.magic), look.magic_fill,
                   look);
  }
}

}  // namespace

void PaintEntityBar(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right, float fill, ImU32 ink,
                    const EntityBarLook& look) {
  if (bottom_right.x <= top_left.x || bottom_right.y <= top_left.y) {
    return;
  }
  const float opacity = std::clamp(look.opacity, 0.0F, 1.0F);
  const float rounding = std::max(look.rounding, 0.0F);

  // LE CREUX SUR TOUTE LA SURFACE, puis la part pleine, puis le cadre EN TRAIT.
  // ⚠ L'ORDRE EST LE SUJET : voir l'en-tête. Un cadre PLEIN posé dessous ferait
  // du creux un rectangle plus bas d'un pixel par côté, et ImGui bornant tout
  // rayon à `hauteur / 2 - 1`, son arrondi tomberait. En trait par-dessus, le
  // creux garde toute la hauteur de la barre.
  into.AddRectFilled(top_left, bottom_right, Faded(look.background, opacity), rounding);

  // LA PART PLEINE, DANS L'INTÉRIEUR DU CADRE, bordure déduite : voir
  // `gauge_fill.h`. Son rayon est celui du creux moins la bordure, sur le même
  // centre, et une part minuscule suit la courbe du coin au lieu d'en dépasser.
  const RoundedBox interior =
      GaugeInterior(top_left, bottom_right, rounding, look.border ? kEntityBarBorderPixels : 0.0F);
  PaintGaugeFill(into, interior, fill, GaugeGrowth::kRightward, Faded(ink, opacity));

  if (look.border) {
    into.AddRect(top_left, bottom_right, Faded(look.border_color, opacity), rounding,
                 ImDrawFlags_RoundCornersAll, kEntityBarBorderPixels);
  }
}

EntityBarWhen EntityBarWhenOf(int stored) {
  if (stored <= 0 || stored >= static_cast<int>(EntityBarWhen::kCount)) {
    return EntityBarWhen::kAlways;
  }
  return static_cast<EntityBarWhen>(stored);
}

EntityBarText EntityBarTextOf(int stored) {
  if (stored <= 0 || stored >= static_cast<int>(EntityBarText::kCount)) {
    return EntityBarText::kNothing;
  }
  return static_cast<EntityBarText>(stored);
}

std::array<EntityBarLook, kEntityBarKindCount> DefaultEntityBars() {
  std::array<EntityBarLook, kEntityBarKindCount> bars{};

  // MOI : éteinte. Mes jauges libres et ma fiche disent déjà tout, et une barre
  // sous mes propres pieds masque la case où je vais cliquer.
  EntityBarLook& own = bars[static_cast<std::size_t>(EntityBarKind::kOwn)];
  own.shown = false;
  own.when = EntityBarWhen::kAlways;
  own.magic = true;

  // MON GROUPE : montrée même intacte. C'est à cela qu'elle sert — voir qu'un
  // allié va tomber avant qu'il ne tombe.
  EntityBarLook& party = bars[static_cast<std::size_t>(EntityBarKind::kParty)];
  party.when = EntityBarWhen::kAlways;

  // UN JOUEUR QUELCONQUE : sa vie n'arrive que s'il est la cible, alors la
  // barre ne se montre que là. Ailleurs, elle ne viendrait jamais et le réglage
  // mentirait.
  EntityBarLook& player = bars[static_cast<std::size_t>(EntityBarKind::kPlayer)];
  player.when = EntityBarWhen::kHoveredOrTarget;

  // UN BOSS : plus large, plus haute, et elle écrit sa part. On la suit
  // longtemps, et de loin.
  EntityBarLook& boss = bars[static_cast<std::size_t>(EntityBarKind::kBoss)];
  boss.width_pixels = 96.0F;
  boss.height_pixels = 8.0F;
  boss.text = EntityBarText::kPercent;

  // TOUT LE RESTE : même réserve que pour un joueur, et pour la même raison.
  EntityBarLook& other = bars[static_cast<std::size_t>(EntityBarKind::kOther)];
  other.when = EntityBarWhen::kHoveredOrTarget;

  return bars;
}

bool ShowsEntityBar(const EntityBarLook& look, const EntityBarView& what) {
  if (!look.shown) {
    return false;
  }
  if (what.dead && look.hide_dead) {
    return false;
  }
  // 🔴 CE QUI SE MONTRERAIT DOIT EXISTER. Une espèce dont les deux barres sont
  // éteintes, ou dont rien n'est connu, ne dessine pas un cadre vide : c'est la
  // différence entre « il n'a plus rien » et « on ne me dit rien ».
  const bool has_health = look.health && what.health.known;
  const bool has_magic = look.magic && what.magic.known;
  if (!has_health && !has_magic) {
    return false;
  }
  switch (look.when) {
    case EntityBarWhen::kAlways:
      return true;
    case EntityBarWhen::kDamaged:
      // ⚠ LA VIE SEULE DÉCIDE DE L'ENTAME. Une magie entamée sur un porteur
      // intact ne fait pas paraître sa barre de vie : le seuil parle de ce qu'on
      // lui a fait, pas de ce qu'il a dépensé.
      return has_health && what.health.current < what.health.most;
    case EntityBarWhen::kHovered:
      return what.hovered;
    case EntityBarWhen::kTarget:
      return what.targeted;
    case EntityBarWhen::kHoveredOrTarget:
      return what.hovered || what.targeted;
    case EntityBarWhen::kCount:
      break;
  }
  return false;
}

float EntityGaugeFill(const EntityGauge& gauge) {
  if (!gauge.known || gauge.most <= 0) {
    return 0.0F;
  }
  return std::clamp(static_cast<float>(gauge.current) / static_cast<float>(gauge.most), 0.0F, 1.0F);
}

ImU32 EntityBarFillColor(const EntityBarLook& look, float fill) {
  const float share = std::clamp(fill, 0.0F, 1.0F);
  const float threshold = std::clamp(look.low_share, 0.0F, 1.0F);
  if (!look.gradient) {
    // La bascule du client : la couleur pleine à partir du quart, l'alerte
    // en dessous.
    return share >= threshold ? look.fill : look.low_fill;
  }
  // ⚠ LE DÉGRADÉ SE MESURE CONTRE LE SEUIL, et non contre le plein : au-dessus
  // du seuil la barre garde sa couleur, en dessous elle glisse vers l'alerte.
  // Un dégradé étalé sur toute la course rendrait un vert jaunâtre dès la
  // première égratignure, ce qui se lit comme une alerte qui n'en est pas une.
  if (share >= threshold || threshold <= 0.0F) {
    return look.fill;
  }
  return Mixed(look.low_fill, look.fill, share / threshold);
}

std::string EntityBarCaption(const EntityGauge& gauge, EntityBarText mode) {
  if (!gauge.known || mode == EntityBarText::kNothing) {
    return {};
  }
  if (gauge.in_percent) {
    mode = EntityBarText::kPercent;
  }
  std::array<char, 64> line{};
  switch (mode) {
    case EntityBarText::kPercent:
      std::snprintf(line.data(), line.size(), "%lld%%", static_cast<long long>(PercentOf(gauge)));
      break;
    case EntityBarText::kValues:
      std::snprintf(line.data(), line.size(), "%lld / %lld", static_cast<long long>(gauge.current),
                    static_cast<long long>(gauge.most));
      break;
    case EntityBarText::kBoth:
      std::snprintf(line.data(), line.size(), "%lld / %lld  (%lld%%)",
                    static_cast<long long>(gauge.current), static_cast<long long>(gauge.most),
                    static_cast<long long>(PercentOf(gauge)));
      break;
    case EntityBarText::kNothing:
    case EntityBarText::kCount:
      return {};
  }
  return std::string(line.data());
}

ImVec2 EntityBarTextOrigin(ImFont& font, float pixels, ImVec2 top_left, ImVec2 bottom_right,
                           const char* text) {
  const ImVec2 line = font.CalcTextSizeA(pixels, FLT_MAX, 0.0F, text);
  const ImVec2 middle((top_left.x + bottom_right.x) * 0.5F, (top_left.y + bottom_right.y) * 0.5F);

  // Le repli, si l'on ne peut rien demander aux glyphes : la boîte de ligne.
  float ink_top = 0.0F;
  float ink_bottom = line.y;

  ImFontBaked* baked = font.GetFontBaked(pixels);
  if (baked != nullptr && text != nullptr) {
    const char* stop = text + std::strlen(text);
    bool found = false;
    // ⚠ DÉCODÉ EN UTF-8, et non parcouru octet par octet : un octet de
    // continuation pris pour un caractère demanderait le glyphe de remplacement,
    // dont la boîte n'a rien à voir avec celle du texte qu'on écrit.
    for (const char* letter = text; letter < stop;) {
      unsigned int codepoint = 0;
      const int eaten = ImTextCharFromUtf8(&codepoint, letter, stop);
      letter += eaten > 0 ? eaten : 1;
      const ImFontGlyph* glyph = baked->FindGlyph(static_cast<ImWchar>(codepoint));
      if (glyph == nullptr || glyph->Visible == 0) {
        continue;  // une espace ne porte pas d'encre
      }
      ink_top = found ? std::min(ink_top, glyph->Y0) : glyph->Y0;
      ink_bottom = found ? std::max(ink_bottom, glyph->Y1) : glyph->Y1;
      found = true;
    }
  }

  return ImVec2(std::floor(middle.x - line.x * 0.5F),
                std::floor(middle.y - (ink_top + ink_bottom) * 0.5F));
}

float EntityBarFontPixels(const EntityBarLook& look) {
  return std::max(kSmallestBakedFontPixels, std::round(look.font_pixels));
}

float EntityBarsHeight(const EntityBarLook& look, const EntityBarView& what) {
  // ⚠ EN PIXELS, COMME LE DESSIN : cette hauteur sert à poser ce qui vient
  // au-dessus et en dessous — le nom, l'enseigne —, et la barre ne s'agrandit
  // plus avec le zoom. Voir `ScreenAnchorScale`.
  const bool shows_health = look.health && what.health.known;
  const bool shows_magic = look.magic && what.magic.known;
  // ⚠ ARRONDIE COMME LE DESSIN : voir `PaintEntityBars`.
  float height = 0.0F;
  if (shows_health) {
    height += std::round(std::max(look.height_pixels, kSmallestEntityBarHeight));
  }
  if (shows_magic) {
    if (height > 0.0F) {
      height += std::round(std::max(look.gap_pixels, 0.0F));
    }
    height += std::round(std::max(look.magic_height_pixels, kSmallestEntityBarHeight));
  }
  return height;
}

void DrawEntityBars(std::span<const EntityBarView> bars, const EntityBarsLook& look) {
  if (!look.shown) {
    return;
  }
  ImDrawList* into = ImGui::GetBackgroundDrawList();
  if (into == nullptr) {
    return;
  }
  for (const EntityBarView& what : bars) {
    const auto rank = static_cast<std::size_t>(what.kind);
    if (rank >= look.kinds.size()) {
      continue;
    }
    const EntityBarLook& how = look.kinds[rank];
    if (!ShowsEntityBar(how, what)) {
      continue;
    }
    PaintEntityBars(*into, what, how);
  }
}

namespace {

// Les valeurs du mode réglage : une jauge de dix mille, pour qu'un pour cent
// arrondi ne saute pas d'un cran à l'autre.
constexpr std::int32_t kTuningMost = 10'000;

// Les périodes du mode réglage, en secondes, et leur décalage par rang. ⚠ DES
// PÉRIODES QUI NE SE DIVISENT PAS : deux barres qui battraient ensemble se
// liraient comme une seule.
constexpr double kTuningHealthPeriod = 4.0;
constexpr double kTuningMagicPeriod = 5.3;
constexpr double kTuningPeriodStep = 0.37;
constexpr double kTuningPhaseStep = 0.13;
constexpr double kTuningMagicPhase = 0.5;

}  // namespace

void PaintEntityBarStack(ImDrawList& into, const EntityBarView& what, const EntityBarLook& look) {
  PaintEntityBars(into, what, look);
}

std::optional<ScreenBand> EntityBarsBandUnderFeet(const EntityBarLook& look,
                                                  const EntityBarView& what) {
  if (!look.under_feet) {
    return std::nullopt;
  }
  // LA MÊME PLACE QUE LE DESSIN : l'écart s'échelonne, la pile non. Voir
  // `PaintEntityBars`.
  ScreenBand band;
  band.top = std::round(what.feet.y + look.offset_pixels * what.scale);
  band.bottom = band.top + EntityBarsHeight(look, what);
  if (band.bottom <= band.top) {
    return std::nullopt;
  }
  return band;
}

float NameTopClearOfBars(float name_top, float name_height, const std::optional<ScreenBand>& bars) {
  if (!bars.has_value() || bars->top >= name_top + name_height || bars->bottom <= name_top) {
    return name_top;
  }
  return bars->bottom + kNameBarGapPixels;
}

EntityGauge TuningEntityGauge(double seconds, std::size_t rank, bool magic) {
  const auto step = static_cast<double>(rank);
  const double period =
      (magic ? kTuningMagicPeriod : kTuningHealthPeriod) + kTuningPeriodStep * step;
  const double phase = kTuningPhaseStep * step + (magic ? kTuningMagicPhase : 0.0);
  // ⚠ UN COSINUS QUI TOUCHE LES DEUX BOUTS : c'est au vide et au plein qu'un
  // arrondi, un seuil d'alerte et un texte se jugent.
  const double share = (1.0 - std::cos(2.0 * std::numbers::pi * (seconds / period + phase))) * 0.5;
  return EntityGauge{.known = true,
                     .current = static_cast<std::int32_t>(std::lround(share * kTuningMost)),
                     .most = kTuningMost};
}

const char* EntityBarKindName(EntityBarKind kind) {
  // ⚠ UN AIGUILLAGE DE LITTÉRAUX, et non une table : `Tr` veut voir sa chaîne à
  // l'appel, et le pointeur qu'il rend meurt au changement de langue.
  switch (kind) {
    case EntityBarKind::kOwn:
      return core::Tr("Moi");
    case EntityBarKind::kParty:
      return core::Tr("Groupe");
    case EntityBarKind::kPlayer:
      return core::Tr("Joueurs");
    case EntityBarKind::kMonster:
      return core::Tr("Monstres");
    case EntityBarKind::kBoss:
      return core::Tr("Boss");
    case EntityBarKind::kOther:
      return core::Tr("Autres");
  }
  return "";
}

}  // namespace ro::ui
