#include "ro/render/effect_track_seed.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_track_figures.h"

using ro::render::ComposeEffectTrackSkirt;
using ro::render::DefaultEffectRingStyle;
using ro::render::EffectRingNodePattern;
using ro::render::EffectRingNodePatternsFor;
using ro::render::EffectRingNodePatternTable;
using ro::render::EffectRingStyle;
using ro::render::EffectRingStyleFor;
using ro::render::EffectRingStyleTable;
using ro::render::EffectTrack;
using ro::render::kEffectRingInstanceLifeTicks;
using ro::render::kEffectTrackSlots;
using ro::render::kEffectTracksPerNode;
using ro::render::SeedEffectRingTracks;

namespace {

// L'alpha de départ que vingt-deux des vingt-trois identifiants passent.
constexpr std::int16_t kUsualStartAlpha = 45;

// Une vie d'instance largement au-dessus du seuil du bâtisseur, et une
// largement en dessous.
constexpr std::int32_t kLongLife = 200;
constexpr std::int32_t kShortLife = kEffectRingInstanceLifeTicks;

// Vingt tics, en millisecondes : de quoi rattraper le retard de la derniere
// piste et laisser l'onde donner une longueur a chaque segment.
constexpr std::uint32_t kMillisecondsWorthTwentyTicks = 20U * ro::render::kEffectTickMs;

}  // namespace

TEST_CASE("Styles d'anneau : la table est TRIÉE et sans doublon", "[render][table]") {
  // La recherche est dichotomique. Un doublon rendrait l'une des deux entrées
  // inatteignable, et un désordre ferait tomber au défaut un style qui a bien
  // sa branche — sans que rien ne le dise.
  const std::span<const EffectRingStyle> table = EffectRingStyleTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].style < table[rang].style);
  }
}

TEST_CASE("Styles d'anneau : un style SANS BRANCHE prend le défaut", "[render][table]") {
  // Le style 0 est employé par un identifiant et n'a pourtant aucune branche :
  // c'est le contrôle que le défaut est bien atteint, et non un trou.
  const EffectRingStyle defaut = DefaultEffectRingStyle();
  CHECK(defaut.trail_variant == 5);
  CHECK(EffectRingStyleFor(0).trail_variant == defaut.trail_variant);
  CHECK(EffectRingStyleFor(0).lift_key == defaut.lift_key);
}

TEST_CASE("Styles d'anneau : le style ne choisit pas sa teinte par une formule",
          "[render][table]") {
  // À partir de trente et un le numéro de teinte est celui du style, et il
  // serait tentant de n'écrire que cela. En dessous les vingt et une branches
  // remappent, et une identité y serait fausse dix fois sur onze.
  CHECK(EffectRingStyleFor(1).trail_variant == 8);
  CHECK(EffectRingStyleFor(5).trail_variant == 4);
  CHECK(EffectRingStyleFor(15).trail_variant == 7);
  CHECK(EffectRingStyleFor(20).trail_variant == 24);
  CHECK(EffectRingStyleFor(31).trail_variant == 31);
  CHECK(EffectRingStyleFor(50).trail_variant == 50);
}

TEST_CASE("Styles d'anneau : le cas 48 REPREND la main après le défaut", "[render][table]") {
  // Le bâtisseur écrit son défaut, PUIS teste quarante-huit et réécrit. Lire
  // les branches sans leur ordre donnerait au style 48 la teinte du défaut, et
  // cinq est une valeur trop plausible pour qu'on s'en aperçoive.
  CHECK(EffectRingStyleFor(48).trail_variant == 48);
  CHECK(EffectRingStyleFor(48).trail_variant != DefaultEffectRingStyle().trail_variant);
}

