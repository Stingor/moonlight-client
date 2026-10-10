// LES TAILLADES QUI GLISSENT : les noeuds des sous-types 86 et 92.
//
// 4 identifiants, DEUX batisseurs. Le premier VISE la cible au premier
// tic et son octet choisit ses images ; le second recoit son azimut du
// site et ne vise rien.
//
// Les images ne sont pas ici : les deux les ecrivent en dur, et le premier
// en charge QUATRE ou UNE selon son octet. Voir EffectGlidingSlashTexturesOf.
//
// REFUSE - l'identifiant 733 : EF_SOULBREAKER4 : le releve des noeuds lui prete un noeud du
// sous-type 86, mais son cas n'appelle jamais le batisseur - qui n'a que TROIS sites, aux cas 361,
// 720 et 1082.
//
// ENGENDRE par scripts/gen_client_effect_gliding_slashes.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_gliding_slash_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectGlidingSlashPattern, 4> kGlidingSlashes = {{
    {361, "Effect_Build_SonicWaveOrPurpleSlash", EffectGlidingSlashBuilder::kSonicWave, 0, 0, 1,
     0},  // EF_SOULBREAKER, purpleslash
    {409, "Effect_Build_PurpleSlash5c", EffectGlidingSlashBuilder::kPurpleSlash, 0, 0, 8,
     -45},  // EF_SOULBREAKER2, purpleslash
    {720, "Effect_Build_SonicWaveOrPurpleSlash", EffectGlidingSlashBuilder::kSonicWave, 1, 0, 1,
     0},  // EF_SOULBREAKER3, quatre sonic_wave
    {1082, "Effect_Build_SonicWaveOrPurpleSlash", EffectGlidingSlashBuilder::kSonicWave, 2, 0, 1,
     0},  // purpleslash
}};

}  // namespace

std::span<const EffectGlidingSlashPattern> EffectGlidingSlashPatternTable() {
  return kGlidingSlashes;
}

}  // namespace ro::render
