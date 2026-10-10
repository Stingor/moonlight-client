// CE QU'UN ÉTAT MONTRE, selon le chemin par lequel le client le montre.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_state_effects.py depuis le relevé
// scripts/client_state_effects.json, pas écrit à la main.
//
// À LA POSE (et à la levée) d'un état, par le message 110 de l'acteur :
//   167 états, 191 couples — 2 chute, 168 pose, 21 toujours.
// AU PAQUET (ce que les deux traitements du changement d'état créent
//   eux-mêmes avant de poster ce message) : 11 couples.
// AU REBÂTISSAGE (le sprite du JOUEUR est refait et tous ses états sont
//   reposés d'un coup) :
//   104 états, 118 couples.
// CE QUI DISPARAÎT à la levée d'un état : 170 couples.
// CE QUI VIT JUSQU'À CE QU'ON LE RETIRE — les AURAS : 43 couples à la pose,
//   39 au rebâtissage, 32 effets distincts.
// LES SONS d'un changement d'état : 4.
// CE QUI SE CALCULE SUR LA VALEUR de l'état : 2 états.
//   152 — EFST_DEVIL1, la vue qui s'assombrit : `valeur + 473`, valeur de 2 à 11
//     toute la famille vit jusqu'au retrait
//   339 — EFST_ROLLINGCUTTER, un anneau par tour : `valeur + 1299`, valeur de 1 à 10, et `valeur +
//   1466` si la bascule des effets minimaux est levée
//     chaque rang s'éteint seul
//
// 🔴 LES DEUX GRANDS CHEMINS SE JUGENT L'UN L'AUTRE : 96 états sont vus par
// les deux, et 90 d'entre eux portent exactement le même jeu d'effets. Deux
// chemins indépendants du client qui tombent d'accord, c'est la meilleure
// preuve qu'on puisse avoir sans lancer le jeu.
//
// ⚠ CE QUE CES TABLES NE PORTENT PAS : 6 cas calculent leur identifiant à
// partir de la VALEUR de l'état, que notre modèle ne porte pas encore.
// Les valeurs sont pourtant SUR LE FIL dès le paquet 0x043F (`val1`, `val2`,
// `val3`) : c'est notre `Actor` qui les jette, pas le serveur qui les tait.
//   pose — états [339] — `valeur + 1466`
//   pose — états [339] — `valeur + 1299`
//   pose — états [152] — `valeur + 473`
//   paquet — états [339] — `valeur + 1299`
//   rebâtissage — états [339] — `valeur + 1299`
//   rebâtissage — états [152] — une formule sur la valeur
#include <algorithm>
#include <array>

#include "ro/render/state_effect.h"

