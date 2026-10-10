#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ro::render {

// LES EFFETS QUE SOIXANTE-SIX COMPÉTENCES TIENNENT DE LEUR LUA.
//
// 🔴 CES COUPLES NE SONT PAS ÉCRITS EN DUR DANS LE CLIENT, et rien ne signale
// qu'il manque quelque chose : le défaut du répartiteur d'emploi va les
// chercher dans un fichier de l'installation, et un défaut a l'air d'un défaut.
// C'est la seule de nos tables d'effets qui vienne d'un fichier de données.
//
// Le fichier est en clair dans `moonlight.grf` :
//
//     data\luafiles514\lua files\skilleffectinfo\skilleffectinfolist.lub
//
// ⚠ IL EST EN CP949, l'encodage coréen : la moitié des noms de fichiers son y
// sont écrits en coréen, et les lire en latin-1 donne des octets qu'aucun VFS
// ne retrouve.
//
// QUATRE CHEMINS DU CLIENT L'INTERROGENT, et chacun ne lit qu'une part :
//
//   · `effectID`, `waveFileName`, `onTarget` : les effets du LANCEUR,
//     qu'`onTarget` déplace sur la cible ;
//   · `targetEffectID`, `targetWaveFileName` : ceux de la CIBLE ;
//   · `groundEffectID` : ceux du SOL ;
//   · `beginEffectID`, la première source de la cascade d'incantation, celle
//     qui prime sur les douze répartiteurs par compétence.
//
// ⚠ QUATRE CHAMPS DE CE LUA NE SONT PAS LUS PAR LE CLIENT : `toTargetEffectID`,
// `toTargetEffectNum`, `beginMotionType` et `LaunchZC_USE_SKILL`. Ils sont lus
// par d'autres fichiers Lua, pas par le C++. Cette table ne les porte pas, et
// ce n'est donc pas un trou.

// D'où part un effet de ce Lua.
enum class SkillLuaSlot : std::uint8_t {
  // `effectID` — sur le lanceur, ou sur la cible si `onTarget`.
  kCaster,
  // `targetEffectID` — toujours sur ce qui reçoit.
  kTarget,
  // `groundEffectID` — sur la case.
  kGround,
  // `beginEffectID` — pendant l'incantation.
  kBegin,
};

// Un effet, sa compétence, et d'où il part.
struct SkillLuaEffectEntry {
  std::uint16_t skill_id = 0;
  std::uint16_t effect_id = 0;
  SkillLuaSlot slot = SkillLuaSlot::kCaster;
};

// Les sons de ce Lua. Ils ne passent PAS par `skill_sound_table`, qui porte les
// sons que le client écrit en dur : aucune de ces soixante-six compétences n'y
// figure.
struct SkillLuaSoundEntry {
  std::uint16_t skill_id = 0;
  // `waveFileName`, joué par le chemin du lanceur et par celui du sol. Vide
  // quand la fiche n'en donne pas.
  std::string_view caster_wave;
  // `targetWaveFileName`, joué par le chemin de la cible.
  std::string_view target_wave;
};

// La table entière, triée par compétence puis par emplacement.
[[nodiscard]] std::span<const SkillLuaEffectEntry> SkillLuaEffectTable();

// Les sons, triés par compétence.
[[nodiscard]] std::span<const SkillLuaSoundEntry> SkillLuaSoundTable();

// LES COMPÉTENCES DONT L'EFFET DE LANCEUR SE POSE SUR LA CIBLE (`onTarget`).
//
// ⚠ CE DRAPEAU NE DÉPLACE QUE L'EMPLACEMENT `kCaster`. Les effets de cible ont
// déjà le leur, et les confondre poserait deux fois le même sur la victime.
[[nodiscard]] std::span<const std::uint16_t> SkillLuaOnTargetSkills();

// Ce que cette compétence tient de son Lua pour cet emplacement. Vide pour les
// milliers d'autres.
[[nodiscard]] std::span<const SkillLuaEffectEntry> SkillLuaEffectsFor(std::uint16_t skill_id,
                                                                      SkillLuaSlot slot);

// Si l'effet de lanceur de cette compétence se pose sur la cible.
[[nodiscard]] bool SkillLuaPutsCasterEffectOnTarget(std::uint16_t skill_id);

// Les deux sons de cette compétence, vides quand elle n'en a pas.
[[nodiscard]] SkillLuaSoundEntry SkillLuaSoundFor(std::uint16_t skill_id);

}  // namespace ro::render
