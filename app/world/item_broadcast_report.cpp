#include "world/item_broadcast_report.h"

#include <array>
#include <cstddef>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/sim/broadcast_balloon.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_link.h"

namespace moonclient {
namespace {

using ro::core::Printed;
using ro::core::Tr;

// Les lettres des grades, du rang 1 au rang 7.
constexpr auto kGradeLetters =
    std::to_array<std::string_view>({"D", "C", "B", "A", "R", "S", "SS"});

// Le lien d'un objet, que le fil et la bulle décorent en nom composé.
[[nodiscard]] std::string ItemLinkOf(std::uint32_t item_id) {
  return ro::vfs::BuildItemLink(ro::vfs::ItemLink{.item_id = item_id}, 0);
}

// Une annonce dans la conversation, puis sa bulle.
void Say(ro::sim::World& world, std::string text, std::uint32_t chat_color,
         std::uint32_t heard_ms) {
  ro::sim::BalloonNotice balloon{.family = ro::sim::BalloonFamily::kAnnouncement,
                                 .heard_ms = heard_ms};
  balloon.lines.push_back(ro::sim::BalloonLine{.text = text, .color = kItemBroadcastColor});
  world.balloon_news().Post(std::move(balloon));
  // ⚠ SOUS LA CASE GÉNÉRALE, et non celle des annonces : le client les range
  // avec ce qui n'a pas de case plus précise.
  world.RecordChat(ro::sim::ChatKind::kAnnouncement, ro::sim::ChatType::kPublic, 0, std::move(text),
                   heard_ms, chat_color);
}

}  // namespace

std::string_view GradeLetter(int grade) {
  if (grade < 1 || grade > static_cast<int>(kGradeLetters.size())) {
    return {};
  }
  return kGradeLetters[static_cast<std::size_t>(grade - 1)];
}

std::string RareObtainLine(const ro::sim::RareItemObtain& obtained) {
  const std::string item = ItemLinkOf(obtained.item_id);
  if (obtained.source == static_cast<std::uint8_t>(ro::sim::RareObtainSource::kNpc)) {
    return Printed(Tr("Félicitations %s, [%s] obtenu !"), obtained.player_name.c_str(),
                   item.c_str());
  }
  // ⚠ LE MONSTRE SE DIT COMME UNE BOÎTE, « a ouvert [Baphomet] » : c'est la
  // phrase du client pour les deux.
  std::string source;
  if (obtained.source == static_cast<std::uint8_t>(ro::sim::RareObtainSource::kBox)) {
    source = ItemLinkOf(obtained.box_item_id);
  } else if (obtained.source == static_cast<std::uint8_t>(ro::sim::RareObtainSource::kMonster)) {
    source = obtained.monster_name;
  }
  return Printed(Tr("%s a ouvert [%s] et obtenu [%s]."), obtained.player_name.c_str(),
                 source.c_str(), item.c_str());
}

std::string RefineAnnounceLine(const ro::sim::RefineAnnounce& refined) {
  const std::string item = ItemLinkOf(refined.item_id);
  if (refined.success) {
    return Printed(Tr("[%s] a raffiné un équipement avec succès : [+%d %s] obtenu."),
                   refined.player_name.c_str(), refined.refine_level, item.c_str());
  }
  return Printed(Tr("[%s] n'a pas réussi à raffiner [+%d %s]."), refined.player_name.c_str(),
                 refined.refine_level, item.c_str());
}

std::string GradeAnnounceLine(const ro::sim::GradeAnnounce& graded) {
  const std::string item = ItemLinkOf(graded.item_id);
  const std::string letter(GradeLetter(graded.grade));
  if (graded.success) {
    return Printed(Tr("[%s] a amélioré le grade avec succès et obtenu l'item [%s grade %s]."),
                   graded.player_name.c_str(), letter.c_str(), item.c_str());
  }
  return Printed(Tr("[%s] n'a pas réussi à améliorer l'item [%s grade %s]."),
                 graded.player_name.c_str(), letter.c_str(), item.c_str());
}

void ReportItemBroadcasts(ro::sim::ItemNews& news, ro::sim::World& world) {
  for (const ro::sim::RareItemObtain& obtained : news.TakeRareObtains()) {
    Say(world, RareObtainLine(obtained), kRareObtainChatColor, obtained.heard_ms);
  }
  for (const ro::sim::RefineAnnounce& refined : news.TakeRefines()) {
    Say(world, RefineAnnounceLine(refined), kItemBroadcastColor, refined.heard_ms);
  }
  for (const ro::sim::GradeAnnounce& graded : news.TakeGrades()) {
    Say(world, GradeAnnounceLine(graded), kItemBroadcastColor, graded.heard_ms);
  }
}

}  // namespace moonclient