TEST_CASE("Styles d'anneau : AUCUNE clé ne soulève le ruban", "[render][table]") {
  // La figure compare la clé à deux flottants exacts. Le client range
  // vingt-quatre juste à côté de la clé 24.000999 ; si l'une des douze valeurs
  // tombait dessus, tout un anneau se décalerait d'une unité.
  for (const EffectRingStyle& style : EffectRingStyleTable()) {
    for (const float key : style.lift_key) {
      CHECK(key != 6.001F);
      CHECK(key != 24.000999F);
    }
  }
}

TEST_CASE("Semis d'anneau : les quatre pistes naissent ALLUMÉES et distinctes",
          "[render][effecttrack]") {
  // Quatre pistes au même azimut ne feraient qu'un ruban là où le client en
  // montre quatre, et le décalage d'alpha est ce qui leur donne leur épaisseur.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectRingTracks(kUsualStartAlpha, 0, kLongLife);

  CHECK(tracks[0].azimuth_degrees == 180);
  CHECK(tracks[1].azimuth_degrees == 270);
  CHECK(tracks[2].azimuth_degrees == 0);
  CHECK(tracks[3].azimuth_degrees == 90);

  CHECK(tracks[0].alpha == kUsualStartAlpha + 135);
  CHECK(tracks[1].alpha == kUsualStartAlpha + 90);
  CHECK(tracks[2].alpha == kUsualStartAlpha + 45);
  CHECK(tracks[3].alpha == kUsualStartAlpha);

  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.degrees == 360);
    CHECK(track.tilt_degrees == 80);
    CHECK(track.radius_at_anchor == Catch::Approx(4.1F));
    CHECK(track.ticks == 0);
  }
}

TEST_CASE("Semis d'anneau : le style sème ses clés ET son numéro de teinte",
          "[render][effecttrack]") {
  // Le style 22 est le seul des seize employés qui change les clés ; il sert de
  // preuve que la table est bien consultée piste par piste, et non une fois.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectRingTracks(kUsualStartAlpha, 22, kLongLife);

  CHECK(tracks[0].lift_key == Catch::Approx(15.0F));
  CHECK(tracks[1].lift_key == Catch::Approx(14.0F));
  CHECK(tracks[2].lift_key == Catch::Approx(13.0F));
  CHECK(tracks[3].lift_key == Catch::Approx(12.0F));

  for (const EffectTrack& track : tracks) {
    CHECK(track.style == 22);
  }
  CHECK(EffectRingStyleFor(22).trail_variant == 26);
}

TEST_CASE("Semis d'anneau : une VIE COURTE éteint et retarde les pistes", "[render][effecttrack]") {
  // Le bâtisseur éteint le semis sous son seuil et échelonne un compteur
  // négatif. Semer à pleine teinte ferait apparaître les quatre rubans d'un
  // coup, au lieu de les laisser arriver l'un après l'autre.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectRingTracks(kUsualStartAlpha, 0, kShortLife);

  for (std::size_t rang = 0; rang < kEffectTracksPerNode; ++rang) {
    CHECK(tracks[rang].alpha == 0);
    CHECK(tracks[rang].ticks == -5 * static_cast<std::int32_t>(rang));
    CHECK(tracks[rang].active);
  }
}

TEST_CASE("Semis d'anneau : un semis frais ne dessine RIEN", "[render][effecttrack]") {
  // Les longueurs de segment naissent nulles : c'est le mouvement qui donne son
  // aire au ruban. Une figure qui sortirait des quads ici poserait vingt quads
  // plats au centre de l'effet, dès la première image.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectRingTracks(kUsualStartAlpha, 0, kLongLife);

  for (const EffectTrack& track : tracks) {
    for (const float length : track.value) {
      CHECK(length == 0.0F);
    }
  }
  const std::vector<ro::render::EffectWorldQuad> quads =
      ComposeEffectTrackSkirt(tracks[0], EffectRingStyleFor(0).trail_variant, {});
  for (const ro::render::EffectWorldQuad& quad : quads) {
    const glm::vec3 cote = quad.corners[1] - quad.corners[0];
    const glm::vec3 other = quad.corners[3] - quad.corners[0];
    CHECK(glm::length(glm::cross(cote, other)) == Catch::Approx(0.0F));
  }
}

