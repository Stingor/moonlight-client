#include "ro/render/led_pose.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "ro/render/sprite_catalog.h"

namespace ro::render {
namespace {

// L'action d'incantation, sur laquelle l'emploi de ces métiers mène sa pose.
constexpr std::uint8_t kUseLedRank = 12;
constexpr std::uint8_t kUseLedPeriodTicks = 4;

struct UseLedFamily {
  std::array<std::uint32_t, 12> jobs;
  std::size_t job_count;
  std::uint8_t first_frame;
  std::uint8_t last_frame;
  std::uint32_t hold_ms;
};

constexpr std::array<UseLedFamily, 3> kUseLedFamilies = {{
    {{4020, 4021, 4015, 4066, 4073, 15, 4016, 4038, 4070, 4077, 4106, 4262}, 12, 0, 0, 400},
    {{25, 4222}, 2, 2, 5, 1000},
    {{24, 4228}, 2, 0, 3, 1000},
}};

// UNE IMAGE DE LA POSE : son numéro, et son rang quand ce n'est pas celui que
// l'état pose.
constexpr std::int8_t kRankOfState = -1;
struct LedStep {
  std::uint8_t frame = 0;
  std::int8_t rank = kRankOfState;
};

// Ce qui choisit entre deux poses d'un même état : le tirage au sort (38), ou
// le métier montré (26).
enum class LedVariant : std::uint8_t {
  kAlways,
  kDrawZero,
  kDrawOne,
  kSoulLinkerLine,
  kOtherJobs,
};

// LA POSE QU'UN ÉTAT MÈNE : une suite d'images, une toutes les `period_ticks`
// avances, la dernière tenue — ou la suite reprise, quand elle tourne. Le
// client en donne deux formes : une plage d'images d'un même rang, et une liste
// de cinq couples (rang, image) ; toutes deux s'écrivent ici en liste.
struct StateLedPose {
  std::int8_t state = 0;
  LedVariant variant = LedVariant::kAlways;
  std::array<LedStep, 8> steps{};
  std::size_t step_count = 0;
  bool loops = false;
  // Vrai quand le client ajoute son tirage au sort, zéro ou un, à l'image.
  bool drawn = false;
  std::uint32_t period_ticks = 0;
  std::uint32_t hold_ms = 0;
};

// Les durées « sans fin » du client sont des durées, écrites telles quelles.
constexpr std::uint32_t kEndlessLoopMs = 99'999'999;
constexpr std::uint32_t kEndlessHoldMs = 9'999'999;
// Les deux rangs qu'une pose impose au lieu de celui de son état.
constexpr std::int8_t kWalkRank = 1;
constexpr std::int8_t kAttackRank = 10;

// La lignée du Soul Linker, à qui l'état 26 donne ses propres images.
constexpr std::array<std::uint32_t, 4> kSoulLinkerLineJobs = {4049, 4227, 4240, 4242};

// Triée par état. Seize fait l'aller-retour 1, 2, 3, 2 : arrivé au bout de sa
// plage, le client repart d'un cran après la première image, puis en recule
// d'un — parce que sa pose tenue est l'action zéro. Dix-sept et quarante-sept,
// qui tiennent leur propre action, reprennent à la première.
constexpr std::array<StateLedPose, 39> kStateLedPoses = {{
    {16, LedVariant::kAlways, {{{1}, {2}, {3}, {2}}}, 4, true, false, 10, kEndlessLoopMs},
    {17, LedVariant::kAlways, {{{1}, {2}, {3}}}, 3, true, false, 10, kEndlessLoopMs},
    {18, LedVariant::kAlways, {{{0}}}, 1, false, false, 10, 3000},
    {kLedCastState,
     LedVariant::kAlways,
     {{{0}, {0}, {0}, {1}, {1}}},
     5,
     false,
     false,
     10,
     kLedCastPoseMs},
    {20, LedVariant::kAlways, {{{1}, {1}, {1}, {1}, {5, kWalkRank}}}, 5, false, false, 10, 3000},
    {21, LedVariant::kAlways, {{{0}}}, 1, false, false, 10, 2000},
    {22, LedVariant::kAlways, {{{2}}}, 1, false, false, 10, 2000},
    {23, LedVariant::kAlways, {{{3}}}, 1, false, false, 10, 2000},
    {24, LedVariant::kAlways, {{{4}}}, 1, false, false, 10, 2000},
    {25, LedVariant::kAlways, {{{1}}}, 1, false, false, 10, 2000},
    {26, LedVariant::kSoulLinkerLine, {{{4}, {5}, {6}, {7}, {2}}}, 5, false, false, 4, 500},
    {26, LedVariant::kOtherJobs, {{{2}, {3}, {4}, {4}, {4}}}, 5, false, false, 4, 500},
    {27, LedVariant::kAlways, {{{5}}}, 1, false, false, 10, 3000},
    {28, LedVariant::kAlways, {{{0}, {1}, {2}, {3}, {4}}}, 5, false, false, 10, 1000},
    {30, LedVariant::kAlways, {{{1}}}, 1, false, false, 10, 2000},
    {31,
     LedVariant::kAlways,
     {{{3, kAttackRank}, {5, kAttackRank}, {7, kAttackRank}, {8, kAttackRank}, {2, kAttackRank}}},
     5,
     false,
     false,
     5,
     500},
    {32, LedVariant::kAlways, {{{4}, {5}, {6}, {7}, {2}}}, 5, false, false, 5, 500},
    {33, LedVariant::kAlways, {{{2}}}, 1, false, false, 10, 1000},
    {34, LedVariant::kAlways, {{{3}}}, 1, false, false, 10, 1000},
    {35, LedVariant::kAlways, {{{0}, {1}}}, 2, false, false, 6, 500},
    {36, LedVariant::kAlways, {{{2}, {3}, {4}, {5}}}, 4, false, false, 6, 1000},
    {37, LedVariant::kAlways, {{{2}}}, 1, false, true, 10, 500},
    {38, LedVariant::kDrawZero, {{{0}, {1}, {2}, {3}}}, 4, false, false, 6, 1000},
    {38, LedVariant::kDrawOne, {{{3}, {2}, {1}, {0}, {0}}}, 5, false, false, 6, 1000},
    {39, LedVariant::kAlways, {{{4}, {5}}}, 2, false, false, 6, 500},
    {40, LedVariant::kAlways, {{{0}, {1}, {2}}}, 3, false, false, 6, 500},
    {41, LedVariant::kAlways, {{{1}}}, 1, false, true, 10, 500},
    {42, LedVariant::kAlways, {{{0}}}, 1, false, false, 10, 500},
    {43, LedVariant::kAlways, {{{5}}}, 1, false, false, 10, 500},
    {44, LedVariant::kAlways, {{{4}}}, 1, false, false, 10, 500},
    {46, LedVariant::kAlways, {{{0}, {0}, {0}, {1}, {1}}}, 5, false, false, 20, 3000},
    {47, LedVariant::kAlways, {{{0}, {1}, {2}, {3}}}, 4, true, false, 6, kEndlessHoldMs},
    {48, LedVariant::kAlways, {{{4}, {5}}}, 2, false, false, 6, 500},
    {49, LedVariant::kAlways, {{{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}}}, 8, false, false, 6, 1000},
    {50, LedVariant::kAlways, {{{0}}}, 1, false, false, 10, kEndlessHoldMs},
    {51, LedVariant::kAlways, {{{2}, {3}, {4}, {4}, {4}}}, 5, false, false, 4, 500},
    {55, LedVariant::kAlways, {{{2}, {3}, {4}}}, 3, false, false, 6, 500},
    {56, LedVariant::kAlways, {{{0}, {1}}}, 2, false, false, 6, 500},
    {57, LedVariant::kAlways, {{{5}}}, 1, false, false, 6, 500},
}};
static_assert(std::ranges::is_sorted(kStateLedPoses, {}, &StateLedPose::state),
              "la recherche des poses d'état est dichotomique");
static_assert(std::ranges::all_of(kStateLedPoses,
                                  [](const StateLedPose& pose) {
                                    return pose.step_count > 0 &&
                                           pose.step_count <= pose.steps.size() &&
                                           pose.period_ticks > 0 && pose.hold_ms > 0;
                                  }),
              "une pose d'état montre au moins une image, et avance");

// La variante que ce tirage et ce métier choisissent.
bool VariantApplies(LedVariant variant, std::uint8_t draw, std::uint32_t job) noexcept {
  const bool soul_linker_line =
      std::ranges::find(kSoulLinkerLineJobs, job) != kSoulLinkerLineJobs.end();
  switch (variant) {
    case LedVariant::kAlways:
      return true;
    case LedVariant::kDrawZero:
      return (draw & 1U) == 0;
    case LedVariant::kDrawOne:
      return (draw & 1U) != 0;
    case LedVariant::kSoulLinkerLine:
      return soul_linker_line;
    case LedVariant::kOtherJobs:
      return !soul_linker_line;
  }
  return false;
}

const StateLedPose* StateLedPoseOf(std::int8_t action_state, std::uint8_t draw,
                                   std::uint32_t job) noexcept {
  const auto [first, last] =
      std::ranges::equal_range(kStateLedPoses, action_state, {}, &StateLedPose::state);
  for (auto pose = first; pose != last; ++pose) {
    if (VariantApplies(pose->variant, draw, job)) {
      return &*pose;
    }
  }
  return nullptr;
}

// 🔴 LES ÉTATS QUE LA FIN RAMÈNE AU REPOS. Le pas du monde du client repose
// l'attente dès qu'il voit l'action finie sous 0, 2, 4, 5, 7, 9, 12, 45, 52 à
// 54 et 56 à 59 ; 49 est inscrit comme l'emploi ordinaire, et 51 passe à
// l'attaque, qui se repose au pas suivant.
constexpr std::array<std::int8_t, 17> kStatesRestingWhenFinished = {
    0, 2, 4, 5, 7, 9, 12, 45, 49, 51, 52, 53, 54, 56, 57, 58, 59};

// LES GESTES D'EFFET DES ÉTATS, et leur place dans le client : 19 et 46
// retirent puis sèment, 21 à 25 sèment.
constexpr std::array<StateEffectStep, 4> kLedCastStateEffects = {{
    {StateEffectKind::kRemove, 446},
    {StateEffectKind::kRemove, 445},
    {StateEffectKind::kSpawn, 445},
    {StateEffectKind::kSpawn, 411},
}};
constexpr std::array<StateEffectStep, 1> kReadyStateEffects = {{
    {StateEffectKind::kSpawn, 449},
}};
constexpr std::array<StateEffectStep, 4> kFallenAngelStateEffects = {{
    {StateEffectKind::kRemove, 446},
    {StateEffectKind::kRemove, 1072},
    {StateEffectKind::kSpawn, 1072},
    {StateEffectKind::kSpawn, 411},
}};
constexpr std::int8_t kFirstReadyState = 21;
constexpr std::int8_t kLastReadyState = 25;
constexpr std::int8_t kFallenAngelState = 46;

// L'état qui s'inscrit comme l'emploi ordinaire, et celui qui finit en attaque.
constexpr std::int8_t kStateStoredAsUse = 49;
constexpr std::int8_t kStateEndingAsAttack = 51;
constexpr std::int8_t kUseState = 7;
constexpr std::int8_t kAttackState = 2;

}  // namespace

std::optional<SkillHeldPose> UseLedPoseOf(std::uint32_t job) noexcept {
  for (const UseLedFamily& family : kUseLedFamilies) {
    const auto fin = family.jobs.begin() + static_cast<std::ptrdiff_t>(family.job_count);
    if (std::find(family.jobs.begin(), fin, job) == fin) {
      continue;
    }
    SkillHeldPose pose;
    pose.play_rank = kUseLedRank;
    pose.first_frame = family.first_frame;
    pose.last_frame = family.last_frame;
    pose.hold_rank = kUseLedRank;
    pose.hold_frame = family.last_frame;
    pose.timed = true;
    pose.hold_ms = family.hold_ms;
    pose.period_ticks = kUseLedPeriodTicks;
    return pose;
  }
  return std::nullopt;
}

bool StateLeadsPose(std::int8_t action_state) noexcept {
  return std::ranges::binary_search(kStateLedPoses, action_state, {}, &StateLedPose::state);
}

std::optional<StateLedFrame> StateLedFrameAt(std::int8_t action_state, std::uint8_t rank,
                                             std::uint32_t elapsed_ms, std::uint32_t tick_ms,
                                             std::uint8_t draw, std::uint32_t job) noexcept {
  const StateLedPose* pose = StateLedPoseOf(action_state, draw, job);
  if (pose == nullptr || tick_ms == 0) {
    return std::nullopt;
  }
  // Passé la durée, le client cesse d'avancer l'image : celle de la dernière
  // avance reste.
  const bool running = elapsed_ms < pose->hold_ms;
  const std::uint32_t shown_ms = running ? elapsed_ms : pose->hold_ms - 1;
  const std::size_t step = shown_ms / tick_ms / pose->period_ticks;
  const std::size_t index =
      pose->loops ? step % pose->step_count : std::min(step, pose->step_count - 1);
  const LedStep& shown = pose->steps[index];
  const auto frame = static_cast<std::uint8_t>(shown.frame + (pose->drawn ? (draw & 1U) : 0U));
  const std::uint8_t shown_rank =
      shown.rank == kRankOfState ? rank : static_cast<std::uint8_t>(shown.rank);
  return StateLedFrame{HeldPoseFrame{shown_rank, frame}, running};
}

bool StateRestsWhenFinished(std::int8_t action_state) noexcept {
  return std::ranges::find(kStatesRestingWhenFinished, action_state) !=
         kStatesRestingWhenFinished.end();
}

std::int8_t StateLeftByFinishedMotion(std::int8_t action_state) noexcept {
  if (action_state == kStateStoredAsUse) {
    return kUseState;
  }
  if (action_state == kStateEndingAsAttack) {
    return kAttackState;
  }
  return action_state;
}

std::span<const StateEffectStep> StateEffectsOf(std::int8_t action_state,
                                                std::uint32_t job) noexcept {
  if (action_state == kLedCastState) {
    if (IsDoramJob(job)) {
      return {};
    }
    return kLedCastStateEffects;
  }
  if (action_state >= kFirstReadyState && action_state <= kLastReadyState) {
    return kReadyStateEffects;
  }
  if (action_state == kFallenAngelState) {
    return kFallenAngelStateEffects;
  }
  return {};
}

}  // namespace ro::render
