#include "options/talk_filter_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs le réglage
// d'origine, sans que rien ne le dise.
constexpr const char* kTalkHiddenKey = "chat_talk_hidden";
constexpr const char* kTalkAndGuildHiddenKey = "chat_talk_and_guild_hidden";

}  // namespace

void TalkFilterOptions::Declare(SettingsFile& into) {
  into.Declare(kTalkHiddenKey, &mute_.talk_hidden);
  into.Declare(kTalkAndGuildHiddenKey, &mute_.talk_and_guild_hidden);
}

void TalkFilterOptions::ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const {
  rows.push_back(ro::ui::OptionRowView{
      .id = IdOf(TalkFilterRow::kTalkHidden),
      .label = Tr("Taire la parole des autres dans la conversation"),
      .help = Tr("La parole publique des autres joueurs, celle du champ de bataille et les "
                 "répliques des pets n'entrent plus dans la conversation. Leur bulle "
                 "reste au-dessus de leur tête."),
      .command = "/notalkmsg",
      .kind = ro::ui::OptionKind::kToggle,
      .on = mute_.talk_hidden});
  rows.push_back(ro::ui::OptionRowView{
      .id = IdOf(TalkFilterRow::kTalkAndGuildHidden),
      .label = Tr("Taire aussi la guilde et le clan"),
      .help = Tr("Comme la case précédente, et la parole de guilde et celle du clan se taisent "
                 "aussi. Ni le groupe, ni les chuchotements ne sont touchés."),
      .command = "/notalkmsg2",
      .kind = ro::ui::OptionKind::kToggle,
      .on = mute_.talk_and_guild_hidden});
}

bool TalkFilterOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kTalkFilterRowBase ||
      asked.changed >= kTalkFilterRowBase + static_cast<int>(TalkFilterRow::kCount)) {
    return false;
  }
  switch (static_cast<TalkFilterRow>(asked.changed - kTalkFilterRowBase)) {
    case TalkFilterRow::kTalkHidden:
      mute_.talk_hidden = asked.on;
      break;
    case TalkFilterRow::kTalkAndGuildHidden:
      mute_.talk_and_guild_hidden = asked.on;
      break;
    case TalkFilterRow::kCount:
      break;
  }
  return true;
}

bool TalkFilterOptions::IsOn(TalkFilterRow row) const noexcept {
  switch (row) {
    case TalkFilterRow::kTalkHidden:
      return mute_.talk_hidden;
    case TalkFilterRow::kTalkAndGuildHidden:
      return mute_.talk_and_guild_hidden;
    case TalkFilterRow::kCount:
      break;
  }
  return false;
}

}  // namespace moonclient
