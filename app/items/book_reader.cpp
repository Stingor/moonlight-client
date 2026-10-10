#include "items/book_reader.h"

#include <algorithm>
#include <utility>

#include "ro/core/log.h"
#include "ro/net/chat_target.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

// La cadence du client : une ligne toutes les quatre secondes, la première tout
// de suite.
constexpr std::uint32_t kRecitalStepMs = 4000;
// Ce qu'une ligne dite peut porter de caractères. ⚠ LE CLIENT DIT LES LIGNES
// REPLIÉES À LA LARGEUR DE SA FENÊTRE, jamais une phrase entière : une ligne de
// livre dépasse souvent ce que la conversation accepte.
constexpr std::size_t kRecitalLineChars = 70;
// Une ligne faite d'une espace est une ligne vide.
constexpr std::string_view kBlankLine = " ";

// Les lignes d'un livre, dites : sans marques de couleur, coupées aux espaces.
[[nodiscard]] std::vector<std::string> RecitalLines(const std::vector<std::string>& lines) {
  std::vector<std::string> said;
  for (const std::string& line : lines) {
    const std::string plain = ro::ui::BookLinePlain(line);
    std::string row;
    std::size_t from = 0;
    while (from <= plain.size()) {
      const std::size_t space = plain.find(' ', from);
      const std::size_t end = space == std::string::npos ? plain.size() : space;
      const std::string_view word(plain.data() + from, end - from);
      if (!row.empty() && row.size() + 1 + word.size() > kRecitalLineChars) {
        said.push_back(row);
        row.clear();
      }
      if (!row.empty()) {
        row += ' ';
      }
      row += word;
      if (space == std::string::npos) {
        break;
      }
      from = space + 1;
    }
    said.push_back(row);
  }
  return said;
}

}  // namespace

void BookReader::See(ro::vfs::Vfs* vfs, const ro::vfs::BookList* list,
                     const ro::vfs::ItemTable* items) noexcept {
  vfs_ = vfs;
  list_ = list;
  items_ = items;
}

void BookReader::RememberFor(std::filesystem::path client_root, std::uint32_t character_id) {
  client_root_ = std::move(client_root);
  character_id_ = character_id;
  bookmarks_ =
      character_id_ != 0 ? LoadBookBookmarks(client_root_, character_id_) : BookBookmarks{};
}

bool BookReader::Readable(const ro::sim::World& world, std::uint32_t item_id) const {
  if (list_ == nullptr || !list_->Has(item_id)) {
    return false;
  }
  return std::ranges::any_of(
      world.inventory().items(),
      [item_id](const ro::sim::InventoryItem& item) { return item.item_id == item_id; });
}

bool BookReader::Load(std::uint32_t item_id) {
  if (loaded_ == item_id) {
    return true;
  }
  if (vfs_ == nullptr) {
    return false;
  }
  auto read = ro::vfs::ReadBook(*vfs_, item_id);
  if (!read) {
    ro::core::LogWarning("livre {} : {}", item_id, ro::core::Describe(read.error()));
    return false;
  }
  book_ = std::move(*read);
  loaded_ = item_id;
  title_.clear();
  if (items_ != nullptr) {
    if (const ro::vfs::ItemEntry* entry = items_->Find(item_id); entry != nullptr) {
      title_ = entry->name;
    }
  }
  view_ = ro::ui::BookView{.item_id = item_id, .title = title_, .lines = book_.lines};
  if (book_.background.has_value()) {
    view_.background =
        IM_COL32(book_.background->red, book_.background->green, book_.background->blue, 255);
  }
  return true;
}

int BookReader::StartPage(std::uint32_t item_id, bool first_page) const {
  if (first_page) {
    return 1;
  }
  const auto kept = bookmarks_.find(item_id);
  return kept != bookmarks_.end() ? kept->second : 1;
}

void BookReader::Open(std::uint32_t item_id, ro::ui::BookWindowState& window, bool first_page) {
  if (!Load(item_id)) {
    return;
  }
  reciting_ = false;
  recital_line_.clear();
  window.open = true;
  window.raise = true;
  window.place_at_mouse = true;
  window.item_id = item_id;
  window.page = StartPage(item_id, first_page);
}

void BookReader::StartRecital(std::uint32_t item_id, ro::ui::BookWindowState& window,
                              bool first_page, std::uint32_t now_ms) {
  if (!Load(item_id)) {
    return;
  }
  window.open = false;
  recital_ = RecitalLines(book_.lines);
  const auto page = static_cast<std::size_t>(StartPage(item_id, first_page));
  next_ = std::min((page - 1) * ro::ui::kBookLinesPerPage, recital_.size());
  reciting_ = !recital_.empty();
  // ⚠ LA PREMIÈRE LIGNE PART TOUT DE SUITE : l'horloge part déjà échue.
  said_at_ms_ = now_ms - kRecitalStepMs;
  recital_line_.clear();
}

void BookReader::NoteClosed(std::uint32_t item_id, int page) {
  if (item_id == 0 || page <= 0) {
    return;
  }
  const auto [kept, added] = bookmarks_.try_emplace(item_id, page);
  if (!added && kept->second == page) {
    return;
  }
  kept->second = page;
  if (character_id_ != 0) {
    static_cast<void>(SaveBookBookmarks(client_root_, character_id_, bookmarks_));
  }
}

void BookReader::NoteLeaving(const ro::ui::BookWindowState& window) {
  if (window.open) {
    NoteClosed(window.item_id, window.page);
  }
}

void BookReader::Tick(ro::net::Connection& link, const ro::sim::World& world,
                      std::uint32_t now_ms) {
  if (!reciting_ || now_ms - said_at_ms_ < kRecitalStepMs) {
    return;
  }
  if (next_ >= recital_.size()) {
    reciting_ = false;
    recital_line_.clear();
    return;
  }
  // UNE LIGNE VIDE SE SAUTE, une seule par tour — comme le client.
  if ((recital_[next_].empty() || recital_[next_] == kBlankLine) && next_ + 1 < recital_.size()) {
    ++next_;
  }
  recital_line_ = recital_[next_];
  if (!recital_line_.empty() && link.is_open()) {
    const ro::net::ChatIntent said{.target = ro::net::ChatTarget::kPublic, .text = recital_line_};
    if (const auto sent = ro::net::SendChat(link, world.player_name(), said); !sent) {
      ro::core::LogWarning("lecture à voix haute : {}", ro::core::Describe(sent.error()));
    }
  }
  ++next_;
  said_at_ms_ = now_ms;
}

}  // namespace moonclient
