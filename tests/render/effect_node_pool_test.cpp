#include "ro/render/effect_node_pool.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_trail_history.h"

#include "support/effect_bearers.h"

using ro::render::AdvanceEffectNode;
using ro::render::EffectNode;
using ro::render::EffectNodePool;
using ro::render::EffectNodeRange;
using ro::render::EffectNodeSeed;
using ro::render::kEffectNodeMaxCatchUpTicks;
using ro::render::kEffectTickMs;
using ro::render::kEffectTicksPerSecond;
using ro::render::PooledEffectNode;
using ro::render::SowEffectNode;

namespace {

// Le patron des particules d'Increase AGI, celui du client : cinquante tics de
// vie, une
// demi-largeur tirée entre trois et neuf, une longueur de pas entre un cinquième
// et deux tiers, un angle quelconque, et un alpha qui monte à deux cents en
// vingt tics puis redescend en vingt.
EffectNodeSeed IncreaseAgilityParticleSeed() {
  EffectNodeSeed seed;
  seed.resource_path = "effect\\ac_center2.tga";
  seed.life_ticks = 50;
  seed.alpha_rise_ticks = 20;
  seed.fade_out_ticks = 20;
  seed.alpha_max = 200.0F;
  seed.half_width = EffectNodeRange{3.0F, 8.9F};
  seed.half_height = EffectNodeRange{0.1796F, 0.1796F};
  seed.step_length = EffectNodeRange{0.20F, 0.69F};
  seed.orbit_degrees = EffectNodeRange{0.0F, 359.0F};
  return seed;
}

}  // namespace

TEST_CASE("Semis : chaque nœud tire dans les bornes de son patron", "[render][effectnode]") {
  const EffectNodeSeed seed = IncreaseAgilityParticleSeed();
  std::uint32_t generator = 1234;

  bool widths_differ = false;
  float first_width = 0.0F;
  for (int sown = 0; sown < 40; ++sown) {
    const EffectNode node = SowEffectNode(seed, generator);
    CHECK(node.half_width.half_size >= 3.0F);
    CHECK(node.half_width.half_size <= 8.9F);
    CHECK(node.step_length >= 0.20F);
    CHECK(node.step_length <= 0.69F);
    CHECK(node.orbit_degrees >= 0.0F);
    CHECK(node.orbit_degrees <= 359.0F);
    // Une plage sans largeur rend sa borne, sans consommer de tirage.
    CHECK(node.half_height.half_size == Catch::Approx(0.1796F));

    if (sown == 0) {
      first_width = node.half_width.half_size;
    } else if (node.half_width.half_size != Catch::Approx(first_width)) {
      widths_differ = true;
    }
  }
  // Le GROUILLEMENT est le but : quarante nœuds tous identiques feraient un
  // halo figé au lieu d'un buff qui scintille.
  CHECK(widths_differ);
}

TEST_CASE("Semis : les rampes se disent en DURÉE, pas en pas", "[render][effectnode]") {
  // Changer le plafond garde alors la même durée de montée, au lieu d'obliger à
  // recalculer le pas à côté.
  const EffectNodeSeed seed = IncreaseAgilityParticleSeed();
  std::uint32_t generator = 7;
  const EffectNode node = SowEffectNode(seed, generator);
  CHECK(node.alpha == Catch::Approx(0.0F));
  CHECK(node.alpha_max == Catch::Approx(200.0F));
  CHECK(node.alpha_step_per_tick == Catch::Approx(10.0F));
  // La sortie commence vingt tics avant la fin.
  CHECK(node.fade_out_tick == 30);
}

TEST_CASE("Semis : une même graine rejoue la même foule", "[render][effectnode]") {
  // C'est ce qui rend un effet ENTIER vérifiable au banc, et non seulement un
  // nœud isolé.
  const EffectNodeSeed seed = IncreaseAgilityParticleSeed();
  std::uint32_t first = 99;
  std::uint32_t second = 99;
  for (int sown = 0; sown < 8; ++sown) {
    const EffectNode left = SowEffectNode(seed, first);
    const EffectNode right = SowEffectNode(seed, second);
    CHECK(left.half_width.half_size == Catch::Approx(right.half_width.half_size));
    CHECK(left.orbit_degrees == Catch::Approx(right.orbit_degrees));
    CHECK(left.wander_seed == right.wander_seed);
  }
  // Et deux nœuds semés À LA SUITE diffèrent, sans quoi la graine ne servirait
  // qu'à les rendre tous pareils.
  std::uint32_t generator = 99;
  const EffectNode one = SowEffectNode(seed, generator);
  const EffectNode two = SowEffectNode(seed, generator);
  CHECK(one.wander_seed != two.wander_seed);
}

TEST_CASE("Troupe : la cadence est FIXE et le reste se garde", "[render][effectnode]") {
  // Une machine à cent vingt images par seconde livre des tranches de huit
  // millisecondes : jeter le reste de chacune n'avancerait jamais d'un tic.
  EffectNodePool pool;
  PooledEffectNode pooled;
  pooled.node.life_ticks = 1000;
  pool.Add(pooled);

  for (int frame = 0; frame < 125; ++frame) {
    pool.Advance(8);
  }
  REQUIRE(pool.size() == 1);
  // Une seconde pleine vaut soixante-deux tics de seize millisecondes, et huit
  // millisecondes restent dues : le client ne compte pas autrement.
  CHECK(pool.nodes()[0].node.age_ticks == kEffectTicksPerSecond);
}

