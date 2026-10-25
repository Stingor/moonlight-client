#include "effects/effect_paint.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "ro/render/effect_render_word.h"
#include "ro/render/ground_quad.h"
#include "ro/render/terrain_mesh.h"

namespace moonclient {

namespace {

// LA SEULE CARTE OÙ LE CLIENT DOUBLE LA BOÎTE DE DISPERSION, nommée en clair
// dans son code. Le nôtre est sans extension, le sien avec.
constexpr std::string_view kWideScatterMapName = "ecl_fild01";

// LES CASES D'UN LOT, POSEES SUR LE RELIEF : chaque cellule y prend ses quatre
// coins du fichier de carte, et garde la portion d'image que la figure lui a
// donnee.
//
// ⚠ C'EST ICI QUE LES DEUX MOITIES SE REJOIGNENT. La figure sait quelle portion
// d'image va sur quelle case et ne sait pas ou sont les cases ; la carte sait ou
// sont les cases et ignore tout de l'effet. Une case hors grille ne rend rien,
// et c'est la meme regle que pour le curseur au sol.
void PlaceGroundCells(const DrawnGroundCells& lot, const ro::formats::GndMap& gnd,
                      const ro::formats::GatMap& gat, std::vector<ro::render::GroundQuad>& quads) {
  const std::array<std::int32_t, 2> centre =
      ro::render::CellAtPosition(gnd, lot.ground_position.x, lot.ground_position.z);
  for (const ro::render::EffectGroundCellPaint& peinture : lot.cells) {
    std::optional<ro::render::GroundQuad> quad =
        ro::render::ComposeGroundCellQuad(gnd, gat, centre[0] + peinture.cell_offset_x,
                                          centre[1] + peinture.cell_offset_y, peinture.color);
    if (!quad.has_value()) {
      continue;
    }
    quad->texture_uv = peinture.texture_uv;
    quads.push_back(*quad);
  }
}

// Les SPRITES des nœuds du lot demandé : ceux qui passent sous les `.str`, ou
// les autres.
void PaintNodeSprites(EffectNodesInPlay& nodes, const EffectPainters& to, bool before_str) {
  for (const EffectNodesInPlay::SpriteNodeBatch& lot : nodes.SpriteBatches()) {
    if (lot.paints_before_str != before_str) {
      continue;
    }
    to.sprites->Draw(*to.camera, lot.actors, to.aspect_ratio,
                     lot.additive ? ro::render::SpriteRenderer::Blending::kAdditive
                                  : ro::render::SpriteRenderer::Blending::kOver,
                     ro::render::SpriteRenderer::DepthWrite::kOff,
                     lot.depth_tested ? ro::render::SpriteRenderer::DepthTest::kOn
                                      : ro::render::SpriteRenderer::DepthTest::kOff,
                     to.fog);
  }
}

// LES SPRITES DES CLASSES du lot demandé : ceux qui passent sous les `.str`, ou
// les autres. Ceux qui passent sous les corps se peignent ailleurs, avant eux.
void PaintClassSprites(EffectsInPlay& effects, ro::render::SpriteRenderer& sprites,
                       const ro::render::Camera& camera, float aspect_ratio,
                       const ro::render::SceneFog& fog, bool behind_actors, bool before_str) {
  for (const EffectClassesInPlay::SpriteBatch& lot : effects.Classes().SpriteBatches()) {
    if (lot.behind_actors != behind_actors || (!behind_actors && lot.before_str != before_str)) {
      continue;
    }
    sprites.Draw(camera, lot.actors, aspect_ratio,
                 lot.additive ? ro::render::SpriteRenderer::Blending::kAdditive
                              : ro::render::SpriteRenderer::Blending::kOver,
                 ro::render::SpriteRenderer::DepthWrite::kOff,
                 lot.depth_tested ? ro::render::SpriteRenderer::DepthTest::kOn
                                  : ro::render::SpriteRenderer::DepthTest::kOff,
                 fog);
  }
}

// Les QUADS DU MONDE du lot demandé.
void PaintWorldQuads(std::span<const ro::render::DrawnWorldQuads> lots, const EffectPainters& to,
                     bool before_str) {
  std::vector<ro::render::DrawnWorldQuads> kept;
  kept.reserve(lots.size());
  for (const ro::render::DrawnWorldQuads& lot : lots) {
    if (ro::render::EffectLotPaintsBeforeStr(lot.render_word) == before_str) {
      kept.push_back(lot);
    }
  }
  if (!kept.empty()) {
    to.effects->DrawWorldQuads(*to.camera, kept, to.aspect_ratio, to.fog);
  }
}

}  // namespace

ro::render::EffectScatterWorld EffectScatterWorldOf(const MapStage& stage) {
  // ⚠ LA TROUPE PARLE EN COORDONNÉES DU MONDE, la carte en CELLULES, et la
  // conversion est ici : c'est le seul endroit qui tienne les deux.
  // `GroundPositionAt` rend le sol de la grille de MARCHABILITÉ — celui où un
  // acteur se tient, donc celui où une pièce rebondit, et non le décor visible.
  ro::render::EffectScatterWorld node_world;
  node_world.ground_height = [&stage](float world_x, float world_z) {
    const std::array<float, 2> cellule =
        ro::render::FractionalCellAt(stage.scene.gnd, world_x, world_z);
    return GroundPositionAt(stage, cellule[0], cellule[1])[1];
  };
  // ET LA CARTE QUI ÉLARGIT LES DISPERSIONS, que le client nomme en clair : il
  // compare le nom courant à « ecl_fild01.rsw » et double alors la boîte où ses
  // particules tombent. Le nôtre est sans extension, le sien avec.
  node_world.wide_scatter = stage.name == kWideScatterMapName;
  // LA MÊME CARTE, ET LE MÊME TEST, plafonnent l'alpha du carré teinté.
  node_world.caps_tinted_quad_alpha = stage.name == kWideScatterMapName;
  return node_world;
}

// 🔴 CE QUI SE PEINT SOUS LES FICHIERS `.str` D'ABORD. Le client vide le seau
// des fichiers après tous ses seaux triés, addition comprise : un nœud au mot
// du constructeur passe DESSOUS l'image d'un `.str`, quel que soit l'ordre des
// naissances. Peindre tous les fichiers avant tous les nœuds couvrait l'éclat
// de feu de Fire Bolt de la flèche et de l'anneau qui l'ont apporté. Voir
// `ro::render::EffectRenderWordPaintsBeforeStr`.
void PaintEffectsUnderStr(EffectsInPlay& effects, EffectNodesInPlay& nodes,
                          const EffectPainters& to) {
  PaintWorldQuads(effects.LaidOnGround(), to, true);
  PaintNodeSprites(nodes, to, true);
  PaintClassSprites(effects, *to.sprites, *to.camera, to.aspect_ratio, to.fog, false, true);
  PaintWorldQuads(nodes.WorldQuads(), to, true);
}

void PaintClassSpritesBehindActors(EffectsInPlay& effects, ro::render::SpriteRenderer& sprites,
                                   const ro::render::Camera& camera, float aspect_ratio,
                                   const ro::render::SceneFog& fog) {
  PaintClassSprites(effects, sprites, camera, aspect_ratio, fog, true, true);
}

void PaintStrEffects(EffectsInPlay& effects, EffectNodesInPlay& nodes,
                     std::span<const ro::render::DrawnEffect> played, const EffectPainters& to) {
  if (!played.empty()) {
    to.effects->Draw(*to.camera, played, to.aspect_ratio);
  }
  // ET CE QU'UNE MÉTHODE NOMMÉE A COUCHÉ AU SOL, en quads du monde.
  PaintWorldQuads(effects.LaidOnGround(), to, false);
  PaintNodeSprites(nodes, to, false);
  // ET LES SPRITES DES CLASSES D'EFFET que leur mot range après les `.str`,
  // composés au même battement qu'eux.
  PaintClassSprites(effects, *to.sprites, *to.camera, to.aspect_ratio, to.fog, false, false);
}

void PaintEffectsLast(EffectsInPlay& effects, EffectNodesInPlay& nodes, const EffectPainters& to) {
  PaintWorldQuads(nodes.WorldQuads(), to, false);
  // LES CASES DE TERRAIN, juste apres les quads du monde : ce sont des marques
  // de SOL, et elles se peignent comme le curseur au sol — en epousant le
  // relief, et en passant devant lui d'un biais de profondeur.
  //
  // ⚠ SANS LA GRILLE DE FRANCHISSEMENT, RIEN NE SE PEINT. Les quatre hauteurs
  // d'une cellule viennent du .gat et de nulle part ailleurs ; une carte lue
  // sans lui garde ses effets de sol muets, comme elle garde son curseur.
  if (const std::span<const DrawnGroundCells> cases = nodes.GroundCells();
      !cases.empty() && to.ground_quads != nullptr && to.stage != nullptr &&
      to.stage->gat.has_value()) {
    std::vector<ro::render::GroundQuad> quads;
    std::vector<ro::render::DrawnGroundQuads> lots;
    std::vector<std::size_t> premiers;
    lots.reserve(cases.size());
    premiers.reserve(cases.size());
    for (const DrawnGroundCells& lot : cases) {
      premiers.push_back(quads.size());
      PlaceGroundCells(lot, to.stage->scene.gnd, *to.stage->gat, quads);
      lots.push_back(ro::render::DrawnGroundQuads{{}, lot.texture_path, lot.texture});
    }
    // Les vues se prennent APRES que le tableau a fini de grandir : les prendre
    // au fil des ajouts les laisserait pendre a la premiere reallocation.
    for (std::size_t rang = 0; rang < lots.size(); ++rang) {
      const std::size_t fin = rang + 1 < lots.size() ? premiers[rang + 1] : quads.size();
      lots[rang].quads = std::span(quads).subspan(premiers[rang], fin - premiers[rang]);
    }
    to.ground_quads->Draw(*to.camera, lots, to.aspect_ratio, to.fog);
  }
  // LES FICHIERS QUI SE PEIGNENT PAR-DESSUS TOUT, après tout ce qui est du
  // monde : le client vide leurs seaux les derniers. Voir
  // `ro::render::StrPaintsOverEverything`.
  if (const std::span<const ro::render::DrawnEffect> dessus = effects.DrawnOverEverything();
      !dessus.empty()) {
    to.effects->Draw(*to.camera, dessus, to.aspect_ratio);
  }
  // ⚠ ET LA QUATRIEME VOIE EN DERNIER, parce qu'elle ne se projette pas : ses
  // coins sont deja des PIXELS, et rien de ce qu'elle peint n'appartient au
  // monde. Voir l'ADR 0022.
  if (const std::span<const ro::render::DrawnScreenQuads> screen =
          nodes.ScreenQuads(*to.camera, to.viewport_width, to.viewport_height);
      !screen.empty()) {
    to.effects->DrawScreenQuads(screen, to.viewport_width, to.viewport_height);
  }
  // Et ce que les CLASSES peignent en plein écran — le givre d'une gelure —,
  // par la même voie, après tout le reste.
  if (const std::span<const ro::render::DrawnScreenQuads> screen = effects.Classes().ScreenQuads();
      !screen.empty()) {
    to.effects->DrawScreenQuads(screen, to.viewport_width, to.viewport_height);
  }
}

void GatherEffectLights(const EffectsInPlay& effects, const EffectNodesInPlay& nodes,
                        const ro::render::Camera& camera, ro::render::TextureGlowCache& glows,
                        std::vector<ro::render::PointLight>& out) {
  out.clear();
  const ro::render::BillboardBasis basis = ro::render::MakeBillboardBasis(camera);
  for (const std::span<const ro::render::DrawnEffect> files :
       {effects.DrawnBehindActors(), effects.DrawnAmongActors(), effects.DrawnOverEverything()}) {
    ro::render::EffectLightsOf(files, {}, basis, glows, out);
  }
  ro::render::EffectLightsOf({}, effects.LaidOnGround(), basis, glows, out);
  ro::render::EffectLightsOf({}, nodes.WorldQuads(), basis, glows, out);
}

}  // namespace moonclient
