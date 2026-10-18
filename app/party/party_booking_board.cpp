// La recherche de groupe. Voir l'en-tête.

#include "party/party_booking_board.h"

#include <cstddef>
#include <ctime>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/local_time.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/net/party_booking.h"
#include "ro/vfs/vfs.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::sim::PartyBookingJobFamily;

static_assert(ro::ui::kPartyBookingSlotCount == ro::sim::kPartyBookingJobSlots);
static_assert(ro::ui::kPartyBookingEmptySlot == ro::sim::kPartyBookingNoJob);
static_assert(ro::ui::kPartyBookingEverywhere == ro::sim::kPartyBookingAnyMap);

// Entre deux métiers d'une annonce.
constexpr std::string_view kJobJoint = ", ";

// LE NOM D'UN RÔLE, déjà traduit ; nul pour un métier.
const char* RoleName(std::int16_t code) {
  switch (code) {
    case ro::sim::kPartyBookingRoleAttack:
      return Tr("Attaque");
    case ro::sim::kPartyBookingRoleDefense:
      return Tr("Défense");
    case ro::sim::kPartyBookingRoleHealer:
      return Tr("Soins");
    case ro::sim::kPartyBookingRoleSupport:
      return Tr("Soutien");
    default:
      return nullptr;
  }
}

// Le nom d'un code de case : un rôle traduit, ou un métier.
std::string CodeName(std::int16_t code) {
  if (const char* role = RoleName(code); role != nullptr) {
    return role;
  }
  return std::string(ro::sim::PartyBookingJobName(code));
}

// Le libellé d'une famille, déjà traduit.
const char* FamilyLabel(PartyBookingJobFamily family) {
  switch (family) {
    case PartyBookingJobFamily::kRoles:
      return Tr("Rôles");
    case PartyBookingJobFamily::kFirst:
      return Tr("Premiers jobs");
    case PartyBookingJobFamily::kSecond:
      return Tr("Seconds jobs");
    case PartyBookingJobFamily::kThirdOne:
      return Tr("Troisièmes jobs (1)");
    case PartyBookingJobFamily::kThirdTwo:
      return Tr("Troisièmes jobs (2)");
    case PartyBookingJobFamily::kFirstHigh:
      return Tr("Premiers jobs transcendants");
    case PartyBookingJobFamily::kSecondHigh:
      return Tr("Seconds jobs transcendants");
    case PartyBookingJobFamily::kOther:
      return Tr("Autres jobs");
  }
  return "";
}

std::string LevelsText(std::int16_t level) {
  return ro::core::Printed("%d~%d", static_cast<int>(level),
                           static_cast<int>(level) + ro::sim::kPartyBookingLevelSpan);
}

}  // namespace

void TogglePartyBooking(PartyBookingWindows& windows) {
  if (windows.window.open) {
    windows.window.open = false;
    return;
  }
  windows.open_asked = true;
}

std::string PartyBookingNoticeText(ro::sim::PartyBookingNotice notice) {
  switch (notice) {
    case ro::sim::PartyBookingNotice::kAdPosted:
      return Tr("Une annonce de groupe vient d'être publiée.");
    case ro::sim::PartyBookingNotice::kRegisterRefused:
      return Tr("La publication de l'annonce de groupe a échoué.");
    case ro::sim::PartyBookingNotice::kAlreadyRegistered:
      return Tr("Vous avez déjà une annonce de groupe.");
    case ro::sim::PartyBookingNotice::kDeleteRefused:
      return Tr("Le retrait de l'annonce de groupe a échoué.");
    case ro::sim::PartyBookingNotice::kNothingToDelete:
      return Tr("Vous n'avez aucune annonce de groupe à retirer.");
    case ro::sim::PartyBookingNotice::kNoResult:
      return Tr("Aucune annonce de groupe ne correspond à cette recherche.");
  }
  return {};
}

std::string PartyBookingClockText(std::uint32_t unix_s) {
  const auto seconds = static_cast<std::time_t>(unix_s);
  std::tm local{};
  if (!ro::core::ToLocalTime(seconds, local)) {
    return {};
  }
  return ro::core::Printed("%02d:%02d", local.tm_hour, local.tm_min);
}

void PartyBookingBoard::StartSession() {
  booking_.Reset();
  list_shown_ = false;
  whisper_.reset();
}

