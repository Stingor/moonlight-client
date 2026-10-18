#include "party/party_compose.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"

namespace moonclient {
namespace {

// Les bits du serveur seuls : la fenêtre ajoute elle-même celui de la carte.
constexpr std::uint8_t kServerShareBits =
    ro::sim::kShareIdle | ro::sim::kShareDead | ro::sim::kShareBusy;

// Une jauge n'est connue que si le serveur en a donné le plafond.
[[nodiscard]] bool GaugeKnown(const ro::sim::PartyGauge& gauge) { return gauge.max > 0; }

// UN FAUX MEMBRE : de quoi remplir une tuile, sans rien du serveur.
struct FakeMember {
  std::uint16_t job = 0;
  std::uint16_t base_level = 0;
  // En pour cent du plafond ; négatif : inconnus, le membre est hors de portée.
  int hp_percent = 0;
  int sp_percent = 0;
  bool offline = false;
  bool leader = false;
};

// Une tuile par état que la grille distingue, dans des jobs variés pour que les
// icônes diffèrent.
constexpr int kUnknownGauge = -1;
constexpr std::array<FakeMember, 7> kFakeMembers = {{
    {.job = 4008, .base_level = 99, .hp_percent = 100, .sp_percent = 80, .leader = true},
    {.job = 4009, .base_level = 97, .hp_percent = 72, .sp_percent = 45},
    {.job = 4012, .base_level = 95, .hp_percent = 40, .sp_percent = 60},
    {.job = 4010, .base_level = 92, .hp_percent = 12, .sp_percent = 10},
    {.job = 4013, .base_level = 90, .hp_percent = 0, .sp_percent = 0},
    {.job = 4015, .base_level = 88, .hp_percent = kUnknownGauge, .sp_percent = kUnknownGauge},
    {.job = 4016, .base_level = 85, .offline = true},
}};
// Des plafonds plausibles : seul leur rapport compte à l'écran.
constexpr int kFakeMaxHp = 24000;
constexpr int kFakeMaxSp = 1200;
constexpr int kWholePercent = 100;

// Le rang d'une demande d'amitié, pour savoir si elle est neuve.
[[nodiscard]] std::uint64_t FriendInviteKey(const ro::sim::FriendInvitation& invitation) {
  constexpr unsigned kCharShift = 32;
  return (static_cast<std::uint64_t>(invitation.char_id) << kCharShift) | invitation.account_id;
}

// Les cases qu'une surface montre : toutes avec SON aperçu, les vraies seules
// sans lui. Voir `PartyStatuses::genuine`.
[[nodiscard]] std::span<const ro::ui::StatusCellView> ShownStatuses(const PartyStatuses& statuses,
                                                                    bool preview) {
  return preview ? statuses.cells
                 : statuses.cells.first(std::min(statuses.genuine, statuses.cells.size()));
}

[[nodiscard]] bool StatusesKnown(const PartyStatuses& statuses, bool preview) {
  return preview ? statuses.known : statuses.genuinely_known;
}

}  // namespace

const char* PartyLens::MvpInviteRefusal(std::string_view /*name*/, bool /*same_account*/) const {
  return nullptr;
}

ro::ui::PartyMemberView PartyViews::MemberRow(const ro::sim::PartyMember& member,
                                              const PartyComposeInput& input, PartyLens& lens,
                                              PartyStatuses& statuses) const {
  ro::ui::PartyMemberView row;
  row.account_id = member.account_id;
  row.char_id = member.char_id;
  row.name = member.name;
  row.map_raw = member.map;
  row.map_name = lens.MapName(member.map);
  row.leader = member.leader;
  row.offline = member.offline;
  row.is_me = member.account_id == self_.account_id;
  row.job_name = lens.JobName(member.job);
  row.base_level = member.base_level;
  row.job_icon = lens.JobIcon(member.job);
  row.actor_present = !member.offline && lens.ActorPresent(member.account_id);
  if ((input.head_mode & ro::ui::kPartyHeadInParty) != 0 && row.actor_present) {
    row.head = lens.Head(member.account_id);
  }
  row.is_friend =
      input.friends != nullptr && input.friends->Find(member.account_id, member.char_id) != nullptr;
  // ⚠ PAS DE RAISON SUR MA LIGNE : on ne s'invite pas soi-même, et le menu ne
  // l'offre pas.
  if (!row.is_me) {
    row.mvp_refusal = lens.MvpInviteRefusal(member.name, member.account_id == self_.account_id);
  }

  if (row.is_me) {
    row.position_known = self_.position_known;
    row.x = self_.x;
    row.y = self_.y;
    row.hp_known = self_.hp_known;
    row.hp = self_.hp;
    row.max_hp = self_.max_hp;
    row.sp_known = self_.sp_known;
    row.sp = self_.sp;
    row.max_sp = self_.max_sp;
  } else {
    if (member.position.has_value()) {
      row.position_known = true;
      row.x = member.position->x;
      row.y = member.position->y;
    }
    if (member.health.has_value() && GaugeKnown(*member.health)) {
      row.hp_known = true;
      row.hp = member.health->current;
      row.max_hp = member.health->max;
    }
    if (const std::optional<ro::sim::PartyGauge> magic =
            input.party->FreshMagic(member.account_id, input.now_ms);
        magic.has_value() && GaugeKnown(*magic)) {
      row.sp_known = true;
      row.sp = magic->current;
      row.max_sp = magic->max;
    }
  }
  if (row.position_known) {
    const PartyMinimap minimap = lens.Minimap(member.map);
    row.minimap = minimap.image;
    row.map_width_cells = minimap.width_cells;
    row.map_height_cells = minimap.height_cells;
  }
  row.share_flags = static_cast<std::uint8_t>(
      input.party->ShareFlagsFor(member.account_id, self_.account_id) & kServerShareBits);
  statuses = PartyStatuses();
  if (!member.offline) {
    statuses = lens.Statuses(member.account_id, row.is_me);
    row.statuses = statuses.cells;
  }
  return row;
}

ro::ui::PartyMemberView PartyViews::SelfRow(PartyLens& lens, PartyStatuses& statuses) const {
  ro::ui::PartyMemberView row;
  row.account_id = self_.account_id;
  row.char_id = self_.char_id;
  row.name = self_.name;
  row.map_raw = self_.map_raw;
  row.map_name = lens.MapName(self_.map_raw);
  row.is_me = true;
  row.job_name = lens.JobName(self_.job);
  row.base_level = self_.base_level;
  row.job_icon = lens.JobIcon(self_.job);
  row.actor_present = lens.ActorPresent(self_.account_id);
  row.position_known = self_.position_known;
  row.x = self_.x;
  row.y = self_.y;
  row.hp_known = self_.hp_known;
  row.hp = self_.hp;
  row.max_hp = self_.max_hp;
  row.sp_known = self_.sp_known;
  row.sp = self_.sp;
  row.max_sp = self_.max_sp;
  statuses = lens.Statuses(self_.account_id, true);
  row.statuses = statuses.cells;
  return row;
}

void PartyViews::ComposeFriends(const PartyComposeInput& input, PartyLens& lens) {
  friends_.clear();
  if (input.friends == nullptr) {
    return;
  }
  const bool heads = (input.head_mode & ro::ui::kPartyHeadInFriends) != 0;
  friends_.reserve(input.friends->friends().size());
  for (const ro::sim::Friend& chum : input.friends->friends()) {
    ro::ui::FriendView row;
    row.account_id = chum.account_id;
    row.char_id = chum.char_id;
    row.name = chum.name;
    row.online = chum.online;
    row.actor_present = chum.online && lens.ActorPresent(chum.account_id);
    if (heads && row.actor_present) {
      row.head = lens.Head(chum.account_id);
    }
    const ro::sim::PartyMember* member = input.party->Find(chum.account_id);
    row.in_my_party = member != nullptr && member->char_id == chum.char_id;
    row.mvp_refusal = lens.MvpInviteRefusal(chum.name, chum.account_id == self_.account_id);
    friends_.push_back(row);
  }
}

void PartyViews::ComposeTiles(const PartyComposeInput& input, bool self_listed) {
  tiles_.clear();
  const auto add = [this, &input](const ro::ui::PartyMemberView& row, const PartyStatuses& statuses,
                                  bool statuses_known) {
    ro::ui::PartyGridTileView tile;
    tile.account_id = row.account_id;
    tile.name = row.name;
    tile.job_label = row.job_name;
    tile.map_label = row.map_name;
    tile.job_icon = row.job_icon;
    tile.base_level = row.base_level;
    tile.is_me = row.is_me;
    tile.is_leader = row.leader;
    tile.is_target = input.target_id != 0 && input.target_id == row.account_id;
    tile.offline = row.offline;
    tile.in_sight = row.is_me || row.actor_present;
    tile.same_map = !row.map_raw.empty() && row.map_raw == self_.map_raw;
    tile.hp_known = row.hp_known;
    tile.hp = row.hp;
    tile.max_hp = row.max_hp;
    tile.sp_known = row.sp_known;
    tile.sp = row.sp;
    tile.max_sp = row.max_sp;
    tile.statuses_known = statuses_known;
    tile.statuses = ShownStatuses(statuses, input.grid_preview);
    tiles_.push_back(tile);
  };
  tiles_.reserve(members_.size() + 1);
  if (!self_listed) {
    add(self_row_, self_statuses_, true);
  }
  for (std::size_t rank = 0; rank < members_.size(); ++rank) {
    const ro::ui::PartyMemberView& row = members_[rank];
    const PartyStatuses& statuses = members_statuses_[rank];
    add(row, statuses, StatusesKnown(statuses, input.grid_preview) && !row.offline);
  }
}

void PartyViews::ComposeFakeTiles(const PartyComposeInput& input, PartyLens& lens) {
  // Les états de MA tuile, prêtés aux faux membres en vue : ce sont les faux
  // états quand l'aperçu de la GRILLE est allumé, les miens sinon.
  const PartyStatuses* mine = &self_statuses_;
  for (std::size_t rank = 0; rank < members_.size(); ++rank) {
    if (members_[rank].is_me) {
      mine = &members_statuses_[rank];
    }
  }
  const std::span<const ro::ui::StatusCellView> borrowed = ShownStatuses(*mine, input.grid_preview);
  fake_names_.clear();
  fake_names_.reserve(kFakeMembers.size());
  for (std::size_t rank = 0; rank < kFakeMembers.size(); ++rank) {
    fake_names_.emplace_back(
        ro::core::Printed(ro::core::Tr("Membre fictif %d"), static_cast<int>(rank + 1)));
  }
  for (std::size_t rank = 0; rank < kFakeMembers.size(); ++rank) {
    const FakeMember& fake = kFakeMembers[rank];
    const bool in_sight = !fake.offline && fake.hp_percent >= 0;
    ro::ui::PartyGridTileView tile;
    tile.account_id = kFakeMemberAccountBase + static_cast<std::uint32_t>(rank);
    tile.name = fake_names_[rank];
    tile.job_label = lens.JobName(fake.job);
    tile.map_label = lens.MapName(self_.map_raw);
    tile.job_icon = lens.JobIcon(fake.job);
    tile.base_level = fake.base_level;
    tile.is_leader = fake.leader;
    tile.offline = fake.offline;
    tile.in_sight = in_sight;
    tile.same_map = !fake.offline;
    tile.hp_known = in_sight;
    tile.max_hp = kFakeMaxHp;
    tile.hp = in_sight ? kFakeMaxHp * fake.hp_percent / kWholePercent : 0;
    tile.sp_known = in_sight;
    tile.max_sp = kFakeMaxSp;
    tile.sp = in_sight ? kFakeMaxSp * fake.sp_percent / kWholePercent : 0;
    tile.statuses_known = in_sight;
    tile.statuses = in_sight ? borrowed : std::span<const ro::ui::StatusCellView>();
    tiles_.push_back(tile);
  }
}

void PartyViews::ComposeInvite(const PartyComposeInput& input) {
  window_.invite = ro::ui::PartyInviteView();
  const std::optional<ro::sim::PartyInvitation>& party = input.party->invitation();
  // ⚠ UNE RÉFÉRENCE À LA LISTE, jamais à une copie : la vue garde le nom.
  static const std::optional<ro::sim::FriendInvitation> kNoFriendInvite;
  const std::optional<ro::sim::FriendInvitation>& chum =
      input.friends != nullptr ? input.friends->invitation() : kNoFriendInvite;
  // La plus récente passe devant : une demande NEUVE prend la place.
  const std::optional<std::uint32_t> party_key =
      party.has_value() ? std::optional<std::uint32_t>(party->party_id) : std::nullopt;
  const std::optional<std::uint64_t> friend_key =
      chum.has_value() ? std::optional<std::uint64_t>(FriendInviteKey(*chum)) : std::nullopt;
  if (party_key.has_value() && party_key != seen_party_invite_) {
    party_invite_first_ = true;
  }
  if (friend_key.has_value() && friend_key != seen_friend_invite_) {
    party_invite_first_ = false;
  }
  seen_party_invite_ = party_key;
  seen_friend_invite_ = friend_key;

  const bool show_party = party.has_value() && (party_invite_first_ || !chum.has_value());
  if (show_party) {
    window_.invite.active = true;
    window_.invite.party_id = party->party_id;
    window_.invite.name = party->party_name;
  } else if (chum.has_value()) {
    window_.invite.active = true;
    window_.invite.is_friend = true;
    window_.invite.account_id = chum->account_id;
    window_.invite.char_id = chum->char_id;
    window_.invite.name = chum->name;
  }
}

void PartyViews::Compose(const PartyComposeInput& input, PartyLens& lens) {
  self_ = input.self;
  window_ = ro::ui::PartyFriendView();
  grid_ = ro::ui::PartyGridView();
  members_.clear();
  members_statuses_.clear();
  self_statuses_ = PartyStatuses();
  if (input.party == nullptr) {
    return;
  }
  const ro::sim::PartyRoster& party = *input.party;

  window_.mvp_invites_withdrawn = input.mvp_invites_withdrawn;
  window_.in_party = party.in_party();
  window_.i_am_leader = party.AmILeader(self_.account_id);
  window_.party_name = party.name();
  if (window_.in_party && party.options().has_value()) {
    const ro::sim::PartyOptions& options = *party.options();
    window_.options = ro::ui::PartyOptionsView{.known = true,
                                               .exp = options.exp_shared ? 1 : 0,
                                               .pickup = options.pickup_shared ? 1 : 0,
                                               .share = options.loot_shared ? 1 : 0};
  }
  window_.share_known = party.share_known();
  window_.share_idle_seconds = party.share_idle_s();

  bool self_listed = false;
  members_.reserve(party.members().size());
  for (const ro::sim::PartyMember& member : party.members()) {
    PartyStatuses statuses;
    members_.push_back(MemberRow(member, input, lens, statuses));
    members_statuses_.push_back(statuses);
    self_listed = self_listed || members_.back().is_me;
  }
  self_row_ = self_listed ? ro::ui::PartyMemberView() : SelfRow(lens, self_statuses_);
  ComposeFriends(input, lens);
  ComposeTiles(input, self_listed);
  if (input.fake_members) {
    ComposeFakeTiles(input, lens);
  }
  // LA LISTE PREND SES CASES EN DERNIER : les tuiles viennent de lire les
  // mêmes, avec l'aperçu de la grille.
  for (std::size_t rank = 0; rank < members_.size(); ++rank) {
    members_[rank].statuses = ShownStatuses(members_statuses_[rank], input.window_preview);
  }
  ComposeInvite(input);

  // ⚠ LES VUES SE PRENNENT APRÈS LE REMPLISSAGE : un span pris plus tôt pendrait
  // à la première réallocation.
  window_.members = members_;
  window_.friends = friends_;
  window_.crown = lens.Crown();
  window_.target_account_id = input.target_id;
  window_.grid_offered = true;
  window_.grid_enabled = input.grid_shown;
  grid_.tiles = tiles_;
  grid_.i_am_leader = window_.i_am_leader;
}

}  // namespace moonclient
