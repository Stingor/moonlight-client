#include "ro/sim/survivors_play.h"

#include <cstdint>
#include <utility>

namespace ro::sim {

namespace {

constexpr std::uint32_t kPerMille = 1000;

}  // namespace

SurvivorsPlay::SurvivorsPlay(SurvivorsRun run) : run_(std::move(run)) {
  // Le journal porte le départ et la version de règles dès la naissance de la
  // partie : c'est ce qui le rend rejouable.
  journal_.ruleset_version = run_.field().rules->ruleset_version;
  journal_.start = run_.field().start;
  journal_.final_step = run_.field().step;
}

std::uint32_t SurvivorsPlay::Advance(std::uint32_t elapsed_ms, SurvivorsInput input) {
  if (run_.field().phase != SurvivorsPhase::kRunning) {
    pending_ms_ = 0;
    return 0;
  }
  // En 64 bits : un écart énorme ne déborde pas avant d'être plafonné.
  const std::uint64_t pending = std::uint64_t{pending_ms_} + elapsed_ms;
  std::uint64_t due = pending / kSurvivorsStepMs;
  if (due > kMostCatchUpSteps) {
    due = kMostCatchUpSteps;
    pending_ms_ = static_cast<std::uint32_t>(pending % kSurvivorsStepMs);
  } else {
    pending_ms_ = static_cast<std::uint32_t>(pending - due * kSurvivorsStepMs);
  }

  const std::uint8_t heading = input.heading == kNoHeading
                                   ? kNoHeading
                                   : static_cast<std::uint8_t>(input.heading % kHeadingSteps);
  std::uint32_t done = 0;
  while (done < due && run_.field().phase == SurvivorsPhase::kRunning) {
    if (heading != last_heading_) {
      journal_.entries.push_back(
          JournalEntry{.step = run_.field().step, .kind = JournalKind::kHeading, .value = heading});
      last_heading_ = heading;
    }
    run_.Step(SurvivorsInput{.heading = heading});
    ++done;
  }
  journal_.final_step = run_.field().step;
  // Un choix ouvert ou une partie finie figent l'horloge : le temps dû au-delà
  // ne se rattrape pas.
  if (run_.field().phase != SurvivorsPhase::kRunning) pending_ms_ = 0;
  return done;
}

bool SurvivorsPlay::Answer(SurvivorsAnswer answer) {
  const std::uint32_t step = run_.field().step;
  if (!run_.Answer(answer)) return false;
  journal_.entries.push_back(
      JournalEntry{.step = step, .kind = JournalKind::kAnswer, .value = PackAnswer(answer)});
  journal_.final_step = run_.field().step;
  return true;
}

void SurvivorsPlay::Abandon() {
  if (run_.field().phase == SurvivorsPhase::kOver) return;
  journal_.entries.push_back(
      JournalEntry{.step = run_.field().step, .kind = JournalKind::kAbandon, .value = 0});
  run_.Abandon();
  journal_.final_step = run_.field().step;
}

std::uint32_t SurvivorsPlay::blend_per_mille() const noexcept {
  return pending_ms_ * kPerMille / kSurvivorsStepMs;
}

}  // namespace ro::sim