TEST_CASE("Troupe : ce qui a fini s'en va", "[render][effectnode]") {
  EffectNodePool pool;
  for (const std::int32_t life : {2, 5, 400}) {
    PooledEffectNode pooled;
    pooled.node.life_ticks = life;
    pool.Add(pooled);
  }
  REQUIRE(pool.size() == 3);

  pool.Advance(kEffectTickMs * 3);
  CHECK(pool.size() == 2);

  pool.Advance(200);
  CHECK(pool.size() == 1);
  CHECK(pool.nodes()[0].node.life_ticks == 400);

  pool.Clear();
  CHECK(pool.size() == 0);
}

TEST_CASE("Troupe : un sprite lancé n'avance qu'UNE fois par tic", "[render][effectnode]") {
  // ⚠ LE CAS DU SPRITE TOMBAIT DANS CELUI DES ANNEAUX, et les deux appelaient
  // le lancer : chaque particule avançait deux fois par tic, et une boule de
  // Soul Strike doublait sa cible. Le mouvement seul est juste et se teste à
  // part ; c'est le CÂBLAGE de la troupe que celui-ci regarde.
  EffectNodePool pool;
  PooledEffectNode ball;
  ball.shape = ro::render::PooledNodeShape::kSprite;
  ball.node.life_ticks = 100;
  ball.launch_direction = glm::vec3{1.0F, 0.0F, 0.0F};
  ball.node.step_length = 5.0F;
  pool.Add(ball);

  pool.Advance(kEffectTickMs);
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(5.0F));
  pool.Advance(kEffectTickMs);
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(10.0F));
}

TEST_CASE("Troupe : la traînée d'un sprite recule d'un cran par tic", "[render][effectnode]") {
  // Le client décale ses N traces à chaque mise à jour du nœud, la tête prenant
  // la position courante : après deux tics,
  // une boule qui avance de cinq garde dix, cinq, et sa naissance.
  EffectNodePool pool;
  PooledEffectNode ball;
  ball.shape = ro::render::PooledNodeShape::kSprite;
  ball.node.life_ticks = 100;
  ball.launch_direction = glm::vec3{1.0F, 0.0F, 0.0F};
  ball.node.step_length = 5.0F;
  ball.trail.assign(3, ro::render::EffectTrailStep{});
  pool.Add(ball);

  pool.Advance(kEffectTickMs * 2);
  REQUIRE(pool.size() == 1);
  const std::vector<ro::render::EffectTrailStep>& trail = pool.nodes()[0].trail;
  REQUIRE(trail.size() == 3);
  CHECK(trail[0].ground_position.x == Catch::Approx(10.0F));
  CHECK(trail[1].ground_position.x == Catch::Approx(5.0F));
  CHECK(trail[2].ground_position.x == Catch::Approx(0.0F));
  // Et la tête est le nœud lui-même, à l'échelle et à l'alpha courants.
  CHECK(trail[0].alpha == Catch::Approx(pool.nodes()[0].node.alpha));
  CHECK(trail[0].sprite_scale == Catch::Approx(pool.nodes()[0].node.sprite_scale));
}

TEST_CASE("Troupe : une longue pause ne fait pas défiler l'effet", "[render][effectnode]") {
  // Une pause au débogueur ou un chargement de carte laisse des secondes à
  // rattraper. Les jouer toutes ferait passer l'effet en accéléré devant le
  // joueur ; mieux vaut qu'il ait manqué ce qu'il n'a pas vu.
  EffectNodePool pool;
  PooledEffectNode pooled;
  pooled.node.life_ticks = 100000;
  pool.Add(pooled);

  pool.Advance(30000);
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes()[0].node.age_ticks == kEffectNodeMaxCatchUpTicks);
}

TEST_CASE("Semis : la graine porte l'avance d'image d'un sprite joué une fois",
          "[render][effectnode]") {
  // Un sprite d'acteur joué comme un nœud dit combien d'images il a, qu'elles
  // ne bouclent pas, et leur période fractionnaire — et le nœud les emporte.
  EffectNodeSeed seed;
  seed.life_ticks = 12;
  seed.texture_count = 7;
  seed.loops_frames = false;
  seed.frame_period_ticks = 1.5F;
  std::uint32_t generator = 3;
  const EffectNode node = SowEffectNode(seed, generator);
  CHECK(node.texture_count == 7);
  CHECK_FALSE(node.loops_frames);
  CHECK(node.frame_period_ticks == Catch::Approx(1.5F));

  // Et les défauts restent ceux du nœud : une image qui boucle.
  const EffectNode plain = SowEffectNode(EffectNodeSeed{}, generator);
  CHECK(plain.texture_count == 1);
  CHECK(plain.loops_frames);
  CHECK(plain.frame_period_ticks == 0.0F);
}

