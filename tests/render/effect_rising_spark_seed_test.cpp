#include "ro/render/effect_rising_spark_seed.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using ro::render::EffectRisingSparkPattern;
using ro::render::EffectRisingSparkPatternsFor;
using ro::render::EffectRisingSparkPatternTable;

TEST_CASE("Étincelle montante : EF_HEAL sème du VERT autour de celui qu'il soigne",
          "[render][table]") {
  // Effet 312, le soin du prêtre. Chaque nombre ci-dessous vient de son
  // bâtisseur.
  //
  // ⚠ LE VERT N'EST NI DANS LA TEXTURE NI DANS LE SPRITE : `pok1.tga` est GRISE,
  // à cent soixante-dix par canal. Il vient de la marque ONZE, à laquelle la
  // table des carrés teintés donne 220, 250, 220.
  //
  // ⚠ UNE SEULE RANGÉE, ET CE TEST EN A EXIGÉ DEUX. Le bâtisseur en bâtit un par
  // STYLE — un pour le style un, un autre pour tout le reste — et 312 passe
  // zéro : il n'en pose qu'UN. Une table qui lirait la FONCTION et non l'effet
  // les prêterait tous les deux à chacun de ses appelants, et le crible par
  // texture ne peut pas les séparer : le style zéro charge pok1, le style un
  // charge pok1 ET pok3, donc aucune image n'est exclusive au style zéro. C'est
  // la GARDE du site qui tranche.
  //
  // ⚠ ET IL SÈME TOUS LES SIX TICS jusqu'au cinquantième, soit HUIT bouffées.
  // Le joueur l'a vu avant nos tables : « le client natif semble produire plus
  // de particules ». Il en produit huit fois plus.
  const std::span<const EffectRisingSparkPattern> heal = EffectRisingSparkPatternsFor(312);
  REQUIRE(heal.size() == 1);
  CHECK(heal.front().schedule.first_tick == 1);
  CHECK(heal.front().schedule.last_tick == 50);
  CHECK(heal.front().schedule.period_ticks == 6);
  std::size_t bouffees = 0;
  for (std::int32_t tick = 0; tick <= 60; ++tick) {
    if (ro::render::EffectNodeScheduleSowsAt(heal.front().schedule, tick)) {
      ++bouffees;
    }
  }
  CHECK(bouffees == 8);
  const EffectRisingSparkPattern& pattern = heal.front();
  CHECK(pattern.colour_mark == 11);
  CHECK(pattern.texture == "data\\texture\\effect\\pok1.tga");
  CHECK(pattern.half_size.low == Catch::Approx(2.0F));
  CHECK(pattern.half_size.high == Catch::Approx(2.9F));
  CHECK(pattern.rise_per_tick.low == Catch::Approx(0.3F));
  CHECK(pattern.rise_per_tick.high == Catch::Approx(0.69F));
  CHECK(pattern.scatter_half_width == Catch::Approx(6.0F));
  // ⚠ SON ANCRE EST SUR LE PORTEUR, sans élévation : c'est son cousin 331 qui se
  // tient six unités plus haut. Les confondre poserait les étincelles de Heal
  // au-dessus de la tête.
  CHECK(pattern.lift_above_owner == Catch::Approx(0.0F));
  // Et il n'ÉCLATE pas : son bâtisseur ne charge qu'une texture pour ce style.
  CHECK_FALSE(pattern.allows_flash);
  CHECK(pattern.flash_texture.empty());
}

