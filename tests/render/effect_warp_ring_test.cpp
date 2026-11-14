// LES ANNEAUX D'UN PORTAIL DE TÉLÉPORTATION, sous-type 65.
//
// 🔴 CE QU'UN WARP MONTRE AU SOL : pas le sprite que la table du client donne à
// la classe 45 — un villageois, que le jeu ne dessine pas —, mais l'effet 321,
// qu'un warp (classe 45) pose à son apparition.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_duration.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_trail_history.h"
#include "ro/render/effect_warp_ring_seed.h"

using ro::render::AdvanceWarpRingTracks;
using ro::render::EffectTrack;
using ro::render::EffectWarpRingPattern;
using ro::render::EffectWarpRingPatternsFor;
using ro::render::EffectWarpRingPatternTable;
using ro::render::kEffectTracksPerNode;
using ro::render::SeedEffectWarpRingTracks;

TEST_CASE("Anneaux de warp : deux étages par effet, et ce qui les sépare", "[render][warpring]") {
  // ⚠ DEUX ÉTAGES, UN AU TIC UN ET UN AU TIC DEUX. Le bâtisseur les décale d'un
  // degré d'azimut et d'un cinquième d'unité de rayon — c'est ce décalage qui
  // épaissit les anneaux sans les dédoubler, et c'est le « tiered » de son nom.
  const std::span<const EffectWarpRingPattern> portal = EffectWarpRingPatternsFor(321);
  REQUIRE(portal.size() == 2);
  CHECK(portal[0].first_tick == 1);
  CHECK(portal[1].first_tick == 2);
  CHECK(portal[0].texture == "data\\texture\\effect\\ring_blue.tga");
  CHECK(portal[1].texture == "data\\texture\\effect\\ring_blue.tga");
  CHECK(portal[0].radii == std::array<float, 4>{2.5F, 5.0F, 7.5F, 10.0F});
  CHECK(portal[1].radii == std::array<float, 4>{2.7F, 5.2F, 7.7F, 10.2F});
  CHECK(portal[0].azimuths == std::array<std::int16_t, 4>{270, 0, 90, 180});
  CHECK(portal[1].azimuths == std::array<std::int16_t, 4>{271, 1, 91, 181});

  // 🔴 ET LA TEINTE DÉPEND DU STYLE. Au style zéro le bâtisseur donne ONZE au
  // premier étage et QUATRE au second ; au style un il donne DIX-HUIT aux deux.
  // Une teinte unique peindrait l'anneau rouge avec les teintes du bleu.
  CHECK(portal[0].trail_variant == 11);
  CHECK(portal[1].trail_variant == 4);
  const std::span<const EffectWarpRingPattern> red = EffectWarpRingPatternsFor(1185);
  REQUIRE(red.size() == 2);
  CHECK(red[0].texture == "data\\texture\\effect\\ring_red.tga");
  CHECK(red[0].trail_variant == 18);
  CHECK(red[1].trail_variant == 18);

  // La table est triée : la recherche est dichotomique.
  const std::span<const EffectWarpRingPattern> table = EffectWarpRingPatternTable();
  for (std::size_t index = 1; index < table.size(); ++index) {
    CHECK(table[index - 1].effect_id <= table[index].effect_id);
  }
  CHECK(EffectWarpRingPatternsFor(999999).empty());
}

