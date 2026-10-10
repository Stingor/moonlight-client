#include "ro/render/floating_number.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/render/damage_mark.h"
#include "ro/render/skill_suite_table.h"
#include "ro/render/sprite_billboard.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {
namespace {

// L'opacité de naissance, sur 255 — et le plein d'un canal.
constexpr float kInitialAlpha = 250.0F;
constexpr float kOpaqueAlpha = 255.0F;

// 🔴 LES HAUTEURS ET LES MONTÉES SE LISENT EN UNITÉS DU MONDE, et se convertissent
// en pixels de sprite à la sortie : sept par unité. Le client déplace la
// POSITION de ses sprites-enfants — l'animation écrit les mêmes champs que la
// pose recopie de la position de la cible — et une position est dans le
// monde. Voir l'en-tête.
constexpr float kPixelsPerUnit = kSpritePixelsPerWorldUnit;

// ── L'allure montante (type 22) ─────────────────────────────────────────────
//
// Le nombre naît vingt-sept unités au-dessus des pieds — le message de position
// retire quinze plus douze à l'ordonnée (message 64 du sprite-enfant) — et
// monte d'un dixième d'unité par tic. Il vit cent vingt tics ; à
// partir du quatre-vingt-dixième, il perd huit points d'opacité par tic. Il
// naît à l'échelle un demi et grossit jusqu'à trois : le client ajoute à chaque
// IMAGE dix-huit centièmes du temps écoulé, ce qui dépend de la cadence — lu à
// soixante images par seconde, cela fait treize centièmes du carré du temps, et
// l'échelle trois est atteinte en un dixième de seconde.
constexpr float kRisingStartUnits = 27.0F;
constexpr float kRisingUnitsPerTick = 0.1F;
constexpr float kRisingLifeTicks = 120.0F;
constexpr float kRisingFadeStartTicks = 90.0F;
constexpr float kRisingFadePerTick = 8.0F;
constexpr float kRisingInitialScale = 0.5F;
constexpr float kRisingFinalScale = 3.0F;
constexpr float kRisingPopPerTickSquared = 0.13F;

// ── L'allure jaillissante (type 14) ─────────────────────────────────────────
//
// Le message de position pose le nombre douze unités au-dessus des pieds, et
// son allure en retire huit dès la première image : il naît à quatre. Il décrit
// une cloche : 2t − t²/30, qui culmine à trente unités au trentième tic et
// revient au soixantième. Dès le tic qui suit sa naissance, un « rebond » de
// quatre tiers d'unité le rabaisse d'autant — c'est tout ce que ce terme fait,
// quel que soit le signe de la hauteur. Il naît à l'échelle cinq et rétrécit de
// vingt-quatre centièmes par tic jusqu'à un et deux dixièmes ; il perd trois
// virgule quatre points d'opacité par tic et vit soixante-dix tics. Sa DÉRIVE de
// côté est un déplacement dans le monde : voir `BurstDriftUnitsAt`.
//
// 🔴 LA CLOCHE PASSE SOUS LES PIEDS en fin de vie, et elle s'y VOIT : le client
// pose tous ses nombres à une profondeur fixe, en avant de la scène entière, si
// bien que ni le terrain ni la cible ne les cachent jamais. Vers le soixante et
// unième tic, elle franchit les pieds, et elle meurt une vingtaine d'unités
// plus bas, presque éteinte.
constexpr float kBurstingStartUnits = 12.0F - 8.0F;
constexpr float kBurstingRiseUnitsPerTick = 2.0F;
constexpr float kBurstingFallUnitsPerTickSquared = 1.0F / 30.0F;
constexpr float kBurstingBounceUnits = 4.0F / 3.0F;
constexpr float kBurstingLifeTicks = 70.0F;
constexpr float kBurstingFadePerTick = 3.4F;
constexpr float kBurstingInitialScale = 5.0F;
constexpr float kBurstingFinalScale = 1.2F;
constexpr float kBurstingShrinkPerTick = 0.24F;

// ── L'allure d'un coup critique (type 16) ───────────────────────────────────
//
// La même que la jaillissante en tout point sauf un : elle s'arrête de
// rétrécir à un et trois dixièmes. Un chiffre de critique reste donc un peu
// plus gros que les autres, et c'est tout ce qui l'en distingue.
constexpr float kCriticalFinalScale = 1.3F;

// ── L'allure d'un soin (type 114) ───────────────────────────────────────────
//
// Elle naît DOUZE unités au-dessus des pieds — le message 64 retire douze, sans
// les quinze de la montante — et à l'échelle de la jaillissante, qui vient du
// même champ. Elle monte de dix-huit centièmes d'unité par tic, rétrécit au
// rythme de la jaillissante jusqu'à un, et perd deux points d'opacité par tic
// dès sa naissance au lieu d'attendre le quatre-vingt-dixième.
constexpr float kHealingStartUnits = 12.0F;
constexpr float kHealingUnitsPerTick = 0.18F;
constexpr float kHealingFadePerTick = 2.0F;
constexpr float kHealingFinalScale = 1.0F;

// Les quatre quadrants de l'angle du client, en degrés.
constexpr float kQuarterTurnDegrees = 90.0F;

// Les lettres qu'écrit une police à la place des deux glyphes d'abréviation du
// sprite.
constexpr char kThousandsLetter = 'K';
constexpr char kMillionsLetter = 'M';

// La vie du genre, en tics.
[[nodiscard]] float LifeTicks(NumberStyle style) {
  // Les deux cloches vivent soixante-dix tics ; le montant et le soin, cent
  // vingt.
  return IsBurst(style) ? kBurstingLifeTicks : kRisingLifeTicks;
}

// La taille où le genre s'arrête : là où son retour finit.
[[nodiscard]] float FinalScaleOf(NumberStyle style) {
  switch (style) {
    case NumberStyle::kRising:
      return kRisingFinalScale;
    case NumberStyle::kBursting:
      return kBurstingFinalScale;
    case NumberStyle::kCriticalBurst:
      return kCriticalFinalScale;
    case NumberStyle::kHealing:
      break;
  }
  return kHealingFinalScale;
}

// OÙ LE GENRE NAÎT, en unités au-dessus des pieds. ⚠ LE REBOND DE LA CLOCHE Y
// EST DÉJÀ : le client ne le retire qu'à partir de la deuxième image, et une
// image ne vaut pas un réglage à part.
[[nodiscard]] float StartUnitsOf(NumberStyle style) {
  switch (style) {
    case NumberStyle::kRising:
      return kRisingStartUnits;
    case NumberStyle::kBursting:
    case NumberStyle::kCriticalBurst:
      return kBurstingStartUnits - kBurstingBounceUnits;
    case NumberStyle::kHealing:
      break;
  }
  return kHealingStartUnits;
}

struct Pose {
  float scale = 1.0F;
  float alpha = kInitialAlpha;
  // Où le nombre naît au-dessus des pieds, en pixels de sprite, départ du
  // réglage compris.
  float rise = 0.0F;
  // Et de combien il a avancé vers son point d'arrivée, x vers la droite et y
  // vers le haut : voir `ArrivalDriftAt`.
  std::array<float, 2> drift{0.0F, 0.0F};
  // De combien son point de départ est décalé vers la droite.
  float start_x = 0.0F;
};

// Le nombre à cet instant.
//
// 🔴 SA VIE ET SON CHEMIN SONT CEUX DU RÉGLAGE, ET RIEN D'AUTRE : le genre ne
// donne plus que la taille où il s'arrête et le point où il naît. Tout ce qui
// le déplace est sa trajectoire, tout ce qui le grandit ou le voile est sa vie.
Pose PoseAt(NumberStyle style, float elapsed_ms, const NumberLook& look) {
  const NumberLife life = LifeOf(style, look);
  Pose pose;
  pose.scale = FinalScaleOf(style) * SettlingScaleAt(elapsed_ms, life) * look.scale;
  pose.alpha =
      std::clamp(kInitialAlpha * FadeAt(elapsed_ms, life), 0.0F, kOpaqueAlpha) * look.opacity;
  pose.rise = StartUnitsOf(style) * kPixelsPerUnit + look.lift_pixels;
  pose.drift = ArrivalDriftAt(elapsed_ms / SaneLifeMs(life.life_ms), look);
  pose.start_x = look.start_x_pixels;
  return pose;
}

// Où le nombre se pose, APRÈS l'échelle : le départ, la montée et la dérive
// déplacent le nombre, ils ne l'étirent pas — et l'axe Y d'un sprite DESCEND,
// donc ce qui monte se retranche.
[[nodiscard]] glm::vec2 LandingOf(const Pose& pose) {
  return glm::vec2{pose.start_x + pose.drift[0], -(pose.rise + pose.drift[1])};
}

// L'opacité d'un quad, celle du nombre par-dessus celle de la pose.
[[nodiscard]] std::uint8_t AlphaOf(const Pose& pose, const FloatingNumber& number) {
  return static_cast<std::uint8_t>(std::clamp(pose.alpha * number.opacity, 0.0F, kOpaqueAlpha));
}

// 🔴 LE FONDU DU RÉGLAGE VA JUSQU'À ZÉRO, celui du client s'arrête un peu
// avant : il perd un pas fixe par tic et s'éteint à la fin de sa vie, quoi
// qu'il reste. Ce qui reste ne dépasse pas un vingtième de son opacité de
// naissance — c'est l'écart qu'on accepte en faisant de son fondu un réglage.
constexpr float kFadeApproachShare = 0.05F;
static_assert(kInitialAlpha - kRisingFadePerTick * (kRisingLifeTicks - kRisingFadeStartTicks) <=
              kInitialAlpha * kFadeApproachShare);
static_assert(kInitialAlpha - kBurstingFadePerTick * kBurstingLifeTicks <=
              kInitialAlpha * kFadeApproachShare);
static_assert(kInitialAlpha - kHealingFadePerTick * kRisingLifeTicks <=
              kInitialAlpha * kFadeApproachShare);

}  // namespace

