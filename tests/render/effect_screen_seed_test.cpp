#include "ro/render/effect_screen_seed.h"

#include <algorithm>
#include <cmath>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_lens_flare_seed.h"
#include "ro/render/effect_screen_quad.h"

using Catch::Approx;
using ro::render::AdvanceEffectScreenNode;
using ro::render::ComposeScreenQuad;
using ro::render::EffectScreenFlag;
using ro::render::EffectScreenNode;
using ro::render::EffectScreenPattern;
using ro::render::EffectScreenPatternsFor;
using ro::render::EffectScreenPatternTable;
using ro::render::EffectScreenQuadRequestOf;
using ro::render::kEffectScreenUnchanged;
using ro::render::SowEffectScreenNode;

TEST_CASE("la table des noeuds d'ecran tient ce qu'elle a mesure", "[render][effect][screen]") {
  const std::span<const EffectScreenPattern> table = EffectScreenPatternTable();
  REQUIRE_FALSE(table.empty());
  for (const EffectScreenPattern& pattern : table) {
    // ⚠ L'UN OU L'AUTRE, ET JAMAIS NI L'UN NI L'AUTRE. Un bâtisseur nomme son
    // image en clair, ou il la COMPOSE — et c'est alors le format qui vaut. Un
    // patron sans l'un ni l'autre n'aurait aucune image à montrer.
    CHECK_FALSE((pattern.texture.empty() && pattern.texture_format.empty()));
    CHECK_FALSE(pattern.client_function.empty());
    // Une plage se lit du bas vers le haut, jamais l'inverse.
    CHECK(pattern.half_width.low <= pattern.half_width.high);
    CHECK(pattern.half_height.low <= pattern.half_height.high);
    CHECK(pattern.step.low <= pattern.step.high);
  }
  CHECK(std::ranges::is_sorted(table, {}, &EffectScreenPattern::effect_id));
}

TEST_CASE("un noeud qui nait transparent a de quoi monter", "[render][effect][screen]") {
  // 🔴 SANS CETTE MONTÉE, LE NŒUD EST MUET TOUTE SA VIE, et son silence ressemble
  // à un effet que le client ne montrerait pas non plus. Six nœuds sur dix
  // étaient dans ce cas : le client les pose à zéro puis écrit
  // `montée = plafond / n`, un CALCUL et non une constante.
  //
  // ⚠ ET LA QUESTION EST « NAÎT-IL À ZÉRO », non « monte-t-il » : un nœud semé à
  // cent vingt-huit se voit sans rien de plus, et c'est le cas de deux d'entre
  // eux.
  for (const EffectScreenPattern& pattern : EffectScreenPatternTable()) {
    if (pattern.alpha > 0.0F) {
      continue;
    }
    INFO(pattern.client_function);
    CHECK(pattern.alpha_step > 0.0F);
  }
}

TEST_CASE("le halo de EF_ENDURE monte jusqu'a son plafond", "[render][effect][screen]") {
  const EffectScreenPattern& halo = EffectScreenPatternsFor(11).front();
  REQUIRE(halo.texture.find("endure") != std::string_view::npos);
  // Sa vie est celle de l'effet — quatre-vingts tics —, et non les cent du
  // constructeur : le bâtisseur la COPIE de son instance.
  CHECK(halo.life_ticks == 80);
  // `plafond / 20` : vingt tics pour atteindre le plafond.
  CHECK(halo.alpha_step == Approx(254.0F / 20.0F));

  std::uint32_t seed = 7U;
  EffectScreenNode node = SowEffectScreenNode(halo, seed);
  CHECK(node.alpha == Approx(0.0F));
  for (int tick = 0; tick < 20; ++tick) {
    REQUIRE(AdvanceEffectScreenNode(node, seed));
  }
  CHECK(node.alpha == Approx(node.alpha_max));
}

