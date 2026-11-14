// LES RELECTURES DE LA TABLE DES PATRONS : ce qu'un effet relu contre le
// client a fait entrer dans les tables à la main — et qu'un essai tient pour
// que la prochaine régénération ne le reperde pas.
//
// Un cas par effet relu. Chacun dit ce que le client écrit ; voir
// `scripts/lectures_a_la_main_des_patrons.py`.
#include <algorithm>
#include <cstdint>
#include <span>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_launch.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_sprite_seed.h"
#include "ro/render/effect_node_world.h"

using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;
using ro::render::EffectNodeShape;

TEST_CASE("EF_GLASSWALL dresse quatre murs en boîte autour du porteur", "[render][table]") {
  // Deux patrons, deux exemplaires chacun, de part et d'autre du point le long
  // de leur normale — et le premier tourné d'un quart de tour par rapport au
  // second, sans quoi les deux paires s'empilent sur un axe.
  const std::span<const EffectNodePattern> murs = EffectNodePatternsFor(13);
  REQUIRE(murs.size() == 2);
  const auto wall_of_half_width = [&murs](float half_width) -> const EffectNodePattern& {
    const auto found = std::ranges::find_if(murs, [half_width](const EffectNodePattern& wall) {
      return wall.ring.half_width == Catch::Approx(half_width);
    });
    REQUIRE(found != murs.end());
    return *found;
  };
  for (const EffectNodePattern& wall : murs) {
    CHECK(wall.shape == EffectNodeShape::kOrientedQuad);
    CHECK(wall.ring.instance_count == 2);
    CHECK(wall.ring.instance_placement == ro::render::EffectNodeInstancePlacement::kMirroredPair);
    CHECK(wall.node_yaw_follows_owner);
    // 🔴 ET IL NE FAIT PAS FACE À LA CAMÉRA : 0x28, sans le bit 0x40. C'est ce
    // qui en fait un plan FIXE du monde, tourné par son lacet ; face à la
    // caméra, les quatre murs se posaient dans le même sens et la boîte
    // n'avait aucun côté.
    CHECK(wall.ring.flags == 0x28U);
    CHECK_FALSE(ro::render::EffectNodeFacesCamera(wall.ring.flags));
    // 🔴 ET SON HAUT PULSE : quarante de haut, un quart d'unité par tic, et le
    // pas se retourne tous les cinquante tics sous le bit 0x20. Sans lui, le
    // mur grimperait au plafond de cent.
    CHECK(wall.ring.height == Catch::Approx(40.0F));
    CHECK(wall.ring.height_growth == Catch::Approx(0.25F));
    CHECK((wall.ring.flags & ro::render::kEffectNodeHeightGrowthFlips) != 0U);
    CHECK(wall.ring.height_flip_period_ticks == 50);
    // 🔴 SA VIE EST CELLE DE L'EFFET — jusqu'à ce qu'on l'enlève —, et non les
    // cent tics du constructeur : un mur qui tombe au bout d'une seconde et
    // demie n'est pas un mur.
    CHECK(wall.life_from_effect);
  }
  // ⚠ L'ÉCART DE L'UN EST LA DEMI-LARGEUR DE L'AUTRE : c'est ce qui ferme la
  // boîte, six sur cinq virgule deux.
  const EffectNodePattern& large = wall_of_half_width(3.0F);
  CHECK(large.ring.instance_spacing == Catch::Approx(2.6F));
  CHECK(large.launch_yaw_low == Catch::Approx(90.0F));
  const EffectNodePattern& narrow = wall_of_half_width(2.6F);
  CHECK(narrow.ring.instance_spacing == Catch::Approx(3.0F));
  CHECK(narrow.launch_yaw_low == Catch::Approx(0.0F));
}

TEST_CASE("seul le bit 0x40 dresse un quad orienté face à la caméra", "[render][table]") {
  // Le mouvement du quad orienté ne bâtit la matrice depuis la caméra que sous
  // ce bit ; sans lui, ce sont les angles du nœud.
  CHECK_FALSE(ro::render::EffectNodeFacesCamera(0x28U));
  CHECK(ro::render::EffectNodeFacesCamera(0x40U));
  CHECK(ro::render::EffectNodeFacesCamera(0x41U));
  // Mesuré dans la table : un quad du piège à onde de choc le porte.
  const std::span<const EffectNodePattern> piege = EffectNodePatternsFor(93);
  CHECK(std::ranges::any_of(piege, [](const EffectNodePattern& pattern) {
    return ro::render::EffectNodeFacesCamera(pattern.ring.flags);
  }));
}

TEST_CASE("EF_HEALSP lâche ses étincelles un tic sur deux, et elles montent", "[render][table]") {
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(14);
  const auto spark = std::ranges::find_if(patterns, [](const EffectNodePattern& pattern) {
    return pattern.shape == EffectNodeShape::kSprite;
  });
  REQUIRE(spark != patterns.end());
  // Le MÊME lancer que les étincelles du soin : elles montent — l'ordonnée du
  // client descend —, freinées des deux tiers du quotient par la vie.
  CHECK(spark->fall_speed_low == Catch::Approx(-1.2F));
  CHECK(spark->fall_acceleration_share == Catch::Approx(1.0F / 1.5F));
  // Une un tic sur DEUX, tant que l'effet vit — cent tics, que le bâtisseur
  // écrit lui-même à son tic zéro.
  CHECK(spark->period_ticks == 2);
  CHECK(spark->last_tick == 99);
}