NumberLife ClientLifeOf(NumberStyle style) noexcept {
  NumberLife life;
  life.life_ms = LifeTicks(style) * kNumberTickMs;
  switch (style) {
    case NumberStyle::kRising:
      // Il naît au sixième de sa taille finale et la rejoint en un dixième de
      // seconde — le client grossit au carré du temps, ici d'un pas régulier.
      life.birth_scale = kRisingInitialScale / kRisingFinalScale;
      life.birth_settle_ms =
          std::sqrt((kRisingFinalScale - kRisingInitialScale) / kRisingPopPerTickSquared) *
          kNumberTickMs;
      life.fade_start_ms = kRisingFadeStartTicks * kNumberTickMs;
      break;
    case NumberStyle::kBursting:
    case NumberStyle::kCriticalBurst:
    case NumberStyle::kHealing:
      // Cinq fois la taille normale, et un vingt-quatrième de moins par tic
      // jusqu'à la taille où le genre s'arrête ; pâle dès la naissance.
      life.birth_scale = kBurstingInitialScale / FinalScaleOf(style);
      life.birth_settle_ms =
          (kBurstingInitialScale - FinalScaleOf(style)) / kBurstingShrinkPerTick * kNumberTickMs;
      life.fade_start_ms = 0.0F;
      break;
  }
  return life;
}

