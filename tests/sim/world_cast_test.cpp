#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/sim/movement.h"
#include "ro/sim/skill_banner.h"
#include "ro/sim/world.h"

using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::DirectionTowards;
using ro::sim::World;

// L'INCANTATION, côté MONDE : ce qu'un lanceur garde d'un sort qu'il prépare,
// et comment il s'oriente.
//
// Sorti de `world_test.cpp`, qui touchait son plafond de lignes. La frontière
// est celle du SUJET : tout ce qui suit parle d'un seul champ de l'acteur —
// `Actor::cast` — et de ce que le serveur en dit.

TEST_CASE("Monde : une incantation se note, et se tourne vers sa cible", "[sim][world]") {
  // Le serveur ne l'annonce qu'au DÉBUT : rien ne dit qu'elle aboutit, et le
  // monde ne lit aucune horloge. Il garde donc l'instant et la durée, et c'est
  // à qui la montre de la voir expirer.
  ro::sim::World world;
  ro::sim::Actor caster;
  caster.id = 10;
  caster.position = CellPosition{100, 100, 0};
  world.Spawn(caster);
  ro::sim::Actor target;
  target.id = 20;
  target.position = CellPosition{100, 110, 0};
  world.Spawn(target);

  ro::sim::Cast cast;
  cast.skill_id = 14;
  cast.target_id = 20;
  cast.element = 1;
  cast.started_ms = 5000;
  cast.duration_ms = 1200;
  REQUIRE(world.RecordCast(10, cast));

  const ro::sim::Actor* found = world.Find(10);
  REQUIRE(found != nullptr);
  REQUIRE(found->cast.has_value());
  CHECK(found->cast->skill_id == 14);
  CHECK(found->cast->duration_ms == 1200);
  CHECK(found->cast->started_ms == 5000);
  // Le lanceur s'est tourné vers ce qu'il vise, comme pour un coup.
  CHECK(found->position.direction ==
        DirectionTowards(CellPosition{100, 100, 0}, CellPosition{100, 110, 0}, 0));

  // Une seconde incantation remplace la première : le serveur n'annonce pas la
  // fin de celle d'avant.
  ro::sim::Cast other;
  other.skill_id = 21;
  other.started_ms = 9000;
  other.duration_ms = 500;
  REQUIRE(world.RecordCast(10, other));
  CHECK(world.Find(10)->cast->skill_id == 21);

  // 🔴 ET UN SORT AU SOL TOURNE AUSSI SON LANCEUR, vers la CASE qu'il vise.
  // Le client fait le même geste dans les DEUX branches d'un sort au sol ;
  // n'en porter qu'une laisserait un mage lancer Storm Gust en tournant le
  // dos à sa zone.
  ro::sim::Cast on_ground;
  on_ground.skill_id = 89;  // WZ_STORMGUST
  on_ground.target_id = 0;
  on_ground.target_cell = CellPosition{80, 100, 0};
  on_ground.duration_ms = 4400;
  REQUIRE(world.RecordCast(10, on_ground));
  CHECK(world.Find(10)->position.direction ==
        DirectionTowards(CellPosition{100, 100, 0}, CellPosition{80, 100, 0}, 0));

  // ⚠ LA CASE ZÉRO N'EST PAS UNE CASE VISÉE mais un champ vide : un sort sur
  // soi-même n'en porte aucune, et s'y tourner ferait pivoter le lanceur vers
  // le coin de la carte.
  const std::uint8_t garde = world.Find(10)->position.direction;
  ro::sim::Cast on_self;
  on_self.skill_id = 34;
  on_self.duration_ms = 1000;
  REQUIRE(world.RecordCast(10, on_self));
  CHECK(world.Find(10)->position.direction == garde);

  // Personne à cet identifiant : rien à noter.
  CHECK_FALSE(world.RecordCast(999, cast));
}

TEST_CASE("Monde : une incantation interrompue s'arrête là où elle en est", "[sim][world]") {
  // C'est le SEUL paquet qui parle de la fin d'une incantation, et il n'arrive
  // que si elle est coupée. Il arrive aussi pour qui n'incantait plus.
  ro::sim::World world;
  ro::sim::Actor caster;
  caster.id = 3;
  world.Spawn(caster);

  CHECK_FALSE(world.CancelCast(3, 1000));
  CHECK_FALSE(world.CancelCast(999, 1000));

  ro::sim::Cast cast;
  cast.skill_id = 21;
  cast.started_ms = 1000;
  cast.duration_ms = 800;
  REQUIRE(world.RecordCast(3, cast));
  // 🔴 ELLE RESTE CONNUE, FINIE : le repos qui suit doit savoir qu'on quitte
  // une incantation — le client en prend la garde —, et ce qui la montre la
  // voit échue. Une durée ramenée à zéro dirait « instantanée ».
  CHECK(world.CancelCast(3, 1300));
  REQUIRE(world.Find(3)->cast.has_value());
  CHECK(world.Find(3)->cast->started_ms == 1000);
  CHECK(world.Find(3)->cast->duration_ms == 300);
  // Et elle se sait interrompue : ses effets s'éteignent net, quand la fin
  // normale les laisse finir.
  CHECK(world.Find(3)->cast->cancelled);
  // Une seconde interruption la trouve encore, et n'y change rien.
  CHECK(world.CancelCast(3, 1500));
  CHECK(world.Find(3)->cast->duration_ms == 300);

  // Coupée à l'instant même de son début, elle garde une milliseconde.
  REQUIRE(world.RecordCast(3, cast));
  CHECK(world.CancelCast(3, 1000));
  CHECK(world.Find(3)->cast->duration_ms == 1);
}

