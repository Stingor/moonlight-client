#include "ro/render/effect_node_pattern_table.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_cross_spikes.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/effect_rising_spark_seed.h"
#include "ro/render/effect_uv_sphere.h"

using ro::render::DrawableProceduralEffectOfSkill;
using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;
using ro::render::EffectNodePatternTable;
using ro::render::EffectNodeShape;
using ro::render::KnowsProceduralEffect;
using ro::render::ProceduralEffectsOfSkill;
using ro::render::ProceduralSkillEffectTable;
using ro::render::SkillEffectOn;

TEST_CASE("Patrons de nœud : la table est TRIÉE et ses entrées se tiennent", "[render][table]") {
  const std::span<const EffectNodePattern> table = EffectNodePatternTable();
  REQUIRE_FALSE(table.empty());

  for (const EffectNodePattern& pattern : table) {
    // Une entrée sans ressource ferait un nœud INVISIBLE, ce qui est pire qu'une
    // absence : on croirait l'effet traité.
    const std::span<const std::string_view> textures =
        ro::render::EffectNodePatternTextures(pattern);
    REQUIRE_FALSE(textures.empty());
    for (const std::string_view path : textures) {
      CHECK_FALSE(path.empty());
    }
    // UN SPRITE N'EN A QU'UNE : son couple .spr/.act porte ses images lui-même.
    if (pattern.shape == EffectNodeShape::kSprite) {
      CHECK(textures.size() == 1);
    }
    CHECK(pattern.life_ticks > 0);
    CHECK(pattern.alpha_max > 0.0F);
    // La sortie est un TIC de la vie, pas une durée : au-delà elle ne se
    // produirait jamais.
    CHECK(pattern.fade_out_tick < pattern.life_ticks);
    if (pattern.shape == EffectNodeShape::kRingBand ||
        pattern.shape == EffectNodeShape::kRingCylinder) {
      // Un anneau sans pas angulaire ne se dessinerait pas. Les plans croisés,
      // eux, n'ont pas de tour à découper.
      CHECK(pattern.ring.step_degrees > 0.0F);
    }
    if (pattern.shape == EffectNodeShape::kRingBand) {
      // Un anneau sans aucun rayon ne se verrait pas.
      CHECK((pattern.ring.radius_at_anchor > 0.0F || pattern.ring.radius_at_height > 0.0F));
    }
    if (pattern.shape == EffectNodeShape::kOrientedQuad) {
      // Deux demi-extensions, et une tranche de texture qui va du bas vers le
      // haut : inversée, elle retournerait l'image.
      //
      // ⚠ UNE EXTENSION NULLE N'EST PAS UNE ABSENCE D'AIRE : un quad peut PARTIR
      // de zéro et s'ouvrir, comme l'onde d'une couronne. La rune au sol de
      // Skill Ground Ring naît sans taille et gagne une unité et demie par tic ;
      // exiger la taille de naissance l'écartait, elle et six autres.
      CHECK((pattern.ring.half_width > 0.0F || pattern.ring.half_width_growth != 0.0F));
      CHECK((pattern.ring.height > 0.0F || pattern.ring.height_growth != 0.0F));
      CHECK(pattern.ring.texture_v_min < pattern.ring.texture_v_max);
    }
    if (pattern.shape == EffectNodeShape::kCrossedPlanes) {
      // Deux demi-extensions, et les deux comptent : un plan sans largeur ou
      // sans hauteur est un trait.
      CHECK(pattern.ring.half_width > 0.0F);
      CHECK(pattern.ring.height > 0.0F);
    }
    if (pattern.shape == EffectNodeShape::kRingCylinder) {
      // UNE COURONNE SE JUGE AUTREMENT : son rayon part souvent de zéro et
      // s'étend, et c'est le rayon À L'ANCRE qui lui donne son épaisseur — le
      // mouvement la réécrit comme le plus petit des deux. Un rayon à l'ancre
      // nul ferait une couronne d'épaisseur nulle.
      //
      // ⚠ UN NÉGATIF, LUI, SE VOIT : le bord intérieur passe au-delà du rayon et
      // la bande s'ouvre vers l'extérieur — l'onde jaune de Sight Trasher.
      //
      // 🔴 SAUF SI ELLE EST PLEINE. Le bit 0x1 la remplit jusqu'au centre, et le
      // rayon intérieur n'a alors plus de sens : l'exiger positif écartait le
      // socle d'un point de téléportation, qui n'en a pas.
      if (!ro::render::EffectNodeFillsRing(pattern.ring.flags)) {
        CHECK(pattern.ring.radius_at_anchor != 0.0F);
      }
      CHECK((pattern.ring.ring_radius > 0.0F || pattern.ring.ring_radius_growth != 0.0F));
    }
  }
  CHECK(std::ranges::is_sorted(table, {}, &EffectNodePattern::effect_id));
}

TEST_CASE("Patrons de nœud : TOUS ceux d'un effet se rendent ensemble", "[render][table]") {
  // Heal dresse une colonne de lumière ET lâche des particules. N'en rendre
  // qu'un tronquait l'effet sans rien dire — le soin n'avait plus sa colonne.
  const std::span<const EffectNodePattern> heal = EffectNodePatternsFor(7);
  REQUIRE(heal.size() == 2);

  const EffectNodePattern& ring = heal[0];
  CHECK(ring.shape == EffectNodeShape::kRingBand);
  CHECK(ro::render::EffectNodePatternTextures(ring).front() ==
        "data\\texture\\effect\\alpha_down.tga");
  CHECK(ring.life_ticks == 100);
  CHECK(ring.alpha_max == Catch::Approx(90.0F));
  // Un cylindre : les deux rayons sont égaux, et la colonne monte de trente-cinq.
  CHECK(ring.ring.radius_at_anchor == Catch::Approx(4.5F));
  CHECK(ring.ring.radius_at_height == Catch::Approx(4.5F));
  CHECK(ring.ring.height == Catch::Approx(35.0F));
  // Le pas vient du CONSTRUCTEUR du client, pas de l'effet.
  CHECK(ring.ring.step_degrees == Catch::Approx(36.0F));

  CHECK(heal[1].shape == EffectNodeShape::kSprite);
}