TEST_CASE("Étincelle montante : EF_HEAL4 est CELUI que le soin joue", "[render][table]") {
  // ⚠ EN JEU, LE SOIN DU PRÊTRE NE JOUE NI 312 NI 320 MAIS 325. C'est son
  // bâtisseur qui le dit, et le style que
  // son site d'appel lui passe : un pour 325, zéro pour 313, deux pour les deux
  // autres. Le style seul sépare des semis que tout le reste confond.
  //
  // ⚠ ET IL LACHE QUATORZE BOUFFEES. Son bâtisseur pose deux nœuds de ce
  // sous-type, et les DEUX sont périodiques : l'un tous les SIX tics, sa
  // première bouffée au sixième, l'autre tous les neuf quand l'âge vaut deux
  // modulo neuf — deux, onze, vingt… —, jusqu'au cinquantième tous les deux.
  // Huit et six.
  const std::span<const EffectRisingSparkPattern> heal4 = EffectRisingSparkPatternsFor(325);
  REQUIRE(heal4.size() == 2);
  CHECK(heal4[0].schedule.first_tick == 1);
  CHECK(heal4[0].schedule.last_tick == 50);
  CHECK(heal4[0].schedule.period_ticks == 6);
  CHECK(heal4[1].schedule.first_tick == 2);
  CHECK(heal4[1].schedule.last_tick == 50);
  CHECK(heal4[1].schedule.period_ticks == 9);
  CHECK(ro::render::EffectNodeScheduleSowsAt(heal4[1].schedule, 2));
  CHECK(ro::render::EffectNodeScheduleSowsAt(heal4[1].schedule, 11));
  // Six bouffées pour l'une, huit pour l'autre : quatorze.
  std::size_t bouffees = 0;
  for (std::int32_t tick = 0; tick <= 60; ++tick) {
    for (const EffectRisingSparkPattern& chacun : heal4) {
      if (ro::render::EffectNodeScheduleSowsAt(chacun.schedule, tick)) {
        ++bouffees;
      }
    }
  }
  CHECK(bouffees == 14);
  const EffectRisingSparkPattern& pattern = heal4.front();
  CHECK(pattern.client_function == "Effect_BuildUpdate_HealBlessRingsPok");
  // IL MONTE PLUS VITE QUE SON COUSIN 313 — six dixièmes contre quatre —, et
  // c'est la SEULE chose que le style change dans ce semis.
  CHECK(pattern.rise_per_tick.low == Catch::Approx(0.6F));
  CHECK(pattern.rise_per_tick.high == Catch::Approx(0.99F));
  const EffectRisingSparkPattern& cousin = EffectRisingSparkPatternsFor(313).front();
  CHECK(cousin.rise_per_tick.low == Catch::Approx(0.4F));
  CHECK(cousin.half_size.low == Catch::Approx(pattern.half_size.low));
  CHECK(cousin.scatter_half_width == Catch::Approx(pattern.scatter_half_width));
  CHECK(cousin.colour_mark == pattern.colour_mark);

  // ET IL ÉCLATE, lui : son bâtisseur charge deux textures pour ce nœud, quand
  // celui de 312 n'en charge qu'une.
  CHECK(pattern.allows_flash);
  CHECK(pattern.flash_texture == "data\\texture\\effect\\pok3.tga");
  CHECK(pattern.colour_mark == 11);
  // Son ancre est sur le porteur : les étincelles montent des pieds du soigné.
  CHECK(pattern.lift_above_owner == Catch::Approx(0.0F));
}

