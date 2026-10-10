// Les images du champ de bataille. Voir l'en-tête.

#include "ro/render/battlefield_art.h"

#include <string_view>
#include <utility>

#include "ro/core/log.h"

namespace ro::render {
namespace {

// Une image, lue dans l'installation.
//
// `Read` et non `ReadRaw` : ces chemins sont des constantes À NOUS, donc de
// l'UTF-8, et c'est le VFS qui les transcode vers le CP949 de l'archive.
[[nodiscard]] formats::Image LoadOne(vfs::Vfs& vfs, std::string_view path) {
  auto bytes = vfs.Read(path);
  if (!bytes) {
    core::LogWarning("champ de bataille : {} absent — son dessin se taira", path);
    return {};
  }
  auto image = formats::DecodeImage(path, *bytes);
  if (!image) {
    core::LogWarning("champ de bataille : {} illisible ({}) — son dessin se taira", path,
                     core::Describe(image.error()));
    return {};
  }
  // Le magenta est le fond de la marque d'ennemi ; les emblèmes n'en portent
  // pas, et la passe ne leur retire rien.
  formats::ApplyMagentaColorKey(*image);
  return std::move(*image);
}

}  // namespace

BattlefieldArt LoadBattlefieldArt(vfs::Vfs& vfs) {
  BattlefieldArt art;
  art.enemy_marker = LoadOne(vfs, kEnemyMarkerPath);
  art.lion = LoadOne(vfs, kLionEmblemPath);
  art.eagle = LoadOne(vfs, kEagleEmblemPath);
  return art;
}

}  // namespace ro::render