TEST_CASE("Patrons de nœud : l'IMPACT en a un, et il n'est la compétence de personne",
          "[render][table]") {
  // Hit3 et Hit4 se voient à CHAQUE COUP PORTÉ, et le serveur les annonce par
  // leur seul identifiant d'effet. Une table indexée par compétence ne pouvait
  // pas les porter : c'est la raison de l'indexation par l'effet.
  const std::span<const EffectNodePattern> hit3 = EffectNodePatternsFor(2);
  REQUIRE_FALSE(hit3.empty());
  CHECK(hit3[0].shape == EffectNodeShape::kRingBand);
  CHECK(ro::render::EffectNodePatternTextures(hit3[0]).front() ==
        "data\\texture\\effect\\lens2.tga");
  CHECK(hit3[0].life_ticks == 15);
  // La hauteur PART DE ZÉRO et enfle : c'est l'anneau qui s'écarte du point
  // d'impact.
  CHECK(hit3[0].ring.height == Catch::Approx(0.0F));
  CHECK(hit3[0].ring.height_growth > 0.0F);

  CHECK(ProceduralEffectsOfSkill(0).empty());
  CHECK(DrawableProceduralEffectOfSkill(0) == nullptr);
}

TEST_CASE("Patrons de nœud : l'ONDE de Magnum Break part de zéro", "[render][table]") {
  // Elle ne pose PAS son rayon, seulement son pas : une couronne qui naîtrait
  // déjà large ne serait pas une onde de choc.
  const std::span<const EffectNodePattern> wave = EffectNodePatternsFor(170);
  REQUIRE_FALSE(wave.empty());
  CHECK(wave[0].shape == EffectNodeShape::kRingCylinder);
  CHECK(wave[0].ring.ring_radius == Catch::Approx(0.0F));
  CHECK(wave[0].ring.ring_radius_growth == Catch::Approx(1.75F));
  // Cinq d'épaisseur, une fois le trou ouvert.
  CHECK(wave[0].ring.radius_at_anchor == Catch::Approx(5.0F));
  CHECK(wave[0].life_ticks == 20);
}

// Combien de tics le filet observe au plus. Voir la sentinelle, plus bas.
constexpr std::int32_t kObservedTicks = 600;

TEST_CASE("Patrons de nœud : CHACUN donne une figure qui a une aire", "[render][table]") {
  // LE TEST DE BOUT EN BOUT DE LA TABLE : du patron relevé jusqu'aux quads.
  //
  // Une entrée peut être complète au sens du crible — ressource nommée, champs
  // posés — et ne rien dessiner quand même, parce qu'un champ manquant laisse
  // une extension nulle. Cela ne se voyait qu'à l'œil, un effet à la fois, en
  // lançant le jeu ; ici toute la table y passe.
  const auto area = [](const ro::render::EffectWorldQuad& quad) {
    // 🔴 DEUX TRIANGLES, ET NON UN PRODUIT VECTORIEL SUR DEUX CÔTÉS. Cette
    // mesure partait de l'arête `c0→c1` et rendait ZÉRO dès qu'elle était nulle
    // — ce qui est le cas de TOUT segment d'un disque plein : ses deux coins
    // intérieurs sont le centre, confondus. La figure était là, l'instrument ne
    // la voyait pas.
    const auto triangle = [](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
      return glm::length(glm::cross(b - a, c - a));
    };
    return triangle(quad.corners[0], quad.corners[1], quad.corners[2]) +
           triangle(quad.corners[0], quad.corners[2], quad.corners[3]);
  };

  const glm::vec3 anchor{0.0F, 0.0F, 0.0F};
  const auto compose = [&anchor](const EffectNodePattern& pattern,
                                 const ro::render::EffectNode& node) {
    switch (pattern.shape) {
      case EffectNodeShape::kRingBand:
        return ro::render::ComposeEffectNodeRingBand(node, anchor, ro::render::kEffectRingUpright);
      case EffectNodeShape::kRingCylinder:
        return ro::render::ComposeEffectNodeRingCylinder(node, anchor,
                                                         ro::render::kEffectRingUpright);
      case EffectNodeShape::kCrossedPlanes:
        return ro::render::ComposeEffectNodeCrossedPlanes(node, anchor,
                                                          ro::render::kEffectRingUpright);
      case EffectNodeShape::kOrientedQuad:
        return ro::render::ComposeEffectNodeOrientedQuad(node, anchor,
                                                         ro::render::kEffectRingUpright);
      case EffectNodeShape::kCrossSpikes:
        // ⚠ LA SEULE FORME DE CETTE TABLE QUI LISE UNE MARQUE DE PISTE, et un
        // bâtisseur sur neuf la pose. Zéro est le cas des huit autres.
        return ro::render::ComposeCrossSpikeQuads(ro::render::EffectCrossSpikeRequestOf(node, 0),
                                                  anchor);
      case EffectNodeShape::kUvSphere:
        return ro::render::ComposeEffectNodeUvSphere(node, anchor, 0.0F);
      case EffectNodeShape::kSprite:
        break;
    }
    return std::vector<ro::render::EffectWorldQuad>{};
  };

  for (const EffectNodePattern& pattern : EffectNodePatternTable()) {
    if (pattern.shape == EffectNodeShape::kSprite) {
      continue;
    }
    INFO("effet " << pattern.effect_id << ", forme " << static_cast<int>(pattern.shape)
                  << ", rayon " << pattern.ring.ring_radius << ", ancre "
                  << pattern.ring.radius_at_anchor << ", drapeaux " << pattern.ring.flags);
    ro::render::EffectNode node = ro::render::EffectNodeFromPattern(pattern);

    // ⚠ À UN MOMENT DE SA VIE, et non au premier tic. Deux raisons, et la
    // seconde a coûté un test :
    //
    // UNE ONDE NAÎT À RAYON NUL et s'étend — Magnum Break et Water Ball partent
    // d'un point, et les juger sur leur naissance les déclarerait vides.
    //
    // ET UN NŒUD NAÎT SOUVENT TRANSPARENT. Vingt-six des soixante-dix-neuf
    // anneaux relevés partent d'un alpha bas et montent ; exiger qu'ils soient
    // visibles dès le premier tic, ce que ce test faisait, revenait à leur
    // interdire d'apparaître en fondu.
    bool view = false;
    bool visible = false;
    // ⚠ UNE VIE DE CENT MILLIONS DE TICS EST UNE SENTINELLE, pas une durée : le
    // client la pose sur les nœuds qui n'expirent pas d'eux-mêmes, et deux
    // patrons relevés la portent. Parcourir leur vie entière ferait tourner ce
    // test cent millions de fois ; quelques secondes de vie suffisent à juger
    // qu'une figure a une aire et qu'elle paraît.
    const std::int32_t observes = std::min(pattern.life_ticks, kObservedTicks);
    for (std::int32_t tick = 0; tick < observes; ++tick) {
      const std::vector<ro::render::EffectWorldQuad> quads = compose(pattern, node);
      REQUIRE_FALSE(quads.empty());
      view = view || std::ranges::any_of(quads, [&area](const ro::render::EffectWorldQuad& quad) {
               return area(quad) > 0.0F;
             });
      visible = visible || ro::render::EffectNodeAlpha(node) > 0;
      if ((view && visible) || !ro::render::AdvanceEffectNode(node)) {
        break;
      }
    }
    CHECK(view);
    // UN NŒUD QUI RESTE TRANSPARENT TOUTE SA VIE ne se verra jamais, et une
    // figure qui a une aire sans jamais paraître est un patron incomplet.
    CHECK(visible);
  }
}