NumberRoute ClientRouteOf(NumberStyle style) noexcept {
  const float life_ticks = LifeTicks(style);
  NumberRoute route;
  switch (style) {
    case NumberStyle::kRising:
      route.arrival_y_pixels = kRisingUnitsPerTick * life_ticks * kPixelsPerUnit;
      break;
    case NumberStyle::kHealing:
      route.arrival_y_pixels = kHealingUnitsPerTick * life_ticks * kPixelsPerUnit;
      break;
    case NumberStyle::kBursting:
    case NumberStyle::kCriticalBurst: {
      // LA CLOCHE a·q + b·q², q la part de la vie : une courbe de Bézier dont
      // l'arrivée vaut a + b et dont chaque poignée s'écarte de −b/3 du chemin
      // droit.
      const float rising = kBurstingRiseUnitsPerTick * life_ticks;
      const float falling = -kBurstingFallUnitsPerTickSquared * life_ticks * life_ticks;
      route.arrival_y_pixels = (rising + falling) * kPixelsPerUnit;
      route.bend_y_pixels = -falling / kBezierHandleWeight * kPixelsPerUnit;
      break;
    }
  }
  return route;
}

NumberLook ClientLookOf(NumberStyle style) noexcept {
  const NumberRoute route = ClientRouteOf(style);
  NumberLook look;
  look.arrival_y_pixels = route.arrival_y_pixels;
  look.bend_one_y_pixels = route.bend_y_pixels;
  look.bend_two_y_pixels = route.bend_y_pixels;
  look.life = ClientLifeOf(style);
  return look;
}

