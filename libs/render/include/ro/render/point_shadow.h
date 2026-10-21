#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/point_lights.h"

namespace ro::render {

// LES OMBRES DES LUMIÈRES PONCTUELLES, côté calcul : quelle carte chacune
// reçoit, et comment elle voit le monde. Les cartes elles-mêmes sont dans
// `PointLightStage`, la passe dans `DrawPointShadows`. Voir docs/adr/0123.
//
// Une lumière ombrée voit la scène par SIX FACES d'un cube centré sur elle,
// chacune un champ carré de 90° tourné vers un axe. Ses faces suivent la
// convention des textures cubiques d'OpenGL : la direction d'un point vu de la
// lumière désigne, lue telle quelle, le texel qui le couvre.

inline constexpr std::size_t kCubeFaceCount = 6;

// LES PALIERS DE FINESSE : le côté d'une face en texels, du plus fin au plus
// grossier. Chaque palier est un tableau de cubes à part, ses couches toutes
// de la même taille.
inline constexpr std::size_t kPointShadowTierCount = 3;
inline constexpr std::array<int, kPointShadowTierCount> kPointShadowTierTexels = {512, 256, 128};

// LE BUDGET MÉMOIRE : les cartes qu'un palier tient au plus à la fois. Une
// carte de 512 texels pèse 6,3 Mo, de 256 1,6 Mo, de 128 0,4 Mo : 101 Mo en
// tout, au plafond.
inline constexpr std::array<std::size_t, kPointShadowTierCount> kPointShadowTierMaxLayers = {8, 16,
                                                                                             64};

// LE PALIER QU'UNE LUMIÈRE DEMANDE, selon sa distance à l'œil en unités du
// monde : le plus fin en deçà de la première borne, le suivant en deçà de la
// seconde, le plus grossier au-delà.
inline constexpr std::array<float, kPointShadowTierCount - 1> kPointShadowTierReachWorld = {120.0F,
                                                                                            240.0F};

// L'HYSTÉRÉSIS : une lumière ne quitte son palier qu'une fois sortie de ses
// bornes de cette part en plus. Sans elle, une lumière posée sur la borne
// changerait de carte à chaque pas.
inline constexpr float kPointShadowTierMargin = 0.15F;

// LE BUDGET DE TEMPS : les faces peintes au plus par image, servies de la
// lumière la plus proche à la plus lointaine. Les autres attendent l'image
// suivante avec ce qu'elles portent déjà. Une face triée coûte quelques
// microsecondes sur gonryun (docs/adr/0123) : le budget tient en 2 ms.
inline constexpr std::size_t kPointShadowFacesPerFrame = 192;

// CE QUI S'ANIME DANS LE DÉCOR — un arbre qui ploie, un drapeau, un moulin —
// fait repeindre les faces qui le voient, mais une image sur tant au plus, et
// après toutes les faces où quelque chose d'autre a changé. Son ombre suit son
// mouvement à quinze images par seconde quand le jeu en tourne soixante.
inline constexpr std::uint64_t kPointShadowAnimatedPeriod = 4;

// LE DÉGAGEMENT d'une lumière, en unités du monde : le plan proche de chaque
// face. Ce qui se tient à moins de cet écart de la lumière, le long de l'axe
// d'une face, n'est pas peint dans sa carte et n'ombre donc rien ; ce qui s'y
// tient n'est pas ombré non plus. C'est le verre d'une lanterne, la vasque
// d'un brasero où l'on pose la flamme : ils entourent la lumière sans la
// boucher. Le plan lointain est la portée de la lumière, au-delà de laquelle
// elle n'éclaire rien.
inline constexpr float kPointShadowClearanceWorld = 10.0F;

// LE PLAN LOINTAIN d'une lumière de portée `range_world` : sa portée, jamais
// plus près que deux fois le dégagement, où la profondeur perdrait son sens.
[[nodiscard]] float PointShadowFar(float range_world);

// CE QU'UNE CARTE PORTE : la lumière pour laquelle elle a été peinte. Une
// lumière qui bouge ou change de portée est une autre lumière ; une flamme qui
// se balance reste la même, voir `PointLight::sway_world`.
struct PaintedPointLight {
  glm::vec3 world_position{0.0F};
  float range_world = 0.0F;

  friend bool operator==(const PaintedPointLight&, const PaintedPointLight&) = default;
};

// LA RÉPARTITION DES CARTES entre les lumières, d'une image à l'autre.
//
// UNE OMBRE NE SAUTE PAS D'UNE CARTE À L'AUTRE pour un pas de la caméra : une
// lumière garde la sienne tant qu'elle reste dans son palier, hystérésis
// comprise. Une carte qu'aucune lumière n'a prise à cette image garde ce
// qu'elle porte : la lumière qui revient la retrouve peinte. Une carte libre
// va à la lumière la plus proche qui n'en a pas ; un palier plein la renvoie
// au suivant, et le dernier plein la laisse sans ombre.
class PointShadowSlots {
 public:
  struct Assignment {
    PointShadowSlot slot;
    // Vrai quand la carte vient d'être donnée à cette lumière : elle porte
    // encore une autre lumière, ou rien, et doit être effacée puis peinte.
    bool fresh = false;
  };

  // Une entrée par lumière de `lights`, rangées de la plus proche de `eye` à
  // la plus lointaine : sa carte, ou rien.
  [[nodiscard]] std::vector<std::optional<Assignment>> Assign(std::span<const PointLight> lights,
                                                              glm::vec3 eye);

  // Les couches qu'emploie le palier `tier` : son tableau doit en tenir autant.
  [[nodiscard]] std::size_t layers_used(std::size_t tier) const { return tiers_.at(tier).size(); }

  // Oublie toutes les cartes : une autre carte du monde se charge.
  void Clear() { tiers_ = {}; }

 private:
  struct Slot {
    std::optional<PaintedPointLight> painted;
    // La dernière répartition qui l'a donnée, pour reprendre la plus ancienne.
    std::uint64_t last_round = 0;
  };

  std::array<std::vector<Slot>, kPointShadowTierCount> tiers_{};
  std::uint64_t round_ = 0;
};

// Le palier que demande une lumière à `distance_world` de l'œil, quand elle
// tient déjà une carte du palier `held` : elle le garde dans ses bornes élargies
// de `kPointShadowTierMargin`.
[[nodiscard]] std::size_t PointShadowTierOf(float distance_world,
                                            std::optional<std::size_t> held = std::nullopt);

// LES SIX VUES D'UNE LUMIÈRE posée en `light_position`, dans l'ordre des faces
// d'OpenGL : +X, −X, +Y, −Y, +Z, −Z. Chacune projette son quart de l'espace
// sur sa face entière, de `kPointShadowClearanceWorld` à `far_world`.
[[nodiscard]] std::array<glm::mat4, kCubeFaceCount> PointShadowFaceViewProjections(
    glm::vec3 light_position, float far_world);

// L'axe que regarde la face `face`, dans le même ordre.
[[nodiscard]] glm::vec3 CubeFaceForward(std::size_t face);

// LA PROFONDEUR QU'UNE FACE ÉCRIT pour un point dont l'écart à la lumière, le
// long de l'axe de cette face, vaut `axis_distance_world` : de zéro au plan
// proche à un au plan lointain. Les nuanceurs la recalculent pour comparer —
// voir `kPointLightGlsl`.
[[nodiscard]] float PointShadowDepth(float axis_distance_world, float far_world);

}  // namespace ro::render
