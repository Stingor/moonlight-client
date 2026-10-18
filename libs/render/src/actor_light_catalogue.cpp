// LES LUMIÈRES QUE PORTENT LES ACTEURS : une entrée par monstre, désigné par
// le nom de son sprite, et une par état. Voir docs/adr/0120.
//
// Les hauteurs se comptent au-dessus des pieds, en unités du monde (sept
// pixels de sprite chacune), et visent la source sur le sprite : la lampe du
// casque, la tête d'une citrouille, le cœur d'une flamme. Une couleur au-delà
// de un sature.

#include <array>

#include "ro/render/actor_lights.h"

namespace ro::render {
namespace {

// LES TEINTES, intensité comprise.
// Une lampe de mineur : blanc chaud, qui ne vacille pas.
constexpr glm::vec3 kMinerLamp{1.0F, 0.86F, 0.6F};
// Une bougie dans une citrouille.
constexpr glm::vec3 kPumpkinCandle{1.0F, 0.55F, 0.15F};
// Une flamme vive, et une braise de lave, plus rouge.
constexpr glm::vec3 kLivingFlame{0.95F, 0.5F, 0.18F};
constexpr glm::vec3 kLavaEmber{1.0F, 0.36F, 0.08F};
// Les feux de Sight, orangés ; la lumière sacrée de Ruwach, blanc bleuté.
constexpr glm::vec3 kSightFlames{1.0F, 0.6F, 0.25F};
constexpr glm::vec3 kRuwachGlow{0.65F, 0.8F, 1.0F};

// LES BITS D'OPTION, avec les valeurs de src/map/status.hpp.
constexpr std::uint32_t kOptionSight = 0x1;
constexpr std::uint32_t kOptionRuwach = 0x2000;

// ⚠ AUCUNE LAMPE DE MONSTRE NE PORTE OMBRE. Une galerie de Mjolnir compte des
// dizaines de mineurs, et une lumière qui marche repeint ses six faces à
// chaque image : elles prendraient le budget de faces des lanternes et des
// sorts. Sight et Ruwach, une lumière par lanceur, portent la leur.
constexpr std::array<SpriteLight, 10> kSpriteLights = {{
    // Le Skeleton Worker et sa lampe de casque, au front.
    {"SKEL_WORKER", {kMinerLamp, 45.0F, 13.0F, false, false}},
    // Jakk : une citrouille évidée, éclairée de l'intérieur.
    {"JAKK", {kPumpkinCandle, 35.0F, 10.0F, true, false}},
    {"JAKK_XMAS", {kPumpkinCandle, 35.0F, 10.0F, true, false}},
    // Les créatures de flamme.
    {"KASA", {kLivingFlame, 40.0F, 6.0F, true, false}},
    {"SALAMANDER", {kLivingFlame, 40.0F, 7.0F, true, false}},
    {"BLAZZER", {kLivingFlame, 35.0F, 6.0F, true, false}},
    {"IMP", {kLivingFlame, 30.0F, 5.0F, true, false}},
    {"IFRIT", {kLivingFlame, 70.0F, 14.0F, true, false}},
    // Les créatures de lave.
    {"MAGMARING", {kLavaEmber, 25.0F, 2.5F, true, false}},
    {"LAVA_GOLEM", {kLavaEmber, 45.0F, 9.0F, true, false}},
}};

constexpr std::array<OptionLight, 2> kOptionLights = {{
    // Les feux qui tournent autour du mage, à mi-corps.
    {kOptionSight, {kSightFlames, 45.0F, 7.0F, true, true}},
    // La sphère de Ruwach, au même endroit, sans flamme.
    {kOptionRuwach, {kRuwachGlow, 45.0F, 7.0F, false, true}},
}};

}  // namespace

std::span<const SpriteLight> SpriteLightCatalogue() { return kSpriteLights; }

std::span<const OptionLight> OptionLightCatalogue() { return kOptionLights; }

}  // namespace ro::render