TEST_CASE("Monde : la posture d'emploi va au lanceur, pas à la cible", "[sim][world]") {
  // 🔴 LE SOIN SE RANGE SUR QUI LE REÇOIT, mais c'est le PRÊTRE qui fait le
  // geste. Lire la posture sur la cible faisait lever les bras à l'allié.
  constexpr std::uint32_t kPriest = 1;
  constexpr std::uint32_t kAlly = 2;
  constexpr std::uint16_t kHeal = 28;
  World world;
  ro::sim::Actor priest;
  priest.id = kPriest;
  world.Spawn(priest);
  ro::sim::Actor ally;
  ally.id = kAlly;
  world.Spawn(ally);

  ro::sim::SupportHit heal;
  heal.skill_id = kHeal;
  heal.source_id = kPriest;
  heal.started_ms = 500;
  REQUIRE(world.RecordSupport(kAlly, heal));

  REQUIRE(world.Find(kPriest)->skill_use.has_value());
  CHECK(world.Find(kPriest)->skill_use->skill_id == kHeal);
  CHECK(world.Find(kPriest)->skill_use->started_ms == 500);
  CHECK_FALSE(world.Find(kAlly)->skill_use.has_value());
  // Le soin, lui, reste sur qui le reçoit : son nombre et son effet s'y lisent.
  CHECK(world.Find(kAlly)->support.has_value());
}

TEST_CASE("Monde : une compétence posée au sol donne sa posture au lanceur", "[sim][world]") {
  // `ZC_NOTIFY_GROUNDSKILL` n'avait aucun geste : le lanceur restait planté.
  constexpr std::uint32_t kWizard = 7;
  constexpr std::uint16_t kSafetyWall = 12;
  World world;
  ro::sim::Actor wizard;
  wizard.id = kWizard;
  world.Spawn(wizard);

  ro::sim::GroundSkillUse wall;
  wall.skill_id = kSafetyWall;
  wall.cell = CellPosition{20, 30, 0};
  wall.started_ms = 900;
  REQUIRE(world.RecordGroundSkill(kWizard, wall));

  REQUIRE(world.Find(kWizard)->skill_use.has_value());
  CHECK(world.Find(kWizard)->skill_use->skill_id == kSafetyWall);
  CHECK(world.Find(kWizard)->skill_use->started_ms == 900);
}

TEST_CASE("Monde : Meteor Storm laisse au lanceur la motion qu'il avait", "[sim][world]") {
  // 🔴 ONZE COMPÉTENCES DE CASE NE POSENT PAS LA MOTION D'EMPLOI : le client
  // n'y touche pas, et n'impose qu'une image à la motion en cours. Le lanceur
  // levait les bras d'un emploi ordinaire.
  constexpr std::uint32_t kWizard = 8;
  constexpr std::uint16_t kMeteorStorm = 83;
  World world;
  ro::sim::Actor wizard;
  wizard.id = kWizard;
  world.Spawn(wizard);
  REQUIRE(world.Turn(kWizard, 0, 1));

  ro::sim::GroundSkillUse storm;
  storm.skill_id = kMeteorStorm;
  storm.cell = CellPosition{20, 30, 0};
  storm.started_ms = 900;
  REQUIRE(world.RecordGroundSkill(kWizard, storm));
  const ro::sim::Actor* caster = world.Find(kWizard);
  CHECK_FALSE(caster->skill_use.has_value());
  REQUIRE(caster->ground_skill.has_value());
  CHECK(caster->ground_skill->skill_id == kMeteorStorm);
  // Le regard reçu avant compte ; celui qui suit rend l'image de la tête.
  CHECK(caster->turns_received_at_ground_skill == caster->turns_received);
  REQUIRE(world.Turn(kWizard, 0, 2));
  CHECK(world.Find(kWizard)->turns_received != caster->turns_received_at_ground_skill);
  CHECK(ro::sim::GroundSkillKeepsCasterMotion(kMeteorStorm));
  CHECK_FALSE(ro::sim::GroundSkillKeepsCasterMotion(12));
}
