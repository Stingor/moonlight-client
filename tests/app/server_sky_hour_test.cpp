// L'HEURE DU SERVEUR, lue dans son fuseau : celle sur laquelle le cycle jour et
// nuit se cale.

#include "world/server_sky_hour.h"

#include <chrono>
#include <cmath>
#include <optional>

#include <catch2/catch_test_macros.hpp>

using moonclient::ServerSkyHour;

namespace {

constexpr float kTolerance = 1e-3F;

std::chrono::system_clock::time_point UtcAt(int year, unsigned month, unsigned day, int hour,
                                            int minute) {
  return std::chrono::sys_days(std::chrono::year(year) / std::chrono::month(month) /
                               std::chrono::day(day)) +
         std::chrono::hours(hour) + std::chrono::minutes(minute);
}

}  // namespace

TEST_CASE("Heure du serveur : elle avance entre deux secondes", "[app][sky]") {
  constexpr int kYear = 2026;
  const auto start = UtcAt(kYear, 1, 15, 10, 30);
  const std::optional<float> before = ServerSkyHour("Europe/Paris", start);
  if (!before.has_value()) {
    SKIP("la base des fuseaux de ce système ne connaît pas Europe/Paris");
  }
  // Une demi-seconde plus tard, le soleil a déjà bougé : il ne saute pas
  // d'un cran par seconde.
  const std::optional<float> after =
      ServerSkyHour("Europe/Paris", start + std::chrono::milliseconds(500));
  REQUIRE(after.has_value());
  CHECK(*after > *before);
}

TEST_CASE("Heure du serveur : Paris, à l'heure d'hiver puis d'été", "[app][sky]") {
  constexpr int kYear = 2026;
  const std::optional<float> winter = ServerSkyHour("Europe/Paris", UtcAt(kYear, 1, 15, 10, 30));
  if (!winter.has_value()) {
    SKIP("la base des fuseaux de ce système ne connaît pas Europe/Paris");
  }
  // UTC+1 en janvier.
  CHECK(std::abs(*winter - 11.5F) < kTolerance);
  // UTC+2 en juillet.
  const std::optional<float> summer = ServerSkyHour("Europe/Paris", UtcAt(kYear, 7, 1, 10, 30));
  REQUIRE(summer.has_value());
  CHECK(std::abs(*summer - 12.5F) < kTolerance);
  // Le jour d'après commence à minuit, heure de Paris : 23 h 30 UTC en hiver.
  const std::optional<float> after_midnight =
      ServerSkyHour("Europe/Paris", UtcAt(kYear, 1, 15, 23, 30));
  REQUIRE(after_midnight.has_value());
  CHECK(std::abs(*after_midnight - 0.5F) < kTolerance);
}

TEST_CASE("Heure du serveur : « HH:MM », à la minute commencée", "[app][sky]") {
  CHECK(moonclient::ServerClockText(0.0F) == "00:00");
  CHECK(moonclient::ServerClockText(9.5F) == "09:30");
  // Juste avant midi : toujours 11 h 59, jamais « 11:60 ».
  CHECK(moonclient::ServerClockText(11.9999F) == "11:59");
  CHECK(moonclient::ServerClockText(23.999F) == "23:59");
}

TEST_CASE("Heure du serveur : un fuseau inconnu ne rend rien", "[app][sky]") {
  constexpr int kYear = 2026;
  CHECK_FALSE(ServerSkyHour("Nulle/Part", UtcAt(kYear, 1, 15, 10, 0)).has_value());
}
