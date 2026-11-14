#include "ro/sim/damage_meter.h"

#include <cstdint>
#include <limits>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/sim/combat_notice.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::CellPosition;
using ro::sim::World;

constexpr std::uint32_t kSelf = 7;
constexpr ro::sim::MeterMode kDone = ro::sim::MeterMode::kDamageDone;

namespace {

Actor Someone(std::uint32_t id, CellPosition where) {
  Actor actor;
  actor.id = id;
  actor.kind = ActorKind::kPlayer;
  actor.position = where;
  actor.name = "quelqu'un";
  return actor;
}

}  // namespace

TEST_CASE("Compteur de dégâts : le rythme se lit sur la fenêtre glissante", "[sim][dpsmeter]") {
  ro::sim::DamageMeter meter;
  meter.SetSelf(kSelf);
  meter.settings().window_secs = 10;
  // Mille dégâts d'un coup, lus sur dix secondes : cent par seconde.
  meter.Record(1000, 1000);
  meter.Tick(1000);
  CHECK(meter.rate(kDone) == Catch::Approx(100.0F));
  CHECK(meter.total(kDone) == 1000);
  CHECK(meter.in_combat());

  // Le même coup deux fois double le rythme, tant qu'il tient dans la fenêtre.
  meter.Record(1000, 2000);
  meter.Tick(2000);
  CHECK(meter.rate(kDone) == Catch::Approx(200.0F));

  // ⚠ ET IL RETOMBE QUAND LE COUP SORT DE LA FENÊTRE, sans qu'un nouveau coup
  // soit nécessaire : c'est une fenêtre GLISSANTE, pas une moyenne de combat.
  meter.Tick(11500);
  CHECK(meter.rate(kDone) == Catch::Approx(100.0F));
}

TEST_CASE("Compteur de dégâts : un silence ferme le combat", "[sim][dpsmeter]") {
  ro::sim::DamageMeter meter;
  meter.SetSelf(kSelf);
  meter.settings().combat_timeout_secs = 5;
  meter.Record(500, 1000);
  meter.Tick(1000);
  REQUIRE(meter.in_combat());
  CHECK(meter.combat_ms(3000) == 2000);

  // Cinq secondes pile ne suffisent pas : le client compare par DÉPASSEMENT.
  meter.Tick(6000);
  CHECK(meter.in_combat());
  meter.Tick(6001);
  CHECK_FALSE(meter.in_combat());
  CHECK(meter.combat_ms(6001) == 0);

  // Le coup suivant ouvre un AUTRE combat : son total repart de zéro, et c'est
  // ce qui fait qu'un compteur dit le rythme de CE combat-ci.
  CHECK(meter.total(kDone) == 500);
  meter.Record(300, 20000);
  CHECK(meter.total(kDone) == 300);
  CHECK(meter.in_combat());
}

TEST_CASE("Compteur de dégâts : le pic est le même rythme que celui qu'on lit", "[sim][dpsmeter]") {
  // 🔴 CE N'EST PAS LE PIC D'UNE COLONNE. Une colonne fait deux dixièmes de
  // seconde, et son rythme ramené à la seconde gonflerait un coup unique de
  // cinq fois.
  ro::sim::DamageMeter meter;
  meter.SetSelf(kSelf);
  meter.settings().window_secs = 10;
  meter.settings().slot_ms = 200;
  meter.Record(1000, 1000);
  meter.Tick(1000);
  CHECK(meter.peak(kDone) == Catch::Approx(100.0F));
  CHECK(meter.peak(kDone) == Catch::Approx(meter.rate(kDone)));

  // Il ne redescend pas avec le rythme : c'est le plus haut ATTEINT.
  meter.Tick(1200);
  CHECK(meter.peak(kDone) == Catch::Approx(100.0F));
}

TEST_CASE("Compteur de dégâts : une colonne fine n'ampute pas la fenêtre", "[sim][dpsmeter]") {
  // 🔴 CENT CINQUANTE COLONNES DE CINQUANTE MILLISECONDES ne couvrent que sept
  // secondes et demie : un coup de la fenêtre glissante de dix secondes doit
  // rester compté, même sorti du graphique.
  ro::sim::DamageMeter meter;
  meter.SetSelf(kSelf);
  meter.settings().slot_ms = 50;
  meter.settings().window_secs = 10;
  meter.settings().combat_timeout_secs = 15;
  meter.Record(1000, 1000);
  meter.Record(1000, 9000);
  meter.Tick(10000);
  CHECK(meter.rate(kDone) == Catch::Approx(200.0F));
}

