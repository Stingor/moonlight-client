#include "ro/ui/entity_sample.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "ro/render/name_plate.h"
#include "ro/render/name_plate_layout.h"

#include "ui_color.h"

namespace ro::ui {
namespace {

// Le fond du cadre, et la ligne du sol sous les pieds.
constexpr ImU32 kSampleBackground = IM_COL32(24, 26, 32, 220);
constexpr ImU32 kSampleGround = IM_COL32(255, 255, 255, 40);
constexpr float kSampleRounding = 4.0F;
// La marge de la ligne du sol aux bords du cadre.
constexpr float kSampleGroundInset = 12.0F;
// Faute de vignette, la hauteur prêtée à l'entité : de quoi poser une barre
// au-dessus d'une tête qui n'est pas dessinée.
constexpr float kSampleStandInHeight = 80.0F;
// Ce que le cadre garde sous le nom, et sa plus grande hauteur : épinglé, il ne
// défile pas, et ce qu'il prend est ôté à la page.
constexpr float kSampleFloorMarginPixels = 12.0F;
constexpr float kLargestSampleHeightPixels = 520.0F;

// L'écart d'une ligne à la suivante, en part de la taille de la police.
constexpr float kLineStepShare = 1.15F;

// Hors mode réglage, les barres sont entamées, pour se lire, et la magie à
// moitié, pour qu'on distingue les deux.
constexpr std::int32_t kSampleMost = 10'000;
constexpr std::int32_t kSampleHealth = 7'000;
constexpr std::int32_t kSampleMagic = 4'500;
// Le rang des barres du cadre dans le mode réglage : celui d'une entité seule.
constexpr std::size_t kSampleTuningRank = 0;

// L'INCANTATION DU CADRE : trois secondes qui recommencent en mode réglage ;
// hors de lui, figée aux trois cinquièmes, pour se lire.
constexpr double kSampleCastSeconds = 3.0;
constexpr std::uint32_t kSampleCastMs = 3'000;
constexpr std::uint32_t kSampleCastStillMs = 1'800;
// Ce que l'étiquette de la barre prend au-dessus d'elle, en part de sa police.
constexpr float kCastCaptionShare = 1.4F;

// La composition coréenne, comme en jeu : c'est le service que l'installation
// déclare.
constexpr render::NamePlateLook kComposition = {.korean_service = true};

// L'ombre d'un nom : le texte quatre fois autour de lui, d'un pixel.
constexpr std::array<ImVec2, 4> kShadowSteps = {ImVec2(-1.0F, 0.0F), ImVec2(1.0F, 0.0F),
                                                ImVec2(0.0F, -1.0F), ImVec2(0.0F, 1.0F)};

// Les deux zones : le client en gris, le réglage en jaune, plus épais.
constexpr ImU32 kGamePickColor = IM_COL32(160, 160, 160, 110);
constexpr ImU32 kPickColor = IM_COL32(255, 210, 60, 230);
constexpr float kGamePickThickness = 1.0F;
constexpr float kPickThickness = 2.0F;

[[nodiscard]] const render::EntityNameLook* NameLookOf(const render::EntityNamesLook& names,
                                                       const EntitySampleShown& shown) {
  if (!shown.name.has_value() || !names.shown) {
    return nullptr;
  }
  const auto rank = static_cast<std::size_t>(*shown.name);
  if (rank >= names.kinds.size() || !names.kinds[rank].shown) {
    return nullptr;
  }
  return &names.kinds[rank];
}

// LA BANDE DES BARRES SOUS LES PIEDS, pieds posés à `feet_y` : celles que le
// cadre peint — l'espèce allumée, quel que soit « quand la montrer ».
[[nodiscard]] std::optional<ScreenBand> SampleBars(const EntitySample& sample,
                                                   const EntityBarsLook& bars,
                                                   const EntitySampleShown& shown, float feet_y) {
  if (!shown.bars.has_value() || !bars.shown) {
    return std::nullopt;
  }
  const auto rank = static_cast<std::size_t>(*shown.bars);
  if (rank >= bars.kinds.size() || !bars.kinds[rank].shown) {
    return std::nullopt;
  }
  EntityBarView what;
  what.feet = ImVec2(0.0F, feet_y);
  what.scale = sample.anchor_scale;
  what.health.known = true;
  what.magic.known = true;
  return EntityBarsBandUnderFeet(bars.kinds[rank], what);
}

// LE HAUT DU NOM, pieds posés à `feet_y` : sa place à lui, écartée des barres
// qu'il chevaucherait — la règle du jeu, `NameTopClearOfBars`.
[[nodiscard]] float SampleNameTop(const EntitySample& sample, const render::EntityNameLook& how,
                                  float line_count, float step, float feet_y,
                                  const std::optional<ScreenBand>& bars) {
  return NameTopClearOfBars(feet_y + how.offset_pixels * sample.anchor_scale, line_count * step,
                            bars);
}

// LA BARRE DE CAST, sur le haut de ce qui est peint — l'ancrage du jeu. ⚠
// `seconds` NÉGATIF : l'incantation se fige, hors mode réglage.
void PaintCast(ImDrawList& into, const EntitySamplePlace& place, const EntitySample& sample,
               const CastBarsLook& cast, EntityBarKind kind, double seconds) {
  CastBarView what;
  what.kind = kind;
  what.head = ImVec2(place.feet.x,
                     sample.sprite.usable() ? place.image_min.y + sample.body_min.y : place.head_y);
  what.total_ms = kSampleCastMs;
  what.elapsed_ms = seconds < 0.0
                        ? kSampleCastStillMs
                        : static_cast<std::uint32_t>(std::fmod(seconds, kSampleCastSeconds) /
                                                     kSampleCastSeconds * kSampleCastMs);
  what.skill_name = sample.cast_skill;
  if (ShowsCastBar(cast, what)) {
    PaintCastBar(into, what, cast);
  }
}

// LES DEUX ZONES QUI RÉPONDENT AU CLIC, autour de la boîte du corps.
void PaintPickZones(ImDrawList& into, const EntitySamplePlace& place, const EntitySample& sample,
                    const EntityPickZones& zones) {
  if (zones.floor_pixels <= 0.0F) {
    return;
  }
  const ImVec2 body_min(place.image_min.x + sample.body_min.x,
                        place.image_min.y + sample.body_min.y);
  const ImVec2 body_max(place.image_min.x + sample.body_max.x,
                        place.image_min.y + sample.body_max.y);
  const auto zone = [&](float floor, ImU32 color, float thickness) {
    const ScreenBand across = PickZoneSpan(body_min.x, body_max.x, floor);
    const ScreenBand down = PickZoneSpan(body_min.y, body_max.y, floor);
    into.AddRect(ImVec2(across.top, down.top), ImVec2(across.bottom, down.bottom), color, 0.0F,
                 ImDrawFlags_None, thickness);
  };
  if (zones.floor_pixels != zones.game_floor_pixels) {
    zone(zones.game_floor_pixels, kGamePickColor, kGamePickThickness);
  }
  zone(zones.floor_pixels, kPickColor, kPickThickness);
}

void PaintBars(ImDrawList& into, const EntitySamplePlace& place, const EntitySample& sample,
               const EntityBarsLook& bars, EntityBarKind kind, double seconds) {
  const auto rank = static_cast<std::size_t>(kind);
  if (!bars.shown || rank >= bars.kinds.size() || !bars.kinds[rank].shown) {
    return;
  }
  EntityBarView what;
  what.kind = kind;
  what.feet = place.feet;
  what.head_y = place.head_y;
  what.scale = sample.anchor_scale;
  if (bars.tuning) {
    what.health = TuningEntityGauge(seconds, kSampleTuningRank, false);
    what.magic = TuningEntityGauge(seconds, kSampleTuningRank, true);
  } else {
    what.health = EntityGauge{.known = true, .current = kSampleHealth, .most = kSampleMost};
    what.magic = EntityGauge{.known = true, .current = kSampleMagic, .most = kSampleMost};
  }
  PaintEntityBarStack(into, what, bars.kinds[rank]);
}

void PaintName(ImDrawList& into, const EntitySamplePlace& place, const EntitySample& sample,
               const render::EntityNamesLook& names, const render::EntityNameLook& how,
               const std::optional<ScreenBand>& bars) {
  ImFont* font = ImGui::GetFont();
  const std::vector<std::string> lines = EntitySampleNameLines(sample, how);
  if (font == nullptr || lines.empty()) {
    return;
  }
  const float pixels = std::round(
      std::clamp(names.font_pixels, render::kSmallestNamePlateFont, render::kLargestNamePlateFont));
  const float step = EntitySampleLineStep(pixels);
  float widest = 0.0F;
  for (const std::string& line : lines) {
    widest = std::max(widest, font->CalcTextSizeA(pixels, FLT_MAX, 0.0F, line.c_str()).x);
  }
  // LA MÊME PLACE QU'EN JEU : sous les pieds, de l'écart réglé, écartée des
  // barres qu'elle chevaucherait.
  const float top =
      SampleNameTop(sample, how, static_cast<float>(lines.size()), step, place.feet.y, bars);
  // L'EMBLÈME PREND SA PLACE À GAUCHE, comme en jeu : le widget s'élargit pour
  // lui, et toutes les lignes partent du même bord. ⚠ `app` A DÉJÀ APPLIQUÉ LES
  // CASES de guilde et de clan : une image présente est une image à montrer.
  const bool emblem = sample.emblem.usable();
  const render::NamePlateLayout placed = render::PlaceNamePlate(
      place.feet.x, top, widest, lines.size(), step,
      emblem ? render::NamePlateLead::kEmblem : render::NamePlateLead::kNone);
  if (emblem) {
    const ImVec2 at(placed.lead_min.x, placed.lead_min.y);
    into.AddImage(sample.emblem.id, at,
                  ImVec2(at.x + sample.emblem.width, at.y + sample.emblem.height), ImVec2(0, 0),
                  ImVec2(1, 1), ScaledAlpha(IM_COL32_WHITE, how.opacity));
  }
  const ImU32 color = ScaledAlpha(how.color, how.opacity);
  const ImU32 shadow = ScaledAlpha(how.shadow_color, how.opacity);
  float y = placed.text_top_y;
  for (const std::string& line : lines) {
    const ImVec2 at(placed.text_left_x, y);
    if (how.shadow) {
      for (const ImVec2 shift : kShadowSteps) {
        into.AddText(font, pixels, ImVec2(at.x + shift.x, at.y + shift.y), shadow, line.c_str());
      }
    }
    into.AddText(font, pixels, at, color, line.c_str());
    y += step;
  }
}

}  // namespace

EntitySamplePlace PlaceEntitySample(ImVec2 frame_min, ImVec2 frame_max, const EntitySample& sample,
                                    float floor_pixels) {
  EntitySamplePlace place;
  // ⚠ SUR LE PIXEL : la vignette se pose texel pour texel, et une barre entre
  // deux pixels se brouille.
  place.feet = ImVec2(std::round((frame_min.x + frame_max.x) * 0.5F),
                      std::round(frame_max.y - floor_pixels));
  place.image_min = ImVec2(place.feet.x - sample.feet.x, place.feet.y - sample.feet.y);
  place.image_max =
      ImVec2(place.image_min.x + sample.sprite.width, place.image_min.y + sample.sprite.height);
  place.head_y = sample.sprite.usable() ? place.image_min.y + sample.head_y
                                        : place.feet.y - kSampleStandInHeight;
  return place;
}

std::vector<std::string> EntitySampleNameLines(const EntitySample& sample,
                                               const render::EntityNameLook& look) {
  render::NamePlateText what;
  what.name = sample.name;
  // ⚠ CE QU'ON NE VEUT PAS VOIR SE VIDE AVANT LA COMPOSITION, comme en jeu :
  // une bannière tue fait tomber la seconde ligne entière.
  what.title = look.title ? std::string_view(sample.title) : std::string_view();
  what.aside = look.aside ? std::string_view(sample.aside) : std::string_view();
  what.banner = look.banner ? std::string_view(sample.banner) : std::string_view();
  // ⚠ L'INSIGNE SUIT LA BANNIÈRE : une bannière tue l'emporte avec elle.
  what.badge = look.banner ? std::string_view(sample.badge) : std::string_view();
  return render::ComposeNamePlateLines(what, kComposition);
}

float EntitySampleLineStep(float font_pixels) { return std::round(font_pixels * kLineStepShare); }

float EntitySampleFloor(const EntitySample& sample, const EntityBarsLook& bars,
                        const render::EntityNamesLook& names, const EntitySampleShown& shown) {
  // MESURÉ PIEDS À ZÉRO : ce qui descend le plus bas, barres ou nom.
  const std::optional<ScreenBand> band = SampleBars(sample, bars, shown, 0.0F);
  float below = band.has_value() ? band->bottom : 0.0F;
  if (const render::EntityNameLook* how = NameLookOf(names, shown); how != nullptr) {
    const auto lines = static_cast<float>(EntitySampleNameLines(sample, *how).size());
    const float step = EntitySampleLineStep(names.font_pixels);
    below = std::max(below, SampleNameTop(sample, *how, lines, step, 0.0F, band) + lines * step);
  }
  return std::max(kEntitySampleFloorPixels, below + kSampleFloorMarginPixels);
}

float EntitySampleHeadroom(const EntitySample& sample, const CastBarsLook& cast,
                           const EntitySampleShown& shown, float font_pixels) {
  if (!shown.bars.has_value() || !cast.replaced) {
    return kEntitySampleHeadroomPixels;
  }
  const auto rank = static_cast<std::size_t>(*shown.bars);
  if (rank >= cast.kinds.size() || !cast.kinds[rank].shown) {
    return kEntitySampleHeadroomPixels;
  }
  const CastBarLook& look = cast.kinds[rank];
  // CE QUI DÉPASSE DU HAUT DU SPRITE — coiffes comprises —, puis ce que la
  // barre monte encore, et son étiquette.
  const float crown =
      sample.sprite.usable() ? std::max(sample.head_y - sample.body_min.y, 0.0F) : 0.0F;
  const float caption = font_pixels * look.font_scale * kCastCaptionShare;
  return std::max(kEntitySampleHeadroomPixels,
                  crown + std::max(-look.offset_pixels, 0.0F) + caption);
}

float EntitySampleHeight(const EntitySample& sample, float floor_pixels, float headroom_pixels) {
  const float stature = std::max(
      sample.sprite.usable() ? std::max(sample.feet.y - sample.head_y, 0.0F) : kSampleStandInHeight,
      sample.reserved_stature);
  return std::clamp(stature + floor_pixels + headroom_pixels, kEntitySampleHeightPixels,
                    kLargestSampleHeightPixels);
}

void PaintEntitySample(ImDrawList& into, ImVec2 frame_min, ImVec2 frame_max,
                       const EntitySample& sample, const EntitySampleLooks& looks,
                       const EntitySampleShown& shown, const EntityPickZones& zones,
                       double seconds) {
  const EntityBarsLook& bars = looks.bars;
  const render::EntityNamesLook& names = looks.names;
  const EntitySamplePlace place = PlaceEntitySample(frame_min, frame_max, sample,
                                                    EntitySampleFloor(sample, bars, names, shown));
  into.AddRectFilled(frame_min, frame_max, kSampleBackground, kSampleRounding);
  into.AddLine(ImVec2(frame_min.x + kSampleGroundInset, place.feet.y),
               ImVec2(frame_max.x - kSampleGroundInset, place.feet.y), kSampleGround);
  if (sample.sprite.usable()) {
    into.AddImage(sample.sprite.id, place.image_min, place.image_max);
    PaintPickZones(into, place, sample, zones);
  }
  if (shown.bars.has_value()) {
    PaintBars(into, place, sample, bars, *shown.bars, seconds);
  }
  if (const render::EntityNameLook* how = NameLookOf(names, shown); how != nullptr) {
    PaintName(into, place, sample, names, *how, SampleBars(sample, bars, shown, place.feet.y));
  }
  if (shown.bars.has_value()) {
    PaintCast(into, place, sample, looks.cast, *shown.bars, bars.tuning ? seconds : -1.0);
  }
}

ScreenBand PickZoneSpan(float low, float high, float floor_pixels) {
  // L'ÉLARGISSEMENT DU CLIENT : le manque réparti des deux côtés.
  const float missing = std::max(floor_pixels - (high - low), 0.0F);
  return ScreenBand{.top = low - missing * 0.5F, .bottom = high + missing * 0.5F};
}

}  // namespace ro::ui
