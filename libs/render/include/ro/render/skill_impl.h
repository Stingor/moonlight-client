#pragma once

#include <cstdint>
#include <optional>

#include "ro/render/skill_cast_table.h"
#include "ro/render/skill_effect_table.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {

// CE QU'UNE COMPÉTENCE MONTRE — une classe par comportement.
//
// POURQUOI UNE CLASSE ET NON UNE TABLE DE PLUS. Le client ne traite pas les
// compétences uniformément : il en range quelques centaines dans un répartiteur
// où chacune a son cas, et ce cas fait ce qu'il veut — deux effets au lieu
// d'un, l'un sur le lanceur et l'autre sur la cible, pas de barre
// d'incantation, un son à lui. Une table par question donne autant de tables
// parallèles qu'il y a de questions, et rien ne dit plus qu'elles parlent de la
// même compétence.
//
// Le serveur a fait ce chemin avant nous : rAthena a remplacé ses switchs
// géants par src/map/skills/, une classe par compétence rendue par une
// fabrique. C'est la même forme ici, et pour la même raison.
//
// ⚠ La DONNÉE reste engendrée. Ces classes ne recopient pas la table : le cas
// général le lit, et seules les compétences dont le comportement échappe à une
// table ont une classe à elles. Une classe par compétence écrite à la main
// serait une seconde source de vérité, qui dériverait.
class SkillImpl {
 public:
  explicit SkillImpl(std::uint16_t skill_id) : skill_id_(skill_id) {}
  SkillImpl(const SkillImpl&) = delete;
  SkillImpl& operator=(const SkillImpl&) = delete;
  virtual ~SkillImpl() = default;

  [[nodiscard]] std::uint16_t skill_id() const noexcept { return skill_id_; }

  // L'effet à jouer quand le coup porte, ou ZÉRO pour une compétence dont nous
  // ne savons rien montrer.
  //
  // ⚠ Zéro ne veut pas dire « sans effet ». Le client en a un pour presque
  // tout ; il est seulement PROCÉDURAL — des particules qu'il dessine en code,
  // sans fichier — pour la plupart. Nous ne rendons que les fichiers.
  [[nodiscard]] virtual std::uint16_t EffectId() const { return 0; }

  // Sur qui cet effet naît.
  [[nodiscard]] virtual SkillEffectOn EffectOn() const { return SkillEffectOn::kTarget; }

  // Vrai si l'incantation de cette compétence montre une barre.
  //
  // Une durée NULLE n'en montre jamais : le serveur annonce l'incantation même
  // d'un sort instantané. Quelques compétences n'en montrent pas non plus
  // malgré une durée annoncée, et c'est là que la table ne suffit plus.
  [[nodiscard]] virtual bool ShowsCastBar(std::uint32_t duration_ms) const {
    return duration_ms != 0;
  }

  // L'EFFET que l'incantation fait naître sur le lanceur, avant tout dégât.
  //
  // ⚠ Ce n'est PAS EffectId : celui-là naît quand le coup porte, celui-ci
  // pendant qu'on prépare le sort. Une compétence en a souvent un des deux
  // seulement — Sonic Blow n'a rien à l'incantation et tout aux dégâts.
  //
  // kCastEffectElemental n'est pas un effet à jouer : c'est la demande de
  // l'aura élémentaire, que CastEffectForElement résout.
  [[nodiscard]] virtual std::int16_t CastEffectId() const { return kCastEffectDefault; }

  // Vrai si l'incantation fait naître l'effet ci-dessus — ou celui que le Lua
  // nomme à sa place.
  //
  // ⚠ LA POSE, ELLE, SE PREND QUAND MÊME : le client résout l'effet et l'état
  // ensemble, pose l'état, puis refuse de faire naître l'effet pour les
  // compétences qui répondent faux ici.
  [[nodiscard]] virtual bool CastSpawnsEffect() const { return true; }

  // La POSE que le lanceur prend le temps de son incantation, telle que le
  // client la numérote. kCastStateNone veut dire qu'il n'en change pas.
  [[nodiscard]] virtual std::int8_t CastState() const { return kCastStateDefault; }

 private:
  std::uint16_t skill_id_ = 0;
};

// L'effet d'aura que l'élément annoncé donne à une incantation.
//
// Rend l'effet tel quel quand il n'est pas kCastEffectElemental, et quand
// l'élément n'a pas d'aura à lui : c'est ce que fait le client, qui laisse
// alors l'effet générique.
[[nodiscard]] std::int16_t CastEffectForElement(std::int16_t cast_effect_id, std::uint32_t element);

// L'effet d'incantation de `skill_id` pour un lanceur de cette classe
// d'affichage : celui que `CastEffectJobOverrideTable` lui donne, ou
// `cast_effect_id` tel quel.
[[nodiscard]] std::int16_t CastEffectForJob(std::uint16_t skill_id, std::int16_t cast_effect_id,
                                            std::uint32_t job);

// Le rang d'animation qu'un état d'action joue, ou RIEN quand l'acteur garde la
// posture qu'il avait.
//
// ⚠ ELLE SERT AUX DEUX CHEMINS, l'INCANTATION et l'EMPLOI : c'est la même table
// d'états chez le client, et lui en donner deux ici les ferait diverger à la
// première correction.
//
// ⚠ LE MÉTIER DÉCIDE POUR TROIS ÉTATS, dont le SEPT, que 1 088 compétences
// prennent par défaut au chemin de l'emploi. Un barde, une danseuse, un ninja
// ou un canonnier y jouent la garde et non l'incantation ; les trois métiers
// montés y jouent l'attaque. Passer zéro donne le rang commun.
//
// ⚠ Une CRÉATURE n'a pas cette table : rien ici. Ce qu'un état lui pose — le
// plus souvent rien, qu'elle continue ce qu'elle jouait — est l'affaire de
// `CreatureActionAnimationFor`.
[[nodiscard]] std::optional<std::size_t> ActionAnimationFor(std::int8_t action_state,
                                                            SpriteLayout layout,
                                                            std::uint32_t job_id);

// La compétence que ce numéro désigne.
//
// JAMAIS NULLE. Une compétence dont on ne sait rien rend l'implémentation par
// défaut, qui ne montre aucun effet et laisse voir sa barre d'incantation :
// c'est ce qui évite à chaque appelant de redire le même « si nul, alors ».
[[nodiscard]] const SkillImpl& SkillImplFor(std::uint32_t skill_id);

}  // namespace ro::render
