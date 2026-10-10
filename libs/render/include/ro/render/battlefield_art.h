#pragma once

#include <string_view>

#include "ro/formats/image.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// ── LES IMAGES DU CHAMP DE BATAILLE ─────────────────────────────────────────
//
// La marque posée au-dessus d'un ennemi, et les deux emblèmes du score : le
// lion du premier camp, l'aigle du second. Dans `data.grf` : une installation
// les a.
inline constexpr std::string_view kEnemyMarkerPath =
    "data\\texture\\유저인터페이스\\전장적표시.bmp";
inline constexpr std::string_view kLionEmblemPath = "data\\texture\\effect\\사자(a).bmp";
inline constexpr std::string_view kEagleEmblemPath = "data\\texture\\effect\\독수리(b).bmp";

// ⚠ CHACUNE PEUT ÊTRE VIDE, et ce qu'elle dessine se tait alors.
//
// ⚠ LEUR ADRESSE EST LEUR IDENTITÉ pour la couche d'écran, qui reconnaît un
// atlas par pointeur : elles restent en place aussi longtemps que les quads qui
// les désignent.
struct BattlefieldArt {
  formats::Image enemy_marker;
  formats::Image lion;
  formats::Image eagle;
};

// Lit les trois images. Une absence se dit au journal et rend une image vide.
[[nodiscard]] BattlefieldArt LoadBattlefieldArt(vfs::Vfs& vfs);

}  // namespace ro::render
