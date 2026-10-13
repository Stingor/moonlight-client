#include "ro/render/doll_animation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <optional>

#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/gif.h"
#include "ro/render/sprite_canvas.h"

namespace ro::render {
namespace {

// Les millisecondes d'un centième de seconde.
constexpr float kMsPerCentisecond = 10.0F;
// L'écart en deçà duquel deux instants n'en font qu'un, et un tour « tombe »
// sur une milliseconde entière, en millisecondes.
constexpr float kSameInstantMs = 0.5F;
// 🔴 L'INSTANT RENDU EST POUSSÉ D'UN QUART DE MILLISECONDE. Un instant planifié
// tombe pile sur un changement d'image, et `k × pas` calculé en flottant peut
// valoir un rien de moins que le seuil : l'image serait alors la précédente.
constexpr float kSampleNudgeMs = 0.25F;
// L'opacité pleine d'un canal.
constexpr std::uint32_t kOpaque = 255;

// Une horloge ramenée à l'essentiel : la durée d'un pas, et combien de pas
// font un tour.
struct Cycle {
  float step_ms = 0.0F;
  std::size_t steps = 0;

  [[nodiscard]] float length_ms() const noexcept { return step_ms * static_cast<float>(steps); }
};

// 🔴 UN PAS QU'ON CROIT : fini, ni nul ni démesuré. Une cadence infinie
// donnerait une boucle infinie, une nulle une division par zéro, une énorme un
// débordement des centièmes. Voir `kShortestDollStepMs`.
[[nodiscard]] bool Believable(float step_ms) {
  return std::isfinite(step_ms) && step_ms >= kShortestDollStepMs && step_ms <= kLongestDollStepMs;
}

// Le corps joue-t-il vraiment ? Une seule image, ou une cadence aberrante, le
// tiennent sur son image zéro.
[[nodiscard]] bool BodyPlays(const DollClocks& clocks) {
  return clocks.body_plays && clocks.body_frames > 1 && Believable(clocks.body_frame_ms);
}

[[nodiscard]] std::vector<Cycle> CyclesOf(const DollClocks& clocks) {
  std::vector<Cycle> cycles;
  if (BodyPlays(clocks)) {
    cycles.push_back(Cycle{.step_ms = clocks.body_frame_ms, .steps = clocks.body_frames});
  }
  for (const AlternateClock& clock : clocks.attachments) {
    if (clock.multiple > 1 && Believable(clock.step_ms())) {
      cycles.push_back(Cycle{.step_ms = clock.step_ms(), .steps = clock.multiple});
    }
  }
  return cycles;
}

// Le délai d'une image seule : la boucle n'a qu'elle.
[[nodiscard]] DollTimeline SingleFrame() {
  return DollTimeline{.sample_ms = {0.0F}, .delays_cs = {0}};
}

// Le plus petit commun multiple des tours, en millisecondes entières ; rien si
// un tour ne tombe pas sur une milliseconde entière ou si la boucle dépasse
// `kLongestDollLoopMs`.
[[nodiscard]] std::optional<long long> CommonLoopMs(const std::vector<Cycle>& cycles) {
  long long loop = 1;
  const auto longest = static_cast<long long>(kLongestDollLoopMs);
  for (const Cycle& cycle : cycles) {
    const float length = cycle.length_ms();
    const long long whole = std::llround(length);
    if (whole <= 0 || std::abs(length - static_cast<float>(whole)) > kSameInstantMs) {
      return std::nullopt;
    }
    loop = std::lcm(loop, whole);
    if (loop > longest) {
      return std::nullopt;
    }
  }
  return loop;
}

// Chaque changement d'image d'une horloge, dans `[0, loop_ms)`, sans doublon.
[[nodiscard]] std::vector<float> StepsWithin(const std::vector<Cycle>& cycles, float loop_ms) {
  std::vector<float> instants;
  for (const Cycle& cycle : cycles) {
    for (std::size_t step = 0;; ++step) {
      const float at = cycle.step_ms * static_cast<float>(step);
      if (at >= loop_ms - kSameInstantMs) {
        break;
      }
      instants.push_back(at);
    }
  }
  std::sort(instants.begin(), instants.end());
  const auto end = std::unique(instants.begin(), instants.end(), [](float left, float right) {
    return right - left < kSameInstantMs;
  });
  instants.erase(end, instants.end());
  return instants;
}

[[nodiscard]] int CentisecondsAt(float ms) {
  return static_cast<int>(std::lround(ms / kMsPerCentisecond));
}

// Les délais d'une suite d'instants qui boucle à `loop_ms`, cumulés sans
// dérive ; les instants trop proches de leur prédécesseur sont écartés.
[[nodiscard]] DollTimeline Spaced(const std::vector<float>& instants, float loop_ms) {
  if (instants.empty()) {
    return SingleFrame();
  }
  DollTimeline timeline;
  for (const float at : instants) {
    if (!timeline.sample_ms.empty() &&
        CentisecondsAt(at) - CentisecondsAt(timeline.sample_ms.back()) <
            formats::kFastestGifDelayCs) {
      continue;
    }
    timeline.sample_ms.push_back(at);
  }
  const int end_cs = CentisecondsAt(loop_ms);
  // ⚠ LA DERNIÈRE IMAGE AUSSI DOIT DURER : trop près de la fin, elle cède sa
  // place à la reprise.
  while (timeline.sample_ms.size() > 1 &&
         end_cs - CentisecondsAt(timeline.sample_ms.back()) < formats::kFastestGifDelayCs) {
    timeline.sample_ms.pop_back();
  }
  for (std::size_t at = 0; at < timeline.sample_ms.size(); ++at) {
    const int next_cs =
        at + 1 < timeline.sample_ms.size() ? CentisecondsAt(timeline.sample_ms[at + 1]) : end_cs;
    timeline.delays_cs.push_back(
        std::max(next_cs - CentisecondsAt(timeline.sample_ms[at]), formats::kFastestGifDelayCs));
  }
  return timeline;
}

// LA COULEUR VRAIE d'un pixel fondu sur du noir transparent : la couleur fondue
// divisée par l'opacité.
void Unpremultiply(formats::Image& image) {
  for (std::size_t at = 0; at + 3 < image.pixels.size(); at += 4) {
    const std::uint32_t alpha = image.pixels[at + 3];
    if (alpha == 0 || alpha == kOpaque) {
      continue;
    }
    for (std::size_t channel = 0; channel < 3; ++channel) {
      const std::uint32_t straight = image.pixels[at + channel] * kOpaque / alpha;
      image.pixels[at + channel] = static_cast<std::uint8_t>(std::min(straight, kOpaque));
    }
  }
}

// Agrandit au plus proche voisin, et entoure de la marge.
[[nodiscard]] formats::Image Enlarged(const formats::Image& sprite, int scale, int margin) {
  formats::Image out;
  out.width = sprite.width * scale + 2 * margin;
  out.height = sprite.height * scale + 2 * margin;
  out.pixels.assign(static_cast<std::size_t>(out.width) * static_cast<std::size_t>(out.height) * 4U,
                    0);
  for (int y = 0; y < out.height - 2 * margin; ++y) {
    const int source_y = y / scale;
    for (int x = 0; x < out.width - 2 * margin; ++x) {
      const int source_x = x / scale;
      const std::size_t from =
          (static_cast<std::size_t>(source_y) * static_cast<std::size_t>(sprite.width) +
           static_cast<std::size_t>(source_x)) *
          4U;
      const std::size_t to =
          (static_cast<std::size_t>(y + margin) * static_cast<std::size_t>(out.width) +
           static_cast<std::size_t>(x + margin)) *
          4U;
      std::copy_n(sprite.pixels.begin() + static_cast<std::ptrdiff_t>(from), 4,
                  out.pixels.begin() + static_cast<std::ptrdiff_t>(to));
    }
  }
  return out;
}

}  // namespace

DollClocks DollClocksOf(const ActorOutfit& outfit, std::size_t animation, std::size_t direction,
                        bool body_plays) {
  DollClocks clocks;
  clocks.body_plays = body_plays;
  clocks.body_frames = outfit.BodyActionFrameCount(animation, direction);
  clocks.body_frame_ms =
      outfit.BodyActionDelayUnits(animation, direction) * formats::kMillisecondsPerSpeedUnit;
  clocks.attachments = outfit.OwnClocks(animation);
  return clocks;
}

DollTimeline PlanDollTimeline(const DollClocks& clocks) {
  const std::vector<Cycle> cycles = CyclesOf(clocks);
  if (cycles.empty()) {
    return SingleFrame();
  }

  if (const std::optional<long long> loop = CommonLoopMs(cycles); loop.has_value()) {
    const auto loop_ms = static_cast<float>(*loop);
    std::vector<float> instants = StepsWithin(cycles, loop_ms);
    if (instants.size() <= kMostDollGifFrames) {
      return Spaced(instants, loop_ms);
    }
  }

  // LE REPLI : un seul tour, celui du corps quand il joue — il est toujours le
  // premier de la liste —, sinon le plus long de ceux des pièces.
  Cycle kept = cycles.front();
  if (!BodyPlays(clocks)) {
    kept = *std::max_element(
        cycles.begin(), cycles.end(),
        [](const Cycle& left, const Cycle& right) { return left.length_ms() < right.length_ms(); });
  }
  kept.steps = std::min(kept.steps, kMostDollGifFrames);
  const std::vector<Cycle> alone = {kept};
  return Spaced(StepsWithin(alone, kept.length_ms()), kept.length_ms());
}

std::vector<DollProbe> DollEnvelopeProbes(const DollClocks& clocks, std::size_t rest_frame) {
  // Les instants des pièces : chaque pas de chaque tour, sur sa longueur.
  std::vector<float> instants = {0.0F};
  for (const AlternateClock& clock : clocks.attachments) {
    if (clock.multiple > 1 && Believable(clock.step_ms())) {
      for (std::size_t step = 1; step < clock.multiple; ++step) {
        instants.push_back(clock.step_ms() * static_cast<float>(step));
      }
    }
  }
  std::sort(instants.begin(), instants.end());
  instants.erase(std::unique(instants.begin(), instants.end()), instants.end());

  const bool plays = BodyPlays(clocks);
  const std::size_t body_frames = plays ? clocks.body_frames : 1;
  std::vector<DollProbe> probes;
  probes.reserve(body_frames * instants.size());
  for (std::size_t frame = 0; frame < body_frames; ++frame) {
    for (const float at_ms : instants) {
      probes.push_back(DollProbe{.at_ms = at_ms, .body_frame = plays ? frame : rest_frame});
    }
  }
  return probes;
}

const ComposedActor& ComposeDollProbe(ActorOutfit& outfit, std::size_t animation,
                                      std::size_t direction, const DollProbe& probe) {
  return outfit.Composed(animation, direction, probe.at_ms + kSampleNudgeMs, /*loop=*/true,
                         probe.body_frame);
}

core::Result<DollPoses> ComposeDollPoses(ActorOutfit& outfit, const DollShot& shot,
                                         const DollTimeline& timeline) {
  const DollClocks clocks = DollClocksOf(outfit, shot.animation, shot.direction, shot.body_plays);
  DollPoses composed;
  // ⚠ DES COPIES : la tenue garde ses compositions, mais rien n'oblige à
  // parier sur la durée de vie de ses références pendant quarante appels.
  std::vector<ComposedActor>& poses = composed.poses;
  poses.reserve(timeline.sample_ms.size());
  for (const float at : timeline.sample_ms) {
    const float elapsed_ms = at + kSampleNudgeMs;
    std::size_t held = 0;
    if (BodyPlays(clocks)) {
      held = static_cast<std::size_t>(elapsed_ms / clocks.body_frame_ms) % clocks.body_frames;
    }
    poses.push_back(
        outfit.Composed(shot.animation, shot.direction, elapsed_ms, /*loop=*/true, held));
  }

  bool grown = false;
  SpriteBounds envelope;
  for (const ComposedActor& pose : poses) {
    if (pose.quads.empty()) {
      continue;
    }
    const SpriteBounds bounds = ComputeBounds(pose.quads);
    envelope.min = grown ? glm::min(envelope.min, bounds.min) : bounds.min;
    envelope.max = grown ? glm::max(envelope.max, bounds.max) : bounds.max;
    grown = true;
  }
  if (!grown) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "la tenue ne peint rien");
  }
  const glm::vec2 low = glm::floor(envelope.min);
  const glm::vec2 high = glm::ceil(envelope.max);
  const glm::vec2 extent = high - low;
  // ⚠ COMPARÉE EN FLOTTANT, AVANT LA CONVERSION : une enveloppe démesurée ne
  // tiendrait pas dans un entier.
  constexpr auto kLargestSide = static_cast<float>(kLargestDollSpritePx);
  if (!(extent.x > 0.0F && extent.y > 0.0F && extent.x <= kLargestSide &&
        extent.y <= kLargestSide)) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "enveloppe du pantin hors bornes");
  }
  composed.low = low;
  composed.width = static_cast<int>(extent.x);
  composed.height = static_cast<int>(extent.y);
  return composed;
}

