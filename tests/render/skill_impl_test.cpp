#include "ro/render/skill_impl.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/cast_bar.h"
#include "ro/render/effect_table.h"

// LA FABRIQUE DES COMPÉTENCES : ce qu'une compétence montre, et sur qui.
using ro::render::EffectFile;
using ro::render::kBowlingBashSkill;
using ro::render::kBrandishSpearSkill;
using ro::render::SkillEffectEntry;
using ro::render::SkillEffectOn;
using ro::render::SkillEffectTable;
using ro::render::SkillImpl;
using ro::render::SkillImplFor;

TEST_CASE("Compétences : une compétence inconnue répond quand même", "[render][skill]") {
  // JAMAIS de nul. Une compétence dont on ne sait rien ne montre aucun effet et
  // laisse voir sa barre : c'est ce qui évite à chaque appelant de redire le
  // même « si nul, alors ».
  const SkillImpl& unknown = SkillImplFor(60000);
  CHECK(unknown.EffectId() == 0);
  CHECK(unknown.EffectOn() == SkillEffectOn::kTarget);
  CHECK(unknown.ShowsCastBar(1000));
  CHECK_FALSE(unknown.ShowsCastBar(0));

  // Zéro n'est la compétence de personne.
  CHECK(SkillImplFor(0).EffectId() == 0);
}

TEST_CASE("Compétences : le cas général répond ce que la table dit", "[render][skill]") {
  // La fabrique ne recopie pas le relevé : elle le lit. Une entrée qui ne se
  // retrouverait pas ici serait une seconde source de vérité.
  for (const SkillEffectEntry& entry : SkillEffectTable()) {
    const SkillImpl& skill = SkillImplFor(entry.skill_id);
    CHECK(skill.skill_id() == entry.skill_id);
    CHECK(skill.EffectId() == entry.effect_id);
    CHECK(skill.EffectOn() == entry.on);
  }
}

TEST_CASE("Compétences : l'effet naît sur le lanceur ou sur la cible", "[render][skill]") {
  // Le client ne les traite pas pareil, et cela se voit : Sonic Blow s'entaille
  // sur l'assassin, Cold Bolt tombe sur ce qu'il gèle. Tout jouer sur la cible
  // met l'effet au mauvais bout d'un écran de distance.
  const SkillImpl& sonic_blow = SkillImplFor(136);
  CHECK(sonic_blow.EffectOn() == SkillEffectOn::kCaster);
  CHECK(EffectFile(sonic_blow.EffectId()) == "SonicBlow.str");

  const SkillImpl& pierce = SkillImplFor(56);
  CHECK(pierce.EffectOn() == SkillEffectOn::kCaster);
  CHECK(EffectFile(pierce.EffectId()) == "Pierce.str");

  const SkillImpl& fire_pillar = SkillImplFor(80);
  CHECK(fire_pillar.EffectOn() == SkillEffectOn::kTarget);
  CHECK(EffectFile(fire_pillar.EffectId()) == "FirePillar.str");

  const SkillImpl& thunderstorm = SkillImplFor(21);
  CHECK(thunderstorm.EffectOn() == SkillEffectOn::kTarget);
  CHECK(EffectFile(thunderstorm.EffectId()) == "ThunderStorm.str");
}

TEST_CASE("Compétences : deux d'entre elles ne montrent jamais leur barre", "[render][skill]") {
  // Le serveur donne une durée à Brandish Spear et à Bowling Bash, et le client
  // ne montre rien : leur temps annoncé est un délai d'après-coup. C'est
  // exactement ce qu'une table ne sait pas dire, et pourquoi la compétence est
  // une classe.
  CHECK_FALSE(SkillImplFor(kBrandishSpearSkill).ShowsCastBar(1000));
  CHECK_FALSE(SkillImplFor(kBowlingBashSkill).ShowsCastBar(1000));

  // Elles gardent leur effet : ne pas montrer de barre n'est pas ne rien
  // montrer.
  CHECK(SkillImplFor(kBrandishSpearSkill).EffectId() != 0);
  CHECK(SkillImplFor(kBowlingBashSkill).EffectId() != 0);

  // Les voisines immédiates, elles, en ont une : ce sont bien DEUX numéros
  // isolés, et non un intervalle.
  CHECK(SkillImplFor(kBrandishSpearSkill - 1).ShowsCastBar(1000));
  CHECK(SkillImplFor(kBrandishSpearSkill + 1).ShowsCastBar(1000));
  CHECK(SkillImplFor(kBowlingBashSkill - 1).ShowsCastBar(1000));
  CHECK(SkillImplFor(kBowlingBashSkill + 1).ShowsCastBar(1000));

  // Une durée NULLE n'en montre jamais, quelle que soit la compétence : le
  // serveur annonce l'incantation même d'un sort instantané.
  CHECK_FALSE(SkillImplFor(14).ShowsCastBar(0));
  CHECK(SkillImplFor(14).ShowsCastBar(1000));
}

TEST_CASE("Compétences : la même demandée deux fois rend la même", "[render][skill]") {
  // La fabrique se construit UNE fois : deux appels ne doivent pas donner deux
  // objets, sans quoi comparer des adresses mentirait.
  CHECK(&SkillImplFor(136) == &SkillImplFor(136));
  CHECK(&SkillImplFor(60000) == &SkillImplFor(60001));
}
