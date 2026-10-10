#include "world/npc_talk.h"

#include "ro/net/npc_dialog.h"
#include "ro/sim/world.h"

#include "world/click_action.h"

namespace moonclient {

void SendNpcAnswer(const ro::ui::NpcDialogAnswer& answer, ro::net::Connection& link,
                   ro::sim::World& world) {
  switch (answer.intent) {
    case ro::ui::NpcDialogIntent::kNext:
      ReportRefusal(ro::net::AnswerNpcNext(link, world));
      return;
    case ro::ui::NpcDialogIntent::kChoose:
      ReportRefusal(ro::net::AnswerNpcMenu(link, world, answer.option_index));
      return;
    case ro::ui::NpcDialogIntent::kNumber:
      ReportRefusal(ro::net::AnswerNpcNumber(link, world, answer.number));
      return;
    case ro::ui::NpcDialogIntent::kText:
      ReportRefusal(ro::net::AnswerNpcText(link, world, answer.text));
      return;
    case ro::ui::NpcDialogIntent::kClose:
      // ⚠ « FERMER » ET « ANNULER » SONT LE MÊME GESTE, et c'est `net` qui sait
      // quel paquet chacun vaut : un script arrêté sur un menu attend un rang,
      // pas une fermeture. Les distinguer ici referait ce choix deux fois.
      ReportRefusal(ro::net::LeaveNpcDialog(link, world));
      return;
    case ro::ui::NpcDialogIntent::kNothing:
      return;
  }
}

}  // namespace moonclient
