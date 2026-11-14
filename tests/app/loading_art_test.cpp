// L'illustration de l'écran de chargement : un tirage borné, un chemin bien
// formé, et le moment d'en changer.

#include "loading/loading_art.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>

using moonclient::kLoadingArtCount;
using moonclient::kLoadingArtKeepMs;
using moonclient::LoadingArtExpired;
using moonclient::LoadingArtPath;
using moonclient::LoadingArtRank;

TEST_CASE("Illustration de chargement : tout tirage tombe dans les bornes", "[app][loading]") {
  for (const std::uint32_t draw :
       {0U, 1U, 35U, 36U, 1000U, std::numeric_limits<std::uint32_t>::max()}) {
    CHECK(LoadingArtRank(draw) < kLoadingArtCount);
  }
  // Chaque rang se tire.
  CHECK(LoadingArtRank(static_cast<std::uint32_t>(kLoadingArtCount - 1)) == kLoadingArtCount - 1);
  CHECK(LoadingArtRank(static_cast<std::uint32_t>(kLoadingArtCount)) == 0);
}

TEST_CASE("Illustration de chargement : le chemin porte le rang en deux chiffres",
          "[app][loading]") {
  CHECK(LoadingArtPath(0) == "data\\texture\\유저인터페이스\\loading00.jpg");
  CHECK(LoadingArtPath(7) == "data\\texture\\유저인터페이스\\loading07.jpg");
  CHECK(LoadingArtPath(kLoadingArtCount - 1) == "data\\texture\\유저인터페이스\\loading35.jpg");
  // Un rang hors bornes s'y ramène, plutôt que de demander un fichier absent.
  CHECK(LoadingArtPath(kLoadingArtCount) == LoadingArtPath(0));
}

TEST_CASE("Illustration de chargement : la même tant que les écrans se suivent", "[app][loading]") {
  constexpr std::uint32_t kShownMs = 10000;
  CHECK(LoadingArtExpired(std::nullopt, kShownMs));
  CHECK_FALSE(LoadingArtExpired(kShownMs, kShownMs));
  CHECK_FALSE(LoadingArtExpired(kShownMs, kShownMs + kLoadingArtKeepMs));
  CHECK(LoadingArtExpired(kShownMs, kShownMs + kLoadingArtKeepMs + 1));
}
