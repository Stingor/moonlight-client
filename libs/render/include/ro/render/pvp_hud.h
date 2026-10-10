#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/screen_overlay.h"

namespace ro::render {

// ── LES DEUX AFFICHAGES DU CLASSEMENT PVP ───────────────────────────────────
//
// 🔴 UN SEUL PAQUET LES ALLUME TOUS LES DEUX, et l'on prendrait volontiers une
// capture qui les montre pour deux travaux séparés. `ZC_NOTIFY_RANKING 0x019a`
// donne le rang et le nombre de combattants ; le compteur les écrit en bas à
// droite, et le chronomètre — qu'aucun paquet ne pilote — compte depuis que le
// rang vaut UN.
//
// 🔴 CE SONT DES IMAGES, PAS DU TEXTE. Le client d'origine n'écrit pas ces
// nombres avec sa police : il pose des sprites, un par chiffre, tirés de deux
// fontes que l'installation porte. Les redessiner en texte donnerait des
// chiffres justes et une image fausse.
//
// ⚠ ET CETTE COUCHE NE CONNAÎT NI LE MONDE NI LE PROTOCOLE : elle reçoit deux
// entiers et une durée. C'est `app` qui les tire de `ro::sim::PvpRank`.

// ── LES DEUX FONTES ─────────────────────────────────────────────────────────
//
// ⚠ ONZE IMAGES CHACUNE, ET LE ONZIÈME N'EST PAS UN CHIFFRE : la barre oblique
// pour le compteur, le deux-points pour le chronomètre. L'ACTION porte le
// glyphe — action N pour le chiffre N, action 10 pour le séparateur — et c'est
// l'ACT qui remet les images dans cet ordre. `timefont.spr` les range en
// « 1 2 3 4 5 6 7 8 9 0 : », si bien que lire le SPR par son index donnerait
// tout décalé d'un cran.
//
// ⚠ `timefont.spr` EST OCTET POUR OCTET IDENTIQUE À `data\sprite\count.spr`.
// Le client charge la première et n'emploie jamais la seconde.
inline constexpr std::string_view kPvpRankFontPath = "data\\sprite\\이팩트\\rankfont";
inline constexpr std::string_view kPvpStopwatchFontPath = "data\\sprite\\이팩트\\timefont";

// L'action qui porte le séparateur, dans l'une et l'autre fonte.
inline constexpr std::size_t kPvpSeparatorAction = 10;

// ── OÙ LE COMPTEUR SE POSE ──────────────────────────────────────────────────
//
// 🔴 EN PIXELS FIXES DEPUIS LE COIN BAS-DROIT, et non en proportion : le
// compteur ne s'écarte pas du coin quand la fenêtre grandit. Relevé sur les
// trois écritures de position du client d'origine.
//
// 🔴 ET LES TROIS RANGÉES SONT EN ESCALIER. Le rang est le plus haut et le plus
// à gauche, la barre au milieu, le total le plus bas et le plus à droite : « 1/2 »
// descend en diagonale. Les aligner ferait un affichage plus propre et FAUX.
//
// ⚠ CES CHIFFRES SONT CEUX DU CLIENT VANILLA, et celui qui tourne chez le
// joueur est patché : un décalage de quelques pixels se corrige contre une
// capture côte à côte, il ne se redécouvre pas.
inline constexpr float kPvpRankDigitPitchPx = 25.0F;
inline constexpr float kPvpRankUnitsFromRightPx = 105.0F;
inline constexpr float kPvpRankUnitsFromBottomPx = 50.0F;
inline constexpr float kPvpRankBarFromRightPx = 80.0F;
inline constexpr float kPvpRankBarFromBottomPx = 40.0F;
inline constexpr float kPvpRankTotalFromBottomPx = 30.0F;

// ── OÙ LE CHRONOMÈTRE SE POSE ───────────────────────────────────────────────
//
// ⚠ CENTRÉ EN ABSCISSE, FIXE EN ORDONNÉE : la moitié de la largeur moins vingt
// pixels par glyphe, et quarante pixels depuis le haut. C'est le seul des deux
// qui suive la fenêtre.
inline constexpr float kPvpStopwatchFromTopPx = 40.0F;
inline constexpr float kPvpStopwatchGlyphPitchPx = 40.0F;
inline constexpr float kPvpStopwatchHalfPitchPx = 20.0F;

// COMBIEN DE GLYPHES UNE DURÉE DEMANDE.
//
// 🔴 LES TRANCHES DE TÊTE NULLES DISPARAISSENT, et par PAQUETS de trois : deux
// glyphes pour les seules secondes, cinq pour « MM:SS », huit pour « HH:MM:SS »,
// onze pour « DD:HH:MM:SS ». Il n'y a pas d'entre-deux — on ne voit jamais
// « M:SS » ni « 0:02:11 ».
//
// ⚠ ET LES POSITIONS 2, 5 ET 8 SONT DES SÉPARATEURS. C'est ce qui explique les
// paquets de trois, et c'est aussi pourquoi un affichage à trois ou quatre
// glyphes n'existe pas.
[[nodiscard]] std::size_t PvpStopwatchGlyphCount(std::uint32_t elapsed_ms) noexcept;

// Vrai quand ce rang de glyphe porte un séparateur plutôt qu'un chiffre.
[[nodiscard]] constexpr bool PvpStopwatchSeparatorAt(std::size_t rank) noexcept {
  return rank % 3 == 2;
}

// LE COMPTEUR « rang / total », en bas à droite.
//
// ⚠ VIDE SI L'UN DES DEUX EST NUL OU NÉGATIF, et c'est la garde du client
// d'origine, SIGNÉE comme la sienne. Le serveur envoie un rang et un total nuls
// en éteignant la zone, et `UINT32_MAX` pour un joueur caché — lu en signé,
// moins un. Aucun des deux ne doit rien peindre ; sa fonte ne porte d'ailleurs
// aucun glyphe tiret, contrairement à ce que le commentaire de rAthena promet.
// OÙ UNE PIÈCE SE POSE, ET À QUELLE TAILLE.
//
// 🔴 LE CLIENT D'ORIGINE N'A PAS CELA : il pose ses deux pièces à des pixels
// FIXES depuis les bords de l'écran, et c'est tout. Chez nous le joueur les
// déplace et les retaille, comme ses jauges libres — d'où ce paramètre.
//
// ⚠ LES DÉCALAGES DU CLIENT RESTENT CE QU'ILS SONT : ils se comptent depuis ce
// coin au lieu du bord de l'écran, et se multiplient par l'échelle. Les
// réécrire en proportions perdrait la mesure.
struct PvpHudPlacement {
  // LE POINT DONT LES DÉCALAGES PARTENT, et il n'est pas le même pour les deux
  // pièces : le coin BAS-DROIT pour le compteur, qui se range dans un coin ; le
  // milieu du bord HAUT pour le chronomètre, qui se centre.
  glm::vec2 corner_px{0.0F, 0.0F};
  float scale = 1.0F;
};

// LA POSE QUI REFAIT CELLE DU CLIENT D'ORIGINE, pour un écran de cette taille.
//
// C'est le défaut : tant que le joueur n'a rien déplacé, les deux pièces sont
// là où le client d'origine les met.
[[nodiscard]] PvpHudPlacement DefaultPvpRankPlacement(glm::vec2 viewport_px);
[[nodiscard]] PvpHudPlacement DefaultPvpStopwatchPlacement(glm::vec2 viewport_px);

[[nodiscard]] std::vector<ScreenQuad> ComposePvpRank(const formats::SprSprite& sprite,
                                                     const formats::ActAnimation& animation,
                                                     std::int32_t rank, std::int32_t total,
                                                     const PvpHudPlacement& placement);

// LE CHRONOMÈTRE DU PREMIER, en haut au centre.
//
// `elapsed_ms` est le temps écoulé depuis que le rang vaut un. L'appelant ne
// l'appelle que lorsqu'il y a lieu — voir `ro::sim::PvpRank::leader_since_ms`.
[[nodiscard]] std::vector<ScreenQuad> ComposePvpStopwatch(const formats::SprSprite& sprite,
                                                          const formats::ActAnimation& animation,
                                                          std::uint32_t elapsed_ms,
                                                          const PvpHudPlacement& placement);

}  // namespace ro::render
