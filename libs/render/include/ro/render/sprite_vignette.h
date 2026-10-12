#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_canvas.h"

namespace ro::render {

// UNE VIGNETTE D'ACTEUR : une silhouette composée, écrasée en UNE image.
//
// ⚠ POUR CE QUI VEUT UNE TEXTURE UNIQUE — une case d'interface, qui pose une
// image et non des quads. Le rendu du jeu ne passe pas par ici : il a ses
// quads, et les poser lui coûte moins que de les écraser d'abord.
//
// ⚠ AUCUNE DÉPENDANCE À OPENGL : l'image se fabrique en mémoire, donc le
// cadrage se vérifie par des nombres, sans fenêtre ni pilote. Ce qui la
// téléverse vit à côté, dans `actor_vignette.h`.

// L'ENVELOPPE D'UNE TENUE pour une animation : toutes ses images, les huit
// directions.
//
// 🔴 C'EST ELLE QUI FIXE L'ÉCHELLE, ET NON LA POSE AFFICHÉE. Mesurer la pose
// qu'on regarde fait sauter la taille à la molette : une arme s'écarte de face
// et se replie de dos, si bien que le personnage grandit et rétrécit pendant
// qu'on le tourne. Sur l'enveloppe de tout, ce qui rentre rentre dans toutes
// les positions.
struct OutfitSpan {
  // Tout ce que l'acteur porte, coiffes et cape comprises.
  SpriteBounds all;
  // Le CORPS SEUL. C'est la seule stature qui ne dépende ni de l'équipement ni
  // de la direction, donc la seule qui donne une taille comparable d'un
  // personnage à l'autre — et le seul point fixe où poser les pieds.
  SpriteBounds body;
  // Faux quand la tenue n'a rien à montrer. ⚠ UN DRAPEAU, ET NON DES BORNES
  // NULLES : une enveloppe vide est indiscernable d'une enveloppe non mesurée,
  // et le cadrage la prendrait pour une mesure.
  bool measured = false;
};

// Mesure l'enveloppe d'une animation.
//
// ⚠ ELLE RECOMPOSE LA TENUE image par image, les huit directions : c'est du
// calcul, pas du chargement, mais il ne se fait pas à chaque image de jeu. Le
// résultat vaut tant que l'apparence et l'animation ne changent pas.
//
// Une animation que le .act ne porte pas mesure celle de l'attente, comme le
// reste de la composition.
[[nodiscard]] OutfitSpan MeasureOutfitSpan(ActorOutfit& outfit, std::size_t animation);

// L'ENVELOPPE D'UNE SEULE POSE : celle que l'on va dessiner, et rien d'autre.
//
// 🔴 POUR UNE VIGNETTE FIGÉE, ET POUR ELLE SEULE. `MeasureOutfitSpan` couvre
// les huit directions parce qu'un pantin TOURNE : ce qui rentre doit rentrer
// dans toutes les positions. Une case de grille, elle, ne tournera jamais —
// elle montre la face, une fois, pour toujours. La mesurer sur les huit la
// rapetisse de tout ce que la plus encombrante occupe : une coiffure dont
// l'arrière est haut se montre alors nettement plus petite que sa voisine,
// sans que rien ne l'explique, et l'on croit à un défaut de sprite.
//
// ⚠ L'ENVELOPPE VIENT DE LA POSE, LE CORPS DE LA TENUE : c'est le sprite de sa
// première pièce qui reconnaît les calques du corps parmi les autres, la même
// règle qu'au-dessus.
[[nodiscard]] OutfitSpan MeasurePoseSpan(const ActorOutfit& outfit, const ComposedActor& composed);

// L'ENVELOPPE DE TOUTES LES IMAGES D'UNE ANIMATION DANS UNE SEULE DIRECTION.
//
// ⚠ POUR LE CADRAGE CENTRÉ, qui ne regarde qu'une direction : un monstre qui
// s'étale de profil ne doit pas rapetisser de face. `measured` n'exige que
// l'enveloppe entière — un monstre n'a qu'une pièce, son corps.
[[nodiscard]] OutfitSpan MeasureDirectionSpan(ActorOutfit& outfit, std::size_t animation,
                                              std::size_t direction);

// COMMENT UNE VIGNETTE TOMBE DANS SA BOÎTE.
enum class VignetteFit : std::uint8_t {
  // Les pieds au bas, centrée en abscisse sur l'enveloppe, AJUSTÉE (agrandie
  // comme réduite). Voir `FrameVignette`.
  kStanding,
  // L'enveloppe entière centrée en X ET en Y, à l'échelle
  // min(largeur / étendue_x, hauteur / étendue_y) PLAFONNÉE À UN : un petit
  // monstre reste petit, un grand est réduit. Le plancher de stature et
  // l'échelle imposée ne s'y appliquent pas.
  kCenteredShrinkOnly,
};

// LA BOÎTE OÙ LA VIGNETTE TOMBE, en pixels d'écran.
struct VignetteBox {
  std::int32_t width = 0;
  std::int32_t height = 0;
  // HAUTEUR MINIMALE DU CORPS, en pixels ; zéro pour aucune.
  //
  // 🔴 UN PLANCHER DE STATURE, et il existe parce que la LARGEUR écrase le
  // personnage : une coiffe de costume qui pose son sprite À CÔTÉ de lui — un
  // compagnon, une monture — double l'enveloppe, et tout faire rentrer le
  // réduit de moitié sous un grand vide. Passé ce plancher, on renonce à tout
  // faire rentrer : ce qui déborde sur les côtés est coupé par la boîte.
  float min_body_height = 0.0F;
  // L'ÉCHELLE IMPOSÉE, en pixels de boîte par pixel de sprite ; zéro pour
  // laisser le cadrage la choisir.
  //
  // 🔴 ELLE EXISTE POUR UNE SCÈNE, c'est-à-dire pour plusieurs acteurs CÔTE À
  // CÔTE. Tant qu'une vignette est seule dans sa case, l'ajuster à son cadre
  // est ce qu'on veut : elle est aussi grande que la case le permet. Dès
  // qu'il y en a deux, ajuster chacune à la sienne les met à des échelles
  // DIFFÉRENTES — un grand chapeau, une arme d'hast, et son porteur rétrécit
  // de moitié à côté de son voisin nu. Une échelle commune est le seul moyen
  // que deux personnages voisins soient comparables ; ce qui dépasse déborde,
  // et c'est le prix.
  //
  // ⚠ ELLE CHANGE AUSSI LE CENTRAGE, et ce n'est pas un détail séparé : à
  // l'ajustement, l'enveloppe ENTIÈRE tient dans la boîte, donc la centrer est
  // juste. Imposée, elle n'y tient plus — et centrer sur l'enveloppe
  // DÉPLACERAIT le personnage au lieu de laisser sortir ce qui dépasse : une
  // arme large le pousserait de côté, un compagnon posé à sa droite l'enverrait
  // à gauche. Le CORPS est le seul point fixe une fois l'ajustement retiré.
  //
  // ⚠ ET LE PLANCHER DE STATURE NE S'Y APPLIQUE PAS : il est le recours de
  // l'ajustement contre une enveloppe trop large, et il n'y a plus
  // d'ajustement à secourir.
  float scale = 0.0F;
  // Le cadrage. Voir `VignetteFit`.
  VignetteFit fit = VignetteFit::kStanding;
};

// OÙ L'ORIGINE DE L'ACTEUR TOMBE DANS LA BOÎTE, et de combien il grandit.
//
// L'origine est le point que le .act pose à l'emplacement de l'acteur : ses
// PIEDS, et non le centre de sa boîte.
struct VignetteFraming {
  float scale = 0.0F;
  glm::vec2 origin{0.0F, 0.0F};

