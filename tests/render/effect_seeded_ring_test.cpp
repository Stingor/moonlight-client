#include "ro/render/effect_seeded_ring.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_cloud_ring_seed.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pulsing_rings_seed.h"
#include "ro/render/effect_trail_palette.h"

using ro::render::EffectSeededRingPatternTable;
using ro::render::EffectSeededRingTrackTable;
using ro::render::kEffectTracksPerNode;

// ⚠ CETTE TABLE EST CELLE D'UN DESSINATEUR, et elle s'ouvre sous-type par
// sous-type.
//
// Dix-sept bâtisseurs partagent le dessinateur `Ring_0`, donc la figure. Mais la
// forme d'un nœud lie la figure ET le mouvement, et le répartiteur de mouvement
// donne à CHAQUE sous-type le sien. Partager le dessinateur ne suffit donc
// pas : leur prêter le mouvement
// du trente ferait dix-sept figures justes qui bougent faux, et l'on croirait
// les effets traités. Treize attendent encore le leur.

TEST_CASE("Famille Ring_0 : chaque patron porte SA forme", "[render][table]") {
  // ⚠ CE TEST A REMPLACE UN GARDE-SILENCE. La table est restee vide tant
  // qu'aucun de ces mouvements n'etait porte ; elle s'ouvre sous-type par
  // sous-type, et CHAQUE patron porte la forme de SON mouvement. Une forme
  // commune ferait bouger les uns comme les autres — la faute deja commise.
  std::uint32_t seed = 3U;
  for (const ro::render::EffectSeededRingPattern& pattern : EffectSeededRingPatternTable()) {
    INFO("effet " << pattern.effect_id);
    const ro::render::PooledEffectNode node = ro::render::SowEffectSeededRingNode(
        pattern, {}, seed, ro::render::EffectNodeLifeFor(pattern.effect_id));
    CHECK(node.shape == pattern.shape);
    CHECK(pattern.texture.starts_with("data"));
  }
}

TEST_CASE("Famille Ring_0 : un azimut TIRÉ reste dans ses bornes", "[render][table]") {
  // ⚠ L'AZIMUT SE TIRE CHEZ PLUSIEURS, et le refuser faute d'être un nombre
  // laissait sans anneau cinq des six effets de cette table — dont EF_HEAL4,
  // l'anneau qui s'ouvre au pied du soigné.
  //
  // Deux bornes égales sont un azimut FIXE, et ne doivent PAS brûler de tirage :
  // le faire décalerait ceux des pistes suivantes. Le test l'exige des deux
  // côtés — une piste fixe rend toujours la même valeur, une piste tirée reste
  // entre ses bornes.
  bool one_drawn = false;
  for (const ro::render::EffectSeededRingPattern& pattern : EffectSeededRingPatternTable()) {
    INFO("effet " << pattern.effect_id);
    for (int attempt = 0; attempt < 8; ++attempt) {
      std::uint32_t seed = static_cast<std::uint32_t>(attempt) + 1U;
      const auto tracks = ro::render::SeedEffectSeededRingTracks(pattern, seed);
      for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
        const ro::render::EffectSeededRingTrack& track_seed =
            EffectSeededRingTrackTable()[pattern.first_track + rang];
        CHECK(tracks[rang].azimuth_degrees >= track_seed.azimuth_degrees);
        CHECK(tracks[rang].azimuth_degrees <=
              std::max(track_seed.azimuth_degrees, track_seed.azimuth_high));
        if (track_seed.azimuth_high > track_seed.azimuth_degrees) {
          one_drawn = true;
        } else {
          CHECK(tracks[rang].azimuth_degrees == track_seed.azimuth_degrees);
        }
      }
    }
  }
  CHECK(one_drawn);
}

