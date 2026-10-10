// L'effet qu'un acteur porte TANT QU'UN ÉTAT dure.
//
// Ceux-là n'arrivent par aucun paquet : le client resème l'effet ENTIER sur
// l'acteur, tous les « durée déclarée » tics, à partir du seul bit d'option.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_option_effects.py depuis les
// relevés de scripts/, pas écrit à la main.

#include "ro/render/option_effect_table.h"

#include <array>

namespace ro::render {
namespace {

constexpr std::array<OptionEffect, 2> kOptionEffects = {{
    // OPTION_SIGHT, tant que le bit est posé — tous les 270 tics ;
    // son de l'allumage : au front montant du bit
    {0x1, 22, 270, "effect\\EF_Sight.wav"},
    // OPTION_RUWACH, tant que le bit est posé — tous les 200 tics ;
    // son de l'allumage : au front montant du bit
    {0x2000, 33, 200, "effect\\EF_Sight.wav"},
}};

}  // namespace

std::span<const OptionEffect> OptionEffectTable() { return kOptionEffects; }

}  // namespace ro::render
