// CE QUE LE RELEVE NE VOYAIT PAS, et qu'une LECTURE A LA MAIN lui a rendu.
//
// Sorti de `effect_node_pattern_table_test.cpp` au plafond de huit cents lignes,
// et la coupure suit une frontiere : d'un cote ce que la table PROMET — tri,
// bornes, horaires, aires —, de l'autre les entrees qu'aucun parcours
// automatique n'aurait produites.
//
// ⚠ CES TESTS-LA VALENT DOUBLE. Une lecture a la main est une affirmation sans
// extracteur pour la verifier : c'est ici, et nulle part ailleurs, qu'elle se
// confronte a la table engendree. Les six cas ci-dessous ont tous ete des
// ABSENCES — un effet sans patron, une texture qui n'etait celle de personne, un
// noeud que sa boucle cachait — et aucun ne se voyait dans un compte.

#include <span>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_node_sprite_seed.h"
#include "ro/render/effect_node_world.h"

using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;
using ro::render::EffectNodeShape;

TEST_CASE("L'INDEX D'ACTION sépare deux effets qui partagent leur sprite", "[render][table]") {
  // ⚠ 363 ET 364 CHARGENT LE MÊME COUPLE `vallentine` ET NE DIFFÈRENT QUE PAR
  // L'ACTION — zéro et un. C'est la seule chose qui les sépare dans leur
  // bâtisseur commun, et sans ce champ les deux
  // effets se montreraient à l'identique : le sprite juste, l'animation fausse
  // pour l'un des deux, et rien à l'écran pour le dire.
  const std::span<const ro::render::EffectNodePattern> saint = EffectNodePatternsFor(363);
  const std::span<const ro::render::EffectNodePattern> valentin = EffectNodePatternsFor(364);
  REQUIRE(saint.size() == 1);
  REQUIRE(valentin.size() == 1);

  const std::span<const std::string_view> first_pair =
      ro::render::EffectNodePatternTextures(saint.front());
  const std::span<const std::string_view> couple_du_second =
      ro::render::EffectNodePatternTextures(valentin.front());
  REQUIRE(first_pair.size() == 1);
  REQUIRE(couple_du_second.size() == 1);
  CHECK(first_pair.front() == couple_du_second.front());

  CHECK(saint.front().action == 0);
  CHECK(valentin.front().action == 1);

  // ET IL ARRIVE JUSQU'AU NŒUD. Le champ ne sert à rien s'il s'arrête au
  // patron : c'est `EffectNode::action` que la composition du sprite lit.
  std::uint32_t generator = 1U;
  CHECK(ro::render::EffectNodeFromPattern(valentin.front(), generator).action == 1);
}

TEST_CASE("les cinq sprites de la Saint-Valentin sont tranchés, un par style", "[render][table]") {
  // Les cinq identifiants que le bâtisseur de la Saint-Valentin sert, et le
  // couple que chacun charge. Le parcours en avant ne les départageait pas : la
  // garde porte sur le paramètre du bâtisseur, à l'intérieur de lui.
  const std::array<std::pair<std::uint32_t, std::string_view>, 5> attendus = {{
      {363, "vallentine"},
      {364, "vallentine"},
      {509, "05vallentine"},
      {519, "fast"},
      {545, "ro2effect"},
  }};
  for (const auto& [effect_id, name] : attendus) {
    const std::span<const ro::render::EffectNodePattern> own_entries =
        EffectNodePatternsFor(effect_id);
    INFO("effet " << effect_id);
    REQUIRE(own_entries.size() == 1);
    CHECK(own_entries.front().shape == ro::render::EffectNodeShape::kSprite);
    const std::span<const std::string_view> couple =
        ro::render::EffectNodePatternTextures(own_entries.front());
    REQUIRE(couple.size() == 1);
    CHECK(couple.front().ends_with(name));
  }
}

