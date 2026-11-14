// LE LANCER D'UNE PARTICULE — la direction, le freinage et la chute.
//
// 🔴 SANS LUI, UNE GERBE SE POSE À PLAT AUTOUR DE SON POINT ET N'EN PART JAMAIS.
// Un sprite de troupe sans traînée se dessine au point de son nœud, et rien
// n'avançait ce point : l'effet montrait toutes ses pièces et paraissait FIGÉ,
// ce qui ne ressemble pas du tout à un effet incomplet.

#include <cmath>
#include <cstdint>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"

using Catch::Approx;
using ro::render::AdvanceLaunchedNode;
using ro::render::DrawEffectLaunch;
using ro::render::EffectLaunchDraw;
using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;
using ro::render::EffectNodeRange;
using ro::render::EffectNodeSeed;
using ro::render::PooledEffectNode;

namespace {

// Une particule qui part droit devant, sans hasard : toutes les plages sont
// pincées sur une seule valeur, ce qui rend le tirage déterministe.
EffectNodeSeed SowingStraightAhead() {
  EffectNodeSeed seed;
  seed.life_ticks = 10;
  seed.step_length = EffectNodeRange{2.0F, 2.0F};
  seed.launch_yaw_degrees = EffectNodeRange{0.0F, 0.0F};
  seed.launch_pitch_degrees = EffectNodeRange{0.0F, 0.0F};
  seed.launch_distance = EffectNodeRange{4.0F, 4.0F};
  return seed;
}

}  // namespace

TEST_CASE("Lancer : deux angles font une direction", "[render][launch]") {
  // 🔴 CE BANC A GARDÉ UNE FORMULE FAUSSE AU VERT, et c'est sa leçon : écrit en
  // même temps que le code qu'il garde, il ne vérifiait que la cohérence de son
  // auteur avec lui-même. Ses valeurs sont désormais celles du CLIENT.
  //
  // ⚠ L'INCLINAISON N'ÉLÈVE PAS, ELLE BASCULE. Chez le CLIENT l'ordonnée
  // descend, donc une inclinaison positive y fait plonger la particule ; chez
  // nous elle la fait MONTER, les deux repères ayant l'ordonnée et la profondeur
  // opposées. Voir `EffectClientOffsetInWorld`.
  std::uint32_t generator = 1;
  EffectNodeSeed seed = SowingStraightAhead();
  const EffectLaunchDraw plat = DrawEffectLaunch(seed, 0.0F, 10, 2.0F, generator);
  // Cap nul : la particule part le long de la PROFONDEUR, pas de l'abscisse — et
  // vers le NORD, qui est notre profondeur DÉCROISSANTE.
  CHECK(plat.direction.x == Approx(0.0F).margin(1e-5));
  CHECK(plat.direction.y == Approx(0.0F).margin(1e-5));
  CHECK(plat.direction.z == Approx(-1.0F));

  seed.launch_pitch_degrees = EffectNodeRange{90.0F, 90.0F};
  generator = 1;
  const EffectLaunchDraw penche = DrawEffectLaunch(seed, 0.0F, 10, 2.0F, generator);
  // Un quart de tour d'inclinaison couche le vecteur ; le client l'envoie vers
  // le bas, notre monde vers le HAUT.
  CHECK(penche.direction.y == Approx(1.0F));
  CHECK(std::abs(penche.direction.z) < 0.001F);
}

TEST_CASE("Lancer : la convention d axes est celle RELEVEE EN VIF", "[render][launch]") {
  // 🔴 UN CAS DU CLIENT, sur un impact en jeu : la colonne du noeud pour ses
  // deux angles, cap 211 degres et inclinaison 13 degres.
  //
  // ⚠ UNE FORMULE FAUSSE PRODUIT LES MEMES TROIS NOMBRES, a l ordre et a un
  // signe pres — le calcul se relit et parait juste, parce qu il l est. C est l
  // affectation aux axes qui ment, et ce banc la epingle.
  //
  // 🔴 LES TROIS NOMBRES SONT CEUX DU CLIENT ; ce banc verifie l IMAGE qu en
  // donne NOTRE monde, dont l ordonnee et la profondeur sont opposees aux
  // siennes. L abscisse, elle, est commune — c est pourquoi elle seule se
  // recopie telle quelle, et c est ce qui rend le cas encore lisible ici.
  EffectNodeSeed seed;
  seed.life_ticks = 10;
  seed.step_length = EffectNodeRange{1.0F, 1.0F};
  seed.launch_yaw_degrees = EffectNodeRange{211.0F, 211.0F};
  seed.launch_pitch_degrees = EffectNodeRange{13.0F, 13.0F};
  std::uint32_t generator = 1;
  const EffectLaunchDraw lance = DrawEffectLaunch(seed, 0.0F, 10, 1.0F, generator);
  CHECK(lance.direction.x == Approx(-0.515038F).margin(1e-5));
  CHECK(lance.direction.y == Approx(-0.192821F).margin(1e-5));
  CHECK(lance.direction.z == Approx(0.835198F).margin(1e-5));
}

TEST_CASE("Lancer : le cap se compose avec la face du PORTEUR", "[render][launch]") {
  // 🔴 LE CLIENT ÉCRIT « orientation de l'acteur + tirage ». La plage ne porte
  // donc que l'ÉCART, et l'appelant y ajoute la face — sans quoi toute gerbe du
  // jeu s'ouvrirait du même côté du monde.
  std::uint32_t generator = 1;
  const EffectNodeSeed seed = SowingStraightAhead();
  const EffectLaunchDraw devant = DrawEffectLaunch(seed, 0.0F, 10, 2.0F, generator);
  generator = 1;
  const EffectLaunchDraw behind = DrawEffectLaunch(seed, 180.0F, 10, 2.0F, generator);
  CHECK(devant.direction.z == Approx(-behind.direction.z).margin(0.001F));
}