TEST_CASE("Patrons de nœud : un ÉCLAIR porte plusieurs images", "[render][table]") {
  // Shockwave Trap fait défiler onze textures et Thunder Plasma Blast neuf.
  // N'en garder qu'une — ce que le relevé faisait — fige un éclair qui devrait
  // battre, et rien ne le signale : on voit une image, simplement immobile.
  const std::span<const EffectNodePattern> piege = EffectNodePatternsFor(93);
  REQUIRE_FALSE(piege.empty());
  const auto anime = std::ranges::find_if(
      piege, [](const EffectNodePattern& pattern) { return pattern.texture_count > 1; });
  REQUIRE(anime != piege.end());
  const std::span<const std::string_view> images = ro::render::EffectNodePatternTextures(*anime);
  CHECK(images.size() > 1);
  // Les images d'une même suite se suivent dans la liste à plat.
  CHECK(images.front() != images.back());
}

TEST_CASE("Patrons de nœud : la TEINTE vient du nœud, pas de la texture", "[render][table]") {
  // Les textures de ces nœuds sont BLANCHES — `alpha_down.tga` est du
  // (254, 254, 254) à alpha dégradé —, et c'est la teinte du nœud qui les
  // colore. Sans elle, tout sort en blanc.
  const std::span<const EffectNodePattern> triple = EffectNodePatternsFor(14);
  REQUIRE(triple.size() >= 3);
  for (std::size_t rang = 0; rang < 3; ++rang) {
    INFO("anneau " << rang);
    CHECK(triple[rang].color[0] == 32);
    CHECK(triple[rang].color[1] == 176);
    CHECK(triple[rang].color[2] == 232);
  }
  // ⚠ BLANC N'EST PAS UNE ABSENCE DE TEINTE : le constructeur du client pose
  // 255 par canal, et Heal ne la change pas. Sa colonne est blanche chez lui
  // aussi ; la teinter d'office serait une invention.
  const std::span<const EffectNodePattern> heal = EffectNodePatternsFor(7);
  REQUIRE_FALSE(heal.empty());
  CHECK(heal[0].color[0] == 255);
  CHECK(heal[0].color[1] == 255);
  CHECK(heal[0].color[2] == 255);

  // La teinte arrive jusqu'au NŒUD, sans quoi la table la porterait pour rien.
  const ro::render::EffectNode node = ro::render::EffectNodeFromPattern(triple[0]);
  CHECK(node.color[1] == Catch::Approx(176.0F));
}

