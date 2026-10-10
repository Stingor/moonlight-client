#include "ro/sim/guild.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "ro/core/ascii.h"

namespace ro::sim {
namespace {

[[nodiscard]] bool ValidPosition(std::int32_t position) {
  return position >= 0 && position < kGuildPositionCount;
}

const std::string kNoLabel;

[[nodiscard]] bool SameName(std::string_view left, std::string_view right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (core::AsciiLower(static_cast<std::uint8_t>(left[index])) !=
        core::AsciiLower(static_cast<std::uint8_t>(right[index]))) {
      return false;
    }
  }
  return true;
}

}  // namespace

void GuildBook::TellBelonging(GuildBelonging belonging) {
  // ⚠ UNE AUTRE GUILDE EFFACE L'ANCIENNE avant de s'installer : ses membres,
  // ses postes et ses compétences n'ont rien à voir avec la nouvelle.
  if (belonging_.has_value() && belonging_->guild_id != belonging.guild_id) {
    Leave();
  }
  TellEmblemVersion(belonging.guild_id, belonging.emblem_version);
  belonging_ = std::move(belonging);
}

void GuildBook::Leave() {
  belonging_.reset();
  summary_.reset();
  notice_subject_.clear();
  notice_body_.clear();
  members_.clear();
  members_known_ = false;
  member_cells_.clear();
  positions_ = {};
  remembered_labels_ = {};
  relations_.clear();
  skills_.clear();
  skills_known_ = false;
  skill_points_ = 0;
  bans_.clear();
  bans_known_ = false;
  menu_flags_.reset();
}

void GuildBook::TellSummary(GuildSummary summary) {
  TellEmblemVersion(summary.guild_id, summary.emblem_version);
  summary_ = std::move(summary);
}

void GuildBook::TellNotice(std::string subject, std::string body) {
  notice_subject_ = std::move(subject);
  notice_body_ = std::move(body);
}

void GuildBook::ReplaceMembers(std::vector<GuildMember> members) {
  members_ = std::move(members);
  members_known_ = true;
}

const GuildMember* GuildBook::FindMember(std::uint32_t char_id) const {
  const auto found = std::ranges::find(members_, char_id, &GuildMember::char_id);
  return found == members_.end() ? nullptr : &*found;
}

void GuildBook::TellMemberOnline(std::uint32_t account_id, std::uint32_t char_id, bool online,
                                 std::int16_t sex, std::int16_t hair, std::int16_t hair_palette) {
  for (GuildMember& member : members_) {
    if (member.account_id != account_id || member.char_id != char_id) {
      continue;
    }
    member.online = online;
    member.sex = sex;
    member.hair = hair;
    member.hair_palette = hair_palette;
  }
}

void GuildBook::TellPositionMoves(std::span<const GuildPositionMove> moves) {
  for (const GuildPositionMove& move : moves) {
    for (GuildMember& member : members_) {
      if (member.account_id == move.account_id && member.char_id == move.char_id) {
        member.position = move.position;
      }
    }
  }
}

void GuildBook::RemoveMember(std::uint32_t char_id) {
  std::erase_if(members_,
                [char_id](const GuildMember& member) { return member.char_id == char_id; });
}

const GuildMemberCell* GuildBook::FindMemberCell(std::uint32_t account_id) const {
  const auto found = member_cells_.find(account_id);
  return found == member_cells_.end() ? nullptr : &found->second;
}

void GuildBook::TellMemberCell(std::uint32_t account_id, std::optional<GuildMemberCell> cell) {
  if (cell.has_value()) {
    member_cells_[account_id] = *cell;
  } else {
    member_cells_.erase(account_id);
  }
}

void GuildBook::TellPositionName(std::int32_t position, std::string name) {
  if (!ValidPosition(position)) {
    return;
  }
  const auto slot = static_cast<std::size_t>(position);
  if (!name.empty()) {
    remembered_labels_[slot] = name;
  }
  positions_[slot].name = std::move(name);
  positions_[slot].has_name = true;
}