TEST_CASE("Compteur de dégâts : les colonnes n'avancent qu'en combat", "[sim][dpsmeter]") {
  ro::sim::DamageMeter meter;
  meter.SetSelf(kSelf);
  meter.settings().slot_ms = 200;
  // ⚠ SANS UN PREMIER COUP, RIEN N'AVANCE : sinon la première rafale
  // remplirait d'un coup tout le tampon de vide.
  meter.Tick(100000);
  CHECK(meter.plot_offset(kDone) == 0);

  meter.Record(400, 1000);
  // Une colonne de deux dixièmes de seconde, quatre cents dégâts dedans : le
  // rythme de la colonne est ramené à la seconde, donc deux mille.
  meter.Tick(1200);
  REQUIRE(meter.plot_offset(kDone) == 1);
  CHECK(meter.plot(kDone)[0] == Catch::Approx(2000.0F));
  // La colonne suivante est vide, et le dit.
  meter.Tick(1400);
  REQUIRE(meter.plot_offset(kDone) == 2);
  CHECK(meter.plot(kDone)[1] == Catch::Approx(0.0F));
}

TEST_CASE("Compteur de dégâts : ce qui n'est pas un coup ne compte pas", "[sim][dpsmeter]") {
  using ro::sim::DamageActionCounts;
  // Ramasser un objet, s'asseoir, se lever, esquiver : le serveur les numérote
  // dans le même champ que les coups.
  CHECK_FALSE(DamageActionCounts(1));
  CHECK_FALSE(DamageActionCounts(2));
  CHECK_FALSE(DamageActionCounts(3));
  CHECK_FALSE(DamageActionCounts(11));
  CHECK(DamageActionCounts(0));
  CHECK(DamageActionCounts(8));
  CHECK(DamageActionCounts(10));

  // Et un coup à zéro dégât n'ouvre pas de combat.
  ro::sim::DamageMeter meter;
  meter.SetSelf(kSelf);
  meter.Record(0, 1000);
  CHECK_FALSE(meter.in_combat());
  CHECK(meter.total(kDone) == 0);
}

TEST_CASE("Compteur de dégâts : le monde compte NOS coups, et eux seuls", "[sim][dpsmeter]") {
  World world;
  world.set_player_id(7);
  world.Spawn(Someone(7, CellPosition{5, 5, 0}));
  world.Spawn(Someone(9, CellPosition{6, 5, 0}));

  ro::sim::Strike mien;
  mien.attacker = 7;
  mien.target = 9;
  mien.damage = 250;
  mien.started_ms = 1000;
  REQUIRE(world.RecordStrike(mien));
  world.damage_meter().Tick(1000);
  CHECK(world.damage_meter().total(kDone) == 250);

  // Le coup d'un autre ne nous est pas attribué.
  ro::sim::Strike own;
  own.attacker = 9;
  own.target = 7;
  own.damage = 999;
  own.started_ms = 1100;
  REQUIRE(world.RecordStrike(own));
  CHECK(world.damage_meter().total(kDone) == 250);

  // ⚠ ET LA SECONDE MAIN COMPTE AUSSI : le serveur la dit à part, et la
  // laisser de côté sous-estimerait tout combattant à deux armes.
  ro::sim::Strike two_hands;
  two_hands.attacker = 7;
  two_hands.target = 9;
  two_hands.damage = 100;
  two_hands.second_damage = 60;
  two_hands.started_ms = 1200;
  REQUIRE(world.RecordStrike(two_hands));
  CHECK(world.damage_meter().total(kDone) == 410);
}

TEST_CASE("Compteur de dégâts : deux mains énormes ne débordent pas", "[sim][dpsmeter]") {
  // La somme des deux mains dépasse un entier 32 bits signé — un comportement
  // indéfini, que l'essai sous UBSan relèverait. Elle se compte sur 64 bits.
  World world;
  world.set_player_id(7);
  world.Spawn(Someone(7, CellPosition{5, 5, 0}));
  world.Spawn(Someone(9, CellPosition{6, 5, 0}));

  ro::sim::Strike enorme;
  enorme.attacker = 7;
  enorme.target = 9;
  enorme.damage = std::numeric_limits<std::int32_t>::max();
  enorme.second_damage = std::numeric_limits<std::int32_t>::max();
  enorme.started_ms = 1000;
  REQUIRE(world.RecordStrike(enorme));
  world.damage_meter().Tick(1000);
  CHECK(world.damage_meter().total(kDone) ==
        std::int64_t{2} * std::numeric_limits<std::int32_t>::max());
}

TEST_CASE("Compteur de dégâts : le palmarès garde les dix plus gros coups", "[sim][dpsmeter]") {
  ro::sim::DamageMeter meter;
  meter.SetSelf(kSelf);
  for (std::int64_t damage = 1; damage <= 15; ++damage) {
    meter.Record(damage * 100, 1000 + static_cast<std::uint32_t>(damage));
  }
  const auto records = meter.records(kDone);
  REQUIRE(records.size() == ro::sim::DamageMeter::kKeptRecords);
  // Du plus gros au plus petit : les cinq premiers coups n'y sont plus.
  CHECK(records.front().blow.amount == 1500);
  CHECK(records.back().blow.amount == 600);

  // Un coup trop petit n'entre pas, et ne déloge personne.
  meter.Record(50, 2000);
  CHECK(meter.records(kDone).back().blow.amount == 600);
}

