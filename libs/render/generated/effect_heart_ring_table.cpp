// LES ANNEAUX EN COEUR : les noeuds du sous-type 110.
//
// 1 identifiant, 20 noeuds : le batisseur est appele une fois par POINT du
// dessin, et chaque noeud porte DEUX anneaux.
//
// ENGENDRE par scripts/gen_client_effect_heart_rings.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_heart_ring_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectHeartRingPattern, 20> kHeartRings = {{
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", 0.0F,
     9.99F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", -3.51F,
     11.88F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", -7.83F,
     12.96F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", -12.42F,
     11.34F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", -15.12F,
     7.02F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", -15.39F,
     1.62F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", -13.77F,
     -3.24F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", -10.8F,
     -7.02F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", -6.75F,
     -10.26F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", -2.7F,
     -12.69F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", 0.0F,
     -16.2F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", 2.97F,
     -12.69F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", 7.02F,
     -10.26F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", 11.07F,
     -7.02F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", 14.04F,
     -3.24F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", 15.66F,
     1.62F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", 15.39F,
     7.02F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", 12.69F,
     11.34F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", 8.1F,
     12.96F},
    {343, "Effect_Build_TwinRingsAtPosFromParam", "data\\texture\\effect\\ring_red.tga", 3.78F,
     11.88F},
}};

}  // namespace

std::span<const EffectHeartRingPattern> EffectHeartRingPatternTable() { return kHeartRings; }

}  // namespace ro::render