TEST_CASE("Lancer : la particule naît À DISTANCE de son point", "[render][launch]") {
  std::uint32_t generator = 1;
  EffectNodeSeed seed = SowingStraightAhead();
  seed.launch_drop = -10.0F;
  const EffectLaunchDraw lance = DrawEffectLaunch(seed, 0.0F, 10, 2.0F, generator);
  // Cap nul : la distance se prend sur la PROFONDEUR, vers le nord — notre
  // profondeur décroissante. Voir la convention dans `DrawEffectLaunch`.
  CHECK(lance.offset.z == Approx(-4.0F));
  // 🔴 ET PLUS HAUT, NON PLUS BAS. Le client retranche dix à son ordonnée, qui
  // DESCEND : la gerbe s'élève donc de dix chez lui, et doit s'élever d'autant
  // chez nous. Recopier son signe l'enterrait, ce qui a été vu en jeu.
  CHECK(lance.offset.y == Approx(10.0F));
}

TEST_CASE("Lancer : le pas FREINE, et ne repart jamais en arrière", "[render][launch]") {
  // 🔴 LE CLIENT BORNE LE PAS PAR UN `fmaxf`. Sans la borne, une particule
  // ralentie repartirait EN ARRIÈRE, ce qui se voit tout de suite sur une
  // gerbe.
  std::uint32_t generator = 1;
  EffectNodeSeed seed = SowingStraightAhead();
  seed.step_decay_share = 1.0F;
  const EffectLaunchDraw lance = DrawEffectLaunch(seed, 0.0F, 10, 2.0F, generator);
  CHECK(lance.step_decay == Approx(-0.2F));

  PooledEffectNode pooled;
  pooled.launch_direction = lance.direction;
  pooled.step_decay = lance.step_decay;
  pooled.node.step_length = 2.0F;
  float previous = pooled.ground_position.x;
  for (int tick_index = 0; tick_index < 40; ++tick_index) {
    AdvanceLaunchedNode(pooled, ro::render::EffectScatterWorld{});
    CHECK(pooled.node.step_length >= 0.0F);
    // La particule n'AVANCE JAMAIS À RECULONS, même longtemps après son freinage.
    CHECK(pooled.ground_position.x >= previous);
    previous = pooled.ground_position.x;
  }
}

TEST_CASE("Lancer : la CHUTE sépare les deux familles d'un impact", "[render][launch]") {
  // 🔴 `EF_HIT1` sème deux nœuds de sous-type 5 ET deux de sous-type 6 : même
  // sprite, même échelle, même pas. Ce que les seconds portent en plus est une
  // vitesse verticale — sans elle, les deux familles sont indiscernables, ce
  // que le générateur en concluait avant de n'en garder qu'une.
  const std::span<const EffectNodePattern> impact = EffectNodePatternsFor(0);
  REQUIRE(impact.size() == 3);
  CHECK(impact[1].fall_speed_low == 0.0F);
  CHECK(impact[2].fall_speed_low < 0.0F);
  // Tout le reste les confond.
  CHECK(impact[1].step_low == impact[2].step_low);
  CHECK(impact[1].sprite_scale == impact[2].sprite_scale);
  // ⚠ ET LEURS CAPS SONT OPPOSÉS : l'une part devant, l'autre derrière.
  CHECK(impact[2].launch_yaw_low - impact[1].launch_yaw_low == Approx(180.0F));
}

TEST_CASE("Lancer : un nœud qui ne part nulle part n'en consomme aucun tirage",
          "[render][launch]") {
  // ⚠ SINON LE GÉNÉRATEUR AVANCERAIT pour rien, et tous les nœuds suivants
  // d'une troupe changeraient de taille pour une raison invisible.
  EffectNodeSeed immobile;
  std::uint32_t generator = 42;
  const EffectLaunchDraw rien = DrawEffectLaunch(immobile, 0.0F, 10, 0.0F, generator);
  CHECK(generator == 42U);
  CHECK(rien.direction.x == 0.0F);
  CHECK(rien.direction.y == 0.0F);
  CHECK(rien.direction.z == 0.0F);

  // Et rien n'avance un nœud sans direction.
  PooledEffectNode pose;
  pose.node.step_length = 5.0F;
  AdvanceLaunchedNode(pose, ro::render::EffectScatterWorld{});
  CHECK(pose.ground_position.x == 0.0F);
}

