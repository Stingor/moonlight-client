// Les deux insignes de boss. Voir l'en-tête : le rang vient du paquet, et le
// fichier du rang.

#include "ro/render/boss_badge.h"

#include <string_view>
#include <utility>

#include "ro/core/log.h"

namespace ro::render {
namespace {

// Un insigne, lu dans l'installation.
//
// `Read` et non `ReadRaw` : ce chemin est une constante À NOUS, donc de
// l'UTF-8, et c'est le VFS qui le transcode vers le CP949 de l'archive. Le
// passer en brut chercherait un nom que le GRF ne porte pas.
[[nodiscard]] formats::Image LoadOne(vfs::Vfs& vfs, std::string_view path) {
  auto bytes = vfs.Read(path);
  if (!bytes) {
    core::LogWarning("insigne de boss : {} absent — le nom s'écrira sans insigne", path);
    return {};
  }
  auto image = formats::DecodeImage(path, *bytes);
  if (!image) {
    core::LogWarning("insigne de boss : {} illisible ({}) — le nom s'écrira sans insigne", path,
                     core::Describe(image.error()));
    return {};
  }
  // Le magenta de ces deux bitmaps est leur FOND : sans cette passe, la tête de
  // mort paraîtrait dans un carré rose vif de vingt-quatre pixels.
  formats::ApplyMagentaColorKey(*image);
  return std::move(*image);
}

}  // namespace

BossBadges LoadBossBadges(vfs::Vfs& vfs) {
  BossBadges badges;
  badges.mini_boss = LoadOne(vfs, kMiniBossBadgePath);
  badges.mvp = LoadOne(vfs, kMvpBadgePath);
  return badges;
}

}  // namespace ro::render
