// LA POSE qu'une compétence impose à son lanceur, à l'annonce ou à l'impact — voir
// ro/render/skill_held_pose.h.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_skill_held_poses.py depuis le relevé
// scripts/client_skill_dispatch.json, pas écrit à la main.

#include <array>

#include "ro/render/skill_held_pose.h"

namespace ro::render {
namespace {

constexpr std::array<SkillHeldPose, 48> kSkillHeldPoses = {{
    {263, 11, 0, 7, 11, 7, true, 0, false, 4, false, 0x2350, true},
    {266, 12, 3, 4, 12, 4, true, 1100, false, 10, false, 0x2350, false},
    {267, 12, 3, 4, 12, 4, true, 1100, false, 10, false, 0x2350, false},
    {268, 12, 0, 0, 12, 0, true, 20, false, 10, true, 0x0000, false},
    {269, 12, 3, 3, 12, 3, true, 3000, false, 10, true, 0x0000, false},
    {270, 12, 1, 2, 12, 2, true, 1000, false, 30, false, 0x0000, false},
    {271, 11, 0, 7, 11, 7, true, 3500, false, 4, false, 0x2350, false},
    {272, 12, 5, 5, 12, 5, true, 0, false, 10, true, 0x2350, true},
    {273, 12, 1, 2, 12, 2, true, 0, false, 6, false, 0x2350, true},
    {318, 12, 4, 5, 12, 4, true, 1800, true, 10, false, 0x0000, false},
    {326, 12, 0, 0, 12, 0, true, 1200, false, 10, true, 0x0000, false},
    {355, 4, 0, 5, 4, 5, true, 1000, false, 10, true, 0x0000, false},
    {364, 11, 0, 3, 11, 3, true, 1000, false, 10, true, 0x0000, false},
    {370, 12, 3, 4, 12, 4, true, 1, false, 10, true, 0x2350, false},
    {371, 12, 4, 4, 12, 4, true, 0, false, 6, false, 0x2350, true},
    {1010, 12, 4, 5, 12, 4, true, 1800, true, 10, true, 0x0000, false},
    {2326, 12, 4, 4, 12, 4, true, 0, false, 6, false, 0x2350, true},
    {2327, 12, 5, 5, 12, 5, true, 0, false, 6, false, 0x2350, true},
    {2328, 12, 1, 2, 12, 2, true, 0, false, 6, false, 0x2350, true},
    {2329, 12, 3, 3, 12, 3, true, 0, false, 6, false, 0x2350, true},
    {2330, 12, 5, 5, 12, 5, true, 0, false, 6, false, 0x2350, true},
    {2334, 12, 3, 3, 12, 3, true, 3000, false, 10, true, 0x0000, false},
    {2518, 12, 3, 4, 12, 4, true, 1100, false, 10, false, 0x2350, false},
    {2564, 11, 0, 7, 11, 7, true, 3500, false, 4, false, 0x2350, false},
    {5214, 12, 1, 1, 12, 1, true, 500, false, 10, true, 0x0000, false},
    {5214, 12, 1, 3, 12, 3, true, 500, false, 10, true, 0x2350, false},
    {5216, 12, 1, 3, 12, 3, true, 500, false, 10, true, 0x0000, false},
    {5220, 12, 1, 1, 12, 1, true, 500, false, 10, true, 0x0000, false},
    {5220, 12, 1, 3, 12, 3, true, 500, false, 10, true, 0x2350, false},
    {5230, 12, 1, 3, 12, 3, true, 500, false, 10, true, 0x0000, false},
    {5230, 12, 1, 3, 12, 3, true, 500, false, 10, true, 0x2350, false},
    {5237, 12, 1, 3, 12, 3, true, 500, false, 10, true, 0x0000, false},
    {5238, 12, 1, 1, 12, 1, true, 500, false, 10, true, 0x0000, false},
    {5239, 12, 1, 1, 12, 1, true, 500, false, 10, true, 0x0000, false},
    {5242, 12, 1, 1, 12, 1, true, 500, false, 10, true, 0x0000, false},
    {5243, 12, 1, 2, 12, 2, true, 1100, false, 5, false, 0x0000, false},
    {5244, 12, 1, 2, 12, 2, true, 1100, false, 5, false, 0x0000, false},
    {5247, 12, 5, 5, 12, 5, true, 500, false, 10, true, 0x0000, false},
    {5251, 12, 2, 2, 12, 2, true, 500, false, 4, false, 0x2350, false},
    {5252, 12, 2, 2, 12, 2, true, 500, false, 4, false, 0x2350, false},
    {5253, 12, 2, 2, 12, 2, true, 500, false, 4, false, 0x2350, false},
    {5254, 12, 2, 2, 12, 2, true, 500, false, 10, true, 0x0000, false},
    {5317, 12, 0, 0, 12, 0, true, 700, false, 10, true, 0x0000, false},
    {5350, 12, 1, 3, 12, 3, true, 500, false, 10, true, 0x0000, false},
    {5350, 12, 1, 3, 12, 3, true, 500, false, 10, true, 0x2350, false},
    {5357, 12, 1, 3, 12, 3, true, 500, false, 10, true, 0x2350, false},
    {5361, 12, 1, 3, 12, 3, true, 500, false, 10, true, 0x0000, false},
    {5363, 12, 1, 3, 12, 3, true, 500, false, 10, true, 0x0000, false},
}};

}  // namespace

std::span<const SkillHeldPose> SkillHeldPoseTable() { return kSkillHeldPoses; }

}  // namespace ro::render