TEST_CASE("Lancer : les etincelles du soin MONTENT", "[render][launch]") {
  // 🔴 CE QUI SE VOYAIT EN JEU : les étincelles d'EF_EXIT restaient posées en
  // cercle autour du personnage au lieu de s'élever. Leur vitesse verticale —
  // moins un virgule deux — était connue et lue par personne : le lancer ne se
  // compose que pour les nœuds inscrits à la main, et celui-ci ne l'était pas.
  const std::span<const EffectNodePattern> heal = EffectNodePatternsFor(7);
  const EffectNodePattern* sparks = nullptr;
  for (const EffectNodePattern& pattern : heal) {
    if (pattern.shape == ro::render::EffectNodeShape::kSprite) {
      sparks = &pattern;
    }
  }
  REQUIRE(sparks != nullptr);
  // ⚠ NÉGATIVE DANS LE PATRON, parce que c'est ce que le client écrit : son
  // ordonnée DESCEND. C'est le semis qui retourne le signe.
  CHECK(sparks->fall_speed_low < 0.0F);

  EffectNodeSeed seed;
  seed.life_ticks = sparks->life_ticks;
  seed.launch_yaw_degrees = EffectNodeRange{sparks->launch_yaw_low, sparks->launch_yaw_high};
  seed.fall_speed = EffectNodeRange{sparks->fall_speed_low, sparks->fall_speed_high};
  seed.fall_acceleration_share = sparks->fall_acceleration_share;

  std::uint32_t generator = 7;
  const EffectLaunchDraw lance = DrawEffectLaunch(seed, 0.0F, sparks->life_ticks, 0.0F, generator);
  // Chez nous, elle MONTE.
  CHECK(lance.fall_speed > 0.0F);

  PooledEffectNode spark;
  spark.launch_direction = lance.direction;
  spark.fall_speed = lance.fall_speed;
  spark.fall_acceleration = lance.fall_acceleration;
  float precedente = spark.ground_position.y;
  for (std::int32_t tick_index = 0; tick_index < sparks->life_ticks; ++tick_index) {
    AdvanceLaunchedNode(spark, ro::render::EffectScatterWorld{});
    // 🔴 ELLE NE REDESCEND JAMAIS DE SA VIE : l'accélération FREINE la montée,
    // elle ne la retourne pas. À cinquante tics, la vitesse n'a pas fini de
    // s'annuler — la retourner ferait retomber les étincelles sur la tête.
    CHECK(spark.ground_position.y > precedente);
    precedente = spark.ground_position.y;
  }
  CHECK(spark.ground_position.y > 0.0F);
}

TEST_CASE("Lancer : les etincelles du soin s'arretent avec l'effet", "[render][launch]") {
  // 🔴 LA BORNE DU SEMIS EST LA VIE QUE LE BÂTISSEUR ÉCRIT DANS SON INSTANCE —
  // cent tics, posés à son tic zéro —, et non la durée de la table du client,
  // cinquante, qu'il remplace. Cet essai disait le contraire : il tenait ce
  // cent pour « la vie du plus long enfant », et arrêtait les étincelles à
  // mi-course de la colonne.
  //
  // ⚠ CE QUI SE VOYAIT, ET QUI AVAIT MOTIVÉ L'ERREUR : des étincelles nées
  // jusqu'au centième tic et mortes au cent-quarante-neuvième, après la
  // colonne. Ce n'était pas une durée trop longue, c'était un semis qui
  // SURVIVAIT à son effet — le client détruit l'instance et ses enfants
  // ensemble, et c'est la borne du semis qui le porte : `EffectNodeLifeFor`,
  // que la table connaît désormais pour le soin.
  const std::span<const EffectNodePattern> heal = EffectNodePatternsFor(7);
  const EffectNodePattern* sparks = nullptr;
  for (const EffectNodePattern& pattern : heal) {
    if (pattern.shape == ro::render::EffectNodeShape::kSprite) {
      sparks = &pattern;
    }
  }
  REQUIRE(sparks != nullptr);
  CHECK(sparks->period_ticks == 9);
  // Cent tics de vie : le dernier semis tombe au quatre-vingt-dix-neuvième.
  CHECK(sparks->last_tick == 99);
  CHECK(ro::render::EffectNodePatternLastSowTick(*sparks) == 99);
  CHECK(ro::render::EffectNodePatternSowsAt(*sparks, 99));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAt(*sparks, 108));

  // ⚠ ET LA DERNIÈRE ÉTINCELLE NE SURVIT PAS À LA COLONNE : née au
  // quatre-vingt-dix-neuvième tic avec cinquante de vie, elle irait au
  // cent-quarante-neuvième ; la vie de l'effet, celle du client, la coupe au
  // centième.
  CHECK(ro::render::EffectNodeLifeIsKnown(7));
  CHECK(ro::render::EffectNodeLifeFor(7) == 100);
  CHECK(99 + sparks->life_ticks > ro::render::EffectNodeLifeFor(7));
}

TEST_CASE("Lancer : le portail sème QUATRE anneaux et des étincelles", "[render][launch]") {
  // 🔴 QUATRE SYMPTÔMES, UNE SEULE CAUSE. EF_WARP sème ses anneaux tous les
  // vingt tics par un modulo, mais ses étincelles tous les DEUX tics par un
  // test de BIT. Sans période, un seul de chaque naissait.
  const std::span<const EffectNodePattern> portal = EffectNodePatternsFor(8);
  const EffectNodePattern* ring = nullptr;
  const EffectNodePattern* sparks = nullptr;
  for (const EffectNodePattern& pattern : portal) {
    if (pattern.shape == ro::render::EffectNodeShape::kRingCylinder) {
      ring = &pattern;
    } else if (pattern.shape == ro::render::EffectNodeShape::kSprite) {
      sparks = &pattern;
    }
  }
  REQUIRE(ring != nullptr);
  REQUIRE(sparks != nullptr);

  // QUATRE ANNEAUX : aux tics zéro, vingt, quarante et soixante.
  int rondes = 0;
  for (std::int32_t tick = 0; tick <= 200; ++tick) {
    if (ro::render::EffectNodePatternSowsAt(*ring, tick)) {
      ++rondes;
    }
  }
  CHECK(rondes == 4);

  // ⚠ ET SA VIE EST CELLE QUE LE CLIENT DÉCLARE — quatre-vingts —, non les cent
  // du constructeur : un quart de temps de trop à grandir. Le bâtisseur la
  // COPIE de l'effet, et c'est la table des vies qui répond.
  CHECK(ring->life_from_effect);
  CHECK(ro::render::EffectNodeLifeFor(8) == 80);
  // ⚠ SON FONDU EST CALCULÉ : la vie moins sa moitié. Sans lui, l'anneau ne
  // s'efface pas — il DISPARAÎT, ce qui se lit comme un effet coupé.
  CHECK(ring->fade_out_tick == 40);

  // LES ÉTINCELLES : une tous les deux tics, et elles MONTENT.
  CHECK(sparks->period_ticks == 2);
  // 🔴 LEUR VITESSE VERTICALE CHANGE DE SENS SELON LE TIRAGE : le client écrit
  // « soixante moins un tirage sur deux cent quarante, sur cent », soit de plus
  // six dixièmes à moins un virgule sept neuf. La plupart montent — l'ordonnée
  // du client descend —, quelques-unes retombent un peu ;
  // c'est ce mélange qui fait le bouillonnement du portail.
  CHECK(sparks->fall_speed_low < 0.0F);
  CHECK(sparks->fall_speed_high > 0.0F);
  CHECK(sparks->launch_distance_high > sparks->launch_distance_low);
}

