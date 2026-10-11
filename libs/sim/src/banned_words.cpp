#include "ro/sim/banned_words.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/ascii.h"
#include "ro/core/text.h"

namespace ro::sim {
namespace {

// Ce qui ferme un mot dans le fichier, et ce qui efface la ligne commencée.
constexpr char kWordEnd = '\r';
constexpr char kWordSeparator = '#';
constexpr char kLineFeed = '\n';
constexpr std::string_view kCommentStart = "//";

// Le lien d'objet, en minuscules : il se saute en entier.
constexpr std::string_view kItemLinkOpen = "<iteml>";
constexpr std::string_view kItemLinkClose = "</iteml>";

constexpr char kMaskStar = '*';

// Le mot jusqu'au premier zéro, comme une chaîne C.
[[nodiscard]] std::string_view UpToNul(std::string_view bytes) {
  return bytes.substr(0, bytes.find('\0'));
}

// Vrai pour un mot fait seulement d'espaces et de tabulations.
[[nodiscard]] bool IsBlank(std::string_view word) {
  return word.find_first_not_of(" \t") == std::string_view::npos;
}

[[nodiscard]] std::size_t CodePointCount(std::string_view utf8) {
  std::size_t count = 0;
  for (std::size_t at = 0; at < utf8.size(); ++count) {
    static_cast<void>(core::NextCodePoint(utf8, at));
  }
  return count;
}

}  // namespace

BannedWords::BannedWords(std::vector<std::string> words) {
  words_.reserve(words.size());
  for (std::string& word : words) {
    if (!word.empty()) {
      words_.push_back(core::AsciiLower(word));
    }
  }
}

BannedWords BannedWords::FromMannerText(std::string_view bytes) {
  std::vector<std::string> words;
  const auto keep = [&words](std::string_view token) {
    const std::string_view word = UpToNul(token);
    if (!IsBlank(word)) {
      words.push_back(core::Utf8FromWire(word));
    }
  };
  std::size_t start = 0;
  for (std::size_t at = 0; at < bytes.size(); ++at) {
    const std::string_view token = bytes.substr(start, at - start);
    switch (bytes[at]) {
      case kLineFeed:
        start = at + 1;
        break;
      case kWordEnd:
        if (!token.starts_with(kCommentStart)) {
          keep(token);
        }
        start = at + 1;
        break;
      case kWordSeparator:
        if (!token.starts_with(kCommentStart)) {
          keep(token);
          start = at + 1;
        }
        break;
      default:
        break;
    }
  }
  return BannedWords(std::move(words));
}

std::string BannedWords::Mask(std::string_view utf8_text) const {
  if (words_.empty()) {
    return std::string(utf8_text);
  }
  const std::string lower = core::AsciiLower(utf8_text);
  std::string masked;
  masked.reserve(utf8_text.size());
  std::size_t at = 0;
  while (at < utf8_text.size()) {
    if (lower.compare(at, kItemLinkOpen.size(), kItemLinkOpen) == 0) {
      if (const std::size_t close = lower.find(kItemLinkClose, at); close != std::string::npos) {
        const std::size_t after = close + kItemLinkClose.size();
        masked.append(utf8_text.substr(at, after - at));
        at = after;
      }
    }
    const std::string* found = nullptr;
    for (const std::string& word : words_) {
      if (lower.compare(at, word.size(), word) == 0) {
        found = &word;
        break;
      }
    }
    if (found != nullptr) {
      masked.append(CodePointCount(*found), kMaskStar);
      at += found->size();
      continue;
    }
    if (at >= utf8_text.size()) {
      break;
    }
    std::size_t next = at;
    static_cast<void>(core::NextCodePoint(utf8_text, next));
    masked.append(utf8_text.substr(at, next - at));
    at = next;
  }
  return masked;
}

}  // namespace ro::sim