void PartyBookingBoard::SeePlaces(ro::vfs::Vfs* vfs) {
  if (places_tried_ || vfs == nullptr) {
    return;
  }
  places_tried_ = true;
  const ro::core::Result<std::vector<std::uint8_t>> bytes = vfs->Read(kPartyBookingPlacesPath);
  if (!bytes) {
    ro::core::LogWarning("recherche de groupe : {} illisible ({})", kPartyBookingPlacesPath,
                         ro::core::Describe(bytes.error()));
    return;
  }
  ro::core::Result<PartyBookingPlaces> read = PartyBookingPlaces::Parse(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
  if (!read) {
    ro::core::LogWarning("recherche de groupe : {}", ro::core::Describe(read.error()));
    return;
  }
  for (const std::string& problem : read->problems()) {
    ro::core::LogWarning("recherche de groupe : {} : {}", kPartyBookingPlacesPath, problem);
  }
  SetPlaces(std::move(*read));
}

void PartyBookingBoard::SetPlaces(PartyBookingPlaces places) {
  places_ = std::move(places);
  places_tried_ = true;
  tables_composed_ = false;
}

ro::net::FrameOutcome PartyBookingBoard::ApplyFrame(const ro::net::Frame& frame,
                                                    ro::sim::World& world, std::uint32_t now_ms) {
  booking_.SetOwnName(world.player_name());
  const ro::net::FrameOutcome outcome =
      ro::net::ApplyPartyBookingFrame(frame, booking_, list_shown_);
  WriteNotices(world, now_ms);
  return outcome;
}

void PartyBookingBoard::WriteNotices(ro::sim::World& world, std::uint32_t now_ms) {
  for (const ro::sim::PartyBookingNotice notice : booking_.TakeNotices()) {
    world.RecordChat(ro::sim::ChatKind::kServer, ro::sim::ChatType::kPartySearch, 0,
                     PartyBookingNoticeText(notice), now_ms, kPartyBookingChatColor);
  }
}

void PartyBookingBoard::Advance(PartyBookingWindows& windows, ro::net::Connection& link,
                                ro::sim::World& world, std::uint32_t now_ms) {
  booking_.SetOwnName(world.player_name());
  Obey(std::exchange(windows.answer, {}), link, world);
  WriteNotices(world, now_ms);
  if (std::exchange(windows.open_asked, false)) {
    windows.window.open = true;
  }
  list_shown_ = windows.window.open;
  windows.view = &view_;
  // Fermée, la fenêtre ne lit pas sa vue : rien à recomposer.
  if (!windows.window.open) {
    return;
  }
  ComposeTables();
  Compose();
}

std::optional<PartyBookingWhisper> PartyBookingBoard::TakeWhisper() {
  return std::exchange(whisper_, std::nullopt);
}

void PartyBookingBoard::Obey(const ro::ui::PartyBookingAction& action, ro::net::Connection& link,
                             const ro::sim::World& world) {
  if (action.verb == ro::ui::PartyBookingVerb::kNothing) {
    return;
  }
  if (action.verb == ro::ui::PartyBookingVerb::kWhisper ||
      action.verb == ro::ui::PartyBookingVerb::kWhisperWindow) {
    whisper_ = PartyBookingWhisper{
        .name = action.name, .own_window = action.verb == ro::ui::PartyBookingVerb::kWhisperWindow};
    return;
  }
  // Hors ligne, rien ne part : la fenêtre garde ce qu'elle montrait.
  if (!link.is_open()) {
    return;
  }
  ro::core::Result<void> sent;
  switch (action.verb) {
    case ro::ui::PartyBookingVerb::kSearch:
      sent = ro::net::RequestPartyBookingSearch(
          link, booking_,
          ro::sim::PartyBookingCriteria{
              .level = action.level, .map_code = action.map_code, .job = action.job});
      break;
    case ro::ui::PartyBookingVerb::kNextPage:
      sent = ro::net::RequestPartyBookingNextPage(link, booking_);
      break;
    case ro::ui::PartyBookingVerb::kRegister: {
      ro::sim::PartyBookingAd draft;
      draft.leader_name = world.player_name();
      draft.level = action.level;
      draft.map_code = action.map_code;
      draft.jobs = ro::sim::PackPartyBookingJobs(action.jobs);
      sent = ro::net::RequestPartyBookingRegister(link, booking_, draft);
      break;
    }
    case ro::ui::PartyBookingVerb::kUpdate:
      sent = ro::net::RequestPartyBookingUpdate(link, booking_,
                                                ro::sim::PackPartyBookingJobs(action.jobs));
      break;
    case ro::ui::PartyBookingVerb::kDelete:
      sent = ro::net::RequestPartyBookingDelete(link, booking_);
      break;
    case ro::ui::PartyBookingVerb::kNothing:
    case ro::ui::PartyBookingVerb::kWhisper:
    case ro::ui::PartyBookingVerb::kWhisperWindow:
      break;
  }
  if (!sent) {
    ro::core::LogWarning("recherche de groupe : {}", ro::core::Describe(sent.error()));
  }
}

void PartyBookingBoard::ComposeTables() {
  if (tables_composed_) {
    return;
  }
  tables_composed_ = true;
  // LES LIEUX : seules les régions proposées entrent dans la vue, mais chaque
  // lieu garde le code de son rang dans la table entière.
  place_views_.clear();
  region_views_.clear();
  if (places_.has_value()) {
    const std::span<const PartyBookingRegion> regions = places_->regions();
    for (std::size_t region = 0; region < regions.size(); ++region) {
      if (!regions[region].offered || regions[region].places.empty()) {
        continue;
      }
      std::vector<ro::ui::PartyBookingPlaceView>& places = place_views_.emplace_back();
      for (std::size_t place = 0; place < regions[region].places.size(); ++place) {
        places.push_back(
            ro::ui::PartyBookingPlaceView{.name = regions[region].places[place],
                                          .map_code = ro::sim::PartyBookingMapCode(region, place)});
      }
    }
    std::size_t rank = 0;
    for (std::size_t region = 0; region < regions.size(); ++region) {
      if (!regions[region].offered || regions[region].places.empty()) {
        continue;
      }
      region_views_.push_back(ro::ui::PartyBookingRegionView{.name = regions[region].name,
                                                             .places = place_views_[rank]});
      ++rank;
    }
  }
  // LES MÉTIERS, par famille. Les libellés se traduisent ici, une fois.
  for (std::size_t family = 0; family < ro::sim::kPartyBookingJobFamilyCount; ++family) {
    const auto which = static_cast<PartyBookingJobFamily>(family);
    job_group_labels_[family] = FamilyLabel(which);
    std::vector<ro::ui::PartyBookingJobView>& jobs = job_views_[family];
    jobs.clear();
    for (const std::int16_t code : ro::sim::PartyBookingJobsOf(which)) {
      const char* role = RoleName(code);
      jobs.push_back(ro::ui::PartyBookingJobView{
          .code = code,
          .name = role != nullptr ? std::string_view(role) : ro::sim::PartyBookingJobName(code)});
    }
    job_groups_[family] =
        ro::ui::PartyBookingJobGroupView{.label = job_group_labels_[family], .jobs = jobs};
  }
}

std::string PartyBookingBoard::PlaceText(std::int16_t map_code) const {
  if (map_code == ro::sim::kPartyBookingAnyMap) {
    return Tr("Partout");
  }
  const std::size_t region = ro::sim::PartyBookingRegionOf(map_code);
  const std::size_t place = ro::sim::PartyBookingPlaceOf(map_code);
  if (places_.has_value()) {
    if (std::string name = places_->NameOf(region, place); !name.empty()) {
      return name;
    }
  }
  return ro::core::Printed(Tr("Lieu %d-%d"), static_cast<int>(region), static_cast<int>(place));
}

std::string PartyBookingBoard::JobsText(const ro::sim::PartyBookingJobs& jobs) const {
  std::string text;
  for (const std::int16_t code : jobs) {
    std::string name = CodeName(code);
    if (name.empty()) {
      continue;
    }
    if (!text.empty()) {
      text += kJobJoint;
    }
    text += name;
  }
  return text;
}

void PartyBookingBoard::Compose() {
  const std::span<const ro::sim::PartyBookingAd> results = booking_.results();
  const std::optional<ro::sim::PartyBookingAd>& mine = booking_.mine();
  // ⚠ LES TEXTES D'ABORD, LES VUES ENSUITE : un vecteur qui grandit déplace ses
  // chaînes, et une vue prise avant pointerait dans le vide.
  row_texts_.resize(results.size() + 1);
  const auto fill = [this](RowTexts& texts, const ro::sim::PartyBookingAd& ad) {
    texts.leader = ad.leader_name;
    texts.levels = LevelsText(ad.level);
    texts.place = PlaceText(ad.map_code);
    texts.jobs = JobsText(ad.jobs);
    texts.posted = ad.posted_unix != 0 ? PartyBookingClockText(ad.posted_unix) : std::string();
  };
  for (std::size_t rank = 0; rank < results.size(); ++rank) {
    fill(row_texts_[rank], results[rank]);
  }
  RowTexts& own = row_texts_.back();
  own = RowTexts{};
  if (mine.has_value()) {
    fill(own, *mine);
  }
  const auto view_of = [](const RowTexts& texts) {
    return ro::ui::PartyBookingRowView{.leader = texts.leader,
                                       .levels = texts.levels,
                                       .place = texts.place,
                                       .jobs = texts.jobs,
                                       .posted = texts.posted};
  };
  rows_.clear();
  for (std::size_t rank = 0; rank < results.size(); ++rank) {
    rows_.push_back(view_of(row_texts_[rank]));
  }
  view_ = ro::ui::PartyBookingView{};
  view_.regions = region_views_;
  view_.job_groups = job_groups_;
  view_.rows = rows_;
  view_.searching = booking_.searching();
  view_.searched = booking_.criteria().has_value() && !booking_.searching();
  view_.more_on_server = booking_.more_on_server();
  view_.has_mine = mine.has_value();
  view_.mine = view_of(own);
  if (mine.has_value()) {
    view_.mine_index = mine->index;
    view_.mine_jobs = mine->jobs;
  }
  view_.registering = booking_.registering();
  view_.least_level = ro::sim::kPartyBookingLeastLevel;
  view_.most_level = ro::sim::kPartyBookingMostLevel;
}

}  // namespace moonclient
