#include "ro/render/skill_named_recipe.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node.h"

namespace ro::render {
namespace {

// Le π du client, écrit en float à sept chiffres : c'est lui qui convertit ses
// angles, et un π plus juste décalerait les bords des secteurs.
constexpr float kClientPi = 3.141592F;
constexpr float kHalfTurnDegrees = 180.0F;
constexpr float kFullTurnDegrees = 360.0F;
constexpr std::int32_t kFullTurn = 360;

// Un centième, en float : le client multiplie ses centièmes tirés par lui.
constexpr float kHundredth = 0.01F;

// Les huit directions d'un sprite ; la moitié qui commence à la cinquième se
// dessine tournée vers le haut ou la droite de l'écran.
constexpr std::size_t kSpriteDirections = 8;
constexpr std::size_t kFirstAwayDirection = 4;

// Les deux trous qu'un nom peut porter : `{}` pour ce qui se remplace par un
// mot, `%d` ou `%02d` pour ce qui se remplace par un nombre.
constexpr std::string_view kWordHole = "{}";
constexpr std::string_view kNumberHole = "%d";
constexpr std::string_view kPaddedNumberHole = "%02d";

// ── L'ALLURE ────────────────────────────────────────────────────────────────
constexpr float kTwoThirdsOfHitStepMs = 16.0F;
constexpr float kFiftyMsPerCadence = 50.0F;
constexpr float kThirtyMs = 30.0F;
constexpr float kHundredMsPerCadence = 100.0F;
constexpr float kHundredMs = 100.0F;
constexpr std::int32_t kServantDelayWithoutSourceMs = 500;

// ── LA LANCE QUI CHOISIT SON QUART ───────────────────────────────────────────
// Les quatre secteurs de l'angle à l'écran, bornes hautes comprises : le
// premier et le troisième sont VERTICAUX, le deuxième et le quatrième
// HORIZONTAUX. Le quatrième enjambe le zéro.
constexpr float kQuarterOneLow = 60.0F;
constexpr float kQuarterOneHigh = 120.0F;
constexpr float kQuarterTwoHigh = 240.0F;
constexpr float kQuarterThreeHigh = 300.0F;
constexpr std::string_view kHorizontalWord = "horizon";
constexpr std::string_view kVerticalWord = "vertical";
// Ce que chaque quart retranche à l'angle avant de l'appliquer.
constexpr float kHorizontalKeptTurn = 0.0F;
constexpr float kHorizontalFlippedTurn = -180.0F;
constexpr float kVerticalKeptTurn = -90.0F;
constexpr float kVerticalFlippedTurn = -270.0F;

// ── LE CÔTÉ ──────────────────────────────────────────────────────────────────
constexpr float kLeftwardLow = 90.0F;
constexpr float kLeftwardHigh = 270.0F;
constexpr std::string_view kRightWord = "right";
constexpr std::string_view kLeftWord = "left";

// ── LE DOUZIÈME DE TOUR ──────────────────────────────────────────────────────
constexpr float kTwelfthShiftDegrees = 105.0F;
constexpr std::int32_t kDegreesPerTwelfth = 30;
constexpr std::int32_t kTwelfths = 12;

// ── LE CARRÉ TIRÉ SANS REMISE ────────────────────────────────────────────────
// Le côté vaut trois, puis deux de plus tous les trois niveaux.
constexpr std::int32_t kGridSmallestSide = 3;
constexpr std::int32_t kGridLevelsPerStep = 3;
constexpr std::int32_t kGridSideStep = 2;
constexpr float kGridSpacing = 5.0F;
constexpr std::int32_t kGridDelayStepMs = 32;
// Le trou du nom tire l'une de trois variantes.
constexpr std::uint32_t kGridNameVariants = 3;

// ── L'ÉVENTAIL EN TROIS VAGUES ───────────────────────────────────────────────
constexpr std::int32_t kFanWaves = 3;
constexpr std::int32_t kFanFewestRays = 15;
constexpr std::uint32_t kFanRaysSpread = 5;
// Une vague part dix pas après la précédente, chaque pas valant trente-deux
// millisecondes.
constexpr std::int32_t kFanWaveSteps = 10;
constexpr std::int32_t kFanDelayStepMs = 32;

// ── L'ANNEAU DISPERSÉ ────────────────────────────────────────────────────────
constexpr std::int32_t kRingShots = 8;
constexpr std::int32_t kRingStepDegrees = 45;
// Le rayon est cinq fois un tirage de zéro à quatre ramené à trois au moins :
// quinze quatre fois sur cinq, vingt sinon.
constexpr std::uint32_t kRingRadiusDraws = 5;
constexpr std::int32_t kRingSmallestRadiusDraw = 3;
constexpr float kRingRadiusStep = 5.0F;
constexpr std::int32_t kRingDelayStepMs = 240;

// ── LA DISPERSION PAR NIVEAU ─────────────────────────────────────────────────
// Le nombre d'éclats par niveau, du premier au dixième ; au-delà, celui du
// dixième. Le niveau zéro n'a pas de nombre : rien ne naît.
constexpr std::array<std::int32_t, 11> kScatterShotsByLevel = {0, 5, 5, 5, 5, 5, 6, 6, 6, 7, 7};
constexpr std::int32_t kScatterHighestLevel = 10;
constexpr std::int32_t kScatterRangePerShot = 50;
constexpr float kScatterStep = 0.1F;
constexpr float kScatterHalf = 0.5F;
constexpr std::int32_t kScatterDelayStepMs = 80;

// ── LES SALVES EN QUARTS OPPOSÉS ─────────────────────────────────────────────
constexpr std::int32_t kBurstSalvos = 3;
constexpr std::int32_t kBurstSalvoStepMs = 200;
constexpr std::uint32_t kQuarters = 4;
constexpr std::int32_t kOppositeQuarter = 2;
constexpr std::int32_t kQuarterDegrees = 90;
// Le rayon : un tirage sur vingt, ramené à dix au moins.
constexpr std::uint32_t kBurstRadiusDraws = 20;
constexpr std::int32_t kBurstSmallestRadius = 10;

// Un entier tiré de zéro à `span` exclu.
[[nodiscard]] std::uint32_t DrawBelow(std::uint32_t span, std::uint32_t& generator) {
  return span == 0 ? 0U : NextEffectNodeDraw(generator) % span;
}

// Un entier tiré entre deux bornes, bornes comprises.
[[nodiscard]] float DrawnBetween(float low, float high, std::uint32_t& generator) {
  if (!(high > low)) {
    return low;
  }
  const auto span = static_cast<std::uint32_t>(high - low) + 1U;
  return low + static_cast<float>(DrawBelow(span, generator));
}

// Les entiers de zéro à `count` exclu, dans un ordre tiré au sort — chacun une
// fois.
[[nodiscard]] std::vector<std::int32_t> Shuffled(std::int32_t count, std::uint32_t& generator) {
  std::vector<std::int32_t> order(static_cast<std::size_t>(std::max(count, 0)));
  std::iota(order.begin(), order.end(), 0);
  for (std::size_t rang = order.size(); rang > 1; --rang) {
    const std::uint32_t pris = DrawBelow(static_cast<std::uint32_t>(rang), generator);
    std::swap(order[rang - 1], order[pris]);
  }
  return order;
}

// Le nom, son trou remplacé par `word`. Un nom sans ce trou reste tel quel.
[[nodiscard]] std::string WithHole(std::string_view name, std::string_view hole,
                                   std::string_view word) {
  std::string forme(name);
  if (const std::size_t trou = forme.find(hole); trou != std::string::npos) {
    forme.replace(trou, hole.size(), word);
  }
  return forme;
}

// Le nom, son trou numérique rempli : `%02d` sur deux chiffres, `%d` tel quel.
[[nodiscard]] std::string WithNumber(std::string_view name, std::int32_t number) {
  std::array<char, 16> chiffres{};
  if (name.find(kPaddedNumberHole) != std::string_view::npos) {
    std::snprintf(chiffres.data(), chiffres.size(), "%02d", number);
    return WithHole(name, kPaddedNumberHole, chiffres.data());
  }
  std::snprintf(chiffres.data(), chiffres.size(), "%d", number);
  return WithHole(name, kNumberHole, chiffres.data());
}

[[nodiscard]] float CosOfClientDegrees(float degrees) {
  return std::cos(degrees / kHalfTurnDegrees * kClientPi);
}
[[nodiscard]] float SinOfClientDegrees(float degrees) {
  return std::sin(degrees / kHalfTurnDegrees * kClientPi);
}

// La lance qui choisit son quart : le nom, le retournement et l'orientation.
[[nodiscard]] std::optional<NamedSowing> PierceByScreenQuarter(std::string_view name, float angle) {
  std::int32_t quart = -1;
  if (angle > kQuarterOneLow && angle <= kQuarterOneHigh) {
    quart = 0;
  } else if (angle > kQuarterOneHigh && angle <= kQuarterTwoHigh) {
    quart = 1;
  } else if (angle > kQuarterTwoHigh && angle <= kQuarterThreeHigh) {
    quart = 2;
  } else if ((angle > kQuarterThreeHigh && angle <= kFullTurnDegrees) ||
             (angle >= 0.0F && angle <= kQuarterOneLow)) {
    quart = 3;
  }
  if (quart < 0) {
    return std::nullopt;
  }
  const bool horizontal = (quart & 1) != 0;
  const bool garde = quart != 1 && quart != 2;
  NamedSowing sowing;
  sowing.name = WithHole(name, kWordHole, horizontal ? kHorizontalWord : kVerticalWord);
  float retranche = 0.0F;
  if (horizontal) {
    sowing.flip_x = garde;
    retranche = garde ? kHorizontalKeptTurn : kHorizontalFlippedTurn;
  } else {
    sowing.flip_y = garde;
    retranche = garde ? kVerticalKeptTurn : kVerticalFlippedTurn;
  }
  // ⚠ L'ENTIER D'ABORD, puis le tour : la troncature du client va vers zéro, et
  // l'angle ne franchit jamais moins un tour.
  sowing.turn_degrees =
      static_cast<float>((static_cast<std::int32_t>(angle + retranche) + kFullTurn) % kFullTurn);
  return sowing;
}

void ShuffledGridByLevel(const SkillNamedEffectEntry& entry, std::int32_t level,
                         NamedSowingDeck& deck, std::uint32_t& generator,
                         std::vector<NamedSowing>& sowing) {
  const std::int32_t cote = kGridSideStep * ((level - 1) / kGridLevelsPerStep) + kGridSmallestSide;
  if (deck.grid_side != cote) {
    deck.grid_side = cote;
    deck.grid_x = Shuffled(cote, generator);
    deck.grid_z = Shuffled(cote, generator);
  }
  for (std::int32_t rang = 0; rang < cote; ++rang) {
    NamedSowing un;
    un.name =
        WithNumber(entry.name, static_cast<std::int32_t>(DrawBelow(kGridNameVariants, generator)));
    const auto place = static_cast<std::size_t>(rang);
    un.client_offset =
        glm::vec3(static_cast<float>(deck.grid_x[place] - cote / 2) * kGridSpacing, 0.0F,
                  static_cast<float>(deck.grid_z[place] - cote / 2) * kGridSpacing);
    un.start_delay_ms = kGridDelayStepMs * rang;
    sowing.push_back(std::move(un));
  }
}

void ShuffledFanWaves(const SkillNamedEffectEntry& entry, std::uint32_t& generator,
                      std::vector<NamedSowing>& sowing) {
  for (std::int32_t vague = 0; vague < kFanWaves; ++vague) {
    const std::int32_t radii =
        kFanFewestRays + static_cast<std::int32_t>(DrawBelow(kFanRaysSpread, generator));
    const float part = kFullTurnDegrees / static_cast<float>(radii);
    const std::vector<std::int32_t> departs = Shuffled(radii, generator);
    for (std::int32_t radius = 0; radius < radii; ++radius) {
      NamedSowing un;
      un.name = WithNumber(entry.name, vague);
      const auto entiere = static_cast<std::uint32_t>(static_cast<std::int32_t>(part));
      un.turn_degrees =
          static_cast<float>(radius) * part + static_cast<float>(DrawBelow(entiere, generator));
      un.start_delay_ms =
          kFanDelayStepMs * (kFanWaveSteps * vague + departs[static_cast<std::size_t>(radius)]);
      sowing.push_back(std::move(un));
    }
  }
}

void ScatteredRing(const SkillNamedEffectEntry& entry, std::uint32_t& generator,
                   std::vector<NamedSowing>& sowing) {
  for (std::int32_t tir = 0; tir < kRingShots; ++tir) {
    const auto angle = static_cast<float>(tir * kRingStepDegrees);
    const std::int32_t vers_z = std::max(
        static_cast<std::int32_t>(DrawBelow(kRingRadiusDraws, generator)), kRingSmallestRadiusDraw);
    const std::int32_t vers_x = std::max(
        static_cast<std::int32_t>(DrawBelow(kRingRadiusDraws, generator)), kRingSmallestRadiusDraw);
    NamedSowing un;
    un.name = std::string(entry.name);
    un.client_offset =
        glm::vec3(SinOfClientDegrees(angle) * kRingRadiusStep * static_cast<float>(vers_x), 0.0F,
                  CosOfClientDegrees(angle) * kRingRadiusStep * static_cast<float>(vers_z));
    un.start_delay_ms = kRingDelayStepMs * tir;
    sowing.push_back(std::move(un));
  }
}

void ScatterByLevel(const SkillNamedEffectEntry& entry, std::int32_t level,
                    std::uint32_t& generator, std::vector<NamedSowing>& sowing) {
  // ⚠ LA BORNE EST NON SIGNÉE : un niveau négatif compte pour le dixième.
  const auto borne =
      static_cast<std::uint32_t>(level) > kScatterHighestLevel ? kScatterHighestLevel : level;
  const std::int32_t eclats = kScatterShotsByLevel[static_cast<std::size_t>(borne)];
  const std::int32_t etendue = kScatterRangePerShot * eclats;
  const float moitie = static_cast<float>(etendue) * kScatterHalf;
  const auto tirages = static_cast<std::uint32_t>(etendue + 1);
  for (std::int32_t shard = 0; shard < eclats; ++shard) {
    const float x = (static_cast<float>(DrawBelow(tirages, generator)) - moitie) * kScatterStep;
    const float z = (static_cast<float>(DrawBelow(tirages, generator)) - moitie) * kScatterStep;
    NamedSowing un;
    un.name = std::string(entry.name);
    un.client_offset = glm::vec3(x, 0.0F, z);
    un.start_delay_ms = kScatterDelayStepMs * shard;
    sowing.push_back(std::move(un));
  }
}

// Un éclat dans son quart de tour : l'abscisse et la profondeur tirent chacune
// leur rayon et leur angle, l'angle d'au moins le début du quart.
[[nodiscard]] glm::vec3 BurstInQuarter(std::int32_t quarter, std::uint32_t& generator) {
  const std::int32_t start = kQuarterDegrees * quarter;
  const auto etendue = static_cast<std::uint32_t>(start + kQuarterDegrees);
  const auto radius_x = static_cast<float>(std::max(
      static_cast<std::int32_t>(DrawBelow(kBurstRadiusDraws, generator)), kBurstSmallestRadius));
  const auto angle_x =
      static_cast<float>(std::max(static_cast<std::int32_t>(DrawBelow(etendue, generator)), start));
  const auto radius_z = static_cast<float>(std::max(
      static_cast<std::int32_t>(DrawBelow(kBurstRadiusDraws, generator)), kBurstSmallestRadius));
  const auto angle_z =
      static_cast<float>(std::max(static_cast<std::int32_t>(DrawBelow(etendue, generator)), start));
  return glm::vec3(SinOfClientDegrees(angle_x) * radius_x, 0.0F,
                   CosOfClientDegrees(angle_z) * radius_z);
}

void PairedQuarterBursts(const SkillNamedEffectEntry& entry, std::uint32_t& generator,
                         std::vector<NamedSowing>& sowing) {
  for (std::int32_t salve = 0; salve < kBurstSalvos; ++salve) {
    const auto quart = static_cast<std::int32_t>(DrawBelow(kQuarters, generator));
    for (const std::int32_t pris :
         {quart, (quart + kOppositeQuarter) % static_cast<std::int32_t>(kQuarters)}) {
      NamedSowing un;
      un.name = std::string(entry.name);
      un.client_offset = BurstInQuarter(pris, generator);
      un.start_delay_ms = kBurstSalvoStepMs * salve;
      sowing.push_back(std::move(un));
    }
  }
}

// Ce que la recette de l'entrée donne, avant les gestes communs.
[[nodiscard]] std::vector<NamedSowing> RecipeSowings(const SkillNamedEffectEntry& entry,
                                                     const NamedSowingContext& context,
                                                     NamedSowingDeck& deck,
                                                     std::uint32_t& generator) {
  std::vector<NamedSowing> sowing;
  // ⚠ SANS VUE, L'ANGLE VAUT ZÉRO, et c'est ce que rend le client pour deux
  // points confondus à l'écran.
  const float angle = context.screen_degrees.value_or(0.0F);
  switch (entry.recipe) {
    case NamedEffectRecipe::kNone: {
      NamedSowing un;
      un.name = entry.name_takes_level
                    ? WithHole(entry.name, kWordHole, std::to_string(context.level))
                    : std::string(entry.name);
      sowing.push_back(std::move(un));
      break;
    }
    case NamedEffectRecipe::kPierceByScreenQuarter:
      if (std::optional<NamedSowing> un = PierceByScreenQuarter(entry.name, angle)) {
        sowing.push_back(std::move(*un));
      }
      break;
    case NamedEffectRecipe::kSideByScreenAngle: {
      NamedSowing un;
      const bool leftward = angle > kLeftwardLow && angle < kLeftwardHigh;
      un.name = WithHole(entry.name, kWordHole, leftward ? kRightWord : kLeftWord);
      sowing.push_back(std::move(un));
      break;
    }
    case NamedEffectRecipe::kTwelfthByScreenAngle: {
      // ⚠ PAS DE CIBLE, PAS D'ÉCLAT : la méthode ne mesure l'angle que si elle
      // en a une.
      if (!context.screen_degrees.has_value()) {
        break;
      }
      const std::int32_t douzieme =
          static_cast<std::int32_t>(angle + kTwelfthShiftDegrees) % kFullTurn / kDegreesPerTwelfth;
      if (static_cast<std::uint32_t>(douzieme) < static_cast<std::uint32_t>(kTwelfths)) {
        NamedSowing un;
        un.name = WithNumber(entry.name, douzieme);
        sowing.push_back(std::move(un));
      }
      break;
    }
    case NamedEffectRecipe::kShuffledGridByLevel:
      ShuffledGridByLevel(entry, context.level, deck, generator, sowing);
      break;
    case NamedEffectRecipe::kShuffledFanWaves:
      ShuffledFanWaves(entry, generator, sowing);
      break;
    case NamedEffectRecipe::kScatteredRing:
      ScatteredRing(entry, generator, sowing);
      break;
    case NamedEffectRecipe::kScatterByLevel:
      ScatterByLevel(entry, context.level, generator, sowing);
      break;
    case NamedEffectRecipe::kPairedQuarterBursts:
      PairedQuarterBursts(entry, generator, sowing);
      break;
  }
  return sowing;
}

}  // namespace

std::int32_t NamedPaceDelayMs(NamedEffectPace pace,
                              const std::optional<NamedEffectPaceOf>& source_pace) {
  switch (pace) {
    case NamedEffectPace::kNone:
      return kRecordStartDelay;
    case NamedEffectPace::kTwoThirdsOfHit:
      if (!source_pace.has_value()) {
        return kRecordStartDelay;
      }
      // ⚠ UNE IMAGE DU COUP NÉGATIVE — un état qui ne la pose pas — donne un
      // délai négatif, et l'effet part alors aussitôt.
      return std::max(
          0, static_cast<std::int32_t>(source_pace->hit_frame * source_pace->cadence_steps *
                                       kTwoThirdsOfHitStepMs));
    case NamedEffectPace::kCadenceTimesFiftyPlusThirty:
      if (!source_pace.has_value()) {
        return kRecordStartDelay;
      }
      return static_cast<std::int32_t>(source_pace->cadence_steps * kFiftyMsPerCadence + kThirtyMs);
    case NamedEffectPace::kCadenceTimesHundredPlusHundred:
      if (!source_pace.has_value()) {
        return kServantDelayWithoutSourceMs;
      }
      return static_cast<std::int32_t>(source_pace->cadence_steps * kHundredMsPerCadence +
                                       kHundredMs);
  }
  return kRecordStartDelay;
}

std::vector<NamedSowing> ExpandNamedEntry(const SkillNamedEffectEntry& entry,
                                          const NamedSowingContext& context, NamedSowingDeck& deck,
                                          std::uint32_t& generator) {
  if ((entry.only_at_level >= 0 && context.level != entry.only_at_level) ||
      (entry.only_with_source_charm >= 0 &&
       context.source_charm_element != entry.only_with_source_charm)) {
    return {};
  }
  std::vector<NamedSowing> sowing = RecipeSowings(entry, context, deck, generator);
  const std::int32_t de_l_allure = NamedPaceDelayMs(entry.pace, context.source_pace);
  for (NamedSowing& un : sowing) {
    // LE DÉCALAGE ÉCRIT s'ajoute à celui de la recette, tiré une fois.
    const NamedEffectOffset& autour = entry.offset;
    un.client_offset += glm::vec3(DrawnBetween(autour.x_min, autour.x_max, generator),
                                  DrawnBetween(autour.height_min, autour.height_max, generator),
                                  DrawnBetween(autour.z_min, autour.z_max, generator));
    // LE DÉLAI : l'allure remplace tout ; sinon la recette, sinon l'entrée.
    if (entry.pace != NamedEffectPace::kNone) {
      un.start_delay_ms = de_l_allure;
    } else if (un.start_delay_ms < 0) {
      un.start_delay_ms = entry.start_delay_ms;
    }
    if (entry.start_delay_drawn_span_ms > 0) {
      un.start_delay_ms =
          std::max(un.start_delay_ms, 0) +
          static_cast<std::int32_t>(DrawBelow(entry.start_delay_drawn_span_ms, generator));
    }
    // L'ORIENTATION : celle de la recette, sinon celle de l'entrée.
    if (!un.turn_degrees.has_value()) {
      switch (entry.turn) {
        case NamedEffectTurn::kNone:
          break;
        case NamedEffectTurn::kFixed:
          un.turn_degrees = entry.turn_degrees;
          break;
        case NamedEffectTurn::kSourceToTarget:
          un.turn_degrees = context.screen_degrees.value_or(0.0F) + entry.turn_degrees;
          break;
      }
      if (entry.turn_drawn_span > 0) {
        un.turn_degrees = un.turn_degrees.value_or(0.0F) +
                          static_cast<float>(DrawBelow(entry.turn_drawn_span, generator));
      }
    }
    switch (entry.flip_x) {
      case NamedEffectFlip::kNever:
        break;
      case NamedEffectFlip::kWhenSourceFacesAway:
        un.flip_x = un.flip_x ||
                    (context.source_sprite_direction.has_value() &&
                     *context.source_sprite_direction % kSpriteDirections >= kFirstAwayDirection);
        break;
      case NamedEffectFlip::kOnOddHit:
        un.flip_x = un.flip_x || (context.hit_index & 1U) != 0;
        break;
    }
    un.scale = DrawnBetween(static_cast<float>(entry.scale_hundredths_min),
                            static_cast<float>(entry.scale_hundredths_max), generator) *
               kHundredth;
    un.laid_on_ground = entry.laid_on_ground;
  }
  return sowing;
}

float NamedScreenDegrees(glm::vec2 from, glm::vec2 to) {
  const glm::vec2 offset = to - from;
  const float length = std::sqrt(offset.x * offset.x + offset.y * offset.y);
  if (length == 0.0F) {
    return 0.0F;
  }
  const auto arc = static_cast<float>(std::acos(static_cast<double>(offset.x / length)));
  const float angle_degrees = arc / kClientPi * kHalfTurnDegrees;
  return from.y > to.y ? kFullTurnDegrees - angle_degrees : angle_degrees;
}

glm::vec2 NamedScreenPoint(const Camera& camera, glm::vec3 world) {
  const glm::vec4 vu = camera.ViewMatrix() * glm::vec4(world, 1.0F);
  // La caméra regarde vers les profondeurs NÉGATIVES : la distance est leur
  // opposé. L'ordonnée de l'écran descend quand celle de la vue monte.
  const float distance = -vu.z;
  if (!(distance > 0.0F)) {
    return glm::vec2(vu.x, -vu.y);
  }
  return glm::vec2(vu.x / distance, -vu.y / distance);
}

}  // namespace ro::render
