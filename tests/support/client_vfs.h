#pragma once

// LE CLIENT TEL QUE LE VFS LE VOIT : une installation montée sur une archive
// vide, pour l'essai qui n'a besoin d'aucun fichier, et les noms dans
// l'encodage de l'archive, pour celui qui y range les siens.

#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/encoding.h"
#include "ro/vfs/vfs.h"

#include "support/grf_builder.h"
#include "support/temp_directory.h"

namespace ro::test_support {

// Une installation montée dont l'unique archive est vide : tout ce qu'on y
// cherche manque, et le chemin qui lit le client reste celui du jeu.
inline vfs::Vfs EmptyClient(const TempDirectory& client) {
  client.WriteFile("vide.grf", BuildGrf({}));
  auto mounted = vfs::Vfs::Mount({.root = client.path(), .archives = {"vide.grf"}});
  REQUIRE(mounted.has_value());
  return std::move(*mounted);
}

// Un nom écrit en UTF-8 dans l'essai, rendu dans le CP949 de l'archive ; l'essai
// tombe si le texte ne s'y écrit pas.
inline std::string Cp949(std::string_view utf8) {
  auto converted = vfs::Utf8ToCp949(utf8);
  REQUIRE(converted.has_value());
  return *converted;
}

}  // namespace ro::test_support
