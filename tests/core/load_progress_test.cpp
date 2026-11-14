// L'avancement d'un chargement : la fraction pondérée, l'étape ouverte, et la
// cadence de l'écran qui le montre.

#include "ro/core/load_progress.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ro::core::LoadPacing;
using ro::core::LoadProgress;
using ro::core::LoadStep;
using ro::core::LoadStepScope;
using ro::core::LoadStepWeight;

namespace {

// Un plan d'un quart et de trois quarts : les parts se lisent sans calcul.
constexpr std::array<LoadStepWeight, 2> kPlan = {
    LoadStepWeight{.step = LoadStep::kMap, .expected_ms = 100.0F},
    LoadStepWeight{.step = LoadStep::kModels, .expected_ms = 300.0F},
};

// Compte les avis qu'un observateur reçoit.
class CountingObserver final : public ro::core::LoadObserver {
 public:
  void Changed(const LoadProgress& progress) override {
    ++calls;
    last_fraction = progress.fraction();
  }
  int calls = 0;
  float last_fraction = 0.0F;
};

}  // namespace

TEST_CASE("Chargement : la fraction pèse chaque étape par sa durée prévue",
          "[core][load_progress]") {
  LoadProgress progress(kPlan);
  CHECK(progress.fraction() == 0.0F);
  progress.Begin(LoadStep::kMap, 0);
  // Une étape sans compte ne pèse qu'à sa fermeture.
  CHECK(progress.fraction() == 0.0F);
  progress.End();
  CHECK(progress.fraction() == Catch::Approx(0.25F));

  progress.Begin(LoadStep::kModels, 4);
  CHECK(progress.step() == LoadStep::kModels);
  progress.Advance(2);
  CHECK(progress.done() == 2);
  CHECK(progress.total() == 4);
  CHECK(progress.fraction() == Catch::Approx(0.25F + 0.75F * 0.5F));
  // Le compte ne dépasse pas le total.
  progress.Advance(10);
  CHECK(progress.done() == 4);
  CHECK(progress.fraction() == Catch::Approx(1.0F));
  progress.End();
  CHECK_FALSE(progress.step().has_value());
  CHECK(progress.fraction() == Catch::Approx(1.0F));
}

TEST_CASE("Chargement : ouvrir une étape ferme la précédente, et une étape hors plan ne pèse rien",
          "[core][load_progress]") {
  LoadProgress progress(kPlan);
  progress.Begin(LoadStep::kMap, 3);
  progress.Begin(LoadStep::kTables, 2);
  CHECK(progress.fraction() == Catch::Approx(0.25F));
  progress.Advance(2);
  CHECK(progress.fraction() == Catch::Approx(0.25F));
  CHECK(progress.step() == LoadStep::kTables);
}

TEST_CASE("Chargement : un plan vide reste à zéro", "[core][load_progress]") {
  LoadProgress progress({});
  progress.Begin(LoadStep::kMap, 1);
  progress.Advance();
  progress.End();
  CHECK(progress.fraction() == 0.0F);
}

TEST_CASE("Chargement : l'observateur est prévenu à chaque mouvement", "[core][load_progress]") {
  CountingObserver observer;
  LoadProgress progress(kPlan, &observer);
  {
    const LoadStepScope models(&progress, LoadStep::kModels, 2);
    models.Advance();
    models.Advance();
  }
  // Ouverture, deux avances, fermeture.
  CHECK(observer.calls == 4);
  CHECK(observer.last_fraction == Catch::Approx(0.75F));
}

TEST_CASE("Chargement : un élément se compte à la fin de son tour, jamais à son début",
          "[core][load_progress]") {
  // 🔴 LE COMPTE DIT CE QUI EST FAIT : pendant la lecture du dernier modèle,
  // l'écran montre « 2 / 3 », et non « 3 / 3 ».
  constexpr std::size_t kItems = 3;
  LoadProgress progress(kPlan);
  {
    const LoadStepScope models(&progress, LoadStep::kModels, kItems);
    for (std::size_t rank = 0; rank < kItems; ++rank) {
      const ro::core::LoadItemDone counted(models);
      CHECK(progress.done() == rank);
      if (rank == 1) {
        // Un tour abandonné en route se compte aussi.
        continue;
      }
      CHECK(progress.done() == rank);
    }
    CHECK(progress.done() == kItems);
  }
}

TEST_CASE("Chargement : une étape sans puits se tait", "[core][load_progress]") {
  const LoadStepScope silent(nullptr, LoadStep::kModels, 2);
  silent.Advance();
  SUCCEED("aucun puits, aucun accès");
}

TEST_CASE("Chargement : l'écran attend son seuil, puis présente à cadence bornée",
          "[core][load_progress]") {
  constexpr std::uint32_t kStartedMs = 1000;
  LoadPacing pacing(kStartedMs, /*already_shown=*/false);
  CHECK_FALSE(pacing.Due(kStartedMs));
  CHECK_FALSE(pacing.Due(kStartedMs + ro::core::kLoadingScreenDelayMs - 1));
  const std::uint32_t first_ms = kStartedMs + ro::core::kLoadingScreenDelayMs;
  REQUIRE(pacing.Due(first_ms));
  pacing.NotePresented(first_ms);
  CHECK(pacing.shown());
  CHECK_FALSE(pacing.Due(first_ms + ro::core::kLoadingPresentIntervalMs - 1));
  CHECK(pacing.Due(first_ms + ro::core::kLoadingPresentIntervalMs));
}

TEST_CASE("Chargement : l'écran apparaît en fondu depuis le noir", "[core][load_progress]") {
  constexpr std::uint32_t kStartedMs = 500;
  const LoadPacing pacing(kStartedMs, /*already_shown=*/false);
  const std::uint32_t threshold_ms = kStartedMs + ro::core::kLoadingScreenDelayMs;
  CHECK(pacing.Opacity(kStartedMs) == 0.0F);
  CHECK(pacing.Opacity(threshold_ms) == 0.0F);
  CHECK(pacing.Opacity(threshold_ms + ro::core::kLoadingScreenFadeMs / 2) ==
        Catch::Approx(0.5F).margin(0.01F));
  CHECK(pacing.Opacity(threshold_ms + ro::core::kLoadingScreenFadeMs) == 1.0F);
  CHECK(pacing.Opacity(threshold_ms + 2 * ro::core::kLoadingScreenFadeMs) == 1.0F);
}

TEST_CASE("Chargement : un écran déjà montré reste plein, sans seuil", "[core][load_progress]") {
  constexpr std::uint32_t kStartedMs = 42;
  const LoadPacing pacing(kStartedMs, /*already_shown=*/true);
  CHECK(pacing.Due(kStartedMs));
  CHECK(pacing.Opacity(kStartedMs) == 1.0F);
}

TEST_CASE("Chargement : la cadence survit au tour de l'horloge", "[core][load_progress]") {
  // L'horloge de trente-deux bits fait le tour en quarante-neuf jours.
  constexpr std::uint32_t kStartedMs = 0xFFFFFFF0U;
  const LoadPacing pacing(kStartedMs, /*already_shown=*/false);
  const std::uint32_t after_ms = kStartedMs + ro::core::kLoadingScreenDelayMs;
  CHECK(pacing.Due(after_ms));
}