TEST_CASE("Lancer : les traits d'EF_ENHANCE montent", "[render][launch]") {
  // 🔴 LEUR MOUVEMENT NE S'ÉCRIT PAS DANS UN CHAMP, IL SE FAIT PAR LA MATRICE.
  // Le client pose le nœud sur un cercle de rayon deux à six, PUIS REMPLACE sa
  // matrice par un quart de tour autour de l'abscisse — et c'est ce quart de
  // tour qui envoie son pas vers le HAUT. Sans lui, le trait naît au sol et y
  // reste.
  const std::span<const EffectNodePattern> renfort = EffectNodePatternsFor(9);
  const EffectNodePattern* trait = nullptr;
  for (const EffectNodePattern& pattern : renfort) {
    if (pattern.shape == ro::render::EffectNodeShape::kCrossedPlanes) {
      trait = &pattern;
    }
  }
  REQUIRE(trait != nullptr);
  // ⚠ UN TOUS LES CINQ TICS, et non un seul : la période était connue, mais
  // aucune BORNE ne la laissait passer.
  CHECK(trait->period_ticks == 5);
  CHECK(trait->last_tick == 79);
  // Le pas, deux unités par tic, et l'inclinaison qui l'envoie vers le haut.
  CHECK(trait->step_low == Approx(2.0F));
  CHECK(trait->launch_pitch_low == Approx(90.0F));

  EffectNodeSeed seed;
  seed.life_ticks = trait->life_ticks;
  seed.step_length = EffectNodeRange{trait->step_low, trait->step_high};
  seed.launch_pitch_degrees = EffectNodeRange{trait->launch_pitch_low, trait->launch_pitch_high};
  seed.launch_distance = EffectNodeRange{trait->launch_distance_low, trait->launch_distance_high};
  std::uint32_t generator = 3;
  const EffectLaunchDraw lance =
      DrawEffectLaunch(seed, 0.0F, trait->life_ticks, trait->step_low, generator);
  // 🔴 DROIT VERS LE HAUT : le quart de tour du client, dans NOTRE repère.
  CHECK(lance.direction.y == Approx(1.0F));
  CHECK(std::abs(lance.direction.z) < 0.001F);
}

TEST_CASE("Lancer : une figure d'ANNEAU qui part avance aussi", "[render][launch]") {
  // 🔴 LE DÉFAUT QUE CE CAS EMPÊCHE, ET IL A TENU DEUX CORRECTIONS. Le lancer
  // était composé par le seul semis des TROUPES, et avancé par le seul
  // mouvement du SPRITE : une figure d'anneau qui part restait clouée à sa
  // naissance, son pas connu, tiré, et jamais appliqué. Les traits
  // d'EF_ENHANCE montent de deux unités par tic chez le client, et ne bougeaient
  // pas d'un pixel chez nous.
  //
  // ⚠ CE N'EST PAS LE MÊME DÉFAUT QUE « le pas n'est pas lu » : là, le nombre
  // manquait ; ici il était là, et personne ne s'en servait. Les deux se lisent
  // pareil à l'écran.
  ro::render::EffectNodePool pool;
  ro::render::PooledEffectNode trait;
  trait.shape = ro::render::PooledNodeShape::kCrossedPlanes;
  trait.node.life_ticks = 50;
  trait.node.step_length = 2.0F;
  // Droit vers le haut, comme le quart de tour du client l'envoie.
  trait.launch_direction = glm::vec3{0.0F, 1.0F, 0.0F};
  pool.Add(trait);

  pool.Advance(static_cast<std::uint32_t>(ro::render::kEffectTickMs) * 5U);
  REQUIRE(pool.nodes().size() == 1);
  CHECK(pool.nodes().front().ground_position.y > 0.0F);
}

TEST_CASE("Lancer : à un quart de tour, le CAP décide de tout", "[render][launch]") {
  // 🔴 LA FORMULE EST DÉGÉNÉRÉE À QUATRE-VINGT-DIX DEGRÉS D'INCLINAISON, et c'est
  // ce qui a envoyé les traits d'EF_ENHANCE dans la direction du regard. L'abscisse
  // vaut le SINUS DU CAP tout court — elle ne se module pas par le cosinus de
  // l'inclinaison (convention du client) —, si bien qu'à un quart de tour le
  // vecteur devient (sin cap, cos cap, 0) : vertical pour un cap NUL, et couché
  // dès que le cap tourne.
  //
  // ⚠ CONSÉQUENCE POUR L'APPELANT : un nœud qui doit monter ne peut pas composer
  // son cap avec la face de son porteur. Celui d'EF_ENHANCE ne la compose pas —
  // son bâtisseur REMPLACE la matrice par un quart de tour pur —, et c'est le
  // drapeau node_yaw_follows_owner qui le dit.
  EffectNodeSeed seed;
  seed.life_ticks = 50;
  seed.step_length = EffectNodeRange{2.0F, 2.0F};
  seed.launch_pitch_degrees = EffectNodeRange{90.0F, 90.0F};

  std::uint32_t generator = 1;
  const EffectLaunchDraw straight = DrawEffectLaunch(seed, 0.0F, 50, 2.0F, generator);
  CHECK(straight.direction.y == Approx(1.0F));

  generator = 1;
  const EffectLaunchDraw rotated = DrawEffectLaunch(seed, 90.0F, 50, 2.0F, generator);
  // La face du porteur COUCHE le vecteur : c'est le défaut vu en jeu.
  CHECK(rotated.direction.y == Approx(0.0F).margin(1e-5));
  CHECK(std::abs(rotated.direction.x) == Approx(1.0F));
}

