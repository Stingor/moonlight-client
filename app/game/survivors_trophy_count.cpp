#include "game/survivors_trophy_count.h"

#include "survivors/survivors_door.h"

namespace moonclient {

void SurvivorsTrophyCount::FollowEntered(const SurvivorsStage& stage, MinigameStage* minigames,
                                         const std::filesystem::path& client_root) {
  if (stage.entered() != SurvivorsScreen::kSummary) return;
  Count(stage, minigames, client_root);
}

bool SurvivorsTrophyCount::Freeze(SurvivorsStage& stage, MinigameStage* minigames,
                                  const std::filesystem::path& client_root) {
  if (!FreezeAndSave(stage, client_root)) return false;
  Count(stage, minigames, client_root);
  return true;
}

void SurvivorsTrophyCount::Count(const SurvivorsStage& stage, MinigameStage* minigames,
                                 const std::filesystem::path& client_root) {
  if (counted_ || minigames == nullptr || stage.play() == nullptr) return;
  won_ = minigames->RecordSurvivorsRun(stage.play()->run(), client_root);
  counted_ = true;
}

}  // namespace moonclient
