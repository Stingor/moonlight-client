// Les effets qu'une action d'ÉTAT désigne par un NOM.
//
// Trente-huit états de quatrième classe montrent un effet que rien d'autre ne
// porte : la table des effets d'option ne connaît que les anciens, indexés par
// un bit du mot d'option. Le nom se résout dans une table de DONNÉES que
// l'installation porte — voir ro::formats::NamedEffectTable.
//
// Le client tient ces actions dans une table du gestionnaire d'effets, que
// chaque fabrique emplit en s'y inscrivant. La table efstids.lub de
// l'installation s'accorde avec elle sans exception.
//
// ⚠ UN ÉTAT N'EST PAS UNE COMPÉTENCE. L'état IG_GUARD_STANCE s'inscrit sous
// 1202, la compétence du même nom sous 5255 : traduire le nom par la table des
// compétences donnerait l'autre.
//
// ⚠ LES DEUX MOMENTS SONT MESURÉS PAR LES VERBES, aucun chemin du client ne les
// distinguant : le slot 0 sème et le slot 1 retire EXACTEMENT ce que le premier
// a semé.
//
// ⚠ UNE CASE PEUT AUSSI POSER UN EFFET PAR SON NUMÉRO, sans rien nommer : ces
// entrées-là ont un nom vide et un `effect_id`, leur retrait précède leur pose.
//
// ⚠ Fichier ENGENDRÉ par scripts/gen_client_state_named_effects.py depuis les
// relevés scripts/client_named_effect_actions.json et
// scripts/client_skill_action_cases.json, pas écrit à la main.

#include <array>

#include "ro/render/skill_named_table.h"

