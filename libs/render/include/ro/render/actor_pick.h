#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/sprite_renderer.h"

namespace ro::render {

// Désigner un acteur à la souris.
//
// Le client ne teste pas les pixels d'un sprite : chaque acteur dessiné dépose
// à l'écran le RECTANGLE de ses quads, et le pointeur désigne le rectangle qui
// le contient (un arbre de rectangles interrogé au pixel de la souris).
// C'est ce qui rend un personnage cliquable entre ses
// jambes, et un Poring de loin cliquable tout court.
//
// Comme le pointage du sol, cela vit dans render : c'est l'INVERSE de la
// projection, et le mettre ailleurs le laisserait dériver de la caméra qu'il
// inverse. Il ne connaît ni le réseau ni le monde : il rend un rang, pas une
// intention.

// Le point de l'écran où se projette un point du monde, en pixels de FENÊTRE
// — l'origine en haut à gauche, l'axe des Y vers le bas, comme la souris.
// Rien pour un point qui passe derrière l'œil.
[[nodiscard]] std::optional<glm::vec2> ProjectToScreen(const Camera& camera, float aspect_ratio,
                                                       const glm::vec3& world_point,
                                                       float viewport_width, float viewport_height);

// Un rectangle d'écran, bornes comprises.
struct ScreenRect {
  glm::vec2 min{0.0F, 0.0F};
  glm::vec2 max{0.0F, 0.0F};

  [[nodiscard]] bool Contains(glm::vec2 point) const noexcept {
    return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
  }
};

// Le rectangle que l'acteur OCCUPE vraiment à l'écran, son ombre exceptée.
//
// C'est celui qu'on surmonte d'une jauge ou d'un nom : il épouse la taille du
// sprite, quand celui du CLIC est élargi pour qu'un petit acteur reste
// attrapable. Rien si l'acteur n'a pas de quad ou s'il passe derrière l'œil.
[[nodiscard]] std::optional<ScreenRect> ActorScreenBounds(const Camera& camera, float aspect_ratio,
                                                          const DrawnActor& drawn,
                                                          float viewport_width,
                                                          float viewport_height);

// COMBIEN DE PIXELS D'ÉCRAN FAIT UN PIXEL DE SON SPRITE, là où il se tient :
// la hauteur de ce qui est dessiné, rapportée à sa hauteur en pixels de sprite.
// Sans la taille du corps (`DrawnActor::scale`) : c'est l'échelle de la caméra
// et de la fenêtre à cet endroit. Rien sans dessin, ou derrière l'œil.
//
// ⚠ CE N'EST PAS `ScreenAnchorScale`, qui multiplie les ÉCARTS de ce qui se pose
// sur un acteur : les deux suivent le recul au même pas, mais ne valent pas la
// même chose.
[[nodiscard]] std::optional<float> ActorSpriteScreenScale(const Camera& camera, float aspect_ratio,
                                                          const DrawnActor& drawn,
                                                          float viewport_width,
                                                          float viewport_height);

// La plus petite emprise cliquable d'un acteur, en pixels, dans une fenêtre de
// 640 de large. Voir `PickFloorPixels`.
inline constexpr float kMinimumPickSizePixels = kDefaultPickFloorPixels;

// ── LES PLANCHERS DE CLIC ───────────────────────────────────────────────────
//
// Le client élargit de part et d'autre tout rectangle cliquable plus étroit
// qu'un plancher. Ce plancher vaut un FACTEUR par famille — quarante pour un
// acteur, trente-quatre pour un objet au sol ou une unité de compétence — fois
// la largeur de la fenêtre rapportée à 640, tronqué à l'entier.
inline constexpr float kPickFloorReferenceWidth = 640.0F;
inline constexpr float kActorPickFloorFactor = kDefaultPickFloorPixels;
inline constexpr float kGroundPickFloorFactor = 34.0F;
// La hauteur de fenêtre à laquelle le client rapporte la hauteur d'un rectangle.
inline constexpr float kPickHeightReferenceHeight = 480.0F;
// Le plus grand diviseur qu'un réglage applique aux planchers : un seizième.
inline constexpr int kLargestPickFloorShift = 4;

// LE PLANCHER D'UNE FAMILLE dans cette fenêtre, en pixels entiers, divisé par
// deux `shift` fois — zéro pour le client tel quel. Jamais moins d'un pixel.
[[nodiscard]] float PickFloorPixels(float factor, float viewport_width, int shift);

// L'EMPRISE D'UNE UNITÉ DE COMPÉTENCE, un bloc de glace ou un piège.
//
// Le client ne regarde pas le modèle de l'unité : il projette le point de sa
// case au sol, y pose un rectangle de quatre pixels de sprite de large et deux
// de haut, le bord bas sur le point — mis à l'échelle de la fenêtre et du recul
// —, puis l'élargit au plancher, en pixels entiers. Il en sort un CARRÉ du
// plancher, centré à peine au-dessus de la case. Rien pour un point qui passe
// derrière l'œil.
[[nodiscard]] std::optional<ScreenRect> GroundUnitScreenRect(
    const Camera& camera, float aspect_ratio, const glm::vec3& ground_point, float floor_pixels,
    float viewport_width, float viewport_height);

// Élargit un rectangle au plancher comme le client : la largeur tronquée à
// l'entier, et la moitié ENTIÈRE du manque de chaque côté.
void WidenToWholeFloor(ScreenRect& rect, float floor_pixels);

// L'emprise d'un acteur à l'écran : le rectangle de ses quads projetés,
// élargi à son plancher (`DrawnActor::pick_floor_pixels`), puis réduit autour
// de son centre à sa part (`DrawnActor::pick_share`). L'ombre n'en fait pas
// partie — on ne clique pas une ombre. Rien si l'acteur n'a aucun quad, ou
// s'il passe derrière l'œil.
[[nodiscard]] std::optional<ScreenRect> ActorScreenRect(const Camera& camera, float aspect_ratio,
                                                        const DrawnActor& drawn,
                                                        float viewport_width,
                                                        float viewport_height);

// L'acteur que désigne un pixel, par son rang dans `actors` ; rien si aucune
// emprise ne le contient.
//
// Quand plusieurs se recouvrent, gagne celui dont la distance à l'œil, plus
// son recul (DrawnActor::pick_bias), est la plus petite. Le recul est la marge
// que le client ajoute à la profondeur de chaque rectangle selon le genre de
// son acteur : c'est elle, bien plus que la distance, qui départage un PNJ
// d'un monstre et un monstre d'un joueur superposés.
//
// UN ACTEUR POSÉ AU SOL (DrawnActor::ground_pick_radius) se désigne par le
// disque qu'il occupe, pas par un rectangle : le pixel doit tomber DANS le
// disque projeté, qui suit l'inclinaison, la rotation et le recul de la
// caméra. Un disque projeté plus étroit que son plancher s'attrape
// par son rectangle élargi, comme tout petit acteur. Et il ne gagne que si
// AUCUN autre acteur ne contient le pixel : il est au sol, sous eux, et ne
// leur vole pas le clic.
//
// ⚠ LE CLIENT D'ORIGINE FAIT AUTREMENT, et moins bien : il donne à un warp un
// rectangle d'écran fixe, large de cinq personnages et plat, qui ne suit ni
// l'inclinaison ni ce que l'on voit, et qui prend le clic à qui se tient à
// côté.
[[nodiscard]] std::optional<std::size_t> PickActor(const Camera& camera, float aspect_ratio,
                                                   std::span<const DrawnActor> actors,
                                                   float screen_x, float screen_y,
                                                   float viewport_width, float viewport_height);

}  // namespace ro::render