TEST_CASE("Frappe de l'âme : deux sprites qu'aucun parcours ne voyait", "[render][table]") {
  // ⚠ LE BÂTISSEUR POSE BIEN SON SPRITE, et le relevé n'en montrait AUCUN. Il
  // charge le sprite depuis une variable que deux ternaires ont remplie plus
  // haut, jamais avec l'adresse d'une constante : l'extracteur cherche
  // `&constante` au site de l'appel et ne
  // voit rien. « Aucune ressource » ne voulait donc pas dire « n'en pose pas »,
  // mais « je n'ai rien vu » — et c'est la cause la plus fréquente parmi les
  // identifiants qui attendent encore un semis.
  const std::array<std::pair<std::uint32_t, std::string_view>, 2> attendus = {{
      {15, "particle1"},
      {451, "particle5"},
  }};
  for (const auto& [effect_id, name] : attendus) {
    const std::span<const ro::render::EffectNodePattern> own_entries =
        EffectNodePatternsFor(effect_id);
    INFO("effet " << effect_id);
    REQUIRE(own_entries.size() == 1);
    CHECK(own_entries.front().shape == ro::render::EffectNodeShape::kSprite);
    // La vie et l'échelle, elles, venaient bien du relevé : quarante tics et
    // deux virgule soixante-quinze.
    CHECK(own_entries.front().life_ticks == 40);
    CHECK(own_entries.front().sprite_scale == Catch::Approx(2.75F));
    const std::span<const std::string_view> couple =
        ro::render::EffectNodePatternTextures(own_entries.front());
    REQUIRE(couple.size() == 1);
    CHECK(couple.front().ends_with(name));
  }

  // ⚠ ET LES DEUX NE PARTAGENT PAS LEUR SPRITE, à la différence de 363 et 364.
  // Le paramètre du site choisit ici le FICHIER, non l'action : les confondre
  // aurait donné la même particule aux deux.
  CHECK(ro::render::EffectNodePatternTextures(EffectNodePatternsFor(15).front()).front() !=
        ro::render::EffectNodePatternTextures(EffectNodePatternsFor(451).front()).front());
}

TEST_CASE("Pointe de terre : la seconde image-clé RETOURNE le pas", "[render][table]") {
  // ⚠ LA POINTE JAILLIT PUIS REDESCEND, et c'est le pas qui le dit. Elle monte
  // d'une unité par tic, puis le client remplace son pas par MOINS un et deux
  // dixièmes au onzième tic. Les quatre champs de bascule que nous portions ne
  // touchaient que les demi-extensions : la pointe montait et RESTAIT, et le
  // crible l'écartait plutôt que de la montrer fausse. Porter le pas les fait
  // entrer — la grille de pointes, et la flèche de glace.
  const std::span<const EffectNodePattern> tip = EffectNodePatternsFor(142);
  REQUIRE_FALSE(tip.empty());
  const EffectNodePattern& pattern = tip[0];
  CHECK(pattern.shape == ro::render::EffectNodeShape::kCrossSpikes);
  CHECK(pattern.ring.second_keyframe_tick == 11);
  CHECK(pattern.ring.second_step_length == Catch::Approx(-1.2F));

  // ⚠ ET LA HAUTEUR EST UN TIRAGE que le client CALCULE : (tirage sur 60 + 1)
  // divisé par dix, plus neuf. Le relevé la rendait « calculé », le crible lui
  // donnait le défaut du constructeur — zéro —, et une croix de pointes à sommet
  // nul est couchée au sol. Elle était donc jugée plate alors qu'elle monte.
  CHECK(pattern.ring.height == Catch::Approx(9.1F));
  CHECK(pattern.ring.height_high == Catch::Approx(15.0F));

  // LA BASCULE SE JOUE SUR LE NŒUD, et elle RETOURNE le pas au lieu de s'y
  // ajouter : ajouter -1,2 à 1 aurait à peine ralenti la pointe.
  ro::render::EffectNode node = ro::render::EffectNodeFromPattern(pattern);
  for (int tick = 0; tick < 11; ++tick) {
    REQUIRE(ro::render::AdvanceEffectNode(node));
    INFO("tic " << tick);
    CHECK(node.step_length > 0.0F);
  }
  REQUIRE(ro::render::AdvanceEffectNode(node));
  CHECK(node.step_length == Catch::Approx(-1.2F));
}