std::size_t DollGifPixels(const DollPoses& poses, const DollShot& shot) {
  const int scale = std::clamp(shot.scale, kSmallestDollGifScale, kLargestDollGifScale);
  const int margin = std::max(shot.margin_px, 0);
  const auto width = static_cast<std::size_t>(poses.width * scale + 2 * margin);
  const auto height = static_cast<std::size_t>(poses.height * scale + 2 * margin);
  return width * height * poses.poses.size();
}

core::Result<std::vector<formats::Image>> DrawDollFrames(const DollPoses& poses,
                                                         const DollShot& shot) {
  if (poses.poses.empty() || poses.width <= 0 || poses.height <= 0) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "aucune pose à dessiner");
  }
  if (DollGifPixels(poses, shot) > kMostDollGifPixels) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "GIF du pantin trop grand");
  }
  const int scale = std::clamp(shot.scale, kSmallestDollGifScale, kLargestDollGifScale);
  const int margin = std::max(shot.margin_px, 0);
  std::vector<formats::Image> frames;
  frames.reserve(poses.poses.size());
  for (const ComposedActor& pose : poses.poses) {
    SpriteCanvas canvas(poses.width, poses.height);
    canvas.Draw(pose.quads, ResolveImages(pose), -poses.low);
    formats::Image sprite = canvas.image();
    Unpremultiply(sprite);
    frames.push_back(Enlarged(sprite, scale, margin));
  }
  return frames;
}

}  // namespace ro::render
