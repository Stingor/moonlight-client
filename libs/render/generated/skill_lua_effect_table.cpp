// LES EFFETS DE SOIXANTE-SIX COMPÉTENCES, tirés du LUA de l'installation.
//
// Engendré par scripts/gen_client_skill_lua_effects.py — ne pas éditer.
// Source : data\luafiles514\lua files\skilleffectinfo\skilleffectinfolist.lub (moonlight.grf)
//
// ⚠ QUATRE MOMENTS LISENT CE LUA, et se partagent les emplacements :
//   · l'emploi de la compétence : son, effectID, onTarget
//   · la cible touchée : son de cible, targetEffectID
//   · la case visée : son, groundEffectID
//   · le début de l'incantation : beginEffectID
//
// Les noms coréens sont en UTF-8 : le .lub est en cp949, et le relevé le
// transcode. Un nom lu en latin-1 ne retrouverait aucun fichier.
#include <algorithm>
#include <array>

#include "ro/render/skill_lua_effect.h"

namespace ro::render {
namespace {

// Triée par compétence, puis par emplacement : la recherche rend une tranche.
constexpr std::array<SkillLuaEffectEntry, 73> kSkillLuaEffects = {{
    {751, 864, SkillLuaSlot::kTarget},    // NPC_ALL_STAT_DOWN
    {754, 1149, SkillLuaSlot::kCaster},   // NPC_IMMUNE_PROPERTY
    {755, 1150, SkillLuaSlot::kCaster},   // NPC_MOVE_COORDINATE
    {755, 1150, SkillLuaSlot::kTarget},   // NPC_MOVE_COORDINATE
    {2552, 1134, SkillLuaSlot::kCaster},  // RL_RICHS_COIN
    {2554, 1066, SkillLuaSlot::kTarget},  // RL_BANISHING_BUSTER
    {2556, 153, SkillLuaSlot::kCaster},   // RL_FLICKER
    {2557, 1078, SkillLuaSlot::kTarget},  // RL_S_STORM
    {2558, 1137, SkillLuaSlot::kCaster},  // RL_E_CHAIN
    {2561, 1133, SkillLuaSlot::kCaster},  // RL_FIREDANCE
    {2561, 647, SkillLuaSlot::kTarget},   // RL_FIREDANCE
    {2562, 1139, SkillLuaSlot::kTarget},  // RL_H_MINE
    {2564, 1140, SkillLuaSlot::kCaster},  // RL_FALLEN_ANGEL
    {2566, 1068, SkillLuaSlot::kTarget},  // RL_D_TAIL
    {2568, 1138, SkillLuaSlot::kCaster},  // RL_HEAT_BARREL
    {2570, 1067, SkillLuaSlot::kTarget},  // RL_SLUGSHOT
    {2574, 1198, SkillLuaSlot::kCaster},  // SJ_LIGHTOFMOON
    {2576, 399, SkillLuaSlot::kCaster},   // SJ_FULLMOONKICK
    {2576, 1223, SkillLuaSlot::kCaster},  // SJ_FULLMOONKICK
    {2576, 1230, SkillLuaSlot::kCaster},  // SJ_FULLMOONKICK
    {2577, 1199, SkillLuaSlot::kCaster},  // SJ_LIGHTOFSTAR
    {2579, 1229, SkillLuaSlot::kCaster},  // SJ_NEWMOONKICK
    {2582, 1202, SkillLuaSlot::kTarget},  // SJ_NOVAEXPLOSING
    {2586, 1231, SkillLuaSlot::kCaster},  // SJ_BOOKOFDIMENSION
    {2588, 1219, SkillLuaSlot::kCaster},  // SJ_DOCUMENT
    {2590, 1197, SkillLuaSlot::kCaster},  // SJ_LIGHTOFSUN
    {2592, 1218, SkillLuaSlot::kCaster},  // SJ_SOLARBURST
    {2594, 1220, SkillLuaSlot::kTarget},  // SJ_FALLINGSTAR_ATK
    {2600, 1233, SkillLuaSlot::kTarget},  // SP_CURSEEXPLOSION
    {2602, 1204, SkillLuaSlot::kCaster},  // SP_SPA
    {2603, 547, SkillLuaSlot::kCaster},   // SP_SHA
    {2606, 1209, SkillLuaSlot::kTarget},  // SP_SOULDIVISION
    {2607, 1234, SkillLuaSlot::kCaster},  // SP_SOULREAPER
    {2609, 1209, SkillLuaSlot::kCaster},  // SP_SOULCOLLECT
    {2610, 1208, SkillLuaSlot::kTarget},  // SP_SOULEXPLOSION
    {2610, 1242, SkillLuaSlot::kTarget},  // SP_SOULEXPLOSION
    {3004, 996, SkillLuaSlot::kTarget},   // KO_JYUMONJIKIRI
    {3005, 997, SkillLuaSlot::kTarget},   // KO_SETSUDAN
    {3006, 983, SkillLuaSlot::kCaster},   // KO_BAKURETSU
    {3006, 990, SkillLuaSlot::kTarget},   // KO_BAKURETSU
    {3007, 981, SkillLuaSlot::kCaster},   // KO_HAPPOKUNAI
    {3007, 986, SkillLuaSlot::kCaster},   // KO_HAPPOKUNAI
    {3008, 330, SkillLuaSlot::kTarget},   // KO_MUCHANAGE
    {3009, 984, SkillLuaSlot::kGround},   // KO_HUUMARANKA
    {3009, 1000, SkillLuaSlot::kGround},  // KO_HUUMARANKA
    {3009, 1002, SkillLuaSlot::kGround},  // KO_HUUMARANKA
    {3011, 57, SkillLuaSlot::kBegin},     // KO_MEIKYOUSISUI
    {3013, 59, SkillLuaSlot::kBegin},     // KO_KYOUGAKU
    {3014, 54, SkillLuaSlot::kBegin},     // KO_JYUSATSU
    {3015, 55, SkillLuaSlot::kBegin},     // KO_KAHU_ENTEN
    {3016, 441, SkillLuaSlot::kBegin},    // KO_HYOUHU_HUBUKI
    {3017, 56, SkillLuaSlot::kBegin},     // KO_KAZEHU_SEIRAN
    {3018, 12, SkillLuaSlot::kBegin},     // KO_DOHU_KOUKAI
    {3019, 989, SkillLuaSlot::kCaster},   // KO_KAIHOU
    {3019, 1008, SkillLuaSlot::kTarget},  // KO_KAIHOU
    {3021, 55, SkillLuaSlot::kBegin},     // KO_GENWAKU
    {3022, 999, SkillLuaSlot::kCaster},   // KO_IZAYOI
    {3023, 991, SkillLuaSlot::kCaster},   // KG_KAGEHUMI
    {3024, 1001, SkillLuaSlot::kTarget},  // KG_KYOMU
    {3025, 169, SkillLuaSlot::kTarget},   // KG_KAGEMUSYA
    {3026, 1010, SkillLuaSlot::kTarget},  // OB_ZANGETSU
    {3026, 12, SkillLuaSlot::kBegin},     // OB_ZANGETSU
    {3027, 1011, SkillLuaSlot::kTarget},  // OB_OBOROGENSOU
    {3029, 1009, SkillLuaSlot::kTarget},  // OB_AKAITSUKI
    {3029, 55, SkillLuaSlot::kBegin},     // OB_AKAITSUKI
    {3031, 135, SkillLuaSlot::kTarget},   // ECL_SNOWFLIP
    {3032, 699, SkillLuaSlot::kTarget},   // ECL_PEONYMAMY
    {3033, 52, SkillLuaSlot::kTarget},    // ECL_SADAGUI
    {3034, 314, SkillLuaSlot::kTarget},   // ECL_SEQUOIADUST
    {3035, 12, SkillLuaSlot::kBegin},     // ECLAGE_RECALL
    {5050, 1123, SkillLuaSlot::kTarget},  // SU_PURRING
    {5051, 1098, SkillLuaSlot::kTarget},  // SU_SHRIMPARTY
    {5053, 1124, SkillLuaSlot::kTarget},  // SU_MEOWMEOW
}};

constexpr std::array<SkillLuaSoundEntry, 37> kSkillLuaSounds = {{
    {751, "", "effect\\EF_DecAgility.wav"},                   // NPC_ALL_STAT_DOWN
    {2554, "", "effect\\RL_BANISHING_BUSTER.wav"},            // RL_BANISHING_BUSTER
    {2555, "", "effect\\RL_B_TRAP.wav"},                      // RL_B_TRAP
    {2556, "effect\\RL_FLICKER.wav", ""},                     // RL_FLICKER
    {2557, "", "effect\\RL_S_STORM.wav"},                     // RL_S_STORM
    {2560, "", "effect\\RL_C_MARKER.wav"},                    // RL_C_MARKER
    {2561, "effect\\데스페라도.wav", ""},                     // RL_FIREDANCE
    {2566, "", "effect\\RL_D_TAIL.wav"},                      // RL_D_TAIL
    {2567, "", "effect\\RL_FIRE_RAIN.wav"},                   // RL_FIRE_RAIN
    {2569, "", "effect\\RL_AM_BLAST.wav"},                    // RL_AM_BLAST
    {2570, "", "effect\\RL_SLUGSHOT.wav"},                    // RL_SLUGSHOT
    {2574, "effect\\t_피링.wav", ""},                         // SJ_LIGHTOFMOON
    {2577, "effect\\t_피링.wav", ""},                         // SJ_LIGHTOFSTAR
    {2586, "effect\\so_elemental_shield.wav", ""},            // SJ_BOOKOFDIMENSION
    {2590, "effect\\t_피링.wav", ""},                         // SJ_LIGHTOFSUN
    {2592, "effect\\g데모닉화이어.wav", ""},                  // SJ_SOLARBURST
    {2594, "", "effect\\wizard_sightrasher.wav"},             // SJ_FALLINGSTAR_ATK
    {2602, "effect\\t_에너지방출.wav", ""},                   // SP_SPA
    {2606, "", "effect\\t_치잉.wav"},                         // SP_SOULDIVISION
    {2607, "effect\\ef_signum.wav", ""},                      // SP_SOULREAPER
    {2610, "", "effect\\hunter_landmine.wav"},                // SP_SOULEXPLOSION
    {3004, "effect\\cru_holy cross.wav", ""},                 // KO_JYUMONJIKIRI
    {3005, "", "effect\\T_전기.wav"},                         // KO_SETSUDAN
    {3006, "effect\\닌자_던지기.wav", "effect\\폭염룡.wav"},  // KO_BAKURETSU
    {3007, "effect\\T_회오리차기.wav", ""},                   // KO_HAPPOKUNAI
    {3008, "effect\\닌자_던지기.wav", ""},                    // KO_MUCHANAGE
    {3009, "effect\\T_회오리차기.wav", ""},                   // KO_HUUMARANKA
    {3019, "effect\\닌자_던지기.wav", ""},                    // KO_KAIHOU
    {3022, "effect\\ab_renovatio.wav", ""},                   // KO_IZAYOI
    {3024, "", "effect\\t_에너지방출.wav"},                   // KG_KYOMU
    {3025, "", "effect\\mon_금강불괴.wav"},                   // KG_KAGEMUSYA
    {3026, "", "effect\\t_따듯한마법.wav"},                   // OB_ZANGETSU
    {3027, "", "effect\\sign_up.wav"},                        // OB_OBOROGENSOU
    {3029, "", "effect\\t_에너지방출.wav"},                   // OB_AKAITSUKI
    {5050, "", "effect\\su_grooming.wav"},                    // SU_PURRING
    {5051, "", "effect\\su_shrimpparty.wav"},                 // SU_SHRIMPARTY
    {5053, "", "effect\\su_chattering.wav"},                  // SU_MEOWMEOW
}};

// Celles dont `onTarget` déplace l'effet de LANCEUR sur la cible.
constexpr std::array<std::uint16_t, 22> kSkillLuaOnTarget = {{
    751,   // NPC_ALL_STAT_DOWN
    754,   // NPC_IMMUNE_PROPERTY
    755,   // NPC_MOVE_COORDINATE
    2562,  // RL_H_MINE
    2602,  // SP_SPA
    2603,  // SP_SHA
    3004,  // KO_JYUMONJIKIRI
    3005,  // KO_SETSUDAN
    3006,  // KO_BAKURETSU
    3019,  // KO_KAIHOU
    3024,  // KG_KYOMU
    3025,  // KG_KAGEMUSYA
    3026,  // OB_ZANGETSU
    3027,  // OB_OBOROGENSOU
    3029,  // OB_AKAITSUKI
    3031,  // ECL_SNOWFLIP
    3032,  // ECL_PEONYMAMY
    3033,  // ECL_SADAGUI
    3034,  // ECL_SEQUOIADUST
    5050,  // SU_PURRING
    5051,  // SU_SHRIMPARTY
    5053,  // SU_MEOWMEOW
}};

}  // namespace

std::span<const SkillLuaEffectEntry> SkillLuaEffectTable() { return kSkillLuaEffects; }

std::span<const SkillLuaSoundEntry> SkillLuaSoundTable() { return kSkillLuaSounds; }

std::span<const std::uint16_t> SkillLuaOnTargetSkills() { return kSkillLuaOnTarget; }

std::span<const SkillLuaEffectEntry> SkillLuaEffectsFor(std::uint16_t skill_id, SkillLuaSlot slot) {
  const auto correspond = [skill_id, slot](const SkillLuaEffectEntry& entry) {
    return entry.skill_id == skill_id && entry.slot == slot;
  };
  const auto first = std::ranges::find_if(kSkillLuaEffects, correspond);
  if (first == kSkillLuaEffects.end()) {
    return {};
  }
  const auto last = std::find_if_not(first, kSkillLuaEffects.end(), correspond);
  return {first, last};
}

bool SkillLuaPutsCasterEffectOnTarget(std::uint16_t skill_id) {
  return std::ranges::binary_search(kSkillLuaOnTarget, skill_id);
}

SkillLuaSoundEntry SkillLuaSoundFor(std::uint16_t skill_id) {
  const auto found = std::ranges::find(kSkillLuaSounds, skill_id, &SkillLuaSoundEntry::skill_id);
  return found == kSkillLuaSounds.end() ? SkillLuaSoundEntry{} : *found;
}

}  // namespace ro::render