TEST_CASE("Pointe de terre : la texture vivait dans une VARIABLE LOCALE", "[render][table]") {
  // ⚠ « AUCUNE RESSOURCE » NE VOULAIT PAS DIRE « N'EN POSE PAS ». Le bâtisseur
  // de la pointe de terre choisit sa texture tout en haut, dans la
  // branche qui joue aussi le son, puis charge cette variable-là cent lignes
  // plus bas et pour ses DEUX nœuds. L'extracteur cherche une chaîne au site de
  // l'appel : il rendait une liste vide.
  const std::array<std::pair<std::uint32_t, std::string_view>, 2> attendus = {{
      {79, "stone.bmp"},
      {619, "ice.tga"},
  }};
  for (const auto& [effect_id, name] : attendus) {
    const std::span<const EffectNodePattern> own_entries = EffectNodePatternsFor(effect_id);
    INFO("effet " << effect_id);
    // DEUX nœuds, et le second est celui que la boucle répète six fois.
    REQUIRE(own_entries.size() == 2);
    for (const EffectNodePattern& node_pattern : own_entries) {
      CHECK(node_pattern.shape == ro::render::EffectNodeShape::kCrossSpikes);
      const std::span<const std::string_view> textures =
          ro::render::EffectNodePatternTextures(node_pattern);
      REQUIRE(textures.size() == 1);
      CHECK(textures.front().ends_with(name));
    }
    // Les deux nœuds ne montent pas à la même hauteur : dix-huit et vingt.
    CHECK(own_entries[0].ring.height != own_entries[1].ring.height);
  }
}

TEST_CASE("Gravitation : la grille de pierres que le relevé ne VOYAIT pas", "[render][table]") {
  // ⚠ ELLE MANQUAIT PARCE QUE LE RELEVÉ SE TENAIT PAR SOUS-TYPE. Sa table des
  // dessinateurs d'anneau nommait le 9 et non le 117, alors que les deux mènent
  // à la MÊME croix de pointes : le bâtisseur de la grille de pierres sortait du
  // relevé entier, et avec lui quatre sites d'émission qu'aucun crible
  // n'accusait.
  const std::span<const EffectNodePattern> own_entries = EffectNodePatternsFor(522);
  REQUIRE(own_entries.size() == 1);
  const EffectNodePattern& node_pattern = own_entries.front();
  CHECK(node_pattern.shape == ro::render::EffectNodeShape::kCrossSpikes);
  // ⚠ ET SON PAS CYCLE, là où celui de la pointe de terre bascule une fois. La
  // réserve est la même — moins un et deux dixièmes — et son emploi est autre.
  CHECK(node_pattern.ring.step_cycles);
  CHECK(node_pattern.ring.second_keyframe_tick == -1);
  CHECK(node_pattern.ring.second_step_length == Catch::Approx(-1.2F));
  CHECK(node_pattern.ring.step_length == Catch::Approx(1.0F));
  CHECK(node_pattern.ring.step_length_change == Catch::Approx(0.01F));
  // La MÊME grille de vingt-cinq cases que la pointe de terre, quatre
  // exemplaires par case.
  CHECK(node_pattern.ring.instance_count == 100);
  CHECK(node_pattern.ring.copies_per_place == 4);
  CHECK(node_pattern.ring.instance_placement ==
        ro::render::EffectNodeInstancePlacement::kSquareGrid);
  CHECK(node_pattern.ring.instance_spacing == Catch::Approx(5.0F));
  // ⚠ ET SON ALPHA RESTE À VINGT. Le secours qui remonte un alpha illisible à
  // son plafond ne vaut que pour un alpha NUL ; celui-ci est POSÉ, et la pierre
  // est pâle parce que le client la veut pâle.
  CHECK(node_pattern.alpha_start == Catch::Approx(20.0F));
  CHECK(node_pattern.alpha_step == Catch::Approx(0.0F));
}

