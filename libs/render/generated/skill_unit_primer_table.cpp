// L'AMORCE D'UNE UNITÉ AU SOL — voir ro/render/skill_unit_primer.h.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_skill_unit_primers.py depuis le
// relevé scripts/client_skill_unit_dispatch.json, pas écrit à la main.

#include <array>

#include "ro/render/skill_unit_primer.h"

namespace ro::render {
namespace {

constexpr std::array<SkillUnitPrimerEntry, 25> kSkillUnitPrimers = {{
    {143, 0, 255, 106},  {149, 0, 255, 139}, {150, 0, 255, 99},  {151, 0, 255, 108},
    {152, 0, 255, 107},  {190, 0, 255, 107}, {191, 0, 255, 99},  {192, 0, 255, 139},
    {193, 0, 255, 108},  {194, 0, 255, 99},  {210, 0, 255, 139}, {211, 0, 255, 139},
    {212, 0, 255, 139},  {213, 0, 255, 139}, {214, 0, 255, 107}, {215, 0, 255, 108},
    {216, 0, 255, 107},  {217, 0, 255, 107}, {218, 0, 255, 464}, {222, 0, 255, 464},
    {224, 0, 255, 1080}, {228, 0, 255, 106}, {230, 0, 255, 106}, {264, 0, 255, 99},
    {268, 0, 255, 1312},
}};

}  // namespace

std::span<const SkillUnitPrimerEntry> SkillUnitPrimerTable() { return kSkillUnitPrimers; }

}  // namespace ro::render
