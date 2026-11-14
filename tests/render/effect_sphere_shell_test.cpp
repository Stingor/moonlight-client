#include "ro/render/effect_sphere_shell.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <set>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_sphere_shell_seed.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_burst.h"

using Catch::Approx;
using ro::render::AdvanceSphereShellTracks;
using ro::render::ComposeSphereShellQuads;
using ro::render::EffectSphereShellPattern;
using ro::render::EffectSphereShellPatternsFor;
using ro::render::EffectSphereShellPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kSphereShellAlphaCeiling;
using ro::render::kSphereShellAlphaFall;
using ro::render::kSphereShellColumnCount;
using ro::render::kSphereShellEquatorHalfSize;
using ro::render::kSphereShellFacingOffsetDegrees;
using ro::render::kSphereShellFadesAfterTick;
using ro::render::kSphereShellGreens;
using ro::render::kSphereShellInnerCenterLift;
using ro::render::kSphereShellLayerCount;
using ro::render::kSphereShellLiftKey;
using ro::render::kSphereShellQuadsPerTrack;
using ro::render::kSphereShellRowCount;
using ro::render::kSphereShellSecondTexture;
using ro::render::SeedEffectSphereShellTracks;
using ro::render::SowEffectSphereShellNode;

namespace {

[[nodiscard]] EffectTrack Semee(std::uint8_t mark = 0) {
  EffectSphereShellPattern pattern;
  pattern.mark = mark;
  return SeedEffectSphereShellTracks(pattern)[0];
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Advanced(std::int32_t ticks,
                                                                     float bearer_heading = 0.0F,
                                                                     std::uint8_t mark = 0) {
  EffectSphereShellPattern pattern;
  pattern.mark = mark;
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectSphereShellTracks(pattern);
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceSphereShellTracks(tracks, bearer_heading);
  }
  return tracks;
}

// La distance d'un point à l'AXE de la sphère, celui qui passe par son centre.
[[nodiscard]] float HorizontalRadius(const glm::vec3& point) {
  return std::sqrt(point.x * point.x + point.z * point.z);
}

}  // namespace

TEST_CASE("le semis n'allume qu'une piste sur quatre", "[render][effect][shell]") {
  // ⚠ LE BÂTISSEUR COUCHE LES TROIS AUTRES, une à une. En allumer quatre
  // peindrait cent quatre-vingts panneaux là où le client en peint quarante-cinq.
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSphereShellTracks(EffectSphereShellPattern{});
  CHECK(tracks[0].active);
  CHECK_FALSE(tracks[1].active);
  CHECK_FALSE(tracks[2].active);
  CHECK_FALSE(tracks[3].active);
  // Et il naît INVISIBLE : c'est le mouvement qui l'allume.
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].lift_key == Approx(kSphereShellLiftKey));
}

TEST_CASE("le noeud porte DEUX images, dans l'ordre du batisseur", "[render][effect][shell]") {
  // ⚠ LE RELEVÉ N'EN VOYAIT QU'UNE — la seconde, écrite en dur. Celle du site
  // vient PREMIÈRE, et la figure s'en sert comme d'un index.
  EffectSphereShellPattern pattern;
  pattern.texture = "data\\texture\\effect\\guardK.tga";
  const ro::render::PooledEffectNode node = SowEffectSphereShellNode(pattern, {}, 100);
  REQUIRE(node.resource_paths.size() == 2);
  CHECK(node.resource_paths[0] == "data\\texture\\effect\\guardK.tga");
  CHECK(node.resource_paths[1] == kSphereShellSecondTexture);
}

TEST_CASE("la coquille rend quarante-cinq panneaux", "[render][effect][shell]") {
  const std::vector<EffectWorldQuad> quads = ComposeSphereShellQuads(Semee(), {});
  REQUIRE(quads.size() == kSphereShellQuadsPerTrack);
  CHECK(quads.size() == kSphereShellRowCount * kSphereShellColumnCount * kSphereShellLayerCount);

  // ⚠ ET ELLE LES REND À ALPHA NUL. Le client n'a AUCUNE garde d'alpha ici,
  // contrairement à toutes les autres figures du lot : la piste naît invisible
  // et sort quand même ses quarante-cinq quads. Une garde ajoutée ici rendrait
  // ce test vide sans que rien ne le dise.
  CHECK(quads.front().tint[3] == 0);

  // Une piste éteinte, elle, ne rend rien — c'est la seule garde du client.
  EffectTrack faded = Semee();
  faded.active = false;
  CHECK(ComposeSphereShellQuads(faded, {}).empty());
}

