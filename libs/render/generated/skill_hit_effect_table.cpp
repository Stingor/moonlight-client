// L'EFFET DE FRAPPE d'une compétence : ce que la CIBLE pose sur elle à chaque
// message de dégâts, en plus de l'effet de la compétence.
//
// C'est un champ du message que reçoit la cible, qui le pose en le jouant.
//
// 38 écart(s) et 2 entrée(s) qui dépendent du coup ; tout le reste suit la
// règle par défaut, dont les bornes sont ci-dessous.
//
// 🔴 ET LE MÊME MESSAGE PORTE UN SECOND EFFET, lu dans une autre table par
// compétence : 205 compétences en posent un, et il s'AJOUTE au premier au lieu
// de le remplacer.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_skill_hit_effects.py depuis le
// relevé scripts/client_skill_hit_effects.json, pas écrit à la main.

#include <array>

#include "ro/render/skill_hit_effect.h"

namespace ro::render {
namespace {

// Triée par identifiant : la recherche est dichotomique.
constexpr std::array<SkillHitEffectEntry, 38> kHitEffects = {{
    {7, 49},      {14, 51},     {15, 51},     {17, 49},    {19, 49},     {20, 52},     {56, -1},
    {57, -1},     {58, -1},     {59, -1},     {90, 147},   {91, 147},    {129, -1},    {184, -1},
    {185, -1},    {186, -1},    {187, -1},    {188, -1},   {189, -1},    {190, 180},   {347, -1},
    {397, -1},    {467, 549},   {468, 549},   {770, 147},  {2022, 1567}, {2202, 1591}, {2317, 1689},
    {2414, 1593}, {2446, 1623}, {2519, 1689}, {2600, 180}, {2602, 180},  {5243, 2095}, {5274, 1826},
    {5320, 2132}, {5322, 2115}, {8202, 49},
}};

constexpr std::array<SkillHitEffectWhen, 2> kConditions = {{
    {54, SkillHitWhen::kAnyDamage, 152, 0},
    {2323, SkillHitWhen::kAnotherCaster, 1708, -1},
}};

// LE SECOND EFFET du même message, lu dans sa propre table. Triée par
// identifiant. ⚠ Les identifiants ABSENTS et ceux que la table met à −1 se
// valent : le client rend −1 dans les deux cas, et les garder doublerait la
// table pour rien.
constexpr std::array<SkillHitEffectEntry, 205> kDamageMsgEffects = {{
    {0, 0},       {1, 1},       {2, 1},       {3, 1},       {4, 1},       {5, 1},
    {6, 1},       {7, 17},      {8, 1},       {9, 1},       {10, 1},      {11, 1},
    {13, 1},      {15, 28},     {16, 23},     {17, 1},      {22, 1},      {23, 1},
    {24, 1},      {25, 1},      {26, 1},      {27, 1},      {28, 1},      {29, 1},
    {30, 1},      {31, 1},      {32, 1},      {33, 1},      {34, 1},      {35, 1},
    {36, 1},      {37, 1},      {38, 1},      {39, 1},      {40, 1},      {41, 1},
    {42, 10},     {43, 1},      {44, 1},      {45, 1},      {46, 1},      {47, 1},
    {48, 1},      {49, 1},      {50, 1},      {51, 1},      {52, 1},      {53, 1},
    {54, 140},    {56, 81},     {58, 81},     {59, 3},      {62, 73},     {66, 84},
    {67, 88},     {68, 152},    {74, 76},     {75, 75},     {76, 87},     {77, 152},
    {78, 85},     {79, 152},    {80, 49},     {81, 49},     {83, 49},     {84, 94},
    {85, 52},     {88, 28},     {89, 51},     {90, 147},    {91, 147},    {92, 147},
    {93, 60},     {115, 73},    {129, 115},   {135, 1},     {136, 122},   {137, 132},
    {140, 124},   {153, 170},   {156, 152},   {159, 181},   {171, 186},   {172, 191},
    {176, 125},   {178, 193},   {179, 194},   {180, 195},   {181, 196},   {182, 197},
    {184, 51},    {185, 147},   {186, 49},    {187, 52},    {188, 192},   {189, 152},
    {190, 184},   {191, 198},   {192, 182},   {199, 216},   {200, 217},   {202, 212},
    {267, 1},     {272, 122},   {273, 1},     {341, 452},   {371, 1},     {372, 1},
    {381, 115},   {382, 1},     {394, 1},     {397, 81},    {499, 1},     {534, 49},
    {537, 51},    {654, 49},    {689, 1},     {690, 1},     {720, 51},    {722, 1052},
    {723, 51},    {736, 182},   {742, 1},     {780, 2308},  {783, 2304},  {1001, 1},
    {1009, 1},    {1016, 2},    {2005, 122},  {2022, 122},  {2031, 1},    {2033, 1},
    {2203, 51},   {2204, 51},   {2210, 216},  {2304, 183},  {2329, 1},    {2447, 51},
    {2448, 1},    {2449, 182},  {2518, 1},    {5214, 1740}, {5215, 1737}, {5220, 1744},
    {5225, 1858}, {5226, 1858}, {5233, 1749}, {5234, 1737}, {5235, 1759}, {5236, 1759},
    {5237, 1784}, {5241, 1990}, {5244, 1997}, {5245, 2053}, {5249, 2080}, {5250, 2082},
    {5277, 1848}, {5287, 1838}, {5289, 1914}, {5294, 1866}, {5295, 1939}, {5296, 1886},
    {5298, 1842}, {5307, 2215}, {5308, 2231}, {5309, 2223}, {5310, 2207}, {5314, 1967},
    {5315, 2156}, {5316, 2017}, {5319, 2093}, {5320, 1967}, {5321, 2131}, {5329, 1970},
    {5340, 2215}, {5341, 2231}, {5342, 2223}, {5343, 2207}, {5357, 2063}, {5369, 2030},
    {5370, 2045}, {5371, 2034}, {5372, 2059}, {5373, 2137}, {5382, 2016}, {5383, 2111},
    {5384, 2253}, {5385, 2251}, {5389, 2141}, {5390, 2150}, {5391, 2153}, {5392, 2144},
    {5393, 2147}, {8044, 2342}, {8047, 2330}, {8048, 2328}, {8050, 2325}, {8054, 2325},
    {8056, 2336}, {8202, 17},   {8445, 2039}, {8448, 2023}, {8451, 2102}, {8454, 2107},
    {8457, 2012},
}};

}  // namespace

std::span<const SkillHitEffectEntry> SkillHitEffectTable() { return kHitEffects; }

std::span<const SkillHitEffectEntry> SkillDamageMsgEffectTable() { return kDamageMsgEffects; }

std::span<const SkillHitEffectWhen> SkillHitEffectConditions() { return kConditions; }

SkillHitEffectDefault SkillHitEffectDefaults() { return {411, 472, 493, 0}; }

}  // namespace ro::render
