// LE CALENDRIER DU DÉFI : les semaines ISO aux bords d'année, la date civile
// dans les deux sens, et la graine de la semaine que le serveur devra rendre.

#include "ro/sim/survivors_week.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::sim::CivilDate;
using ro::sim::CivilDateExists;
using ro::sim::CivilDateOfDay;
using ro::sim::DayOfCivilDate;
using ro::sim::IsoWeek;
using ro::sim::IsoWeekOfDay;
using ro::sim::SeedOfWeek;
using ro::sim::UtcInstant;
using ro::sim::UtcInstantOf;

namespace {

[[nodiscard]] IsoWeek WeekOf(std::int32_t year, std::uint8_t month, std::uint8_t day) {
  return IsoWeekOfDay(DayOfCivilDate(CivilDate{.year = year, .month = month, .day = day}));
}

}  // namespace

TEST_CASE("Semaine survivors : la date civile dans les deux sens", "[sim][survivors]") {
  CHECK(DayOfCivilDate(CivilDate{}) == 0);
  CHECK(CivilDateOfDay(0) == CivilDate{});
  // Valeurs relevées à part (Python, `date.toordinal`).
  CHECK(DayOfCivilDate(CivilDate{.year = 2020, .month = 12, .day = 31}) == 18627);
  CHECK(DayOfCivilDate(CivilDate{.year = 2026, .month = 12, .day = 28}) == 20815);
  CHECK(CivilDateOfDay(-1) == CivilDate{.year = 1969, .month = 12, .day = 31});
  CHECK(CivilDateOfDay(DayOfCivilDate(CivilDate{.year = 2024, .month = 2, .day = 29})) ==
        CivilDate{.year = 2024, .month = 2, .day = 29});
  // Chaque jour de 1960 à 2100 revient à lui-même.
  constexpr std::int64_t kFirstDay = -3653;
  constexpr std::int64_t kLastDay = 47482;
  bool round_trip = true;
  for (std::int64_t day = kFirstDay; day <= kLastDay; ++day) {
    if (DayOfCivilDate(CivilDateOfDay(day)) != day) round_trip = false;
  }
  CHECK(round_trip);
}

TEST_CASE("Semaine survivors : une date qui n'existe pas se reconnaît", "[sim][survivors]") {
  CHECK(CivilDateExists(CivilDate{.year = 2024, .month = 2, .day = 29}));
  CHECK_FALSE(CivilDateExists(CivilDate{.year = 2026, .month = 2, .day = 29}));
  CHECK_FALSE(CivilDateExists(CivilDate{.year = 2100, .month = 2, .day = 29}));
  CHECK(CivilDateExists(CivilDate{.year = 2000, .month = 2, .day = 29}));
  CHECK_FALSE(CivilDateExists(CivilDate{.year = 2026, .month = 4, .day = 31}));
  CHECK_FALSE(CivilDateExists(CivilDate{.year = 2026, .month = 13, .day = 1}));
  CHECK_FALSE(CivilDateExists(CivilDate{.year = 2026, .month = 1, .day = 0}));
}

TEST_CASE("Semaine survivors : les semaines ISO des bords d'année", "[sim][survivors]") {
  CHECK(WeekOf(2020, 12, 31) == IsoWeek{.year = 2020, .week = 53});
  CHECK(WeekOf(2021, 1, 3) == IsoWeek{.year = 2020, .week = 53});
  CHECK(WeekOf(2021, 1, 4) == IsoWeek{.year = 2021, .week = 1});
  CHECK(WeekOf(2026, 12, 28) == IsoWeek{.year = 2026, .week = 53});
  // Le 1970-01-01 est un jeudi : la semaine 1 de 1970.
  CHECK(IsoWeekOfDay(0) == IsoWeek{.year = 1970, .week = 1});
  // Un dimanche ferme sa semaine, le lundi suivant ouvre la suivante.
  CHECK(WeekOf(2026, 10, 25) == IsoWeek{.year = 2026, .week = 43});
  CHECK(WeekOf(2026, 10, 26) == IsoWeek{.year = 2026, .week = 44});
}