TEST_CASE("les panneaux sont TANGENTS a une sphere", "[render][effect][shell]") {
  // ⚠ C'EST CE QUI DISTINGUE CETTE FIGURE D'UN BILLBOARD : le centre de chaque
  // panneau est à la même distance du centre de sa coquille, et ses quatre
  // coins s'en écartent également. Un quad qui ferait face à la caméra n'aurait
  // aucune de ces deux propriétés.
  const EffectTrack track = Semee();
  const std::vector<EffectWorldQuad> quads = ComposeSphereShellQuads(track, {});
  REQUIRE(quads.size() == kSphereShellQuadsPerTrack);

  // La coquille du milieu est centrée sur la clé, à la clé plus deux dixièmes.
  const glm::vec3 centre_de_la_sphere{0.0F, kSphereShellLiftKey, 0.0F};
  for (std::size_t rang = 1; rang < quads.size(); rang += kSphereShellLayerCount) {
    const EffectWorldQuad& quad = quads[rang];
    const glm::vec3 middle =
        (quad.corners[0] + quad.corners[1] + quad.corners[2] + quad.corners[3]) / 4.0F;
    CHECK(glm::length(middle - centre_de_la_sphere) == Approx(kSphereShellLiftKey + 0.2F));
    // Et les quatre coins sont à la même distance du centre du panneau : c'est
    // un CARRÉ, pas un trapèze.
    const float attendue = glm::length(quad.corners[0] - middle);
    for (const glm::vec3& corner : quad.corners) {
      CHECK(glm::length(corner - middle) == Approx(attendue));
    }
  }
}

TEST_CASE("les panneaux du haut et du bas sont plus petits", "[render][effect][shell]") {
  // ⚠ TROIS RANGÉES, ET LEURS TAILLES NE SONT PAS ÉGALES : l'équateur porte des
  // carrés de trois de demi-taille, les deux autres de deux. Leur donner la
  // même taille laisserait des trous à l'équateur ou ferait déborder les pôles.
  const std::vector<EffectWorldQuad> quads = ComposeSphereShellQuads(Semee(), {});
  REQUIRE(quads.size() == kSphereShellQuadsPerTrack);
  const auto half_size = [&quads](std::size_t row) {
    const EffectWorldQuad& quad = quads[row * kSphereShellColumnCount * kSphereShellLayerCount];
    return glm::length(quad.corners[0] - quad.corners[1]) / 2.0F;
  };
  CHECK(half_size(0) == Approx(kSphereShellEquatorHalfSize - 1.0F));
  CHECK(half_size(1) == Approx(kSphereShellEquatorHalfSize));
  CHECK(half_size(2) == Approx(kSphereShellEquatorHalfSize - 1.0F));
}

TEST_CASE("la coquille est OUVERTE derriere", "[render][effect][shell]") {
  // ⚠ CINQ COLONNES AU PAS DE QUARANTE-CINQ FONT UN DEMI-TOUR, pas un tour :
  // c'est ce qui en fait un bouclier et non une boule. Les caps des cinq
  // colonnes de l'équateur s'étendent sur cent quatre-vingts degrés, bornes
  // comprises, et aucun ne revient sur un autre.
  const std::vector<EffectWorldQuad> quads = ComposeSphereShellQuads(Semee(), {});
  std::set<std::int32_t> caps;
  const std::size_t start = 1 * kSphereShellColumnCount * kSphereShellLayerCount;
  for (std::size_t column = 0; column < kSphereShellColumnCount; ++column) {
    const EffectWorldQuad& quad = quads[start + column * kSphereShellLayerCount];
    const glm::vec3 middle =
        (quad.corners[0] + quad.corners[1] + quad.corners[2] + quad.corners[3]) / 4.0F;
    caps.insert(static_cast<std::int32_t>(
        std::lround(std::atan2(static_cast<double>(middle.z), static_cast<double>(middle.x)) *
                    180.0 / std::numbers::pi_v<double>)));
  }
  CHECK(caps.size() == kSphereShellColumnCount);
  CHECK(*caps.rbegin() - *caps.begin() == 180);
}

TEST_CASE("la coquille MONTE au lieu de s'enfoncer", "[render][effect][shell]") {
  // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND. Le centre de la coquille
  // intérieure est DEUX unités au-dessus de celui des deux autres ; recopier le
  // signe du client mettrait tout le bouclier sous le sol.
  const std::vector<EffectWorldQuad> quads = ComposeSphereShellQuads(Semee(), {});
  REQUIRE(quads.size() == kSphereShellQuadsPerTrack);
  for (const EffectWorldQuad& quad : quads) {
    for (const glm::vec3& corner : quad.corners) {
      CHECK(corner.y > 0.0F);
    }
  }
  // Le milieu d'un panneau d'équateur : la coquille intérieure est deux unités
  // plus haut que la suivante, alors qu'elle est plus PRÈS de l'axe.
  const std::size_t start = 1 * kSphereShellColumnCount * kSphereShellLayerCount;
  CHECK(quads[start].corners[0].y ==
        Approx(quads[start + 1].corners[0].y + kSphereShellInnerCenterLift));
}

