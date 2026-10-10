// Ce qu'une UNITÉ POSÉE AU SOL montre TANT QU'ELLE VIT.
//
// Ce n'est ni son modèle ni l'effet de sa pose : c'est l'effet que le client
// ENTRETIENT sous elle, tic après tic, et c'est la seule image que Firewall,
// Safety Wall ou Quagmire aient jamais eue.
//
// ⚠ DEUX FORMES. Un effet PERMANENT est créé une fois et s'entretient seul ;
// un effet RÉPÉTÉ se relance quand `durée − retrait` tics ont passé, le
// retrait étant le recouvrement qui empêche de voir le sol nu entre deux
// passes. Le tic fait seize millisecondes — voir kEffectTickMs.
//
// ⚠ LA TABLE N'EST PAS CRIBLÉE : elle dit ce que le CLIENT fait, et la
// plupart de ces effets sont procéduraux. Celui qui n'a ni fichier ni patron
// de nœud ne se peint pas encore, et c'est à l'appelant de le dire — une
// fois, pas à chaque image.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_skill_unit_visuals.py depuis le
// relevé scripts/client_skill_unit_visuals.json, pas écrit à la main.

#include <array>

#include "ro/render/skill_unit_table.h"

namespace ro::render {
namespace {

// Triée par identifiant d'unité : la recherche est dichotomique.
constexpr std::array<SkillUnitVisualEntry, 104> kSkillUnitVisuals = {{
    {126, 315, SkillUnitVisualShape::kRepeated, 7},
    {127, 25, SkillUnitVisualShape::kRepeated, 7},
    {128, 317, SkillUnitVisualShape::kRepeated, 8},
    {129, 316, SkillUnitVisualShape::kRepeated, 8},
    {130, 91, SkillUnitVisualShape::kRepeated, 8},
    {131, 319, SkillUnitVisualShape::kPermanent, 0},
    {132, 318, SkillUnitVisualShape::kPermanent, 0},
    {135, 138, SkillUnitVisualShape::kPermanent, 0},
    {136, 97, SkillUnitVisualShape::kRepeated, 8},
    {141, 74, SkillUnitVisualShape::kPermanent, 0},
    {142, 95, SkillUnitVisualShape::kRepeated, 20},
    {146, 171, SkillUnitVisualShape::kPermanent, 0},
    {154, 239, SkillUnitVisualShape::kPermanent, 0},
    {155, 240, SkillUnitVisualShape::kPermanent, 0},
    {156, 241, SkillUnitVisualShape::kPermanent, 0},
    {157, 242, SkillUnitVisualShape::kPermanent, 0},
    {158, 278, SkillUnitVisualShape::kPermanent, 0},
    {159, 279, SkillUnitVisualShape::kPermanent, 0},
    {160, 280, SkillUnitVisualShape::kPermanent, 0},
    {161, 281, SkillUnitVisualShape::kPermanent, 0},
    {162, 282, SkillUnitVisualShape::kPermanent, 0},
    {163, 283, SkillUnitVisualShape::kPermanent, 0},
    {164, 284, SkillUnitVisualShape::kPermanent, 0},
    {165, 285, SkillUnitVisualShape::kPermanent, 0},
    {166, 277, SkillUnitVisualShape::kPermanent, 0},
    {167, 286, SkillUnitVisualShape::kPermanent, 0},
    {168, 287, SkillUnitVisualShape::kPermanent, 0},
    {169, 288, SkillUnitVisualShape::kPermanent, 0},
    {170, 289, SkillUnitVisualShape::kPermanent, 0},
    {171, 290, SkillUnitVisualShape::kPermanent, 0},
    {172, 291, SkillUnitVisualShape::kPermanent, 0},
    {173, 292, SkillUnitVisualShape::kPermanent, 0},
    {174, 293, SkillUnitVisualShape::kPermanent, 0},
    {175, 294, SkillUnitVisualShape::kPermanent, 0},
    {177, 302, SkillUnitVisualShape::kPermanent, 0},
    {178, 341, SkillUnitVisualShape::kRepeated, 8},
    {179, 370, SkillUnitVisualShape::kPermanent, 0},
    {180, 374, SkillUnitVisualShape::kPermanent, 0},
    {182, 405, SkillUnitVisualShape::kPermanent, 0},
    {183, 404, SkillUnitVisualShape::kPermanent, 0},
    {184, 522, SkillUnitVisualShape::kPermanent, 0},
    {185, 517, SkillUnitVisualShape::kPermanent, 0},
    {187, 620, SkillUnitVisualShape::kPermanent, 0},
    {188, 631, SkillUnitVisualShape::kPermanent, 0},
    {189, 634, SkillUnitVisualShape::kRepeated, 1},
    {190, 638, SkillUnitVisualShape::kRepeated, 1},
    {191, 639, SkillUnitVisualShape::kRepeated, 1},
    {192, 640, SkillUnitVisualShape::kRepeated, 1},
    {193, 641, SkillUnitVisualShape::kRepeated, 1},
    {194, 642, SkillUnitVisualShape::kRepeated, 1},
    {195, 74, SkillUnitVisualShape::kPermanent, 0},
    {196, 74, SkillUnitVisualShape::kPermanent, 0},
    {197, 138, SkillUnitVisualShape::kPermanent, 0},
    {199, 1555, SkillUnitVisualShape::kRepeated, 0},
    {200, 671, SkillUnitVisualShape::kPermanent, 0},
    {201, 672, SkillUnitVisualShape::kPermanent, 0},
    {202, 1655, SkillUnitVisualShape::kRepeated, 0},
    {203, 142, SkillUnitVisualShape::kPermanent, 0},
    {204, 822, SkillUnitVisualShape::kPermanent, 0},
    {205, 825, SkillUnitVisualShape::kRepeated, 8},
    {206, 827, SkillUnitVisualShape::kRepeated, 8},
    {207, 828, SkillUnitVisualShape::kRepeated, 8},
    {208, 829, SkillUnitVisualShape::kRepeated, 8},
    {218, 856, SkillUnitVisualShape::kRepeated, 8},
    {219, 1276, SkillUnitVisualShape::kRepeated, 8},
    {220, 920, SkillUnitVisualShape::kRepeated, 7},
    {221, 926, SkillUnitVisualShape::kRepeated, 7},
    {222, 860, SkillUnitVisualShape::kRepeated, 8},
    {224, 1080, SkillUnitVisualShape::kRepeated, 0},
    {225, 772, SkillUnitVisualShape::kPermanent, 0},
    {226, 783, SkillUnitVisualShape::kPermanent, 0},
    {227, 784, SkillUnitVisualShape::kPermanent, 0},
    {228, 929, SkillUnitVisualShape::kRepeated, 1},
    {230, 912, SkillUnitVisualShape::kRepeated, 1},
    {231, 916, SkillUnitVisualShape::kRepeated, 1},
    {232, 917, SkillUnitVisualShape::kRepeated, 1},
    {233, 918, SkillUnitVisualShape::kRepeated, 1},
    {234, 919, SkillUnitVisualShape::kRepeated, 1},
    {235, 921, SkillUnitVisualShape::kRepeated, 1},
    {237, 25, SkillUnitVisualShape::kRepeated, 7},
    {238, 939, SkillUnitVisualShape::kPermanent, 0},
    {239, 465, SkillUnitVisualShape::kRepeated, 1},
    {240, 1088, SkillUnitVisualShape::kRepeated, 1},
    {241, 916, SkillUnitVisualShape::kRepeated, 1},
    {242, 916, SkillUnitVisualShape::kRepeated, 1},
    {243, 916, SkillUnitVisualShape::kRepeated, 1},
    {244, 916, SkillUnitVisualShape::kRepeated, 1},
    {245, 959, SkillUnitVisualShape::kRepeated, 20},
    {246, 964, SkillUnitVisualShape::kRepeated, 20},
    {247, 975, SkillUnitVisualShape::kRepeated, 20},
    {248, 992, SkillUnitVisualShape::kRepeated, 20},
    {249, 993, SkillUnitVisualShape::kRepeated, 20},
    {250, 994, SkillUnitVisualShape::kRepeated, 20},
    {251, 995, SkillUnitVisualShape::kRepeated, 20},
    {252, 1007, SkillUnitVisualShape::kRepeated, 20},
    {253, 1020, SkillUnitVisualShape::kRepeated, 20},
    {254, 1026, SkillUnitVisualShape::kRepeated, 1},
    {255, 25, SkillUnitVisualShape::kRepeated, 1},
    {256, 25, SkillUnitVisualShape::kRepeated, 1},
    {257, 1050, SkillUnitVisualShape::kRepeated, 1},
    {258, 1051, SkillUnitVisualShape::kRepeated, 1},
    {259, 1278, SkillUnitVisualShape::kRepeated, 0},
    {261, 1064, SkillUnitVisualShape::kRepeated, 1},
    {264, 1120, SkillUnitVisualShape::kRepeated, 1},
}};

// LA FORME AMORCÉE : l'effet dépend du NIVEAU de la compétence qui a posé
// l'unité, parce que le client le range à l'apparition dans un champ de
// l'acteur et que le tic l'y consomme. Triée par unité puis par niveau.
constexpr std::array<SkillUnitLevelVisualEntry, 5> kSkillUnitLevelVisuals = {{
    {268, 1, 1602},
    {268, 2, 1603},
    {268, 3, 1604},
    {268, 4, 1605},
    {268, 5, 1606},
}};

}  // namespace

std::span<const SkillUnitVisualEntry> SkillUnitVisualTable() { return kSkillUnitVisuals; }

std::span<const SkillUnitLevelVisualEntry> SkillUnitLevelVisualTable() {
  return kSkillUnitLevelVisuals;
}

}  // namespace ro::render