NumberLife LifeOf(NumberStyle style, const NumberLook& look) noexcept {
  return look.life.has_value() ? *look.life : ClientLifeOf(style);
}

std::array<float, 2> BurstDriftClientDiagonal(float facing_degrees) noexcept {
  // 🔴 L'ANGLE N'EST PAS RAMENÉ AU TOUR, et c'est la règle : le client le
  // tronque en entier et le compare tel quel. Un angle négatif ou d'un tour et
  // plus tombe donc dans le dernier quadrant — voir l'en-tête.
  const auto angle = static_cast<float>(static_cast<std::int32_t>(facing_degrees));
  if (angle < 0.0F) {
    return {kBurstDriftDiagonal, kBurstDriftDiagonal};
  }
  if (angle < kQuarterTurnDegrees) {
    return {-kBurstDriftDiagonal, kBurstDriftDiagonal};
  }
  if (angle < 2.0F * kQuarterTurnDegrees) {
    return {-kBurstDriftDiagonal, -kBurstDriftDiagonal};
  }
  if (angle < 3.0F * kQuarterTurnDegrees) {
    return {kBurstDriftDiagonal, -kBurstDriftDiagonal};
  }
  return {kBurstDriftDiagonal, kBurstDriftDiagonal};
}

std::vector<std::uint8_t> NumberGlyphs(std::uint32_t value, bool abbreviate) {
  std::vector<std::uint8_t> glyphs;
  std::uint32_t written = value;
  // Le marqueur D'ABORD : détaché le premier, il prend le rang des unités et se
  // dessine donc à droite, comme chez le client.
  if (!abbreviate) {
    written = std::min(value, kLargestShownValue);
  } else if (value >= kFirstMillionsValue) {
    glyphs.push_back(kMillionsGlyph);
    written = value / kValuesPerMillionsGlyph;
  } else if (value >= kFirstThousandsValue) {
    glyphs.push_back(kThousandsGlyph);
    written = value / kValuesPerThousandsGlyph;
  }
  do {
    glyphs.push_back(static_cast<std::uint8_t>(written % 10));
    written /= 10;
  } while (written != 0);
  return glyphs;
}

std::uint16_t ShownHitCount(std::uint8_t damage_type, std::uint16_t announced_hits) {
  const bool counts = damage_type == kMultipleBlows || damage_type == kMultipleBlowsEndured ||
                      damage_type == kCriticalMultipleBlows;
  if (!counts || announced_hits < 1 || announced_hits > kLargestHitCount) {
    return 1;
  }
  return announced_hits;
}

std::optional<std::array<std::uint8_t, 3>> SupportNumberColor(std::uint16_t skill_id) {
  for (const SkillSuiteStep& step : SkillSuiteOf(skill_id, SkillSuiteMoment::kAnnounced)) {
    if (step.kind == SkillSuiteKind::kSupportNumber) {
      return std::array<std::uint8_t, 3>{static_cast<std::uint8_t>(step.number_argb >> 16U),
                                         static_cast<std::uint8_t>(step.number_argb >> 8U),
                                         static_cast<std::uint8_t>(step.number_argb)};
    }
  }
  return std::nullopt;
}