TEST_CASE("Vie : on ne borne QUE ce qui est mesuré", "[render][launch]") {
  // 🔴 UN NŒUD NE SURVIT PAS À SON EFFET — le client détruit l'instance et ses
  // enfants ensemble —, et c'est ce qui fait que les rayons d'EF_ENHANCE
  // s'éteignent AVEC leur colonne au lieu de monter une seconde de plus.
  //
  // ⚠ MAIS BORNER CONTRE UN DÉFAUT SERAIT PIRE QUE NE PAS BORNER. La table des
  // vies rend cent tics pour tout effet qu'elle ne connaît pas : pris pour une
  // vraie durée, ce nombre couperait les nœuds de tous les autres. D'où la
  // question séparée, et ce cas qui la tient.
  CHECK(ro::render::EffectNodeLifeIsKnown(9));
  CHECK(ro::render::EffectNodeLifeFor(9) == 80);
  // Un identifiant qu'aucune espèce « copie » ne sert : la table se tait, et
  // l'appelant ne borne rien.
  CHECK_FALSE(ro::render::EffectNodeLifeIsKnown(0));
  CHECK(ro::render::EffectNodeLifeFor(0) == ro::render::kEffectNodeDefaultLifeTicks);
}

TEST_CASE("Vie : la colonne d'EF_ENHANCE s'EFFACE, elle ne disparaît pas", "[render][launch]") {
  // ⚠ SON FONDU EST CALCULÉ, non posé : la vie moins son tiers, cinquante-quatre
  // sur quatre-vingts. À zéro, la colonne se coupe net — ce qui se lit comme un
  // effet tronqué, et non comme une sortie manquante.
  const std::span<const EffectNodePattern> renfort = EffectNodePatternsFor(9);
  const EffectNodePattern* column = nullptr;
  for (const EffectNodePattern& pattern : renfort) {
    if (pattern.shape == ro::render::EffectNodeShape::kRingBand) {
      column = &pattern;
    }
  }
  REQUIRE(column != nullptr);
  CHECK(column->fade_out_tick == 54);
  CHECK(column->life_from_effect);
}

TEST_CASE("Lancer : les trois impacts portent le leur", "[render][launch]") {
  for (const std::uint32_t effect_id : {0U, 2U, 3U}) {
    INFO("effet " << effect_id);
    bool has_launch = false;
    for (const EffectNodePattern& pattern : EffectNodePatternsFor(effect_id)) {
      if (pattern.shape != ro::render::EffectNodeShape::kSprite) {
        continue;
      }
      has_launch = true;
      CHECK(pattern.step_high > 0.0F);
      CHECK(pattern.launch_pitch_high > pattern.launch_pitch_low);
      CHECK(pattern.launch_distance_high > 0.0F);
      // ⚠ LA CHUTE AU DÉPART EST NÉGATIVE : la gerbe part sous le point touché.
      CHECK(pattern.launch_drop < 0.0F);
    }
    CHECK(has_launch);
  }
}

TEST_CASE("Rebond : la piece retourne sa vitesse sous le sol", "[render][launch]") {
  // 🔴 CE QUE LE CLIENT FAIT, ET QUE NOUS NE FAISIONS PAS : sous le terrain, la
  // mise a jour du sous-type 6 DIVISE la vitesse verticale par moins un et
  // demi. La particule repart vers le haut, plus lentement — et c'est a cet
  // instant-la qu'elle sonne.
  ro::render::EffectScatterWorld world;
  world.ground_height = [](float, float) { return 0.0F; };

  PooledEffectNode piece;
  piece.launch_direction = glm::vec3{0.0F, 0.0F, 1.0F};
  piece.bounces_at_floor = true;
  piece.bounce_sound = "effect\\EF_Coin%d.wav";
  piece.bounce_sound_draws = 3;
  piece.ground_position = glm::vec3{0.0F, 0.5F, 0.0F};
  piece.fall_speed = -1.0F;

  AdvanceLaunchedNode(piece, world);
  // Elle est passee sous le sol : 0,5 moins 1 fait moins 0,5.
  CHECK(piece.ground_position.y == Approx(-0.5F));
  // ⚠ ET LA VITESSE S'EST RETOURNEE, divisee par un et demi.
  CHECK(piece.fall_speed == Approx(1.0F / 1.5F));
  CHECK(piece.bounced_this_tick);
}

