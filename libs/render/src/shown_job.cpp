// Le métier qu'un acteur montre, d'après ses options et deux de ses états.

#include "ro/render/shown_job.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

namespace ro::render {
namespace {

// Un métier et le corps qu'il prend quand la règle s'applique.
struct Mount {
  std::uint32_t job;
  std::uint32_t shown;
};

// Chevalier, croisé, leurs transcendés et leurs bébés, la Royal Guard, son
// transcendé, son bébé et l'Imperial Guard.
constexpr std::array<Mount, 10> kPecoMounts = {{{7, 13},
                                                {14, 21},
                                                {4008, 4014},
                                                {4015, 4022},
                                                {4030, 4036},
                                                {4037, 4044},
                                                {4066, 4082},
                                                {4073, 4083},
                                                {4102, 4110},
                                                {4258, 4281}}};

// Rune Knight, son transcendé, son bébé et le Dragon Knight : le dragon vert.
constexpr std::array<Mount, 4> kGreenDragonMounts = {
    {{4054, 4080}, {4060, 4081}, {4096, 4109}, {4252, 4280}}};
// Les quatre autres couleurs n'ont de corps que pour le Rune Knight et son
// transcendé.
constexpr std::array<Mount, 2> kBlackDragonMounts = {{{4054, 4088}, {4060, 4089}}};
constexpr std::array<Mount, 2> kWhiteDragonMounts = {{{4054, 4090}, {4060, 4091}}};
constexpr std::array<Mount, 2> kBlueDragonMounts = {{{4054, 4092}, {4060, 4093}}};
constexpr std::array<Mount, 2> kRedDragonMounts = {{{4054, 4094}, {4060, 4095}}};

// Ranger, son transcendé, son bébé et le Wind Hawk.
constexpr std::array<Mount, 4> kWolfMounts = {
    {{4056, 4084}, {4062, 4085}, {4098, 4111}, {4257, 4278}}};

// Mechanic, son transcendé, son bébé et le Meister.
constexpr std::array<Mount, 4> kMadogearMounts = {
    {{4058, 4086}, {4064, 4087}, {4100, 4112}, {4253, 4279}}};

// Star Gladiator, son bébé, Star Emperor, son bébé et Sky Emperor.
constexpr std::array<Mount, 5> kUnionFlights = {
    {{4047, 4048}, {4226, 4238}, {4239, 4243}, {4241, 4244}, {4302, 4316}}};

std::optional<std::uint32_t> MountedJob(std::span<const Mount> mounts, std::uint32_t job_id) {
  const auto found = std::ranges::find(mounts, job_id, &Mount::job);
  return found == mounts.end() ? std::nullopt : std::optional{found->shown};
}

bool Has(std::uint32_t option, std::uint32_t bit) { return (option & bit) != 0; }

// La couleur du dragon : le PREMIER bit levé la choisit, même quand ce métier
// n'a pas de corps pour elle.
std::span<const Mount> DragonMounts(std::uint32_t option) {
  if (Has(option, kOptionGreenDragon)) {
    return kGreenDragonMounts;
  }
  if (Has(option, kOptionBlackDragon)) {
    return kBlackDragonMounts;
  }
  if (Has(option, kOptionWhiteDragon)) {
    return kWhiteDragonMounts;
  }
  if (Has(option, kOptionBlueDragon)) {
    return kBlueDragonMounts;
  }
  if (Has(option, kOptionRedDragon)) {
    return kRedDragonMounts;
  }
  return {};
}

}  // namespace

std::uint32_t ShownJobFor(std::uint32_t job_id, std::uint32_t option,
                          ShownBodyStates states) noexcept {
  if (Has(option, kOptionWedding)) {
    return kWeddingJob;
  }
  if (!Has(option, kOptionFlying)) {
    if (Has(option, kOptionXmas)) {
      return kXmasJob;
    }
    if (Has(option, kOptionSummer)) {
      return kSummerJob;
    }
    if (Has(option, kOptionHanbok)) {
      return kHanbokJob;
    }
    if (Has(option, kOptionOktoberfest)) {
      return kOktoberfestJob;
    }
    if (states.dressed_up) {
      return kDressUpJob;
    }
  }
  const bool madogear = states.madogear || Has(option, kOptionMadogear);
  const std::array<std::pair<bool, std::span<const Mount>>, 5> rules = {{
      {Has(option, kOptionPeco), kPecoMounts},
      {true, DragonMounts(option)},
      {Has(option, kOptionWolfRider), kWolfMounts},
      {madogear, kMadogearMounts},
      {Has(option, kOptionFlying), kUnionFlights},
  }};
  for (const auto& [applies, mounts] : rules) {
    if (!applies) {
      continue;
    }
    if (const std::optional<std::uint32_t> shown = MountedJob(mounts, job_id); shown.has_value()) {
      return *shown;
    }
  }
  return job_id;
}

bool IsCostumeJob(std::uint32_t shown_job) noexcept {
  return shown_job == kWeddingJob || (shown_job >= kXmasJob && shown_job <= kDressUpJob);
}

bool IsMadogearJob(std::uint32_t shown_job) noexcept {
  return std::ranges::find(kMadogearMounts, shown_job, &Mount::shown) != kMadogearMounts.end();
}

}  // namespace ro::render
