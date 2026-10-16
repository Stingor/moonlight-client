#include "world/server_sky_hour.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string>

namespace moonclient {

std::optional<float> ServerSkyHour(std::string_view zone,
                                   std::chrono::system_clock::time_point now) {
  using std::chrono::duration;
  using std::chrono::milliseconds;
  // ⚠ LA BASE DES FUSEAUX SIGNALE UN NOM INCONNU PAR EXCEPTION : elle s'arrête
  // ici, et devient un résultat vide.
  try {
    const std::chrono::time_zone* place = std::chrono::locate_zone(std::string(zone));
    // ⚠ À LA MILLISECONDE, PAS À LA SECONDE : le soleil avance alors à chaque
    // image, et les ombres ne sautent pas d'un cran par seconde.
    const auto local = place->to_local(std::chrono::time_point_cast<milliseconds>(now));
    const auto since_midnight = local - std::chrono::floor<std::chrono::days>(local);
    return duration<float, std::ratio<3600>>(since_midnight).count();
  } catch (const std::runtime_error&) {
    return std::nullopt;
  }
}

std::string ServerClockText(float hour) {
  constexpr int kMinutesPerDay = 24 * 60;
  constexpr int kMinutesPerHour = 60;
  // ⚠ EN MINUTES ENTIÈRES D'ABORD : les heures et les minutes ne se tirent pas
  // à part, sans quoi 11 h 59,99 s'écrirait « 11:60 ».
  const int minutes =
      std::clamp(static_cast<int>(std::floor(hour * static_cast<float>(kMinutesPerHour))), 0,
                 kMinutesPerDay - 1);
  return std::format("{:02}:{:02}", minutes / kMinutesPerHour, minutes % kMinutesPerHour);
}

}  // namespace moonclient