std::optional<std::array<std::uint8_t, 3>> EffectNumberColor(std::uint32_t effect_id) {
  // Les deux qui NOMMENT ce qu'ils comptent portent la couleur de ce qu'ils
  // rendent : le vert de la vie, le bleu de la magie — les mêmes que le soin et
  // la récupération lancés en compétence.
  if (effect_id == kHpOverTimeEffect) {
    return kHealNumberColor;
  }
  if (effect_id == kSpOverTimeEffect) {
    return kSpRecoveryNumberColor;
  }
  if (effect_id < kFirstColoredNumberEffect || effect_id > kLastColoredNumberEffect) {
    return std::nullopt;
  }
  // Les huit qui ne nomment QU'UNE COULEUR, dans l'ordre où le client les
  // essaie — vert, bleu, rouge, violet, noir, blanc, jaune, rose. Le dernier
  // n'est pas un rose franc : le client écrit 0xFFFF55B1.
  constexpr std::array<std::array<std::uint8_t, 3>,
                       kLastColoredNumberEffect - kFirstColoredNumberEffect + 1>
      kColors = {{
          {0, 255, 0},
          {0, 0, 255},
          {255, 0, 0},
          {255, 0, 255},
          {0, 0, 0},
          {255, 255, 255},
          {255, 255, 0},
          {255, 85, 177},
      }};
  return kColors[effect_id - kFirstColoredNumberEffect];
}

bool FloatingNumberExpired(NumberStyle style, float elapsed_ms, const NumberLook& look) {
  return elapsed_ms > LifeOf(style, look).life_ms;
}

bool FloatingNumberExpired(NumberStyle style, float elapsed_ms) {
  return FloatingNumberExpired(style, elapsed_ms, ClientLookOf(style));
}

ComposedActor ComposeFloatingNumber(const DigitSprite& digits, const FloatingNumber& number,
                                    float elapsed_ms) {
  return ComposeFloatingNumber(digits, number, elapsed_ms, ClientLookOf(number.style));
}

ComposedActor ComposeFloatingNumber(const DigitSprite& digits, const FloatingNumber& number,
                                    float elapsed_ms, const NumberLook& look) {
  ComposedActor composed;
  if (digits.sprite == nullptr || digits.animation == nullptr || elapsed_ms < 0.0F ||
      FloatingNumberExpired(number.style, elapsed_ms, look)) {
    return composed;
  }
  const Pose pose = PoseAt(number.style, elapsed_ms, look);
  const glm::vec2 landing = LandingOf(pose);
  const std::array<std::uint8_t, 4> tint = {number.color[0], number.color[1], number.color[2],
                                            AlphaOf(pose, number)};

  // Du plus fort au plus faible, de gauche à droite : le glyphe de rang i
  // depuis les unités est à un demi-écart fois (n − 1 − 2i), et le nombre est
  // centré.
  const std::vector<std::uint8_t> glyphs_from_units = NumberGlyphs(number.value, look.abbreviate);
  const auto count = static_cast<float>(glyphs_from_units.size());
  for (std::size_t place = glyphs_from_units.size(); place-- > 0;) {
    const float rank = static_cast<float>(place);
    const float offset_x = look.digit_pitch_pixels * 0.5F * (count - 1.0F - 2.0F * rank);
    for (SpriteQuad quad :
         BuildFrameQuads(*digits.sprite, *digits.animation, 0, glyphs_from_units[place])) {
      for (glm::vec2& corner : quad.corners) {
        // L'écart entre chiffres est AVANT l'échelle — il fait partie du
        // nombre —, la place APRÈS : elle le déplace sans l'étirer.
        corner = (corner + glm::vec2{offset_x, 0.0F}) * pose.scale + landing;
      }
      quad.tint = tint;
      composed.sources.push_back(
          SpriteImageKey{digits.sprite, nullptr, quad.section, quad.image_index});
      composed.quads.push_back(quad);
    }
  }
  return composed;
}