TEST_CASE("Semis : un sprite qui part pâle SANS pas reste pâle", "[render][effectnode]") {
  // Le type sept de la fabrique pose un alpha de deux cent vingt et un pas nul :
  // une durée de montée d'un tic le porterait au plafond.
  EffectNodeSeed seed;
  seed.life_ticks = 60;
  seed.alpha_max = 254.0F;
  seed.alpha_start = 220.0F;
  seed.alpha_step = 0.0F;
  std::uint32_t generator = 5;
  EffectNode node = SowEffectNode(seed, generator);
  CHECK(node.alpha == Catch::Approx(220.0F));
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    REQUIRE(AdvanceEffectNode(node));
  }
  CHECK(node.alpha == Catch::Approx(220.0F));

  // Et un départ TIRÉ reste dans sa fourchette — le type soixante et un.
  seed.alpha_start = 70.0F;
  seed.alpha_start_high = 140.0F;
  for (std::int32_t draw = 0; draw < 50; ++draw) {
    const EffectNode drawn = SowEffectNode(seed, generator);
    CHECK(drawn.alpha >= 70.0F);
    CHECK(drawn.alpha <= 140.0F);
  }
}

TEST_CASE("Semis : la cadence d'image se TIRE quand elle a une borne haute",
          "[render][effectnode]") {
  // `rand() % 5 + 3`, type dix-neuf de la fabrique : trois à sept tics par image.
  EffectNodeSeed seed;
  seed.life_ticks = 60;
  seed.frame_hold_ticks = 3;
  seed.frame_hold_ticks_high = 7;
  std::uint32_t generator = 11;
  bool seen_other_than_three = false;
  for (std::int32_t draw = 0; draw < 60; ++draw) {
    const EffectNode node = SowEffectNode(seed, generator);
    CHECK(node.frame_hold_ticks >= 3);
    CHECK(node.frame_hold_ticks <= 7);
    seen_other_than_three = seen_other_than_three || node.frame_hold_ticks != 3;
  }
  CHECK(seen_other_than_three);

  // Sans borne haute, la cadence est celle posée — et quatre par défaut.
  CHECK(SowEffectNode(EffectNodeSeed{}, generator).frame_hold_ticks ==
        ro::render::kEffectNodeDefaultFrameHoldTicks);
}

TEST_CASE("Troupe : un nœud accroché SUIT son porteur, un nœud posé reste",
          "[render][effectnode]") {
  // 🔴 CE QUI DURE DOIT SUIVRE. Faire suivre l'horloge d'un effet ne déplace que
  // ses semis À VENIR ; l'anneau du classement PVP sème une fois, vit dix mille
  // tics, et restait donc à la case où le joueur se tenait quand il l'a reçu.
  // Le joueur l'a vu abandonné derrière lui au premier pas.
  EffectNodePool pool;
  constexpr std::uint32_t kBearer = 2000;

  PooledEffectNode ring;
  ring.node.life_ticks = 9999;
  pool.AnchorNextTo(kBearer);
  pool.Add(ring);
  pool.AnchorNextTo(0);

  // ⚠ ET LE VOISIN NE BOUGE PAS. Une marque au sol posée au même endroit n'a pas
  // de porteur : la déplacer avec lui la ferait glisser sur la carte.
  PooledEffectNode mark;
  mark.node.life_ticks = 9999;
  pool.Add(mark);

  REQUIRE(pool.size() == 2);
  REQUIRE(ro::test_support::IsAnchoredTo(pool, kBearer));
  CHECK_FALSE(ro::test_support::IsAnchoredTo(pool, kBearer + 1));

  ro::test_support::MoveBearer(pool, kBearer, glm::vec3{40.0F, 3.0F, 70.0F});
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(40.0F));
  CHECK(pool.nodes()[0].ground_position.z == Catch::Approx(70.0F));
  CHECK(pool.nodes()[1].ground_position.x == Catch::Approx(0.0F));
  CHECK(pool.nodes()[1].ground_position.z == Catch::Approx(0.0F));

  // ⚠ ZÉRO N'EST PAS UNE ANCRE : c'est l'absence d'ancre. L'employer comme telle
  // emmènerait tous les nœuds posés du monde d'un seul coup.
  ro::test_support::MoveBearer(pool, 0, glm::vec3{99.0F, 0.0F, 99.0F});
  CHECK(pool.nodes()[1].ground_position.x == Catch::Approx(0.0F));
}