TEST_CASE("Patrons de nœud : la COLONNE DE HEAL, mesurée bout en bout", "[render][table]") {
  // Heal est court et entièrement lisible, et c'est pourquoi il sert d'étalon :
  // chaque nombre ci-dessous est une valeur que son bâtisseur écrit. Un chiffre
  // qui bouge ici dit qu'une pièce de la table a changé de sens.
  const std::span<const EffectNodePattern> heal = EffectNodePatternsFor(7);
  REQUIRE(heal.size() >= 2);
  const auto column = std::ranges::find_if(heal, [](const EffectNodePattern& pattern) {
    return pattern.shape == EffectNodeShape::kRingBand;
  });
  REQUIRE(column != heal.end());

  CHECK(column->life_ticks == 100);
  CHECK(column->ring.radius_at_anchor == Catch::Approx(4.5F));
  CHECK(column->ring.radius_at_height == Catch::Approx(4.5F));
  CHECK(column->ring.height == Catch::Approx(35.0F));
  // LE TIC DE SORTIE EST CALCULÉ — la vie moins vingt —, et rien ne l'écrit en
  // clair. Tant qu'il n'était pas lu, la colonne ne s'effaçait jamais.
  CHECK(column->fade_out_tick == 80);
  // ET L'ALPHA MONTE : de zéro à quatre-vingt-dix, par le plafond divisé par
  // trente. La colonne met donc trente tics à paraître, sur cent de vie.
  CHECK(column->alpha_max == Catch::Approx(90.0F));
  CHECK(column->alpha_start == Catch::Approx(0.0F));
  CHECK(column->alpha_step == Catch::Approx(3.0F));

  // Et la montée se voit sur le NŒUD, pas seulement dans la table.
  ro::render::EffectNode node = ro::render::EffectNodeFromPattern(*column);
  CHECK(ro::render::EffectNodeAlpha(node) == 0);
  for (int tick = 0; tick < 30; ++tick) {
    REQUIRE(ro::render::AdvanceEffectNode(node));
  }
  CHECK(ro::render::EffectNodeAlpha(node) == 90);
}

TEST_CASE(
    "Patrons de nœud : les ÉTINCELLES de Heal s'effacent, et n'ont pas toutes la même "
    "taille",
    "[render][table]") {
  // La seconde moitié de Heal : une étincelle semée toutes les neuf frames,
  // qui vit cinquante tics et commence à s'effacer au trente-quatrième — la vie
  // moins son tiers. Ce tic-là ne s'écrit nulle part en clair : le compilateur
  // divise par trois en multipliant par une constante magique.
  const std::span<const EffectNodePattern> heal = EffectNodePatternsFor(7);
  const auto spark = std::ranges::find_if(heal, [](const EffectNodePattern& pattern) {
    return pattern.shape == EffectNodeShape::kSprite;
  });
  REQUIRE(spark != heal.end());
  CHECK(spark->life_ticks == 50);
  CHECK(spark->fade_out_tick == 34);
  // Elle se pose sur un CERCLE, chacune au cap qu'elle tire — de rayon SIX,
  // parce que le rayon de trois du bâtisseur compte deux fois : baké dans le
  // décalage, puis ajouté par le mouvement du sous-type 7. Voir l'essai
  // « les étincelles naissent à DEUX fois le rayon ».
  CHECK(spark->scatter == Catch::Approx(6.0F));
}

TEST_CASE("Patrons de nœud : une nuée de sprites MÊLE ses tailles", "[render][table]") {
  // Hit3 — l'impact que l'on voit à chaque coup porté — tire la taille de ses
  // particules entre six dixièmes et un et demi. Tant que le tirage n'était pas
  // lu, le champ restait absent et toutes sortaient à l'échelle un : une nuée de
  // sprites identiques, là où le client en mêle les tailles.
  const std::span<const EffectNodePattern> impact = EffectNodePatternsFor(2);
  const auto particle = std::ranges::find_if(impact, [](const EffectNodePattern& pattern) {
    return pattern.shape == EffectNodeShape::kSprite && pattern.sprite_scale_high > 0.0F;
  });
  REQUIRE(particle != impact.end());
  CHECK(particle->sprite_scale == Catch::Approx(0.6F));
  CHECK(particle->sprite_scale_high == Catch::Approx(1.59F));
}

TEST_CASE("Patrons de nœud : SANS sortie, le nœud ne fond pas dès la naissance",
          "[render][table]") {
  // « Pas de sortie » est moins un, comme au constructeur du client. `StepAlpha`
  // compare l'âge au tic de sortie : une absence prise pour un tic ferait fondre
  // tous les nœuds sans sortie depuis leur naissance, et ceux qui montent ne
  // monteraient jamais.
  EffectNodePattern pattern;
  pattern.life_ticks = 40;
  pattern.fade_out_tick = -1;
  // Une seule image : sans elle le nœud épuise ses textures et meurt au premier tic.
  pattern.texture_count = 1;
  pattern.alpha_max = 200.0F;
  pattern.alpha_start = 0.0F;
  pattern.alpha_step = 20.0F;
  ro::render::EffectNode node = ro::render::EffectNodeFromPattern(pattern);
  for (int tick = 0; tick < 10; ++tick) {
    REQUIRE(ro::render::AdvanceEffectNode(node));
  }
  CHECK(ro::render::EffectNodeAlpha(node) == 200);

  // 🔴 ZÉRO, LUI, EST UN TIC : les étincelles d'impact fondent dès leur
  // naissance, sur toute leur vie.
  pattern.fade_out_tick = 0;
  pattern.alpha_start = 200.0F;
  pattern.alpha_step = 0.0F;
  ro::render::EffectNode fondant = ro::render::EffectNodeFromPattern(pattern);
  for (int tick = 0; tick < 20; ++tick) {
    REQUIRE(ro::render::AdvanceEffectNode(fondant));
  }
  CHECK(ro::render::EffectNodeAlpha(fondant) < 110);
  CHECK(ro::render::EffectNodeAlpha(fondant) > 90);
}

