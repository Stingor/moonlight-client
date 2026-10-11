#include "game/mail_bug_report.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/text.h"

namespace moonclient {
namespace {

using ro::ui::MailboxTab;
using ro::ui::MailboxView;
using ro::ui::MailboxWindowState;
using ro::ui::MailComposeView;
using ro::ui::MailComposeWindowState;
using ro::ui::MailRowView;

// Le libellé d'un rapport — « Courrier : « Bienvenue » de Kafra (#42) » —,
// jamais plus long.
constexpr std::size_t kLabelBytes = 192;
// Le plus long texte repris : un nom, un sujet, un filtre.
constexpr std::size_t kMostTextBytes = 48;
// Le plus long refus repris : une phrase entière du client.
constexpr std::size_t kMostErrorBytes = 96;

[[nodiscard]] const char* Flag(bool value) { return value ? "true" : "false"; }

// Un texte borné, coupé sur une frontière de caractère, prêt pour le JSON.
[[nodiscard]] std::string Bounded(std::string_view text, std::size_t most_bytes = kMostTextBytes) {
  return JsonEscaped(ro::core::WithoutDanglingUtf8(text.substr(0, most_bytes)));
}

// Le texte d'un tampon de saisie, jusqu'à son premier zéro.
template <std::size_t kBytes>
[[nodiscard]] std::string_view Typed(const std::array<char, kBytes>& buffer) {
  const auto end = std::ranges::find(buffer, '\0');
  return {buffer.data(), static_cast<std::size_t>(end - buffer.begin())};
}

[[nodiscard]] const char* TabWord(MailboxTab tab) {
  switch (tab) {
    case MailboxTab::kNormal:
      return "normal";
    case MailboxTab::kAccount:
      return "account";
    case MailboxTab::kReturned:
      return "returned";
    case MailboxTab::kAll:
      return "all";
  }
  return "?";
}

[[nodiscard]] const char* ConfirmWord(ro::ui::MailConfirm confirm) {
  switch (confirm) {
    case ro::ui::MailConfirm::kNone:
      return "none";
    case ro::ui::MailConfirm::kDelete:
      return "delete";
    case ro::ui::MailConfirm::kReturn:
      return "return";
    case ro::ui::MailConfirm::kClaimAll:
      return "claim_all";
    case ro::ui::MailConfirm::kDeleteAll:
      return "delete_all";
  }
  return "?";
}

[[nodiscard]] const char* CheckWord(ro::ui::MailRecipientCheck check) {
  switch (check) {
    case ro::ui::MailRecipientCheck::kNone:
      return "none";
    case ro::ui::MailRecipientCheck::kPending:
      return "pending";
    case ro::ui::MailRecipientCheck::kUnknown:
      return "unknown";
    case ro::ui::MailRecipientCheck::kFound:
      return "found";
  }
  return "?";
}

// Une suite de pièces jointes, par item et quantité, bornée à ce qu'un
// courrier peut porter.
template <typename Line, typename ItemOf>
[[nodiscard]] std::string Items(std::span<const Line> lines, ItemOf item_of) {
  std::string items = "[";
  const std::size_t shown =
      std::min(lines.size(), static_cast<std::size_t>(ro::ui::kMailMaxAttachments));
  for (std::size_t rank = 0; rank < shown; ++rank) {
    const ro::ui::MailItemView& line = item_of(lines[rank]);
    items += std::format(R"({}{{"item_id":{},"amount":{}}})", rank == 0 ? "" : ",",
                         line.item.item_id, line.amount);
  }
  items += ']';
  return items;
}

// LE COURRIER SÉLECTIONNÉ : son en-tête, puis son contenu s'il est arrivé.
[[nodiscard]] std::string SelectedFields(const MailRowView& row, const MailboxView& view) {
  const std::string expires =
      row.expire_seconds_left.has_value() ? std::to_string(*row.expire_seconds_left) : "null";
  std::string fields = std::format(
      R"("selected":{{"mail_id":{},"box":{},"read":{},"carried":{},"sender":"{}","title":"{}",)"
      R"("expire_s":{},"content_ready":{},"content_unanswered":{})",
      row.ref.mail_id, static_cast<int>(row.ref.box), Flag(row.read), row.carried,
      Bounded(row.sender), Bounded(row.title), expires, Flag(row.content_ready),
      Flag(row.content_unanswered));
  // ⚠ UN CONTENU D'UN AUTRE COURRIER NE SE MONTRE PAS : il ne se rapporte pas.
  if (view.content != nullptr && view.content->ref == row.ref) {
    fields += std::format(
        R"(,"content":{{"zeny":{},"body_bytes":{},"items":{}}})", view.content->zeny,
        view.content->body.size(),
        Items(
            view.content->items,
            [](const ro::ui::MailItemView& line) -> const ro::ui::MailItemView& { return line; }));
  }
  fields += '}';
  return fields;
}

}  // namespace

BugReportContext MailboxBugReport(const MailboxWindowState& window, const MailboxView& view) {
  const std::string_view filter = Typed(window.filter);
  const ro::ui::MailboxTabs tabs = ro::ui::CountMailboxTabs(view.rows);
  const auto shown = std::ranges::count_if(view.rows, [&](const MailRowView& row) {
    return ro::ui::MailRowShown(row, window.tab, filter);
  });
  // Les comptes dans l'ordre des onglets : reçus, compte, retournés, tous.
  std::string counts = "[";
  for (std::size_t rank = 0; rank < tabs.counts.size(); ++rank) {
    counts += std::format("{}{}", rank == 0 ? "" : ",", tabs.counts[rank]);
  }
  counts += ']';
  std::string fields =
      std::format(R"("mailbox":{{"tab":"{}","filter":"{}","rows":{},"shown":{},"counts":{},)"
                  R"("unread":{},"loading":{},"confirm":"{}")",
                  TabWord(window.tab), Bounded(filter), view.rows.size(), shown, counts,
                  view.unread, Flag(view.list_loading), ConfirmWord(window.confirm));

  const MailRowView* selected = nullptr;
  if (window.selected.has_value()) {
    const auto found = std::ranges::find_if(
        view.rows, [&](const MailRowView& row) { return row.ref == *window.selected; });
    selected = found != view.rows.end() ? &*found : nullptr;
  }
  if (selected != nullptr) {
    fields += ',';
    fields += SelectedFields(*selected, view);
  }
  fields += '}';

  // Le libellé : le courrier ouvert, sinon l'onglet.
  std::array<char, kLabelBytes> label{};
  if (selected != nullptr) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Courrier : « %s » de %s (#%lld)"),
                  std::string(selected->title).c_str(), std::string(selected->sender).c_str(),
                  static_cast<long long>(selected->ref.mail_id));
  } else {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Courrier : onglet %s, %u courriers"),
                  TabWord(window.tab), static_cast<unsigned>(shown));
  }
  return FieldsBugReport(std::string(ro::core::WithoutDanglingUtf8(label.data())), fields);
}

