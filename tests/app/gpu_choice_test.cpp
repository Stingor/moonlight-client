// La préférence de carte graphique se lit dans la valeur que Windows range, et
// s'y réécrit sans toucher aux autres champs.

#include "options/gpu_choice.h"

#include <catch2/catch_test_macros.hpp>

using moonclient::GpuPreference;
using moonclient::GpuPreferenceAt;
using moonclient::GpuPreferenceIn;
using moonclient::WithGpuPreference;

TEST_CASE("la préférence se lit parmi les autres champs") {
  CHECK(GpuPreferenceIn("AppStatus=4;GpuPreference=2;") == GpuPreference::kHighPerformance);
  CHECK(GpuPreferenceIn("GpuPreference=1;") == GpuPreference::kPowerSaving);
  CHECK(GpuPreferenceIn("GpuPreference=0;") == GpuPreference::kAutomatic);
  // Le dernier point-virgule est facultatif.
  CHECK(GpuPreferenceIn("GpuPreference=2") == GpuPreference::kHighPerformance);
}

TEST_CASE("une préférence absente ou illisible vaut l'automatique") {
  CHECK(GpuPreferenceIn("") == GpuPreference::kAutomatic);
  CHECK(GpuPreferenceIn("AppStatus=0;") == GpuPreference::kAutomatic);
  CHECK(GpuPreferenceIn("GpuPreference=7;") == GpuPreference::kAutomatic);
  CHECK(GpuPreferenceIn("GpuPreference=2x;") == GpuPreference::kAutomatic);
  CHECK(GpuPreferenceIn("GpuPreference=;") == GpuPreference::kAutomatic);
  // Un nom qui ne fait que commencer pareil n'est pas le champ.
  CHECK(GpuPreferenceIn("GpuPreferenceOld=2;") == GpuPreference::kAutomatic);
}

TEST_CASE("réécrire la préférence garde les autres champs, dans leur ordre") {
  CHECK(WithGpuPreference("AppStatus=4;GpuPreference=2;", GpuPreference::kPowerSaving) ==
        "AppStatus=4;GpuPreference=1;");
  CHECK(WithGpuPreference("SwapEffectUpgradeEnable=1;Flag;", GpuPreference::kHighPerformance) ==
        "SwapEffectUpgradeEnable=1;Flag;GpuPreference=2;");
  CHECK(WithGpuPreference("", GpuPreference::kHighPerformance) == "GpuPreference=2;");
}

TEST_CASE("l'automatique retire le champ au lieu d'écrire zéro") {
  CHECK(WithGpuPreference("AppStatus=4;GpuPreference=2;", GpuPreference::kAutomatic) ==
        "AppStatus=4;");
  CHECK(WithGpuPreference("GpuPreference=1;", GpuPreference::kAutomatic).empty());
  CHECK(WithGpuPreference("", GpuPreference::kAutomatic).empty());
}

TEST_CASE("relire ce qu'on a écrit rend ce qu'on a demandé") {
  for (const GpuPreference wanted :
       {GpuPreference::kAutomatic, GpuPreference::kPowerSaving, GpuPreference::kHighPerformance}) {
    CHECK(GpuPreferenceIn(WithGpuPreference("AppStatus=4;GpuPreference=1;", wanted)) == wanted);
  }
}

TEST_CASE("un rang de liste hors des trois garde la préférence d'avant") {
  CHECK(GpuPreferenceAt(2, GpuPreference::kAutomatic) == GpuPreference::kHighPerformance);
  CHECK(GpuPreferenceAt(0, GpuPreference::kPowerSaving) == GpuPreference::kAutomatic);
  CHECK(GpuPreferenceAt(-1, GpuPreference::kPowerSaving) == GpuPreference::kPowerSaving);
  CHECK(GpuPreferenceAt(3, GpuPreference::kPowerSaving) == GpuPreference::kPowerSaving);
}