TEST_CASE("EF_EXIT et EF_HEALSP : les étincelles naissent à DEUX fois le rayon",
          "[render][table]") {
  // 🔴 LE RAYON COMPTE DEUX FOIS chez le client, et la table engendrée ne le
  // voit qu'une. Le bâtisseur pose un rayon de 3 puis cuit
  // `RotY(cap) · (0, 0, rayon)` dans le décalage du nœud, et le mouvement du
  // sous-type 7 ajoute le même vecteur à chaque tic. Trois unités deux fois :
  // six, hors de la colonne, dont la couronne la plus large fait quatre et demi.
  // À trois, toutes les étincelles naîtraient DANS la colonne.
  for (const std::uint32_t each_effect_id : {7U, 14U}) {
    INFO(each_effect_id);
    const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(each_effect_id);
    const auto spark = std::ranges::find_if(patterns, [](const EffectNodePattern& pattern) {
      return pattern.shape == EffectNodeShape::kSprite;
    });
    REQUIRE(spark != patterns.end());
    CHECK(spark->scatter == Catch::Approx(6.0F));
    // Et rien d'autre ne les écarte du centre : ni distance de lancer, ni pas.
    CHECK(spark->launch_distance_low == 0.0F);
    CHECK(spark->launch_distance_high == 0.0F);
    CHECK(spark->step_low == 0.0F);
  }
}

TEST_CASE("EF_STEAL lâche dix étincelles qui montent et s'écartent", "[render][table]") {
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(18);
  REQUIRE(patterns.size() == 1);
  const EffectNodePattern& spark = patterns.front();
  CHECK(spark.copies == 10);
  // ⚠ SA VIE EST COPIÉE de l'effet, cinquante, et son fondu en est le tiers.
  CHECK(spark.life_ticks == 50);
  CHECK(spark.fade_out_tick == 34);
  // Naissance à quatre à sept unités le long de la direction, dix au-dessus.
  CHECK(spark.launch_distance_low == Catch::Approx(4.0F));
  CHECK(spark.launch_distance_high == Catch::Approx(7.0F));
  CHECK(spark.launch_drop == Catch::Approx(-10.0F));
  // Le pas est TIRÉ, et la table engendrée ne le rend pas : la lecture le
  // porte.
  CHECK(spark.step_low == Catch::Approx(0.5F));
  CHECK(spark.step_high == Catch::Approx(0.99F));
  CHECK(spark.fall_speed_low == Catch::Approx(-1.49F));
  CHECK(spark.fall_acceleration_share == Catch::Approx(2.0F));
  CHECK(spark.sprite_scale_high == Catch::Approx(1.99F));
}

TEST_CASE("EF_PATTACK et EF_DETOXICATION ne jouent qu'UNE branche, et elle monte",
          "[render][table]") {
  // 🔴 LES DEUX BRANCHES PARTAGENT UN BÂTISSEUR, et chacune a son effet : celle
  // du paramètre un pour l'attaque empoisonnée, celle des effets SIMPLIFIÉS pour
  // la désintoxication. Jouer la première doublerait la nuée, la seconde en
  // appauvrirait la cadence — de cinq tics à quinze.
  for (const std::uint32_t each_effect_id : {20U, 21U}) {
    INFO(each_effect_id);
    const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(each_effect_id);
    REQUIRE(patterns.size() == 1);
    CHECK(patterns.front().period_ticks == 5);
    // Le quart de tour de la matrice envoie le pas vers le HAUT : chez nous
    // c'est une chute négative, freinée de moitié, et le cercle de naissance
    // une distance de lancer à plat.
    CHECK(patterns.front().fall_speed_high == Catch::Approx(-0.30F));
    CHECK(patterns.front().fall_speed_low == Catch::Approx(-0.59F));
    CHECK(patterns.front().fall_acceleration_share == Catch::Approx(0.5F));
    CHECK(patterns.front().launch_distance_low == Catch::Approx(2.0F));
    CHECK(patterns.front().launch_distance_high == Catch::Approx(7.0F));
  }
  // Et la désintoxication garde son sprite, qui appartient à sa branche.
  CHECK(ro::render::EffectNodePatternTextures(EffectNodePatternsFor(21).front())
            .front()
            .find("particle2") != std::string_view::npos);
}

TEST_CASE("la boule de feu de EF_SIGHT naît en tournant, son ombre à ses pieds",
          "[render][table]") {
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(22);
  REQUIRE(patterns.size() == 2);
  for (const EffectNodePattern& pattern : patterns) {
    // Un tic sur DEUX — un test de bit —, et une naissance qui balaie un cercle
    // de quinze à moins cinq degrés par tic.
    CHECK(pattern.period_ticks == 2);
    CHECK(pattern.last_tick == 269);
    CHECK(pattern.birth_sweep_radius == Catch::Approx(15.0F));
    CHECK(pattern.birth_sweep_degrees_per_tick == Catch::Approx(-5.0F));
  }
  // ⚠ LA BOULE SEULE EST EN L'AIR, à vingt unités ; l'ombre reste au sol. C'est
  // ce qui demande une lecture par RANG et non par forme.
  CHECK(patterns[0].launch_drop == Catch::Approx(-20.0F));
  CHECK(patterns[1].launch_drop == Catch::Approx(0.0F));
  // 🔴 ET LES DEUX RÉTRÉCISSENT : la boule part de deux et demi et
  // perd un dixième par tic, l'ombre d'un et demi et six centièmes. Sans ce
  // pas, la traînée avait la même taille du bout à l'autre.
  CHECK(patterns[0].sprite_scale == Catch::Approx(2.5F));
  CHECK(patterns[0].sprite_scale_step == Catch::Approx(-0.1F));
  CHECK(patterns[1].sprite_scale == Catch::Approx(1.5F));
  CHECK(patterns[1].sprite_scale_step == Catch::Approx(-0.06F));
  // 🔴 ET LES DEUX PÂLISSENT : la boule perd trois d'alpha par tic, l'ombre un.
  // Ne garder que les pas MONTANTS laisserait à la traînée l'alpha de sa
  // naissance du premier sprite au dernier, là où le client descend à
  // quatre-vingt-treize.
  CHECK(patterns[0].alpha_start == Catch::Approx(150.0F));
  CHECK(patterns[0].alpha_step == Catch::Approx(-3.0F));
  CHECK(patterns[1].alpha_start == Catch::Approx(120.0F));
  CHECK(patterns[1].alpha_step == Catch::Approx(-1.0F));
}

