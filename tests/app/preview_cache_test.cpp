// LES APERÇUS GARDÉS PAR L'ÉDITEUR DE CARTE : une neuve par image, les moins
// récemment demandées jetées d'abord, et l'échec mémorisé.

#include "world/preview_cache.h"

#include <cstddef>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>

using moonclient::PreviewCache;

namespace {

constexpr std::size_t kTwoKept = 2;

// Une fabrique qui compte ses appels et rend la longueur du nom ; un nom qui
// commence par « ! » échoue.
struct CountingMaker {
  int* calls = nullptr;

  std::optional<int> operator()(const std::string& file) const {
    ++*calls;
    if (!file.empty() && file.front() == '!') {
      return std::nullopt;
    }
    return static_cast<int>(file.size());
  }
};

}  // namespace

TEST_CASE("Aperçus : une seule neuve par image, la suivante attend", "[app][map_editor]") {
  PreviewCache<int> cache(kTwoKept);
  int calls = 0;
  const CountingMaker make{.calls = &calls};
  cache.BeginFrame();
  const auto first = cache.Get("abc", make);
  REQUIRE(first.picture != nullptr);
  CHECK(*first.picture == 3);
  CHECK(cache.Get("de", make).pending());
  // Ce qui est déjà fait se montre encore dans la même image.
  CHECK(cache.Get("abc", make).picture != nullptr);
  CHECK(calls == 1);
  cache.BeginFrame();
  const auto second = cache.Get("de", make);
  REQUIRE(second.picture != nullptr);
  CHECK(*second.picture == 2);
  CHECK(calls == 2);
}

TEST_CASE("Aperçus : la moins récemment demandée part la première", "[app][map_editor]") {
  PreviewCache<int> cache(kTwoKept);
  int calls = 0;
  const CountingMaker make{.calls = &calls};
  cache.BeginFrame();
  static_cast<void>(cache.Get("a", make));
  cache.BeginFrame();
  static_cast<void>(cache.Get("b", make));
  cache.BeginFrame();
  // « a » redemandé redevient le plus récent : c'est « b » qui partira.
  static_cast<void>(cache.Get("a", make));
  static_cast<void>(cache.Get("c", make));
  CHECK(cache.size() == kTwoKept);
  CHECK(calls == 3);
  cache.BeginFrame();
  CHECK(cache.Get("a", make).picture != nullptr);
  CHECK(calls == 3);
  CHECK(cache.Get("b", make).picture != nullptr);
  CHECK(calls == 4);
}

TEST_CASE("Aperçus : un échec se garde et ne se refait pas", "[app][map_editor]") {
  PreviewCache<int> cache(kTwoKept);
  int calls = 0;
  const CountingMaker make{.calls = &calls};
  cache.BeginFrame();
  const auto failed = cache.Get("!absent", make);
  CHECK(failed.failed);
  CHECK(failed.picture == nullptr);
  CHECK_FALSE(failed.pending());
  cache.BeginFrame();
  CHECK(cache.Get("!absent", make).failed);
  CHECK(calls == 1);
  cache.Clear();
  CHECK(cache.size() == 0);
}

namespace {

// Une horloge factice : chaque aperçu la fait avancer de `step_ms`.
struct SteppingClock {
  double* now_ms = nullptr;

  double operator()() const { return *now_ms; }
};

struct SlowMaker {
  double* now_ms = nullptr;
  double step_ms = 0.0;
  int* calls = nullptr;

  std::optional<int> operator()(const std::string& file) const {
    ++*calls;
    *now_ms += step_ms;
    return static_cast<int>(file.size());
  }
};

constexpr std::size_t kManyKept = 16;
constexpr double kBudgetMs = 10.0;

}  // namespace

TEST_CASE("Aperçus : un budget par image, en millisecondes", "[app][map_editor]") {
  PreviewCache<int> cache(kManyKept, kBudgetMs);
  double now_ms = 0.0;
  int calls = 0;
  // Trois millisecondes chacun : quatre tiennent sous dix — le quatrième
  // commence à neuf —, le cinquième attend.
  const SlowMaker make{.now_ms = &now_ms, .step_ms = 3.0, .calls = &calls};
  const SteppingClock clock{.now_ms = &now_ms};
  cache.BeginFrame();
  for (const char* file : {"a", "b", "c", "d"}) {
    CHECK(cache.Get(file, make, clock).picture != nullptr);
  }
  CHECK(cache.Get("e", make, clock).pending());
  CHECK(cache.made_this_frame() == 4);
  CHECK(calls == 4);
  cache.BeginFrame();
  CHECK(cache.Get("e", make, clock).picture != nullptr);
  CHECK(calls == 5);
}

TEST_CASE("Aperçus : le premier de l'image passe même plus long que le budget",
          "[app][map_editor]") {
  PreviewCache<int> cache(kManyKept, kBudgetMs);
  double now_ms = 0.0;
  int calls = 0;
  const SlowMaker make{.now_ms = &now_ms, .step_ms = 50.0, .calls = &calls};
  const SteppingClock clock{.now_ms = &now_ms};
  cache.BeginFrame();
  CHECK(cache.Get("lent", make, clock).picture != nullptr);
  CHECK(cache.Get("suivant", make, clock).pending());
  // Ce qui est fait se montre sans rien coûter, budget épuisé ou non.
  CHECK(cache.Get("lent", make, clock).picture != nullptr);
  CHECK(calls == 1);
}
