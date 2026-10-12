#include "items/book_reader.h"

#include <algorithm>
#include <utility>

#include "ro/core/log.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

// La cadence du client : une ligne quand PLUS de quatre secondes ont passé
// depuis la précédente, la première dès l'image suivante.
constexpr std::uint32_t kRecitalStepMs = 4000;
// LA LIGNE DITE, dans la conversation : un rose pâle, sans orateur ni nom, dans
// la case générale — celle des lignes que le client écrit lui-même.
constexpr std::uint32_t kRecitalChatColor = 0xFFE4E1;
// 🔴 LE CLIENT DIT LES LIGNES QUE SA FENÊTRE MONTRE, repliées en pixels à sa
// largeur, jamais une phrase entière. Ce repli en caractères n'est qu'un SECOURS,
// tant que la fenêtre n'a jamais été dessinée : aucune largeur n'est mesurée.
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

void BookReader::StopRecital() noexcept {
  reciting_ = false;
  recital_line_.clear();
}

void BookReader::Open(std::uint32_t item_id, ro::ui::BookWindowState& window, bool first_page) {
  // LE MÊME LIVRE, DÉJÀ SOUS LES YEUX : le bouton ne fait rien.
  if (!reciting_ && window.open && window.item_id == item_id) {
    return;
  }
  const bool same_recital = reciting_ && loaded_ == item_id;
  if (!Load(item_id)) {
    return;
  }
  StopRecital();
  window.open = true;
  window.raise = true;
  window.place_at_mouse = true;
  window.item_id = item_id;
  window.page = same_recital ? recital_page_ : StartPage(item_id, first_page);
}

void BookReader::StartRecital(std::uint32_t item_id, ro::ui::BookWindowState& window,
                              bool first_page, std::uint32_t now_ms) {
  if (reciting_) {
    StopRecital();
    return;
  }
  // Le livre ouvert se récite depuis sa page.
  const bool from_window = window.open && window.item_id == item_id;
  if (!Load(item_id)) {
    return;
  }
  recital_page_ = from_window ? window.page : StartPage(item_id, first_page);
  window.open = false;
  recital_ = RecitalLines(book_.lines);
  const auto page = static_cast<std::size_t>(std::max(recital_page_, 1));
  next_ = (page - 1) * ro::ui::kBookLinesPerPage;
  reciting_ = !book_.lines.empty();
  // ⚠ LA PREMIÈRE LIGNE PART À L'IMAGE SUIVANTE : l'horloge part déjà échue.
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
  StopRecital();
}

const std::vector<std::string>* BookReader::RecitalRows(
    const ro::ui::BookWindowState& window) const noexcept {
  if (window.rows_for == loaded_ && !window.rows.empty()) {
    return &window.rows;
  }
  // Une largeur déjà mesurée : la fenêtre replie ce livre à sa prochaine image.
  if (window.rows_width > 0.0F) {
    return nullptr;
  }
  return &recital_;
}

bool BookReader::Tick(ro::sim::World& world, const ro::ui::BookWindowState& window,
                      std::uint32_t now_ms) {
  if (!reciting_ || now_ms - said_at_ms_ <= kRecitalStepMs) {
    return false;
  }
  const std::vector<std::string>* const rows = RecitalRows(window);
  if (rows == nullptr) {
    return false;
  }
  if (next_ >= rows->size()) {
    StopRecital();
    return false;
  }
  // UNE LIGNE VIDE SE SAUTE, une seule par tour — comme le client.
  const auto blank = [](const std::string& plain) { return plain.empty() || plain == kBlankLine; };
  if (blank(ro::ui::BookLinePlain((*rows)[next_])) && next_ + 1 < rows->size()) {
    ++next_;
  }
  recital_line_ = ro::ui::BookLinePlain((*rows)[next_]);
  ++next_;
  said_at_ms_ = now_ms;
  // 🔴 POUR LE SEUL LECTEUR : rien ne part au serveur, aucune tête ne parle.
  if (!recital_line_.empty()) {
    world.RecordChat(ro::sim::ChatKind::kServer, ro::sim::ChatType::kPublic, 0, recital_line_,
                     now_ms, kRecitalChatColor);
  }
  return true;
}

}  // namespace moonclient