std::optional<NumberText> ComposeFloatingNumberText(const FloatingNumber& number,
                                                    float elapsed_ms) {
  return ComposeFloatingNumberText(number, elapsed_ms, ClientLookOf(number.style));
}

std::optional<NumberText> ComposeFloatingNumberText(const FloatingNumber& number, float elapsed_ms,
                                                    const NumberLook& look) {
  if (elapsed_ms < 0.0F || FloatingNumberExpired(number.style, elapsed_ms, look)) {
    return std::nullopt;
  }
  const Pose pose = PoseAt(number.style, elapsed_ms, look);
  NumberText text;
  text.landing_pixels = LandingOf(pose);
  text.scale = pose.scale;
  text.pitch_pixels = look.digit_pitch_pixels;
  text.tint = {number.color[0], number.color[1], number.color[2], AlphaOf(pose, number)};
  // Les glyphes viennent unités d'abord : on les retourne pour lire de gauche à
  // droite, et le marqueur d'abréviation, détaché le premier, finit à droite.
  const std::vector<std::uint8_t> glyphs_from_units = NumberGlyphs(number.value, look.abbreviate);
  for (std::size_t place = glyphs_from_units.size(); place-- > 0;) {
    const std::uint8_t glyph = glyphs_from_units[place];
    text.glyphs.push_back(glyph == kThousandsGlyph  ? kThousandsLetter
                          : glyph == kMillionsGlyph ? kMillionsLetter
                                                    : static_cast<char>('0' + glyph));
  }
  return text;
}

std::optional<GlyphBox> DigitGlyphBox(const DigitSprite& digits) {
  if (digits.sprite == nullptr || digits.animation == nullptr) {
    return std::nullopt;
  }
  const std::vector<SpriteQuad> zero = BuildFrameQuads(*digits.sprite, *digits.animation, 0, 0);
  if (zero.empty()) {
    return std::nullopt;
  }
  float top = zero.front().corners[0].y;
  float bottom = top;
  for (const SpriteQuad& quad : zero) {
    for (const glm::vec2& corner : quad.corners) {
      top = std::min(top, corner.y);
      bottom = std::max(bottom, corner.y);
    }
  }
  if (bottom <= top) {
    return std::nullopt;
  }
  return GlyphBox{.height_pixels = bottom - top, .middle_y_pixels = (top + bottom) * 0.5F};
}

ComposedActor ComposeNumberStar(const FloatingNumber& number, const formats::SprSprite& mark_sprite,
                                const formats::ActAnimation& mark_animation, DamageMark star,
                                float elapsed_ms) {
  return ComposeNumberStar(number, mark_sprite, mark_animation, star, elapsed_ms,
                           ClientLookOf(number.style));
}

