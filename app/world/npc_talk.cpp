#include "world/npc_talk.h"

#include <optional>
#include <string>

#include "ro/net/npc_dialog.h"
#include "ro/sim/world.h"

#include "chat/banned_input.h"
#include "world/click_action.h"

namespace moonclient {

void SendNpcAnswer(const ro::ui::NpcDialogAnswer& answer, ro::net::Connection& link,
                   ro::sim::World& world, const ro::sim::BannedWords* banned,
                   std::uint32_t now_ms) {
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
      if (banned != nullptr) {
        if (const std::optional<std::string> refused =
                BannedInputRefusal(*banned, answer.text, BannedInput::kSentence);
            refused.has_value()) {
          world.RecordChat(ro::sim::ChatKind::kServer, 0, *refused, now_ms,
                           BannedInputColor(BannedInput::kSentence));
          return;
        }
      }
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
