#include "options/whisper_options.h"

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs le réglage
// d'origine, sans que rien ne le dise.
constexpr const char* kOpenFromFriendKey = "chat_1_1_friends";
constexpr const char* kOpenFromStrangerKey = "chat_1_1_unknown";
constexpr const char* kAlarmKey = "chat_1_1_alarm";

[[nodiscard]] int IdOf(WhisperRow row) { return kWhisperRowBase + static_cast<int>(row); }

}  // namespace

void WhisperOptions::Declare(SettingsFile& into) {
  into.Declare(kOpenFromFriendKey, &setup_.open_from_friend);
  into.Declare(kOpenFromStrangerKey, &setup_.open_from_stranger);
  into.Declare(kAlarmKey, &setup_.alarm);
}

void WhisperOptions::ComposeInto(std::vector<ro::ui::OptionRowView>& rows) const {
  rows.push_back(ro::ui::OptionRowView{
      .id = IdOf(WhisperRow::kOpenFromFriend),
      .label = Tr("Ouvrir le chat 1:1 entre amis"),
      .help = Tr("Une fenêtre à part, avec sa propre saisie : ce qui s'y dit ne paraît plus dans "
                 "le fil. Décoché, le chuchotement reste une ligne du fil."),
      .kind = ro::ui::OptionKind::kToggle,
      .on = setup_.open_from_friend});
  rows.push_back(ro::ui::OptionRowView{
      .id = IdOf(WhisperRow::kOpenFromStranger),
      .label = Tr("Ouvrir le chat 1:1 entre inconnus"),
      .help = Tr("Tout joueur qui n'est pas dans votre liste d'amis, membres de votre guilde "
                 "compris. Vos propres chuchotements ouvrent aussi la fenêtre de leur "
                 "destinataire."),
      .kind = ro::ui::OptionKind::kToggle,
      .on = setup_.open_from_stranger});
  rows.push_back(ro::ui::OptionRowView{
      .id = IdOf(WhisperRow::kAlarm),
      .label = Tr("Alerte à la réception d'un chat 1:1"),
      .help = Tr("Un son quand une conversation s'ouvre d'elle-même, à l'arrivée d'un "
                 "chuchotement — pas à chaque ligne."),
      .kind = ro::ui::OptionKind::kToggle,
      .on = setup_.alarm});
}

bool WhisperOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kWhisperRowBase ||
      asked.changed >= kWhisperRowBase + static_cast<int>(WhisperRow::kCount)) {
    return false;
  }
  switch (static_cast<WhisperRow>(asked.changed - kWhisperRowBase)) {
    case WhisperRow::kOpenFromFriend:
      setup_.open_from_friend = asked.on;
      break;
    case WhisperRow::kOpenFromStranger:
      setup_.open_from_stranger = asked.on;
      break;
    case WhisperRow::kAlarm:
      setup_.alarm = asked.on;
      break;
    case WhisperRow::kCount:
      break;
  }
  return true;
}

}  // namespace moonclient