  [[nodiscard]] bool usable() const noexcept { return scale > 0.0F; }
};

// Cadre une enveloppe dans une boîte : tout rentre, le personnage est centré en
// abscisse, ses pieds touchent le bas.
//
// ⚠ SAUF SI LA BOÎTE IMPOSE SON ÉCHELLE — voir `VignetteBox::scale` : le
// personnage est alors centré sur son CORPS, et ce qui dépasse sort du cadre.
//
// ⚠ LES PIEDS VIENNENT DU CORPS, la largeur de l'enveloppe ENTIÈRE. Prendre les
// pieds sur l'enveloppe enfoncerait le personnage sous le bas du cadre dès
// qu'une cape descend plus bas que ses bottes ; prendre la largeur sur le corps
// rognerait tout ce qu'il porte.
//
// ⚠ TOUT CELA VAUT POUR `VignetteFit::kStanding`. Le cadrage centré ne lit que
// l'enveloppe entière, et ignore l'échelle imposée comme le plancher.
[[nodiscard]] VignetteFraming FrameVignette(const OutfitSpan& span, const VignetteBox& box);

// LE PLUS GRAND CÔTÉ D'UN CANEVAS DE VIGNETTE, en pixels. Une vignette est une
// case d'interface : rien ne demande davantage, et une boîte démesurée sous une
// petite échelle n'allouerait plus une image mais des centaines de mégaoctets.
inline constexpr float kVignetteLargestCanvasPx = 1024.0F;

// Écrase les calques d'un acteur composé dans une image qui couvre la boîte.
//
// 🔴 L'IMAGE EST EN PIXELS DE SPRITE, PAS EN PIXELS D'ÉCRAN, et c'est ce qui
// donne un avatar LISSE. Rastériser à l'échelle de la boîte reviendrait à
// agrandir au plus proche dans le processeur : à 1,6, un pixel sur deux est
// doublé et la silhouette se découpe en marches d'escalier, quel que soit le
// filtre posé sur la texture ensuite — puisqu'elle serait alors posée un pour
// un. On dessine donc à l'échelle UN, et c'est la carte qui agrandit, en
// bilinéaire, quand l'interface étire la texture sur la boîte.
//
// La taille rendue est donc celle de la boîte DIVISÉE par l'échelle du
// cadrage. Ce qui en sort est coupé, comme le ferait un rectangle de découpe :
// c'est ce que le plancher de stature rend possible.
//
// ⚠ LE CANEVAS NE DÉPASSE JAMAIS `kVignetteLargestCanvasPx` DE CÔTÉ : au-delà,
// l'image est rendue plus grossièrement, la silhouette au même endroit.
//
// Rend une image vide quand la boîte ou le cadrage ne tiennent pas debout.
[[nodiscard]] formats::Image DrawVignette(const ComposedActor& actor, const VignetteBox& box,
                                          const VignetteFraming& framing);

}  // namespace ro::render
