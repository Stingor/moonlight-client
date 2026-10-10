// LES REPÈRES DES ONGLETS : la taille du texte qu'un Ctrl + molette fait varier,
// les vignettes que les lignes montrent, et les non-lus des languettes. À part
// de `chat_tab_view.cpp`, qui n'a plus la place de les porter.

#include <algorithm>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include "chat/chat_tab_view.h"

namespace moonclient {
namespace {

// CE QUI CHANGE L'ASPECT D'UNE MÊME LIGNE : les tables d'objets, qui traduisent
// ses balises ; le diagnostic, qui lui pose une étiquette ; le balisage, qui en
// retire les marques.
constexpr std::uint64_t kLookDecorated = 1;
constexpr std::uint64_t kLookDiagnostic = 2;
constexpr std::uint64_t kLookMarkup = 4;
// CE QUE LE LECTEUR A : un lien de réglage vers l'onglet d'un compagnon est un
// lien quand le compagnon est là, un texte inerte sinon — et sa largeur change.
constexpr std::uint64_t kLookHomunculus = 8;
constexpr std::uint64_t kLookMercenary = 16;
constexpr std::uint64_t kLookAchievements = 32;

}  // namespace

std::uint64_t ChatTabsView::LookBits(bool diagnostic) const noexcept {
  return ((items_ != nullptr && words_ != nullptr ? kLookDecorated : 0) |
          (diagnostic ? kLookDiagnostic : 0) | (rich_.markup ? kLookMarkup : 0) |
          (setting_reach_.homunculus ? kLookHomunculus : 0) |
          (setting_reach_.mercenary ? kLookMercenary : 0) |
          (setting_reach_.achievements ? kLookAchievements : 0))
         << kLookShift;
}

ChatDecorTables ChatTabsView::Tables() const noexcept {
  return ChatDecorTables{.items = items_,
                         .words = words_,
                         .creators = &creators_,
                         .maps = maps_,
                         .mvp_clock = &mvp_clock_,
                         .settings = &setting_reach_};
}

void ChatTabsView::Publish(DecoratedChatLine line, ro::ui::ChatLineView& view, GroupViews& views) {
  // ⚠ LE JETON EST POSÉ ICI, et nulle part ailleurs : c'est le seul endroit qui
  // voie TOUS les liens de TOUTES les fenêtres dans un ordre stable.
  for (std::size_t rank = 0; rank < line.links.size() && rank < line.carried.size(); ++rank) {
    ro::ui::ChatLinkSpan& link = line.links[rank];
    link.token = static_cast<std::uint32_t>(shown_links_.size());
    if (link.kind == ro::ui::ChatLinkKind::kUrl && rich_.url_facts) {
      link.url = rich_.url_facts(line.carried[rank].url);
    }
    shown_links_.push_back(line.carried[rank]);
  }
  // 🔴 LA CLÉ D'UNE VIGNETTE EST CELLE DE L'IMAGE, PAS DE LA LIGNE : le fil rend
  // les vignettes qu'il attend par leur clé, toutes fenêtres confondues, et deux
  // lignes ont chacune leur vignette de rang zéro.
  const auto first_key = static_cast<std::uint32_t>(shown_pictures_.size());
  NumberPictureKeys(line, first_key);
  for (ro::ui::ChatInlineImage& image : line.images) {
    if (image.kind == ro::ui::ChatInlineKind::kPicture) {
      // ⚠ UNE VIGNETTE EN ÉCHEC N'A PAS D'ADRESSE : sa clé ne désigne rien.
      if (image.state != ro::ui::ChatPictureState::kFailed && rich_.picture) {
        rich_.picture(line.picture_urls[image.picture_key - first_key], image);
      }
    } else if (image.kind == ro::ui::ChatInlineKind::kItemIcon && rich_.item_icon) {
      image.texture = rich_.item_icon(image.item_id);
    }
  }
  for (std::string& url : line.picture_urls) {
    shown_pictures_.push_back(std::move(url));
  }
  line.picture_urls.clear();
  views.decorated.push_back(std::move(line));
  const DecoratedChatLine& kept = views.decorated.back();
  view.text = kept.text;
  view.links = kept.links;
  view.styles = kept.styles;
  view.images = kept.images;
  view.body_offset = kept.body_offset;
}

bool ChatTabsView::UnreadOf(std::size_t rank, std::span<const ro::sim::ChatLine> chat) {
  const std::span<const ChatTab> tabs = tabs_.tabs();
  if (rank >= tabs.size()) {
    return false;
  }
  if (unread_memo_.size() != tabs.size()) {
    unread_memo_.assign(tabs.size(), UnreadMemo{});
  }
  const ChatTab& tab = tabs[rank];
  UnreadMemo& memo = unread_memo_[rank];
  const bool same = memo.known && memo.lines == chat.size() && memo.last_seq == last_seq_ &&
                    memo.accepted == tab.accepted && memo.content == tab.content &&
                    memo.read_through == tab.read_through &&
                    memo.cleared_through == tab.cleared_through;
  if (!same) {
    memo = UnreadMemo{.known = true,
                      .accepted = tab.accepted,
                      .content = tab.content,
                      .read_through = tab.read_through,
                      .cleared_through = tab.cleared_through,
                      .last_seq = last_seq_,
                      .lines = chat.size(),
                      .unread = TabHasUnread(tab, chat, desk_)};
  }
  return memo.unread;
}

std::string_view ChatTabsView::PictureUrlOf(std::uint32_t key) const {
  return key < shown_pictures_.size() ? std::string_view(shown_pictures_[key]) : std::string_view();
}

bool ChatTabsView::ZoomLook(std::uint32_t group, int steps) {
  const auto found = shown_.find(group);
  if (found == shown_.end() || !found->second.active.has_value()) {
    return false;
  }
  const std::size_t rank = *found->second.active;
  const std::span<const ChatTab> tabs = tabs_.tabs();
  // ⚠ LE RANG A PU VIEILLIR depuis le tri : un onglet retiré ou déplacé entre
  // deux en ferait désigner un autre, peut-être d'une autre fenêtre.
  if (rank >= tabs.size() || tabs[rank].group != group || !tabs[rank].look.own) {
    return false;
  }
  // ⚠ VRAI MÊME AU BOUT DE LA BARRE : l'onglet a son apparence, le geste est le
  // sien. Rendre faux ferait bouger le réglage général, que cet onglet ne suit
  // pas — le joueur ne verrait rien changer, et retrouverait ses autres onglets
  // agrandis.
  TabLook look = tabs[rank].look;
  const int before = std::clamp(look.font_percent, ro::ui::kSmallestChatFontPercent,
                                ro::ui::kLargestChatFontPercent);
  look.font_percent = std::clamp(before + steps * ro::ui::kChatFontStepPercent,
                                 ro::ui::kSmallestChatFontPercent, ro::ui::kLargestChatFontPercent);
  if (look.font_percent != tabs[rank].look.font_percent) {
    tabs_.SetLook(rank, look);
    SaveIfNeeded();
  }
  return true;
}

}  // namespace moonclient