TEST_CASE("Semaine survivors : la graine de trois semaines, épinglée", "[sim][survivors]") {
  // La valeur que le serveur devra rendre (ADR 0133), calculée à part.
  CHECK(SeedOfWeek(2026, 43) == 0x0551'40FB'F975'2556ULL);
  CHECK(SeedOfWeek(2026, 53) == 0xF5B9'B308'E3D2'B7A1ULL);
  CHECK(SeedOfWeek(2027, 1) == 0x9D08'63B3'A0BD'853FULL);
}

TEST_CASE("Semaine survivors : le calendrier tient loin de 1970", "[sim][survivors]") {
  // Valeurs relevées à part (Python, `date.toordinal() − 719163`).
  constexpr std::int64_t kFirstDay = -719162;  // 0001-01-01
  constexpr std::int64_t kLastDay = 2932896;   // 9999-12-31
  CHECK(DayOfCivilDate(CivilDate{.year = 1, .month = 1, .day = 1}) == kFirstDay);
  CHECK(CivilDateOfDay(kFirstDay) == CivilDate{.year = 1, .month = 1, .day = 1});
  CHECK(DayOfCivilDate(CivilDate{.year = 9999, .month = 12, .day = 31}) == kLastDay);
  CHECK(CivilDateOfDay(kLastDay) == CivilDate{.year = 9999, .month = 12, .day = 31});
  CHECK(CivilDateOfDay(DayOfCivilDate(CivilDate{.year = 1600, .month = 2, .day = 29})) ==
        CivilDate{.year = 1600, .month = 2, .day = 29});
  // Un jour sur sept, de l'an 1 à l'an 9999, revient à lui-même.
  constexpr std::int64_t kStepDays = 7;
  bool round_trip = true;
  for (std::int64_t day = kFirstDay; day <= kLastDay; day += kStepDays) {
    if (DayOfCivilDate(CivilDateOfDay(day)) != day) round_trip = false;
  }
  CHECK(round_trip);
}

TEST_CASE("Semaine survivors : un instant UTC se découpe en date et heure", "[sim][survivors]") {
  CHECK(UtcInstantOf(0) == UtcInstant{});
  // 2026-10-30 14:03:22 UTC.
  constexpr std::int64_t kAfternoonS = 50'602;
  const std::int64_t day = DayOfCivilDate(CivilDate{.year = 2026, .month = 10, .day = 30});
  CHECK(UtcInstantOf(day * ro::sim::kSecondsPerDay + kAfternoonS) ==
        UtcInstant{.date = CivilDate{.year = 2026, .month = 10, .day = 30},
                   .hour = 14,
                   .minute = 3,
                   .second = 22});
  // Une seconde avant 1970 : la veille, à 23:59:59.
  CHECK(UtcInstantOf(-1) == UtcInstant{.date = CivilDate{.year = 1969, .month = 12, .day = 31},
                                       .hour = 23,
                                       .minute = 59,
                                       .second = 59});
}

TEST_CASE("Semaine survivors : le lundi et le rang continu d'une semaine", "[sim][survivors]") {
  using ro::sim::IsoWeekOrdinal;
  using ro::sim::MondayOfIsoWeek;
  // Valeurs relevées à part (Python, `date.fromisocalendar`).
  CHECK(MondayOfIsoWeek(IsoWeek{.year = 1970, .week = 1}) == -3);
  CHECK(MondayOfIsoWeek(IsoWeek{.year = 2026, .week = 43}) ==
        DayOfCivilDate(CivilDate{.year = 2026, .month = 10, .day = 19}));
  CHECK(MondayOfIsoWeek(IsoWeek{.year = 2020, .week = 53}) ==
        DayOfCivilDate(CivilDate{.year = 2020, .month = 12, .day = 28}));
  CHECK(IsoWeekOrdinal(IsoWeek{.year = 1970, .week = 1}) == 0);
  CHECK(IsoWeekOrdinal(IsoWeek{.year = 1969, .week = 52}) == -1);
  CHECK(IsoWeekOrdinal(IsoWeek{.year = 2026, .week = 43}) == 2964);
  // La semaine 53 et la semaine 1 suivante se suivent sans trou.
  CHECK(IsoWeekOrdinal(IsoWeek{.year = 2026, .week = 53}) == 2974);
  CHECK(IsoWeekOrdinal(IsoWeek{.year = 2027, .week = 1}) == 2975);
  // Chaque jour de 2000 à 2040 a le rang de sa semaine ISO.
  constexpr std::int64_t kFirstDay = 10957;  // 2000-01-01
  constexpr std::int64_t kLastDay = 25567;   // 2040-01-01
  constexpr std::int64_t kDaysPerWeek = 7;
  constexpr std::int64_t kFirstEpochMonday = -3;
  bool consistent = true;
  for (std::int64_t day = kFirstDay; day <= kLastDay; ++day) {
    const IsoWeek week = IsoWeekOfDay(day);
    if (IsoWeekOrdinal(week) != (day - kFirstEpochMonday) / kDaysPerWeek) consistent = false;
    if (day < MondayOfIsoWeek(week) || day >= MondayOfIsoWeek(week) + kDaysPerWeek) {
      consistent = false;
    }
  }
  CHECK(consistent);
}
