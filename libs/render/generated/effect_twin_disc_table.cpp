// LES DEUX DISQUES : les noeuds du sous-type 28.
//
// 5 identifiants et 6 lignes : l'un des cinq appelle son batisseur
// DEUX FOIS, sur deux images differentes.
//
// Colonnes : style, rayon, alpha, mot de rendu, octet du site.
//
// ENGENDRE par scripts/gen_client_effect_twin_discs.py - ne pas
// modifier a la main.

#include <array>
#include <cstdint>
#include <optional>

#include "ro/render/effect_twin_disc_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectTwinDiscPattern, 6> kTwinDiscs = {{
    {201, "Effect_Build_TwinDiscFromParam1c", "data\\texture\\effect\\pikapika2.bmp", 4, 15.0F, 200,
     13U, std::uint8_t{0}},
    // octet 0 : style 4, rayon 15
    {398, "Effect_Build_TwinDiscFromParam1c", "data\\texture\\effect\\pikapika2.bmp", 4, 15.0F, 200,
     13U, std::uint8_t{0}},
    // octet 0 : style 4, rayon 15
    {650, "Effect_Build_TwinDisc1cFromParam", "data\\texture\\effect\\pikapika2.bmp", 4, 46.0F, 25,
     5U, std::nullopt},
    // un seul argument : style 4, rayon 46
    {680, "Effect_Build_TwinDiscFromParam1c", "data\\texture\\effect\\pikapika2.bmp", 15, 15.0F,
     200, 13U, std::uint8_t{1}},
    // octet 1 : style 15, rayon 15
    {881, "Effect_Build_TwinDiscFromParam1c", "data\\texture\\effect\\emp shock.tga", 17, 15.0F,
     200, 13U, std::uint8_t{2}},
    // octet 2 : style 17, rayon 15
    {881, "Effect_Build_TwinDiscFromParam1c", "data\\texture\\effect\\cir0002.bmp", 18, 24.0F, 200,
     13U, std::uint8_t{3}},
    // octet 3 : style 18, rayon 24
}};

}  // namespace

std::span<const EffectTwinDiscPattern> EffectTwinDiscPatternTable() { return kTwinDiscs; }

}  // namespace ro::render