TEST_CASE("Troupe : suivre un porteur DÉPLACE le nœud, il ne l'y recolle pas",
          "[render][effectnode]") {
  // 🔴 CE QUI SÉPARE UN NŒUD DE SON PORTEUR EST À LUI. Faire suivre en posant la
  // place du porteur telle quelle effaçait tout l'écart : la part de couronne
  // d'un exemplaire, la sortie du terrain, et surtout l'ENFOUISSEMENT — les
  // trente unités sous lesquelles une pointe de glace naît avant de jaillir.
  //
  // Le joueur l'a vu sur Frost Diver : recollées au sol, les pointes montaient
  // de leurs vingt-huit unités AU-DESSUS de leur cible au lieu d'en sortir.
  EffectNodePool pool;
  constexpr std::uint32_t kBearer = 3000;
  const glm::vec3 pieds{10.0F, 5.0F, 20.0F};

  PooledEffectNode tip;
  tip.node.life_ticks = 40;
  // Enterrée de trente sous les pieds de sa cible, et écartée d'une demi-unité
  // sur le côté comme un exemplaire de couronne.
  tip.ground_position = pieds + glm::vec3{0.5F, -30.0F, 0.0F};
  pool.AnchorNextTo(kBearer, pieds);
  pool.Add(tip);
  pool.AnchorNextTo(0);

  REQUIRE(pool.size() == 1);
  // CE QUI SE GARDE EST LA PLACE DE L'ANCRE, dont se mesurera le pas — et non
  // l'écart de naissance, qui servait à REPOSER le nœud.
  CHECK(pool.nodes()[0].anchor_ground_position.y == Catch::Approx(5.0F));

  // L'ENFOUISSEMENT EST DANS LA PLACE DU NŒUD, et c'est lui qui doit survivre.
  CHECK(pool.nodes()[0].ground_position.y == Catch::Approx(-25.0F));

  // La cible fait un pas : la pointe le suit SANS remonter.
  const glm::vec3 after{14.0F, 5.0F, 20.0F};
  ro::test_support::MoveBearer(pool, kBearer, after);
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(14.5F));
  CHECK(pool.nodes()[0].ground_position.y == Catch::Approx(-25.0F));
  CHECK(pool.nodes()[0].ground_position.z == Catch::Approx(20.0F));

  // ⚠ ET UN NŒUD SANS ANCRE N'A PAS D'ÉCART : le sien reste nul, sans quoi le
  // premier `MoveBearers` venu le décalerait de sa propre place.
  PooledEffectNode placed;
  placed.node.life_ticks = 40;
  placed.ground_position = glm::vec3{1.0F, 2.0F, 3.0F};
  pool.Add(placed);
  CHECK(pool.nodes()[1].anchor_ground_position.x == Catch::Approx(0.0F));
  CHECK(pool.nodes()[1].anchor_ground_position.y == Catch::Approx(0.0F));
  CHECK(pool.nodes()[1].anchor_ground_position.z == Catch::Approx(0.0F));
}

TEST_CASE("Troupe : un porteur parti EMPORTE ce qu'il portait", "[render][effectnode]") {
  // 🔴 UN NŒUD ACCROCHÉ NE MEURT QUE DE SA VIE, et celle de l'anneau du
  // classement PVP vaut dix mille tics : le joueur qui quittait la carte laissait
  // son anneau tourner sur une case vide. Vu en jeu.
  EffectNodePool pool;
  constexpr std::uint32_t kParti = 2000;
  constexpr std::uint32_t kRemaining = 2001;

  for (const std::uint32_t bearer : {kParti, kRemaining}) {
    PooledEffectNode ring;
    ring.node.life_ticks = 9999;
    pool.AnchorNextTo(bearer);
    pool.Add(ring);
  }
  // ⚠ ET UNE MARQUE SANS PORTEUR, qui doit survivre : zéro n'est pas une ancre
  // absente, c'est l'ABSENCE d'ancre. Interroger le prédicat sur zéro effacerait
  // tout le décor — les impacts au sol n'appartiennent à personne.
  pool.AnchorNextTo(0);
  PooledEffectNode mark;
  mark.node.life_ticks = 9999;
  pool.Add(mark);
  REQUIRE(pool.size() == 3);

  pool.DropAnchoredUnless([](std::uint32_t bearer) { return bearer == kRemaining; });
  REQUIRE(pool.size() == 2);
  CHECK(ro::test_support::IsAnchoredTo(pool, kRemaining));
  CHECK_FALSE(ro::test_support::IsAnchoredTo(pool, kParti));
  // La marque du décor est toujours là, et elle n'a toujours pas d'ancre.
  CHECK(pool.nodes()[1].anchor_id == 0);
}

TEST_CASE("Troupe : l'ancre d'un semis tient toute sa PORTÉE", "[render][effectnode]") {
  // 🔴 LA FAUTE ÉTAIT DE REPOSER TROP TÔT, PAS D'OUBLIER DE REPOSER. Le semis
  // d'un effet procédural appelle quarante familles à la suite ; l'ancre y était
  // reposée après la PREMIÈRE, et les trente-neuf autres naissaient sans
  // porteur. Le mur de Safety Wall restait alors à l'écran dix mille tics après
  // que le serveur l'avait levé — que sa durée s'achève ou que ses coups
  // absorbés s'épuisent, les deux fins passent par cette levée.
  EffectNodePool pool;
  constexpr std::uint32_t kSowingCarrier = 3100;

  {
    const EffectNodePool::AnchoredSowing anchor{pool, kSowingCarrier, glm::vec3{0.0F, 0.0F, 0.0F}};
    // DEUX FAMILLES, l'une après l'autre, et les DEUX portent l'ancre. C'est
    // exactement ce qui manquait : la seconde n'en avait pas.
    for (int family = 0; family < 2; ++family) {
      PooledEffectNode effect_node;
      effect_node.node.life_ticks = 9999;
      pool.Add(effect_node);
    }
  }
  REQUIRE(pool.size() == 2);
  CHECK(pool.nodes()[0].anchor_id == kSowingCarrier);
  CHECK(pool.nodes()[1].anchor_id == kSowingCarrier);

  // ⚠ ET LA SORTIE DE PORTÉE REPOSE L'ANCRE : ce qui naît ensuite pour un autre
  // effet n'appartient à personne, sans quoi le décor suivrait le premier venu.
  PooledEffectNode decor;
  decor.node.life_ticks = 9999;
  pool.Add(decor);
  CHECK(pool.nodes()[2].anchor_id == 0);

  // Le porteur parti emporte SES deux nœuds, et laisse le décor.
  pool.DropAnchoredUnless([](std::uint32_t) { return false; });
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes()[0].anchor_id == 0);
}

