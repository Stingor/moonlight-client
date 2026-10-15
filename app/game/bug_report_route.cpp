#include "game/bug_report_route.h"

#include <format>
#include <string>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/bug_report.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/style_code.h"
#include "ro/sim/world.h"

#include "character/style_recipe.h"
#include "game/bug_report_context.h"
#include "game/card_album_bug_report.h"
#include "game/cash_shop_bug_report.h"
#include "game/craft_atlas_bug_report.h"

namespace moonclient {
namespace {

// LE RAPPORT DE LA FENÊTRE « IA DU COMPAGNON » : le compagnon, sa classe et le
// profil appliqué.
[[nodiscard]] BugReportContext CompanionAiBugReport(const ro::ui::CompanionAiView& view) {
  const bool homunculus = view.companion == ro::ui::kCompanionAiHomunculus;
  const std::string fields = std::format(R"("companion":"{}","companion_class":{},"profile":"{}")",
                                         homunculus ? "homunculus" : "mercenary",
                                         view.companion_class, JsonEscaped(view.profile));
  return FieldsBugReport(std::format("{} : {} — {}", ro::core::Tr("IA du compagnon"),
                                     view.companion_name, view.profile),
                         fields);
}

// LE RAPPORT DE LA FENÊTRE DES TACTIQUES : le monstre (son titre) et le
// compagnon.
[[nodiscard]] BugReportContext CompanionTacticBugReport(const ro::ui::CompanionTacticView& view) {
  const std::string fields = std::format(R"("tactic":"{}","companion":"{}")",
                                         JsonEscaped(view.title), JsonEscaped(view.companion));
  return FieldsBugReport(std::format("{} — {}", view.title, view.companion), fields);
}

}  // namespace

void RouteBugReports(const UiStage::Answers& answers, UiStage::Windows& windows,
                     const ro::sim::World& world, const DescriptionView& item,
                     const ro::ui::SkillDescriptionView& skill, bool buttons,
                     ro::net::Connection& link) {
  ro::ui::BugReportWindowState& box = windows.bug_report;
  if (answers.description.report_bug) {
    AskBugReport(box, item.BugReport(windows.description.item_id));
  }
  if (answers.skill_description.report_bug) {
    const ro::sim::KnownSkill* known = world.skill_book().Find(windows.skill_description.skill_id);
    AskBugReport(box, SkillBugReport(windows.skill_description.skill_id, skill.name,
                                     known != nullptr ? known->level : 0));
  }
  if (answers.npc.report_bug && world.npc_dialog().has_value()) {
    const std::uint32_t npc_id = world.npc_dialog()->npc_id;
    const ro::sim::Actor* npc = world.Find(npc_id);
    AskBugReport(box, NpcBugReport(npc_id, npc != nullptr ? npc->name : std::string()));
  }
  if (answers.style.report_bug) {
    AskBugReport(box, StyleBugReport(ro::sim::EncodeStyleCode(ToWire(windows.style.recipe))));
  }
  if (answers.companion_ai.report_bug) {
    AskBugReport(box, CompanionAiBugReport(windows.companion_ai_view));
  }
  if (answers.companion_tactic.report_bug) {
    AskBugReport(box, CompanionTacticBugReport(windows.companion_tactic_view));
  }
  // ⚠ COMPOSÉS APRÈS LE DESSIN ET AVANT QUE LEUR ÉTAGE NE CONSOMME LA RÉPONSE :
  // l'état et la vue sont ceux que l'image a montrés.
  if (answers.card_album.report_bug && windows.card_album_view != nullptr) {
    AskBugReport(box, CardAlbumBugReport(windows.card_album, *windows.card_album_view));
  }
  if (windows.craft_atlas.answer.report_bug && windows.craft_atlas.view != nullptr) {
    AskBugReport(box, CraftAtlasBugReport(windows.craft_atlas.state, *windows.craft_atlas.view));
  }
  if (windows.cash_shop.answer.report_bug && windows.cash_shop.view != nullptr) {
    AskBugReport(box, CashShopBugReport(windows.cash_shop.state, *windows.cash_shop.view));
  }
  if (answers.bug_report.send) {
    if (const auto sent = ro::net::SendBugReport(link, box.category, box.context_json,
                                                 answers.bug_report.message);
        !sent) {
      ro::core::LogWarning("rapport de bug : {} — il n'est pas parti",
                           ro::core::Describe(sent.error()));
    }
  }

  // LE BOUTON, POUR L'IMAGE SUIVANTE : un seul réglage pour toutes les fenêtres.
  box.buttons = buttons;
  windows.description.bug_report = buttons;
  windows.monster_sheet.database.state.item_sheet.bug_report = buttons;
  windows.skill_description.bug_report = buttons;
  windows.npc_dialog.bug_report = buttons;
  windows.style.bug_report = buttons;
  windows.navigation.state.bug_report = buttons;
  windows.achievements.window.bug_report = buttons;
  windows.quests.window.bug_report = buttons;
  windows.battleground.window.bug_report = buttons;
  windows.companion_ai.bug_report = buttons;
  windows.companion_tactic.bug_report = buttons;
  windows.options.bug_report = buttons;
  windows.hotkeys.bug_report = buttons;
  windows.character.bug_report = buttons;
  windows.card_album.bug_report = buttons;
  windows.craft_atlas.state.bug_report = buttons;
  windows.cash_shop.state.bug_report = buttons;
  windows.mvp_tracker.window.bug_report = buttons;
  windows.party_booking.window.bug_report = buttons;
  windows.mail.box.bug_report = buttons;
  windows.mail.compose.bug_report = buttons;
}

}  // namespace moonclient
