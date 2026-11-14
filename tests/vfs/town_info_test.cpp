// LES COMMODITÉS DES VILLES : la table se lit, la casse de la carte ne compte
// pas, une table mal formée est refusée, et celle livrée avec le client se
// relit en entier.

#include "ro/vfs/town_info.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using ro::vfs::TownFacility;
using ro::vfs::TownInfo;

namespace {

constexpr std::string_view kTable = R"(
version: 1
towns:
  prontera:
    - {name: Kafra, x: 155, y: 194, facility: kafra}
    - {name: "Inn: the second", x: 204, y: 187, facility: inn}
    - {x: 30, y: 40, facility: stylist}
  Geffen:
    - {name: Smith, x: 0, y: 1023, facility: smith}
  empty: []
)";

[[nodiscard]] std::string WithEntry(std::string_view entry) {
  return std::string("version: 1\ntowns:\n  ile:\n    - ") + std::string(entry) + "\n";
}

}  // namespace

TEST_CASE("Commodités de ville : la table se lit", "[vfs][town_info]") {
  const auto info = TownInfo::Parse(kTable);
  REQUIRE(info.has_value());
  CHECK(info->map_count() == 3);

  const auto prontera = info->On("prontera");
  REQUIRE(prontera.size() == 3);
  CHECK(prontera[0].name == "Kafra");
  CHECK(prontera[0].cell_x == 155);
  CHECK(prontera[0].cell_y == 194);
  CHECK(prontera[0].facility == TownFacility::kKafra);
  CHECK(prontera[1].name == "Inn: the second");
  CHECK(prontera[1].facility == TownFacility::kInn);
  // Le nom est facultatif.
  CHECK(prontera[2].name.empty());
  CHECK(prontera[2].facility == TownFacility::kStylist);

  CHECK(info->On("morocc").empty());
  CHECK(info->On("empty").empty());
}

TEST_CASE("Commodités de ville : la casse du nom de carte ne compte pas", "[vfs][town_info]") {
  const auto info = TownInfo::Parse(kTable);
  REQUIRE(info.has_value());
  CHECK(info->On("PRONTERA").size() == 3);
  REQUIRE(info->On("geffen").size() == 1);
  CHECK(info->On("geffen")[0].cell_y == 1023);
  CHECK(info->On("geffen")[0].facility == TownFacility::kSmith);
}

TEST_CASE("Commodités de ville : chaque nature a son nom", "[vfs][town_info]") {
  for (std::uint8_t rank = 0; rank < ro::vfs::kTownFacilityCount; ++rank) {
    const std::string entry =
        "{x: 1, y: 2, facility: " + std::string(ro::vfs::kTownFacilityNames.at(rank)) + "}";
    const auto info = TownInfo::Parse(WithEntry(entry));
    REQUIRE(info.has_value());
    CHECK(info->On("ile")[0].facility == static_cast<TownFacility>(rank));
  }
}

TEST_CASE("Commodités de ville : une table mal formée est refusée", "[vfs][town_info]") {
  CHECK_FALSE(TownInfo::Parse("towns: {}").has_value());
  CHECK_FALSE(TownInfo::Parse("version: 2\ntowns: {}").has_value());
  CHECK_FALSE(TownInfo::Parse("version: 1").has_value());
  CHECK_FALSE(TownInfo::Parse("version: 1\ntowns: {ile: {x: 1}}").has_value());
  CHECK_FALSE(TownInfo::Parse(WithEntry("{x: 1, y: 2}")).has_value());
  CHECK_FALSE(TownInfo::Parse(WithEntry("{x: 1, y: 2, facility: bank}")).has_value());
  CHECK_FALSE(TownInfo::Parse(WithEntry("{x: 1, y: 1024, facility: inn}")).has_value());
  CHECK_FALSE(TownInfo::Parse(WithEntry("{x: -1, y: 2, facility: inn}")).has_value());
  CHECK_FALSE(TownInfo::Parse(WithEntry("{x: one, y: 2, facility: inn}")).has_value());
  CHECK(TownInfo::Parse("version: 1\ntowns: {}").has_value());
}

TEST_CASE("Commodités de ville : la table livrée se relit en entier", "[vfs][town_info]") {
  const std::filesystem::path path = std::filesystem::path(__FILE__).parent_path() / ".." / ".." /
                                     "dist" / "data" / "yaml_files" / "town_info.yml";
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.good());
  std::ostringstream text;
  text << file.rdbuf();
  const auto info = TownInfo::Parse(text.str());
  REQUIRE(info.has_value());
  CHECK(info->map_count() > 0);
  // Les repères sont les PNJ que le serveur place : la Kafra de Prontera se
  // tient en 155,194.
  const auto prontera = info->On("prontera");
  CHECK(std::ranges::any_of(prontera, [](const ro::vfs::TownFacilityEntry& entry) {
    return entry.facility == TownFacility::kKafra && entry.cell_x == 155 && entry.cell_y == 194;
  }));
}