TEST_CASE("la poussiere de EF_ENDURE nait sur un cercle", "[render][effect][screen]") {
  // ⚠ ET SON RAYON SE POSE SUR LE CAP, celui-là même qui la fait avancer — d'un
  // pas NÉGATIF. La poussière naît donc loin et revient vers le porteur ; tirer
  // un angle à part romprait cette convergence.
  const EffectScreenPattern& poussiere = EffectScreenPatternsFor(11).back();
  REQUIRE(poussiere.texture.find("alpha_down") != std::string_view::npos);
  CHECK(poussiere.step.high < 0.0F);

  std::uint32_t seed = 13U;
  std::set<int> abscisses;
  for (int attempt = 0; attempt < 16; ++attempt) {
    const EffectScreenNode node = SowEffectScreenNode(poussiere, seed);
    const glm::vec2 from_fixed =
        node.screen_offset - glm::vec2{poussiere.screen_offset_x, poussiere.screen_offset_y};
    const float radius = std::hypot(from_fixed.x, from_fixed.y);
    CHECK(radius >= Approx(poussiere.birth_radius.low).margin(0.01));
    CHECK(radius <= Approx(poussiere.birth_radius.high).margin(0.01));
    abscisses.insert(static_cast<int>(node.screen_offset.x));
    // 🔴 ET SA TRAÎNÉE EST COUCHÉE SUR SA COURSE : le client tire UN angle pour
    // le cap et pour le quad. À l'angle zéro, les rayons seraient tous
    // horizontaux au lieu de converger vers le porteur.
    CHECK(node.turn_degrees == Approx(node.heading_degrees));
  }
  // Seize naissances ne tiennent pas en un point : c'est ce qui fait la gerbe.
  CHECK(abscisses.size() > 1);
  // Le cercle est remonté de vingt pixels, comme le halo : ce retrait suit le
  // calcul de la position.
  CHECK(poussiere.screen_offset_y == Approx(-20.0F));
  // Le halo, lui, ne tourne pas : son angle reste celui du constructeur.
  const EffectScreenPattern& halo = EffectScreenPatternsFor(11).front();
  CHECK_FALSE(halo.turn_follows_heading);
  std::uint32_t halo_seed = 5U;
  CHECK(SowEffectScreenNode(halo, halo_seed).turn_degrees == Approx(0.0F));
}

TEST_CASE("la fumee de glace DERIVE sans pencher", "[render][effect][screen]") {
  // 🔴 LE MÊME ANGLE POUR LE CAP ET POUR LE QUAD appartient aux RAYONS DE
  // GLACE du bâtisseur, un sous-type 2 posé par le même bâtisseur ; la fumée
  // ne pose que son cap. Lui prêter le tour de son voisin la coucherait
  // jusqu'à vingt degrés.
  const std::span<const EffectScreenPattern> fumees = EffectScreenPatternsFor(51);
  REQUIRE_FALSE(fumees.empty());
  CHECK_FALSE(fumees.front().turn_follows_heading);
  std::uint32_t seed = 21U;
  const EffectScreenNode node = SowEffectScreenNode(fumees.front(), seed);
  CHECK(node.turn_degrees == Approx(0.0F));
  // Le cap, lui, est bien tiré : c'est par lui que la fumée dérive.
  CHECK(node.heading_degrees >= -20.0F);
  CHECK(node.heading_degrees <= 19.0F);
}

TEST_CASE("la moitie de ces batisseurs seme a repetition", "[render][effect][screen]") {
  // 🔴 LA CADENCE VIT DANS LA GARDE DU BÂTISSEUR, pas dans un champ du nœud, et
  // un semis unique montrerait un grain là où le client fait une gerbe.
  const EffectScreenPattern& poussiere = EffectScreenPatternsFor(11).back();
  CHECK(poussiere.birth_last_tick == 49);
  CHECK(poussiere.birth_period == 1);
  // Le sable naît un tic sur DEUX pendant soixante — `age < 60 && (age & 1) == 0`.
  REQUIRE_FALSE(EffectScreenPatternsFor(46).empty());
  CHECK(EffectScreenPatternsFor(46).front().birth_last_tick == 58);
  CHECK(EffectScreenPatternsFor(46).front().birth_period == 2);
  // ⚠ ET LE CAS ORDINAIRE RESTE UNE SEULE FOIS, au premier tic : le halo du même
  // effet ne naît qu'à la naissance de l'effet.
  CHECK(EffectScreenPatternsFor(11).front().birth_last_tick == 0);

  for (const EffectScreenPattern& pattern : EffectScreenPatternTable()) {
    INFO(pattern.client_function);
    // Une période nulle ou négative ferait un modulo sur zéro au semis.
    CHECK(pattern.birth_period > 0);
    CHECK(pattern.birth_last_tick >= 0);
  }
}

TEST_CASE("l'identifiant ecarte n'est pas dans la table", "[render][effect][screen]") {
  // ⚠ LE 164 EST LA BARRE D'ICÔNES D'ÉTAT, qui lit l'état de jeu. Le générateur
  // le refuse bruyamment plutôt que de faire entrer de l'état de jeu dans
  // `render`.
  CHECK(EffectScreenPatternsFor(164).empty());
  // ⚠ LE 32 N'Y EST PLUS REFUSÉ. Il ne nommait pas son image parce que le client
  // la COMPOSE : huit noms numérotés, qui se lisent dans le format du
  // `sprintf`.
  REQUIRE_FALSE(EffectScreenPatternsFor(32).empty());
  const EffectScreenPattern& napalm = EffectScreenPatternsFor(32).front();
  CHECK(napalm.texture.empty());
  CHECK_FALSE(napalm.texture_format.empty());
  CHECK(napalm.texture_count == 8);
}