TEST_CASE("Troupe : la vie demandée RELÈVE celle du patron, sans jamais la couper",
          "[render][effectnode]") {
  // 🔴 CE QUI ACCOMPAGNE UNE ACTION DURE CE QUE L'ACTION DURE. Le patron d'un
  // effet donne la vie d'une BOUFFÉE — soixante-dix tics pour l'anneau en
  // couches d'une incantation, soit 1 120 ms — alors qu'une incantation va de
  // 800 ms à plus de quatre secondes : le cercle s'éteignait au tiers des sorts
  // longs, le lanceur restant à incanter au-dessus d'un sol nu.
  EffectNodePool pool;

  // Un sort LONG : la vie demandée l'emporte.
  PooledEffectNode circle;
  circle.node.life_ticks = 70;
  pool.LiveAtLeast(275);
  pool.Add(circle);

  // ⚠ ET UN SORT PLUS BREF QUE LA BOUFFÉE LA LAISSE FINIR. C'est un PLANCHER, et
  // c'est l'ordre du client : il pose la durée voulue sur l'instance, puis le
  // bâtisseur y applique la sienne au premier tic. Raccourcir couperait la
  // bouffée au milieu de sa disparition.
  PooledEffectNode bref;
  bref.node.life_ticks = 70;
  pool.LiveAtLeast(50);
  pool.Add(bref);

  // Celui qui pose le plancher le repose : le voisin garde son patron.
  pool.LiveAtLeast(0);
  PooledEffectNode neighbor;
  neighbor.node.life_ticks = 70;
  pool.Add(neighbor);

  REQUIRE(pool.size() == 3);
  CHECK(pool.nodes()[0].node.life_ticks == 275);
  CHECK(pool.nodes()[1].node.life_ticks == 70);
  CHECK(pool.nodes()[2].node.life_ticks == 70);

  // Et la vie relevée se VIT. ⚠ PAR TRANCHES SOUS LE PLAFOND DE RATTRAPAGE :
  // un seul grand pas n'avancerait que d'une seconde (`kEffectNodeMaxCatchUpTicks`),
  // et l'essai passerait au vert sans avoir rien exercé.
  constexpr std::int32_t kTrancheTicks = 40;
  static_assert(kTrancheTicks < kEffectNodeMaxCatchUpTicks);
  const auto advance_by = [&pool](std::int32_t ticks) {
    pool.Advance(static_cast<std::uint32_t>(kEffectTickMs * ticks));
  };

  advance_by(kTrancheTicks);
  advance_by(kTrancheTicks);
  // Quatre-vingts tics : la bouffée de soixante-dix est finie, le cercle du
  // sort long reste. Sans le plancher, la troupe serait déjà vide.
  CHECK(pool.size() == 1);

  for (int tranche = 0; tranche < 4; ++tranche) {
    advance_by(kTrancheTicks);
  }
  // Deux cent quarante tics : encore en deçà des deux cent soixante-quinze.
  CHECK(pool.size() == 1);

  advance_by(kTrancheTicks);
  CHECK(pool.size() == 0);
}

