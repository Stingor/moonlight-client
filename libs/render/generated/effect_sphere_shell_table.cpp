// LES COQUILLES SPHERIQUES : les noeuds du sous-type 61.
//
// 7 identifiants et DEUX batisseurs freres : celui de Kyrie, dont le site passe
// un octet que le batisseur FILTRE - seuls deux, trois, quatre et cinq
// passent, le reste retombe sur zero - et son frere, qui ecrit sa marque
// en dur a UN - sa coquille tourne - parce que son site ne lui passe rien.
//
// CHAQUE RANGEE PORTE SA SECONDE IMAGE ET SA CLE DE MONTEE : les deux
// batisseurs en different, et les tenir pour constantes de fichier
// donnait au bouclier de Kyrie l'image de l'autre.
//
// ENGENDRE par scripts/gen_client_effect_sphere_shells.py - ne pas
// modifier a la main.

#include <array>

#include "ro/render/effect_sphere_shell_seed.h"

namespace ro::render {
namespace {

constexpr std::array<EffectSphereShellPattern, 7> kSphereShells = {{
    {336, "Effect_Build_KyrieGuard", "data\\texture\\effect\\guardK.tga", 0,
     "data\\texture\\effect\\guardK2.tga", 7.8F},  // EF_GUARD
    {496, "EffectUpdate_BlackMaximizePowerSword", "data\\texture\\effect\\a01.bmp", 1,
     "data\\texture\\effect\\a01.bmp", 10.0F},  // EF_GUARD2
    {675, "Effect_Build_KyrieGuard", "data\\texture\\effect\\guardK.tga", 2,
     "data\\texture\\effect\\guardK2.tga", 7.8F},  // EF_GUARD3
    {782, "EffectUpdate_BlackMaximizePowerSword", "data\\texture\\effect\\성체강복.tga", 1,
     "data\\texture\\effect\\a01.bmp", 10.0F},  // EF_GUARD4
    {1032, "Effect_Build_KyrieGuard", "data\\texture\\effect\\guardK.tga", 3,
     "data\\texture\\effect\\guardK2.tga", 7.8F},
    {1149, "Effect_Build_KyrieGuard", "data\\texture\\effect\\guardK.tga", 4,
     "data\\texture\\effect\\guardK2.tga", 7.8F},
    {1600, "Effect_Build_KyrieGuard", "data\\texture\\effect\\guardK.tga", 5,
     "data\\texture\\effect\\guardK2.tga", 7.8F},
}};

}  // namespace

std::span<const EffectSphereShellPattern> EffectSphereShellPatternTable() { return kSphereShells; }

}  // namespace ro::render
