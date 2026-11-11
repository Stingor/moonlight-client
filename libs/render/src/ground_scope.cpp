#include "ro/render/ground_scope.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ro::render {
namespace {

// LA PORTÉE DE ZONE de chaque compétence que la table nomme. Le côté du carré
// est cette portée plus deux.
struct ScopeRange {
  std::uint16_t skill_id = 0;
  std::int16_t range = 0;
};

// TRIÉE par compétence : les cas que le client écrit en dur, puis les douze
// classes de son registre des actions qui répondent autre chose que zéro.
constexpr std::array kScopeRanges = std::to_array<ScopeRange>({
    {11, 3},   {17, 5},   {21, 5},    {25, 3},    {47, 3},    {69, 3},    {70, 5},   {79, 7},
    {80, 3},   {83, 9},   {85, 11},   {86, 3},    {88, 3},    {89, 9},    {91, 5},   {92, 5},
    {110, 3},  {129, 3},  {130, 3},   {137, 3},   {140, 3},   {381, 3},   {478, 7},  {483, 3},
    {484, 5},  {708, 18}, {723, 9},   {731, 9},   {733, 7},   {736, -2},  {739, 7},  {769, 9},
    {770, 5},  {2005, 5}, {2008, 9},  {2032, 5},  {2044, 5},  {2213, 18}, {2237, 7}, {2260, 9},
    {2261, 3}, {2446, 9}, {2447, -2}, {2449, -2}, {2450, 7},  {2452, 7},  {2453, 7}, {2454, 7},
    {5004, 9}, {5006, 7}, {5013, 7},  {5028, 7},  {5042, 7},  {5217, 11}, {5218, 9}, {5222, 9},
    {5227, 7}, {5229, 7}, {5230, 7},  {5267, 9},  {5278, 19}, {5279, 7},  {5319, 9}, {5369, 9},
    {5370, 9}, {5371, 9}, {5372, 9},  {5373, 9},  {5409, 5},  {5410, 3},  {5411, 5}, {5412, 3},
    {5415, 9}, {5422, 7}, {5429, 9},  {5445, 9},  {5455, 7},  {5457, -2}, {5501, 7},
});

// Ce que le côté ajoute à la portée.
constexpr std::int32_t kSideOverRange = 2;

constexpr std::uint16_t kPoisonSmoke = 2032;
constexpr std::uint16_t kWarlockComet = 2213;
constexpr std::uint16_t kCompetentia = 5278;
constexpr std::string_view kMagicTarget = "effect\\magic_target.tga";
constexpr std::string_view kPoisonSmokeCircle =
    "effect\\new_poisonsmoke\\new_poisonsmoke_cast\\poi_magic_circle_1.tga";
constexpr std::string_view kPoisonSmokeCircleMinimal =
    "effect\\mineffect\\new_poisonsmoke\\new_poisonsmoke_cast\\poi_magic_circle_1.tga";
constexpr std::string_view kCometRune =
    "effect\\new_comet\\new_comet_cast_magicrune\\new_comet_cast_magicrune.tga";
constexpr std::string_view kCompetentiaRune =
    "effect\\competentia\\competentia_cast_bottom\\com_rune_01.tga";

constexpr float kAngleStepOverSide = 0.5F;
constexpr std::int32_t kAlphaFloor = 128;
constexpr float kAlphaSwing = 40.0F;
constexpr float kCompetentiaAlphaShare = 0.6F;
constexpr std::uint8_t kWhite = 255;

}  // namespace

std::int32_t GroundScopeSide(std::uint16_t skill_id) {
  const auto found = std::ranges::lower_bound(kScopeRanges, skill_id, {}, &ScopeRange::skill_id);
  const std::int32_t range =
      found != kScopeRanges.end() && found->skill_id == skill_id ? found->range : 0;
  return range + kSideOverRange;
}

std::string_view GroundScopeTexture(std::uint16_t skill_id, bool minimal_effects) {
  switch (skill_id) {
    case kPoisonSmoke:
      return minimal_effects ? kPoisonSmokeCircleMinimal : kPoisonSmokeCircle;
    case kWarlockComet:
      return kCometRune;
    case kCompetentia:
      return kCompetentiaRune;
    default:
      return kMagicTarget;
  }
}

float GroundScopeAngle(std::int32_t side, std::uint32_t ticks) {
  if (side <= 0) {
    return 0.0F;
  }
  // Un pas par tic, ajouté l'un après l'autre comme le client les ajoute.
  const float step = kAngleStepOverSide / static_cast<float>(side);
  float angle = 0.0F;
  for (std::uint32_t tick = 0; tick < ticks; ++tick) {
    angle += step;
  }
  return angle;
}

std::array<std::uint8_t, 4> GroundScopeColor(std::uint16_t skill_id, float angle) {
  // LA CONVERSION DU CLIENT SATURE : un sinus négatif ne retranche rien.
  const auto raised = static_cast<std::int32_t>(std::max(0.0F, std::sin(angle) * kAlphaSwing));
  std::int32_t alpha = kAlphaFloor + raised;
  if (skill_id == kCompetentia) {
    alpha = static_cast<std::int32_t>(static_cast<float>(alpha) * kCompetentiaAlphaShare);
  }
  return {kWhite, kWhite, kWhite, static_cast<std::uint8_t>(std::clamp(alpha, 0, 255))};
}

std::vector<EffectGroundCellPaint> ComposeGroundScopeCells(
    std::int32_t side, float angle, const std::array<std::uint8_t, 4>& color) {
  std::vector<EffectGroundCellPaint> cells;
  if (side <= 0) {
    return cells;
  }
  const float sine = std::sin(angle);
  const float cosine = std::cos(angle);
  const float centre = static_cast<float>(side) * 0.5F;
  const float size = static_cast<float>(side);
  const std::int32_t half = side / 2;
  cells.reserve(static_cast<std::size_t>(side) * static_cast<std::size_t>(side));
  for (std::int32_t row = 0; row < side; ++row) {
    for (std::int32_t column = 0; column < side; ++column) {
      EffectGroundCellPaint paint;
      paint.cell_offset_x = column - half;
      paint.cell_offset_y = row - half;
      paint.color = color;
      // Les quatre coins, abscisse croissante d'abord : l'écart au centre du
      // carré tourne, puis se ramène dans l'image.
      for (std::size_t corner = 0; corner < paint.texture_uv.size(); ++corner) {
        const float x = static_cast<float>(column + (corner % 2 == 1 ? 1 : 0)) - centre;
        const float y = static_cast<float>(row + (corner >= 2 ? 1 : 0)) - centre;
        paint.texture_uv[corner] = glm::vec2{(y * sine + x * cosine + centre) / size,
                                             (y * cosine - x * sine + centre) / size};
      }
      cells.push_back(paint);
    }
  }
  return cells;
}

}  // namespace ro::render
