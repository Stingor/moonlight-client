// CE QU'UNE LIGNE MONTRE — ses balises traduites, l'en-tête d'un chuchotement
// et son pseudo cliquable — et le fil de chaque conversation 1:1.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <imgui.h>

#include "ro/net/discord_relay.h"

#include "chat/chat_item_link.h"
#include "chat/chat_tab_view.h"

namespace moonclient {
namespace {

constexpr int kSecondsPerMinute = 60;
constexpr int kSecondsPerHour = 60 * kSecondsPerMinute;
constexpr int kSecondsPerDay = 24 * kSecondsPerHour;
constexpr std::uint32_t kMsPerSecond = 1000;

// Une couleur de ligne, rouge, vert puis bleu, et l'opacité qui est la nôtre.
constexpr int kRedShift = 16;
constexpr int kGreenShift = 8;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr int kOpaque = 255;

[[nodiscard]] ImU32 ColorOf(std::uint32_t rgb) {
  return IM_COL32((rgb >> kRedShift) & kByteMask, (rgb >> kGreenShift) & kByteMask, rgb & kByteMask,
                  kOpaque);
}

// L'expéditeur d'un `<MVPL>` d'une ligne qui n'est pas un chuchotement. ⚠ LA
// TÊTE NE SE CHERCHE QUE SI LA LIGNE PORTE LA BALISE : la question se pose à
// chaque ligne, à chaque image.
[[nodiscard]] std::string MvpSenderIfLinked(const ro::sim::ChatLine& line) {
  if (FindMvpLink(line.text, 0) == std::string_view::npos) {
    return {};
  }
  return MvpSenderOfLine(line.kind, line.type, line.text);
}

}  // namespace

bool CarriesMvpLink(const ro::ui::ChatLineView& view) {
  return std::ranges::any_of(view.links, [](const ro::ui::ChatLinkSpan& link) {
    return link.kind == ro::ui::ChatLinkKind::kMvp;
  });
}

void ChatTabsView::NoteMvpLinks(DecoratedChatLine& line, std::size_t from,
                                std::string_view sender) {
  for (std::size_t rank = from; rank < line.links.size() && rank < line.carried.size(); ++rank) {
    if (line.links[rank].kind != ro::ui::ChatLinkKind::kMvp) {
      continue;
    }
    line.carried[rank].sender = std::string(sender);
    line.links[rank].mvp = mvp_judge_ ? mvp_judge_(line.carried[rank].mvp) : nullptr;
  }
}

void ChatTabsView::ShowText(std::string_view raw, ro::ui::ChatLineView& view, GroupViews& views,
                            std::string_view sender, bool discord_line,
                            const ro::sim::ChatAuthor* author) {
  view.text = raw;
  view.body_offset = ChatBodyOffset(raw, discord_line);
  if (author != nullptr && !discord_line && ShowAuthored(raw, *author, view, views, sender)) {
    return;
  }
  // ⚠ LA QUESTION D'ABORD, LA LIGNE DÉCORÉE ENSUITE : `DecorateRichChatLine`
  // pose `MayNeedRichDecor` avant tout, une fois par ligne. RIEN N'EST RECOPIÉ
  // POUR LA QUASI-TOTALITÉ DES LIGNES : sans balise ni balisage, la vue
  // continue de regarder le texte du monde.
  DecoratedChatLine decorated;
  if (!DecorateRichChatLine(raw, discord_line, Tables(), rich_, &decorated)) {
    return;
  }
  NoteMvpLinks(decorated, 0, sender);
  // 🔴 L'AVATAR NE SE POSE QUE VIGNETTES PERMISES : le télécharger expose
  // l'adresse du joueur au CDN de Discord.
  if (discord_line && author != nullptr && rich_.pictures) {
    if (std::string url = ro::net::DiscordAvatarUrl(author->avatar_path); !url.empty()) {
      PlaceLineAvatar(decorated, std::move(url));
    }
  }
  Publish(std::move(decorated), view, views);
}

bool ChatTabsView::ShowAuthored(std::string_view raw, const ro::sim::ChatAuthor& author,
                                ro::ui::ChatLineView& view, GroupViews& views,
                                std::string_view sender) {
  // ⚠ RECOPIÉE, comme un chuchotement : la tête et la phrase se décorent à part.
  DecoratedChatLine composed;
  if (!DecorateAuthoredLine(raw, author, own_name_, Tables(), rich_, &composed)) {
    return false;
  }
  // Le pseudo de l'auteur ouvre la ligne : c'est son premier lien.
  if (!composed.links.empty() && composed.links.front().kind == ro::ui::ChatLinkKind::kPlayer &&
      composed.links.front().offset == author.name_offset) {
    composed.links.front().refusals = RefusalsOf(author.name);
  }
  NoteMvpLinks(composed, 0, sender);
  Publish(std::move(composed), view, views);
  return true;
}

void ChatTabsView::ShowLine(const ro::sim::ChatLine& line, WhisperSurface surface,
                            ro::ui::ChatLineView& view, GroupViews& views) {
  if (line.whisper.side == ro::sim::WhisperSide::kNone) {
    ShowText(line.text, view, views, MvpSenderIfLinked(line),
             line.type == ro::sim::ChatType::kDiscord, line.author ? &*line.author : nullptr);
    return;
  }
  // ── UN CHUCHOTEMENT : son en-tête, puis sa phrase ─────────────────────────
  // ⚠ RECOPIÉ, LUI : l'en-tête ne vit pas dans le monde. Les chuchotements sont
  // rares au regard du reste du fil, et c'est ce qui rend la copie abordable.
  WhisperHead head = ComposeWhisperHead(line.whisper, surface, own_name_);
  // ⚠ DANS LA CONVERSATION, LA LIGNE ENTIÈRE SE MASQUE, noms compris ; le fil la
  // montre telle quelle. Le pseudo masqué garde son lien vers le vrai nom.
  const bool masks = surface == WhisperSurface::kConversation;
  if (masks && head.name_length > 0) {
    const std::string name = ro::sim::BannedWords::MaskWith(
        conversation_banned_,
        std::string_view(head.text).substr(head.name_offset, head.name_length));
    head.text.replace(head.name_offset, head.name_length, name);
    head.name_length = name.size();
  } else if (masks) {
    head.text = ro::sim::BannedWords::MaskWith(conversation_banned_, head.text);
  }
  DecoratedChatLine composed;
  composed.text = std::move(head.text);
  const auto body_at = static_cast<std::uint32_t>(composed.text.size());
  // LE PSEUDO D'ABORD : les morceaux d'une ligne se suivent dans l'ordre.
  if (head.name_length > 0) {
    ChatLinkTarget target;
    target.kind = ro::ui::ChatLinkKind::kPlayer;
    target.player = line.whisper.peer_name;
    composed.links.push_back(
        ro::ui::ChatLinkSpan{.offset = static_cast<std::uint32_t>(head.name_offset),
                             .length = static_cast<std::uint32_t>(head.name_length),
                             .kind = ro::ui::ChatLinkKind::kPlayer,
                             .refusals = RefusalsOf(line.whisper.peer_name)});
    composed.carried.push_back(std::move(target));
  }
  // LA PHRASE, décalée derrière l'en-tête : ses liens, ses styles, ses images.
  const std::string masked =
      masks ? ro::sim::BannedWords::MaskWith(conversation_banned_, WhisperSaid(line, words_))
            : std::string(WhisperSaid(line, words_));
  const std::string_view said = masked;
  DecoratedChatLine body;
  if (DecorateRichChatLine(said, false, Tables(), rich_, &body)) {
    // 🔴 UN CHUCHOTEMENT SE RÉSOUT PAR SON SENS, jamais par son en-tête : reçu,
    // l'expéditeur est le pair ; notre écho, c'est nous.
    NoteMvpLinks(body, 0, MvpWhisperSenderOf(line.whisper, own_name_));
    AppendRichBody(composed, std::move(body));
  } else {
    composed.text.append(said);
  }
  // ⚠ CE QUE DIT UN CHUCHOTEMENT COMMENCE APRÈS SON EN-TÊTE, et la coupe y est
  // exacte : l'en-tête est à nous.
  composed.body_offset = body_at;
  Publish(std::move(composed), view, views);
}

const ro::ui::PlayerLinkRefusals* ChatTabsView::RefusalsOf(std::string_view name) {
  if (!judge_) {
    return nullptr;
  }
  auto found = refusals_.find(name);
  if (found == refusals_.end()) {
    found = refusals_.emplace(std::string(name), judge_(name)).first;
  }
  return &found->second;
}

void ChatTabsView::RefreshWhispers(std::span<const ro::sim::ChatLine> chat,
                                   std::span<const Conversation> conversations) {
  // Une conversation refermée n'a plus de fil à tenir.
  std::erase_if(whisper_views_, [conversations](const auto& entry) {
    return std::ranges::find(conversations, entry.first, &Conversation::peer) ==
           conversations.end();
  });
  const std::uint64_t look_bits = LookBits(false);
  for (const Conversation& conversation : conversations) {
    GroupViews& views = whisper_views_[conversation.peer];
    views.lines.clear();
    views.decorated.clear();
    for (const ro::sim::ChatLine& line : chat) {
      if (line.kind != ro::sim::ChatKind::kWhisper ||
          line.whisper.side == ro::sim::WhisperSide::kNone ||
          line.whisper.peer_name != conversation.peer) {
        continue;
      }
      ro::ui::ChatLineView view{.color = ColorOf(ConversationColor(line.whisper.side))};
      // L'heure, comme dans le fil, quand le repère du jour est connu.
      if (day_second_at_boot_ >= 0) {
        const int second = ((day_second_at_boot_ + static_cast<int>(line.said_ms / kMsPerSecond)) %
                                kSecondsPerDay +
                            kSecondsPerDay) %
                           kSecondsPerDay;
        view.hour = static_cast<std::uint8_t>(second / kSecondsPerHour);
        view.minute = static_cast<std::uint8_t>((second / kSecondsPerMinute) % kSecondsPerMinute);
        view.second = static_cast<std::uint8_t>(second % kSecondsPerMinute);
      }
      ShowLine(line, WhisperSurface::kConversation, view, views);
      view.id = CarriesMvpLink(view) ? 0 : look_bits | line.seq;
      view.seq = line.seq;
      views.lines.push_back(view);
    }
  }
}

std::span<const ro::ui::ChatLineView> ChatTabsView::whisper_lines(std::string_view peer) const {
  const auto found = whisper_views_.find(peer);
  return found == whisper_views_.end() ? std::span<const ro::ui::ChatLineView>()
                                       : found->second.lines;
}

ro::ui::ChatLogLayout* ChatTabsView::whisper_layout(std::string_view peer) const {
  const auto found = whisper_views_.find(peer);
  return found == whisper_views_.end() ? nullptr : &found->second.layout;
}

}  // namespace moonclient
