#pragma once

#include <array>
#include <optional>

#include <glm/glm.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/render/camera.h"

namespace ro::render {

// Retrouver le point du terrain que désigne un pixel de l'écran.
//
// POURQUOI CELA VIT DANS render
//
// Le calcul est l'INVERSE exact de la projection : il défait la matrice que la
// caméra a construite, puis interroge le même terrain que le maillage. Le
// mettre ailleurs le laisserait dériver de la caméra qu'il inverse, et le clic
// se mettrait à viser à côté sans que rien ne le signale.
//
// Il ne connaît ni le réseau ni le monde : il rend un point, pas une intention.

// Le rayon qui part de l'œil et traverse un pixel.
struct ScreenRay {
  glm::vec3 origin{0.0F, 0.0F, 0.0F};
  glm::vec3 direction{0.0F, 0.0F, -1.0F};
};

// Le rayon qui traverse le pixel donné.
//
// Les coordonnées sont celles de la fenêtre : l'origine en HAUT à gauche, l'axe
// des Y vers le bas — la convention de SDL, pas celle d'OpenGL. La conversion
// est faite ici, une fois, plutôt que chez chaque appelant.
[[nodiscard]] ScreenRay MakeScreenRay(const Camera& camera, float aspect_ratio, float screen_x,
                                      float screen_y, float viewport_width, float viewport_height);

// Pas d'avance du rayon lors de la recherche, en unités du monde.
//
// Une cellule en vaut cinq : un pas plus court que la moitié d'une cellule ne
// peut pas enjamber un relief sans le voir. Le raffinement qui suit rattrape le
// reste, donc l'allonger ne gagnerait que du temps sur un clic.
inline constexpr float kGroundPickStep = 2.0F;

// Nombre de bissections après la première rencontre.
//
// Vingt divisions ramènent un pas de deux unités sous le millionième d'unité :
// bien en deçà de ce qu'une cellule de cinq unités peut distinguer.
inline constexpr int kGroundPickRefinements = 20;

// Le point du terrain que vise un pixel, ou rien s'il ne vise pas la carte.
//
// La recherche marche le long du rayon jusqu'à passer SOUS le terrain, puis
// resserre. Une solution analytique n'existe pas : le sol est un champ de
// hauteurs, pas un plan, et le prendre pour un plan ferait viser à côté dès
// qu'il y a du relief — d'autant plus loin que la caméra est rasante.
//
// Rend nullopt quand le rayon sort de la carte sans l'avoir touchée. Hors des
// bords, GroundHeightAt prolonge la hauteur du cube le plus proche : sans ce
// contrôle, un clic sur le ciel désignerait une case au-delà de la carte.
//
// ⚠ `gat` donne le sol PRATICABLE, celui sur lequel on marche, et c'est lui
// qu'il faut viser : sans lui, un clic sur un pont désigne la case du sol qui
// passe DESSOUS, et le personnage part ailleurs. Nul quand la carte n'a pas de
// grille — on vise alors le terrain visible, faute de mieux.
[[nodiscard]] std::optional<std::array<float, 3>> PickGround(const formats::GndMap& gnd,
                                                             const formats::GatMap* gat,
                                                             const Camera& camera,
                                                             float aspect_ratio, float screen_x,
                                                             float screen_y, float viewport_width,
                                                             float viewport_height);

}  // namespace ro::render