TEST_CASE("un format compose AUTANT de chemins que d'images, a partir de UN",
          "[render][effect][screen]") {
  // ⚠ LE CLIENT NUMÉROTE À PARTIR DE UN : son `sprintf` reçoit l'index de boucle
  // PLUS UN. Partir de zéro chercherait une image qui n'existe pas et en
  // laisserait une dernière de côté.
  const std::vector<std::string> paths =
      ro::render::EffectScreenTexturePaths(EffectScreenPatternsFor(32).front());
  REQUIRE(paths.size() == 8);
  CHECK(paths.front().find("1.TGA") != std::string::npos);
  CHECK(paths.back().find("8.TGA") != std::string::npos);
  CHECK(paths.front().find("%d") == std::string::npos);

  // Et un patron qui nomme son image en clair n'en rend qu'UNE, quoi que dise
  // son compte : c'est le format, et lui seul, qui en fait plusieurs.
  EffectScreenPattern simple;
  simple.texture = "data\\texture\\essai.tga";
  simple.texture_count = 4;
  const std::vector<std::string> single = ro::render::EffectScreenTexturePaths(simple);
  REQUIRE(single.size() == 1);
  CHECK(single.front() == simple.texture);
}

TEST_CASE("un effet peut poser DEUX noeuds d'ecran", "[render][effect][screen]") {
  // EF_ENDURE en pose deux : le halo et la poussière qui tombe. Les ranger par
  // identifiant sans les compter tous n'en montrerait qu'un.
  const std::span<const EffectScreenPattern> endure = EffectScreenPatternsFor(11);
  REQUIRE(endure.size() == 2);
  std::set<std::string_view> images;
  for (const EffectScreenPattern& pattern : endure) {
    images.insert(pattern.texture);
  }
  CHECK(images.size() == 2);
}

TEST_CASE("le semis garde les defauts que le batisseur ne pose pas", "[render][effect][screen]") {
  // ⚠ UN PATRON ÉCRIT ICI, ET NON UNE ENTRÉE DE LA TABLE. Ce cas parle du SEMIS,
  // pas d'un effet : l'adosser à un identifiant le faisait tomber le jour où cet
  // identifiant a changé de table. Trois champs posés, le reste laissé — c'est
  // exactement la question.
  EffectScreenPattern pattern;
  pattern.life_ticks = 17;
  pattern.turn_speed = -5.0F;
  pattern.screen_offset_y = -20.0F;

  std::uint32_t seed = 11U;
  const EffectScreenNode node = SowEffectScreenNode(pattern, seed);

  // Ce que le bâtisseur POSE.
  CHECK(node.life_ticks == 17);
  CHECK(node.turn_speed == Approx(-5.0F));
  CHECK(node.screen_offset.y == Approx(-20.0F));
  // ⚠ ET CE QU'IL NE POSE PAS GARDE LE DÉFAUT DU CLIENT, pas zéro : son alpha
  // vaut 254, et le mettre à zéro rendrait l'éclat invisible.
  CHECK(node.alpha == Approx(254.0F));
  CHECK(node.color[0] == 255);
  CHECK(node.half_width_step_after == kEffectScreenUnchanged);
}

TEST_CASE("un tirage reste dans ses bornes et varie", "[render][effect][screen]") {
  // ⚠ Le Napalm Beat, dont la demi-largeur se tire entre vingt et
  // cinquante-neuf.
  REQUIRE_FALSE(EffectScreenPatternsFor(32).empty());
  const EffectScreenPattern& shard = EffectScreenPatternsFor(32).front();
  REQUIRE(shard.half_width.low < shard.half_width.high);
  std::uint32_t seed = 3U;
  std::set<int> largeurs;
  for (int attempt = 0; attempt < 16; ++attempt) {
    const EffectScreenNode node = SowEffectScreenNode(shard, seed);
    CHECK(node.half_width >= Approx(shard.half_width.low));
    CHECK(node.half_width <= Approx(shard.half_width.high));
    largeurs.insert(static_cast<int>(node.half_width * 100.0F));
  }
  // Le générateur AVANCE : deux nœuds semés à la suite ne sont pas jumeaux.
  CHECK(largeurs.size() > 1);
}