TEST_CASE("EF_RUWACH sème sa particule et son ombre en tournant, cousin de Sight",
          "[render][table]") {
  // 🔴 LE CAS BÂTIT SES NŒUDS LUI-MÊME, sans bâtisseur : son sprite passait pour
  // « choisi par branche » et son ombre n'en avait aucun. L'effet ne semait
  // rien, et Ruwach était invisible autour de son lanceur.
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(33);
  REQUIRE(patterns.size() == 2);
  CHECK(ro::render::EffectNodePatternTextures(patterns[0]).front().find("particle2") !=
        std::string_view::npos);
  CHECK(ro::render::EffectNodePatternTextures(patterns[1]).front().find("Shadow") !=
        std::string_view::npos);
  for (const EffectNodePattern& pattern : patterns) {
    // Un tic sur TROIS, jusqu'à la durée déclarée de deux cents tics, sur un
    // cercle de quinze parcouru à moins cinq degrés tous les 1,3 tic.
    CHECK(pattern.period_ticks == 3);
    CHECK(pattern.last_tick == 199);
    CHECK(pattern.birth_sweep_radius == Catch::Approx(15.0F));
    CHECK(pattern.birth_sweep_degrees_per_tick == Catch::Approx(-5.0F / 1.3F));
    CHECK(pattern.life_ticks == 25);
    CHECK(pattern.fade_out_tick == 19);
  }
  // La particule à dix unités de haut, l'ombre au sol.
  CHECK(patterns[0].launch_drop == Catch::Approx(-10.0F));
  CHECK(patterns[1].launch_drop == Catch::Approx(0.0F));
  CHECK(patterns[0].sprite_scale == Catch::Approx(3.0F));
  CHECK(patterns[0].sprite_scale_step == Catch::Approx(-0.1F));
  CHECK(patterns[0].alpha_start == Catch::Approx(250.0F));
  CHECK(patterns[0].alpha_step == Catch::Approx(-3.0F));
  // Aucune cadence posée pour la particule : les quatre tics du constructeur.
  CHECK(patterns[0].sprite_frame_hold_ticks == 0);
  CHECK(patterns[1].sprite_frame_hold_ticks == 2);
  CHECK(patterns[1].sprite_scale == Catch::Approx(1.5F));
  CHECK(patterns[1].sprite_scale_step == Catch::Approx(-0.06F));
  CHECK(patterns[1].alpha_start == Catch::Approx(150.0F));
  CHECK(patterns[1].alpha_step == Catch::Approx(-2.5F));
}

TEST_CASE("EF_MAGNUMBREAK dresse son dôme de feu entre ses deux couronnes", "[render][table]") {
  // 🔴 LA TABLE DES ANNEAUX NE PORTE PAS CE DESSINATEUR : la sphère du
  // sous-type 14 est lue à la main dans le bâtisseur de Magnum Break, et sans
  // elle Magnum Break n'aurait que ses couronnes.
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(17);
  const auto dome = std::ranges::find_if(patterns, [](const EffectNodePattern& pattern) {
    return pattern.shape == EffectNodeShape::kUvSphere;
  });
  REQUIRE(dome != patterns.end());
  // Le rayon part de zéro et gagne 1,15 par tic, freiné de la moitié du
  // quotient par la vie de l'effet — trente ; le lacet tourne de trois degrés
  // par tic ; le pas angulaire est celui du constructeur.
  CHECK(dome->ring.ring_radius == Catch::Approx(0.0F));
  CHECK(dome->ring.ring_radius_growth == Catch::Approx(1.15F));
  CHECK(dome->ring.ring_radius_acceleration == Catch::Approx(-(1.15F / 30.0F) * 0.5F));
  CHECK(dome->ring.yaw_step_degrees == Catch::Approx(3.0F));
  CHECK(dome->ring.step_degrees == Catch::Approx(36.0F));
  // Sa vie est celle de l'effet, son alpha monte de zéro en quinze tics et
  // redescend sur les quinze derniers.
  CHECK(dome->life_from_effect);
  CHECK(dome->alpha_start == Catch::Approx(0.0F));
  CHECK(dome->alpha_step == Catch::Approx(254.0F / 15.0F));
  CHECK(dome->fade_out_tick == 15);
  CHECK(ro::render::EffectNodePatternTextures(*dome).front().find("대폭발") !=
        std::string_view::npos);
}

TEST_CASE("EF_FROSTDIVER2 dresse HUIT pointes penchées, pas une droite", "[render][table]") {
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(28);
  REQUIRE(patterns.size() == 1);
  const EffectNodePattern& tip = patterns.front();
  CHECK(tip.shape == EffectNodeShape::kCrossSpikes);
  CHECK(tip.ring.instance_count == 8);
  CHECK(tip.ring.lean_degrees == Catch::Approx(-10.0F));
  CHECK(tip.ring.lean_degrees_high == Catch::Approx(9.0F));
  CHECK(tip.ring.draws_its_yaw);
  // Trente unités sous le sol, et un pas qui se fige au vingtième tic.
  CHECK(tip.ring.burial_depth == Catch::Approx(30.0F));
  CHECK(tip.ring.motion_stop_tick == 20);
}

TEST_CASE("une couronne FREINE son rayon, et le relevé ne le voyait pas", "[render][table]") {
  // Le frein `−(pas / vie) · k`, calculé chez presque tous les bâtisseurs. Sans
  // lui l'onde s'étend d'un pas constant, un tiers trop loin.
  const auto crown_of = [](std::uint32_t each_effect_id) -> const EffectNodePattern& {
    const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(each_effect_id);
    const auto found = std::ranges::find_if(patterns, [](const EffectNodePattern& pattern) {
      return pattern.shape == EffectNodeShape::kRingCylinder;
    });
    REQUIRE(found != patterns.end());
    return *found;
  };
  CHECK(crown_of(8).ring.ring_radius_acceleration == Catch::Approx(-(1.25F / 80.0F) * 0.5F));
  CHECK(crown_of(17).ring.ring_radius_acceleration == Catch::Approx(-(1.75F / 30.0F) * 0.5F));
  CHECK(crown_of(36).ring.ring_radius_acceleration == Catch::Approx(-(1.0F / 30.0F) * 0.5F));
  // Et le nœud qui en sort porte bien les trois étages du rayon.
  const ro::render::EffectNode node = ro::render::EffectNodeFromPattern(crown_of(17));
  CHECK(node.ring_radius.growth_acceleration_per_tick == Catch::Approx(-(1.75F / 30.0F) * 0.5F));
  // EF_PORTAL : la couronne renaît tous les quatorze tics et s'efface à
  // mi-vie ; ses deux anneaux s'effacent six tics avant la fin des cent.
  CHECK(crown_of(36).period_ticks == 14);
  CHECK(crown_of(36).fade_out_tick == 15);
  for (const EffectNodePattern& pattern : EffectNodePatternsFor(36)) {
    if (pattern.shape == EffectNodeShape::kRingBand) {
      CHECK(pattern.fade_out_tick == 94);
    }
  }
  for (const EffectNodePattern& pattern : EffectNodePatternsFor(17)) {
    CHECK(pattern.fade_out_tick == 15);
  }
}