ComposedActor ComposeNumberStar(const FloatingNumber& number, const formats::SprSprite& mark_sprite,
                                const formats::ActAnimation& mark_animation, DamageMark star,
                                float elapsed_ms, const NumberLook& look) {
  if (elapsed_ms < 0.0F || FloatingNumberExpired(number.style, elapsed_ms, look)) {
    return {};
  }
  // 🔴 ELLE A SA PROPRE TAILLE ET SA PROPRE OPACITÉ, ET ON NE LUI DONNE QUE SA
  // PLACE. Chez le client, c'est un sprite-enfant à part — genre quinze contre
  // seize pour le nombre —, qui naît à l'échelle trois quand le nombre naît à
  // cinq et rétrécit deux fois plus lentement. `ComposeDamageMark` les calcule ;
  // le premier port lui appliquait la pose du nombre en entier, ce qui la
  // faisait naître presque deux fois trop grosse.
  //
  // ⚠ CE QU'ELLE PREND ENCORE DU NOMBRE, C'EST SA PLACE : les deux genres
  // partagent le même corps dans le client — même hauteur de naissance, même
  // cloche, même dérive —, si bien que les suivre revient à porter l'étoile là
  // où le nombre s'est rendu. Elle n'a donc ni hauteur ni point d'arrivée en
  // propre, sans quoi elle se déplacerait deux fois.
  NumberLook star_look;
  star_look.scale = look.scale;
  // Le nuancier du critique peut voiler ses nombres ; l'étoile suit, faute de
  // quoi un nombre translucide garderait un fond opaque.
  star_look.opacity = look.opacity * number.opacity;
  // SON TEMPS EST CELUI DU NOMBRE, rapporté au client : une vie deux fois plus
  // longue l'étire d'autant, et un nombre qui naît deux fois plus gros que
  // chez le client garde son fond à sa mesure.
  const NumberLife life = LifeOf(number.style, look);
  const NumberLife client = ClientLifeOf(number.style);
  const MarkPace pace{.stretch = life.life_ms / client.life_ms,
                      .birth_ratio = life.birth_scale / client.birth_scale,
                      .birth_settle_ms = life.birth_settle_ms};
  ComposedActor composed = ComposeDamageMark(mark_sprite, mark_animation, star, kMarkOnStar,
                                             elapsed_ms, star_look, pace);
  const glm::vec2 landing = LandingOf(PoseAt(number.style, elapsed_ms, look));
  for (SpriteQuad& quad : composed.quads) {
    for (glm::vec2& corner : quad.corners) {
      corner += landing;
    }
  }
  return composed;
}

namespace {

// Un nombre et l'étoile qu'il porte, dans le même cadre — l'étoile derrière :
// un fond se peint sous ce qu'il porte.
ComposedActor ComposeStarredNumber(const DigitSprite& digits, const FloatingNumber& number,
                                   const formats::SprSprite& mark_sprite,
                                   const formats::ActAnimation& mark_animation, DamageMark star,
                                   float elapsed_ms, const NumberLook& look) {
  ComposedActor composed =
      ComposeNumberStar(number, mark_sprite, mark_animation, star, elapsed_ms, look);
  const ComposedActor front = ComposeFloatingNumber(digits, number, elapsed_ms, look);
  composed.quads.insert(composed.quads.end(), front.quads.begin(), front.quads.end());
  composed.sources.insert(composed.sources.end(), front.sources.begin(), front.sources.end());
  return composed;
}

}  // namespace

ComposedActor ComposeCriticalNumber(const DigitSprite& digits, const FloatingNumber& number,
                                    const formats::SprSprite& mark_sprite,
                                    const formats::ActAnimation& mark_animation, float elapsed_ms,
                                    const NumberLook& look) {
  return ComposeStarredNumber(digits, number, mark_sprite, mark_animation, DamageMark::kCritical,
                              elapsed_ms, look);
}

ComposedActor ComposeRisingStarNumber(const DigitSprite& digits, const FloatingNumber& number,
                                      const formats::SprSprite& mark_sprite,
                                      const formats::ActAnimation& mark_animation, float elapsed_ms,
                                      const NumberLook& look) {
  return ComposeStarredNumber(digits, number, mark_sprite, mark_animation, DamageMark::kRisingStar,
                              elapsed_ms, look);
}

ComposedActor ComposeCriticalNumber(const DigitSprite& digits, const FloatingNumber& number,
                                    const formats::SprSprite& mark_sprite,
                                    const formats::ActAnimation& mark_animation, float elapsed_ms) {
  return ComposeCriticalNumber(digits, number, mark_sprite, mark_animation, elapsed_ms,
                               ClientLookOf(number.style));
}

ComposedActor ComposeRisingStarNumber(const DigitSprite& digits, const FloatingNumber& number,
                                      const formats::SprSprite& mark_sprite,
                                      const formats::ActAnimation& mark_animation,
                                      float elapsed_ms) {
  return ComposeRisingStarNumber(digits, number, mark_sprite, mark_animation, elapsed_ms,
                                 ClientLookOf(number.style));
}

}  // namespace ro::render