TEST_CASE("la coquille exterieure porte la seconde image, sauf la marque cinq",
          "[render][effect][shell]") {
  // ⚠ DEUX COQUILLES SUR L'IMAGE DU SITE, UNE SUR CELLE ÉCRITE EN DUR. Les
  // mettre toutes sur la même peindrait quinze panneaux avec la mauvaise image,
  // et rien à l'écran ne dirait laquelle.
  const std::vector<EffectWorldQuad> vert = ComposeSphereShellQuads(Semee(0), {});
  REQUIRE(vert.size() == kSphereShellQuadsPerTrack);
  CHECK(vert[0].texture == 0);
  CHECK(vert[1].texture == 0);
  CHECK(vert[2].texture == 1);
  // Les deux intérieures s'AJOUTENT, la troisième se mélange par l'alpha.
  CHECK(vert[0].blend_source == vert[1].blend_source);
  CHECK(vert[2].blend_destination != vert[0].blend_destination);

  // ⚠ LA MARQUE CINQ RAMÈNE LA TROISIÈME À LA PREMIÈRE IMAGE. C'est le seul des
  // cinq identifiants qui ne montre jamais la seconde.
  const std::vector<EffectWorldQuad> blue = ComposeSphereShellQuads(Semee(5), {});
  CHECK(blue[2].texture == 0);
  CHECK(blue[2].blend_destination == blue[0].blend_destination);
}

TEST_CASE("la marque zero peint TROIS verts, les autres un seul", "[render][effect][shell]") {
  // ⚠ SEULE LA MARQUE ZÉRO FAIT VARIER SA TEINTE D'UNE COQUILLE À L'AUTRE. Lui
  // donner un vert unique aplatirait EF_GUARD, le plus courant des cinq.
  const std::vector<EffectWorldQuad> vert = ComposeSphereShellQuads(Semee(0), {});
  for (std::size_t layer = 0; layer < kSphereShellLayerCount; ++layer) {
    CHECK(vert[layer].tint[0] == kSphereShellGreens[layer][0]);
    CHECK(vert[layer].tint[1] == kSphereShellGreens[layer][1]);
    CHECK(vert[layer].tint[2] == kSphereShellGreens[layer][2]);
  }
  CHECK(vert[0].tint[0] != vert[1].tint[0]);

  const std::vector<EffectWorldQuad> blue = ComposeSphereShellQuads(Semee(5), {});
  for (std::size_t layer = 1; layer < kSphereShellLayerCount; ++layer) {
    CHECK(blue[layer].tint[0] == blue[0].tint[0]);
    CHECK(blue[layer].tint[1] == blue[0].tint[1]);
    CHECK(blue[layer].tint[2] == blue[0].tint[2]);
  }
}

TEST_CASE("l'azimut se RECALE sur le porteur a chaque tic", "[render][effect][shell]") {
  // ⚠ IL N'AVANCE PAS, IL EST ÉCRASÉ : trois quarts de tour de plus que l'angle
  // du porteur, à chaque tic. L'avancer le ferait dériver dès le premier
  // demi-tour du personnage.
  CHECK(Advanced(1)[0].azimuth_degrees == kSphereShellFacingOffsetDegrees);
  CHECK(Advanced(40)[0].azimuth_degrees == kSphereShellFacingOffsetDegrees);
  // Et il se replie dans le tour, une seule fois.
  CHECK(Advanced(5, 100.0F)[0].azimuth_degrees == 10);
  // Le bouclier tourne donc AVEC son porteur, et non malgré lui.
  CHECK(Advanced(5, 45.0F)[0].azimuth_degrees == 315);
}

TEST_CASE("le bouclier monte, se tient, puis s'eteint en enflant", "[render][effect][shell]") {
  // L'alpha monte de vingt pendant cinq tics, et plafonne à cent.
  CHECK(Advanced(1)[0].alpha == 20);
  CHECK(Advanced(5)[0].alpha == kSphereShellAlphaCeiling);
  // ⚠ PUIS IL SE TIENT VINGT ET UN TICS, sans que rien ne bouge. Coller les
  // deux seuils supprimerait ce palier, qui est la moitié de la vie du
  // bouclier.
  const std::array<EffectTrack, kEffectTracksPerNode> au_palier =
      Advanced(kSphereShellFadesAfterTick);
  CHECK(au_palier[0].alpha == kSphereShellAlphaCeiling);
  CHECK(au_palier[0].value[0] == Approx(0.0F));

  // ⚠ ET L'ENFLURE ET LA CHUTE SONT LE MÊME GESTE : la coquille extérieure ne
  // s'écarte QUE pendant qu'elle s'éteint.
  const std::array<EffectTrack, kEffectTracksPerNode> after =
      Advanced(kSphereShellFadesAfterTick + 1);
  CHECK(after[0].alpha == kSphereShellAlphaCeiling - kSphereShellAlphaFall);
  CHECK(after[0].value[0] == Approx(0.1F));

  // L'alpha ne passe pas sous zéro, et l'enflure continue.
  const std::array<EffectTrack, kEffectTracksPerNode> late = Advanced(60);
  CHECK(late[0].alpha == 0);
  CHECK(late[0].value[0] > 3.0F);
}

