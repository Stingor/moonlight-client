#include "ro/render/damage_mark.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/sprite_billboard.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {
namespace {

// Les actions du fichier, relues en les rendant une par une :
//
//   0  « Miss »            3  la bulle « LUCKY!! »
//   1  « Guard »           4  « Miss », une autre animation
//   2  l'étoile d'impact
constexpr std::size_t kMissAction = 0;
constexpr std::size_t kCriticalAction = 2;
constexpr std::size_t kLuckyAction = 3;

// L'opacité pleine, et celle où naissent les sprites-enfants du mot. Le mot
// pâlit depuis la seconde, l'étoile depuis la première ; seule la bulle ne
// pâlit pas — elle s'éteint avec son animation.
constexpr float kOpaque = 255.0F;
constexpr float kInitialAlpha = 250.0F;

// La montée d'une marque se lit en unités du monde, comme celle d'un nombre.
constexpr float kPixelsPerUnit = kSpritePixelsPerWorldUnit;

// L'image où l'étoile en est : elle tourne d'une demie par tic et SE FIGE sur
// la dernière — le client cesse de la calculer dès qu'elle l'atteint, au lieu
// de la faire repartir à zéro.
[[nodiscard]] std::size_t StarFrameAt(const formats::ActAction& action, float ticks) {
  if (action.frames.empty()) {
    return 0;
  }
  const std::size_t last = action.frames.size() - 1;
  const float turned = kStarFramesPerTick * std::max(0.0F, ticks);
  return std::min(last, static_cast<std::size_t>(turned));
}

// Sa taille : elle naît à trois et rétrécit jusqu'à un.
[[nodiscard]] float StarScaleAt(float ticks) {
  return std::max(kStarFinalScale, kStarInitialScale - kStarShrinkPerTick * ticks);
}

// L'étoile d'un coup de pied garde l'état de sa NAISSANCE jusqu'au premier
// tic — l'échelle trois et le gris des deux étoiles, à pleine opacité —, puis
// prend celui que chaque tic lui repose.
[[nodiscard]] float RisingStarScaleAt(float ticks) {
  return ticks > 0.0F ? kRisingStarScale : kRisingStarBirthScale;
}

[[nodiscard]] float RisingStarAlphaAt(float ticks) {
  if (ticks <= 0.0F) {
    return kOpaque;
  }
  return std::clamp(
      kRisingStarAlpha - std::max(0.0F, ticks - kRisingStarFadeStartTicks) * kRisingStarFadePerTick,
      0.0F, kOpaque);
}

}  // namespace

std::size_t DamageMarkAction(DamageMark mark) {
  switch (mark) {
    case DamageMark::kLucky:
      return kLuckyAction;
    case DamageMark::kCritical:
    case DamageMark::kRisingStar:
      return kCriticalAction;
    case DamageMark::kMiss:
      break;
  }
  return kMissAction;
}

float DamageMarkLifeMs(const formats::ActAnimation& animation, DamageMark mark, float stretch) {
  const std::size_t index = DamageMarkAction(mark);
  if (index >= animation.actions().size()) {
    return 0.0F;
  }
  if (IsStillMark(mark)) {
    // Sa durée est celle du genre, pas celle de l'action : le mot ne joue pas
    // ses images, il monte.
    return kMissLifeTicks * kMarkTickMs * SaneLifeFactor(stretch);
  }
  if (mark == DamageMark::kCritical) {
    // L'étoile non plus : elle vit ce que vit la cloche qui la porte, et se
    // fige sur sa dernière image bien avant, en continuant de pâlir.
    return kStarLifeTicks * kMarkTickMs * SaneLifeFactor(stretch);
  }
  if (mark == DamageMark::kRisingStar) {
    return kRisingStarLifeTicks * kMarkTickMs * SaneLifeFactor(stretch);
  }
  const formats::ActAction& action = animation.actions()[index];
  return static_cast<float>(action.frames.size()) * action.frame_interval_ms() *
         SaneLifeFactor(stretch);
}

