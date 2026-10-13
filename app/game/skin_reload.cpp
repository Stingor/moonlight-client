// Le skin, lu et relu. Voir l'en-tête.

#include "game/skin_reload.h"

#include <array>

#include "ro/core/log.h"
#include "ro/formats/image.h"
#include "ro/render/battlefield_art.h"
#include "ro/render/boss_badge.h"
#include "ro/render/signboard.h"
#include "ro/render/skin_pieces.h"

namespace moonclient {
namespace {

// LES IMAGES DU MONDE TIRÉES DU DOSSIER D'INTERFACE. Le peintre des
// surimpressions les range par ADRESSE, et une image relue garde la sienne :
// sans cet oubli, il peindrait l'ancienne.
void ForgetWorldArt(GamePieces& pieces) {
  const std::array<const ro::formats::Image*, 11> images = {&pieces.boss_badges.mini_boss,
                                                            &pieces.boss_badges.mvp,
                                                            &pieces.battlefield_art.enemy_marker,
                                                            &pieces.battlefield_art.lion,
                                                            &pieces.battlefield_art.eagle,
                                                            &pieces.signboards.open_room,
                                                            &pieces.signboards.locked_room,
                                                            &pieces.signboards.vending,
                                                            &pieces.signboards.buying,
                                                            &pieces.signboards.spotlit_vending,
                                                            &pieces.signboards.spotlit_buying};
  for (const ro::formats::Image* image : images) {
    pieces.overlay.ForgetAtlas(*image);
  }
}

}  // namespace

void LoadSkinTextures(ro::vfs::Vfs& vfs, GamePieces& pieces) {
  const ro::render::SkinPieces read = ro::render::LoadSkinPieces(vfs);
  pieces.skin_missing = read.missing.size();
  if (read.complete()) {
    ro::core::LogInfo("habillage : les {} pièces sont là", ro::render::kSkinPieceCount);
  } else {
    // ⚠ EN ALERTE : c'est une ressource absente sur laquelle on se replie, et
    // cette ligne est le SEUL canal par lequel le manque atteint quelqu'un —
    // `pieces.skin_missing` n'est lu nulle part ailleurs.
    ro::core::LogWarning("habillage : {} pièce(s) sur {} manquent à l'installation",
                         read.missing.size(), ro::render::kSkinPieceCount);
  }
  pieces.skin = ro::render::SkinTextures::Upload(read);
}

void ReloadSkinPieces(ro::vfs::Vfs& vfs, GamePieces& pieces, bool world_loaded) {
  LoadSkinTextures(vfs, pieces);
  if (pieces.game_images.has_value()) {
    pieces.game_images->Clear();
  }
  if (pieces.item_icons.has_value()) {
    pieces.item_icons->Clear();
  }
  if (pieces.skill_icons.has_value()) {
    pieces.skill_icons->Clear();
  }
  if (pieces.card_thumbnails.has_value()) {
    pieces.card_thumbnails->Clear();
  }
  // Les flèches du guidage : elles se relisent d'elles-mêmes, mais le sol
  // garde leur texture sous le même chemin.
  pieces.effect_ground.ForgetTextures();
  // Le pantin des descriptions a son propre moteur d'effets.
  if (pieces.doll_effects.has_value()) {
    pieces.doll_effects->ForgetTextures();
  }
  if (pieces.portrait_effects.has_value()) {
    pieces.portrait_effects->ForgetTextures();
  }
  if (world_loaded) {
    ForgetWorldArt(pieces);
    pieces.boss_badges = ro::render::LoadBossBadges(vfs);
    pieces.battlefield_art = ro::render::LoadBattlefieldArt(vfs);
    pieces.signboards = ro::render::LoadSignboardSurfaces(vfs);
  }
}

}  // namespace moonclient
