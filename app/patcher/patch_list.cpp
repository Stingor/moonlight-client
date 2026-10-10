#include "patcher/patch_list.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <expected>
#include <iterator>
#include <string>
#include <system_error>

namespace moonclient::patcher {
namespace {

constexpr std::string_view kBlanks = " \t\r";
constexpr std::string_view kCommentSlashes = "//";
constexpr std::string_view kCommentHash = "#";
constexpr std::size_t kSha256HexLength = 64;

[[nodiscard]] std::unexpected<ro::core::Error> Broken(std::size_t line_number,
                                                      const std::string& why) {
  return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                        "liste des patchs, ligne " + std::to_string(line_number) + " : " + why);
}

// Le mot suivant de `rest`, qui avance au-delà.
[[nodiscard]] std::string_view NextWord(std::string_view& rest) {
  const std::size_t start = rest.find_first_not_of(kBlanks);
  if (start == std::string_view::npos) {
    rest = {};
    return {};
  }
  rest.remove_prefix(start);
  const std::size_t end = std::min(rest.find_first_of(kBlanks), rest.size());
  const std::string_view word = rest.substr(0, end);
  rest.remove_prefix(end);
  return word;
}

[[nodiscard]] bool IsPlainFileName(std::string_view name) {
  return !name.empty() && !name.starts_with('.') &&
         name.find_first_of("/\\:") == std::string_view::npos &&
         name.find("..") == std::string_view::npos;
}

[[nodiscard]] bool IsHexDigit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

[[nodiscard]] std::string LowerHex(std::string_view hex) {
  std::string lowered(hex);
  std::ranges::transform(lowered, lowered.begin(), [](char c) {
    return (c >= 'A' && c <= 'F') ? static_cast<char>(c - 'A' + 'a') : c;
  });
  return lowered;
}

}  // namespace

ro::core::Result<std::vector<PatchEntry>> ParsePatchList(std::string_view text) {
  std::vector<PatchEntry> patches;
  std::size_t line_number = 0;
  while (!text.empty()) {
    ++line_number;
    const std::size_t line_end = std::min(text.find('\n'), text.size());
    std::string_view rest = text.substr(0, line_end);
    text.remove_prefix(std::min(line_end + 1, text.size()));

    const std::string_view index_word = NextWord(rest);
    if (index_word.empty() || index_word.starts_with(kCommentSlashes) ||
        index_word.starts_with(kCommentHash)) {
      continue;
    }
    PatchEntry patch;
    const auto [parsed_end, parse_error] = std::from_chars(
        index_word.data(), index_word.data() + index_word.size(), patch.patch_index);
    if (parse_error != std::errc() || parsed_end != index_word.data() + index_word.size()) {
      return Broken(line_number, "« " + std::string(index_word) + " » n'est pas un index");
    }
    const std::string_view archive_word = NextWord(rest);
    if (!IsPlainFileName(archive_word)) {
      return Broken(line_number,
                    "« " + std::string(archive_word) + " » n'est pas un nom d'archive");
    }
    patch.archive_name = std::string(archive_word);
    const std::string_view hash_word = NextWord(rest);
    if (!hash_word.empty()) {
      if (hash_word.size() != kSha256HexLength || !std::ranges::all_of(hash_word, IsHexDigit)) {
        return Broken(line_number, "« " + std::string(hash_word) + " » n'est pas un SHA-256");
      }
      patch.sha256_hex = LowerHex(hash_word);
    }
    patches.push_back(std::move(patch));
  }
  std::ranges::stable_sort(patches, {}, &PatchEntry::patch_index);
  return patches;
}

std::vector<PatchEntry> PendingPatches(const std::vector<PatchEntry>& patches,
                                       std::optional<std::uint64_t> last_applied_index) {
  const bool state_is_known =
      last_applied_index.has_value() && std::ranges::any_of(patches, [&](const PatchEntry& patch) {
        return patch.patch_index == *last_applied_index;
      });
  if (!state_is_known) {
    return patches;
  }
  std::vector<PatchEntry> pending;
  std::ranges::copy_if(patches, std::back_inserter(pending), [&](const PatchEntry& patch) {
    return patch.patch_index > *last_applied_index;
  });
  return pending;
}

}  // namespace moonclient::patcher