TEST_CASE("Patrons de nœud : une SECONDE IMAGE-CLÉ freine la rune qui s'ouvre", "[render][table]") {
  // La rune au sol de Skill Ground Ring — huit identifiants la partagent — naît
  // SANS TAILLE et gagne une unité et demie par tic, en accélérant. Au
  // dix-septième tic, le client remplace les deux étages par une réserve bien
  // plus lente : elle s'ouvre vite, puis se pose.
  const std::span<const EffectNodePattern> rune = EffectNodePatternsFor(119);
  REQUIRE_FALSE(rune.empty());
  const EffectNodePattern& pattern = rune[0];
  CHECK(pattern.ring.half_width == Catch::Approx(0.0F));
  CHECK(pattern.ring.half_width_growth == Catch::Approx(1.5F));
  CHECK(pattern.ring.second_keyframe_tick == 17);
  CHECK(pattern.ring.second_half_width_growth == Catch::Approx(0.1F));

  // ET LA BASCULE SE JOUE SUR LE NŒUD, au tic dit et une seule fois.
  ro::render::EffectNode node = ro::render::EffectNodeFromPattern(pattern);
  for (int tick = 0; tick < 17; ++tick) {
    REQUIRE(ro::render::AdvanceEffectNode(node));
    INFO("tic " << tick);
    CHECK(node.half_width.growth_per_tick > 1.0F);
  }
  const float before_switch = node.half_width.half_size;
  REQUIRE(ro::render::AdvanceEffectNode(node));
  // Le pas est tombé à la valeur de la réserve : la rune continue de s'ouvrir,
  // mais quinze fois plus lentement.
  CHECK(node.half_width.growth_per_tick < 0.2F);
  CHECK(node.half_width.half_size > before_switch);
  // ET UNE SEULE FOIS : au tic suivant, le pas reprend sa propre accélération
  // au lieu d'être remis à celui de la réserve.
  const float after = node.half_width.growth_per_tick;
  REQUIRE(ro::render::AdvanceEffectNode(node));
  CHECK(node.half_width.growth_per_tick > after);
}

TEST_CASE("Patrons de nœud : un effet SÈME AU FIL DES TICS, pas d'un coup", "[render][table]") {
  // ⚠ UN EFFET N'EST PAS UN SEMIS MAIS UNE SUITE DE SEMIS. Le client rappelle
  // son bâtisseur à chaque tic : EF_EXIT pose sa colonne au tic zéro et lâche
  // une étincelle tous les neuf tics jusqu'à la fin de sa vie. Tout semer d'un
  // coup fait naître ensemble ce qui devait couler.
  //
  // 🔴 ET CETTE VIE EST CELLE QUE SON BÂTISSEUR ÉCRIT DANS SON INSTANCE — cent
  // tics, posés au tic zéro —, qui remplace les
  // cinquante que la table du client déclare. Ce cas a épinglé tour à tour les
  // deux nombres : le cent, quand les étincelles survivaient à la colonne ;
  // puis le cinquante, en tenant ce symptôme pour une durée trop longue. C'était
  // un semis qui survivait à son effet — le client détruit l'instance et ses
  // enfants ensemble —, et c'est la borne du semis qui le porte, non l'horaire.
  const std::span<const EffectNodePattern> heal = EffectNodePatternsFor(7);
  const auto column = std::ranges::find_if(heal, [](const EffectNodePattern& pattern) {
    return pattern.shape == EffectNodeShape::kRingBand;
  });
  const auto spark = std::ranges::find_if(heal, [](const EffectNodePattern& pattern) {
    return pattern.shape == EffectNodeShape::kSprite;
  });
  REQUIRE(column != heal.end());
  REQUIRE(spark != heal.end());

  // LA COLONNE : une seule fois, au premier tic.
  CHECK(column->period_ticks == 0);
  CHECK(ro::render::EffectNodePatternSowsAt(*column, 0));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAt(*column, 1));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAt(*column, 9));

  // L'ÉTINCELLE : tous les neuf tics, jusqu'à la fin de la vie de l'effet.
  CHECK(spark->period_ticks == 9);
  CHECK(spark->first_tick == 0);
  CHECK(spark->last_tick == 99);
  int sowing = 0;
  for (std::int32_t tick = 0; tick <= 200; ++tick) {
    if (ro::render::EffectNodePatternSowsAt(*spark, tick)) {
      ++sowing;
      CHECK(tick % 9 == 0);
      CHECK(tick <= 99);
    }
  }
  // De zéro à quatre-vingt-dix-neuf par neuf : douze étincelles. La dernière
  // naît au dernier tic de l'effet, et c'est la vie de l'effet — mesurée, voir
  // `EffectNodeLifeFor(7)` — qui la coupe avec la colonne.
  CHECK(sowing == 12);
  // ET L'EFFET SE RETIRE : sans cette borne, la période sèmerait sans fin.
  CHECK(ro::render::EffectNodePatternLastSowTick(*spark) == 99);
  CHECK(ro::render::EffectNodePatternLastSowTick(*column) == 0);
}

TEST_CASE("Patrons de nœud : un horaire ne dit jamais l'impossible", "[render][table]") {
  // Le filet de l'horaire, sur la table entière. Un dernier tic sous le premier
  // ne sèmerait jamais, et une période sans borne sèmerait sans fin : ni l'un ni
  // l'autre ne doit pouvoir entrer.
  for (const EffectNodePattern& pattern : EffectNodePatternTable()) {
    INFO("effet " << pattern.effect_id);
    CHECK(pattern.first_tick >= 0);
    CHECK(pattern.period_ticks >= 0);
    if (pattern.period_ticks > 0) {
      CHECK(pattern.last_tick >= pattern.first_tick);
    }
    // Tout patron sème AU MOINS UNE FOIS dans sa fenêtre, sans quoi il est en
    // table pour rien.
    bool sown = false;
    for (std::int32_t tick = pattern.first_tick;
         tick <= ro::render::EffectNodePatternLastSowTick(pattern) && !sown; ++tick) {
      sown = ro::render::EffectNodePatternSowsAt(pattern, tick);
    }
    CHECK(sown);
  }
}