ComposedActor ComposeDamageMark(const formats::SprSprite& sprite,
                                const formats::ActAnimation& animation, DamageMark mark,
                                const std::array<std::uint8_t, 3>& color, float elapsed_ms,
                                const NumberLook& look, const MarkPace& pace) {
  ComposedActor composed;
  const float life_ms = DamageMarkLifeMs(animation, mark, pace.stretch);
  if (elapsed_ms < 0.0F || elapsed_ms >= life_ms) {
    return composed;
  }
  const std::size_t index = DamageMarkAction(mark);
  const formats::ActAction& action = animation.actions()[index];
  // Le temps de la marque, ÉTIRÉ comme la vie du nombre qu'elle accompagne :
  // son animation et sa montée ralentissent ensemble.
  const float stretched_ms = elapsed_ms / SaneLifeFactor(pace.stretch);
  const float ticks = stretched_ms / kMarkTickMs;
  // TROIS FAÇONS D'AVANCER L'IMAGE, une par genre : le mot reste sur sa
  // PREMIÈRE et monte ; la bulle joue son animation une fois à la cadence de
  // son fichier ; l'étoile TOURNE d'une demie par tic, puis se fige sur sa
  // dernière — elle ne disparaît pas avec son animation, elle survit à
  // découvert jusqu'au terme de la cloche.
  //
  // L'étoile d'un coup de pied est la quatrième : son image est REMISE à la
  // première à chaque tic, et elle ne monte pas d'elle-même — c'est le nombre
  // qui la porte, comme la cloche porte l'étoile du critique.
  const bool still = IsStillMark(mark);
  const bool star = mark == DamageMark::kCritical;
  const bool rising_star = mark == DamageMark::kRisingStar;
  const std::size_t frame = still || rising_star ? 0
                            : star               ? StarFrameAt(action, ticks)
                                                 : FrameAtTimeOnce(action, stretched_ms);
  const float rise = (still ? kMissUnitsPerTick * ticks * kPixelsPerUnit : 0.0F) + look.lift_pixels;
  // La marque va vers le point d'arrivée comme un nombre, au fil de SA vie.
  const std::array<float, 2> drift = ArrivalDriftAt(elapsed_ms / life_ms, look);
  // Où la marque se pose, APRÈS l'échelle : le départ, la montée et la dérive la
  // déplacent sans l'étirer, et l'axe Y d'un sprite DESCEND — ce qui monte se
  // retranche.
  const glm::vec2 landing{look.start_x_pixels + drift[0], -(rise + drift[1])};
  // Le mot pâlit depuis l'opacité de naissance, l'étoile depuis le plein ; la
  // bulle ne pâlit pas.
  const float faded = still  ? std::clamp(kInitialAlpha - kMissFadePerTick * ticks, 0.0F, kOpaque)
                      : star ? std::clamp(kOpaque - kStarFadePerTick * ticks, 0.0F, kOpaque)
                      : rising_star ? RisingStarAlphaAt(ticks)
                                    : kOpaque;
  const auto alpha = static_cast<std::uint8_t>(std::clamp(faded * look.opacity, 0.0F, kOpaque));
  // La teinte de l'étoile d'un coup de pied est celle de son GENRE, et non de
  // qui la demande : le gris de la naissance, puis le bleu pâle que chaque tic
  // repose.
  const std::array<std::uint8_t, 3> rgb =
      rising_star ? (ticks > 0.0F ? kMarkOnRisingStar : kMarkOnStar) : color;
  const std::array<std::uint8_t, 4> tint = {rgb[0], rgb[1], rgb[2], alpha};
  // LES ÉTOILES PORTENT LEUR PROPRE TAILLE, grandie à la naissance comme le
  // nombre qui les porte ; les deux autres n'ont que celle du réglage.
  const NumberLife birth{.birth_scale = pace.birth_ratio, .birth_settle_ms = pace.birth_settle_ms};
  const float scale = look.scale * SettlingScaleAt(elapsed_ms, birth) *
                      (star          ? StarScaleAt(ticks)
                       : rising_star ? RisingStarScaleAt(ticks)
                                     : 1.0F);
  for (SpriteQuad quad : BuildFrameQuads(sprite, animation, index, frame)) {
    for (glm::vec2& corner : quad.corners) {
      corner = corner * scale + landing;
    }
    quad.tint = tint;
    composed.sources.push_back(SpriteImageKey{&sprite, nullptr, quad.section, quad.image_index});
    composed.quads.push_back(quad);
  }
  return composed;
}

}  // namespace ro::render
