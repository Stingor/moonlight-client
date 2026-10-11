// Les lieux de la recherche de groupe. Voir l'en-tête.

#include "party/party_booking_places.h"

#include <cstddef>
#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace moonclient {
namespace {

constexpr const char* kVersionKey = "version";
constexpr const char* kRegionsKey = "regions";
constexpr const char* kNameKey = "name";
constexpr const char* kOfferedKey = "offered";
constexpr const char* kPlacesKey = "places";
// Entre le nom d'une région et celui d'un lieu.
constexpr std::string_view kPlaceJoint = " — ";

[[nodiscard]] std::unexpected<ro::core::Error> Broken(const std::string& why) {
  return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                        std::string(kPartyBookingPlacesPath) + " : " + why);
}

[[nodiscard]] std::string ScalarOf(const YAML::Node& value) {
  return value && value.IsScalar() ? value.as<std::string>() : std::string();
}

[[nodiscard]] PartyBookingRegion RegionOf(const YAML::Node& entry, std::size_t rank,
                                          std::vector<std::string>& problems) {
  PartyBookingRegion region;
  const std::string where = "région n° " + std::to_string(rank + 1);
  if (!entry.IsMap()) {
    problems.push_back(where + " : une table est attendue, laissée vide");
    return region;
  }
  region.name = ScalarOf(entry[kNameKey]);
  if (const YAML::Node offered = entry[kOfferedKey]; offered && offered.IsScalar()) {
    region.offered = offered.as<bool>(true);
  }
  const YAML::Node places = entry[kPlacesKey];
  if (!places || !places.IsSequence()) {
    problems.push_back(where + " : aucune liste « places »");
    return region;
  }
  for (std::size_t place = 0; place < places.size(); ++place) {
    std::string name = ScalarOf(places[place]);
    if (name.empty()) {
      problems.push_back(where + ", lieu n° " + std::to_string(place + 1) + " : sans nom");
    }
    region.places.push_back(std::move(name));
  }
  return region;
}

}  // namespace

ro::core::Result<PartyBookingPlaces> PartyBookingPlaces::Parse(std::string_view text) {
  YAML::Node root;
  try {
    root = YAML::Load(std::string(text));
  } catch (const YAML::Exception& broken) {
    return Broken(broken.what());
  }
  if (!root.IsMap()) {
    return Broken("une table est attendue à la racine");
  }
  const YAML::Node version = root[kVersionKey];
  const int read_version = version && version.IsScalar() ? version.as<int>(0) : 0;
  if (read_version != kPartyBookingPlacesVersion) {
    return Broken("version " + std::to_string(read_version) +
                  " inconnue (attendue : " + std::to_string(kPartyBookingPlacesVersion) + ")");
  }
  const YAML::Node list = root[kRegionsKey];
  if (!list || !list.IsSequence()) {
    return Broken("aucune liste « regions »");
  }
  PartyBookingPlaces file;
  for (std::size_t rank = 0; rank < list.size(); ++rank) {
    file.regions_.push_back(RegionOf(list[rank], rank, file.problems_));
  }
  return file;
}

std::string PartyBookingPlaces::NameOf(std::size_t region, std::size_t place) const {
  if (region >= regions_.size() || place >= regions_[region].places.size()) {
    return {};
  }
  const PartyBookingRegion& known = regions_[region];
  return known.name + std::string(kPlaceJoint) + known.places[place];
}

}  // namespace moonclient