TEST_CASE("Famille Ring_0 : l'anneau d'EF_HEAL4 est celui du soin", "[render][table]") {
  // ⚠ L'ANNEAU AU PIED DU SOIGNÉ EST UN AUTRE NŒUD QUE SES ÉTINCELLES, et c'est
  // le même bâtisseur qui pose les deux. N'en déclarer qu'un laissait le soin à
  // moitié : des étincelles qui montent de nulle part.
  //
  // ⚠ ET IL EN POSE DEUX, à deux tics et sous deux textures : le sien au tic
  // zéro, en `ring_white.tga`, puis un second au tic un en `alpha_down.tga`.
  // N'en garder qu'un laissait le soin à moitié.
  const std::span<const ro::render::EffectSeededRingPattern> heal4 =
      ro::render::EffectSeededRingPatternsFor(325);
  REQUIRE(heal4.size() == 2);
  CHECK(heal4[0].client_function == "Effect_BuildUpdate_HealBlessRingsPok");
  CHECK(heal4[0].texture == "data\\texture\\effect\\ring_white.tga");
  CHECK(heal4[0].schedule.first_tick == 0);
  CHECK(heal4[1].texture == "data\\texture\\effect\\alpha_down.tga");
  CHECK(heal4[1].schedule.first_tick == 1);
  // ⚠ LES DEUX SONT VERTS. La teinte du premier se décide sur un TEST du style —
  // quatre au style deux, neuf partout ailleurs. L'effet 312 n'est pas au style
  // deux : son anneau principal est vert, sans quoi le soin paraîtrait blafard.
  CHECK(heal4[0].trail_variant == 9);
  CHECK(heal4[1].trail_variant == 9);
  // Chacun une seule fois : ce sont des anneaux, pas une pluie.
  CHECK(heal4[0].schedule.period_ticks == 0);
  CHECK(heal4[1].schedule.period_ticks == 0);
  // Ses quatre pistes sont vives, et deux d'entre elles s'inclinent : les deux
  // premières sont à plat, les deux autres se couchent de cinquante et
  // quarante-huit degrés. Quarante-six et quarante-
  // quatre sont celles du SECOND nœud, bâti au seul style un.
  const std::span<const ro::render::EffectSeededRingTrack> table = EffectSeededRingTrackTable();
  const std::size_t ou = heal4.front().first_track;
  for (std::size_t rang = 0; rang < kEffectTracksPerNode; ++rang) {
    INFO("piste " << rang);
    CHECK(table[ou + rang].active);
    CHECK(table[ou + rang].radius_at_anchor > 0.0F);
  }
  CHECK(table[ou].tilt_degrees == 90);
  CHECK(table[ou + 2].tilt_degrees == 50);
  CHECK(table[ou + 3].tilt_degrees == 48);
}

TEST_CASE("Famille Ring_0 : la VARIANTE DE TEINTE n'est pas une échelle", "[render][table]") {
  // ⚠ CE CHAMP N'EST PAS UNE ÉCHELLE DE SPRITE. Il porte, pour CE
  // dessinateur-ci, le numéro de variante que la palette de traînée consulte —
  // ADR 0017 : le sens d'un champ appartient à son dessinateur. Zéro en dur
  // rendrait BLANCS tous les anneaux de la famille.
  //
  // Neuf donne un vert tendre, et c'est celui du soin.
  const ro::render::EffectTrailVariant vert = ro::render::EffectTrailVariantFor(9);
  CHECK(vert.color[0] == 140);
  CHECK(vert.color[1] == 210);
  CHECK(vert.color[2] == 140);

  // L'anneau d'EF_HEAL qui porte sa teinte la porte VERTE.
  const std::span<const ro::render::EffectSeededRingPattern> heal =
      ro::render::EffectSeededRingPatternsFor(312);
  REQUIRE(heal.size() == 1);
  CHECK(heal.front().trail_variant == 9);

  // ⚠ ET TOUTES NE SONT PAS ZERO : une table entière à zéro voudrait dire que
  // personne ne porte cette teinte.
  //
  // ⚠ MAIS ZERO EST UNE VALEUR DU CLIENT, et exiger que TOUTES soient colorées
  // serait trop fort. Le bâtisseur de l'arche de `EF_DEFENDER` écrit
  // littéralement zéro à cette variante : elle est blanche parce que le client
  // la veut blanche. Exiger le
  // contraire aurait fait inventer une teinte à un anneau qui n'en a pas.
  std::size_t colorees = 0;
  for (const ro::render::EffectSeededRingPattern& pattern :
       ro::render::EffectSeededRingPatternTable()) {
    if (pattern.trail_variant != 0) {
      ++colorees;
    }
  }
  CHECK(colorees > 0);
  CHECK(colorees < ro::render::EffectSeededRingPatternTable().size());
}

TEST_CASE("Famille Ring_0 : la table des pistes se lit par blocs de QUATRE", "[render][table]") {
  // La règle tient même sur une table vide, et c'est elle qui devra tenir quand
  // la table se remplira : un patron désigne la première de ses quatre pistes.
  CHECK(EffectSeededRingTrackTable().size() % kEffectTracksPerNode == 0);
  for (const ro::render::EffectSeededRingPattern& pattern : EffectSeededRingPatternTable()) {
    INFO("effet " << pattern.effect_id);
    CHECK(pattern.first_track % kEffectTracksPerNode == 0);
    CHECK(pattern.first_track + kEffectTracksPerNode <= EffectSeededRingTrackTable().size());
  }
}