TEST_CASE("Pilier de feu : TROIS anneaux sortent d'une seule boucle", "[render][table]") {
  // ⚠ LE RELEVÉ N'EN VOYAIT QU'UN, et c'est une limite d'outil : le bâtisseur
  // des anneaux rouge ou blanc appelle la fabrique DANS UNE BOUCLE. Il n'y a
  // donc qu'un appel dans son code, et les champs y sont des expressions
  // du compteur de tour — que l'extracteur rend « calculé ». Le crible écartait
  // le nœud faute de rayon, et l'effet n'avait aucun patron.
  const std::span<const EffectNodePattern> own_entries = EffectNodePatternsFor(138);
  REQUIRE(own_entries.size() == 3);

  // ⚠ LE PLUS ÉTROIT EST LE PLUS HAUT : les trois anneaux sont emboîtés, et leur
  // hauteur DÉCROÎT quand leur rayon croît. Les poser à la même hauteur ferait un
  // cerceau plat au lieu d'un pilier.
  CHECK(own_entries[0].ring.radius_at_anchor == Catch::Approx(2.5F));
  CHECK(own_entries[1].ring.radius_at_anchor == Catch::Approx(3.5F));
  CHECK(own_entries[2].ring.radius_at_anchor == Catch::Approx(4.5F));
  CHECK(own_entries[0].ring.height == Catch::Approx(27.5F));
  CHECK(own_entries[1].ring.height == Catch::Approx(15.5F));
  CHECK(own_entries[2].ring.height == Catch::Approx(7.5F));
  for (const EffectNodePattern& node_pattern : own_entries) {
    CHECK(node_pattern.shape == ro::render::EffectNodeShape::kRingBand);
    CHECK(node_pattern.alpha_start == Catch::Approx(240.0F));
    const std::span<const std::string_view> textures =
        ro::render::EffectNodePatternTextures(node_pattern);
    REQUIRE(textures.size() == 1);
    // ⚠ ET SA TEXTURE EST CELLE DE SON ARGUMENT, pas celle de la dernière
    // branche : le bâtisseur est partagé, et son site pousse zéro pour le rouge
    // et un pour le blanc (ADR 0019). Le parcours en avant rendait `ring_white`
    // aux deux.
    CHECK(textures.front().ends_with("magic_red.tga"));
  }

  const std::span<const EffectNodePattern> blancs = EffectNodePatternsFor(718);
  REQUIRE(blancs.size() == 3);
  const std::span<const std::string_view> blanches =
      ro::render::EffectNodePatternTextures(blancs.front());
  REQUIRE(blanches.size() == 1);
  CHECK(blanches.front().ends_with("ring_white.tga"));
}

TEST_CASE("Résurrection : un anneau immobile à l'âge zéro, puis un qui se resserre",
          "[render][table]") {
  // ⚠ UN APPEL, DEUX ANNEAUX : le cas bâtit son anneau tous les vingt-cinq
  // tics, et le RÉÉCRIT à l'âge zéro. Le relevé n'en voyait qu'un, sans
  // texture, et l'effet n'avait aucun dessin.
  const std::span<const EffectNodePattern> own_entries = EffectNodePatternsFor(140);
  REQUIRE(own_entries.size() == 2);
  const EffectNodePattern& first = own_entries[0];
  const EffectNodePattern& suivants = own_entries[1];
  for (const EffectNodePattern& node_pattern : own_entries) {
    CHECK(node_pattern.shape == EffectNodeShape::kRingBand);
    CHECK(node_pattern.alpha_start == Catch::Approx(240.0F));
    CHECK(node_pattern.ring.radius_at_anchor == Catch::Approx(7.5F));
    CHECK(node_pattern.ring.height == Catch::Approx(7.5F));
    CHECK(node_pattern.ring.yaw_step_degrees == Catch::Approx(1.5F));
    CHECK(node_pattern.ring.yaw_step_change == Catch::Approx(0.03F));
    const std::span<const std::string_view> textures =
        ro::render::EffectNodePatternTextures(node_pattern);
    REQUIRE(textures.size() == 1);
    CHECK(textures.front().ends_with("ring_yellow.tga"));
  }
  // L'ÂGE ZÉRO : vie de l'effet, sortie au dixième de la fin, rayons figés.
  CHECK(first.life_from_effect);
  CHECK(first.life_ticks == 250);
  CHECK(first.fade_out_tick == 225);
  CHECK(first.period_ticks == 0);
  CHECK(first.ring.radius_at_height == Catch::Approx(11.0F));
  CHECK(first.ring.radius_at_anchor_growth == Catch::Approx(0.0F));
  CHECK(first.ring.radius_at_height_growth == Catch::Approx(0.0F));
  // LES SUIVANTS : tous les vingt-cinq tics, soixante de vie, sortie à mi-vie,
  // et les deux rayons qui rentrent en ralentissant.
  CHECK_FALSE(suivants.life_from_effect);
  CHECK(suivants.life_ticks == 60);
  CHECK(suivants.fade_out_tick == 30);
  CHECK(suivants.first_tick == 25);
  CHECK(suivants.period_ticks == 25);
  CHECK(suivants.ring.radius_at_height == Catch::Approx(10.5F));
  CHECK(suivants.ring.radius_at_anchor_growth == Catch::Approx(-0.12F));
  CHECK(suivants.ring.radius_at_anchor_acceleration == Catch::Approx(0.003F));
  CHECK(suivants.ring.radius_at_height_growth == Catch::Approx(-0.12F));
  CHECK(suivants.ring.radius_at_height_acceleration == Catch::Approx(0.003F));
}