TEST_CASE("Patrons de nœud : une demi-extension TIRÉE AU SORT reste dans ses bornes",
          "[render][table]") {
  // Increase AGI, et toute la famille ac_center2 derrière lui, TIRE sa
  // demi-largeur : « (rand() % 60 + 30) / 10 », soit de trois à huit et neuf
  // dixièmes. C'est la valeur du client, pas un choix d'ici.
  const std::span<const EffectNodePattern> agi = EffectNodePatternsFor(37);
  REQUIRE_FALSE(agi.empty());
  const EffectNodePattern& pattern = agi[0];
  CHECK(pattern.shape == EffectNodeShape::kCrossedPlanes);
  CHECK(pattern.ring.half_width == Catch::Approx(3.0F));
  CHECK(pattern.ring.half_width_high == Catch::Approx(8.9F));

  // CENT NŒUDS, ET PAS DEUX DE LA MÊME TAILLE. Une aura dont tous les plans
  // auraient la même demi-largeur serait une figure, pas une aura : c'est
  // l'écart des tirages qui lui donne son épaisseur.
  std::uint32_t generator = 1U;
  float first = 0.0F;
  bool all_alike = true;
  for (int pose = 0; pose < 100; ++pose) {
    const ro::render::EffectNode node = ro::render::EffectNodeFromPattern(pattern, generator);
    const float width = node.half_width.half_size;
    INFO("pose " << pose << " : " << width);
    CHECK(width >= 3.0F);
    CHECK(width <= 8.9F);
    if (pose == 0) {
      first = width;
    } else if (width != first) {
      all_alike = false;
    }
  }
  CHECK_FALSE(all_alike);
}

TEST_CASE("Patrons de nœud : sans tirage, la valeur est CELLE-LÀ et la graine dort",
          "[render][table]") {
  // Une borne haute nulle n'est pas une borne : c'est l'absence de tirage. Le
  // nœud prend alors la valeur telle quelle, et le générateur n'avance même
  // pas — sans quoi ajouter un tirage à un patron changerait la figure de tous
  // ceux semés après lui.
  EffectNodePattern pattern;
  pattern.ring.half_width = 7.0F;
  pattern.ring.height = 0.25F;
  std::uint32_t generator = 12345U;
  const std::uint32_t before = generator;
  const ro::render::EffectNode node = ro::render::EffectNodeFromPattern(pattern, generator);
  CHECK(node.half_width.half_size == Catch::Approx(7.0F));
  CHECK(node.half_height.half_size == Catch::Approx(0.25F));
  CHECK(generator == before);
}

TEST_CASE("Patrons de nœud : toute borne haute posée DÉPASSE sa valeur", "[render][table]") {
  // Une borne haute sous la basse rendrait une fourchette retournée, et le
  // tirage ne la verrait pas : il rendrait la basse en silence. C'est le genre
  // de relevé faux qui a l'air d'une mesure — voir le lecteur de tirage.
  for (const EffectNodePattern& pattern : EffectNodePatternTable()) {
    INFO("effet " << pattern.effect_id);
    if (pattern.ring.half_width_high != 0.0F) {
      CHECK(pattern.ring.half_width_high > pattern.ring.half_width);
    }
    if (pattern.ring.height_high != 0.0F) {
      CHECK(pattern.ring.height_high > pattern.ring.height);
    }
    if (pattern.sprite_scale_high != 0.0F) {
      CHECK(pattern.sprite_scale_high > pattern.sprite_scale);
    }
  }
}

TEST_CASE("Patrons de nœud : un effet inconnu rend un span VIDE", "[render][table]") {
  // ⚠ PAS L'IDENTIFIANT ZÉRO. Il a longtemps servi d'« inconnu » parce que la
  // table ne le portait pas ; c'est EF_HIT1, l'impact de base, et le relevé
  // étendu lui a rendu son anneau bleu. Un identifiant absent AUJOURD'HUI ne
  // l'est pas pour toujours : le seul qui le reste est celui qu'aucun
  // répartiteur ne peut porter.
  //
  // Le troisième étage commute jusqu'à 2437 ; au-delà, rien ne répond.
  CHECK(EffectNodePatternsFor(65535).empty());
  CHECK(EffectNodePatternsFor(0xFFFFFFFFU).empty());
}

TEST_CASE("Compétences procédurales : la jointure porte TOUT ce que le client attache",
          "[render][table]") {
  // ⚠ ELLE NE TRIE PLUS PAR CE QUE NOUS DESSINONS, et c'est la correction. Le
  // générateur qui l'engendre ne connaît qu'une famille de patrons sur treize ;
  // lui laisser le choix faisait passer pour muette toute compétence servie par
  // l'une des douze autres.
  const std::span<const ro::render::ProceduralSkillEffect> table = ProceduralSkillEffectTable();
  REQUIRE_FALSE(table.empty());
  CHECK(std::ranges::is_sorted(table, {}, &ro::render::ProceduralSkillEffect::skill_id));

  for (const ro::render::ProceduralSkillEffect& couple : table) {
    CHECK(couple.skill_id != 0);
    // Un effet ZÉRO n'est l'effet de personne : il ferait croire la compétence
    // jointe alors qu'elle ne mène nulle part.
    CHECK(couple.effect_id != 0);
  }

  // ET AUCUN COUPLE NE S'Y RÉPÈTE : les deux chemins du client — celui des
  // dégâts et celui sans dégâts — mènent au même acteur, et en semer deux
  // doublerait la troupe.
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    const ro::render::ProceduralSkillEffect& before = table[rang - 1];
    const ro::render::ProceduralSkillEffect& here = table[rang];
    CHECK_FALSE((before.skill_id == here.skill_id && before.effect_id == here.effect_id &&
                 before.on == here.on));
  }
}