TEST_CASE("Compteur de dégâts : à égalité, le plus ancien garde sa place", "[sim][dpsmeter]") {
  ro::sim::DamageMeter meter;
  meter.SetSelf(kSelf);
  meter.Record(ro::sim::MeterBlow{.who = kSelf, .amount = 700, .skill_id = 5}, 1000);
  meter.Record(ro::sim::MeterBlow{.who = kSelf, .amount = 700, .skill_id = 6}, 1100);
  const auto records = meter.records(kDone);
  REQUIRE(records.size() == 2);
  CHECK(records[0].blow.skill_id == 5);
  CHECK(records[1].blow.skill_id == 6);
}

TEST_CASE("Compteur de dégâts : le palmarès survit aux combats et à l'oubli", "[sim][dpsmeter]") {
  ro::sim::DamageMeter meter;
  meter.SetSelf(kSelf);
  meter.settings().combat_timeout_secs = 5;
  meter.Record(900, 1000);
  CHECK(meter.fight() == 1);
  // Un autre combat, bien plus tard : il porte un autre numéro, et le coup du
  // premier reste au palmarès.
  meter.Record(400, 60000);
  CHECK(meter.fight() == 2);
  REQUIRE(meter.records(kDone).size() == 2);
  CHECK(meter.records(kDone)[0].activity == 1);
  CHECK(meter.records(kDone)[1].activity == 2);

  // ⚠ OUBLIER LE COMBAT N'EFFACE PAS LE PALMARÈS ; c'est un geste à part.
  meter.Forget();
  CHECK(meter.records(kDone).size() == 2);
  meter.ForgetRecords();
  CHECK(meter.records(kDone).empty());
}

TEST_CASE("Compteur de dégâts : la moyenne se divise jusqu'au dernier coup", "[sim][dpsmeter]") {
  ro::sim::DamageMeter meter;
  meter.SetSelf(kSelf);
  meter.settings().combat_timeout_secs = 5;
  CHECK(meter.fight_span_ms(1000) == 0);
  meter.Record(100, 1000);
  meter.Record(100, 3000);
  meter.Tick(4000);
  CHECK(meter.fight_span_ms(4000) == 3000);
  // Le combat clos, la durée s'arrête au dernier coup porté : c'est le bilan,
  // pas une horloge qui continue de courir.
  meter.Tick(9000);
  REQUIRE_FALSE(meter.in_combat());
  CHECK(meter.fight_span_ms(9000) == 2000);
}

TEST_CASE("Compteur de dégâts : les cumuls de zone attendent la conversation", "[sim][dpsmeter]") {
  ro::sim::DamageMeter meter;
  meter.SetSelf(kSelf);
  meter.Record(ro::sim::MeterBlow{.who = kSelf, .amount = 1200, .skill_id = ro::sim::kAreaCycleKey},
               1000);
  meter.Record(300, 1100);
  meter.Record(ro::sim::MeterBlow{.who = kSelf, .amount = 800, .skill_id = ro::sim::kAreaCycleKey},
               1200);
  const std::vector<std::int64_t> cycles = meter.TakeAreaCycles();
  REQUIRE(cycles.size() == 2);
  CHECK(cycles[0] == 1200);
  CHECK(cycles[1] == 800);
  CHECK(meter.TakeAreaCycles().empty());
  // Ils comptent au total comme n'importe quel coup.
  CHECK(meter.total(kDone) == 2300);

  // ⚠ LA FILE EST BORNÉE : une conversation qui ne passe plus ne la fait pas
  // grossir.
  for (std::uint32_t at = 0; at < 2 * ro::sim::DamageMeter::kKeptAreaCycles; ++at) {
    meter.Record(ro::sim::MeterBlow{.who = kSelf, .amount = 10, .skill_id = ro::sim::kAreaCycleKey},
                 1300 + at);
  }
  CHECK(meter.TakeAreaCycles().size() == ro::sim::DamageMeter::kKeptAreaCycles);
}

TEST_CASE("Compteur de dégâts : le coup retenu nomme sa cible et sa compétence",
          "[sim][dpsmeter]") {
  World world;
  world.set_player_id(7);
  world.Spawn(Someone(7, CellPosition{5, 5, 0}));
  Actor poring = Someone(9, CellPosition{6, 5, 0});
  poring.kind = ActorKind::kMonster;
  poring.name = "Poring";
  world.Spawn(poring);

  ro::sim::Strike bash;
  bash.attacker = 7;
  bash.target = 9;
  bash.damage = 1234;
  bash.skill_id = 5;
  bash.skill_level = 10;
  bash.type = 10;
  bash.started_ms = 1000;
  REQUIRE(world.RecordStrike(bash));
  const auto records = world.damage_meter().records(kDone);
  REQUIRE(records.size() == 1);
  const ro::sim::MeterBlow& kept = records[0].blow;
  CHECK(kept.amount == 1234);
  CHECK(kept.skill_id == 5);
  CHECK(kept.skill_level == 10);
  CHECK(kept.critical);
  CHECK(kept.other_name == "Poring");
}
