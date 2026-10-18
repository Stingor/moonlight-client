#include "ro/vfs/books.h"

#include <charconv>
#include <string_view>

#include "ro/vfs/encoding.h"

#include "install_table.h"

namespace ro::vfs {
namespace {

constexpr char kBackgroundMark = '%';
// « %RRGGBB » : la marque et six chiffres hexadécimaux.
constexpr std::size_t kBackgroundLength = 7;
constexpr int kHexBase = 16;
constexpr std::size_t kHexPair = 2;

[[nodiscard]] std::string_view Trimmed(std::string_view line) {
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
    line.remove_suffix(1);
  }
  return line;
}

[[nodiscard]] std::optional<std::uint8_t> HexByte(std::string_view pair) {
  unsigned value = 0;
  const auto [end, error] =
      std::from_chars(pair.data(), pair.data() + pair.size(), value, kHexBase);
  if (error != std::errc() || end != pair.data() + pair.size()) {
    return std::nullopt;
  }
  return static_cast<std::uint8_t>(value);
}

[[nodiscard]] std::optional<BookBackground> BackgroundOf(std::string_view line) {
  if (line.size() < kBackgroundLength || line.front() != kBackgroundMark) {
    return std::nullopt;
  }
  const auto red = HexByte(line.substr(1, kHexPair));
  const auto green = HexByte(line.substr(1 + kHexPair, kHexPair));
  const auto blue = HexByte(line.substr(1 + 2 * kHexPair, kHexPair));
  if (!red || !green || !blue) {
    return std::nullopt;
  }
  return BookBackground{.red = *red, .green = *green, .blue = *blue};
}

[[nodiscard]] std::string BookPath(std::uint32_t item_id) {
  return "data\\book\\" + std::to_string(item_id) + ".txt";
}

}  // namespace

Book ParseBook(std::string_view cp949) {
  Book book;
  bool first = true;
  std::size_t from = 0;
  while (from <= cp949.size()) {
    const std::size_t end = cp949.find('\n', from);
    const std::string_view line =
        Trimmed(cp949.substr(from, end == std::string_view::npos ? cp949.npos : end - from));
    // ⚠ LA COULEUR N'EST LUE QU'EN TÊTE : un « % » plus loin est du texte.
    if (first) {
      first = false;
      if (const auto background = BackgroundOf(line); background.has_value()) {
        book.background = background;
        if (end == std::string_view::npos) {
          break;
        }
        from = end + 1;
        continue;
      }
    }
    book.lines.push_back(Cp949ToUtf8Lossy(line));
    if (end == std::string_view::npos) {
      break;
    }
    from = end + 1;
  }
  // La dernière ligne vide d'un fichier terminé par un saut n'est pas une ligne.
  if (!book.lines.empty() && book.lines.back().empty()) {
    book.lines.pop_back();
  }
  return book;
}

core::Result<BookList> ParseBookList(std::string_view yaml_text) {
  BookList list;
  auto read = VisitTableSection(yaml_text, kBookListPath, "books", YAML::NodeType::Sequence,
                                [&list](const YAML::Node& books) {
                                  for (const YAML::Node& book : books) {
                                    list.item_ids.insert(ItemIdOf(book));
                                  }
                                });
  if (!read) {
    return std::unexpected(read.error());
  }
  return list;
}

core::Result<BookList> ReadBookList(Vfs& vfs) {
  auto text = ReadOptionalTable(vfs, kBookListPath);
  if (!text) {
    return std::unexpected(text.error());
  }
  return text->has_value() ? ParseBookList(**text) : BookList{};
}

core::Result<Book> ReadBook(Vfs& vfs, std::uint32_t item_id) {
  auto bytes = vfs.Read(BookPath(item_id));
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  return ParseBook(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

}  // namespace ro::vfs