TEST_CASE("Compétences procédurales : l'effet MONTRÉ est le premier qu'on sache dessiner",
          "[render][table]") {
  // La règle, éprouvée sur la table entière : ce que rend le choix est un couple
  // de la compétence, il se dessine, et rien avant lui ne se dessinait.
  std::size_t montrees = 0;
  for (const ro::render::ProceduralSkillEffect& couple : ProceduralSkillEffectTable()) {
    const ro::render::ProceduralSkillEffect* shows =
        DrawableProceduralEffectOfSkill(couple.skill_id);
    const std::span<const ro::render::ProceduralSkillEffect> own_entries =
        ProceduralEffectsOfSkill(couple.skill_id);
    REQUIRE_FALSE(own_entries.empty());
    if (shows == nullptr) {
      // Rien de ce que le client lui attache ne se dessine encore.
      for (const ro::render::ProceduralSkillEffect& own : own_entries) {
        CHECK_FALSE(KnowsProceduralEffect(own.effect_id));
      }
      continue;
    }
    ++montrees;
    CHECK(shows->skill_id == couple.skill_id);
    CHECK(KnowsProceduralEffect(shows->effect_id));
    for (const ro::render::ProceduralSkillEffect& own : own_entries) {
      if (&own == shows) {
        break;
      }
      CHECK_FALSE(KnowsProceduralEffect(own.effect_id));
    }
  }
  // ⚠ ET LE COMPTE NE DOIT PAS RETOMBER : cent treize compétences se montraient
  // quand le générateur triait, cent quatre-vingt-trois depuis que le dessin
  // tranche. Le chiffre monte à chaque famille portée, il ne descend pas.
  CHECK(montrees >= 183);
}

TEST_CASE("Compétences procédurales : Blessing part sur la cible", "[render][table]") {
  const ro::render::ProceduralSkillEffect* blessing = DrawableProceduralEffectOfSkill(34);
  REQUIRE(blessing != nullptr);
  CHECK(blessing->effect_id == 42);
  CHECK(blessing->on == SkillEffectOn::kTarget);

  // Fire Ball naît sur le LANCEUR : tout jouer sur la cible mettrait l'effet au
  // mauvais bout d'un écran de distance.
  const ro::render::ProceduralSkillEffect* fireball = DrawableProceduralEffectOfSkill(17);
  REQUIRE(fireball != nullptr);
  CHECK(fireball->on == SkillEffectOn::kCaster);
}

TEST_CASE("Compétences procédurales : Soul Strike naît sur le LANCEUR et vole vers la cible",
          "[render][table]") {
  // 🔴 « VISÉ » DIT OÙ L'EFFET REGARDE, PAS OÙ IL NAÎT. Le client fait naître
  // toute instance visée sur l'attaquant et lui donne la position de la cible
  // par commande. Le bâtisseur de Soul Strike ancre sa boule à l'instance, dix
  // unités au-dessus, et la fait voler jusqu'au point visé : née sur la cible,
  // elle volerait vers le mage. `@effect 15` ne le voyait pas — les deux bouts y sont
  // le même point.
  for (const std::uint16_t skill_id : {std::uint16_t{13}, std::uint16_t{5220}}) {
    INFO(skill_id);
    const ro::render::ProceduralSkillEffect* soul = DrawableProceduralEffectOfSkill(skill_id);
    REQUIRE(soul != nullptr);
    CHECK(soul->effect_id == 15);
    CHECK(soul->on == SkillEffectOn::kCaster);
  }
  // Et la boule VISE : sa direction va de sa naissance à la cible, son pas est
  // la distance divisée par sa vie.
  const std::span<const EffectNodePattern> balls = EffectNodePatternsFor(15);
  REQUIRE_FALSE(balls.empty());
  CHECK(balls.front().aims_at_target);
  // ⚠ LES TRENTE-NEUF AUTRES EFFETS VISÉS RESTENT NÉS SUR LA CIBLE : leurs
  // patrons ont été ajustés ainsi, et c'est équivalent pour qui s'ancre au point
  // visé — la flèche de glace tombe sur ce qu'elle gèle.
  const ro::render::ProceduralSkillEffect* coldbolt = DrawableProceduralEffectOfSkill(14);
  REQUIRE(coldbolt != nullptr);
  CHECK(coldbolt->on == SkillEffectOn::kTarget);
}

TEST_CASE("Compétences procédurales : HEAL trouve enfin son effet", "[render][table]") {
  // ⚠ LE SOIN DU PRÊTRE NE JOUE PAS 312. Le client lui attache deux effets, dans
  // cet ordre : 325 par le chemin SANS DÉGÂTS — EF_HEAL4, le soin ordinaire —,
  // puis 320 par celui des dégâts, qui est le soin porté sur un mort-vivant.
  // C'est la mesure qui le dit, pas le nom : 312 est bâti par une autre fonction
  // et n'est jamais joint à la compétence 28.
  const std::span<const ro::render::ProceduralSkillEffect> own_entries =
      ProceduralEffectsOfSkill(28);
  REQUIRE(own_entries.size() == 2);
  CHECK(own_entries[0].effect_id == 325);
  CHECK(own_entries[1].effect_id == 320);

  // Et c'est le PREMIER qu'il montre, puisque nous savons désormais le dessiner.
  // Tant que le générateur triait sur sa seule famille, Heal ne trouvait aucun
  // effet et ne dessinait rien en jeu.
  const ro::render::ProceduralSkillEffect* shows = DrawableProceduralEffectOfSkill(28);
  REQUIRE(shows != nullptr);
  CHECK(shows->effect_id == 325);
  CHECK_FALSE(ro::render::EffectRisingSparkPatternsFor(shows->effect_id).empty());
}