TEST_CASE("Anneaux de warp : les quatre pistes se dessinent, et la clé EST le rayon",
          "[render][warpring]") {
  // ⚠ QUATRE PISTES VIVES, là où les anneaux qui pulsent en gardent une comme
  // registre. En éteindre une ferait disparaître un anneau sur quatre.
  const std::span<const EffectWarpRingPattern> portal = EffectWarpRingPatternsFor(321);
  REQUIRE(!portal.empty());
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectWarpRingTracks(portal[0]);
  for (std::size_t rang = 0; rang < kEffectTracksPerNode; ++rang) {
    INFO("piste " << rang);
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].degrees == 360);
    // La clé et le rayon sont la MÊME mesure : le client recopie l'une dans
    // l'autre au semis comme à chaque tic, et la jupe lit la clé comme la
    // largeur de son ruban.
    CHECK(tracks[rang].lift_key == tracks[rang].radius_at_anchor);
    // ⚠ LES MARQUES VALENT ZÉRO, à l'inverse des anneaux qui pulsent : le
    // mouvement n'écrit la longueur d'un segment QUE là où la marque est nulle.
    // Les poser à un figerait les vingt et un segments.
    CHECK(tracks[rang].mark[0] == 0);
  }
  CHECK(tracks[0].radius_at_anchor == 2.5F);
  CHECK(tracks[3].radius_at_anchor == 10.0F);
  // Un anneau naît ÉTEINT : c'est le mouvement qui l'allume.
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("Anneaux de warp : le rayon se referme puis SE REMBOBINE", "[render][warpring]") {
  // 🔴 C'EST LE REMBOBINAGE QUI REND LA FIGURE PERPÉTUELLE. Le rayon se referme
  // d'un vingtième par tic ; passé le centre il repart de dix, et repart
  // ÉTEINT — ce qui le fait renaître en fondu au bord extérieur au lieu de
  // réapparaître d'un bloc.
  const std::span<const EffectWarpRingPattern> portal = EffectWarpRingPatternsFor(321);
  REQUIRE(!portal.empty());
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectWarpRingTracks(portal[0]);
  constexpr std::int32_t kNodeLife = 99999999;

  AdvanceWarpRingTracks(tracks, kNodeLife);
  CHECK(tracks[0].radius_at_anchor == 2.45F);
  // L'inclinaison se DÉDUIT du rayon : quatre-vingt-dix moins neuf fois lui.
  // Un anneau large est presque à plat, et se redresse en se refermant.
  CHECK(tracks[0].tilt_degrees == 67);
  CHECK(tracks[3].tilt_degrees == 0);
  // L'alpha monte d'un par tic.
  CHECK(tracks[0].alpha == 1);
  // Et les vingt et un segments suivent la clé.
  CHECK(tracks[0].value[0] == tracks[0].lift_key);

  // Le premier anneau part de 2,5 et se referme d'un vingtième : il atteint le
  // centre vers le cinquantième tic.
  //
  // ⚠ « VERS », ET NON « AU » : le pas de 0,05 ne s'écrit pas exactement en
  // binaire, et cinquante soustractions laissent un résidu positif. Le client
  // accumule de la même façon — c'est sa soustraction que nous reproduisons, pas
  // une multiplication qui tomberait juste. Le contrôle guette donc le
  // REMBOBINAGE lui-même, et non un numéro de tic.
  std::int32_t ticks = 1;
  while (tracks[0].radius_at_anchor < 5.0F && ticks < 60) {
    AdvanceWarpRingTracks(tracks, kNodeLife);
    ++ticks;
  }
  CHECK(ticks < 60);
  CHECK(tracks[0].radius_at_anchor == 10.0F);
  // ET IL REPART ÉTEINT : l'alpha retombe à zéro au rembobinage, puis reprend
  // son pas dans le même tic.
  CHECK(tracks[0].alpha == 1);
  // ⚠ ET SES VOISINS N'ONT PAS REMBOBINÉ : partis de 5, 7,5 et 10, ils arrivent
  // à cinquante tics d'intervalle. C'est cet échelonnement qui fait une figure
  // qui ne s'arrête jamais plutôt que quatre anneaux qui clignotent ensemble.
  CHECK(tracks[1].radius_at_anchor > 2.4F);
  CHECK(tracks[3].radius_at_anchor > 7.4F);
  // Leur alpha, lui, n'a pas été remis : il monte d'un par tic depuis zéro.
  CHECK(tracks[3].alpha == static_cast<std::int16_t>(ticks));
}

