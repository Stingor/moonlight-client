// CE QU'UNE UNITÉ AU SOL FAIT À SON APPARITION — voir
// ro/render/skill_unit_pose.h.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_skill_unit_poses.py depuis le relevé
// scripts/client_skill_unit_dispatch.json, pas écrit à la main.

#include <array>

#include "ro/render/skill_unit_pose.h"

namespace ro::render {
namespace {

constexpr std::array<SkillUnitPoseStep, 54> kSkillUnitPoses = {{
    {133, SkillUnitPoseKind::kEffect, 141},
    {148, SkillUnitPoseKind::kEffect, 145, 0, 255, true},
    {216, SkillUnitPoseKind::kEffect, 145, 0, 255, true},
    {260, SkillUnitPoseKind::kEffect, 1069, 1, 1},
    {260, SkillUnitPoseKind::kEffect, 1070, 2, 3},
    {260, SkillUnitPoseKind::kEffect, 1071, 4, 5},
    {262, SkillUnitPoseKind::kEffect, 1104},
    {263, SkillUnitPoseKind::kEffect, 1125},
    {265, SkillUnitPoseKind::kEffect, 1200},
    {266, SkillUnitPoseKind::kEffect, 1752},
    {267, SkillUnitPoseKind::kEffect, 1772},
    {269, SkillUnitPoseKind::kEffect, 1893, 0, 255, false, 90.0F, true},
    {269, SkillUnitPoseKind::kEffect, 1894, 0, 255, false, 90.0F, true},
    {270, SkillUnitPoseKind::kRemoveEffect, 1793},
    {270, SkillUnitPoseKind::kEffect, 1793},
    {271, SkillUnitPoseKind::kRemoveEffect, 1794},
    {271, SkillUnitPoseKind::kEffect, 1794},
    {271, SkillUnitPoseKind::kRemoveEffect, 1795},
    {271, SkillUnitPoseKind::kEffect, 1795},
    {272, SkillUnitPoseKind::kEffect, 1776},
    {272, SkillUnitPoseKind::kEffect, 1777},
    {273, SkillUnitPoseKind::kRemoveEffect, 1804},
    {273, SkillUnitPoseKind::kEffect, 1804},
    {274, SkillUnitPoseKind::kEffect, 1768},
    {276, SkillUnitPoseKind::kEffect, 1957},
    {277, SkillUnitPoseKind::kEffect, 2130},
    {277, SkillUnitPoseKind::kEffect, 2129},
    {278, SkillUnitPoseKind::kEffect, 2216},
    {278, SkillUnitPoseKind::kEffect, 2217},
    {278, SkillUnitPoseKind::kEffect, 2218},
    {278, SkillUnitPoseKind::kEffect, 2219},
    {279, SkillUnitPoseKind::kEffect, 2232},
    {279, SkillUnitPoseKind::kEffect, 2233},
    {279, SkillUnitPoseKind::kEffect, 2234},
    {279, SkillUnitPoseKind::kEffect, 2235},
    {280, SkillUnitPoseKind::kEffect, 2224},
    {280, SkillUnitPoseKind::kEffect, 2225},
    {280, SkillUnitPoseKind::kEffect, 2226},
    {280, SkillUnitPoseKind::kEffect, 2227},
    {281, SkillUnitPoseKind::kEffect, 2208},
    {281, SkillUnitPoseKind::kEffect, 2209},
    {281, SkillUnitPoseKind::kEffect, 2210},
    {281, SkillUnitPoseKind::kEffect, 2211},
    {282, SkillUnitPoseKind::kEffect, 2042},
    {282, SkillUnitPoseKind::kEffect, 2041},
    {283, SkillUnitPoseKind::kEffect, 2031},
    {284, SkillUnitPoseKind::kEffect, 2056},
    {284, SkillUnitPoseKind::kEffect, 2055},
    {285, SkillUnitPoseKind::kEffect, 2307},
    {20852, SkillUnitPoseKind::kEffect, 1874},
    {20853, SkillUnitPoseKind::kEffect, 1882},
    {20854, SkillUnitPoseKind::kEffect, 1878},
    {20855, SkillUnitPoseKind::kEffect, 1919},
    {21292, SkillUnitPoseKind::kEffect, 2341},
}};

}  // namespace

std::span<const SkillUnitPoseStep> SkillUnitPoseTable() { return kSkillUnitPoses; }

}  // namespace ro::render
