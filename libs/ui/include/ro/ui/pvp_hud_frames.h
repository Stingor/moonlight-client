#pragma once

#include <array>
#include <cstddef>

#include <glm/glm.hpp>

#include "ro/render/pvp_hud.h"
#include "ro/ui/align_grid.h"
#include "ro/ui/hud_frame.h"

namespace ro::ui {

// ── LA PLACE DES DEUX PIÈCES DU CLASSEMENT PVP ──────────────────────────────
//
// Le compteur « rang / total » et le chronomètre du premier. Le client d'origine
// les pose à des pixels FIXES ; ici le joueur les
// pose lui-même, comme ses jauges libres.
//
// 🔴 CETTE COUCHE NE DESSINE NI L'UN NI L'AUTRE, et c'est la raison d'être de ce
// fichier. Les deux pièces sont des SPRITES du GRF — deux fontes de chiffres —,
// que la couche de rendu compose en quads d'écran ; `ui` ne sait pas les peindre
// et n'a pas à l'apprendre. Elle ne tient ici que la POIGNÉE : un cadre sans
// fond, qu'on saisit pour déplacer et par les bords pour retailler, et dont la
// géométrie sert ensuite d'ancre au dessin.
//
// ⚠ VERROUILLÉ, LE CADRE DISPARAÎT ENTIÈREMENT et laisse passer les clics : ce
// qui reste à l'écran est le dessin, que la couche de rendu pose. C'est ce qui
// permet de composer son écran puis de jouer dessus.

// Les deux pièces, par leur rang.
enum class PvpHudPart : std::size_t {
  // Le compteur du coin, « 1/2 ».
  kRank = 0,
  // Le chronomètre du premier, « 02:11 ».
  kTimer,
  kCount,
};

inline constexpr std::size_t kPvpHudPartCount = static_cast<std::size_t>(PvpHudPart::kCount);

[[nodiscard]] constexpr std::size_t Rank(PvpHudPart part) noexcept {
  return static_cast<std::size_t>(part);
}

// LA TAILLE DE RÉFÉRENCE D'UNE PIÈCE, celle où ses chiffres ont la taille que le
// client d'origine leur donne.
//
// ⚠ C'EST ELLE QUI DONNE L'ÉCHELLE, et non un réglage à part : retailler le
// cadre agrandit les chiffres dans la même proportion. Un curseur d'échelle EN
// PLUS du cadre donnerait deux façons de dire la même chose, qui se
// contrediraient dès qu'on emploie les deux.
//
// Le compteur : trois rangées de chiffres au pas de vingt-cinq pixels, larges
// d'une centaine. Le chronomètre : onze glyphes au pas de quarante.
inline constexpr float kPvpRankReferenceWidth = 110.0F;
inline constexpr float kPvpRankReferenceHeight = 90.0F;
inline constexpr float kPvpTimerReferenceWidth = 440.0F;
inline constexpr float kPvpTimerReferenceHeight = 40.0F;

// L'échelle qu'un cadre donne à sa pièce : le rapport de sa largeur à la
// largeur de référence.
//
// ⚠ LA LARGEUR SEULE, et non le plus petit des deux rapports. Les deux pièces
// sont des RANGÉES de chiffres : c'est leur largeur qui dit combien de place
// elles prennent, et suivre la hauteur ferait rapetisser le compteur dès qu'on
// tire son cadre vers le bas pour lui faire de la marge.
[[nodiscard]] float PvpHudScaleOf(PvpHudPart part, const HudRect& rect) noexcept;

// La place et la taille des deux pièces, plus ce qui vaut pour les deux.
struct PvpHudLook {
  // ⚠ VERROUILLÉ PAR DÉFAUT : on ne compose son écran que lorsqu'on le décide,
  // et un HUD qui s'attrape par accident en pleine mêlée est un HUD perdu.
  bool locked = true;
  bool sticky = true;
  bool center_mark = false;
  // Un filet autour des cadres, pendant la pose : ils n'ont pas de fond, et sans
  // lui on ne voit pas ce qu'on attrape tant qu'on ne l'a pas attrapé.
  bool border = true;
  // ⚠ LARGEUR NULLE = JAMAIS POSÉ, ET IL FAUT L'ÉCRIRE. Le défaut d'un `HudRect`
  // est cent vingt sur vingt : une taille PLAUSIBLE, donc indiscernable d'un
  // cadre que le joueur aurait posé lui-même. Laisser ce défaut ferait naître les
  // deux pièces au coin haut-gauche au lieu de leur place d'origine, et rien ne
  // dirait que c'est un trou plutôt qu'un choix. Voir `PvpHudRectUnset`.
  std::array<HudRect, kPvpHudPartCount> parts{
      HudRect{.x = 0.0F, .y = 0.0F, .width = 0.0F, .height = 0.0F},
      HudRect{.x = 0.0F, .y = 0.0F, .width = 0.0F, .height = 0.0F},
  };
};

// LE CADRE QU'UNE PIÈCE A TANT QUE LE JOUEUR N'A RIEN POSÉ : celui qui met le
// dessin là où le client d'origine le met.
//
// 🔴 IL SE CALCULE ET NE SE PERSISTE PAS. Un défaut écrit dans le fichier de
// réglages serait celui d'UNE taille d'écran, et suivrait le joueur qui change
// de résolution — deux pièces coincées hors de l'image, sans rien qui le dise.
[[nodiscard]] HudRect DefaultPvpHudRect(PvpHudPart part, glm::vec2 viewport_px) noexcept;

// VRAI QUAND CE CADRE N'A JAMAIS ÉTÉ POSÉ.
//
// ⚠ UNE LARGEUR NULLE NE PEUT PAS VENIR D'UN GESTE : `HudFrameLook::least_width`
// borne toute prise à huit pixels. C'est ce qui en fait un drapeau sûr, là où
// n'importe quelle taille plausible se confondrait avec un choix du joueur.
[[nodiscard]] bool PvpHudRectUnset(const HudRect& rect) noexcept;

// LA POSE QU'UN CADRE DONNE À SA PIÈCE, prête pour la couche de rendu.
//
// ⚠ LE POINT DE RÉFÉRENCE N'EST PAS LE MÊME POUR LES DEUX, et c'est la figure
// qui le décide : le compteur se range dans un coin et part donc de son coin
// bas-droit ; le chronomètre se centre — son nombre de glyphes change avec la
// durée — et part du milieu de son bord haut.
[[nodiscard]] render::PvpHudPlacement PvpHudPlacementOf(PvpHudPart part, const PvpHudLook& look,
                                                        glm::vec2 viewport_px) noexcept;

// LES DEUX CADRES, POUR CETTE IMAGE. Rend vrai si le joueur en a bougé un — à
// l'appelant de persister.
//
// ⚠ ELLE NE FAIT RIEN QUAND TOUT EST VERROUILLÉ, et c'est voulu : un cadre
// verrouillé ne se dessine pas, ne prend pas la souris, et n'a donc aucune
// raison d'être ouvert. Ce qui reste visible à l'écran est le dessin de la
// couche de rendu.
//
// ⚠ ELLE SÈME LES CADRES JAMAIS POSÉS avant de les ouvrir : sans cela le premier
// déverrouillage montrerait deux poignées au coin haut-gauche, loin du dessin
// qu'elles commandent.
bool PvpHudFrames(PvpHudLook& look, const AlignGrid& grid, HudGrip& grip, glm::vec2 viewport_px);

}  // namespace ro::ui