TEST_CASE("la grêle pose quatre exemplaires par case, deux de pierre et deux de glace",
          "[render][table]") {
  // ⚠ CENT NŒUDS SUR VINGT-CINQ CASES. Un seul par case rendait le champ quatre
  // fois trop clair, et tout en glace.
  const std::span<const EffectNodePattern> grele = EffectNodePatternsFor(522);
  REQUIRE(grele.size() == 1);
  const EffectNodePattern& node_pattern = grele.front();
  CHECK(node_pattern.shape == EffectNodeShape::kCrossSpikes);
  CHECK(node_pattern.ring.instance_count == 100);
  CHECK(node_pattern.ring.copies_per_place == 4);
  CHECK(node_pattern.ring.instance_placement ==
        ro::render::EffectNodeInstancePlacement::kSquareGrid);
  CHECK(node_pattern.textures_per_copy);
  const std::span<const std::string_view> images =
      ro::render::EffectNodePatternTextures(node_pattern);
  REQUIRE(images.size() == 2);
  CHECK(images[0].ends_with("stone.bmp"));
  CHECK(images[1].ends_with("ice.tga"));
  // LES TIRAGES DANS L'ORDRE DU BÂTISSEUR : deux jetés, l'échelle, le lacet,
  // puis le penchant.
  CHECK(node_pattern.supplement.draws_discarded_before_scale == 2);
  CHECK(node_pattern.supplement.yaw_drawn_before_lean);
}

TEST_CASE("la grille range ses tirages comme le client : profondeur puis abscisse",
          "[render][table]") {
  // Le client boucle sur sa profondeur de moins dix à dix, puis sur son
  // abscisse. Sa profondeur est la nôtre au signe près : la première case est à
  // moins dix d'abscisse et à PLUS dix de profondeur chez nous.
  std::uint32_t generator = 1U;
  const glm::vec3 earliest = ro::render::EffectNodeInstanceOffset(
      ro::render::EffectNodeInstancePlacement::kSquareGrid, 25, 0, 5.0F, 0.0F, 0.0F, generator);
  CHECK(earliest.x == Catch::Approx(-10.0F));
  CHECK(earliest.z == Catch::Approx(10.0F));
  const glm::vec3 deuxieme = ro::render::EffectNodeInstanceOffset(
      ro::render::EffectNodeInstancePlacement::kSquareGrid, 25, 1, 5.0F, 0.0F, 0.0F, generator);
  CHECK(deuxieme.x == Catch::Approx(-5.0F));
  CHECK(deuxieme.z == Catch::Approx(10.0F));
  const glm::vec3 sixieme = ro::render::EffectNodeInstanceOffset(
      ro::render::EffectNodeInstancePlacement::kSquareGrid, 25, 5, 5.0F, 0.0F, 0.0F, generator);
  CHECK(sixieme.z == Catch::Approx(5.0F));
}

