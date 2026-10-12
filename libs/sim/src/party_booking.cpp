// La recherche de groupe. Voir l'en-tête.

#include "ro/sim/party_booking.h"

#include <algorithm>
#include <utility>

namespace ro::sim {
namespace {

// LES MÉTIERS, du code `kPartyBookingFirstJobCode` au dernier. CATALOGUE : on
// n'y cherche qu'une entrée à la fois.
constexpr std::array<std::string_view, kPartyBookingLastJobCode - kPartyBookingFirstJobCode + 1>
    kJobNames = {{
        "Swordman",     "Mage",          "Archer",         "Acolyte",        "Merchant",
        "Thief",        "Knight",        "Priest",         "Wizard",         "Blacksmith",
        "Hunter",       "Assassin",      "Crusader",       "Monk",           "Sage",
        "Rogue",        "Alchemist",     "Bard",           "Dancer",         "Rune Knight",
        "Warlock",      "Ranger",        "Arch Bishop",    "Mechanic",       "Guillotine Cross",
        "Royal Guard",  "Sorcerer",      "Minstrel",       "Wanderer",       "Sura",
        "Genetic",      "Shadow Chaser", "High Swordman",  "High Mage",      "High Archer",
        "High Acolyte", "High Merchant", "High Thief",     "Lord Knight",    "High Priest",
        "High Wizard",  "Whitesmith",    "Sniper",         "Assassin Cross", "Paladin",
        "Champion",     "Professor",     "Stalker",        "Creator",        "Clown",
        "Gypsy",        "Novice",        "High Novice",    "Super Novice",   "Gunslinger",
        "Ninja",        "Taekwon",       "Star Gladiator", "Soul Linker",
    }};

// LES FAMILLES : leurs codes se suivent, sauf les rôles.
constexpr std::array<std::int16_t, 4> kRoles = {kPartyBookingRoleAttack, kPartyBookingRoleDefense,
                                                kPartyBookingRoleHealer, kPartyBookingRoleSupport};

// Le premier code de chaque famille de métiers, de `kFirst` à `kOther` : une
// famille va de sa borne à la suivante, la dernière jusqu'au dernier code.
constexpr std::array<std::int16_t, kPartyBookingJobFamilyCount - 1> kFamilyStarts = {
    kPartyBookingFirstJobCode,  // Swordman
    11,                         // Knight
    24,                         // Rune Knight
    30,                         // Royal Guard
    37,                         // High Swordman
    43,                         // Lord Knight
    56,                         // Novice
};

constexpr std::array<std::int16_t, kPartyBookingLastJobCode - kPartyBookingFirstJobCode + 1>
MakeJobCodes() {
  std::array<std::int16_t, kPartyBookingLastJobCode - kPartyBookingFirstJobCode + 1> codes = {};
  for (std::size_t rank = 0; rank < codes.size(); ++rank) {
    codes[rank] = static_cast<std::int16_t>(kPartyBookingFirstJobCode + static_cast<int>(rank));
  }
  return codes;
}
constexpr auto kJobCodes = MakeJobCodes();

bool HasJob(const PartyBookingJobs& jobs) {
  return std::any_of(jobs.begin(), jobs.end(),
                     [](std::int16_t job) { return job != kPartyBookingNoJob; });
}

}  // namespace

std::string_view PartyBookingJobName(std::int16_t code) {
  if (code < kPartyBookingFirstJobCode || code > kPartyBookingLastJobCode) {
    return {};
  }
  return kJobNames[static_cast<std::size_t>(code - kPartyBookingFirstJobCode)];
}

bool IsPartyBookingRole(std::int16_t code) {
  return code >= kPartyBookingRoleAttack && code <= kPartyBookingRoleSupport;
}

std::span<const std::int16_t> PartyBookingJobsOf(PartyBookingJobFamily family) {
  if (family == PartyBookingJobFamily::kRoles) {
    return kRoles;
  }
  // Les rôles ne sont pas dans la table des bornes : elle commence à `kFirst`.
  const std::size_t rank = static_cast<std::size_t>(family) - 1;
  const int first = kFamilyStarts[rank];
  const int end =
      rank + 1 < kFamilyStarts.size() ? kFamilyStarts[rank + 1] : kPartyBookingLastJobCode + 1;
  return std::span<const std::int16_t>(kJobCodes).subspan(
      static_cast<std::size_t>(first - kPartyBookingFirstJobCode),
      static_cast<std::size_t>(end - first));
}

bool PartyBookingAdMatches(const PartyBookingAd& ad, const PartyBookingCriteria& criteria) {
  // ⚠ LE NIVEAU CHERCHÉ EST CELUI DU PERSONNAGE, et une annonce vaut jusqu'à
  // quinze niveaux AU-DESSUS du sien : elle répond donc si son niveau tombe
  // dans les quinze niveaux EN DESSOUS de celui qu'on cherche. C'est la
  // fenêtre de `party_booking_search` côté serveur.
  if (criteria.level > 0 &&
      (ad.level > criteria.level || int{ad.level} < criteria.level - kPartyBookingLevelSpan)) {
    return false;
  }
  if (criteria.map_code != kPartyBookingAnyMap && ad.map_code != criteria.map_code) {
    return false;
  }
  if (criteria.job > 0 &&
      std::find(ad.jobs.begin(), ad.jobs.end(), criteria.job) == ad.jobs.end()) {
    return false;
  }
  return true;
}

PartyBookingFault CheckPartyBookingRegister(std::int16_t level, const PartyBookingJobs& jobs) {
  if (level < kPartyBookingLeastLevel || level > kPartyBookingMostLevel) {
    return PartyBookingFault::kLevelOutOfRange;
  }
  return HasJob(jobs) ? PartyBookingFault::kNone : PartyBookingFault::kNoJob;
}

PartyBookingFault CheckPartyBookingUpdate(const PartyBookingJobs& jobs) {
  return HasJob(jobs) ? PartyBookingFault::kNone : PartyBookingFault::kNoJob;
}

PartyBookingFault CheckPartyBookingSearch(const PartyBookingCriteria& criteria) {
  if (criteria.level != 0 &&
      (criteria.level < kPartyBookingLeastLevel || criteria.level > kPartyBookingMostLevel)) {
    return PartyBookingFault::kLevelOutOfRange;
  }
  if (criteria.map_code != kPartyBookingAnyMap && criteria.job != kPartyBookingNoJob) {
    return PartyBookingFault::kMapAndJob;
  }
  return PartyBookingFault::kNone;
}

PartyBookingJobs PackPartyBookingJobs(std::span<const std::int16_t> codes) {
  PartyBookingJobs jobs = PartyBookingAd{}.jobs;
  std::size_t filled = 0;
  for (const std::int16_t code : codes) {
    if (code == kPartyBookingNoJob || filled == jobs.size()) {
      continue;
    }
    jobs[filled++] = code;
  }
  return jobs;
}

void PartyBooking::Reset() {
  std::string own_name = std::move(own_name_);
  *this = PartyBooking();
  own_name_ = std::move(own_name);
}

void PartyBooking::NoteSearchSent(const PartyBookingCriteria& criteria) {
  criteria_ = criteria;
  searching_ = true;
}

void PartyBooking::NoteRegisterSent(const PartyBookingAd& draft) { draft_ = draft; }

void PartyBooking::TellSearchResults(bool more_results, std::vector<PartyBookingAd> page) {
  searching_ = false;
  more_on_server_ = more_results;
  // LA PAGE SUIVANTE PART DU RANG QUI SUIT LA DERNIÈRE ANNONCE de celle-ci.
  if (!page.empty()) {
    next_index_ = page.back().index + 1;
  }
  if (page.empty()) {
    notices_.push_back(PartyBookingNotice::kNoResult);
  }
  results_ = std::move(page);
}

void PartyBooking::TellRegisterResult(std::uint16_t result) {
  std::optional<PartyBookingAd> draft = std::exchange(draft_, std::nullopt);
  if (result == kPartyBookingRegistered) {
    // Son rang arrive avec l'avis que le serveur pousse à tous, juste après.
    if (draft.has_value() && !mine_.has_value()) {
      mine_ = std::move(draft);
    }
    return;
  }
  notices_.push_back(result == kPartyBookingAlreadyRegistered
                         ? PartyBookingNotice::kAlreadyRegistered
                         : PartyBookingNotice::kRegisterRefused);
}

void PartyBooking::TellDeleteResult(std::uint16_t result) {
  switch (result) {
    case kPartyBookingDeleted:
    case kPartyBookingDeletedExpired:
      mine_.reset();
      break;
    case kPartyBookingDeleteRefused:
      notices_.push_back(PartyBookingNotice::kDeleteRefused);
      break;
    case kPartyBookingNothingRegistered:
      mine_.reset();
      notices_.push_back(PartyBookingNotice::kNothingToDelete);
      break;
    default:
      break;
  }
}

void PartyBooking::TellInserted(const PartyBookingAd& ad, bool list_shown) {
  if (!own_name_.empty() && ad.leader_name == own_name_) {
    mine_ = ad;
  }
  const bool matches = !criteria_.has_value() || PartyBookingAdMatches(ad, *criteria_);
  if (!matches) {
    return;
  }
  if (!list_shown) {
    notices_.push_back(PartyBookingNotice::kAdPosted);
    return;
  }
  if (PartyBookingAd* known = Find(ad.index); known != nullptr) {
    *known = ad;
    return;
  }
  results_.push_back(ad);
}

void PartyBooking::TellUpdated(std::uint32_t index, const PartyBookingJobs& jobs) {
  if (PartyBookingAd* known = Find(index); known != nullptr) {
    known->jobs = jobs;
  }
  if (mine_.has_value() && mine_->index == index) {
    mine_->jobs = jobs;
  }
}

void PartyBooking::TellDeleted(std::uint32_t index) {
  std::erase_if(results_, [index](const PartyBookingAd& ad) { return ad.index == index; });
  if (mine_.has_value() && mine_->index == index) {
    mine_.reset();
  }
}

std::vector<PartyBookingNotice> PartyBooking::TakeNotices() { return std::exchange(notices_, {}); }

PartyBookingAd* PartyBooking::Find(std::uint32_t index) {
  const auto found = std::find_if(results_.begin(), results_.end(),
                                  [index](const PartyBookingAd& ad) { return ad.index == index; });
  return found != results_.end() ? &*found : nullptr;
}

}  // namespace ro::sim