TEST_CASE("les FLÈCHES naissent de côté et de haut, et visent leur point", "[render][table]") {
  // EF_ICEARROW et EF_FIREARROW : la même écriture. Une flèche au douzième tic
  // puis tous les dix ; l'anneau d'impact vingt-cinq tics après chacune.
  for (const std::uint32_t each_effect_id : {26U, 31U}) {
    INFO(each_effect_id);
    const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(each_effect_id);
    const auto arrow = std::ranges::find_if(patterns, [](const EffectNodePattern& pattern) {
      return pattern.shape == EffectNodeShape::kCrossedPlanes;
    });
    REQUIRE(arrow != patterns.end());
    CHECK(arrow->ring.aims_at_anchor);
    CHECK(arrow->ring.offset_x == Catch::Approx(25.0F));
    CHECK(arrow->ring.offset_x_high == Catch::Approx(34.0F));
    CHECK(arrow->ring.offset_z == Catch::Approx(15.0F));
    CHECK(arrow->ring.offset_z_high == Catch::Approx(24.0F));
    // Soixante de haut : le client l'écrit en négatif, son ordonnée descend.
    CHECK(arrow->ring.burial_depth == Catch::Approx(-60.0F));
    CHECK(arrow->step_low == Catch::Approx(2.75F));
    CHECK(arrow->first_tick == 12);
    CHECK(arrow->period_ticks == 10);
    // Le bit deux la plante au sol.
    CHECK((arrow->ring.flags & ro::render::kEffectNodeStopsAtGround) != 0);
    // AUTANT DE FLÈCHES QUE DE COUPS : le bâtisseur multiplie le paramètre par
    // dix au douzième tic et sème tant que `âge − 12` reste en
    // dessous. Dix coups, dix flèches ; une pour `@effect` ; et la vie de
    // l'instance, cent soixante, borne le dernier semis au cent cinquante-deux.
    CHECK(arrow->sowings_follow_param);
    CHECK(arrow->last_tick == 152);
    using ro::render::EffectNodePatternSowsAtParam;
    CHECK(EffectNodePatternSowsAtParam(*arrow, 12, 1));
    CHECK_FALSE(EffectNodePatternSowsAtParam(*arrow, 22, 1));
    CHECK(EffectNodePatternSowsAtParam(*arrow, 102, 10));
    CHECK_FALSE(EffectNodePatternSowsAtParam(*arrow, 112, 10));
    const auto ring = std::ranges::find_if(patterns, [](const EffectNodePattern& pattern) {
      return pattern.shape == EffectNodeShape::kRingCylinder;
    });
    REQUIRE(ring != patterns.end());
    CHECK(ring->first_tick == 37);
    CHECK(ring->period_ticks == 10);
    // Un anneau par flèche, vingt-cinq tics après elle, et pas un après la
    // mort de l'instance.
    CHECK(ring->sowings_follow_param);
    CHECK(ring->last_tick == 157);
    CHECK(EffectNodePatternSowsAtParam(*ring, 37, 1));
    CHECK_FALSE(EffectNodePatternSowsAtParam(*ring, 47, 1));
    // ET IL NAÎT ÉTEINT : alpha zéro, puis un dixième du plafond par tic — la
    // brume s'étale en dix tics. Un alpha nul sans pas passerait pour « déjà
    // plein », et l'anneau surgirait.
    CHECK(ring->alpha_start == Catch::Approx(0.0F));
    CHECK(ring->alpha_step == Catch::Approx(25.4F));
  }
}

TEST_CASE("la flèche de feu fait défiler SIX images, une par tic", "[render][table]") {
  // Ses six noms sont COMPOSÉS par le client, et aucun n'est écrit en clair :
  // sans eux, la flèche de feu n'existe pas dans la table.
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(31);
  const auto arrow = std::ranges::find_if(patterns, [](const EffectNodePattern& pattern) {
    return pattern.shape == EffectNodeShape::kCrossedPlanes;
  });
  REQUIRE(arrow != patterns.end());
  const std::span<const std::string_view> images = ro::render::EffectNodePatternTextures(*arrow);
  REQUIRE(images.size() == 6);
  CHECK(images.front().find("1.tga") != std::string_view::npos);
  CHECK(images.back().find("6.tga") != std::string_view::npos);
  CHECK(arrow->ring.frame_hold_ticks == 1);
  CHECK(ro::render::EffectNodeFromPattern(*arrow).frame_hold_ticks == 1);
}

TEST_CASE("l'impact de la flèche de feu est l'anneau JAUNE, pas les images de la flèche",
          "[render][table]") {
  // Les six images de flèche n'appartiennent qu'à la flèche. L'anneau qui
  // s'étale sous chaque impact porte une seule image, un halo blanc-jaune :
  // habillé des images de la flèche, il devenait une couronne de flammèches
  // orange.
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(31);
  const auto ring = std::ranges::find_if(patterns, [](const EffectNodePattern& pattern) {
    return pattern.shape == EffectNodeShape::kRingCylinder;
  });
  REQUIRE(ring != patterns.end());
  const std::span<const std::string_view> images = ro::render::EffectNodePatternTextures(*ring);
  REQUIRE(images.size() == 1);
  CHECK(images.front() == "data\\texture\\effect\\ring_yellow.tga");
}

