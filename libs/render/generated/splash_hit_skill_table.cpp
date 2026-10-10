// LES COMPÉTENCES DONT L'IMPACT SE MONTRE AUX SORTES ORDINAIREMENT MUETTES —
// voir ro/render/skill_hit_effect.h.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_splash_hit_skills.py depuis le relevé
// scripts/client_skill_dispatch.json, pas écrit à la main.

#include <array>

#include "ro/render/skill_hit_effect.h"

namespace ro::render {
namespace {

constexpr std::array<SplashHitSkill, 20> kSplashHitSkills = {{
    {5215, 0x0020}, {5225, 0x0020}, {5226, 0x0020}, {5234, 0x0020}, {5235, 0x0020},
    {5236, 0x0020}, {5237, 0x0020}, {5241, 0x0020}, {5244, 0x0020}, {5245, 0x0020},
    {5294, 0x0020}, {5295, 0x0020}, {5296, 0x0020}, {5298, 0x0020}, {5314, 0x0020},
    {5319, 0x0020}, {5329, 0x0020}, {5383, 0x0020}, {5384, 0x0020}, {5385, 0x0020},
}};

}  // namespace

std::span<const SplashHitSkill> SplashHitSkillTable() { return kSplashHitSkills; }

}  // namespace ro::render