TEST_CASE("Rebond : rien ne rebondit en montant ni sans sol", "[render][launch]") {
  // ⚠ LA GARDE DU SENS. Une particule qui MONTE depuis un point deja sous le
  // terrain rebondirait a chaque tic et resterait collee au sol.
  ro::render::EffectScatterWorld world;
  world.ground_height = [](float, float) { return 0.0F; };

  PooledEffectNode montante;
  montante.launch_direction = glm::vec3{0.0F, 0.0F, 1.0F};
  montante.bounces_at_floor = true;
  montante.ground_position = glm::vec3{0.0F, -5.0F, 0.0F};
  montante.fall_speed = 1.0F;
  AdvanceLaunchedNode(montante, world);
  CHECK_FALSE(montante.bounced_this_tick);

  // ET UN MONDE QUI NE REPOND PAS laisse passer : mieux vaut une piece qui
  // traverse qu'une altitude inventee.
  PooledEffectNode without_world;
  without_world.launch_direction = glm::vec3{0.0F, 0.0F, 1.0F};
  without_world.bounces_at_floor = true;
  without_world.ground_position = glm::vec3{0.0F, 0.5F, 0.0F};
  without_world.fall_speed = -1.0F;
  AdvanceLaunchedNode(without_world, ro::render::EffectScatterWorld{});
  CHECK_FALSE(without_world.bounced_this_tick);
  CHECK(without_world.fall_speed == Approx(-1.0F));
}

TEST_CASE("Rebond : les pieces d'EF_COIN partent, retombent et sonnent", "[render][launch]") {
  // 🔴 L'EFFET 10 N'AVAIT NI LANCER NI BRUIT. Ses cinq pieces naissaient l'une
  // sur l'autre au pied du personnage, et son `.wav` n'etait dans aucun des
  // quatre tables de son : il est joue par la mise a jour du NOEUD, quand la
  // piece touche le sol.
  const EffectNodePattern* pieces = nullptr;
  for (const EffectNodePattern& pattern : EffectNodePatternsFor(10)) {
    if (pattern.shape == ro::render::EffectNodeShape::kSprite) {
      pieces = &pattern;
    }
  }
  REQUIRE(pieces != nullptr);
  // Le lancer : un cap tire, une distance de deux ou trois, une montee.
  CHECK(pieces->launch_yaw_high > pieces->launch_yaw_low);
  CHECK(pieces->launch_distance_low == Approx(2.0F));
  CHECK(pieces->launch_distance_high == Approx(3.0F));
  CHECK(pieces->step_high > pieces->step_low);
  // ⚠ NEGATIVE DANS LE PATRON : c'est ce que le client ecrit, son ordonnee
  // descendant. Le semis retourne le signe.
  CHECK(pieces->fall_speed_low < 0.0F);
  CHECK(pieces->launch_drop == Approx(-10.0F));
  // Et le rebond, avec le nom A FORMER et ses trois variantes.
  CHECK(pieces->bounces_at_floor);
  CHECK(pieces->bounce_sound == "effect\\EF_Coin%d.wav");
  CHECK(pieces->bounce_sound_draws == 3);
}

TEST_CASE("Rebond : un seul patron le fait sonner", "[render][launch]") {
  // ⚠ UN SON DE REBOND SE PAIE D'UNE LECTURE A LA MAIN, et un champ du noeud le
  // commande. Si un jour un deuxieme patron sonne, que ce soit parce qu'on l'a
  // lu — pas parce que la garde a lache.
  std::size_t ringing_count = 0;
  std::size_t bouncing_count = 0;
  for (std::uint32_t effect_id = 0; effect_id < 2500; ++effect_id) {
    for (const EffectNodePattern& pattern : EffectNodePatternsFor(effect_id)) {
      if (pattern.bounces_at_floor) {
        ++bouncing_count;
      }
      if (!pattern.bounce_sound.empty()) {
        ++ringing_count;
        CHECK(pattern.bounces_at_floor);
        CHECK(pattern.bounce_sound_draws > 0);
      }
    }
  }
  CHECK(ringing_count == 1);
  // Les six sous-type 6 connus : 0, 10, 18, 81, 180, 184.
  CHECK(bouncing_count >= 1);
}

TEST_CASE("Rebond : une piece d'EF_COIN retombe et sonne pendant sa vie", "[render][launch]") {
  // 🔴 CE QUE LE TEST PRECEDENT NE DISAIT PAS : que la piece ATTEINT le sol.
  // Le rebond peut etre juste, le son branche, et la gerbe ne jamais redescendre
  // assez dans le temps qui lui est donne — l'effet resterait muet sans qu'une
  // seule assertion ne bronche. On simule donc la vie entiere.
  const EffectNodePattern* pieces = nullptr;
  for (const EffectNodePattern& pattern : EffectNodePatternsFor(10)) {
    if (pattern.shape == ro::render::EffectNodeShape::kSprite) {
      pieces = &pattern;
    }
  }
  REQUIRE(pieces != nullptr);

  ro::render::EffectScatterWorld world;
  world.ground_height = [](float, float) { return 0.0F; };

  // Les cinq pieces du semis, chacune avec son tirage.
  std::uint32_t generator = 7;
  int ringing_count = 0;
  for (int piece = 0; piece < 5; ++piece) {
    EffectNodeSeed style_seed;
    style_seed.launch_yaw_degrees = {pieces->launch_yaw_low, pieces->launch_yaw_high};
    style_seed.launch_pitch_degrees = {pieces->launch_pitch_low, pieces->launch_pitch_high};
    style_seed.launch_distance = {pieces->launch_distance_low, pieces->launch_distance_high};
    style_seed.launch_drop = pieces->launch_drop;
    style_seed.step_length = {pieces->step_low, pieces->step_high};
    style_seed.step_decay_share = pieces->step_decay_share;
    style_seed.fall_speed = {pieces->fall_speed_low, pieces->fall_speed_high};
    style_seed.fall_acceleration_share = pieces->fall_acceleration_share;

    const std::int32_t life = pieces->life_ticks;
    const EffectLaunchDraw lance = DrawEffectLaunch(style_seed, 0.0F, life, 0.7F, generator);

    PooledEffectNode pooled;
    pooled.launch_direction = lance.direction;
    pooled.ground_position = lance.offset;
    pooled.node.step_length = 0.7F;
    pooled.step_decay = lance.step_decay;
    pooled.fall_speed = lance.fall_speed;
    pooled.fall_acceleration = lance.fall_acceleration;
    pooled.bounces_at_floor = pieces->bounces_at_floor;
    pooled.bounce_sound = std::string(pieces->bounce_sound);
    pooled.bounce_sound_draws = pieces->bounce_sound_draws;

    // ⚠ ELLE PART VERS LE HAUT : le client ecrit une vitesse negative, que le
    // semis retourne. Sans cela la gerbe tomberait tout droit.
    CHECK(pooled.fall_speed > 0.0F);
    CHECK(pooled.ground_position.y > 0.0F);

    bool a_sonne = false;
    for (std::int32_t tick_index = 0; tick_index < life; ++tick_index) {
      AdvanceLaunchedNode(pooled, world);
      if (pooled.bounced_this_tick) {
        a_sonne = true;
        pooled.bounced_this_tick = false;
      }
    }
    if (a_sonne) {
      ++ringing_count;
    }
  }
  // LES CINQ TOUCHENT LE SOL avant de s'eteindre : c'est ce qu'on entend en jeu.
  CHECK(ringing_count == 5);
}