TEST_CASE("EF_FIREBALL lance quatre boules qui VISENT la cible", "[render][table]") {
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(24);
  REQUIRE(patterns.size() == 1);
  const EffectNodePattern& ball = patterns.front();
  CHECK(ball.aims_at_target);
  // Aux tics vingt, vingt-quatre, vingt-huit et trente-deux.
  CHECK(ball.first_tick == 20);
  CHECK(ball.last_tick == 32);
  CHECK(ball.period_ticks == 4);
  // 🔴 QUINZE UNITÉS AU-DESSUS DU LANCEUR, et non six : le décalage vertical
  // du nœud vaut moins quinze dans le repère du client, qui descend. Neuf
  // unités d'écart, presque deux cellules : les boules traverseraient le
  // lanceur à sa taille au lieu de lui survoler la tête.
  CHECK(ball.launch_drop == Catch::Approx(-15.0F));
  CHECK(ball.life_ticks == 20);
  CHECK(ball.color[0] == 246);
}

TEST_CASE("Fire Ball : la traînée s'éteint PAR L'ARRIÈRE", "[render][table]") {
  // 🔴 LES QUATRE BOULES NE SE RESSEMBLENT PAS. Le bâtisseur de Fire Ball garde
  // la PREMIÈRE telle que le constructeur la pose — blanche, alpha deux cent
  // cinquante-quatre — et n'écrit couleur et alpha que pour les trois
  // suivantes, derrière la garde « tic différent de vingt ». Une seule teinte
  // pour les quatre donne quatre boules identiques, et la tête ne mène plus
  // rien.
  const ro::render::EffectTintByRank* head = ro::render::EffectTintByRankFor(24, 1);
  REQUIRE(head != nullptr);
  CHECK(head->color == std::array<std::uint8_t, 3>{255, 255, 255});
  CHECK(head->alpha == Catch::Approx(254.0F));

  // ⚠ ET L'ALPHA DESCEND, rang après rang : cent quatre-vingts au tic
  // vingt-quatre, cent trente au vingt-huit, quatre-vingts au trente-deux.
  const std::array<float, 3> suivantes = {180.0F, 130.0F, 80.0F};
  for (std::size_t rang = 0; rang < suivantes.size(); ++rang) {
    const ro::render::EffectTintByRank* ball =
        ro::render::EffectTintByRankFor(24, static_cast<std::int32_t>(rang) + 2);
    INFO("rang " << rang + 2);
    REQUIRE(ball != nullptr);
    CHECK(ball->color == std::array<std::uint8_t, 3>{246, 199, 76});
    CHECK(ball->alpha == Catch::Approx(suivantes[rang]));
  }

  // Il n'y en a que quatre : la cinquième n'existe pas, et aucun autre effet
  // n'éteint sa traînée ainsi.
  CHECK(ro::render::EffectTintByRankFor(24, 5) == nullptr);
  CHECK(ro::render::EffectTintByRankFor(24, 0) == nullptr);
  CHECK(ro::render::EffectTintByRankFor(15, 1) == nullptr);
}

TEST_CASE("Le sprite qui se COUCHE sur son vol : viser n'est pas s'orienter", "[render][table]") {
  // 🔴 DEUX CHOSES QUE LE MÊME NŒUD PEUT FAIRE SÉPARÉMENT. Viser, c'est PARTIR
  // vers sa cible ; s'orienter, c'est coucher son IMAGE le long de ce vol. Le
  // client les écrit dans deux champs — le pas et le cap pour le premier, la
  // face du nœud pour le second —, et neuf bâtisseurs seulement font les deux.
  //
  // ⚠ SANS LE SECOND, LA BOULE POINTE VERS LE HAUT quelle que soit la direction
  // du tir : son image est dessinée debout. Le joueur l'a vu en se tenant DE
  // CÔTÉ par rapport à sa cible.
  const std::span<const EffectNodePattern> balls = EffectNodePatternsFor(24);
  REQUIRE(balls.size() == 1);
  CHECK(balls.front().aims_at_target);
  CHECK(balls.front().turns_with_flight);

  // LES BOULES DE 80 ET 751 FONT LES DEUX AUSSI : la compétence leur donne le
  // point visé aussitôt nées, et leur bâtisseur écrit la face ET le pas vers lui.
  for (const std::uint32_t effect_id : {80U, 751U}) {
    const std::span<const EffectNodePattern> own_entries = EffectNodePatternsFor(effect_id);
    INFO("effet " << effect_id);
    REQUIRE(!own_entries.empty());
    CHECK(own_entries.front().turns_with_flight);
    CHECK(own_entries.front().aims_at_target);
    CHECK(own_entries.front().aim_heading_on_client_table);
    CHECK(own_entries.front().dies_near_target);
  }
  CHECK(EffectNodePatternsFor(80).front().life_ticks == 15);

  // ET CE N'EST PAS LE CAS ORDINAIRE : les pointes de Frost Diver ne se couchent
  // sur rien, et les âmes de Soul Strike visent sans que leur image tourne.
  CHECK_FALSE(EffectNodePatternsFor(28).front().turns_with_flight);
  CHECK_FALSE(EffectNodePatternsFor(15).front().turns_with_flight);
}

TEST_CASE("les âmes de EF_SOULSTRIKE VOLENT vers leur cible, elles ne plongent pas",
          "[render][table]") {
  // 🔴 LE SOUS-TYPE HUIT NE FAIT PAS BOUGER QUE LA HAUTEUR. Son mouvement
  // n'ajoute JAMAIS sa « chute » à l'ordonnée : il la retranche au pas, et fait
  // avancer le nœud le long de sa face — tournée vers le point visé par le
  // bâtisseur. L'âme vole donc du mage au monstre ; `@effect`, dont les deux
  // bouts sont le même point, ne le montre pas.
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(15);
  REQUIRE(patterns.size() == 1);
  const EffectNodePattern& soul = patterns.front();
  CHECK(soul.life_ticks == 40);
  CHECK(soul.launch_drop == Catch::Approx(-10.0F));
  CHECK(soul.aims_at_target);
  CHECK(soul.brakes_along_axis);
  // Le reste — frein, balancement, semis par paramètre — est lu dans l'essai
  // « ses boules suivent le PARAMÈTRE ».
}

