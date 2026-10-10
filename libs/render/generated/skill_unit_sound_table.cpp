// LE BRUIT d'une unité posée au sol, en DEUX MOMENTS.
//
// À la POSE, le client joue un son une fois — c'est tout ce que Firewall fait
// entendre à l'apparition. À l'ENTRETIEN, il en rejoue un chaque fois qu'il
// resème son effet : le son suit alors la cadence de l'image.
//
// ⚠ CE NE SONT PAS LES SONS DES EFFETS. La table des sons d'effet n'en couvre
// aucun : le client les joue depuis le cas de l'UNITÉ, et non depuis la
// fonction de son effet.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_skill_unit_sounds.py depuis le
// relevé scripts/client_skill_unit_visuals.json, pas écrit à la main.

#include "ro/render/skill_unit_sound_table.h"

#include <array>

namespace ro::render {
namespace {

// POSE — triée par identifiant d'unité : la recherche est dichotomique.
constexpr std::array<SkillUnitSoundEntry, 7> kPlacementSounds = {{
    {127, "effect\\EF_FireWall.wav", 1},
    {189, "effect\\화염진.wav", 8},
    {237, "effect\\EF_FireWall.wav", 1},
    {257, "effect\\NC_MAGMA_ERUPTION.wav", 1},
    {258, "effect\\lg_kings_grace.wav", 1},
    {263, "effect\\su_nyanggrass.wav", 1},
    {265, "effect\\g카트토네이도.wav", 1},
}};

// ENTRETIEN — triée par identifiant d'unité : la recherche est dichotomique.
constexpr std::array<SkillUnitSoundEntry, 13> kSustainSounds = {{
    {218, "effect\\REVERBERATION.wav", 1},
    {219, "effect\\rainstorm.wav", 1},
    {222, "effect\\POEMOFNETHERWORLD.wav", 1},
    {228, "effect\\S워머.wav", 1},
    {231, "effect\\EF_FireWall.wav", 1},
    {232, "effect\\EF_FireWall.wav", 1},
    {233, "effect\\EF_FireWall.wav", 1},
    {234, "effect\\G가시나무덫.wav", 1},
    {235, "effect\\S스펠피스트.wav", 1},
    {241, "effect\\EF_FireWall.wav", 1},
    {242, "effect\\EF_FireWall.wav", 1},
    {243, "effect\\EF_FireWall.wav", 1},
    {244, "effect\\EF_FireWall.wav", 1},
}};

}  // namespace

std::span<const SkillUnitSoundEntry> SkillUnitPlacementSoundTable() { return kPlacementSounds; }

std::span<const SkillUnitSoundEntry> SkillUnitSustainSoundTable() { return kSustainSounds; }

}  // namespace ro::render
