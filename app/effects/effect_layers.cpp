#include "effects/effect_layers.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/ground_quad.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/terrain_mesh.h"
#include "ro/sim/movement.h"

#include "effects/effect_requests.h"
#include "world/actor_view.h"

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

// ⚠ L'ORDRE DES COUCHES EST UN CHOIX PAR PRIMITIVE CHEZ LE CLIENT, PAS UN ORDRE
// GLOBAL — et c'est pourquoi cette fonction ne peut pas encore le rendre.
//
// Le client range chaque primitive dans un SEAU selon le mot qu'on lui passe,
// puis vide ces seaux dans un ordre fixe, chacun sous ses états. Pour les nœuds
// d'effet, chacun porte le sien : voir `ro/render/effect_render_word.h`, qui en
// porte le mélange et l'épreuve de profondeur. L'ordre des seaux ENTRE acteurs
// et effets, lui, n'est pas porté.
//
// Observé en jeu côte à côte : le natif peint son sprite APRÈS l'effet de Heal —
// le soigné passe devant ses propres étincelles. Mais cela ne vaut PAS pour tous
// les effets, et inverser notre ordre global l'aurait fait croire.
//
// Tant que ces drapeaux ne sont pas portés, nous gardons l'ordre unique
// ci-dessous — les effets par-dessus les acteurs —, et Heal en pâtit seul.
void DrawEffectLayers(EffectsInPlay& effects, EffectNodesInPlay& nodes, ActorSounds& sounds,
                      ActorFootprints& footprints, ItemDrops& drops, const SoundStage& scene,
                      ro::render::SpriteRenderer& sprites,
                      ro::render::EffectRenderer& effect_renderer,
                      ro::render::GroundQuadRenderer& ground_quads, float aspect_ratio,
                      float viewport_width, float viewport_height,
                      const ro::render::SceneFog& fog) {
  // 🔴 LA TROUPE APPREND SA CARTE AVANT DE BATTRE, et c'est ce qui manquait : un
  // nœud qui rebondit cherche le sol à chaque tic, et sans réponse il le
  // traverse — les pièces d'`EF_COIN` ne sonnaient pas pour cette seule raison.
  //
  // ⚠ LA TROUPE PARLE EN COORDONNÉES DU MONDE, la carte en CELLULES, et la
  // conversion est ici : c'est le seul endroit qui tienne les deux.
  // `GroundPositionAt` rend le sol de la grille de MARCHABILITÉ — celui où un
  // acteur se tient, donc celui où une pièce rebondit, et non le décor visible.
  ro::render::EffectScatterWorld scatter_world = EffectScatterWorldOf(*scene.stage);
  // LE CORPS D'UN PORTEUR, que la marque de la brûlure lit pour se poser
  // au-dessus de lui. ⚠ DES POINTEURS ET NON LA SCÈNE : la troupe garde cette
  // fonction jusqu'à l'image suivante, et la scène meurt avec celle-ci.
  if (scene.wardrobe != nullptr && scene.world != nullptr) {
    ro::render::ActorWardrobe* wardrobe = scene.wardrobe;
    const ro::sim::World* world = scene.world;
    scatter_world.body_first_layer_y = [wardrobe,
                                        world](std::uint32_t actor_id) -> std::optional<float> {
      const ro::sim::Actor* actor = world->Find(actor_id);
      if (actor == nullptr) {
        return std::nullopt;
      }
      const ro::render::ActorOutfit* outfit = wardrobe->OutfitFor(LookOf(*actor));
      return outfit != nullptr ? outfit->BodyFirstLayerY() : std::nullopt;
    };
  }
  nodes.NoticeWorld(std::move(scatter_world));
  // LA VUE ET LES ACTEURS, pour les semis nommés : un angle à l'écran, une
  // direction de sprite, une allure se lisent ici et nulle part dans les
  // effets.
  EffectsInPlay::NamedActorView actor_view;
  if (scene.wardrobe != nullptr) {
    ro::render::ActorWardrobe* garde_robe = scene.wardrobe;
    actor_view.pace_of = [garde_robe, &scene](const ro::sim::Actor& actor, std::uint16_t skill_id,
                                              std::optional<std::uint32_t> attack_motion_ms) {
      return NamedPaceOf(*garde_robe, *scene.world, actor, scene.planner->PathFor(actor), skill_id,
                         attack_motion_ms, scene.now_ms);
    };
    // BRONCHER, c'est jouer la blessure à cet instant : la posture le décide,
    // avec ses gardes — un joueur qui frappe, une pose menée.
    actor_view.flinches_at = [garde_robe, &scene](const ro::sim::Actor& actor,
                                                  std::uint32_t at_ms) {
      ro::render::ActorOutfit* outfit = garde_robe->OutfitFor(LookOf(actor));
      if (outfit == nullptr) {
        return false;
      }
      std::vector<ro::render::ActorEffectPlay> joues;
      PlayedEffectsOf(actor, at_ms, joues);
      return MotionAt(garde_robe, *scene.world, actor, *outfit, scene.planner->PathFor(actor),
                      at_ms, 0.0F, joues)
                 .motion == ro::render::Motion::kHurt;
    };
  }
  actor_view.sprite_direction_of = [&scene](const ro::sim::Actor& actor,
                                            std::uint32_t at_ms) -> std::optional<std::size_t> {
    std::vector<ro::render::ActorEffectPlay> joues;
    PlayedEffectsOf(actor, at_ms, joues);
    return ro::render::SpriteDirection(
        ro::sim::FacingAt(actor, scene.planner->PathFor(actor), at_ms),
        scene.camera->yaw_degrees - ro::render::SpinDegreesAfterEffects(joues));
  };
  effects.SeeScene(*scene.camera, std::move(actor_view));
  effects.Classes().SeeScene(*scene.camera, viewport_width, viewport_height,
                             effects.MinimalEffects());
  effects.NoticeEffectRemovals(*scene.world);
  effects.Notice(*scene.world, *scene.stage, *scene.planner);
  effects.NoticeSkills(*scene.world, *scene.stage, *scene.planner, scene.now_ms);
  // Et l'IMPACT de chaque frappe sur ce qu'elle touche, qui ne part pas au
  // geste mais avec le chiffre des dégâts : le client en pose un par message.
  effects.NoticeHitEffects(scene);
  // Et les FLÈCHES des arcs, qu'aucun paquet n'annonce : c'est l'attaque
  // elle-même qui les fait partir, à l'instant du geste.
  effects.NoticeArrows(scene);
  // Les états qui s'entretiennent eux-mêmes, Sight en tête : rien ne les
  // annonce, et ils ne se voient que si on les redemande.
  effects.NoticeOptionEffects(*scene.world, *scene.stage, *scene.planner, scene.now_ms);
  // Les TRAITS DE DEVOTION, qu'une liste de protégés retend : après le suivi des
  // porteurs, qui vient d'éteindre ceux dont la cible est partie.
  effects.NoticeDevotionLinks(*scene.world, *scene.stage, *scene.planner, scene.now_ms);
  // Ce qu'un changement d'état montre UNE fois : Hiding, le silence.
  effects.NoticeStateChanges(*scene.world, *scene.stage, *scene.planner, scene.now_ms);
  // Et les états NOMMÉS, qui ne s'entretiennent PAS : ils jouent aux deux
  // instants où l'état change, et c'est la donnée qui les fait durer ensuite.
  // Trente-huit états de quatrième classe n'ont que ce chemin-là.
  effects.NoticeStateEffects(*scene.world, *scene.stage, *scene.planner, scene.now_ms);
  effects.NoticeHatEffects(*scene.world, *scene.stage, *scene.planner, scene.now_ms);
  // Et les UNITÉS posées, qui s'entretiennent de la même façon : un mur de
  // feu ne se voit pas parce qu'un paquet le redit, mais parce que le
  // client le redessine tant qu'il occupe sa case.
  effects.NoticeSkillUnitVisuals(*scene.world, *scene.stage, scene.now_ms);
  // LES POINTS DE TÉLÉPORTATION : ils ne s'annoncent pas davantage, et leur
  // effet se pose une seule fois par acteur.
  effects.NoticeWarpActors(*scene.world, *scene.stage, scene.now_ms);
  // Et les PAS d'un coureur, qui se posent la ou il etait : la course ne
  // dessine rien elle-meme, elle seme une empreinte tous les sept battements.
  footprints.Notice(*scene.world, *scene.stage, *scene.planner, effects, scene.now_ms);
  // Et le PILIER DE LUMIÈRE d'un butin que le serveur veut voir de loin : il se
  // plante à la naissance de l'objet, sur la case où il se posera. Son bruit
  // part avec lui, par la même porte que celui des effets nommés — c'est un
  // fichier .wav écrit en dur dans la classe du client, et non une entrée d'une
  // table indexée par identifiant.
  const std::vector<SownNamedSound> pillar_sounds =
      drops.NoticePillars(*scene.world, *scene.stage, effects, scene.now_ms);
  sounds.HearNamedEffects(scene, pillar_sounds);
  // Le BRUIT des unités part avec leur image, et non à part : le son de
  // pose une fois, celui de l'entretien à chaque semis. Seul le semeur sait
  // quand il a semé, et il ne connaît pas l'audio — c'est ici que les deux
  // se rejoignent.
  sounds.HearSkillUnits(scene, effects.JustSownUnits());
  // Et le bruit des effets lancés PAR LEUR IDENTIFIANT — un paquet d'effet nu,
  // une ligne du banc d'essai. La liste s'emporte, si bien qu'un effet ne sonne
  // qu'une fois quel que soit le nombre d'images qu'il dure.
  const std::vector<LaunchedEffect> launched = effects.TakeLaunchedEffects();
  sounds.HearEffects(scene, launched);
  // Et celui des effets NOMMÉS, dont le .wav est dans la fiche et non dans une
  // table indexée par identifiant : ces huit unités-là n'ont de bruit nulle
  // part ailleurs.
  const std::vector<SownNamedSound> named_sounds = effects.TakeSownNamedSounds();
  sounds.HearNamedEffects(scene, named_sounds);
  const std::span<const ro::render::DrawnEffect> played =
      effects.Drawn(scene.now_ms, *scene.stage, *scene.world, *scene.planner);
  // La troupe des nœuds procéduraux doit BATTRE à chaque image : pas de
  // garde, sans quoi elle s'arrêterait dès qu'il n'y a rien à peindre — et
  // elle ne repartirait jamais.
  //
  // ⚠ CET APPEL COMPOSE LES DEUX FORMES. Les sprites en sortent
  // directement ; les ANNEAUX, qui sont de la géométrie du monde et non des
  // panneaux, se reprennent à part et vont au renderer d'effets.
  //
  // ⚠ PAR LOTS, que le MOT DE RENDU de chaque nœud décide : le client peint un
  // sprite en addition pour le seul mot cinq, à l'ordinaire sinon, et lève
  // par-dessus tout ceux que 0x200 marque, sans épreuve de profondeur. La
  // décomposition suit le client jusqu'au vidage des seaux — voir
  // `SpriteBatches` et la composition, `EffectNodesInPlay::Drawn`. Un nœud sans
  // mot s'ajoute et passe l'épreuve.
  static_cast<void>(nodes.Drawn(scene.now_ms, *scene.camera));
  const EffectPainters painters{.sprites = &sprites,
                                .effects = &effect_renderer,
                                .ground_quads = &ground_quads,
                                .stage = scene.stage,
                                .camera = scene.camera,
                                .aspect_ratio = aspect_ratio,
                                .viewport_width = viewport_width,
                                .viewport_height = viewport_height,
                                .fog = fog};
  PaintEffectsUnderStr(effects, nodes, painters);
  PaintStrEffects(effects, nodes, played, painters);
  // ET LES SONS DE LA TROUPE, juste après le battement qui les a fait avoir :
  // les pièces d'`EF_COIN` sonnent une à une, là où chacune retombe, et chaque
  // boule de Soul Strike à son semis. Voir `ActorSounds::HearNodeSounds`.
  sounds.HearNodeSounds(scene, nodes.TakeSounds());
  // ET LES EFFETS QUE LES NŒUDS VIENNENT DE DEMANDER en se peignant, comme ce
  // qu'une classe demande sur son porteur en s'en allant : ils naissent ici, et
  // se montreront à l'image suivante. Ceux qui changent un acteur partent avec
  // les effets d'acteur des suites — voir `EffectsInPlay::TakeActorEffects`.
  effects.SowRequestedEffects(*scene.world, *scene.stage, *scene.planner, scene.now_ms);
  PaintEffectsLast(effects, nodes, painters);
}

}  // namespace moonclient
