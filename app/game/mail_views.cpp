// Les vues du courrier : la liste, le courrier ouvert, l'écriture — ses pièces
// jointes, le sac tel qu'elle le voit, ses frais et ses compteurs.

#include <algorithm>
#include <string>
#include <string_view>

#include "ro/core/text.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "game/mail_stage.h"
#include "game/skin_image_of.h"

namespace moonclient {
namespace {

// Les trois images de la colonne « reste à récupérer », sous le dossier
// d'interface de l'installation, dans son dossier `basic_interface`.
constexpr std::string_view kZenyIcon = "basic_interface\\rodexsystem\\renewal\\icon_zeny.bmp";
constexpr std::string_view kItemsIcon = "basic_interface\\rodexsystem\\renewal\\icon_item.bmp";
constexpr std::string_view kZenyAndItemsIcon =
    "basic_interface\\rodexsystem\\renewal\\icon_zeny_n_item.bmp";

ro::ui::MailBox UiBoxOf(ro::sim::MailBox box) {
  switch (box) {
    case ro::sim::MailBox::kNormal:
      return ro::ui::MailBox::kNormal;
    case ro::sim::MailBox::kAccount:
      return ro::ui::MailBox::kAccount;
    case ro::sim::MailBox::kReturned:
      return ro::ui::MailBox::kReturned;
  }
  return ro::ui::MailBox::kNormal;
}

std::uint8_t CarriedOf(const ro::sim::MailHeader& header) {
  std::uint8_t carried = 0;
  if (header.has_zeny) {
    carried |= ro::ui::kMailCarriesZeny;
  }
  if (header.has_items) {
    carried |= ro::ui::kMailCarriesItems;
  }
  if (header.from_npc) {
    carried |= ro::ui::kMailFromSystem;
  }
  return carried;
}

ro::ui::MailRecipientCheck CheckOf(ro::sim::MailRecipientState state) {
  switch (state) {
    case ro::sim::MailRecipientState::kPending:
      return ro::ui::MailRecipientCheck::kPending;
    case ro::sim::MailRecipientState::kNotFound:
      return ro::ui::MailRecipientCheck::kUnknown;
    case ro::sim::MailRecipientState::kFound:
      return ro::ui::MailRecipientCheck::kFound;
    // ⚠ UN SILENCE N'EST PAS « INEXISTANT » : carte sans courrier, serveur de
    // personnages injoignable. Rien ne se dit.
    case ro::sim::MailRecipientState::kUnchecked:
    case ro::sim::MailRecipientState::kSilent:
      return ro::ui::MailRecipientCheck::kNone;
  }
  return ro::ui::MailRecipientCheck::kNone;
}

template <std::size_t kBytes>
std::string_view TypedText(const std::array<char, kBytes>& buffer) {
  const auto end = std::ranges::find(buffer, '\0');
  return {buffer.data(), static_cast<std::size_t>(end - buffer.begin())};
}

}  // namespace

void MailStage::LoadIcons() {
  if (images_ == nullptr || (icons_loaded_ && images_->generation() == icons_generation_)) {
    return;
  }
  icons_loaded_ = true;
  icons_generation_ = images_->generation();
  icons_ = ro::ui::MailCarriedIcons{.zeny = SkinImageAt(images_, kZenyIcon),
                                    .items = SkinImageAt(images_, kItemsIcon),
                                    .zeny_and_items = SkinImageAt(images_, kZenyAndItemsIcon)};
}

void MailStage::ComposeBox(const MailWindows& windows, const TradeContext& context) {
  if (!mailbox_.open()) {
    return;
  }
  const std::int64_t now_unix_s = MailNowUnixSeconds();
  rows_.clear();
  for (const ro::sim::MailHeader& header : mailbox_.headers()) {
    // Un courrier sans échéance ne montre rien, comme une expiration nulle.
    std::optional<std::int64_t> seconds_left;
    if (!header.never_expires) {
      seconds_left = ro::sim::MailSecondsLeft(header, now_unix_s);
    }
    rows_.push_back(
        ro::ui::MailRowView{.ref = {.mail_id = header.mail_id, .box = UiBoxOf(header.box)},
                            .read = header.read,
                            .carried = CarriedOf(header),
                            .content_ready = mailbox_.FindContent(header.mail_id) != nullptr,
                            .content_unanswered = mailbox_.ReadUnanswered(header.mail_id),
                            .sender = header.sender,
                            .title = header.title,
                            .expire_seconds_left = seconds_left});
  }

  // ── LE COURRIER OUVERT ───────────────────────────────────────────────────
  const ro::sim::MailContent* content = nullptr;
  if (windows.box.selected.has_value()) {
    content = mailbox_.FindContent(windows.box.selected->mail_id);
  }
  content_items_.clear();
  content_names_.clear();
  content_lines_.clear();
  if (content != nullptr) {
    for (const ro::sim::InventoryItem& item : content->items) {
      content_items_.push_back(item);
      content_names_.push_back(context.items.NameOf(item, context.creators));
    }
    // ⚠ LES VUES SE PRENNENT ICI, une fois les chaînes rangées.
    for (std::size_t rank = 0; rank < content_items_.size(); ++rank) {
      const ro::sim::InventoryItem& item = content_items_[rank];
      content_lines_.push_back(
          ro::ui::MailItemView{.item = {.item_id = item.item_id,
                                        .name = content_names_[rank],
                                        .icon = context.items.IconOf(item.item_id),
                                        .damaged = item.damaged},
                               .amount = item.amount});
    }
    content_view_ =
        ro::ui::MailContentView{.ref = {.mail_id = content->mail_id, .box = UiBoxOf(content->box)},
                                .body = content->body,
                                .zeny = content->zeny,
                                .items = content_lines_};
  }

  box_view_ = ro::ui::MailboxView{.rows = rows_,
                                  .unread = static_cast<int>(mailbox_.UnreadCount()),
                                  .list_loading = mailbox_.ListLoading(context.now_ms),
                                  .content = content != nullptr ? &content_view_ : nullptr,
                                  .icons = icons_};
}

void MailStage::ComposeWrite(const MailWindows& windows, const TradeContext& context) {
  if (!flow_.shown()) {
    attached_items_.clear();
    return;
  }
  const ro::ui::MailComposeWindowState& state = windows.compose;

  // ── LES PIÈCES JOINTES ACCUSÉES ──────────────────────────────────────────
  attached_items_.clear();
  attached_names_.clear();
  for (const ro::sim::MailAttachment& attachment : mailbox_.attachments()) {
    attached_items_.push_back(attachment.item);
    attached_names_.push_back(context.items.NameOf(attachment.item, context.creators));
  }
  attached_.clear();
  for (std::size_t rank = 0; rank < attached_items_.size(); ++rank) {
    const ro::sim::InventoryItem& item = attached_items_[rank];
    attached_.push_back(
        ro::ui::MailAttachmentView{.line = {.item = {.item_id = item.item_id,
                                                     .name = attached_names_[rank],
                                                     .icon = context.items.IconOf(item.item_id),
                                                     .damaged = item.damaged},
                                            .amount = item.amount},
                                   .client_slot = item.client_slot});
  }

  // ── LE SAC, TEL QUE L'ÉCRITURE LE VOIT ───────────────────────────────────
  // Ce qui est joint ou en route est retranché ; ce qu'on porte ne s'envoie pas.
  bag_.clear();
  for (const ro::sim::InventoryItem& item : context.world.inventory().items()) {
    if (item.equipped()) {
      continue;
    }
    const std::int32_t left = static_cast<std::int32_t>(item.amount) - WithheldAt(item.client_slot);
    bag_.push_back(ro::ui::MailBagView{.client_slot = item.client_slot,
                                       .amount = std::max<std::int32_t>(left, 0)});
  }

  // ── LE DESTINATAIRE VÉRIFIÉ ──────────────────────────────────────────────
  // 🔴 LA VÉRIFICATION NE VAUT QUE POUR LE NOM INTERROGÉ : une saisie dont les
  // octets du fil diffèrent n'est pas vérifiée, ni sur sa ligne ni dans le
  // récapitulatif.
  const ro::sim::MailRecipient& recipient = mailbox_.recipient();
  const std::optional<std::string> typed_wire = RecipientWire(TypedText(state.recipient));
  const bool same_name = typed_wire.has_value() && *typed_wire == recipient.queried_wire;
  const ro::ui::MailRecipientCheck check =
      same_name ? CheckOf(recipient.state) : ro::ui::MailRecipientCheck::kNone;
  checked_job_.clear();
  if (check == ro::ui::MailRecipientCheck::kFound && jobs_ != nullptr) {
    checked_job_ = jobs_->Find(recipient.job);
  }

  // ── LES FRAIS, LA BOURSE, LES COMPTEURS ──────────────────────────────────
  const std::size_t pieces = mailbox_.attachment_count();
  const std::int64_t zeny = std::clamp<std::int64_t>(state.zeny, 0, ro::sim::kMailMostZeny);
  const std::int64_t total = ro::sim::MailSendTotal(zeny, pieces);
  const std::int64_t purse = context.world.player_status().zeny;
  // ⚠ UNE BOURSE INCONNUE NE COUVRE RIEN : un envoi qui coûte ne part pas.
  const bool purse_short = purse == ro::sim::kUnknownStat ? total > 0 : total > purse;

  compose_view_ = ro::ui::MailComposeView{
      .attachments = attached_,
      .bag = bag_,
      .attachment_capacity = static_cast<int>(ro::sim::kMailMostItems),
      .check = check,
      .checked_name = same_name ? std::string_view(recipient.name) : std::string_view(),
      .checked_job = checked_job_,
      .checked_level = same_name ? recipient.base_level : 0,
      .fee = ro::sim::MailFeeFor(zeny, pieces),
      .total_debit = total,
      .purse_short = purse_short,
      .subject_wire_bytes = ro::core::WireFromUtf8Text(TypedText(state.subject)).size(),
      .message_wire_bytes = ro::core::WireFromUtf8Text(TypedText(state.message)).size(),
      .subject_max_bytes = ro::sim::kMailMostTitleBytes,
      .message_max_bytes = ro::sim::kMailMostBodyBytes,
      .error = compose_error_,
      .send_in_flight = mailbox_.send_in_flight() || flow_.reopening()};
}

}  // namespace moonclient
