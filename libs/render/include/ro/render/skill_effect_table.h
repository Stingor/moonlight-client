#pragma once

#include <cstdint>
#include <span>

namespace ro::render {

// SUR QUI l'effet d'une compétence naît quand le coup porte.
//
// Le client le décide compétence par compétence, et cela se voit : Sonic Blow
// s'entaille sur l'assassin, Cold Bolt tombe sur ce qu'il gèle. Jouer tout sur
// la cible met l'effet au mauvais bout d'un écran de distance.
enum class SkillEffectOn : std::uint8_t {
  // Sur qui reçoit le coup. C'est le cas le plus fréquent, et le défaut : la
  // table ne note que ce qui s'en écarte.
  kTarget,
  // Sur qui le porte.
  kCaster,
};

// Une entrée de la table des effets de compétence.
struct SkillEffectEntry {
  std::uint16_t skill_id = 0;
  std::uint16_t effect_id = 0;
  SkillEffectOn on = SkillEffectOn::kTarget;
};

// La table entière, triée par identifiant de compétence.
//
// ⚠ C'est la DONNÉE, pas la façon de s'en servir : ce qu'une compétence montre
// se demande à SkillImplFor, qui sait aussi ce qu'une table ne dit pas.
[[nodiscard]] std::span<const SkillEffectEntry> SkillEffectTable();

// CE QUE LE CHEMIN DES DÉGÂTS JOUE, et lui seul.
//
// 🔴 UNE COMPÉTENCE A JUSQU'À TROIS EFFETS, UN PAR CHEMIN, et les confondre les
// joue au mauvais moment. Magnum Break montre ses anneaux UNE FOIS, à
// l'annonce du sort, par le répartiteur des compétences sans dégâts ; ce que
// son chemin des DÉGÂTS pose est autre chose, l'anneau de choc ordinaire, et
// une fois par victime. Prendre l'effet d'annonce pour celui du coup dessinait
// les anneaux de Magnum Break sur chaque créature touchée.
//
// Cette table-ci ne réunit donc QUE les deux sources du coup : le répartiteur
// des dégâts et la carte des effets visés. Une compétence absente ne montre
// rien à l'impact, et c'est une réponse.
struct SkillDamageEffectEntry {
  std::uint16_t skill_id = 0;
  std::uint16_t effect_id = 0;
  // ⚠ UN EFFET VISÉ NAÎT SUR LA SOURCE DU COUP, jamais sur la cible : le client
  // le fait naître sur l'acteur qui reçoit le message 37, puis lui
  // donne la position de la cible par une commande. Pour une compétence posée
  // au sol, cette source est l'UNITÉ, donc la case visée — c'est ce qui met le
  // blizzard de Storm Gust là où le joueur a cliqué, et non sur ses victimes.
  SkillEffectOn on = SkillEffectOn::kCaster;
};

// Triée par identifiant de compétence.
[[nodiscard]] std::span<const SkillDamageEffectEntry> SkillDamageEffectTable();

// L'entrée de cette compétence, ou nul quand le coup ne montre rien.
[[nodiscard]] const SkillDamageEffectEntry* SkillDamageEffectOf(std::uint16_t skill_id);

// ET CE QUE L'ANNONCE JOUE, l'autre moitié de la même règle.
//
// 🔴 HEAL A LES DEUX, ET ILS DIFFÈRENT : il s'annonce par un effet et frappe
// les morts-vivants par un autre, EF_HEAL3. Une table unique donnerait la
// préséance au coup, si bien qu'un soin ordinaire montrerait l'effet du coup.
// Le répartiteur d'annonce du client, lui, ne connaît que sa propre source.
struct SkillAnnouncedEffectEntry {
  std::uint16_t skill_id = 0;
  std::uint16_t effect_id = 0;
  // Sur le LANCEUR ou sur qui reçoit : ici chaque entrée le dit en clair, le
  // répartiteur d'annonce nommant l'un ou l'autre.
  SkillEffectOn on = SkillEffectOn::kTarget;
};

// Triée par identifiant de compétence.
[[nodiscard]] std::span<const SkillAnnouncedEffectEntry> SkillAnnouncedEffectTable();

// L'entrée de cette compétence, ou nul quand son annonce ne montre rien.
[[nodiscard]] const SkillAnnouncedEffectEntry* SkillAnnouncedEffectOf(std::uint16_t skill_id);

}  // namespace ro::render
