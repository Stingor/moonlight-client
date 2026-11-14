// LE DOSSIER DE CACHE DE L'UTILISATEUR : le choix entre les sources du système,
// et le dossier du client composé dessous.

#include "ro/core/user_cache_directory.h"

#include <filesystem>
#include <optional>

#include <catch2/catch_test_macros.hpp>

using ro::core::ClientCacheDirectoryUnder;
using ro::core::ErrorCode;
using ro::core::PickWindowsCacheDirectory;
using ro::core::PickXdgCacheDirectory;

namespace {

// Un chemin absolu sur la plateforme de l'essai : « C:\ » ou « / » selon elle.
std::filesystem::path Absolute(const char* below) {
  return std::filesystem::current_path().root_path() / below;
}

}  // namespace

TEST_CASE("Cache de l'utilisateur : sous Windows, le dossier connu d'abord", "[core][cache]") {
  const std::filesystem::path known = Absolute("connu");
  const std::filesystem::path variable = Absolute("variable");
  CHECK(PickWindowsCacheDirectory(known, variable) == known);
  CHECK(PickWindowsCacheDirectory(std::nullopt, variable) == variable);
  // Un dossier connu vide ou relatif ne compte pas.
  CHECK(PickWindowsCacheDirectory(std::filesystem::path(), variable) == variable);
  CHECK(PickWindowsCacheDirectory(std::filesystem::path("relatif"), variable) == variable);
}

TEST_CASE("Cache de l'utilisateur : sous Windows, rien d'absolu, pas de dossier", "[core][cache]") {
  const auto none = PickWindowsCacheDirectory(std::nullopt, std::filesystem::path("relatif"));
  REQUIRE_FALSE(none.has_value());
  CHECK(none.error().code == ErrorCode::kNotFound);
}

TEST_CASE("Cache de l'utilisateur : sous Linux, XDG_CACHE_HOME puis ~/.cache", "[core][cache]") {
  const std::filesystem::path xdg = Absolute("xdg");
  const std::filesystem::path home = Absolute("maison");
  CHECK(PickXdgCacheDirectory(xdg, home) == xdg);
  CHECK(PickXdgCacheDirectory(std::nullopt, home) == home / ".cache");
  // La spécification XDG ignore un chemin relatif.
  CHECK(PickXdgCacheDirectory(std::filesystem::path("relatif"), home) == home / ".cache");
  const auto none = PickXdgCacheDirectory(std::nullopt, std::nullopt);
  REQUIRE_FALSE(none.has_value());
  CHECK(none.error().code == ErrorCode::kNotFound);
}

TEST_CASE("Cache de l'utilisateur : le client y range son propre dossier", "[core][cache]") {
  const std::filesystem::path user_cache = Absolute("cache");
  CHECK(ClientCacheDirectoryUnder(user_cache) == user_cache / "Moonlight-Destiny");
}