BugReportContext MailComposeBugReport(const MailComposeWindowState& window,
                                      const MailComposeView& view) {
  const std::string_view recipient = Typed(window.recipient);
  const std::string fields = std::format(
      R"("mail_compose":{{"recipient":"{}","check":"{}","checked":{{"name":"{}","job":"{}",)"
      R"("level":{}}},"subject":"{}","subject_bytes":{},"message_bytes":{},"zeny":{},)"
      R"("fee":{},"total_debit":{},"purse_short":{},"attachments":{},"capacity":{},)"
      R"("send_in_flight":{},"confirming_send":{},"asking_amount":{},"error":"{}"}})",
      Bounded(recipient), CheckWord(view.check), Bounded(view.checked_name),
      Bounded(view.checked_job), view.checked_level, Bounded(Typed(window.subject)),
      view.subject_wire_bytes, view.message_wire_bytes, window.zeny, view.fee, view.total_debit,
      Flag(view.purse_short),
      Items(view.attachments,
            [](const ro::ui::MailAttachmentView& attached) -> const ro::ui::MailItemView& {
              return attached.line;
            }),
      view.attachment_capacity, Flag(view.send_in_flight), Flag(window.confirming_send),
      Flag(window.asking_amount), Bounded(view.error, kMostErrorBytes));

  std::array<char, kLabelBytes> label{};
  if (!recipient.empty()) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Courrier : écriture à %s"),
                  std::string(recipient).c_str());
  } else {
    std::snprintf(label.data(), label.size(), "%s", ro::core::Tr("Courrier : écriture"));
  }
  return FieldsBugReport(std::string(ro::core::WithoutDanglingUtf8(label.data())), fields);
}

}  // namespace moonclient
