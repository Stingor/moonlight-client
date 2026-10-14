#include "character/status_window_view.h"

namespace moonclient {

ro::ui::StatusWindowView StatusWindowViewOf(const ro::ui::CharacterStatsView& stats,
                                            std::string_view guild_name) {
  ro::ui::StatusWindowView view;
  view.attributes = stats.attributes;
  view.status_point = stats.status_point;
  view.atk1 = stats.atk1;
  view.atk2 = stats.atk2;
  view.matk_min = stats.matk2;
  view.matk_max = stats.matk1;
  view.def1 = stats.def1;
  view.def2 = stats.def2;
  view.mdef1 = stats.mdef1;
  view.mdef2 = stats.mdef2;
  view.hit = stats.hit;
  view.flee = stats.flee1;
  view.perfect_dodge = stats.flee2;
  view.critical = stats.critical;
  view.aspd = stats.aspd;
  view.guild_name = guild_name;
  return view;
}

std::string GuildNameOf(const ro::sim::GuildBook& book) {
  if (!book.in_guild()) {
    return {};
  }
  if (book.summary().has_value()) {
    return book.summary()->name;
  }
  return book.belonging()->name;
}

ro::ui::CharacterWindowAction SheetActionOf(const ro::ui::StatusWindowAction& asked) {
  ro::ui::CharacterWindowAction sheet;
  if (asked.verb == ro::ui::StatusVerb::kRaiseAttribute) {
    sheet.verb = ro::ui::CharacterVerb::kRaiseAttribute;
    sheet.attribute = asked.attribute;
    sheet.amount = asked.amount;
  }
  return sheet;
}

}  // namespace moonclient