TEST_CASE("Rebond : chaque patron qui rebondit passe par un semeur qui le porte",
          "[render][launch]") {
  // 🔴 TROIS SILENCES SUCCESSIFS, ET CELUI-CI EST LE TROISIEME : le rebond etait
  // pose dans le semeur d'ANNEAU, et un patron de SPRITE n'y passe jamais. Le
  // champ voyageait sur un chemin que les pieces d'EF_COIN n'empruntent pas.
  //
  // DEUX SEMEURS, et ils ne partagent rien : `SowRing` prend le patron
  // directement, `Sow` passe par une `EffectNodeSeed`. Ce test dit par lequel
  // chaque patron qui rebondit doit passer ; si une forme nouvelle s'y ajoute,
  // il faudra verifier que SON semeur porte le champ, et l'assertion le
  // rappellera.
  std::size_t en_sprite = 0;
  std::size_t en_figure = 0;
  for (std::uint32_t effect_id = 0; effect_id < 2500; ++effect_id) {
    for (const EffectNodePattern& pattern : EffectNodePatternsFor(effect_id)) {
      if (!pattern.bounces_at_floor) {
        continue;
      }
      if (pattern.shape == ro::render::EffectNodeShape::kSprite) {
        ++en_sprite;
      } else {
        ++en_figure;
      }
    }
  }
  // Les six nœuds de sous-type 6 connus sont tous des sprites : c'est `Sow`
  // qui les sème, par la graine, et `SowRing` n'en voit aucun.
  CHECK(en_sprite > 0);
  CHECK(en_figure == 0);
}

TEST_CASE("Lancer : la boule de Soul Strike freine sur son axe et se balance", "[render][launch]") {
  // Le mouvement du sous-type 8 : `avance = pas − vitesse verticale`, et un
  // écart `Rot(axe, roulis) · (0, tangage, 0)`. Une boule à deux cents unités
  // de sa cible : pas cinq sur quarante tics.
  constexpr std::int32_t kLife = 40;
  const auto ready_ball = [] {
    PooledEffectNode ball;
    ball.launch_direction = glm::vec3{1.0F, 0.0F, 0.0F};
    ball.node.step_length = 5.0F;
    ball.brakes_along_axis = true;
    // La vitesse verticale du client, 3,5, et son freinage `−2 × 3,5 / 40` :
    // chez nous au signe près, comme toute chute.
    ball.fall_speed = -3.5F;
    ball.fall_acceleration = 0.175F;
    // Le tangage du balancement : −2 par tic, freiné de `+2 × 2 / 40`.
    ball.swing_speed = -2.0F;
    ball.swing_acceleration = 0.1F;
    return ball;
  };

  PooledEffectNode ball = ready_ball();
  float previous_advance = 0.0F;
  for (std::int32_t tick_index = 1; tick_index <= kLife; ++tick_index) {
    const float before = ball.ground_position.x;
    AdvanceLaunchedNode(ball, ro::render::EffectScatterWorld{});
    const float advance = ball.ground_position.x - before;
    // ELLE PART LENTEMENT ET ACCÉLÈRE : cinq moins trois et demi, plus le
    // freinage, au premier tic ; l'avance croît ensuite de 0,175 par tic.
    if (tick_index == 1) {
      CHECK(advance == Approx(1.675F));
    } else {
      CHECK(advance > previous_advance);
    }
    previous_advance = advance;
    // À MI-VOL, L'ARC CULMINE : vingt et une unités au-dessus de l'axe. Le
    // tangage cumule la vitesse d'AVANT son freinage : `20 × 2 − 0,1 × 190`.
    if (tick_index == 20) {
      CHECK(ball.ground_position.y == Approx(21.0F).margin(0.01F));
    }
  }
  // Elle atteint sa cible à sa mort, à trois unités et demie près — c'est
  // l'écriture du client, et non une approximation de la nôtre.
  CHECK(ball.ground_position.x == Approx(203.5F).margin(0.01F));
  // Et redescend vers son axe sans l'atteindre : deux unités au-dessus au
  // quarantième tic, `40 × 2 − 0,1 × 780`.
  CHECK(ball.ground_position.y == Approx(2.0F).margin(0.01F));
  CHECK(ball.ground_position.z == Approx(0.0F).margin(0.001F));

  // À UN QUART DE TOUR DE ROULIS, le même balancement s'écarte DE CÔTÉ : c'est
  // ce qui éventaille les boules d'un Soul Strike à plusieurs coups.
  //
  // 🔴 ET DE QUEL CÔTÉ SE CALCULE. L'axe du client est à l'envers du vol : pour
  // un vol sur +X, c'est (−1, 0, 0) chez lui comme chez nous. Son écart à
  // mi-vol est (0, −21, 0) — vers le haut, son ordonnée descendant — que
  // `Rot((−1, 0, 0), 90°)` envoie sur (0, 0, +21) ; notre profondeur est
  // l'opposée de la sienne, d'où −21. Vu en jeu à l'envers quand l'axe était
  // celui du vol.
  PooledEffectNode roulee = ready_ball();
  roulee.roll_degrees = 90.0F;
  for (std::int32_t tick_index = 1; tick_index <= 20; ++tick_index) {
    AdvanceLaunchedNode(roulee, ro::render::EffectScatterWorld{});
  }
  CHECK(roulee.ground_position.z == Approx(-21.0F).margin(0.01F));
  CHECK(roulee.ground_position.y == Approx(0.0F).margin(0.01F));
  // Et son avance est la même qu'à plat : vingt tics de `1,5 + 0,175 × k`.
  CHECK(roulee.ground_position.x == Approx(30.0F + 0.175F * 210.0F).margin(0.01F));
}

