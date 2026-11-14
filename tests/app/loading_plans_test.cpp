// Les plans des chargements et les lignes que l'écran en montre.

#include "loading/loading_plans.h"

#include <algorithm>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/core/load_progress.h"

using moonclient::GameOpenPlan;
using moonclient::LoadPercentText;
using moonclient::LoadStepLine;
using moonclient::MapPlan;
using ro::core::LoadProgress;
using ro::core::LoadStep;
using ro::core::LoadStepWeight;

namespace {

[[nodiscard]] float ExpectedMsOf(std::span<const LoadStepWeight> plan, LoadStep step) {
  float total_ms = 0.0F;
  for (const LoadStepWeight& entry : plan) {
    if (entry.step == step) {
      total_ms += entry.expected_ms;
    }
  }
  return total_ms;
}

[[nodiscard]] bool EachStepOnce(std::span<const LoadStepWeight> plan) {
  for (const LoadStepWeight& entry : plan) {
    if (std::ranges::count_if(plan, [&entry](const LoadStepWeight& other) {
          return other.step == entry.step;
        }) != 1) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST_CASE("Plans de chargement : le sol arrondi pèse plus que le sol d'origine", "[app][loading]") {
  const std::vector<LoadStepWeight> flat = MapPlan(false);
  const std::vector<LoadStepWeight> rounded = MapPlan(true);
  CHECK(ExpectedMsOf(rounded, LoadStep::kGround) > ExpectedMsOf(flat, LoadStep::kGround));
  CHECK(EachStepOnce(flat));
  CHECK(EachStepOnce(rounded));
}

TEST_CASE("Plans de chargement : l'entrée en jeu ne compte les tables que si elles manquent",
          "[app][loading]") {
  CHECK(ExpectedMsOf(GameOpenPlan(false, true), LoadStep::kTables) > 0.0F);
  CHECK(ExpectedMsOf(GameOpenPlan(false, false), LoadStep::kTables) == 0.0F);
  CHECK(ExpectedMsOf(GameOpenPlan(true, false), LoadStep::kEffects) > 0.0F);
  CHECK(EachStepOnce(GameOpenPlan(true, true)));
}

TEST_CASE("Plans de chargement : une carte parcourue en entier finit à cent pour cent",
          "[app][loading]") {
  const std::vector<LoadStepWeight> plan = MapPlan(true);
  LoadProgress progress(plan);
  for (const LoadStepWeight& entry : plan) {
    progress.Begin(entry.step, 0);
  }
  progress.End();
  CHECK(progress.fraction() == Catch::Approx(1.0F));
  CHECK(LoadPercentText(progress.fraction()).starts_with("100"));
}

TEST_CASE("Plans de chargement : la ligne dit l'étape et son compte", "[app][loading]") {
  const std::vector<LoadStepWeight> plan = MapPlan(false);
  LoadProgress progress(plan);
  CHECK(LoadStepLine(progress).empty());
  progress.Begin(LoadStep::kModels, 540);
  progress.Advance(312);
  // Le libellé est traduit ; le compte, lui, ne l'est pas.
  CHECK(LoadStepLine(progress).ends_with(" 312 / 540"));
  progress.End();
  progress.Begin(LoadStep::kLights, 0);
  const std::string lights = LoadStepLine(progress);
  CHECK_FALSE(lights.empty());
  CHECK(lights.find('/') == std::string::npos);
}

TEST_CASE("Plans de chargement : le pourcentage reste entre zéro et cent", "[app][loading]") {
  CHECK(LoadPercentText(-1.0F).starts_with("0"));
  CHECK(LoadPercentText(0.574F).starts_with("57"));
  CHECK(LoadPercentText(2.0F).starts_with("100"));
}