TEST_CASE("EF_HEAL3 tire sa port\u00e9e, et son second anneau SUIT le premier", "[render][table]") {
  // 312 et 320 sortent du MÊME bâtisseur, à un paramètre près. 312 pose des
  // constantes ; 320 TIRE, et son second anneau reprend le tirage du premier au
  // lieu d'en refaire un. C'est ce lien-là que le modèle a dû apprendre.
  const std::span<const ro::render::EffectSeededRingPattern> heal_patterns =
      ro::render::EffectSeededRingPatternsFor(320);
  REQUIRE(heal_patterns.size() == 1);
  const std::span<const ro::render::EffectSeededRingTrack> table =
      ro::render::EffectSeededRingTrackTable();
  const ro::render::EffectSeededRingTrack& first = table[heal_patterns.front().first_track];
  const ro::render::EffectSeededRingTrack& second = table[heal_patterns.front().first_track + 1];

  // La première tire : ses bornes DIFFÈRENT.
  CHECK(first.follows_track == ro::render::kSeededRingTrackDrawsAlone);
  CHECK(first.lift_key == 40.0F);
  CHECK(first.lift_key_high == 59.0F);
  CHECK(first.radius_at_anchor == 4.5F);
  CHECK(first.radius_at_anchor_high > 4.99F);

  // La seconde ne tire pas : elle SUIT, à deux dixièmes d'épaisseur près.
  CHECK(second.follows_track == 0);
  CHECK(second.lift_key_plus == 0.0F);
  CHECK(second.radius_at_anchor_plus > 0.19F);
  CHECK(second.radius_at_anchor_plus < 0.21F);

  // ⚠ ET LE SEMIS DOIT RENDRE L'ÉCART CONSTANT, pas seulement le porter. Deux
  // tirages indépendants dans les deux intervalles donneraient un écart variable
  // — deux anneaux qui se croisent une fois sur deux. On sème plusieurs fois,
  // avec des graines différentes, et l'écart ne doit pas bouger.
  for (std::uint32_t seed = 1; seed < 40; ++seed) {
    std::uint32_t draw = seed;
    const std::array<ro::render::EffectTrack, ro::render::kEffectTracksPerNode> tracks =
        ro::render::SeedEffectSeededRingTracks(heal_patterns.front(), draw);
    INFO("graine " << seed);
    CHECK(tracks[0].lift_key >= 40.0F);
    CHECK(tracks[0].lift_key <= 59.0F);
    CHECK(tracks[1].lift_key == tracks[0].lift_key);
    CHECK(tracks[1].radius_at_anchor - tracks[0].radius_at_anchor > 0.19F);
    CHECK(tracks[1].radius_at_anchor - tracks[0].radius_at_anchor < 0.21F);
  }

  // Le témoin : 312, même bâtisseur, ne tire RIEN et ne suit personne.
  const std::span<const ro::render::EffectSeededRingPattern> fixed =
      ro::render::EffectSeededRingPatternsFor(312);
  REQUIRE(fixed.size() == 1);
  const ro::render::EffectSeededRingTrack& without_draw = table[fixed.front().first_track];
  CHECK(without_draw.lift_key == without_draw.lift_key_high);
  CHECK(without_draw.radius_at_anchor == without_draw.radius_at_anchor_high);
  CHECK(without_draw.follows_track == ro::render::kSeededRingTrackDrawsAlone);
}