TEST_CASE("un nœud qui s'arrête au sol s'y PLANTE, puis s'efface", "[render][table]") {
  ro::render::PooledEffectNode arrow;
  arrow.node.life_ticks = 70;
  arrow.node.step_length = 2.75F;
  arrow.ground_position = {0.0F, 60.0F, 0.0F};
  // Elle descend tout droit vers un sol à zéro.
  arrow.launch_direction = {0.0F, -1.0F, 0.0F};
  arrow.stops_at_floor = true;
  arrow.floor_y = 0.0F;
  int ticks = 0;
  while (arrow.stops_at_floor && ticks < 100) {
    ro::render::AdvanceLaunchedNode(arrow, ro::render::EffectScatterWorld{});
    ++ticks;
  }
  // Soixante unités à 2,75 par tic : vingt-deux tics, et elle ne bouge plus.
  CHECK(ticks == 22);
  CHECK(arrow.node.step_length == Catch::Approx(0.0F));
  CHECK(arrow.node.age_ticks == 70 - ro::render::kEffectNodeRestAfterImpactTicks);
  const glm::vec3 plantee = arrow.ground_position;
  ro::render::AdvanceLaunchedNode(arrow, ro::render::EffectScatterWorld{});
  CHECK(arrow.ground_position == plantee);
}

TEST_CASE("EF_SOULSTRIKE : ses boules suivent le PARAMÈTRE, freinent sur l'axe et se balancent",
          "[render][table]") {
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(15);
  const auto ball = std::ranges::find_if(patterns, [](const EffectNodePattern& pattern) {
    return pattern.shape == EffectNodeShape::kSprite;
  });
  REQUIRE(ball != patterns.end());
  // Elle VISE : tournée vers le point visé, son pas est la distance divisée par
  // sa vie. Sans cible, elle ne part pas.
  CHECK(ball->aims_at_target);
  // 🔴 LA « CHUTE » N'EN EST PAS UNE ICI : le mouvement du sous-type 8 la
  // RETRANCHE au pas. Trois et demi, freiné de deux fois le quotient par la
  // vie — lu comme une chute, il ferait plonger la boule.
  CHECK(ball->brakes_along_axis);
  CHECK(ball->fall_speed_low == Catch::Approx(3.5F));
  CHECK(ball->fall_acceleration_share == Catch::Approx(2.0F));
  // Le balancement : moins deux par tic, freiné de même.
  CHECK(ball->swing_speed_low == Catch::Approx(-2.0F));
  CHECK(ball->swing_acceleration_share == Catch::Approx(2.0F));
  // Née dix unités au-dessus de l'instance.
  CHECK(ball->launch_drop == Catch::Approx(-10.0F));
  // ET DOUZE TRACES DE TRAÎNÉE : la boule se peint à ses douze dernières
  // positions. Ce n'est pas un compte d'exemplaires.
  CHECK(ball->trail_trace_count == 12);
  // ET SON SON SE JOUE À CHAQUE BOULE : il est dans la garde du semis, non au
  // tic zéro. Cinq coups, cinq sons.
  CHECK(ball->sound_at_sowing);
  CHECK(ro::render::EffectSoundPlaysAtSowing(15));
  CHECK_FALSE(ro::render::EffectSoundPlaysAtSowing(14));

  // UNE TOUS LES ONZE TICS DÈS LE SIXIÈME, autant que le paramètre, sept au plus.
  CHECK(ball->first_tick == 6);
  CHECK(ball->period_ticks == 11);
  // ⚠ AVEC SA PHASE : `(âge + 5) % 11`, et non les multiples de onze.
  CHECK(ball->period_phase_ticks == 5);
  CHECK(ball->last_tick == 72);
  CHECK(ball->sowings_follow_param);
  CHECK(ball->param_cap == 7);
  using ro::render::EffectNodePatternSowingRank;
  using ro::render::EffectNodePatternSowsAtParam;
  // Zéro : le défaut du `switch`, aucune boule. Un : `@effect 15`, une seule.
  CHECK_FALSE(EffectNodePatternSowsAtParam(*ball, 6, 0));
  CHECK(EffectNodePatternSowsAtParam(*ball, 6, 1));
  CHECK_FALSE(EffectNodePatternSowsAtParam(*ball, 17, 1));
  // Cinq : Soul Strike au niveau dix, où le serveur frappe cinq fois.
  CHECK(EffectNodePatternSowsAtParam(*ball, 50, 5));
  CHECK_FALSE(EffectNodePatternSowsAtParam(*ball, 61, 5));
  // Au-delà des sept cas écrits, le `switch` retombe sur son défaut : rien.
  CHECK_FALSE(EffectNodePatternSowsAtParam(*ball, 6, 8));
  CHECK(EffectNodePatternSowingRank(*ball, 17) == 2);
  CHECK(EffectNodePatternSowingRank(*ball, 7) == 0);

  // LE ROULIS PAR PARAMÈTRE, `base + écart × rang` : à deux, les
  // deux boules se balancent à l'opposé l'une de l'autre.
  using ro::render::EffectRollByParamFor;
  const ro::render::EffectRollByParam* single = EffectRollByParamFor(15, 1);
  REQUIRE(single != nullptr);
  CHECK(single->base_degrees == Catch::Approx(0.0F));
  CHECK(single->spread_degrees == Catch::Approx(1.0F));
  const ro::render::EffectRollByParam* cinq = EffectRollByParamFor(15, 5);
  REQUIRE(cinq != nullptr);
  CHECK(cinq->base_degrees == Catch::Approx(-90.0F));
  CHECK(cinq->spread_degrees == Catch::Approx(45.0F));
  CHECK(EffectRollByParamFor(15, 8) == nullptr);
  CHECK(EffectRollByParamFor(14, 1) == nullptr);
}