TEST_CASE("Lancer : un pas NUL qui gagne en dur part quand même", "[render][launch]") {
  // EF_DECAGILITY : pas nul, une variation du pas de 0,015 posée en dur. Sans
  // cette variation comptée, le lancer se croyait immobile et le rayon restait
  // en l'air.
  std::uint32_t generator = 1;
  EffectNodeSeed seed = SowingStraightAhead();
  seed.step_length = EffectNodeRange{0.0F, 0.0F};
  seed.step_change_per_tick = 0.015F;
  const EffectLaunchDraw lance = DrawEffectLaunch(seed, 0.0F, 50, 0.0F, generator);
  REQUIRE(lance.direction != glm::vec3{0.0F, 0.0F, 0.0F});
  CHECK(lance.step_decay == Approx(0.015F));

  PooledEffectNode pooled;
  pooled.launch_direction = lance.direction;
  pooled.step_decay = lance.step_decay;
  for (int tick_index = 0; tick_index < 10; ++tick_index) {
    AdvanceLaunchedNode(pooled, ro::render::EffectScatterWorld{});
  }
  // Le pas a pris 0,15 en dix tics, et la particule a avancé de leur somme.
  CHECK(pooled.node.step_length == Approx(0.15F));
  CHECK(glm::length(pooled.ground_position) == Approx(0.015F * 55.0F));
}

TEST_CASE("Lancer : les rayons des buffs d'agilité montent, ou tombent", "[render][launch]") {
  // Increase Agility, sa variante AgiDex et Decrease Agility : un rayon tous
  // les deux tics pendant les cent de l'instance, né sur un cercle de deux à
  // huit unités, élevé d'un quart de tour — ou abaissé pour le 38.
  for (const std::uint32_t each_effect_id : {37U, 43U, 38U}) {
    INFO("effet " << each_effect_id);
    const EffectNodePattern* radius = nullptr;
    for (const EffectNodePattern& pattern : EffectNodePatternsFor(each_effect_id)) {
      if (pattern.shape == ro::render::EffectNodeShape::kCrossedPlanes) {
        radius = &pattern;
      }
    }
    REQUIRE(radius != nullptr);
    CHECK(radius->first_tick == 0);
    CHECK(radius->last_tick == 99);
    CHECK(radius->period_ticks == 2);
    CHECK(radius->launch_distance_low == Approx(2.0F));
    CHECK(radius->launch_distance_high == Approx(8.0F));
    const bool descend = each_effect_id == 38U;
    CHECK(radius->launch_pitch_low == Approx(descend ? -90.0F : 90.0F));
    if (descend) {
      CHECK(radius->step_change_per_tick == Approx(0.015F));
      CHECK(radius->step_high == Approx(0.0F));
    } else {
      CHECK(radius->step_low == Approx(0.2F));
      CHECK(radius->step_high == Approx(0.69F));
    }
  }
}

TEST_CASE("Lancer : Concentration sème sa traînée tous les quatre tics, de 16 à 108",
          "[render][launch]") {
  // `âge − 13 ≤ 96` et `âge % 4 == 0` : vingt-quatre traînées qui
  // montent de 0,33 à 1,15 par tic, sur un cercle de deux à huit.
  for (const std::uint32_t each_effect_id : {220U, 756U}) {
    INFO("effet " << each_effect_id);
    const EffectNodePattern& trail = EffectNodePatternsFor(each_effect_id).front();
    CHECK(trail.first_tick == 16);
    CHECK(trail.last_tick == 108);
    CHECK(trail.period_ticks == 4);
    CHECK(trail.launch_pitch_low == Approx(90.0F));
    CHECK(trail.step_high == Approx(69.0F / 60.0F));
  }
}

TEST_CASE("Lancer : l'AcCenter d'agilité suit la branche de SON paramètre", "[render][launch]") {
  // Au-dessous de deux (456), demi-largeur 2..11,9 et pas 0,20..0,69 ; à deux et
  // plus (777, 779), 4..13,9 et 0,40..0,89. Les deux branches comptent.
  const EffectNodePattern& little = EffectNodePatternsFor(456).front();
  const EffectNodePattern& grand = EffectNodePatternsFor(779).front();
  CHECK(little.ring.half_width == Approx(2.0F));
  CHECK(little.ring.half_width_high == Approx(11.9F));
  CHECK(little.step_high == Approx(0.69F));
  CHECK(grand.ring.half_width == Approx(4.0F));
  CHECK(grand.step_high == Approx(0.89F));
  CHECK(little.period_ticks == 2);
}
