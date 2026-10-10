#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/entity_bars.h"

namespace ro::ui {

// ── LES BARRES D'INCANTATION AU-DESSUS DES ENTITÉS ──────────────────────────
//
// Deux peintures pour une même horloge — le début et la durée que le serveur a
// annoncés.
//
// 🔴 CELLE DU CLIENT, quand on ne la remplace pas : soixante pixels sur six,
// posés en `(x - 30, haut du sprite)`, soit AU-DESSUS DE LA TÊTE, au même
// ancrage que la bulle de parole — la bulle monte de là, la barre descend.
// Trois rectangles plats et rien d'autre : le cadre, le creux, la part écoulée,
// aux couleurs de la jauge de vie (c'est la même classe de jauge, jamais
// reteintée). Aucun texte : ni nom de compétence, ni durée.
//
// 🔴 CELLE QUI LA REMPLACE se règle point par point : taille, arrondi, couleurs,
// et l'étiquette que le client n'a jamais eue — le nom du sort et le temps
// restant.
//
// ⚠ `ui` NE SAIT RIEN DU MONDE : `app` projette la tête en pixels, dit qui
// incante et nomme le sort. Même frontière que les jauges de vie.

// QUAND LE NOM DU SORT S'ÉCRIT.
//
// ⚠ EN `int` PARCE QU'IL SE PERSISTE, et l'ordre part dans le fichier de
// réglages : ne pas intercaler.
enum class CastBarName : int {
  kNever = 0,
  // SEULEMENT SI AUCUNE BULLE NE L'ANNONCE DÉJÀ. 🔴 LE DÉFAUT : le client annonce
  // le sort d'un JOUEUR dans une bulle au-dessus de sa tête, et le récrire sur
  // la barre le donne deux fois. Un monstre, lui, n'annonce rien — c'est là que
  // le nom a de la valeur. La bulle se constate, elle ne se déduit pas de
  // l'espèce : un monstre qui parle et une bulle expirée au milieu d'un long
  // sort sont couverts du même coup.
  kIfFree = 1,
  kAlways = 2,
  kCount,
};

// Le rang lu du fichier, borné : un `static_cast` hors bornes sur une
// énumération n'est pas une valeur bizarre, c'est un comportement indéfini.
[[nodiscard]] CastBarName CastBarNameOf(int stored);

// LES MESURES DE LA BARRE DU CLIENT, en pixels.
inline constexpr float kClientCastBarWidthPixels = 60.0F;
inline constexpr float kClientCastBarHeightPixels = 6.0F;

// Les bornes des curseurs.
inline constexpr float kSmallestCastBarWidth = 30.0F;
inline constexpr float kLargestCastBarWidth = 300.0F;
inline constexpr float kSmallestCastBarHeight = 3.0F;
inline constexpr float kLargestCastBarHeight = 40.0F;
inline constexpr float kLargestCastBarOffset = 60.0F;
inline constexpr float kLargestCastBarRounding = 12.0F;
inline constexpr float kSmallestCastBarFontScale = 0.6F;
inline constexpr float kLargestCastBarFontScale = 1.6F;
inline constexpr float kSmallestCastBarOpacity = 0.25F;

// LA TEINTE D'ORIGINE DES BARRES : bleue, et orange sur les monstres — le sort
// qu'on surveille n'est pas le leur, et une couleur commune le noie au milieu
// d'un pack.
inline constexpr ImU32 kCastBarFill = IM_COL32(107, 173, 255, 255);
inline constexpr ImU32 kMonsterCastBarFill = IM_COL32(255, 140, 64, 255);

// LA BARRE D'UNE ESPÈCE, telle que le joueur l'a réglée.
struct CastBarLook {
  // ⚠ ÉTEINTE, ELLE L'EST AUSSI SANS REMPLACEMENT : c'est alors la barre du
  // client qu'on masque — sinon, décocher le remplacement ferait revenir celle
  // qu'on voulait cacher.
  bool shown = true;
  CastBarName name = CastBarName::kIfFree;
  bool time_left = true;
  bool border = true;
  // Le client en fait soixante sur six, illisibles à distance.
  float width_pixels = 76.0F;
  float height_pixels = 9.0F;
  // Vers le bas quand il est positif, depuis le haut du sprite.
  float offset_pixels = 0.0F;
  float rounding = 3.0F;
  // Une part de la taille du texte de l'interface.
  float font_scale = 0.8F;
  // De 0,25 à un, appliquée à TOUT — fond, remplissage, cadre et texte.
  float opacity = 1.0F;
  ImU32 background = IM_COL32(13, 13, 18, 199);
  ImU32 fill = kCastBarFill;
};

// Les barres d'origine, une par espèce : orange sur les monstres et les boss.
[[nodiscard]] constexpr std::array<CastBarLook, kEntityBarKindCount> DefaultCastBarLooks() {
  std::array<CastBarLook, kEntityBarKindCount> looks{};
  looks[static_cast<std::size_t>(EntityBarKind::kMonster)].fill = kMonsterCastBarFill;
  looks[static_cast<std::size_t>(EntityBarKind::kBoss)].fill = kMonsterCastBarFill;
  return looks;
}

// CE QUE LE JOUEUR A FAIT DES BARRES D'INCANTATION.
struct CastBarsLook {
  // REMPLACER LA BARRE DU CLIENT. Éteint, c'est la sienne qui se peint, telle
  // quelle — et l'apparence de chaque espèce se grise.
  bool replaced = true;
  // UNE BARRE PAR ESPÈCE, rangée comme celles de vie : une barre fine sur les
  // moutons, une large et nommée sur un MVP.
  std::array<CastBarLook, kEntityBarKindCount> kinds = DefaultCastBarLooks();
};

// CE QU'`app` A TROUVÉ SUR UN ACTEUR QUI INCANTE, en pixels d'écran.
struct CastBarView {
  // Le point d'ancrage : `x` celui des pieds, `y` le haut du sprite — là où la
  // bulle de parole prend appui.
  ImVec2 head{0.0F, 0.0F};
  EntityBarKind kind = EntityBarKind::kPlayer;
  std::uint32_t elapsed_ms = 0;
  std::uint32_t total_ms = 0;
  // Le nom du sort, vide quand on ne le connaît pas. ⚠ UNE VUE : ce qu'elle
  // désigne doit survivre à la peinture.
  std::string_view skill_name;
  // Une bulle occupe-t-elle la place au-dessus ? Voir `CastBarName::kIfFree`.
  bool balloon = false;
};

// VRAI SI CETTE BARRE PARAÎT : son espèce allumée, remplacée ou non.
[[nodiscard]] bool ShowsCastBar(const CastBarsLook& look, const CastBarView& what);

// La part écoulée, de zéro à un.
[[nodiscard]] float CastBarProgress(const CastBarView& what);

// LES TROIS RECTANGLES DE LA BARRE DU CLIENT, en pixels entiers.
//
// 🔴 LA PART PLEINE EST CELLE DU CLIENT, ARRONDIS COMPRIS : un pourcentage
// ENTIER de l'écoulé, puis `largeur × pour cent / 100 − 2` tronqué. Elle ne
// paraît donc qu'à cinq pour cent, et rejoint le bord du creux au terme.
struct ClientCastBarRects {
  ImVec2 frame_min;
  ImVec2 frame_max;
  ImVec2 trough_min;
  ImVec2 trough_max;
  // Vide quand `fill_max.x <= fill_min.x`.
  ImVec2 fill_min;
  ImVec2 fill_max;
};
[[nodiscard]] ClientCastBarRects ClientCastBarRectsOf(const CastBarView& what);

// L'ÉTIQUETTE : le nom quand le réglage le veut et qu'on le connaît, le temps
// restant — « Storm Gust  1,4 s ». Vide quand elle n'a rien à dire.
[[nodiscard]] std::string CastBarCaption(const CastBarLook& look, const CastBarView& what);

// « 1,4 s ». ⚠ LE SÉPARATEUR DÉCIMAL VIENT DU CATALOGUE : l'anglais pose le sien.
[[nodiscard]] std::string CastSecondsText(std::uint32_t remaining_ms);

// UNE BARRE, celle du client ou celle qui la remplace, selon le réglage. ⚠
// ELLE NE FILTRE RIEN : c'est `ShowsCastBar` qui dit si elle paraît.
void PaintCastBar(ImDrawList& into, const CastBarView& what, const CastBarsLook& look);

// Peint toutes les barres qui paraissent, sur la liste de FOND.
void DrawCastBars(std::span<const CastBarView> bars, const CastBarsLook& look);

}  // namespace ro::ui