TEST_CASE(
    "la traînée d'un sprite se lit au champ 500 : trois traces pour un impact, une par défaut",
    "[render][table]") {
  // Trois bâtisseurs seulement posent ce compte : 3 chez les deux étincelles
  // d'impact, 12 chez Soul Strike. Un nœud dont le bâtisseur n'y touche pas
  // garde le UN du constructeur, et se dessine une fois.
  const auto sprite_de = [](std::uint32_t each_effect_id) {
    const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(each_effect_id);
    return std::ranges::find_if(patterns, [](const EffectNodePattern& pattern) {
      return pattern.shape == EffectNodeShape::kSprite;
    });
  };
  const auto spark_hit3 = sprite_de(2);
  REQUIRE(spark_hit3 != EffectNodePatternsFor(2).end());
  CHECK(spark_hit3->trail_trace_count == 3);
  const auto spark_hit4 = sprite_de(3);
  REQUIRE(spark_hit4 != EffectNodePatternsFor(3).end());
  CHECK(spark_hit4->trail_trace_count == 3);
  const auto heal_spark = sprite_de(7);
  REQUIRE(heal_spark != EffectNodePatternsFor(7).end());
  CHECK(heal_spark->trail_trace_count == 1);
}

TEST_CASE("les étincelles d'impact fondent dès leur naissance et se réduisent à rien",
          "[render][table]") {
  // Le fondu commence au tic zéro : l'alpha descend sur toute la vie. Et le
  // pas d'échelle vaut `−(échelle / vie)` : l'échelle tirée tombe à zéro à la
  // mort. « Pas de sortie », lui, est moins un.
  for (const std::uint32_t each_effect_id : {2U, 3U}) {
    const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(each_effect_id);
    const auto spark = std::ranges::find_if(patterns, [](const EffectNodePattern& pattern) {
      return pattern.shape == EffectNodeShape::kSprite;
    });
    INFO(each_effect_id);
    REQUIRE(spark != patterns.end());
    CHECK(spark->fade_out_tick == 0);
    CHECK(spark->sprite_scale_step_share == Catch::Approx(-1.0F));
  }
}

TEST_CASE("Blessing : une couronne, une lueur qui pâlit, des étincelles qui tombent en clignotant",
          "[render][table]") {
  // La couronne porte le bit 1, qui la rend PLEINE ; sans lui, elle ne
  // paraîtrait pas.
  const std::span<const EffectNodePattern> benir = EffectNodePatternsFor(42);
  REQUIRE(benir.size() == 3);
  CHECK(benir[0].shape == EffectNodeShape::kRingCylinder);
  CHECK(benir[0].fade_out_tick == 120);
  // Les étincelles : une tous les quatre tics, qui DESCENDENT, et le va-et-vient
  // d'alpha (bit 0x80).
  const EffectNodePattern& sparks = benir[2];
  CHECK(sparks.period_ticks == 4);
  CHECK(sparks.fall_speed_low == Catch::Approx(0.2F));
  CHECK((sparks.sprite_flags & ro::render::kEffectNodeAlphaBounces) != 0U);
}

TEST_CASE("un éclat qui lève le bit d'arrêt s'arrête au tic 468", "[render][table]") {
  // Le bit d'arrêt, et l'arrêt au tic 20 : le mouvement du sous-type 9 met le
  // pas à zéro au vingtième tic. Sans cet arrêt, le pas devenu négatif
  // ramènerait les éclats d'Ice Wall sous le sol en moins d'une seconde.
  const std::span<const EffectNodePattern> wall = EffectNodePatternsFor(74);
  REQUIRE_FALSE(wall.empty());
  CHECK(wall.front().ring.motion_stop_tick == 20);
  // Stone Tri Spikes s'arrête au dixième.
  const std::span<const EffectNodePattern> trepied = EffectNodePatternsFor(132);
  REQUIRE_FALSE(trepied.empty());
  CHECK(trepied.front().ring.motion_stop_tick == 10);
}

TEST_CASE("les couronnes de Magnum Break et de Water Ball naissent transparentes",
          "[render][table]") {
  // Le pas d'alpha vaut `plafond / 15` : l'alpha part de zéro et atteint le
  // plafond en quinze tics. La table engendrée rend ce pas « calculé ».
  for (const std::uint32_t each_effect_id : {17U, 942U}) {
    INFO("effet " << each_effect_id);
    std::size_t couronnes = 0;
    for (const EffectNodePattern& pattern : EffectNodePatternsFor(each_effect_id)) {
      if (pattern.shape != EffectNodeShape::kRingCylinder) {
        continue;
      }
      ++couronnes;
      CHECK(pattern.alpha_start == Catch::Approx(0.0F));
      CHECK(pattern.alpha_step == Catch::Approx(254.0F / 15.0F));
      CHECK(pattern.ring.ring_radius_acceleration < 0.0F);
    }
    CHECK(couronnes == 2);
  }
}

TEST_CASE("EF_MAGNUMBREAK 170 et Water Ball dressent eux aussi leur dôme", "[render][table]") {
  // Les nœuds du sous-type 14 que la table des anneaux ne porte pas : celui de
  // l'effet 170 (feu, vie vingt, pas 1,35, alpha vers 240 d'un septième par
  // tic) et celui de l'effet 942 (glace, vie trente, pas 1,15).
  const auto dome_de = [](std::uint32_t each_effect_id) -> const EffectNodePattern* {
    for (const EffectNodePattern& pattern : EffectNodePatternsFor(each_effect_id)) {
      if (pattern.shape == EffectNodeShape::kUvSphere) {
        return &pattern;
      }
    }
    return nullptr;
  };
  const EffectNodePattern* feu = dome_de(170);
  REQUIRE(feu != nullptr);
  CHECK(feu->life_ticks == 20);
  CHECK(feu->ring.ring_radius_growth == Catch::Approx(1.35F));
  CHECK(feu->alpha_max == Catch::Approx(240.0F));
  CHECK(feu->alpha_step == Catch::Approx(240.0F / 7.0F));
  const EffectNodePattern* glace = dome_de(942);
  REQUIRE(glace != nullptr);
  CHECK(glace->life_ticks == 30);
  CHECK(glace->ring.ring_radius_growth == Catch::Approx(1.15F));
  CHECK(ro::render::EffectNodePatternTextures(*glace).front().find("ice.tga") !=
        std::string_view::npos);
}

