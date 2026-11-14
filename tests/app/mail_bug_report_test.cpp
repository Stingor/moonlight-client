// LES RAPPORTS D'UN BUG DU COURRIER : un JSON valable, qui décrit le courrier
// ouvert — en-tête, zeny, pièces jointes — SANS SON CORPS, et l'écriture sans
// son message.

#include "game/mail_bug_report.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "web/json_value.h"

using moonclient::MailboxBugReport;
using moonclient::MailComposeBugReport;
using ro::ui::MailAttachmentView;
using ro::ui::MailboxView;
using ro::ui::MailboxWindowState;
using ro::ui::MailComposeView;
using ro::ui::MailComposeWindowState;
using ro::ui::MailContentView;
using ro::ui::MailItemView;
using ro::ui::MailRef;
using ro::ui::MailRowView;

namespace {

// Le serveur tronque le contexte à 1024 octets ; le rapport doit en rester loin.
constexpr std::size_t kComfortableBytes = 900;
constexpr std::int64_t kMailId = 4242;
constexpr std::uint32_t kRedPotionId = 501;
constexpr std::uint32_t kJellopyId = 909;
constexpr std::string_view kSecretBody = "le code du coffre est 1234";

[[nodiscard]] bool Has(const moonclient::BugReportContext& report, std::string_view piece) {
  return report.json.find(piece) != std::string::npos;
}

}  // namespace

TEST_CASE("Rapport de la boîte : le courrier ouvert, sans son corps", "[app][bugreport]") {
  const MailRef ref{.mail_id = kMailId};
  const std::array rows = {
      MailRowView{.ref = ref,
                  .read = true,
                  .carried = static_cast<std::uint8_t>(ro::ui::kMailCarriesZeny |
                                                       ro::ui::kMailCarriesItems),
                  .content_ready = true,
                  .sender = "Kafra",
                  .title = "Bienvenue \"chez nous\""},
      MailRowView{.ref = MailRef{.mail_id = 7}, .sender = "Autre", .title = "Autre sujet"}};
  const std::array items = {
      MailItemView{.item = {.item_id = kRedPotionId, .name = "Red Potion"}, .amount = 3},
      MailItemView{.item = {.item_id = kJellopyId, .name = "Jellopy"}, .amount = 10}};
  const MailContentView content{.ref = ref, .body = kSecretBody, .zeny = 5000, .items = items};
  const MailboxView view{.rows = rows, .unread = 1, .content = &content};
  MailboxWindowState window;
  window.selected = ref;

  const moonclient::BugReportContext report = MailboxBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.label.find("Kafra") != std::string::npos);
  CHECK(report.label.find("4242") != std::string::npos);
  CHECK(Has(report, R"("tab":"normal")"));
  CHECK(Has(report, R"("rows":2,"shown":2)"));
  CHECK(Has(report, R"("mail_id":4242,"box":0,"read":true,"carried":6,"sender":"Kafra")"));
  CHECK(Has(report, R"("title":"Bienvenue \"chez nous\"")"));
  CHECK(Has(report, R"("zeny":5000)"));
  CHECK(Has(report, R"("items":[{"item_id":501,"amount":3},{"item_id":909,"amount":10}])"));
  CHECK(Has(report, std::string(R"("body_bytes":)") + std::to_string(kSecretBody.size())));
  CHECK_FALSE(Has(report, "coffre"));
  CHECK(report.json.size() < kComfortableBytes);
}

TEST_CASE("Rapport de la boîte : sans sélection, l'onglet et le filtre", "[app][bugreport]") {
  const std::array rows = {
      MailRowView{.ref = MailRef{.mail_id = 1}, .sender = "Kafra", .title = "Bienvenue"},
      MailRowView{.ref = MailRef{.mail_id = 2}, .sender = "Autre", .title = "Rien"}};
  const MailboxView view{.rows = rows, .list_loading = true};
  MailboxWindowState window;
  window.filter = {'k', 'a', 'f'};

  const moonclient::BugReportContext report = MailboxBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(Has(report, R"("filter":"kaf","rows":2,"shown":1)"));
  CHECK(Has(report, R"("loading":true)"));
  CHECK_FALSE(Has(report, "\"selected\""));
}

TEST_CASE("Rapport de l'écriture : destinataire, pièces jointes et frais, sans le message",
          "[app][bugreport]") {
  const std::array attached = {MailAttachmentView{
      .line = MailItemView{.item = {.item_id = kJellopyId, .name = "Jellopy"}, .amount = 2},
      .client_slot = 4}};
  MailComposeView view{.attachments = attached, .check = ro::ui::MailRecipientCheck::kFound};
  view.checked_name = "Ami";
  view.checked_job = "Priest";
  view.checked_level = 99;
  view.fee = 250;
  view.total_debit = 1250;
  view.message_wire_bytes = kSecretBody.size();
  view.error = "Ta bourse ne couvre pas le zeny joint et les frais d'envoi.";
  MailComposeWindowState window;
  window.recipient = {'A', 'm', 'i'};
  window.subject = {'S', 'a', 'l', 'u', 't'};
  std::ranges::copy(kSecretBody, window.message.begin());
  window.zeny = 1000;

  const moonclient::BugReportContext report = MailComposeBugReport(window, view);
  REQUIRE(moonclient::web::ParseJson(report.json).has_value());
  CHECK(report.label.find("Ami") != std::string::npos);
  CHECK(Has(report, R"("recipient":"Ami","check":"found","checked":{"name":"Ami","job":"Priest",)"
                    R"("level":99})"));
  CHECK(Has(report, R"("subject":"Salut")"));
  CHECK(Has(report, R"("zeny":1000,"fee":250,"total_debit":1250)"));
  CHECK(Has(report, R"("attachments":[{"item_id":909,"amount":2}])"));
  CHECK(Has(report, R"("error":"Ta bourse)"));
  CHECK_FALSE(Has(report, "coffre"));
  CHECK(report.json.size() < kComfortableBytes);
}