TEST_CASE("Famille Ring_0 : le portail en attente porte SA colonne", "[render][table]") {
  // 🔴 LE MUR POSE TROIS NŒUDS, UN PAR TIC. Son bâtisseur pose la COLONNE au
  // tic zéro (sous-type 60), les anneaux bleus au tic un (63) et la fumée au
  // tic deux (62). Sans la colonne, la fumée persiste seule.
  //
  // ⚠ LE SOUS-TYPE 60 se dessine de la même jupe que le 63, avec son propre
  // mouvement.
  const std::span<const ro::render::EffectSeededRingPattern> wall =
      ro::render::EffectSeededRingPatternsFor(317);
  REQUIRE(wall.size() == 1);
  CHECK(wall.front().texture == "data\\texture\\effect\\magic_violet.tga");
  CHECK(wall.front().shape == ro::render::PooledNodeShape::kRadialParticleGroups);

  // LES TROIS COLONNES CONCENTRIQUES : même clé, même inclinaison, même style,
  // et TROIS rayons qui décroissent.
  const std::span<const ro::render::EffectSeededRingTrack> table = EffectSeededRingTrackTable();
  constexpr std::array<float, 3> kRadii = {4.0F, 3.0F, 2.0F};
  for (std::size_t rang = 0; rang < kRadii.size(); ++rang) {
    INFO("colonne " << rang);
    const ro::render::EffectSeededRingTrack& track = table[wall.front().first_track + rang];
    CHECK(track.active);
    // ⚠ MILLE QUATRE CENTS TICS AVANT DE S'ÉTEINDRE : c'est ce que le mouvement
    // compare au compteur pour décider si l'alpha monte ou tombe. Un style court
    // ferait disparaître la colonne bien avant le mur.
    CHECK(track.style == 1400);
    CHECK(track.degrees == 360);
    // QUATRE-VINGT-DIX DEGRÉS : la jupe pose alors son bord extérieur TOUT DROIT
    // au-dessus de son bord intérieur — d'où une colonne, et non une jupe.
    CHECK(track.tilt_degrees == 90);
    // ⚠ CINQUANTE N'EST PAS UNE ÉLÉVATION. La jupe compare sa clé à deux
    // flottants EXACTS, et cinquante n'en est pas : la colonne part donc du sol.
    // C'est le mouvement qui recopie cette clé dans les vingt et une cases, où
    // la figure la lit comme la HAUTEUR du ruban.
    CHECK(track.lift_key == 50.0F);
    CHECK(track.radius_at_anchor == kRadii[rang]);
    // L'AZIMUT SE TIRE SUR LE TOUR ENTIER (`rand() % 360`) : deux murs posés
    // côte à côte ne tournent pas en phase.
    CHECK(track.azimuth_degrees == 0);
    CHECK(track.azimuth_high == 359);
  }
  // LA QUATRIÈME EST ÉTEINTE, et le bâtisseur l'écrit.
  CHECK_FALSE(table[wall.front().first_track + 3].active);
}

TEST_CASE("Portail en attente : trois n\u0153uds, trois tics, trois familles", "[render][table]") {
  // 🔴 LA GARDE DE CE BÂTISSEUR EST UNE ÉGALITÉ, PAS UNE PÉRIODE. Il est
  // rappelé à chaque tic et lit trois fois le compteur de son instance :
  // `if (compteur == 0)` la
  // colonne, `if (compteur == 1)` les anneaux qui pulsent, `if (compteur == 2)`
  // la fumée. Au-delà du deuxième tic il ne pose plus rien, jamais.
  //
  // ⚠ LES TROIS NŒUDS VIVENT DANS TROIS TABLES, parce qu'ils sont de trois
  // SOUS-TYPES — 60, 63 et 62 — et qu'une table suit son dessinateur. Aucune
  // des trois ne voit les deux autres : ce banc les vérifie ensemble.
  const std::span<const ro::render::EffectSeededRingPattern> column =
      ro::render::EffectSeededRingPatternsFor(317);
  const std::span<const ro::render::EffectPulsingRingsPattern> rings =
      ro::render::EffectPulsingRingsPatternsFor(317);
  const std::span<const ro::render::EffectCloudRingPattern> smoke =
      ro::render::EffectCloudRingPatternsFor(317);
  REQUIRE(column.size() == 1);
  REQUIRE(rings.size() == 1);
  REQUIRE(smoke.size() == 1);

  CHECK(column.front().schedule.first_tick == 0);
  CHECK(rings.front().first_tick == 1);
  CHECK(smoke.front().schedule.first_tick == 2);

  // ⚠ UNE SEULE FOIS CHACUN : une période ferait renaître sous le mur, toutes
  // les N images, un nœud fait pour vivre dix mille tics.
  CHECK(rings.front().rebuild_period_ticks == 0);
  CHECK(ro::render::EffectNodeScheduleLastSowTick(column.front().schedule) == 0);
  CHECK(ro::render::EffectNodeScheduleLastSowTick(smoke.front().schedule) == 2);

  // ET LES TROIS PORTENT LA MÊME TEINTE, celle que le site choisit par son
  // style : quatre pour le mur bleu. Le rouge — 341 — en porte dix.
  CHECK(column.front().trail_variant == 4);
  CHECK(rings.front().trail_variant == 4);
  CHECK(smoke.front().trail_variant == 4);
  CHECK(ro::render::EffectCloudRingPatternsFor(341).front().trail_variant == 10);
}