namespace ro::render {
namespace {

// Triée par identifiant d'état, puis par slot ; les noms d'un même slot gardent
// l'ordre dans lequel l'action les joue.
constexpr std::array<StateNamedEffectEntry, 162> kStateNamedEffects = {{
    // UNLIMIT (722)
    {722, 0, "unlimit_loop_bottom", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {722, 0, "unlimit_loop", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {722, 1, "unlimit_loop_bottom", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {722, 1, "unlimit_loop", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // GRADUAL_GRAVITY (1014)
    {.state_id = 1014,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 779},
    {.state_id = 1014,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 779},
    // SERVANTWEAPON (1172)
    {.state_id = 1172,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 1694},
    // SERVANT_SIGN (1173)
    {1173, 0, "servant_sign_bottom", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1173, 0, "servant_sign", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1173, 1, "servant_sign_bottom", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1173, 1, "servant_sign", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // CHARGINGPIERCE_COUNT (1175)
    {.state_id = 1175,
     .slot = 0,
     .name = "chargingpierce_hit",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .on_designated_actor = true},
    {.state_id = 1175,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .counts_charges = true},
    {.state_id = 1175,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 1764},
    // VIGOR (1178)
    {1178, 0, "vigor_buff", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1178, 1, "vigor_buff", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // SHADOW_EXCEED (1192)
    {.state_id = 1192,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 1004,
     .endless = true},
    {.state_id = 1192,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 1004},
    // DANCING_KNIFE (1193)
    {1193, 0, "dancing_knife_skill_bottom_begin", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1193, 0, "dancing_knife_skill_bottom_loop", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1193, 0, "dancing_knife_skill_begin", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1193, 0, "dancing_knife_skill_loop", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1193, 1, "dancing_knife_skill_bottom_begin", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1193, 1, "dancing_knife_skill_bottom_loop", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1193, 1, "dancing_knife_skill_begin", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1193, 1, "dancing_knife_skill_loop", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // IG_GUARD_STANCE (1202)
    {1202, 0, "new_guard_stance_loop_bottom", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1202, 1, "new_guard_stance_loop_bottom", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // IG_ATTACK_STANCE (1203)
    {1203, 0, "new_attack_stance_loop_bottom", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1203, 1, "new_attack_stance_loop_bottom", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // IG_GUARDIAN_SHIELD (1204)
    {1204, 0, "new_guardianshield_buff_start", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1204, 0, "new_guardianshield_buff_loop", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1204, 1, "new_guardianshield_buff_loop", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // IG_HOLY_SHIELD (1220)
    {1220, 0, "new_holy_shield_loop", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1220, 0, "new_holy_shield_loop_bottom", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1220, 1, "new_holy_shield_loop", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1220, 1, "new_holy_shield_loop_bottom", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // IG_ULTIMATE_SACRIFICE (1221)
    {1221, 0, "ultimate_sacrifice_buff", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1221, 1, "ultimate_sacrifice_buff", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // IG_GRAND_JUDGEMENT (1222)
    {1222, 0, "new_grand_judgement_buff", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1222, 1, "new_grand_judgement_buff", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // CALAMITYGALE (1252)
    {1252, 0, "calumitygale_loop", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1252, 1, "calumitygale_loop", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // MYSTIC_SYMPHONY (1256)
    {1256, 0, "mysticsymphony_note", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1256, 1, "mysticsymphony_note", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // GEF_NOCTURN (1259)
    {.state_id = 1259,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2293},
    // AIN_RHAPSODY (1260)
    {.state_id = 1260,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2297},
    // JAWAII_SERENADE (1262)
    {.state_id = 1262,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2298},
    // PRON_MARCH (1263)
    {.state_id = 1263,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2294},
    // INTENSIVE_AIM (1345)
    {1345, 1, "intensive_aim_00", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1345, 1, "intensive_aim_01", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1345, 1, "intensive_aim_02", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1345, 1, "intensive_aim_03", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1345, 1, "intensive_aim_04", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1345, 1, "intensive_aim_05", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1345, 1, "intensive_aim_06", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1345, 1, "intensive_aim_07", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1345, 1, "intensive_aim_08", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1345, 1, "intensive_aim_09", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1345, 1, "intensive_aim_10", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1345, 1, "intensiveaim_full", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // INTENSIVE_AIM_COUNT (1346)
    {1346, 0, "intensive_aim_00", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1346, 0, "intensive_aim_01", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1346, 0, "intensive_aim_02", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1346, 0, "intensive_aim_03", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1346, 0, "intensive_aim_04", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1346, 0, "intensive_aim_05", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1346, 0, "intensive_aim_06", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1346, 0, "intensive_aim_07", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1346, 0, "intensive_aim_08", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1346, 0, "intensive_aim_09", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1346, 0, "intensive_aim_10", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1346, 0, "intensiveaim_full", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {.state_id = 1346,
     .slot = 0,
     .name = "intensive_aim_%02d",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .name_takes_value = true},
    {.state_id = 1346,
     .slot = 0,
     .name = "intensiveaim_full",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .only_at_value = 10},
    // HIDDEN_CARD (1354)
    {1354, 0, "hiddencard_thunder", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1354, 1, "hiddencard_thunder", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // T_FIRST_GOD (1360)
    {.state_id = 1360,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2355},
    {.state_id = 1360,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2355,
     .effect_param = 1},
    // T_SECOND_GOD (1361)
    {.state_id = 1361,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2355},
    {.state_id = 1361,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2355,
     .effect_param = 2},
    // T_THIRD_GOD (1362)
    {.state_id = 1362,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2355},
    {.state_id = 1362,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2355,
     .effect_param = 3},
    // T_FOURTH_GOD (1363)
    {.state_id = 1363,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2355},
    {.state_id = 1363,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2355,
     .effect_param = 4},
    // T_FIVETH_GOD (1364)
    {1364, 0, "circle_directions_elements_loop", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1364, 1, "circle_directions_elements_loop", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // HEAVEN_AND_EARTH (1365)
    {1365, 0, "soul_of_heaven_and_earth_buff", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1365, 1, "soul_of_heaven_and_earth_buff", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // HOGOGONG (1366)
    {1366, 0, "howling_of_chul_ho_scared", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1366, 1, "howling_of_chul_ho_scared", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // MARINE_FESTIVAL (1367)
    {1367, 0, "marine_festival_of_ki_sul", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    // SANDY_FESTIVAL (1368)
    {1368, 0, "sandy_festival_of_ki_sul", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    // TEMPORARY_COMMUNION (1377)
    {1377, 0, "temporary_communion_buff_bottom", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1377, 0, "temporary_communion_buff", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1377, 1, "temporary_communion_buff_bottom", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1377, 1, "temporary_communion_buff", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // BREAKINGLIMIT (1383)
    {1383, 0, "breakinglimit_buff", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1383, 1, "breakinglimit_buff", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // RULEBREAK (1384)
    {1384, 0, "rulebreak_buff", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1384, 1, "rulebreak_buff", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // RISING_SUN (1385)
    {1385, 0, "ske_rising_sun_00", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1385, 0, "ske_rising_sun_01", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1385, 1, "ske_rising_sun_01", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // NOON_SUN (1386)
    {1386, 0, "ske_rising_sun_02", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1386, 0, "ske_rising_sun_03", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1386, 1, "ske_rising_sun_03", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // SUNSET_SUN (1387)
    {1387, 0, "ske_rising_sun_04", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1387, 0, "ske_rising_sun_05", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1387, 1, "ske_rising_sun_05", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // RISING_MOON (1388)
    {1388, 0, "ske_rising_moon_00", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1388, 0, "ske_rising_moon_01", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1388, 1, "ske_rising_moon_01", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // MIDNIGHT_MOON (1389)
    {1389, 0, "ske_rising_moon_02", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1389, 0, "ske_rising_moon_03", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1389, 1, "ske_rising_moon_03", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // DAWN_MOON (1390)
    {1390, 0, "ske_rising_moon_04", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1390, 0, "ske_rising_moon_05", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1390, 1, "ske_rising_moon_05", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // STAR_BURST (1391)
    {.state_id = 1391,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2348},
    {.state_id = 1391,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2348},
    // SKY_ENCHANT (1392)
    {1392, 0, "ske_enchanting_sky_00", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1392, 0, "ske_enchanting_sky_01", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1392, 1, "ske_enchanting_sky_01", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // SHADOW_CLOCK (1393)
    {1393, 0, "kagekakusi", NamedEffectVerb::kSow, NamedEffectAnchor::kPosition},
    {.state_id = 1393,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 963},
    {.state_id = 1393,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 963},
    // NIGHTMARE (1395)
    {1395, 0, "efst_nightmare", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1395, 1, "efst_nightmare", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // ChangeSizeNPC (1420)
    {.state_id = 1420,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2374,
     .endless = true},
    {.state_id = 1420,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2374},
    // ChangeSizeMonster (1421)
    {.state_id = 1421,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2374,
     .endless = true},
    {.state_id = 1421,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2374},
    // ShowEffect1 (1422)
    {.state_id = 1422,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2375,
     .endless = true},
    {.state_id = 1422,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2375},
    // ShowEffect2 (1423)
    {.state_id = 1423,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2375,
     .endless = true},
    {.state_id = 1423,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2375},
    // ShowEffect3 (1424)
    {.state_id = 1424,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2375,
     .endless = true},
    {.state_id = 1424,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2375},
    // TARGET_MARKER (1453)
    {1453, 0, "target_marker_start", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1453, 0, "target_marker_loop", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1453, 1, "target_marker_loop", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // BLOCK_SEAL (1454)
    {1454, 0, "npc_block_seal_start", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1454, 0, "npc_block_seal_loop_bottom", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1454, 0, "npc_block_seal_loop", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {.state_id = 1454,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2400},
    {1454, 1, "npc_block_seal_loop_bottom", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1454, 1, "npc_block_seal_loop", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {.state_id = 1454,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2400},
    // FROST_STORM (1455)
    {.state_id = 1455,
     .slot = 0,
     .name = "",
     .verb = NamedEffectVerb::kSow,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2403},
    {.state_id = 1455,
     .slot = 1,
     .name = "",
     .verb = NamedEffectVerb::kCut,
     .anchor = NamedEffectAnchor::kActor,
     .effect_id = 2403},
    // GROGGY (1456)
    {1456, 0, "groggy_on_loop", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1456, 1, "groggy_on_loop", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    // GenderChange (1559)
    {1559, 0, "gender_change_cast", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1559, 0, "gender_change_loop", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1559, 1, "gender_change_loop", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1559, 1, "gender_change_cast", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1559, 1, "gender_change", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    // MYSTERY_POWDER (1665)
    {1665, 0, "mistery_powder_loop_bottom", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1665, 0, "mistery_powder_loop", NamedEffectVerb::kSow, NamedEffectAnchor::kActor},
    {1665, 1, "mistery_powder_loop_bottom", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
    {1665, 1, "mistery_powder_loop", NamedEffectVerb::kCut, NamedEffectAnchor::kActor},
}};

}  // namespace

std::span<const StateNamedEffectEntry> StateNamedEffectTable() { return kStateNamedEffects; }

}  // namespace ro::render