TEST_CASE("EF_WARPZONE rejoue son horaire tous les 145 tics, sans fin", "[render][table]") {
  // 🔴 SON BÂTISSEUR RAMÈNE L'ÂGE DE SON INSTANCE À ZÉRO À 145, et repart comme
  // à l'âge zéro : la colonne, les anneaux tous les 28 tics sous l'âge 117, les
  // sprites tous les dix tics. L'instance ne meurt jamais de sa durée.
  constexpr std::uint32_t kWarpZoneEffect = 61;
  constexpr std::int32_t kCycleTicks = 145;
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(kWarpZoneEffect);
  REQUIRE(patterns.size() == 3);
  const EffectNodePattern& column = patterns[0];
  const EffectNodePattern& ring = patterns[1];
  const EffectNodePattern& sprite = patterns[2];
  for (const EffectNodePattern& pattern : patterns) {
    CHECK(pattern.supplement.age_wrap_ticks == kCycleTicks);
    CHECK(ro::render::EffectNodePatternLastSowTick(pattern) == ro::render::kEffectLifeForever);
  }
  CHECK_FALSE(ro::render::EffectInstanceLifeIsKnown(kWarpZoneEffect));

  // La colonne : une par cycle.
  CHECK(ro::render::EffectNodePatternSowsAt(column, 0));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAt(column, kCycleTicks - 1));
  CHECK(ro::render::EffectNodePatternSowsAt(column, kCycleTicks));
  CHECK(ro::render::EffectNodePatternSowsAt(column, 2 * kCycleTicks));
  // Les anneaux : cinq par cycle, aux âges 0, 28, 56, 84 et 112.
  std::int32_t rings_in_a_cycle = 0;
  for (std::int32_t tick = 0; tick < kCycleTicks; ++tick) {
    rings_in_a_cycle += ro::render::EffectNodePatternSowsAt(ring, tick) ? 1 : 0;
  }
  CHECK(rings_in_a_cycle == 5);
  CHECK(ro::render::EffectNodePatternSowsAt(ring, 112));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAt(ring, 140));
  CHECK(ro::render::EffectNodePatternSowsAt(ring, kCycleTicks + 28));
  // Les sprites : tous les dix tics de l'âge, qui repart à 145 — et non tous
  // les dix tics de l'horloge.
  CHECK(ro::render::EffectNodePatternSowsAt(sprite, 140));
  CHECK(ro::render::EffectNodePatternSowsAt(sprite, kCycleTicks));
  CHECK_FALSE(ro::render::EffectNodePatternSowsAt(sprite, 150));
  CHECK(ro::render::EffectNodePatternSowsAt(sprite, kCycleTicks + 10));

  // 🔴 ET SA VIE DE TABLE BORNE ENCORE SES NŒUDS PAR LE FONDU, que l'âge
  // reboucle ou non : 250 tics, que son bâtisseur ne récrit pas. Un sprite de
  // 130 tics semé à l'âge 130 ou 140 du cycle lui survivrait ; il pâlit dès sa
  // naissance sur ce qui reste à l'instance, son âge déjà crû.
  REQUIRE(sprite.life_ticks == 130);
  const auto sprite_sown_at = [&sprite](std::int32_t tick) {
    ro::render::EffectNode node = ro::render::EffectNodeFromPattern(sprite);
    ro::render::EffectNodeFadesWithACyclingInstance(
        node, ro::render::EffectNodePatternInstanceCycle(sprite, tick));
    return node;
  };
  CHECK(sprite_sown_at(kCycleTicks + 130).owner_fade_at_age == 0);
  CHECK(sprite_sown_at(kCycleTicks + 130).owner_fade_over_ticks == 119);
  CHECK(sprite_sown_at(140).owner_fade_over_ticks == 109);
  CHECK(sprite_sown_at(120).owner_fade_at_age == ro::render::kEffectNodeOwnerFadeNever);
  // Les anneaux ne lui survivent jamais.
  for (std::int32_t tick = 0; tick < kCycleTicks; ++tick) {
    if (ro::render::EffectNodePatternSowsAt(ring, tick)) {
      ro::render::EffectNode node = ro::render::EffectNodeFromPattern(ring);
      ro::render::EffectNodeFadesWithACyclingInstance(
          node, ro::render::EffectNodePatternInstanceCycle(ring, tick));
      CHECK(node.owner_fade_at_age == ro::render::kEffectNodeOwnerFadeNever);
    }
  }
}