TEST_CASE("Anneaux de warp : l'alpha retombe à l'approche de la fin de vie", "[render][warpring]") {
  // La figure ne s'éteint QUE par là : son rayon, lui, tourne sans fin. Sans ce
  // fondu, un warp retiré laisserait ses anneaux à pleine force jusqu'à la
  // dernière image.
  const std::span<const EffectWarpRingPattern> portal = EffectWarpRingPatternsFor(321);
  REQUIRE(!portal.empty());
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectWarpRingTracks(portal[0]);
  constexpr std::int32_t kShortLife = 60;
  // Dix-neuf tics de montée : le fondu commence AU tic `vie − 40`, soit au
  // vingtième, et ce tic-là descend au lieu de monter.
  for (std::int32_t tick = 0; tick < 19; ++tick) {
    AdvanceWarpRingTracks(tracks, kShortLife);
  }
  const std::int16_t at_highest = tracks[0].alpha;
  CHECK(at_highest == 19);
  AdvanceWarpRingTracks(tracks, kShortLife);
  // ⚠ IL DESCEND DE TROIS, ET NON D'UN : la chute est trois fois plus rapide
  // que la montée. Un anneau s'efface donc en vingt-trois tics, pas en soixante.
  CHECK(tracks[0].alpha == at_highest - 3);
  for (std::int32_t tick = 0; tick < 40; ++tick) {
    AdvanceWarpRingTracks(tracks, kShortLife);
  }
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("Anneaux de warp : la durée de l'effet est une SENTINELLE", "[render][warpring]") {
  // ⚠ ET C'EST CE QUI REND LE FONDU NÉCESSAIRE. Le client déclare 99999999 pour
  // l'effet 321 : ce n'est pas une durée, c'est « jusqu'à ce qu'on l'enlève ».
  // Un warp montre donc ses anneaux tant qu'il tient sa case, et rien dans la
  // figure ne les arrête.
  REQUIRE(ro::render::EffectDurationIsKnown(321));
  CHECK(ro::render::EffectDurationTicks(321) == 99999999);
}

TEST_CASE("Étincelles de warp : dispersées, petites, et elles TOURNENT", "[render][warpring]") {
  // Les anneaux ne sont pas seuls : le même bâtisseur sème une étincelle tous
  // les dix tics, et le joueur les a jugées trois fois — « naissent en un point
  // au lieu de virvolter », « probablement trop grosses », « particules
  // immobiles ». Trois mesures, trois champs.
  const std::span<const ro::render::EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(321);
  const ro::render::EffectNodePattern* spark = nullptr;
  for (const ro::render::EffectNodePattern& pattern : patterns) {
    if (pattern.shape == ro::render::EffectNodeShape::kSprite) {
      spark = &pattern;
    }
  }
  REQUIRE(spark != nullptr);

  // ⚠ LA DISPERSION VA DANS `scatter`, non dans le lancer : c'est elle que le
  // semis lit pour écarter ses nœuds en cercle. Posée ailleurs, les étincelles
  // s'empilaient au centre.
  CHECK(spark->scatter > 4.0F);

  // ⚠ ET L'ÉCHELLE EST SEPT DIXIÈMES : le client écrit `(rand() % 10) / 10 + 0,7`
  // et la division est ENTIÈRE, donc le tirage rend toujours zéro. Le défaut du
  // constructeur — un — les faisait de moitié trop grosses.
  CHECK(spark->sprite_scale == 0.7F);
  // Pas de fourchette : une borne haute égale à la basse serait un tirage qui
  // ne se voit pas.
  CHECK(spark->sprite_scale_high == 0.0F);

  // 🔴 ET ELLES ORBITENT. Une vitesse de cap fait tourner le CAP du nœud, et sa
  // position se recalcule depuis ce cap : la particule tourne autour de son
  // point de naissance. Sans lui elle reste où elle est née.
  CHECK(spark->orbit_degrees_per_tick > 0.0F);
  // ⚠ ET L'ORBITE NE FAIT RIEN SANS DISPERSION : née au centre, la particule
  // tournerait autour d'elle-même. Les deux se tiennent, et c'est pourquoi le
  // contrôle les lit ensemble.
  CHECK((spark->orbit_degrees_per_tick == 0.0F || spark->scatter > 0.0F));

  // 🔴 ET ELLES S'ENVOLENT. Le bâtisseur écrit trois dixièmes NÉGATIFS au champ
  // 748, et l'ordonnée du client descend : la particule MONTE. Son pas, le 752,
  // vaut `-(748 / vie) / 1,5` — elle ralentit sans jamais retomber. Sans ces
  // deux champs l'étincelle rase le sol, ce que le joueur a vu : « tout ce
  // qu'elles font c'est orbiter dans le centre du warp ».
  CHECK(spark->fall_speed_low == -0.3F);
  CHECK(spark->fall_speed_high == -0.3F);
  CHECK(spark->fall_acceleration_share == Catch::Approx(1.0F / 1.5F));

  // 🔴 ET LEUR DISPERSION COMPTE DEUX FOIS : le bâtisseur bake le bras dans le
  // décalage fixe du nœud, et le mouvement ajoute le même bras une seconde fois
  // en le faisant tourner. L'étincelle naît donc au BORD de l'anneau le plus
  // large, et son cercle est centré sur le point baké — elle spirale vers le
  // centre en montant, au lieu de tourner à distance constante.
  CHECK(spark->scatter_counts_twice);

  // ET LE BRAS S'ALLONGE : cinq dix-millièmes par tic. C'est
  // négligeable à l'œil, et porté quand même.
  CHECK(spark->orbit_radius_step_per_tick == 0.0005F);

  // Un semis tous les dix tics, sans fin : la vie de soixante-dix tics borne
  // la troupe à sept étincelles vivantes.
  CHECK(spark->period_ticks == 10);
  CHECK(spark->life_ticks == 70);
}

TEST_CASE("Étincelle de warp : elle monte de quatorze unités en ralentissant",
          "[render][warpring]") {
  // 🔴 LA MESURE N'EST PAS DANS LE PATRON MAIS DANS CE QU'IL PRODUIT. Le patron
  // porte la vitesse du CLIENT, signe compris ; ce qui compte est que le nœud
  // MONTE chez nous, où l'ordonnée est opposée à la sienne.
  //
  // La hauteur attendue vient de l'arithmétique du client, faite à la main :
  // `somme(0,3 − 0,002857 × t)` pour t de un à soixante-dix, soit 21 − 7,1.
  const std::span<const ro::render::EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(321);
  const ro::render::EffectNodePattern* spark = nullptr;
  for (const ro::render::EffectNodePattern& pattern : patterns) {
    if (pattern.shape == ro::render::EffectNodeShape::kSprite) {
      spark = &pattern;
    }
  }
  REQUIRE(spark != nullptr);

  ro::render::EffectNodeSeed seed;
  seed.fall_speed = {spark->fall_speed_low, spark->fall_speed_high};
  seed.fall_acceleration_share = spark->fall_acceleration_share;
  std::uint32_t generator = 1U;
  const ro::render::EffectLaunchDraw lance =
      ro::render::DrawEffectLaunch(seed, 0.0F, spark->life_ticks, 0.0F, generator);

  // ⚠ LE SIGNE SE RETOURNE AU TIRAGE, et c'est tout ce qui sépare une gerbe qui
  // monte d'une qui s'enterre.
  CHECK(lance.fall_speed > 0.0F);
  CHECK(lance.fall_acceleration < 0.0F);

  ro::render::PooledEffectNode node;
  node.launch_direction = glm::vec3{0.0F, 0.0F, 1.0F};
  node.fall_speed = lance.fall_speed;
  node.fall_acceleration = lance.fall_acceleration;
  const ro::render::EffectScatterWorld world;
  for (std::int32_t tick = 0; tick < spark->life_ticks; ++tick) {
    ro::render::AdvanceLaunchedNode(node, world);
  }
  CHECK(node.ground_position.y == Catch::Approx(13.9F).margin(0.1F));
  // ET ELLE RALENTIT SANS RETOMBER : la vitesse reste positive au dernier tic.
  CHECK(node.fall_speed > 0.0F);
  CHECK(node.fall_speed < lance.fall_speed);
}

TEST_CASE("Étincelle de warp : la troupe entière la fait monter, et le dessin suit",
          "[render][warpring]") {
  // 🔴 CE QUE L'ESSAI D'AVANT NE COUVRAIT PAS. Il posait la direction de lancer
  // à la main et n'appelait que `AdvanceLaunchedNode` ; celui-ci rejoue le
  // câblage du semis — la direction se TIRE — et fait battre la troupe, dont
  // l'ordre compte : l'orbite recompose la place avant que le vol n'y ajoute sa
  // hauteur.
  //
  // ⚠ ET IL LIT LA PLACE QUE LA COMPOSITION LIRA. Un sprite à traînée ne se
  // dessine PAS au point de son nœud mais à celui de chaque cran ; une tête de
  // traînée restée au sol laisserait l'étincelle collée là, quoi que le nœud
  // fasse.
  const std::span<const ro::render::EffectNodePattern> patterns =
      ro::render::EffectNodePatternsFor(321);
  const ro::render::EffectNodePattern* pattern = nullptr;
  for (const ro::render::EffectNodePattern& candidat : patterns) {
    if (candidat.shape == ro::render::EffectNodeShape::kSprite) {
      pattern = &candidat;
    }
  }
  REQUIRE(pattern != nullptr);

  ro::render::EffectNodeSeed seed;
  seed.life_ticks = pattern->life_ticks;
  seed.fade_out_ticks = pattern->life_ticks - pattern->fade_out_tick;
  seed.alpha_max = pattern->alpha_max;
  seed.fall_speed = {pattern->fall_speed_low, pattern->fall_speed_high};
  seed.fall_acceleration_share = pattern->fall_acceleration_share;
  seed.orbit_degrees_per_tick = pattern->orbit_degrees_per_tick;
  seed.orbit_radius_step_per_tick = pattern->orbit_radius_step_per_tick;
  seed.scatter_counts_twice = pattern->scatter_counts_twice;
  seed.trail_trace_count = pattern->trail_trace_count;

  std::uint32_t generator = 7U;
  ro::render::PooledEffectNode pooled;
  pooled.node = ro::render::SowEffectNode(seed, generator);
  const float cap = pooled.node.orbit_degrees * std::numbers::pi_v<float> / 180.0F;
  const glm::vec3 arm{std::cos(cap) * pattern->scatter, 0.0F, std::sin(cap) * pattern->scatter};
  const glm::vec3 centre{100.0F, 0.0F, 200.0F};
  pooled.orbit_anchor = centre + (seed.scatter_counts_twice ? arm : glm::vec3{0.0F, 0.0F, 0.0F});
  pooled.ground_position = pooled.orbit_anchor + arm;
  pooled.orbit_radius = pattern->scatter;
  pooled.orbit_degrees_per_tick = seed.orbit_degrees_per_tick;
  pooled.orbit_radius_step_per_tick = seed.orbit_radius_step_per_tick;
  const ro::render::EffectLaunchDraw lance = ro::render::DrawEffectLaunch(
      seed, 0.0F, pooled.node.life_ticks, pooled.node.step_length, generator);
  pooled.launch_direction = lance.direction;
  pooled.step_decay = lance.step_decay;
  pooled.fall_speed = lance.fall_speed;
  pooled.fall_acceleration = lance.fall_acceleration;
  ro::render::SeedEffectTrailHistory(pooled.trail, seed.trail_trace_count, pooled.node,
                                     pooled.ground_position);
  pooled.ground_position += lance.offset;

  // ⚠ LA NAISSANCE EST AU DOUBLE DE LA DISPERSION, au bord de l'anneau le plus
  // large — c'est le bake du bâtisseur, compté une seconde fois par le
  // mouvement.
  const glm::vec3 from_center = pooled.ground_position - centre;
  CHECK(std::sqrt(from_center.x * from_center.x + from_center.z * from_center.z) ==
        Catch::Approx(2.0F * pattern->scatter).margin(0.01F));

  ro::render::EffectNodePool pool;
  pool.Add(pooled);
  for (std::int32_t tick = 0; tick < pattern->life_ticks - 1; ++tick) {
    pool.Advance(static_cast<std::uint32_t>(ro::render::kEffectTickMs));
  }
  REQUIRE(pool.size() == 1);
  const ro::render::PooledEffectNode& vieilli = pool.nodes().front();

  // 🔴 ELLE A MONTÉ. La hauteur vient de l'arithmétique du client faite à la
  // main : `somme(0,3 − 0,002857 × t)` sur la vie du nœud.
  CHECK(vieilli.ground_position.y == Catch::Approx(13.9F).margin(0.3F));
  // ET ELLE EST RENTRÉE VERS LE CENTRE en tournant : le bras fixe et le bras qui
  // tourne se sont écartés de cent quarante degrés.
  const glm::vec3 fin = vieilli.ground_position - centre;
  CHECK(std::sqrt(fin.x * fin.x + fin.z * fin.z) < pattern->scatter);

  // 🔴 ET LE DESSIN LIT BIEN CETTE PLACE-LÀ. La composition dessine un sprite au
  // point de chaque CRAN de sa traînée dès qu'il en a une, et au point du nœud
  // sinon. L'étincelle n'en a pas — une seule trace ne fait pas d'historique —,
  // donc sa hauteur atteint l'écran sans intermédiaire. Si un jour elle en
  // recevait une, ce contrôle tomberait, et c'est ce qu'on veut : la hauteur
  // passerait alors par un autre chemin, qu'il faudrait mesurer.
  CHECK(vieilli.trail.empty());
}