TEST_CASE("un halo seme finit par se dessiner", "[render][effect][screen]") {
  // Un halo qui naît sans hauteur : c'est le mouvement qui l'ouvre, et la
  // figure ne rend rien tant qu'elle est nulle. Patron écrit ici : ce cas parle
  // du mouvement, pas d'un effet.
  EffectScreenPattern shard;
  shard.half_width = {5.0F, 5.0F};
  shard.half_height_step = 1.5F;
  std::uint32_t seed = 5U;
  EffectScreenNode node = SowEffectScreenNode(shard, seed);
  CHECK(node.half_height == Approx(0.0F));
  CHECK(ComposeScreenQuad(EffectScreenQuadRequestOf(node)).empty());

  REQUIRE(AdvanceEffectScreenNode(node, seed));
  CHECK(node.half_height > 0.0F);
  CHECK_FALSE(ComposeScreenQuad(EffectScreenQuadRequestOf(node)).empty());
}

TEST_CASE("le halo de EF_ENDURE pend sous son point", "[render][effect][screen]") {
  // ⚠ LE 183 PORTE LE DRAPEAU D'ANCRAGE, ET LE 37 NON : deux effets du même
  // sous-type ne se posent donc pas de la même façon. Le 183 est une gerbe de
  // reflets, tenue dans sa propre table.
  REQUIRE_FALSE(ro::render::EffectLensFlarePatternsFor(183).empty());
  CHECK(ro::render::HasEffectScreenFlag(ro::render::EffectLensFlarePatternsFor(183).front().flags,
                                        EffectScreenFlag::kHangsBelowAnchor));
  REQUIRE_FALSE(EffectScreenPatternsFor(37).empty());
  CHECK_FALSE(ro::render::HasEffectScreenFlag(EffectScreenPatternsFor(37).front().flags,
                                              EffectScreenFlag::kHangsBelowAnchor));
}

TEST_CASE("la fumee du froid RALENTIT au dixieme tic", "[render][effect][screen]") {
  // 🔴 SIX PIXELS PAR TIC PENDANT DIX, PUIS UN DEMI. Le bâtisseur d'EF_COLDHIT
  // pose les deux pas d'après et le tic de bascule ; sans eux la fumée
  // grossit de six jusqu'au bout et finit
  // deux fois trop large.
  REQUIRE_FALSE(EffectScreenPatternsFor(51).empty());
  const EffectScreenPattern& smoke = EffectScreenPatternsFor(51).front();
  CHECK(smoke.half_width_step == Approx(6.0F));
  CHECK(smoke.half_height_step == Approx(6.0F));
  CHECK(smoke.half_width_step_after == Approx(0.5F));
  CHECK(smoke.half_height_step_after == Approx(0.5F));
  CHECK(smoke.second_step_tick == 10);

  // Et la taille qui en sort, tic par tic : la bascule se voit dans le nombre.
  //
  // ⚠ LE PREMIER TIC LIT L'ÂGE ZÉRO — le répartiteur du client n'incrémente
  // qu'APRÈS le mouvement —, si bien que le pas de six s'applique DIX fois, des
  // âges zéro à neuf, et le demi-pas à partir du onzième tic.
  std::uint32_t seed = 17U;
  EffectScreenNode node = SowEffectScreenNode(smoke, seed);
  CHECK(node.half_width == Approx(1.0F));
  for (int tick = 0; tick < 10; ++tick) {
    REQUIRE(AdvanceEffectScreenNode(node, seed));
  }
  CHECK(node.half_width == Approx(61.0F));
  // ⚠ LA DERNIÈRE IMAGE DESSINÉE est celle de la dernière mise à jour VIVANTE :
  // celle qui tue le nœud avance encore sa taille, mais rien ne la montre.
  float last_drawn_half_width = node.half_width;
  while (AdvanceEffectScreenNode(node, seed)) {
    last_drawn_half_width = node.half_width;
  }
  // Au dernier souffle : 1 + 10 × 6 + 15 × 0,5. Sans les pas d'après, la même
  // fumée finissait à 151.
  CHECK(last_drawn_half_width == Approx(68.5F));

  // ⚠ ET SON QUAD NE TOURNE PAS. Le tour appartient aux rayons de glace du même
  // bâtisseur, un autre sous-type ; la fumée ne pose que son cap.
  CHECK_FALSE(smoke.turn_follows_heading);
  CHECK(node.turn_degrees == Approx(0.0F));
}

TEST_CASE("l'image d'EF_DECAGI nait quatre-vingts pixels plus haut", "[render][effect][screen]") {
  // Le bâtisseur retire quatre-vingts pixels de la hauteur héritée. À zéro,
  // « slow » naîtrait sur les pieds du porteur.
  const auto patterns = EffectScreenPatternsFor(38);
  REQUIRE(patterns.size() == 1);
  CHECK(patterns.front().screen_offset_y == Approx(-80.0F));
}