TEST_CASE("Famille Ring_0 : un n\u0153ud vit la vie de son EFFET", "[render][table]") {
  // 🔴 LE DÉFAUT DU CONSTRUCTEUR NE TIENT PAS LIEU DE RÉPONSE. Les onze
  // bâtisseurs de cette famille donnent au nœud la DURÉE de l'instance, celle
  // que l'instance compare à son âge pour dire « encore en vie ».
  //
  // ⚠ CENT TICS FONT UNE SECONDE ET DEMIE. La colonne du portail s'éteindrait
  // là, pendant que les anneaux qui pulsent — qui reçoivent déjà la durée de
  // leur effet — tiennent les vingt-trois secondes du mur.
  std::uint32_t seed = 7U;
  const std::span<const ro::render::EffectSeededRingPattern> wall =
      ro::render::EffectSeededRingPatternsFor(317);
  REQUIRE(wall.size() == 1);
  const ro::render::PooledEffectNode column = ro::render::SowEffectSeededRingNode(
      wall.front(), {}, seed, ro::render::EffectNodeLifeFor(317));
  CHECK(column.node.life_ticks == 9999);
  CHECK(column.node.life_ticks != ro::render::kEffectNodeDefaultLifeTicks);

  // ⚠ ET UNE VIE NULLE NE TUE PAS LE NŒUD AU SEMIS : un effet dont la table des
  // durées ne parle pas rendrait zéro, et un nœud mort-né ne se distingue pas
  // d'un nœud absent — l'erreur la plus difficile à voir de cette famille.
  const ro::render::PooledEffectNode sans_duree =
      ro::render::SowEffectSeededRingNode(wall.front(), {}, seed, 0);
  CHECK(sans_duree.node.life_ticks >= 1);
}

TEST_CASE("Famille Ring_0 : deux effets d'un m\u00eame b\u00e2tisseur ne partagent pas sa teinte",
          "[render][table]") {
  // 🔴 LE BÂTISSEUR TRANCHE SA TEINTE PAR UN TEST DU STYLE : il en écrit deux,
  // dans deux branches, et chaque effet prend celle de son style.
  //
  // Les quatre couples ci-dessous sortent chacun d'UN bâtisseur et de DEUX
  // branches ; les confondre donnait à l'un la couleur de l'autre.
  struct Couple {
    std::uint32_t effect_id;
    std::uint8_t variant;
  };
  // Le bâtisseur des anneaux du soin : neuf au style zéro, CINQ au style un —
  // et l'effet 320 passe un. Celui du portail : QUATRE pour le violet, dix pour
  // le rouge.
  constexpr std::array<Couple, 4> kTeintes = {{
      {312, 9},
      {320, 5},
      {317, 4},
      {341, 10},
  }};
  for (const Couple& couple : kTeintes) {
    INFO("effet " << couple.effect_id);
    const std::span<const ro::render::EffectSeededRingPattern> patterns =
        ro::render::EffectSeededRingPatternsFor(couple.effect_id);
    REQUIRE_FALSE(patterns.empty());
    CHECK(patterns.front().trail_variant == couple.variant);
  }
}

TEST_CASE("Famille Ring_0 : une piste ne suit qu'une piste D\u00c9J\u00c0 sem\u00e9e",
          "[render][table]") {
  // ⚠ SUIVRE UNE PISTE POSTÉRIEURE NE VEUT RIEN DIRE : le semis les remplit dans
  // l'ordre, et la suivie serait encore vide. La table ne doit jamais le dire.
  const std::span<const ro::render::EffectSeededRingTrack> table =
      ro::render::EffectSeededRingTrackTable();
  for (std::size_t rang = 0; rang < table.size(); ++rang) {
    const ro::render::EffectSeededRingTrack& track = table[rang];
    if (track.follows_track == ro::render::kSeededRingTrackDrawsAlone) {
      continue;
    }
    INFO("piste " << rang);
    CHECK(track.follows_track >= 0);
    CHECK(static_cast<std::size_t>(track.follows_track) < rang % ro::render::kEffectTracksPerNode);
  }
}

TEST_CASE("l'anneau blanc d'EF_HEAL4 sème le bloc du style deux", "[render][table]") {
  // Le nœud 0 de ce bâtisseur ne compare son paramètre qu'à zéro : aux styles
  // un et deux, il sème les MÊMES pistes, clé de soixante. Celles du nœud 1,
  // bâti après lui, ne valent qu'au seul style un.
  const auto first_white = [](std::uint32_t each_effect_id) {
    for (const ro::render::EffectSeededRingPattern& pattern :
         ro::render::EffectSeededRingPatternsFor(each_effect_id)) {
      if (pattern.client_function == "Effect_BuildUpdate_HealBlessRingsPok" &&
          pattern.schedule.first_tick == 0) {
        return static_cast<int>(pattern.first_track);
      }
    }
    return -1;
  };
  REQUIRE(first_white(253) >= 0);
  CHECK(first_white(325) == first_white(253));
}