TEST_CASE("Anneaux d'un effet : la table est TRIÉE par identifiant", "[render][table]") {
  // La recherche est une plage d'égaux : un effet dont les rangées ne se
  // suivraient pas perdrait celles qui traînent, et il n'en resterait qu'un
  // anneau — exactement la moitié que le relevé oubliait.
  const std::span<const EffectRingNodePattern> table = EffectRingNodePatternTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
}

TEST_CASE("Anneaux d'un effet : aucune rangée n'est incomplète", "[render][table]") {
  // Une rangée sans texture serait un anneau invisible, et cela ressemble à une
  // figure ratée bien plus qu'à une absence.
  for (const EffectRingNodePattern& ring : EffectRingNodePatternTable()) {
    CHECK(!ring.texture.empty());
    CHECK(ring.texture.starts_with("data\\texture\\"));
    CHECK(ring.start_alpha > 0);
  }
}

TEST_CASE("Anneaux d'un effet : un effet en sème DEUX", "[render][table]") {
  // Vingt et un des vingt-trois appellent le bâtisseur deux fois. Le second
  // anneau part d'un alpha plus bas, et c'est ce qui donne sa profondeur à la
  // figure ; n'en poser qu'un la montrerait à moitié.
  const std::span<const EffectRingNodePattern> douze = EffectRingNodePatternsFor(12);
  REQUIRE(douze.size() == 2);
  CHECK(douze[0].start_alpha == 45);
  CHECK(douze[1].start_alpha == 25);
  CHECK(douze[0].texture == douze[1].texture);
}

TEST_CASE("Anneaux d'un effet : les deux anneaux n'ont pas toujours la même teinte",
          "[render][table]") {
  // L'effet 1556 est le seul à changer de style entre ses deux appels. C'est le
  // contrôle que le style est bien relevé par appel, et non par effet.
  const std::span<const EffectRingNodePattern> two = EffectRingNodePatternsFor(1556);
  REQUIRE(two.size() == 2);
  CHECK(two[0].style != two[1].style);
  CHECK(EffectRingStyleFor(two[0].style).trail_variant !=
        EffectRingStyleFor(two[1].style).trail_variant);
}

TEST_CASE("Anneaux d'un effet : un effet ABSENT rend un span vide", "[render][table]") {
  // La table ne porte que le sous-type 30. Rendre autre chose qu'un vide ferait
  // semer des anneaux à des effets qui n'en ont pas.
  CHECK(EffectRingNodePatternsFor(0).empty());
  CHECK(EffectRingNodePatternsFor(1).empty());
}

TEST_CASE("Anneaux d'un effet : le plancher tient SOUS le seuil qui allume le semis",
          "[render][effecttrack]") {
  // Le bâtisseur relève la vie de son instance à ce plancher : une instance
  // qui vivait moins naît donc éteinte et échelonnée.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectRingTracks(45, 0, kEffectRingInstanceLifeTicks);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[3].ticks < 0);
}

TEST_CASE("Anneaux d'un effet : la VRAIE vie de l'instance allume le semis",
          "[render][effecttrack]") {
  // 🔴 UNE INSTANCE QUI VIT PLUS DE SOIXANTE TICS naît ALLUMÉE : ses quatre
  // pistes partent visibles, sans retard. Juger sur le plancher seul les
  // éteignait toutes.
  const EffectRingNodePattern& ring = EffectRingNodePatternsFor(12).front();
  const ro::render::PooledEffectNode longue = ro::render::SowEffectRingNode(ring, {}, 1000);
  CHECK(longue.node.life_ticks == 1000);
  CHECK(longue.tracks[0].alpha == ring.start_alpha + 135);
  CHECK(longue.tracks[3].ticks == 0);
  // Et une vie plus courte que le plancher est RELEVÉE, non gardée.
  const ro::render::PooledEffectNode brief = ro::render::SowEffectRingNode(ring, {}, 10);
  CHECK(brief.node.life_ticks == kEffectRingInstanceLifeTicks);
  CHECK(brief.tracks[0].alpha == 0);
}