TEST_CASE("Étincelle montante : les quatre carrés ne se ressemblent pas", "[render][table]") {
  // ⚠ LE CLIENT TIRE DANS LA BOUCLE QUI SÈME LES QUATRE PISTES : la taille, la
  // montée et les deux écarts du plan. Leur donner la même valeur ferait quatre
  // jumeaux superposés là où il en montre quatre différents, et c'est exactement
  // le genre d'écart qu'on ne voit pas sur une capture.
  const EffectRisingSparkPattern& pattern = EffectRisingSparkPatternsFor(312).front();
  std::uint32_t seed = 1U;
  const auto tracks = ro::render::SeedEffectRisingSparkTracks(pattern, seed);

  bool tailles_pareilles = true;
  bool ancres_pareilles = true;
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    tailles_pareilles =
        tailles_pareilles && tracks[rang].radius_at_anchor == tracks[0].radius_at_anchor;
    ancres_pareilles = ancres_pareilles && tracks[rang].anchor.x == tracks[0].anchor.x;
  }
  CHECK_FALSE(tailles_pareilles);
  CHECK_FALSE(ancres_pareilles);

  for (const ro::render::EffectTrack& track : tracks) {
    CHECK(track.active);
    // L'ALPHA NAÎT NUL : l'étincelle ne se voit pas au tic du semis.
    CHECK(track.alpha == 0);
    CHECK(track.ticks == 0);
    // Chaque tirage reste dans ses bornes.
    CHECK(track.radius_at_anchor >= Catch::Approx(2.0F));
    CHECK(track.radius_at_anchor <= Catch::Approx(2.9F));
    CHECK(track.value[2] >= Catch::Approx(0.3F));
    CHECK(track.value[2] <= Catch::Approx(0.69F));
    // ⚠ AUCUNE DÉRIVE : le semis pose zéro, et les étincelles montent DROIT. Le
    // cap que le mouvement tire ne se voit alors nulle part.
    CHECK(track.value[1] == Catch::Approx(0.0F));
    // L'ancre est un ÉCART, dans le carré que la dispersion décrit.
    CHECK(std::abs(track.anchor.x) <= Catch::Approx(6.0F));
    CHECK(std::abs(track.anchor.z) <= Catch::Approx(6.0F));
    CHECK(track.anchor.y == Catch::Approx(0.0F));
    CHECK(track.mark[2] == 11);
    CHECK(track.mark[1] == 0);
  }
}

TEST_CASE("Étincelle montante : la même graine rejoue la même troupe", "[render][table]") {
  const EffectRisingSparkPattern& pattern = EffectRisingSparkPatternsFor(312).front();
  std::uint32_t one = 7U;
  std::uint32_t other = 7U;
  const auto first = ro::render::SeedEffectRisingSparkTracks(pattern, one);
  const auto second = ro::render::SeedEffectRisingSparkTracks(pattern, other);
  for (std::size_t rang = 0; rang < first.size(); ++rang) {
    INFO("piste " << rang);
    CHECK(first[rang].radius_at_anchor == second[rang].radius_at_anchor);
    CHECK(first[rang].anchor.x == second[rang].anchor.x);
  }
  // ET LE GÉNÉRATEUR A AVANCÉ : deux troupes semées à la suite diffèrent.
  const auto suivante = ro::render::SeedEffectRisingSparkTracks(pattern, one);
  CHECK(suivante[0].radius_at_anchor != first[0].radius_at_anchor);
}

TEST_CASE("Étincelle montante : à l'ÉCLAT, les quatre pistes ne partagent plus leur texture",
          "[render][table]") {
  // ⚠ LA PREMIÈRE MARQUE EST UN INDEX DE TEXTURE, piste par piste : le client la
  // passe au traceur de quads pour la piste QU'IL DESSINE. Le mouvement l'allume
  // à deux tics propres à chaque piste — trente-cinq et trente-six pour la
  // première, sept de moins ensuite —, et c'est ainsi qu'une étincelle éclate.
  //
  // Ce test lie les deux moitiés : un semis qui autorise l'éclat, et un
  // mouvement qui le pose. Tant que la figure ne lisait pas cette marque, les
  // deux pouvaient être justes sans que rien ne paraisse.
  const std::span<const EffectRisingSparkPattern> effect_id = EffectRisingSparkPatternsFor(331);
  REQUIRE_FALSE(effect_id.empty());
  const EffectRisingSparkPattern& pattern = effect_id.front();
  REQUIRE(pattern.allows_flash);
  REQUIRE_FALSE(pattern.flash_texture.empty());

  std::uint32_t seed = 5U;
  auto tracks = ro::render::SeedEffectRisingSparkTracks(pattern, seed);
  for (const ro::render::EffectTrack& track : tracks) {
    CHECK(track.mark[1] == 1);
    CHECK(track.mark[0] == 0);
  }

  // Au trente-cinquième tic, la PREMIÈRE éclate et les trois autres non : deux
  // lots de rendu, là où un seul suffisait au tic précédent.
  for (int tick = 0; tick < 35; ++tick) {
    ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
  }
  CHECK(tracks[0].mark[0] == 1);
  CHECK(tracks[1].mark[0] == 0);
  CHECK(tracks[2].mark[0] == 0);
  CHECK(tracks[3].mark[0] == 0);

  // Et la quatrième a déjà éclaté, vingt et un tics plus tôt : elle est
  // retombée sur sa première texture.
  for (int tick = 0; tick < 3; ++tick) {
    ro::render::AdvanceRisingSparkTracks(tracks, 0, 0, seed);
  }
  CHECK(tracks[0].mark[0] == 0);
}

