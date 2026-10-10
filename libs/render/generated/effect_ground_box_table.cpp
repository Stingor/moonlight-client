// LES BOITES AU SOL : les noeuds du sous-type 64.
//
// 4 identifiants, UN SEUL batisseur, et l'octet du site porte tout :
// la hauteur, l'alpha, la duree, la taille, la teinte et le melange.
//
// ENGENDRE par scripts/gen_client_effect_ground_boxes.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_ground_box_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectGroundBoxPattern, 4> kGroundBoxes = {{
    {318, "Effect_Build_AdditiveVariantNode64", "data\\texture\\effect\\ring_red.tga",
     0},  // EF_BOTTOM_MAG, blanche, large, cle 50, melange par l'alpha
    {319, "Effect_Build_AdditiveVariantNode64", "data\\texture\\effect\\alpha_down.tga",
     1},  // EF_BOTTOM_SANC, vert pale, etroite, cle 16, additive
    {405, "Effect_Build_AdditiveVariantNode64", "data\\texture\\effect\\ring_white.tga",
     2},  // EF_BOTTOM_FOGWALL, sombre, etroite, cle 32, additive - et DEUX diagonales de plus
    {1088, "Effect_Build_AdditiveVariantNode64", "data\\texture\\effect\\ring_yellow.tga",
     4},  // sans constante, jaune, etroite, cle 16, additive - et son bleu PULSE
}};

}  // namespace

std::span<const EffectGroundBoxPattern> EffectGroundBoxPatternTable() { return kGroundBoxes; }

}  // namespace ro::render