TEST_CASE("Troupe : une ÉCHÉANCE raccourcit là où un plancher ne peut pas",
          "[render][effectnode]") {
  // 🔴 LES DEUX EXISTENT, ET CONFONDRE L UN AVEC L AUTRE SE PAIE. Le client
  // pose une vie VOULUE sur l instance quand l effet accompagne une action,
  // puis laisse le bâtisseur relever la sienne : c est un plancher. Mais un
  // repère posé au sol porte une ÉCHÉANCE et meurt à l heure dite, quelle que
  // soit la vie que sa table lui donne.
  //
  // La grille de visée en déclare trois mille — quarante-huit secondes. Posée
  // avec un plancher de la durée du sort, elle gardait sa propre vie et restait
  // sur la carte longtemps après l incantation ; un plancher ne raccourcit
  // jamais. Vu en jeu.
  EffectNodePool pool;
  constexpr std::int32_t kDeclaree = 3000;
  constexpr std::int32_t kDureeDuSort = 875;

  PooledEffectNode with_floor;
  with_floor.node.life_ticks = kDeclaree;
  pool.LiveAtLeast(kDureeDuSort);
  pool.Add(with_floor);

  PooledEffectNode with_deadline;
  with_deadline.node.life_ticks = kDeclaree;
  pool.LiveAtLeast(0);
  pool.LiveExactly(kDureeDuSort);
  pool.Add(with_deadline);

  // ⚠ ET L ÉCHÉANCE PRIME SUR LE PLANCHER quand les deux sont posés : elle dit
  // une heure, pas une préférence.
  PooledEffectNode both;
  both.node.life_ticks = kDeclaree;
  pool.LiveAtLeast(9999);
  pool.LiveExactly(kDureeDuSort);
  pool.Add(both);

  pool.LiveAtLeast(0);
  pool.LiveExactly(0);
  PooledEffectNode neighbor;
  neighbor.node.life_ticks = kDeclaree;
  pool.Add(neighbor);

  REQUIRE(pool.size() == 4);
  CHECK(pool.nodes()[0].node.life_ticks == kDeclaree);     // le plancher n a pas mordu
  CHECK(pool.nodes()[1].node.life_ticks == kDureeDuSort);  // l échéance, si
  CHECK(pool.nodes()[2].node.life_ticks == kDureeDuSort);
  CHECK(pool.nodes()[3].node.life_ticks == kDeclaree);  // et elle se repose à zéro
}

TEST_CASE("Troupe : un nœud peut TOURNER autour de son point de naissance",
          "[render][effectnode]") {
  // 🔴 UNE ORBITE N'EST PAS UN LANCER. Un nœud lancé s'éloigne de sa place ;
  // celui-ci tourne autour d'un point à distance CONSTANTE, et sa position se
  // recalcule depuis son angle à chaque tic. Le semis la figeait une fois pour
  // toutes, si bien que les étincelles d'un point de téléportation restaient où
  // elles étaient nées — « particules immobiles », a dit le joueur.
  //
  // Le champ est le 816 du client, et son bâtisseur y écrit un et demi à deux
  // degrés et demi par tic.
  EffectNodePool pool;
  PooledEffectNode pooled;
  pooled.node.life_ticks = 1000;
  pooled.orbit_anchor = glm::vec3{10.0F, 0.0F, 20.0F};
  pooled.orbit_radius = 5.0F;
  pooled.orbit_degrees_per_tick = 90.0F;
  pooled.ground_position = pooled.orbit_anchor + glm::vec3{5.0F, 3.0F, 0.0F};
  pool.Add(pooled);

  const auto radius = [](const PooledEffectNode& effect_node) {
    const glm::vec3 arm = effect_node.ground_position - effect_node.orbit_anchor;
    return std::sqrt(arm.x * arm.x + arm.z * arm.z);
  };

  pool.Advance(kEffectTickMs);
  REQUIRE(pool.size() == 1);
  // UN QUART DE TOUR : le nœud est passé de l'abscisse à la profondeur.
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(10.0F).margin(0.001F));
  CHECK(pool.nodes()[0].ground_position.z == Catch::Approx(25.0F).margin(0.001F));
  // ⚠ ET SA HAUTEUR N'A PAS BOUGÉ : ce que le vol accumule en ordonnée reste.
  // La recomposer ferait retomber au sol une étincelle qui monte.
  CHECK(pool.nodes()[0].ground_position.y == Catch::Approx(3.0F).margin(0.001F));

  // 🔴 ET LE RAYON NE DÉRIVE PAS. C'est ce qui sépare une orbite d'un pas
  // tangent : ajouter un déplacement ferait spiraler la particule vers
  // l'extérieur, un peu plus à chaque tour.
  for (int tick_index = 0; tick_index < 40; ++tick_index) {
    pool.Advance(kEffectTickMs);
  }
  REQUIRE(pool.size() == 1);
  CHECK(radius(pool.nodes()[0]) == Catch::Approx(5.0F).margin(0.001F));
}

TEST_CASE("Troupe : sans rayon ni pas, rien ne tourne", "[render][effectnode]") {
  // ⚠ UNE PARTICULE NÉE AU CENTRE TOURNE AUTOUR D'ELLE-MÊME, c'est-à-dire ne
  // bouge pas — et c'est le cas de presque tous les nœuds de la troupe, qui ne
  // déclarent aucune orbite. Le contrôle tient ce silence : une orbite posée
  // par défaut déplacerait tout ce qui existe.
  for (const float radius : {0.0F, 5.0F}) {
    for (const float step : {0.0F, 90.0F}) {
      if (radius > 0.0F && step != 0.0F) {
        continue;
      }
      EffectNodePool pool;
      PooledEffectNode pooled;
      pooled.node.life_ticks = 1000;
      pooled.orbit_radius = radius;
      pooled.orbit_degrees_per_tick = step;
      pooled.ground_position = glm::vec3{7.0F, 1.0F, 3.0F};
      pool.Add(pooled);
      pool.Advance(kEffectTickMs);
      REQUIRE(pool.size() == 1);
      INFO("rayon " << radius << ", pas " << step);
      CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(7.0F).margin(0.001F));
      CHECK(pool.nodes()[0].ground_position.z == Catch::Approx(3.0F).margin(0.001F));
    }
  }
}

