#include "chat/chat_monster_link.h"

#include <charconv>
#include <utility>

#include "chat/chat_item_link.h"

namespace moonclient {
namespace {

constexpr char kFieldSeparator = ':';

[[nodiscard]] const char* RankTag(std::uint8_t rank) {
  if (rank == kMonsterRankMvp) {
    return "[MVP]";
  }
  if (rank == kMonsterRankBoss) {
    return "[Boss]";
  }
  return "[Mob]";
}

}  // namespace

std::string BuildMonsterLink(const MonsterLink& link) {
  if (link.monster_id == 0 || link.name.empty() ||
      link.name.find(kMonsterLinkClose) != std::string::npos) {
    return {};
  }
  std::string tag(kMonsterLinkOpen);
  tag += std::to_string(link.monster_id);
  tag.push_back(kFieldSeparator);
  tag += std::to_string(link.rank > kMonsterRankMvp ? kMonsterRankOrdinary : link.rank);
  tag.push_back(kFieldSeparator);
  tag += link.name;
  tag += kMonsterLinkClose;
  return tag;
}

bool ParseMonsterLink(std::string_view text, MonsterLink* out, std::size_t* after) {
  if (after != nullptr) {
    *after = 0;
  }
  if (!text.starts_with(kMonsterLinkOpen)) {
    return false;
  }
  const std::size_t body_at = kMonsterLinkOpen.size();
  const std::size_t close = text.find(kMonsterLinkClose, body_at);
  if (after != nullptr) {
    *after = close == std::string_view::npos ? text.size() : close + kMonsterLinkClose.size();
  }
  if (close == std::string_view::npos) {
    return false;
  }
  const std::string_view body = text.substr(body_at, close - body_at);
  const std::size_t first = body.find(kFieldSeparator);
  const std::size_t second =
      first == std::string_view::npos ? first : body.find(kFieldSeparator, first + 1);
  if (second == std::string_view::npos) {
    return false;
  }
  MonsterLink read;
  const std::string_view id_field = body.substr(0, first);
  if (std::from_chars(id_field.data(), id_field.data() + id_field.size(), read.monster_id).ec !=
      std::errc{}) {
    return false;
  }
  const std::string_view rank_field = body.substr(first + 1, second - first - 1);
  int rank = 0;
  if (std::from_chars(rank_field.data(), rank_field.data() + rank_field.size(), rank).ec !=
          std::errc{} ||
      rank < kMonsterRankOrdinary || rank > kMonsterRankMvp) {
    rank = kMonsterRankOrdinary;
  }
  read.rank = static_cast<std::uint8_t>(rank);
  read.name = std::string(body.substr(second + 1));
  if (read.monster_id == 0 || read.name.empty()) {
    return false;
  }
  if (out != nullptr) {
    *out = std::move(read);
  }
  return true;
}

std::size_t FindMonsterLink(std::string_view text, std::size_t from) {
  return text.find(kMonsterLinkOpen, from);
}

std::string MonsterLinkName(const MonsterLink& link) {
  std::string name = RankTag(link.rank);
  name.push_back(' ');
  name += link.name;
  return name;
}

std::string MonsterBestiaryUrl(std::uint32_t monster_id) {
  return std::string(kBestiaryPageUrl) + std::to_string(monster_id);
}

std::string MonsterLinkLabel(const MonsterLink& link) {
  return ChatLinkLabel(MonsterLinkName(link));
}

}  // namespace moonclient