TEST_CASE("une vie de mille tics est une durée, pas une sentinelle", "[render][table]") {
  // La fabrique recopie mille tics pour ses types 61, 63 et 64 ; les prendre
  // pour « jusqu'à ce qu'on l'enlève » les faisait vivre cent tics.
  for (const std::uint32_t each_effect_id : {912U, 919U, 921U}) {
    INFO("effet " << each_effect_id);
    REQUIRE_FALSE(EffectNodePatternsFor(each_effect_id).empty());
    CHECK(EffectNodePatternsFor(each_effect_id).front().life_ticks == 1000);
  }
}

TEST_CASE("les bâtisseurs Effect_Init qui écrivent la face se tournent sur leur vol",
          "[render][table]") {
  // `BATISSEURS_QUI_ECRIVENT_LA_FACE` nomme les deux boules de feu et le sprite
  // directionnel vert ; un filtre sur le préfixe de leur étiquette les en
  // écartait.
  for (const std::uint32_t each_effect_id : {891U, 913U}) {
    INFO("effet " << each_effect_id);
    REQUIRE_FALSE(EffectNodePatternsFor(each_effect_id).empty());
    CHECK(EffectNodePatternsFor(each_effect_id).front().turns_with_flight);
  }
  // ⚠ SAUF LE SPRITE DIRECTIONNEL VERT (935) : il écrit la face, mais son
  // drapeau n'a pas le bit 0x20 qui fait coucher le quad sur le vol. C'est son
  // ACTION qui montre la direction.
  REQUIRE_FALSE(EffectNodePatternsFor(935).empty());
  CHECK_FALSE(EffectNodePatternsFor(935).front().turns_with_flight);
  CHECK(EffectNodePatternsFor(935).front().action_from_aim_quadrant);
}

TEST_CASE("Le coup d'Investigate montre par son action la direction de la cible",
          "[render][table]") {
  // 🔴 LE MÊME BÂTISSEUR POUR QUATRE EFFETS, ET LA MÊME QUEUE : une action par
  // quart de tour de l'angle du point visé vers l'instance, caméra comprise —
  // 2, 1, 0 puis 3. Sans elle, le sprite partait toujours du même côté.
  constexpr ro::render::EffectActionsByQuarter kInvestigateQuarters = {2, 1, 0, 3};
  for (const std::uint32_t each_effect_id : {267U, 606U, 607U, 723U}) {
    INFO("effet " << each_effect_id);
    REQUIRE_FALSE(EffectNodePatternsFor(each_effect_id).empty());
    CHECK(EffectNodePatternsFor(each_effect_id).front().action_from_aim_quadrant);
    CHECK(EffectNodePatternsFor(each_effect_id).front().aim_quadrant_actions ==
          kInvestigateQuarters);
    // ET IL SE TIENT SUR LE POINT VISÉ, pas sur le lanceur où naît son effet.
    CHECK(ro::render::EffectNodeSeedFromPattern(EffectNodePatternsFor(each_effect_id).front(), 1, 0,
                                                0)
              .sits_on_aim_point);
  }
}

TEST_CASE("EF_SOULSTRIKE2 est l'âme de Soul Strike dans une autre image", "[render][table]") {
  // Le même bâtisseur que Soul Strike, appelé avec un : son argument choisit
  // `particle5` au lieu de `particle1`, et rien d'autre ne change — visée,
  // frein et horaire compris.
  const std::span<const EffectNodePattern> autres = EffectNodePatternsFor(451);
  const std::span<const EffectNodePattern> premieres = EffectNodePatternsFor(15);
  REQUIRE(autres.size() == 1);
  REQUIRE(premieres.size() == 1);
  const EffectNodePattern& soul = autres.front();
  const EffectNodePattern& modele = premieres.front();
  CHECK(soul.aims_at_target);
  CHECK(soul.brakes_along_axis);
  CHECK(soul.sound_at_sowing);
  CHECK(soul.first_tick == modele.first_tick);
  CHECK(soul.period_ticks == modele.period_ticks);
  CHECK(soul.period_phase_ticks == modele.period_phase_ticks);
  CHECK(soul.launch_drop == Catch::Approx(modele.launch_drop));
  CHECK(ro::render::EffectNodePatternTextures(soul).front().find("particle5") !=
        std::string_view::npos);
}

TEST_CASE("les arcs de Blood Drain et d'Energy Drain partent ensemble, roulés en éventail",
          "[render][table]") {
  // Les deux bâtisseurs d'arc, l'ordinaire et le bleu : une boucle d'autant de
  // tours que le paramètre, chaque arc roulé de −90 puis de 90 de plus, visant
  // d'un pas DOUBLE.
  for (const std::uint32_t each_effect_id : {216U, 217U, 1052U}) {
    INFO("effet " << each_effect_id);
    const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(each_effect_id);
    REQUIRE(patterns.size() == 1);
    const EffectNodePattern& arc = patterns.front();
    CHECK(arc.copies == (each_effect_id == 1052U ? 1U : 3U));
    CHECK(arc.aims_at_target);
    CHECK(arc.brakes_along_axis);
    CHECK(arc.aim_step_factor == Catch::Approx(2.0F));
    CHECK(arc.copy_roll_start_degrees == Catch::Approx(-90.0F));
    CHECK(arc.copy_roll_step_degrees == Catch::Approx(90.0F));
    CHECK(arc.fall_speed_low == Catch::Approx(2.0F));
    // Et la graine les emporte.
    const ro::render::EffectNodeSeed seed = ro::render::EffectNodeSeedFromPattern(arc, 60, 0, 0);
    CHECK(seed.aim_step_factor == Catch::Approx(2.0F));
    CHECK(seed.copy_roll_step_degrees == Catch::Approx(90.0F));
  }
}