TEST_CASE("Troupe : le bras de l'orbite s'allonge, et l'ancre n'est pas la place",
          "[render][effectnode]") {
  // LE CHAMP 844 : le client intègre ce pas dans la distance à chaque tic, avant
  // de recomposer la position depuis le cap. Le bras s'allonge donc, et le
  // cercle n'en est plus un — c'est une spirale.
  EffectNodePool pool;
  PooledEffectNode pooled;
  pooled.node.life_ticks = 1000;
  pooled.orbit_anchor = glm::vec3{0.0F, 0.0F, 0.0F};
  pooled.orbit_radius = 5.0F;
  pooled.orbit_degrees_per_tick = 90.0F;
  pooled.orbit_radius_step_per_tick = 0.25F;
  pooled.ground_position = glm::vec3{5.0F, 0.0F, 0.0F};
  pool.Add(pooled);

  const auto radius = [](const PooledEffectNode& effect_node) {
    const glm::vec3 arm = effect_node.ground_position - effect_node.orbit_anchor;
    return std::sqrt(arm.x * arm.x + arm.z * arm.z);
  };

  for (int tick_index = 0; tick_index < 4; ++tick_index) {
    pool.Advance(kEffectTickMs);
  }
  REQUIRE(pool.size() == 1);
  // Quatre tics, quatre quarts de pas : le bras a gagné une unité.
  CHECK(radius(pool.nodes()[0]) == Catch::Approx(6.0F).margin(0.001F));
  // ET UN TOUR COMPLET RAMÈNE LE CAP, non la place : le nœud est reparti dans
  // la direction de son départ, mais plus loin.
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(6.0F).margin(0.001F));
  CHECK(pool.nodes()[0].ground_position.z == Catch::Approx(0.0F).margin(0.001F));
}

TEST_CASE("Troupe : suivre un porteur TRANSLATE le nœud, sans effacer son vol",
          "[render][effectnode]") {
  // 🔴 L'ANCRAGE RECOLLAIT AU LIEU DE TRANSLATER, et c'est ce qui clouait au sol
  // les étincelles d'un point de téléportation. `MoveBearers` reposait la place
  // du nœud à `ancre + écart de naissance` À CHAQUE IMAGE : sa vitesse verticale
  // s'intégrait correctement, sa hauteur était réécrite juste après, et elle ne
  // dépassait donc jamais UN tic. Mesuré en jeu par un témoin — 0,60 puis 0,84
  // puis 0,60, au même âge.
  //
  // ⚠ ET L'ORBITE SURVIVAIT, ce qui rendait la panne invisible : elle se
  // recalcule depuis son ancre au lieu de s'intégrer. Une moitié du mouvement
  // marchait, l'autre non, et l'œil ne voyait qu'un défaut de hauteur.
  EffectNodePool pool;
  const glm::vec3 bearer{10.0F, 0.0F, 20.0F};
  pool.AnchorNextTo(7U, bearer);
  PooledEffectNode pooled;
  pooled.node.life_ticks = 1000;
  // Né EN L'AIR et sous terre par rapport à son porteur : les deux écarts
  // doivent survivre au premier pas.
  pooled.ground_position = bearer + glm::vec3{3.0F, -30.0F, 0.0F};
  pooled.launch_direction = glm::vec3{0.0F, 0.0F, 1.0F};
  pooled.fall_speed = 0.5F;
  pool.Add(pooled);
  pool.AnchorNextTo(0U);

  // Dix tics de vol : le nœud monte de cinq unités.
  for (int tick_index = 0; tick_index < 10; ++tick_index) {
    pool.Advance(kEffectTickMs);
  }
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes()[0].ground_position.y == Catch::Approx(-25.0F).margin(0.001F));

  // 🔴 LE PORTEUR NE BOUGE PAS : suivre ne doit RIEN changer. C'est le cas du
  // point de téléportation, qui ne marche jamais — et c'est là que la panne
  // frappait le plus fort.
  ro::test_support::MoveBearer(pool, 7U, bearer);
  CHECK(pool.nodes()[0].ground_position.y == Catch::Approx(-25.0F).margin(0.001F));
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(13.0F).margin(0.001F));

  // ET QUAND IL MARCHE, le nœud le suit d'un PAS : son vol reste acquis.
  ro::test_support::MoveBearer(pool, 7U, bearer + glm::vec3{4.0F, 0.0F, 1.0F});
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(17.0F).margin(0.001F));
  CHECK(pool.nodes()[0].ground_position.z == Catch::Approx(21.0F).margin(0.001F));
  CHECK(pool.nodes()[0].ground_position.y == Catch::Approx(-25.0F).margin(0.001F));
}

