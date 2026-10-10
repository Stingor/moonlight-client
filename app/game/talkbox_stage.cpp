#include "game/talkbox_stage.h"

#include "ro/net/skill.h"
#include "ro/sim/skill_book.h"
#include "ro/vfs/talkbox_words.h"

namespace moonclient {

void TalkboxStage::Stage(const ro::vfs::ItemWords& words, ro::ui::TalkboxWindowState& state,
                         ro::ui::TalkboxView& view) const {
  state.open = pending_.has_value();
  if (!pending_.has_value()) {
    view = ro::ui::TalkboxView{};
    return;
  }
  // GRAFFITI garde la ligne de Talkie Box, mais perd le titre et la longueur.
  if (pending_->skill_id == ro::sim::kGraffitiSkill) {
    view = ro::ui::TalkboxView{.prompt = ro::vfs::TalkboxPrompt(words),
                               .field_chars = ro::ui::kGraffitiFieldChars};
    return;
  }
  view = ro::ui::TalkboxView{.title = ro::vfs::TalkboxTitle(words),
                             .prompt = ro::vfs::TalkboxPrompt(words)};
}

ro::core::Result<void> TalkboxStage::Apply(const ro::ui::TalkboxWindowAction& asked,
                                           ro::net::Connection& link) {
  if (!pending_.has_value()) {
    return {};
  }
  if (asked.confirmed.has_value()) {
    const GroundCast cast = *pending_;
    pending_.reset();
    return ro::net::RequestSkillOnGroundWithText(link, cast.skill_id, cast.skill_level, cast.cell,
                                                 *asked.confirmed);
  }
  if (asked.cancelled) {
    pending_.reset();
  }
  return {};
}

}  // namespace moonclient
