#pragma once

#include <string_view>

#include "ro/formats/image.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// ── L'INSIGNE DE BOSS, À GAUCHE DU NOM ──────────────────────────────────────
//
// Une tête de mort ARGENT pour un mini-boss, DORÉE pour un MVP. Le client la
// choisit sur le seul rang que le paquet d'apparition déclare, et la pose au
// bord GAUCHE de l'étiquette, le texte décalé d'autant.
//
// 🔴 C'EST LE SEUL SIGNE QUI DISE VRAIMENT « BOSS ». La couleur rouge qu'on lui
// attribue volontiers n'en est pas un : elle vient de l'ÉCART DE NIVEAU (voir
// `NameColorForLevelGap`), et un monstre ordinaire assez fort la porte aussi.
// Les deux se rencontrent sur les mêmes monstres, ce qui les fait confondre.
//
// Le rang `1` et le rang `2` nomment chacun son fichier ; toute autre valeur ne
// pose rien.

// Les deux fichiers de l'installation. ⚠ ILS SONT DANS `data.grf`, pas dans un
// habillage : une installation les a.
inline constexpr std::string_view kMiniBossBadgePath =
    "data\\texture\\유저인터페이스\\montype_boss.bmp";
inline constexpr std::string_view kMvpBadgePath = "data\\texture\\유저인터페이스\\montype_mvp.bmp";

// Le côté de ces deux bitmaps, en pixels. ⚠ C'EST LA TAILLE DU FICHIER, et le
// client s'en sert telle quelle pour écarter le texte : la tête de mort
// n'occupe que treize pixels sur onze au milieu, et l'insigne paraît donc plus
// petit qu'il ne mesure. Sa pose : voir `ro/render/name_plate_layout.h`.
inline constexpr float kBossBadgeSizePixels = 24.0F;

// LES DEUX INSIGNES, tels que l'installation les donne.
//
// ⚠ CHACUN PEUT ÊTRE VIDE, et l'étiquette s'écrit alors sans insigne — comme
// pour toute ressource absente, cela ne coûte que ce qu'on ne voit pas.
//
// ⚠ LEUR ADRESSE EST LEUR IDENTITÉ pour la couche d'écran, qui reconnaît un
// atlas par pointeur pour ne le téléverser qu'une fois. Ces deux images doivent
// donc rester en place aussi longtemps que les quads qui les désignent.
struct BossBadges {
  formats::Image mini_boss;
  formats::Image mvp;
};

// Lit les deux insignes. Une absence se dit au journal et rend une image vide.
[[nodiscard]] BossBadges LoadBossBadges(vfs::Vfs& vfs);

}  // namespace ro::render