TEST_CASE("Troupe : suivre un porteur emmène aussi l'ancre de l'orbite", "[render][effectnode]") {
  // ⚠ SANS ELLE, UN NŒUD QUI TOURNE REVIENT À SON ANCIEN CERCLE au tic suivant :
  // sa place se RECALCULE depuis l'ancre de l'orbite, elle ne s'intègre pas. Le
  // translater sans elle ne tiendrait donc qu'une image.
  EffectNodePool pool;
  const glm::vec3 bearer{0.0F, 0.0F, 0.0F};
  pool.AnchorNextTo(9U, bearer);
  PooledEffectNode pooled;
  pooled.node.life_ticks = 1000;
  pooled.orbit_anchor = bearer;
  pooled.orbit_radius = 5.0F;
  pooled.orbit_degrees_per_tick = 90.0F;
  pooled.ground_position = bearer + glm::vec3{5.0F, 0.0F, 0.0F};
  pool.Add(pooled);
  pool.AnchorNextTo(0U);

  ro::test_support::MoveBearer(pool, 9U, bearer + glm::vec3{100.0F, 0.0F, 0.0F});
  pool.Advance(kEffectTickMs);
  REQUIRE(pool.size() == 1);
  // Un quart de tour AUTOUR DU NOUVEAU POINT, non de l'ancien.
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(100.0F).margin(0.001F));
  CHECK(pool.nodes()[0].ground_position.z == Catch::Approx(5.0F).margin(0.001F));
}

TEST_CASE("Troupe : le roulis d'une âme tourne aussi son sprite", "[render][effectnode]") {
  // Le roulis sert deux lecteurs : le balancement et le dessinateur de
  // sprites, qui l'ajoute à l'angle de chaque calque.
  ro::render::EffectNodeSeed style_seed;
  style_seed.life_ticks = 40;
  style_seed.roll_degrees = 90.0F;
  std::uint32_t rng_state = 1U;
  const ro::render::EffectNode soul = ro::render::SowEffectNode(style_seed, rng_state);
  CHECK(soul.draws_rotated);
  CHECK(soul.spin_degrees == Catch::Approx(90.0F));
  // Sans roulis, rien ne tourne.
  ro::render::EffectNodeSeed right_side;
  right_side.life_ticks = 40;
  const ro::render::EffectNode plate = ro::render::SowEffectNode(right_side, rng_state);
  CHECK_FALSE(plate.draws_rotated);
}

TEST_CASE("Troupe : un mouvement qui se repose sur son porteur le suit quand il marche",
          "[render][effectnode]") {
  // Quarante-cinq mouvements font `ground_position = owner_position` à chaque tic.
  // `MoveBearers` translatait le nœud mais pas son porteur : au tic suivant, le
  // mouvement le ramenait à sa place de naissance.
  EffectNodePool pool;
  const glm::vec3 bearer{0.0F, 0.0F, 0.0F};
  pool.AnchorNextTo(11U, bearer);
  PooledEffectNode pooled;
  pooled.shape = ro::render::PooledNodeShape::kTurningRing;
  pooled.node.life_ticks = 1000;
  pooled.ground_position = bearer;
  pooled.owner_position = bearer;
  pool.Add(pooled);
  pool.AnchorNextTo(0U);

  const glm::vec3 farther = bearer + glm::vec3{30.0F, 0.0F, -10.0F};
  ro::test_support::MoveBearer(pool, 11U, farther);
  pool.Advance(kEffectTickMs);
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes()[0].ground_position.x == Catch::Approx(30.0F).margin(0.001F));
  CHECK(pool.nodes()[0].ground_position.z == Catch::Approx(-10.0F).margin(0.001F));
}

TEST_CASE("Troupe : le monde qui se déplace emmène chaque point du nœud, pas ses pistes",
          "[render][effectnode]") {
  EffectNodePool pool;
  PooledEffectNode pooled;
  pooled.node.life_ticks = 1000;
  pooled.ground_position = {10.0F, 2.0F, 20.0F};
  pooled.owner_position = {11.0F, 0.0F, 21.0F};
  pooled.target_position = glm::vec3{40.0F, 0.0F, 50.0F};
  pooled.orbit_anchor = {12.0F, 0.0F, 22.0F};
  pooled.haunt_center = {13.0F, 0.0F, 23.0F};
  pooled.trail.push_back(ro::render::EffectTrailStep{.ground_position = {9.0F, 2.0F, 19.0F}});
  pooled.tracks[0].active = true;
  pooled.tracks[0].anchor = {1.0F, 2.0F, 3.0F};
  pool.Add(pooled);
  REQUIRE(pool.TracksAlive());

  const glm::vec3 shift{-160.0F, 0.0F, 160.0F};
  pool.Shift(shift);
  const PooledEffectNode& moved = pool.nodes().front();
  CHECK(moved.ground_position == pooled.ground_position + shift);
  CHECK(moved.owner_position == pooled.owner_position + shift);
  CHECK(moved.target_position == *pooled.target_position + shift);
  CHECK(moved.orbit_anchor == pooled.orbit_anchor + shift);
  CHECK(moved.haunt_center == pooled.haunt_center + shift);
  CHECK(moved.trail.front().ground_position == pooled.trail.front().ground_position + shift);
  // LES PISTES GARDENT LEURS POINTS : leur sens est celui de leur figure.
  CHECK(moved.tracks[0].anchor == pooled.tracks[0].anchor);

  pool.Clear();
  CHECK_FALSE(pool.TracksAlive());
}