TEST_CASE("seule la coquille exterieure enfle", "[render][effect][shell]") {
  // ⚠ LE BOUCLIER ÉCLATE PAR SA PEAU. Faire enfler les trois coquilles ferait
  // grossir une boule au lieu d'ouvrir une coquille.
  EffectTrack track = Semee();
  track.value[0] = 5.0F;
  const std::vector<EffectWorldQuad> quads = ComposeSphereShellQuads(track, {});
  const std::vector<EffectWorldQuad> repos = ComposeSphereShellQuads(Semee(), {});
  REQUIRE(quads.size() == repos.size());
  const std::size_t equateur = 1 * kSphereShellColumnCount * kSphereShellLayerCount;
  CHECK(HorizontalRadius(quads[equateur].corners[0]) ==
        Approx(HorizontalRadius(repos[equateur].corners[0])));
  CHECK(HorizontalRadius(quads[equateur + 2].corners[0]) >
        HorizontalRadius(repos[equateur + 2].corners[0]) + 4.0F);
}

TEST_CASE("les trois pistes eteintes n'avancent pas", "[render][effect][shell]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advanced(30);
  CHECK(tracks[0].ticks == 30);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].ticks == 0);
    CHECK(tracks[rang].alpha == 0);
  }
}

TEST_CASE("la table des coquilles est triee et filtree", "[render][effect][shell]") {
  const std::span<const EffectSphereShellPattern> table = EffectSphereShellPatternTable();
  REQUIRE(table.size() == 7);
  CHECK(std::ranges::is_sorted(table, {}, &EffectSphereShellPattern::effect_id));
  // ⚠ L'OCTET DU SITE N'EST PAS LA MARQUE : le bâtisseur ne laisse passer que
  // deux, trois, quatre et cinq. L'identifiant 336 passe zéro, et zéro reste ;
  // le bâtisseur du frère écrit un, sans filtre.
  for (const EffectSphereShellPattern& pattern : table) {
    CHECK(pattern.mark <= 5);
  }
  REQUIRE_FALSE(EffectSphereShellPatternsFor(336).empty());
  CHECK(EffectSphereShellPatternsFor(336).front().mark == 0);
  CHECK(EffectSphereShellPatternsFor(1600).front().mark == 5);
  CHECK(EffectSphereShellPatternsFor(1).empty());
}

TEST_CASE("le frere de Kyrie ne partage ni son image ni sa cle", "[render][effect][shell]") {
  // ⚠ DEUX BÂTISSEURS, ET C'EST TOUT CE QUI LES SÉPARE. Le frère pose la
  // même figure que celui de Kyrie — sous-type 61, échelle onze,
  // piste unique — mais sa seconde image est `a01.bmp` et sa clé de montée vaut
  // dix. Les tenir pour constantes de fichier, comme avant, donnait au bouclier
  // de Kyrie l'image de l'autre.
  REQUIRE_FALSE(EffectSphereShellPatternsFor(496).empty());
  const EffectSphereShellPattern& frere = EffectSphereShellPatternsFor(496).front();
  CHECK(frere.second_texture == "data\\texture\\effect\\a01.bmp");
  CHECK(frere.texture == frere.second_texture);
  CHECK(frere.lift_key == Approx(10.0F));
  // Son site ne lui passe aucun octet : son bâtisseur écrit la marque en dur, à un.
  CHECK(frere.mark == 1);

  const EffectSphereShellPattern& kyrie = EffectSphereShellPatternsFor(336).front();
  CHECK(kyrie.second_texture == kSphereShellSecondTexture);
  CHECK(kyrie.lift_key == Approx(kSphereShellLiftKey));
  CHECK(kyrie.second_texture != frere.second_texture);

  // ET LE SEMIS PORTE LA DIFFÉRENCE, sans quoi la table la porterait pour rien.
  const ro::render::PooledEffectNode node = SowEffectSphereShellNode(frere, {}, 60);
  REQUIRE(node.resource_paths.size() == 2);
  CHECK(node.resource_paths[1] == "data\\texture\\effect\\a01.bmp");
  CHECK(node.tracks[0].lift_key == Approx(10.0F));
}