namespace ro::render {
namespace {

// Triées par état, puis par effet : la recherche rend une tranche.
constexpr std::array<StateEffectEntry, 191> kStateEffects = {{
    {2, 166, StateEffectMoment::kOnApply, false},
    {25, 166, StateEffectMoment::kOnApply, false},
    {31, 166, StateEffectMoment::kOnApply, false},
    {68, 166, StateEffectMoment::kOnApply, false},
    {70, 865, StateEffectMoment::kOnApply, false},
    {71, 873, StateEffectMoment::kOnApply, false},
    {72, 877, StateEffectMoment::kOnApply, false},
    {73, 861, StateEffectMoment::kOnApply, false},
    {74, 865, StateEffectMoment::kOnApply, false},
    {75, 38, StateEffectMoment::kOnApply, false},
    {76, 871, StateEffectMoment::kOnApply, false},
    {77, 873, StateEffectMoment::kOnApply, false},
    {78, 867, StateEffectMoment::kOnApply, false},
    {79, 864, StateEffectMoment::kOnApply, false},
    {80, 871, StateEffectMoment::kOnApply, false},
    {81, 875, StateEffectMoment::kOnApply, false},
    {82, 864, StateEffectMoment::kOnApply, false},
    {83, 865, StateEffectMoment::kOnApply, false},
    {84, 875, StateEffectMoment::kOnApply, false},
    {86, 166, StateEffectMoment::kOnApply, false},
    {86, 203, StateEffectMoment::kOnApply, false},
    {87, 254, StateEffectMoment::kAlways, true},
    {87, 1132, StateEffectMoment::kAlways, true},
    {97, 655, StateEffectMoment::kOnApply, true},
    {97, 656, StateEffectMoment::kOnRelease, false},
    {103, 367, StateEffectMoment::kOnApply, false},
    {103, 474, StateEffectMoment::kOnApply, true},
    {105, 369, StateEffectMoment::kOnApply, false},
    {107, 368, StateEffectMoment::kOnApply, false},
    {110, 375, StateEffectMoment::kOnApply, false},
    {113, 382, StateEffectMoment::kOnApply, true},
    {121, 396, StateEffectMoment::kOnApply, true},
    {122, 396, StateEffectMoment::kOnApply, true},
    {123, 394, StateEffectMoment::kOnApply, false},
    {152, 164, StateEffectMoment::kAlways, true},
    {152, 487, StateEffectMoment::kAlways, false},
    {153, 419, StateEffectMoment::kOnApply, true},
    {159, 546, StateEffectMoment::kOnApply, false},
    {161, 166, StateEffectMoment::kOnApply, false},
    {165, 418, StateEffectMoment::kAlways, true},
    {165, 544, StateEffectMoment::kAlways, true},
    {166, 418, StateEffectMoment::kAlways, true},
    {166, 544, StateEffectMoment::kAlways, true},
    {167, 418, StateEffectMoment::kAlways, true},
    {167, 544, StateEffectMoment::kAlways, true},
    {188, 166, StateEffectMoment::kOnApply, false},
    {198, 601, StateEffectMoment::kOnApply, false},
    {200, 604, StateEffectMoment::kOnApply, false},
    {201, 604, StateEffectMoment::kOnApply, false},
    {207, 617, StateEffectMoment::kOnApply, false},
    {232, 418, StateEffectMoment::kOnApply, true},
    {232, 544, StateEffectMoment::kOnApply, true},
    {235, 254, StateEffectMoment::kAlways, true},
    {235, 1132, StateEffectMoment::kAlways, true},
    {277, 456, StateEffectMoment::kAlways, false},
    {278, 456, StateEffectMoment::kAlways, false},
    {279, 456, StateEffectMoment::kAlways, false},
    {280, 456, StateEffectMoment::kAlways, false},
    {281, 456, StateEffectMoment::kAlways, false},
    {328, 788, StateEffectMoment::kOnApply, true},
    {330, 755, StateEffectMoment::kOnApply, true},
    {334, 792, StateEffectMoment::kOnApply, false},
    {350, 833, StateEffectMoment::kOnApply, false},
    {351, 789, StateEffectMoment::kOnApply, true},
    {358, 752, StateEffectMoment::kOnApply, false},
    {360, 744, StateEffectMoment::kOnApply, true},
    {379, 744, StateEffectMoment::kAlways, true},
    {394, 809, StateEffectMoment::kOnApply, true},
    {405, 909, StateEffectMoment::kOnApply, false},
    {422, 823, StateEffectMoment::kOnApply, true},
    {433, 453, StateEffectMoment::kOnApply, false},
    {434, 879, StateEffectMoment::kOnApply, true},
    {437, 940, StateEffectMoment::kOnApply, true},
    {439, 865, StateEffectMoment::kOnApply, false},
    {441, 867, StateEffectMoment::kOnApply, false},
    {445, 925, StateEffectMoment::kOnApply, false},
    {448, 864, StateEffectMoment::kOnApply, false},
    {451, 871, StateEffectMoment::kOnApply, false},
    {452, 87, StateEffectMoment::kOnApply, false},
    {453, 799, StateEffectMoment::kOnApply, false},
    {454, 865, StateEffectMoment::kOnApply, false},
    {464, 368, StateEffectMoment::kOnApply, false},
    {473, 375, StateEffectMoment::kOnApply, false},
    {645, 998, StateEffectMoment::kOnApply, true},
    {646, 394, StateEffectMoment::kOnApply, false},
    {655, 963, StateEffectMoment::kAlways, false},
    {656, 1004, StateEffectMoment::kOnApply, true},
    {657, 1003, StateEffectMoment::kOnApply, true},
    {659, 963, StateEffectMoment::kOnApply, false},
    {659, 1005, StateEffectMoment::kOnApply, true},
    {660, 544, StateEffectMoment::kOnApply, true},
    {675, 1013, StateEffectMoment::kOnApply, true},
    {676, 1019, StateEffectMoment::kOnApply, false},
    {713, 1037, StateEffectMoment::kOnApply, false},
    {716, 1057, StateEffectMoment::kOnApply, false},
    {717, 1048, StateEffectMoment::kOnApply, false},
    {730, 1040, StateEffectMoment::kOnApply, false},
    {734, 1049, StateEffectMoment::kOnApply, false},
    {735, 404, StateEffectMoment::kOnApply, false},
    {736, 1055, StateEffectMoment::kOnApply, false},
    {737, 1054, StateEffectMoment::kOnApply, false},
    {754, 1076, StateEffectMoment::kAlways, false},
    {755, 1061, StateEffectMoment::kOnApply, false},
    {755, 1075, StateEffectMoment::kOnApply, false},
    {756, 1139, StateEffectMoment::kOnApply, false},
    {757, 1074, StateEffectMoment::kOnApply, false},
    {760, 1136, StateEffectMoment::kOnApply, false},
    {859, 1090, StateEffectMoment::kOnApply, false},
    {869, 792, StateEffectMoment::kOnApply, false},
    {896, 1110, StateEffectMoment::kOnApply, false},
    {917, 1119, StateEffectMoment::kOnApply, false},
    {919, 1097, StateEffectMoment::kOnApply, false},
    {937, 1096, StateEffectMoment::kOnApply, false},
    {1016, 1151, StateEffectMoment::kOnApply, false},
    {1017, 1152, StateEffectMoment::kOnApply, false},
    {1018, 1153, StateEffectMoment::kOnApply, false},
    {1019, 1154, StateEffectMoment::kOnApply, false},
    {1020, 1155, StateEffectMoment::kOnApply, false},
    {1021, 1156, StateEffectMoment::kOnApply, false},
    {1022, 1157, StateEffectMoment::kOnApply, false},
    {1023, 1158, StateEffectMoment::kOnApply, false},
    {1024, 1159, StateEffectMoment::kOnApply, false},
    {1025, 1160, StateEffectMoment::kOnApply, false},
    {1038, 1215, StateEffectMoment::kOnApply, false},
    {1039, 1217, StateEffectMoment::kOnApply, false},
    {1040, 1214, StateEffectMoment::kOnApply, false},
    {1041, 1221, StateEffectMoment::kOnApply, false},
    {1043, 1216, StateEffectMoment::kOnApply, false},
    {1050, 1201, StateEffectMoment::kOnApply, false},
    {1055, 1222, StateEffectMoment::kOnApply, false},
    {1121, 375, StateEffectMoment::kOnApply, false},
    {1125, 1362, StateEffectMoment::kOnApply, false},
    {1126, 1644, StateEffectMoment::kOnApply, false},
    {1139, 1398, StateEffectMoment::kOnApply, true},
    {1141, 1420, StateEffectMoment::kOnApply, true},
    {1142, 396, StateEffectMoment::kOnApply, true},
    {1148, 1525, StateEffectMoment::kOnApply, true},
    {1150, 1741, StateEffectMoment::kOnApply, false},
    {1152, 1800, StateEffectMoment::kOnApply, false},
    {1160, 2294, StateEffectMoment::kOnApply, false},
    {1162, 2296, StateEffectMoment::kOnApply, false},
    {1165, 1610, StateEffectMoment::kOnApply, true},
    {1166, 1599, StateEffectMoment::kOnApply, false},
    {1167, 1601, StateEffectMoment::kOnApply, true},
    {1176, 1726, StateEffectMoment::kOnApply, false},
    {1176, 1727, StateEffectMoment::kOnApply, false},
    {1184, 1859, StateEffectMoment::kOnApply, false},
    {1190, 1991, StateEffectMoment::kOnApply, false},
    {1196, 1913, StateEffectMoment::kOnApply, false},
    {1197, 1810, StateEffectMoment::kOnApply, false},
    {1200, 2288, StateEffectMoment::kOnApply, false},
    {1201, 1852, StateEffectMoment::kOnApply, false},
    {1201, 1853, StateEffectMoment::kOnApply, false},
    {1201, 2300, StateEffectMoment::kOnApply, false},
    {1205, 164, StateEffectMoment::kOnApply, true},
    {1205, 334, StateEffectMoment::kOnApply, true},
    {1207, 2244, StateEffectMoment::kOnApply, false},
    {1208, 2271, StateEffectMoment::kAlways, false},
    {1208, 2272, StateEffectMoment::kOnApply, false},
    {1209, 2245, StateEffectMoment::kOnApply, false},
    {1210, 2268, StateEffectMoment::kOnApply, false},
    {1211, 2273, StateEffectMoment::kOnApply, false},
    {1212, 2269, StateEffectMoment::kOnApply, false},
    {1214, 2274, StateEffectMoment::kOnApply, false},
    {1216, 2246, StateEffectMoment::kOnApply, false},
    {1230, 2051, StateEffectMoment::kOnApply, false},
    {1230, 2054, StateEffectMoment::kOnApply, false},
    {1231, 2078, StateEffectMoment::kOnApply, false},
    {1231, 2079, StateEffectMoment::kOnApply, false},
    {1231, 2081, StateEffectMoment::kOnApply, false},
    {1232, 1978, StateEffectMoment::kOnApply, false},
    {1232, 1981, StateEffectMoment::kOnApply, false},
    {1233, 1982, StateEffectMoment::kOnApply, false},
    {1233, 1985, StateEffectMoment::kOnApply, false},
    {1234, 1975, StateEffectMoment::kOnApply, false},
    {1236, 1839, StateEffectMoment::kOnApply, false},
    {1237, 1817, StateEffectMoment::kOnApply, false},
    {1237, 2296, StateEffectMoment::kOnApply, false},
    {1245, 2126, StateEffectMoment::kOnApply, false},
    {1249, 2238, StateEffectMoment::kOnApply, false},
    {1250, 1900, StateEffectMoment::kOnApply, false},
    {1258, 2060, StateEffectMoment::kOnApply, false},
    {1261, 2296, StateEffectMoment::kOnApply, false},
    {1271, 2298, StateEffectMoment::kOnApply, false},
    {1301, 744, StateEffectMoment::kOnApply, true},
    {1318, 2277, StateEffectMoment::kOnApply, false},
    {1319, 2278, StateEffectMoment::kOnApply, false},
    {1337, 2305, StateEffectMoment::kOnApply, false},
    {1338, 2306, StateEffectMoment::kOnApply, false},
    {1339, 2302, StateEffectMoment::kOnApply, false},
    {1339, 2303, StateEffectMoment::kOnRelease, false},
}};

constexpr std::array<StateEffectEntry, 11> kStatePacketEffects = {{
    {354, 729, StateEffectMoment::kAlways, false},
    {358, 752, StateEffectMoment::kAlways, false},
    {432, 1665, StateEffectMoment::kAlways, false},
    {432, 1667, StateEffectMoment::kAlways, false},
    {432, 1668, StateEffectMoment::kAlways, false},
    {434, 879, StateEffectMoment::kAlways, true},
    {439, 865, StateEffectMoment::kAlways, false},
    {441, 867, StateEffectMoment::kAlways, false},
    {451, 871, StateEffectMoment::kAlways, false},
    {1125, 1362, StateEffectMoment::kAlways, false},
    {1237, 1821, StateEffectMoment::kAlways, false},
}};

constexpr std::array<StateEffectEntry, 118> kStateRebuildEffects = {{
    {2, 166, StateEffectMoment::kAlways, false},
    {25, 166, StateEffectMoment::kAlways, false},
    {31, 166, StateEffectMoment::kAlways, false},
    {68, 166, StateEffectMoment::kAlways, false},
    {86, 203, StateEffectMoment::kAlways, false},
    {87, 254, StateEffectMoment::kAlways, true},
    {87, 1132, StateEffectMoment::kAlways, true},
    {97, 655, StateEffectMoment::kAlways, true},
    {103, 474, StateEffectMoment::kAlways, true},
    {105, 369, StateEffectMoment::kAlways, false},
    {107, 368, StateEffectMoment::kAlways, false},
    {110, 375, StateEffectMoment::kAlways, false},
    {113, 382, StateEffectMoment::kAlways, true},
    {123, 394, StateEffectMoment::kAlways, false},
    {149, 424, StateEffectMoment::kAlways, true},
    {152, 164, StateEffectMoment::kAlways, true},
    {152, 487, StateEffectMoment::kAlways, false},
    {153, 419, StateEffectMoment::kAlways, true},
    {161, 166, StateEffectMoment::kAlways, false},
    {165, 418, StateEffectMoment::kAlways, true},
    {165, 544, StateEffectMoment::kAlways, true},
    {166, 418, StateEffectMoment::kAlways, true},
    {166, 544, StateEffectMoment::kAlways, true},
    {167, 418, StateEffectMoment::kAlways, true},
    {167, 544, StateEffectMoment::kAlways, true},
    {188, 166, StateEffectMoment::kAlways, false},
    {207, 617, StateEffectMoment::kAlways, false},
    {232, 418, StateEffectMoment::kAlways, true},
    {232, 544, StateEffectMoment::kAlways, true},
    {235, 254, StateEffectMoment::kAlways, true},
    {235, 1132, StateEffectMoment::kAlways, true},
    {263, 586, StateEffectMoment::kAlways, true},
    {266, 673, StateEffectMoment::kAlways, true},
    {328, 788, StateEffectMoment::kAlways, true},
    {330, 755, StateEffectMoment::kAlways, true},
    {334, 792, StateEffectMoment::kAlways, false},
    {350, 833, StateEffectMoment::kAlways, false},
    {351, 789, StateEffectMoment::kAlways, true},
    {360, 744, StateEffectMoment::kAlways, true},
    {379, 744, StateEffectMoment::kAlways, true},
    {394, 809, StateEffectMoment::kAlways, true},
    {422, 823, StateEffectMoment::kAlways, true},
    {437, 940, StateEffectMoment::kAlways, true},
    {445, 925, StateEffectMoment::kAlways, false},
    {448, 864, StateEffectMoment::kAlways, false},
    {452, 87, StateEffectMoment::kAlways, false},
    {453, 799, StateEffectMoment::kAlways, false},
    {464, 368, StateEffectMoment::kAlways, false},
    {473, 375, StateEffectMoment::kAlways, false},
    {656, 1004, StateEffectMoment::kAlways, true},
    {657, 1003, StateEffectMoment::kAlways, true},
    {659, 963, StateEffectMoment::kAlways, false},
    {659, 1005, StateEffectMoment::kAlways, true},
    {675, 1013, StateEffectMoment::kAlways, true},
    {716, 1057, StateEffectMoment::kAlways, false},
    {717, 1048, StateEffectMoment::kAlways, false},
    {730, 1040, StateEffectMoment::kAlways, false},
    {735, 404, StateEffectMoment::kAlways, false},
    {736, 1055, StateEffectMoment::kAlways, false},
    {737, 1054, StateEffectMoment::kAlways, false},
    {760, 1136, StateEffectMoment::kAlways, false},
    {869, 792, StateEffectMoment::kAlways, false},
    {896, 1110, StateEffectMoment::kAlways, false},
    {917, 1119, StateEffectMoment::kAlways, false},
    {919, 1097, StateEffectMoment::kAlways, false},
    {937, 1096, StateEffectMoment::kAlways, false},
    {1016, 1151, StateEffectMoment::kAlways, false},
    {1017, 1152, StateEffectMoment::kAlways, false},
    {1018, 1153, StateEffectMoment::kAlways, false},
    {1019, 1154, StateEffectMoment::kAlways, false},
    {1020, 1155, StateEffectMoment::kAlways, false},
    {1021, 1156, StateEffectMoment::kAlways, false},
    {1022, 1157, StateEffectMoment::kAlways, false},
    {1023, 1158, StateEffectMoment::kAlways, false},
    {1024, 1159, StateEffectMoment::kAlways, false},
    {1025, 1160, StateEffectMoment::kAlways, false},
    {1038, 1215, StateEffectMoment::kAlways, false},
    {1039, 1217, StateEffectMoment::kAlways, false},
    {1040, 1214, StateEffectMoment::kAlways, false},
    {1041, 1221, StateEffectMoment::kAlways, false},
    {1043, 1216, StateEffectMoment::kAlways, false},
    {1050, 1201, StateEffectMoment::kAlways, false},
    {1055, 1222, StateEffectMoment::kAlways, false},
    {1056, 424, StateEffectMoment::kAlways, true},
    {1057, 424, StateEffectMoment::kAlways, true},
    {1058, 424, StateEffectMoment::kAlways, true},
    {1059, 424, StateEffectMoment::kAlways, true},
    {1121, 375, StateEffectMoment::kAlways, false},
    {1126, 1644, StateEffectMoment::kAlways, false},
    {1150, 1741, StateEffectMoment::kAlways, false},
    {1152, 1800, StateEffectMoment::kAlways, false},
    {1165, 1610, StateEffectMoment::kAlways, true},
    {1176, 1726, StateEffectMoment::kAlways, false},
    {1176, 1727, StateEffectMoment::kAlways, false},
    {1196, 1913, StateEffectMoment::kAlways, false},
    {1197, 1810, StateEffectMoment::kAlways, false},
    {1201, 1852, StateEffectMoment::kAlways, false},
    {1201, 1853, StateEffectMoment::kAlways, false},
    {1205, 164, StateEffectMoment::kAlways, true},
    {1205, 334, StateEffectMoment::kAlways, true},
    {1208, 2270, StateEffectMoment::kAlways, false},
    {1208, 2272, StateEffectMoment::kAlways, false},
    {1210, 2268, StateEffectMoment::kAlways, false},
    {1211, 2273, StateEffectMoment::kAlways, false},
    {1212, 2269, StateEffectMoment::kAlways, false},
    {1232, 1978, StateEffectMoment::kAlways, false},
    {1232, 1981, StateEffectMoment::kAlways, false},
    {1233, 1982, StateEffectMoment::kAlways, false},
    {1233, 1985, StateEffectMoment::kAlways, false},
    {1234, 1975, StateEffectMoment::kAlways, false},
    {1236, 1839, StateEffectMoment::kAlways, false},
    {1237, 1817, StateEffectMoment::kAlways, false},
    {1244, 2109, StateEffectMoment::kAlways, false},
    {1245, 2126, StateEffectMoment::kAlways, false},
    {1250, 1900, StateEffectMoment::kAlways, false},
    {1258, 2060, StateEffectMoment::kAlways, false},
    {1318, 2277, StateEffectMoment::kAlways, false},
    {1319, 2278, StateEffectMoment::kAlways, false},
}};

constexpr std::array<StateEffectEntry, 170> kStateRemovals = {{
    {2, 166, StateEffectMoment::kAlways, false},
    {25, 166, StateEffectMoment::kAlways, false},
    {31, 166, StateEffectMoment::kAlways, false},
    {68, 166, StateEffectMoment::kAlways, false},
    {70, 865, StateEffectMoment::kAlways, false},
    {71, 873, StateEffectMoment::kAlways, false},
    {72, 877, StateEffectMoment::kAlways, false},
    {73, 861, StateEffectMoment::kAlways, false},
    {74, 865, StateEffectMoment::kAlways, false},
    {75, 38, StateEffectMoment::kAlways, false},
    {76, 871, StateEffectMoment::kAlways, false},
    {77, 873, StateEffectMoment::kAlways, false},
    {78, 867, StateEffectMoment::kAlways, false},
    {79, 864, StateEffectMoment::kAlways, false},
    {80, 871, StateEffectMoment::kAlways, false},
    {81, 875, StateEffectMoment::kAlways, false},
    {82, 864, StateEffectMoment::kAlways, false},
    {83, 865, StateEffectMoment::kAlways, false},
    {84, 875, StateEffectMoment::kAlways, false},
    {86, 166, StateEffectMoment::kAlways, false},
    {86, 203, StateEffectMoment::kAlways, false},
    {87, 254, StateEffectMoment::kAlways, true},
    {87, 1132, StateEffectMoment::kAlways, true},
    {95, 453, StateEffectMoment::kAlways, false},
    {97, 655, StateEffectMoment::kOnRelease, true},
    {103, 367, StateEffectMoment::kAlways, false},
    {103, 474, StateEffectMoment::kAlways, true},
    {105, 369, StateEffectMoment::kAlways, false},
    {107, 368, StateEffectMoment::kAlways, false},
    {110, 375, StateEffectMoment::kAlways, false},
    {113, 382, StateEffectMoment::kAlways, true},
    {121, 395, StateEffectMoment::kAlways, true},
    {121, 396, StateEffectMoment::kAlways, true},
    {122, 396, StateEffectMoment::kAlways, true},
    {123, 394, StateEffectMoment::kAlways, false},
    {149, 424, StateEffectMoment::kAlways, true},
    {152, 164, StateEffectMoment::kAlways, true},
    {153, 419, StateEffectMoment::kAlways, true},
    {161, 166, StateEffectMoment::kAlways, false},
    {165, 418, StateEffectMoment::kAlways, true},
    {165, 544, StateEffectMoment::kAlways, true},
    {166, 418, StateEffectMoment::kAlways, true},
    {166, 544, StateEffectMoment::kAlways, true},
    {167, 418, StateEffectMoment::kAlways, true},
    {167, 544, StateEffectMoment::kAlways, true},
    {188, 166, StateEffectMoment::kAlways, false},
    {198, 601, StateEffectMoment::kAlways, false},
    {200, 604, StateEffectMoment::kAlways, false},
    {201, 604, StateEffectMoment::kAlways, false},
    {207, 617, StateEffectMoment::kAlways, false},
    {232, 418, StateEffectMoment::kAlways, true},
    {232, 544, StateEffectMoment::kAlways, true},
    {235, 254, StateEffectMoment::kAlways, true},
    {235, 1132, StateEffectMoment::kAlways, true},
    {328, 788, StateEffectMoment::kAlways, true},
    {330, 755, StateEffectMoment::kAlways, true},
    {334, 792, StateEffectMoment::kAlways, false},
    {350, 833, StateEffectMoment::kAlways, false},
    {351, 789, StateEffectMoment::kAlways, true},
    {358, 752, StateEffectMoment::kAlways, false},
    {360, 744, StateEffectMoment::kAlways, true},
    {379, 744, StateEffectMoment::kAlways, true},
    {394, 809, StateEffectMoment::kAlways, true},
    {394, 819, StateEffectMoment::kAlways, false},
    {422, 823, StateEffectMoment::kAlways, true},
    {433, 453, StateEffectMoment::kAlways, false},
    {434, 879, StateEffectMoment::kAlways, true},
    {437, 940, StateEffectMoment::kAlways, true},
    {445, 925, StateEffectMoment::kAlways, false},
    {448, 864, StateEffectMoment::kAlways, false},
    {452, 87, StateEffectMoment::kAlways, false},
    {453, 799, StateEffectMoment::kAlways, false},
    {454, 865, StateEffectMoment::kAlways, false},
    {464, 368, StateEffectMoment::kAlways, false},
    {473, 375, StateEffectMoment::kAlways, false},
    {606, 963, StateEffectMoment::kAlways, false},
    {645, 998, StateEffectMoment::kAlways, true},
    {646, 394, StateEffectMoment::kAlways, false},
    {655, 963, StateEffectMoment::kAlways, false},
    {656, 1004, StateEffectMoment::kAlways, true},
    {657, 1003, StateEffectMoment::kAlways, true},
    {659, 963, StateEffectMoment::kAlways, false},
    {659, 1005, StateEffectMoment::kAlways, true},
    {660, 544, StateEffectMoment::kAlways, true},
    {675, 1013, StateEffectMoment::kOnRelease, true},
    {676, 1019, StateEffectMoment::kAlways, false},
    {713, 1037, StateEffectMoment::kAlways, false},
    {716, 1057, StateEffectMoment::kAlways, false},
    {717, 1048, StateEffectMoment::kAlways, false},
    {730, 1040, StateEffectMoment::kAlways, false},
    {734, 1049, StateEffectMoment::kAlways, false},
    {735, 404, StateEffectMoment::kAlways, false},
    {736, 1055, StateEffectMoment::kAlways, false},
    {737, 1054, StateEffectMoment::kAlways, false},
    {755, 1075, StateEffectMoment::kAlways, false},
    {756, 1139, StateEffectMoment::kOnRelease, false},
    {760, 1136, StateEffectMoment::kOnRelease, false},
    {859, 1090, StateEffectMoment::kOnRelease, false},
    {869, 792, StateEffectMoment::kAlways, false},
    {896, 1110, StateEffectMoment::kOnRelease, false},
    {917, 1119, StateEffectMoment::kOnRelease, false},
    {919, 1097, StateEffectMoment::kOnRelease, false},
    {937, 1096, StateEffectMoment::kAlways, false},
    {1016, 1151, StateEffectMoment::kOnRelease, false},
    {1017, 1152, StateEffectMoment::kOnRelease, false},
    {1018, 1153, StateEffectMoment::kOnRelease, false},
    {1019, 1154, StateEffectMoment::kOnRelease, false},
    {1020, 1155, StateEffectMoment::kOnRelease, false},
    {1021, 1156, StateEffectMoment::kOnRelease, false},
    {1022, 1157, StateEffectMoment::kOnRelease, false},
    {1023, 1158, StateEffectMoment::kOnRelease, false},
    {1024, 1159, StateEffectMoment::kOnRelease, false},
    {1025, 1160, StateEffectMoment::kOnRelease, false},
    {1038, 1215, StateEffectMoment::kAlways, false},
    {1039, 1217, StateEffectMoment::kAlways, false},
    {1040, 1214, StateEffectMoment::kAlways, false},
    {1041, 1221, StateEffectMoment::kAlways, false},
    {1043, 1216, StateEffectMoment::kAlways, false},
    {1050, 1201, StateEffectMoment::kAlways, false},
    {1055, 1222, StateEffectMoment::kAlways, false},
    {1056, 424, StateEffectMoment::kAlways, true},
    {1057, 424, StateEffectMoment::kAlways, true},
    {1058, 424, StateEffectMoment::kAlways, true},
    {1059, 424, StateEffectMoment::kAlways, true},
    {1121, 375, StateEffectMoment::kAlways, false},
    {1125, 1362, StateEffectMoment::kOnRelease, false},
    {1126, 1644, StateEffectMoment::kAlways, false},
    {1139, 1398, StateEffectMoment::kAlways, true},
    {1141, 1420, StateEffectMoment::kAlways, true},
    {1142, 396, StateEffectMoment::kAlways, true},
    {1148, 1525, StateEffectMoment::kAlways, true},
    {1150, 1741, StateEffectMoment::kOnRelease, false},
    {1152, 1800, StateEffectMoment::kAlways, false},
    {1165, 1610, StateEffectMoment::kAlways, true},
    {1166, 1599, StateEffectMoment::kAlways, false},
    {1167, 1601, StateEffectMoment::kAlways, true},
    {1176, 1726, StateEffectMoment::kAlways, false},
    {1176, 1727, StateEffectMoment::kAlways, false},
    {1184, 1859, StateEffectMoment::kOnRelease, false},
    {1190, 1991, StateEffectMoment::kOnRelease, false},
    {1196, 1913, StateEffectMoment::kOnRelease, false},
    {1197, 1810, StateEffectMoment::kOnRelease, false},
    {1201, 1852, StateEffectMoment::kOnRelease, false},
    {1201, 1853, StateEffectMoment::kAlways, false},
    {1205, 334, StateEffectMoment::kOnRelease, true},
    {1208, 2270, StateEffectMoment::kOnRelease, false},
    {1208, 2272, StateEffectMoment::kOnRelease, false},
    {1209, 2245, StateEffectMoment::kAlways, false},
    {1210, 2268, StateEffectMoment::kAlways, false},
    {1211, 2273, StateEffectMoment::kOnRelease, false},
    {1212, 2269, StateEffectMoment::kAlways, false},
    {1214, 2274, StateEffectMoment::kOnRelease, false},
    {1230, 2051, StateEffectMoment::kOnRelease, false},
    {1230, 2052, StateEffectMoment::kOnRelease, false},
    {1231, 2078, StateEffectMoment::kOnRelease, false},
    {1231, 2079, StateEffectMoment::kOnRelease, false},
    {1231, 2081, StateEffectMoment::kOnRelease, false},
    {1232, 1978, StateEffectMoment::kOnRelease, false},
    {1232, 1981, StateEffectMoment::kOnRelease, false},
    {1233, 1982, StateEffectMoment::kOnRelease, false},
    {1233, 1985, StateEffectMoment::kOnRelease, false},
    {1234, 1975, StateEffectMoment::kOnRelease, false},
    {1236, 1839, StateEffectMoment::kOnRelease, false},
    {1237, 1817, StateEffectMoment::kOnRelease, false},
    {1245, 2126, StateEffectMoment::kOnRelease, false},
    {1250, 1900, StateEffectMoment::kOnRelease, false},
    {1301, 744, StateEffectMoment::kOnRelease, true},
    {1318, 2277, StateEffectMoment::kOnRelease, false},
    {1319, 2278, StateEffectMoment::kOnRelease, false},
    {1339, 2302, StateEffectMoment::kOnRelease, false},
}};

constexpr std::array<StateSoundEntry, 4> kStateSounds = {{
    {134, "effect\\T_벽튕김.wav"},
    {152, "effect\\_blind.wav"},
    {182, "lava_golem_move.wav"},
    {422, "effect\\manhole.wav"},
}};

// Les gestes que le message 110 joue à la pose COMME à la levée, gardés par
// un effet porté — lus à la main dans scripts/gen_client_state_effects.py.
constexpr std::array<StateGuardedGesture, 4> kStateGuardedGestures = {{
    {133, 443, false, 442},
    {133, 442, true, 442},
    {134, 426, false, 0},
    {134, 442, true, 442},
}};

constexpr std::array<StateValueEffectEntry, 2> kStateValueEffects = {{
    {152, 473, 0, 2, 11, true, true},
    {339, 1299, 1466, 1, 10, false, false},
}};

}  // namespace

std::span<const StateEffectEntry> StateEffectTable() { return kStateEffects; }

std::span<const StateEffectEntry> StatePacketEffectTable() { return kStatePacketEffects; }

std::span<const StateEffectEntry> StateRebuildEffectTable() { return kStateRebuildEffects; }

std::span<const StateEffectEntry> StateRemovalTable() { return kStateRemovals; }

std::span<const StateValueEffectEntry> StateValueEffectTable() { return kStateValueEffects; }

const StateValueEffectEntry* StateValueEffectFor(std::uint16_t state_id) {
  const auto found =
      std::ranges::find(kStateValueEffects, state_id, &StateValueEffectEntry::state_id);
  return found == kStateValueEffects.end() ? nullptr : &*found;
}

std::span<const StateSoundEntry> StateSoundTable() { return kStateSounds; }

std::span<const StateEffectEntry> StateEffectsFor(std::uint16_t state_id) {
  const auto first = std::ranges::find(kStateEffects, state_id, &StateEffectEntry::state_id);
  if (first == kStateEffects.end()) {
    return {};
  }
  const auto last = std::find_if(
      first, kStateEffects.end(),
      [state_id](const StateEffectEntry& entry) { return entry.state_id != state_id; });
  return {first, last};
}

std::span<const StateEffectEntry> StatePacketEffectsFor(std::uint16_t state_id) {
  const auto first = std::ranges::find(kStatePacketEffects, state_id, &StateEffectEntry::state_id);
  if (first == kStatePacketEffects.end()) {
    return {};
  }
  const auto last = std::find_if(
      first, kStatePacketEffects.end(),
      [state_id](const StateEffectEntry& entry) { return entry.state_id != state_id; });
  return {first, last};
}

std::span<const StateEffectEntry> StateRebuildEffectsFor(std::uint16_t state_id) {
  const auto first = std::ranges::find(kStateRebuildEffects, state_id, &StateEffectEntry::state_id);
  if (first == kStateRebuildEffects.end()) {
    return {};
  }
  const auto last = std::find_if(
      first, kStateRebuildEffects.end(),
      [state_id](const StateEffectEntry& entry) { return entry.state_id != state_id; });
  return {first, last};
}

std::span<const StateEffectEntry> StateRemovalsFor(std::uint16_t state_id) {
  const auto first = std::ranges::find(kStateRemovals, state_id, &StateEffectEntry::state_id);
  if (first == kStateRemovals.end()) {
    return {};
  }
  const auto last = std::find_if(
      first, kStateRemovals.end(),
      [state_id](const StateEffectEntry& entry) { return entry.state_id != state_id; });
  return {first, last};
}

std::span<const StateGuardedGesture> StateGuardedGesturesFor(std::uint16_t state_id) {
  const auto first =
      std::ranges::find(kStateGuardedGestures, state_id, &StateGuardedGesture::state_id);
  const auto last = std::find_if(
      first, kStateGuardedGestures.end(),
      [state_id](const StateGuardedGesture& gesture) { return gesture.state_id != state_id; });
  return {first, last};
}

std::string_view StateSoundFor(std::uint16_t state_id) {
  const auto found = std::ranges::find(kStateSounds, state_id, &StateSoundEntry::state_id);
  return found == kStateSounds.end() ? std::string_view{} : found->wave;
}

}  // namespace ro::render