TEST_CASE("Étincelle montante : la table entière se tient", "[render][table]") {
  const std::span<const EffectRisingSparkPattern> table = EffectRisingSparkPatternTable();
  REQUIRE_FALSE(table.empty());
  std::uint32_t previous = 0;
  for (const EffectRisingSparkPattern& pattern : table) {
    INFO("effet " << pattern.effect_id);
    // Triée, pour que la recherche par bornes dise vrai.
    CHECK(pattern.effect_id >= previous);
    previous = pattern.effect_id;
    // Un carré sans taille est un point : il ne se verrait pas.
    CHECK(pattern.half_size.low > 0.0F);
    CHECK(pattern.half_size.high >= pattern.half_size.low);
    // Une étincelle qui ne monte pas n'est pas une étincelle montante.
    CHECK(pattern.rise_per_tick.low > 0.0F);
    CHECK(pattern.rise_per_tick.high >= pattern.rise_per_tick.low);
    CHECK_FALSE(pattern.texture.empty());
    // ⚠ UNE TEXTURE D'ÉCLAT SANS ÉCLAT NE SERAIT JAMAIS MONTRÉE, et une marque
    // d'éclat sans seconde texture ferait clignoter vers rien.
    CHECK(pattern.flash_texture.empty() != pattern.allows_flash);
    // La marque de couleur est celle d'un carré teinté, et zéro n'en est pas une.
    CHECK(pattern.colour_mark > 0);
    // ⚠ UN HORAIRE QUI SE REPETE A BESOIN D'UNE BORNE : une période sans dernier
    // tic sèmerait tant que l'horloge tourne, et la troupe enflerait sans fin.
    CHECK(pattern.schedule.period_ticks >= 0);
    if (pattern.schedule.period_ticks > 0) {
      CHECK(pattern.schedule.last_tick > 0);
    }
  }
}

TEST_CASE("Étincelle montante : l'ancre est posée DANS LE MONDE au semis", "[render][table]") {
  // La figure n'ajoute jamais l'origine du nœud — le client pousse un au
  // dix-huitième paramètre de son traceur. Une ancre laissée en écart poserait
  // les étincelles au coin de la carte.
  const EffectRisingSparkPattern& pattern = EffectRisingSparkPatternsFor(312).front();
  const glm::vec3 bearer{500.0F, 7.0F, 300.0F};
  std::uint32_t seed = 7U;
  const ro::render::PooledEffectNode node =
      ro::render::SowEffectRisingSparkNode(pattern, bearer, seed);
  for (const ro::render::EffectTrack& track : node.tracks) {
    CHECK(std::abs(track.anchor.x - bearer.x) <= pattern.scatter_half_width);
    CHECK(std::abs(track.anchor.z - bearer.z) <= pattern.scatter_half_width);
    CHECK(track.anchor.y == Catch::Approx(bearer.y + pattern.lift_above_owner));
  }
}