TEST_CASE("Nœud d'anneau : la troupe fait battre ses PISTES", "[render][effecttrack]") {
  // Les pistes avancent par le même tic que le nœud. Les faire battre au moment
  // de composer les figerait dès qu'une image est sautée, et les ferait courir
  // deux fois si l'on compose deux fois.
  ro::render::EffectNodePool pool;
  pool.Add(ro::render::SowEffectRingNode(EffectRingNodePatternsFor(12).front(), {},
                                         kEffectRingInstanceLifeTicks));
  REQUIRE(pool.size() == 1);
  REQUIRE(pool.nodes().front().tracks.front().value.front() == 0.0F);

  // Assez de temps pour que le retard de la dernière piste soit rattrapé.
  pool.Advance(kMillisecondsWorthTwentyTicks);

  const ro::render::PooledEffectNode& node = pool.nodes().front();
  CHECK(node.node.age_ticks > 0);
  for (const EffectTrack& track : node.tracks) {
    CHECK(track.ticks > 0);
    CHECK(track.value.front() > 0.0F);
  }
}

TEST_CASE("Nœud d'anneau : les quatre pistes N'ONDULENT PAS ensemble", "[render][effecttrack]") {
  // Le rang entre dans la phase, et deux formules se partagent les quatre
  // pistes. Les faire battre d'un seul mouvement montrerait un anneau épais au
  // lieu de quatre rubans qui se croisent.
  ro::render::EffectNodePool pool;
  pool.Add(ro::render::SowEffectRingNode(EffectRingNodePatternsFor(12).front(), {},
                                         kEffectRingInstanceLifeTicks));
  pool.Advance(kMillisecondsWorthTwentyTicks);

  const ro::render::PooledEffectNode& node = pool.nodes().front();
  const std::size_t middle = kEffectTrackSlots / 2;
  bool one_differs = false;
  for (std::size_t rang = 1; rang < kEffectTracksPerNode; ++rang) {
    one_differs = one_differs || node.tracks[rang].value[middle] != node.tracks[0].value[middle];
  }
  CHECK(one_differs);
}

TEST_CASE("Nœud d'anneau : une fois battu, il MONTRE quelque chose", "[render][effecttrack]") {
  // C'est le seul test qui relie les trois mains : le semis, le mouvement et la
  // figure. Chacune passe ses propres épreuves sans que l'ensemble dessine, et
  // c'est exactement ce qui est arrivé au lot 5.
  std::size_t montrent = 0;
  for (const EffectRingNodePattern& ring : EffectRingNodePatternTable()) {
    ro::render::EffectNodePool pool;
    pool.Add(ro::render::SowEffectRingNode(ring, {}, kEffectRingInstanceLifeTicks));
    pool.Advance(kMillisecondsWorthTwentyTicks);
    REQUIRE(pool.size() == 1);

    const ro::render::PooledEffectNode& node = pool.nodes().front();
    float area = 0.0F;
    for (const EffectTrack& track : node.tracks) {
      for (const ro::render::EffectWorldQuad& quad :
           ComposeEffectTrackSkirt(track, node.trail_variant, node.ground_position)) {
        // UNE AIRE, ET PAS SEULEMENT UN QUAD : un ruban de longueur nulle sort
        // vingt quads plats, et les compter ferait passer pour dessine un
        // anneau que le mouvement n'a jamais ouvert.
        area += glm::length(
            glm::cross(quad.corners[1] - quad.corners[0], quad.corners[3] - quad.corners[0]));
      }
    }
    if (area > 0.0F) {
      ++montrent;
    }
  }
  CHECK(montrent == EffectRingNodePatternTable().size());
}