void GuildBook::TellPositionRights(std::int32_t position, std::int32_t rights,
                                   std::int32_t pay_rate) {
  if (!ValidPosition(position)) {
    return;
  }
  GuildPosition& slot = positions_[static_cast<std::size_t>(position)];
  slot.rights = rights;
  slot.pay_rate = pay_rate;
  slot.has_rights = true;
}

const std::string& GuildBook::PositionLabel(std::int32_t position) const {
  if (!ValidPosition(position)) {
    return kNoLabel;
  }
  const auto slot = static_cast<std::size_t>(position);
  return positions_[slot].name.empty() ? remembered_labels_[slot] : positions_[slot].name;
}

bool GuildBook::IsMaster(std::uint32_t char_id) const {
  if (!belonging_.has_value()) {
    return false;
  }
  if (summary_.has_value() && summary_->master_char_id != 0) {
    return static_cast<std::uint32_t>(summary_->master_char_id) == char_id;
  }
  if (summary_.has_value() && !summary_->master_name.empty()) {
    const GuildMember* own = FindMember(char_id);
    return own != nullptr && SameName(own->name, summary_->master_name);
  }
  return belonging_->master;
}

GuildRights GuildBook::RightsOf(std::uint32_t char_id) const {
  const GuildMember* own = FindMember(char_id);
  const bool master = IsMaster(char_id);
  if (own == nullptr || !ValidPosition(own->position) ||
      !positions_[static_cast<std::size_t>(own->position)].has_rights) {
    return GuildRights{.may_invite = master, .may_expel = master};
  }
  const std::int32_t rights = positions_[static_cast<std::size_t>(own->position)].rights;
  return GuildRights{
      .known = true,
      .may_invite = (rights & kGuildMayInvite) != 0,
      .may_expel = (rights & kGuildMayExpel) != 0,
      .may_storage = (rights & kGuildMayStorage) != 0,
  };
}

bool GuildBook::MayWield(std::uint16_t skill_id, std::uint32_t char_id) const {
  return in_guild() && (skill_id == kMemberGuildSkill || IsMaster(char_id));
}

void GuildBook::ReplaceRelations(std::vector<GuildRelation> relations) {
  relations_ = std::move(relations);
}

void GuildBook::RemoveRelation(std::int32_t guild_id, GuildRelationKind kind) {
  std::erase_if(relations_, [guild_id, kind](const GuildRelation& relation) {
    return relation.guild_id == guild_id && relation.kind == kind;
  });
}

void GuildBook::ReplaceSkills(int points, std::vector<GuildSkill> skills) {
  skill_points_ = points;
  skills_ = std::move(skills);
  skills_known_ = true;
}

const GuildSkill* GuildBook::FindSkill(std::uint16_t skill_id) const {
  const auto found = std::ranges::find(skills_, skill_id, &GuildSkill::id);
  return found == skills_.end() ? nullptr : &*found;
}

void GuildBook::ReplaceBans(std::vector<GuildBan> bans) {
  bans_ = std::move(bans);
  bans_known_ = true;
}

void GuildBook::TellCreation(GuildCreation result) {
  creation_ = result;
  ++creation_seq_;
}

std::int32_t GuildBook::EmblemVersionOf(std::int32_t guild_id) const {
  const auto found = emblem_versions_.find(guild_id);
  return found == emblem_versions_.end() ? 0 : found->second;
}

void GuildBook::TellEmblemVersion(std::int32_t guild_id, std::int32_t version) {
  if (guild_id <= 0) {
    return;
  }
  emblem_versions_[guild_id] = version;
}

const PushedEmblem* GuildBook::PushedEmblemOf(std::int32_t guild_id) const {
  const auto found = pushed_emblems_.find(guild_id);
  return found == pushed_emblems_.end() ? nullptr : &found->second;
}

void GuildBook::TellPushedEmblem(std::int32_t guild_id, PushedEmblem emblem) {
  if (guild_id <= 0) {
    return;
  }
  TellEmblemVersion(guild_id, emblem.version);
  pushed_emblems_[guild_id] = std::move(emblem);
}

}  // namespace ro::sim
