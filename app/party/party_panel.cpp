// La fenêtre Groupe / Amis et la grille, vues de la partie : ce que la
// composition demande au jeu, l'interrogation du PS, les avis du serveur et le
// départ du groupe.

#include "party/party_panel.h"

#include <algorithm>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/net/party.h"
#include "ro/net/social_text.h"
#include "ro/net/target_info.h"
#include "ro/render/skin_pieces.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "world/actor_styles.h"
#include "world/actor_view.h"
#include "world/click_action.h"
#include "world/map_sign_stage.h"

namespace moonclient {
namespace {

// Combien de têtes NEUVES une image accepte de composer, et leur taille.
constexpr int kHeadBudget = 8;
constexpr std::int32_t kHeadBoxPixels = 32;

// L'art du groupe, sous le dossier d'interface.
constexpr std::string_view kJobIconPrefix = "renewalparty\\icon_jobs_";
constexpr std::string_view kCrownPath = "renewalparty\\ico_partyCrown.bmp";
constexpr std::string_view kMinimapFolder = "map\\";
constexpr std::string_view kBitmapExtension = ".bmp";

// Les teintes des avis, en 0xRRGGBB : un succès, un refus, et ce qui concerne
// le groupe entier.
constexpr std::uint32_t kSuccessColor = 0x00FFFF;
constexpr std::uint32_t kRefusalColor = 0xFF0000;
constexpr std::uint32_t kPartyColor = 0xFFFF00;

[[nodiscard]] std::uint32_t ColorOf(ro::net::SocialTone tone) {
  switch (tone) {
    case ro::net::SocialTone::kSuccess:
      return kSuccessColor;
    case ro::net::SocialTone::kRefusal:
      return kRefusalColor;
    case ro::net::SocialTone::kParty:
      return kPartyColor;
  }
  return kSuccessColor;
}

void Say(ro::sim::World& world, const ro::net::SocialLine& line, std::uint32_t now_ms) {
  // Ce qui concerne le groupe entier — son chef, son partage — se range sous
  // « configuration du groupe » ; le reste sous la case générale.
  if (line.tone == ro::net::SocialTone::kParty) {
    world.RecordChat(ro::sim::ChatKind::kParty, ro::sim::ChatType::kPartySetup, 0, line.text,
                     now_ms, ColorOf(line.tone));
    return;
  }
  world.RecordChat(ro::sim::ChatKind::kServer, 0, line.text, now_ms, ColorOf(line.tone));
}

// Une surface montre-t-elle le PS d'un autre membre à cette image ? La fenêtre
// ne le montre que sur son onglet de groupe, et pour un membre dont les PV sont
// connus.
[[nodiscard]] bool WindowShowsSp(const ro::ui::PartyFriendView& view,
                                 const ro::ui::PartyFriendLook& look,
                                 const ro::ui::PartyFriendWindowState& window) {
  if (!window.open || window.tab != ro::ui::PartyFriendTab::kParty || !look.show_sp) {
    return false;
  }
  for (const ro::ui::PartyMemberView& member : view.members) {
    if (!member.is_me && !member.offline && member.hp_known) {
      return true;
    }
  }
  return false;
}

// Une surface montre-t-elle les états des membres à cette image ? La fenêtre ne
// les montre que sur son onglet de groupe, sur les lignes des membres en ligne.
[[nodiscard]] bool WindowShowsStatuses(const ro::ui::PartyFriendView& view,
                                       const ro::ui::PartyFriendLook& look,
                                       const ro::ui::PartyFriendWindowState& window) {
  if (!window.open || window.tab != ro::ui::PartyFriendTab::kParty || !look.show_buffs) {
    return false;
  }
  return std::ranges::any_of(view.members,
                             [](const ro::ui::PartyMemberView& member) { return !member.offline; });
}

}  // namespace

PartyPanel::PartyPanel(PartyPanelPieces pieces) : pieces_(pieces) {
  if (pieces_.wardrobe != nullptr) {
    thumbnails_.emplace(*pieces_.wardrobe);
  }
}

void PartyPanel::ForgetStatuses(const ro::sim::PartyRoster& party, std::uint32_t own_account_id) {
  std::erase_if(statuses_, [&party, own_account_id](const auto& entry) {
    return entry.first != own_account_id && party.Find(entry.first) == nullptr;
  });
}

void PartyPanel::PurgeMemberStatuses(const ro::sim::PartyRoster& party,
                                     const ro::sim::World& world) {
  if (seen_map_entries_ != world.map_entries()) {
    seen_map_entries_ = world.map_entries();
    member_statuses_.Clear();
    status_poll_.Reset();
  }
  const std::uint32_t own = world.player_id();
  member_statuses_.Purge(facts_.now_ms, [this, &party, own](std::uint32_t account_id) {
    const ro::sim::PartyMember* member = party.Find(account_id);
    return account_id != own && member != nullptr && !member->offline && ActorPresent(account_id);
  });
}

void PartyPanel::Refresh(const ro::sim::PartyRoster& party, const ro::sim::FriendList& friends,
                         const ro::sim::World& world, const PartyOptions& options,
                         const ro::ui::PartyFriendWindowState& window,
                         const PartyFrameFacts& facts) {
  world_ = &world;
  facts_ = facts;
  const std::uint32_t own = world.player_id();
  const ro::sim::PartyMember* own_row = party.Find(own);
  own_map_raw_ =
      own_row != nullptr && !own_row->map.empty() ? own_row->map : MapSignFileOf(world.map_name());
  const ro::ui::PartyFriendLook& look = options.window();
  const ro::ui::PartyGridLook& grid = options.grid();
  const int window_preview = options.window_preview() && look.show_buffs ? look.buff_max : 0;
  const int grid_preview = options.grid_preview() && grid.show_statuses ? grid.status_most : 0;
  preview_ = std::max(window_preview, grid_preview);
  ForgetStatuses(party, own);
  PurgeMemberStatuses(party, world);
  if (thumbnails_.has_value() && look.head_mode != 0 && window.open) {
    thumbnails_->BeginFrame(kHeadBudget);
  }

  PartyComposeInput input;
  input.party = &party;
  input.friends = &friends;
  input.self.account_id = own;
  input.self.char_id = facts.char_id;
  input.self.map_raw = own_map_raw_;
  const ro::sim::PlayerStatus& status = world.player_status();
  if (const ro::sim::Actor* self = world.player(); self != nullptr) {
    input.self.name = self->name;
    input.self.job = self->look.job;
    input.self.hp_known = self->max_health > 0;
    input.self.hp = self->health;
    input.self.max_hp = self->max_health;
    input.self.position_known = true;
    input.self.x = self->position.x;
    input.self.y = self->position.y;
  }
  if (status.base_level > 0) {
    input.self.base_level = static_cast<int>(status.base_level);
  }
  input.self.sp_known = status.max_sp > 0;
  if (input.self.sp_known) {
    input.self.sp = static_cast<int>(status.sp);
    input.self.max_sp = static_cast<int>(status.max_sp);
  }
  const ro::sim::Actor* selected = world.selected();
  input.target_id =
      selected != nullptr && selected->kind == ro::sim::ActorKind::kPlayer ? selected->id : 0;
  input.now_ms = facts.now_ms;
  input.grid_shown = grid.shown;
  input.fake_members = grid.shown && options.grid_fake_members();
  // ⚠ LES CASES SE COMPOSENT UNE FOIS, avec l'aperçu le plus grand des deux, et
  // chaque surface n'en garde les faux que si c'est SON aperçu qui est allumé.
  input.window_preview = window_preview > 0;
  input.grid_preview = grid_preview > 0;
  // Les têtes ne se composent que fenêtre ouverte : c'est là seulement qu'elles
  // se montrent.
  input.head_mode = window.open ? look.head_mode : 0;
  input.mvp_invites_withdrawn = mvp_withdrawn_;
  views_.Compose(input, *this);
}

void PartyPanel::Poll(ro::net::Connection& link, const ro::sim::PartyRoster& party,
                      const PartyOptions& options, const ro::ui::PartyFriendWindowState& window,
                      std::uint32_t own_account_id, std::uint32_t now_ms) {
  const bool grid_wants = options.grid().shown && options.grid().show_sp;
  const bool wanted = grid_wants || WindowShowsSp(views_.window(), options.window(), window);
  const std::uint32_t asked = poll_.Next(party.members(), own_account_id, wanted, now_ms);
  if (asked != 0) {
    ReportRefusal(ro::net::RequestTargetInfo(link, asked));
  }

  const bool grid_wants_statuses = options.grid().shown && options.grid().show_statuses;
  const bool statuses_wanted =
      grid_wants_statuses || WindowShowsStatuses(views_.window(), options.window(), window);
  const std::uint32_t described = status_poll_.Next(
      party.members(), own_account_id, statuses_wanted, member_statuses_,
      [this](std::uint32_t account_id) { return ActorPresent(account_id); }, now_ms);
  if (described != 0) {
    ReportRefusal(ro::net::RequestTargetStatuses(link, described));
  }
}

bool PartyPanel::ActorPresent(std::uint32_t account_id) const {
  const ro::sim::Actor* actor = world_ != nullptr ? world_->Find(account_id) : nullptr;
  return actor != nullptr && actor->kind == ro::sim::ActorKind::kPlayer;
}

ro::ui::SkinImage PartyPanel::ImageAt(std::string_view path) {
  if (pieces_.images == nullptr) {
    return {};
  }
  std::string full(ro::render::kSkinRoot);
  full += path;
  // ⚠ `GetOurs` : le chemin est écrit dans NOS sources, en UTF-8.
  const ro::render::IconTexture& icon = pieces_.images->GetOurs(full);
  if (!icon.usable()) {
    return {};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                           .width = static_cast<float>(icon.width_px),
                           .height = static_cast<float>(icon.height_px)};
}

ro::ui::SkinImage PartyPanel::JobIcon(std::uint16_t job) {
  std::string path(kJobIconPrefix);
  path += std::to_string(job);
  path += kBitmapExtension;
  return ImageAt(path);
}

std::string_view PartyPanel::JobName(std::uint16_t job) const {
  return pieces_.jobs != nullptr ? pieces_.jobs->Find(job) : std::string_view();
}

std::string_view PartyPanel::MapName(std::string_view map_raw) const {
  if (pieces_.map_names == nullptr || map_raw.empty()) {
    return {};
  }
  return pieces_.map_names->DisplayNameOf(map_raw).value_or(std::string_view());
}

PartyMinimap PartyPanel::Minimap(std::string_view map_raw) {
  // La mini-carte n'est connue que de MA carte : c'est la seule dont on tienne
  // les dimensions, et le serveur ne dit la position que sur elle.
  if (world_ == nullptr || map_raw != own_map_raw_ || world_->map_name().empty() ||
      facts_.map_width_cells <= 0 || facts_.map_height_cells <= 0) {
    return {};
  }
  std::string path(kMinimapFolder);
  path += world_->map_name();
  path += kBitmapExtension;
  return PartyMinimap{.image = ImageAt(path),
                      .width_cells = facts_.map_width_cells,
                      .height_cells = facts_.map_height_cells};
}

ro::ui::SkinImage PartyPanel::Head(std::uint32_t account_id) {
  const ro::sim::Actor* actor = world_ != nullptr ? world_->Find(account_id) : nullptr;
  if (actor == nullptr || !thumbnails_.has_value()) {
    return {};
  }
  ro::render::ActorLook look = LookOf(*actor);
  // 🔴 LA COIFFURE ET LA COULEUR QUE SA RECETTE IMPOSE, comme sur la carte : la
  // tête montrait sinon celles du serveur, que la recette remplace.
  if (pieces_.wardrobe != nullptr) {
    ImposeStyleNumbers(account_id, look, *pieces_.wardrobe, world_->styles());
  }
  ro::render::ActorThumbnails::Cell cell;
  cell.subject = ro::render::ActorThumbnails::Subject::kHead;
  cell.look.player = true;
  cell.look.job = look.job;
  cell.look.sex = look.sex;
  cell.look.hair = look.hair;
  cell.look.hair_palette = look.hair_palette;
  cell.box.width = kHeadBoxPixels;
  cell.box.height = kHeadBoxPixels;
  const ro::render::IconTexture& head = thumbnails_->Get(cell);
  if (!head.usable()) {
    return {};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(head.texture.name()),
                           .width = static_cast<float>(head.width_px),
                           .height = static_cast<float>(head.height_px)};
}

ro::ui::SkinImage PartyPanel::Crown() { return ImageAt(kCrownPath); }

const char* PartyPanel::MvpInviteRefusal(std::string_view name, bool same_account) const {
  return mvp_judge_ ? mvp_judge_(name, same_account) : nullptr;
}

PartyStatuses PartyPanel::Statuses(std::uint32_t account_id, bool is_me) {
  if (world_ == nullptr) {
    return {};
  }
  StatusView& view =
      statuses_.try_emplace(account_id, pieces_.status_table, pieces_.status_icons).first->second;
  if (is_me) {
    view.Refresh(*world_, facts_.now_ms, preview_);
    return PartyStatuses{
        .cells = view.cells(), .known = true, .genuine = view.genuine(), .genuinely_known = true};
  }
  // L'aperçu passe outre « on n'en sait rien » : c'est hors de portée qu'on
  // règle ses surfaces.
  const std::vector<ro::sim::ActiveStatus>* known = member_statuses_.Find(account_id);
  view.RefreshList(known != nullptr ? std::span<const ro::sim::ActiveStatus>(*known)
                                    : std::span<const ro::sim::ActiveStatus>(),
                   facts_.now_ms, preview_);
  return PartyStatuses{.cells = view.cells(),
                       .known = known != nullptr || preview_ > 0,
                       .genuine = view.genuine(),
                       .genuinely_known = known != nullptr};
}

void SayPartyNotices(ro::sim::PartyRoster& party, ro::sim::FriendList& friends,
                     ro::sim::World& world, std::uint32_t now_ms) {
  for (const ro::sim::PartyNotice& notice : party.TakeNotices()) {
    if (notice.kind == ro::sim::PartyNoticeKind::kOptionsChanged) {
      continue;
    }
    Say(world, ro::net::PartyNoticeLine(notice), now_ms);
  }
  for (const ro::sim::FriendNotice& notice : friends.TakeNotices()) {
    Say(world, ro::net::FriendNoticeLine(notice), now_ms);
  }
}

bool LeaveParty(ro::net::Connection& link, ro::sim::PartyRoster& party,
                std::uint32_t own_account_id) {
  const ro::sim::LeavePlan plan = ro::sim::PlanLeave(party, own_account_id);
  switch (plan.step) {
    case ro::sim::LeaveStep::kLeave:
      ReportRefusal(ro::net::RequestPartyLeave(link));
      return false;
    case ro::sim::LeaveStep::kHandOverThenLeave:
      // La direction passe AVANT le départ : le groupe ne reste pas sans tête.
      ReportRefusal(ro::net::RequestPartyLeader(link, plan.successor_account_id));
      ReportRefusal(ro::net::RequestPartyLeave(link));
      return false;
    case ro::sim::LeaveStep::kNeedsHandOver:
      party.Notify(ro::sim::PartyNotice{.kind = ro::sim::PartyNoticeKind::kLeaveNeedsHandOver});
      return false;
    case ro::sim::LeaveStep::kConfirmDisband:
      return true;
  }
  return false;
}

}  // namespace moonclient
