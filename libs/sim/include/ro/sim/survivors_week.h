#pragma once

#include <cstdint>

namespace ro::sim {

// LE CALENDRIER DU DÉFI HEBDOMADAIRE, en entiers : aucune horloge ici. L'appelant
// passe le jour UTC qu'il a lu ; le serveur refait le même calcul (lot 8S), et
// les deux doivent tomber sur la même semaine et la même graine.

// Une date du calendrier grégorien proleptique.
struct CivilDate {
  std::int32_t year = 1970;
  std::uint8_t month = 1;  // 1 à 12
  std::uint8_t day = 1;    // 1 à 31
  [[nodiscard]] bool operator==(const CivilDate&) const = default;
};

// Une semaine ISO 8601 : le lundi la commence, et la semaine 1 d'une année est
// celle qui contient son premier jeudi. L'année ISO peut donc différer de
// l'année civile autour du 1er janvier. {0, 0} : aucune semaine.
struct IsoWeek {
  std::int32_t year = 0;
  std::uint8_t week = 0;  // 1 à 53
  [[nodiscard]] bool operator==(const IsoWeek&) const = default;
};

// Les jours depuis le 1970-01-01, négatifs avant, et la date qu'ils désignent.
[[nodiscard]] CivilDate CivilDateOfDay(std::int64_t days_since_epoch) noexcept;
[[nodiscard]] std::int64_t DayOfCivilDate(const CivilDate& date) noexcept;
// Vrai quand le mois est dans 1..12 et le jour dans ce mois-là.
[[nodiscard]] bool CivilDateExists(const CivilDate& date) noexcept;

inline constexpr std::int64_t kSecondsPerDay = 86400;
inline constexpr std::int64_t kSecondsPerHour = 3600;
inline constexpr std::int64_t kSecondsPerMinute = 60;

// Un instant UTC découpé en date et heure.
struct UtcInstant {
  CivilDate date;
  std::uint8_t hour = 0;    // 0 à 23
  std::uint8_t minute = 0;  // 0 à 59
  std::uint8_t second = 0;  // 0 à 59
  [[nodiscard]] bool operator==(const UtcInstant&) const = default;
};

// Les secondes depuis le 1970-01-01 00:00:00 UTC, négatives avant, et
// l'instant qu'elles désignent.
[[nodiscard]] UtcInstant UtcInstantOf(std::int64_t utc_s) noexcept;

[[nodiscard]] IsoWeek IsoWeekOfDay(std::int64_t days_since_epoch) noexcept;
// Le lundi d'une semaine ISO, en jours depuis le 1970-01-01.
[[nodiscard]] std::int64_t MondayOfIsoWeek(const IsoWeek& week) noexcept;
// LE RANG D'UNE SEMAINE dans la suite continue des semaines : 0 pour 1970-W01
// (celle du 1970-01-01, ouverte le lundi 1969-12-29), négatif avant. Une
// rotation hebdomadaire s'y lit sans sauter au passage d'une année de 53
// semaines.
[[nodiscard]] std::int64_t IsoWeekOrdinal(const IsoWeek& week) noexcept;

// LA GRAINE DU DÉFI DE LA SEMAINE : le finaliseur SplitMix64 appliqué à
// `année ISO × 100 + semaine` plus un sel nommé (survivors_week.cpp). La
// formule est écrite dans l'ADR 0133 : le serveur rend la même valeur.
[[nodiscard]] std::uint64_t SeedOfWeek(std::int32_t iso_year, std::uint8_t iso_week) noexcept;

}  // namespace ro::sim