TEST_CASE("Patrons : les quatre IMPACTS sèment ce que le client sème", "[render][table]") {
  // 🔴 UN NŒUD SEMÉ UNE FOIS N'EST PAS CELUI QU'ON VOIT. Les quatre impacts
  // montraient toutes leurs pièces et paraissaient FIGÉS : une étincelle au
  // lieu de quatre ne ressemble pas à un effet incomplet. Le compte vient de la
  // boucle qui entoure le bâtisseur chez le client.
  struct Expected {
    std::uint32_t effect_id;
    std::size_t patterns;
    std::uint16_t last_copies;
  };
  // EF_HIT1 : un anneau, puis deux nœuds qui s'écartent et deux qui RETOMBENT.
  // EF_HIT3 : deux anneaux et huit étincelles. EF_HIT4 : un anneau et cinq.
  constexpr std::array<Expected, 3> kImpacts = {{{0, 3, 2}, {2, 3, 8}, {3, 2, 5}}};
  for (const Expected& expected : kImpacts) {
    const std::span<const EffectNodePattern> own_entries =
        EffectNodePatternsFor(expected.effect_id);
    INFO("effet " << expected.effect_id);
    REQUIRE(own_entries.size() == expected.patterns);
    CHECK(own_entries.back().copies == expected.last_copies);
  }

  // ⚠ ET LES DEUX FAMILLES DE L'IMPACT DE BASE NE SONT PAS UN DOUBLON. Elles
  // partagent leur sprite et leur échelle ; ce qui les sépare est que la
  // seconde retombe. Le générateur n'en gardait qu'une.
  const std::span<const EffectNodePattern> impact = EffectNodePatternsFor(0);
  REQUIRE(impact.size() == 3);
  CHECK(impact[0].shape == EffectNodeShape::kRingBand);
  CHECK(impact[1].shape == EffectNodeShape::kSprite);
  CHECK(impact[2].shape == EffectNodeShape::kSprite);
  CHECK(impact[1].copies == 2);
  CHECK(impact[2].copies == 2);
}

TEST_CASE("Patrons : UN exemplaire ne veut pas dire « mesuré à un »", "[render][table]") {
  // ⚠ C'EST POURQUOI L'ANCIENNE RÈGLE SURVIT AU CHAMP. Trente des cinquante-deux
  // boucles du gisement ne se lisent pas, et rendent donc un ; le semis ne peut
  // pas distinguer ce un-là de celui d'un nœud vraiment unique. Un compte
  // MESURÉ passe devant, l'unité laisse la place au défaut.
  //
  // Ce banc garde la forme du champ, pas une valeur : il dit qu'aucun patron ne
  // porte zéro exemplaire, ce qui ne sèmerait rien du tout.
  std::size_t mesures = 0;
  for (const EffectNodePattern& node_pattern : EffectNodePatternTable()) {
    REQUIRE(node_pattern.copies >= 1);
    if (node_pattern.copies > 1) {
      ++mesures;
    }
  }
  CHECK(mesures > 0);
}

TEST_CASE("Patrons : une texture n'appartient qu'à l'effet dont le cas la nomme",
          "[render][table]") {
  // 🔴 LE RELEVÉ DES ANNEAUX EST INDEXÉ PAR FONCTION, et le répartiteur en sert
  // dix-neuf : lui prendre ses nœuds ENTIERS prêtait à chaque effet les figures
  // de ses voisins. EF_TOPRANK portait ainsi l'anneau bleu d'EF_PARTY par-dessus
  // le sien, et la bande debout d'EF_TELEPORTATION avec — ce qui se voyait en
  // jeu, parce que ces deux-là vivent 9999 tics et ne s'effacent jamais.
  //
  // ⚠ CE BANC NOMME DES CHEMINS ET NON DES NUMÉROS : le numéro d'une texture
  // dans la liste à plat bouge dès qu'une entrée entre dans la table, le chemin
  // dans l'archive non.
  //
  // ⚠ ET IL NE REGARDE QUE LE RÉPARTITEUR, parce que la fuite n'a lieu qu'entre
  // SES entrants. Deux effets peuvent fort bien partager une image sans que rien
  // ne soit cassé : EF_LOCKON porte lui aussi `LockOn128.tga`, par son propre
  // bâtisseur, et c'est juste — le réticule de visée et l'anneau du premier au
  // classement sont dessinés avec la même figure.
  constexpr std::string_view kRepartiteur = "DispatchByType";
  constexpr std::array<std::pair<std::string_view, std::uint32_t>, 2> kProprietaires = {{
      {"data\\texture\\effect\\LockOn128.tga", 159},  // EF_TOPRANK
      {"data\\texture\\effect\\party.tga", 160},      // EF_PARTY
  }};
  for (const auto& [path, owner] : kProprietaires) {
    std::vector<std::uint32_t> porteurs;
    for (const EffectNodePattern& node_pattern : EffectNodePatternTable()) {
      if (node_pattern.client_function != kRepartiteur) {
        continue;
      }
      for (const std::string_view texture : ro::render::EffectNodePatternTextures(node_pattern)) {
        if (texture == path) {
          porteurs.push_back(node_pattern.effect_id);
        }
      }
    }
    INFO(path);
    REQUIRE(porteurs.size() == 1);
    CHECK(porteurs.front() == owner);
  }

  // ET UN SEUL NŒUD CHACUN : leur cas n'en bâtit qu'un, et un second voudrait
  // dire que la fonction a reversé les siens.
  REQUIRE(EffectNodePatternsFor(159).size() == 1);
  REQUIRE(EffectNodePatternsFor(160).size() == 1);
}
