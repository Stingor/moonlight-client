// LE DESSIN DU MONDE : le terrain, le décor, les acteurs, les effets, puis ce
// qui se peint en coordonnées d'écran.
//
// ⚠ C'EST L'ORDRE QUI FAIT L'IMAGE, et c'est pourquoi cette étape est seule dans
// son fichier : chaque couche masque, éclaircit ou découpe la précédente, et
// presque chaque ligne porte la raison d'être à sa place plutôt qu'ailleurs.

#include <optional>
#include <span>
#include <utility>

#include <SDL3/SDL.h>

#include "ro/net/actor_name.h"
#include "ro/net/movement.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/ground_cursor.h"
#include "ro/render/ground_pick.h"
#include "ro/render/skill_area.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/actor.h"
#include "ro/sim/damage_meter.h"
#include "ro/sim/ground_skill_log.h"
#include "ro/sim/skill_book.h"

#include "effects/effect_layers.h"
#include "game/game.h"
#include "game/setting_destination_open.h"
#include "world/armed_cursor.h"
#include "world/click_action.h"
#include "world/entity_menu.h"
#include "world/ground_unit_view.h"
#include "world/pick_box_view.h"
#include "world/scene_graphics.h"
#include "world/screen_overlays.h"

namespace moonclient {

std::optional<ro::sim::CellPosition> Game::HoveredCell() const {
  return GroundCellAt((*stage_), camera_, aspect_ratio_, mouse_.x, mouse_.y,
                      static_cast<float>(shell_.width()), static_cast<float>(shell_.height()));
}

bool Game::CameraTurning() const {
  // Seule la rotation change le curseur ; incliner ou reculer gardent le survol.
  float x = 0.0F;
  float y = 0.0F;
  const SDL_MouseButtonFlags held = SDL_GetMouseState(&x, &y);
  const SDL_Keymod mods = SDL_GetModState();
  const auto turns = [&](MouseGesture gesture) {
    const std::optional<ActionRef> action = settings_->bindings().ActionForGesture(
        gesture, (mods & SDL_KMOD_CTRL) != 0, (mods & SDL_KMOD_SHIFT) != 0,
        (mods & SDL_KMOD_ALT) != 0);
    return action == ActionRef(GameAction::kCameraOrbit) ||
           action == ActionRef(GameAction::kCameraOrbitZoom);
  };
  return ((held & SDL_BUTTON_RMASK) != 0 && turns(MouseGesture::kRightDrag)) ||
         ((held & SDL_BUTTON_MMASK) != 0 && turns(MouseGesture::kMiddleDrag));
}

ro::core::Result<void> Game::DrawWorld(std::uint32_t now_ms) {
  // ⚠ LA TAILLE DE LA FENÊTRE SE LIT, ELLE NE SE MESURE PAS ICI : la coquille
  // la relève une fois par image, en tête de boucle. Voir `ClientShell::width`
  // — la mesurer dans le dessin du MONDE la laisserait périmée pour tout écran
  // qui n'en dessine aucun.

  // LA CASE QUE LA SOURIS DÉSIGNE, cherchée UNE FOIS pour toute l'image. Trois
  // choses en dépendent — le cadre posé au sol, la marche tenue, la forme du
  // curseur —, et la chercher coûte une marche du rayon le long du terrain.
  const std::optional<ro::sim::CellPosition> hovered_cell = HoveredCell();
  const ro::ui::PickTuning pick_tuning = ro::ui::PickTuningOf(
      windows_.staff_tools.look.picking, Rights().staff_tools, static_cast<float>(shell_.width()),
      settings_->options().pages().pick_precision.floor_shift());

  // LE MONDE, DANS SA CIBLE À LUI quand la page « Graphics » en demande une —
  // MSAA, échelle de rendu, post-traitement ; droit dans la fenêtre sinon.
  const ro::render::PostFxSettings post_fx = settings_->options().graphics().post_fx();
  const ro::render::DrawableSize surface = gl_->drawable_size();
  if (!scene_post_.Begin(surface.width, surface.height,
                         settings_->options().graphics().scene_target(), post_fx.enabled)) {
    gl_->BindForDisplay();
  }
  // 🔴 ET REPEINT DANS LA FENÊTRE À CHAQUE SORTIE, erreur comprise : ce qui se
  // peint ensuite — nombres, noms, jauges, interface — va droit à l'écran, et
  // tomberait sinon dans une cible que personne ne montre.
  const auto present_scene = [this, &post_fx, now_ms]() {
    gl_->BindForDisplay();
    scene_post_.Present(post_fx, now_ms);
  };
  // LA COUCHE AU-DESSUS DU MONDE s'ouvre ici, avant toute sortie anticipée :
  // son HUD de l'image précédente ne doit pas survivre à une image sans lui.
  pieces_.over_world.Prepare({.effects = &effect_renderer_,
                              .overlay = &pieces_.overlay,
                              .nodes = effect_nodes_.has_value() ? &*effect_nodes_ : nullptr,
                              .camera = &camera_,
                              .scene_width = static_cast<float>(size_.width),
                              .scene_height = static_cast<float>(size_.height),
                              .hud_width = static_cast<float>(shell_.width()),
                              .hud_height = static_cast<float>(shell_.height())});
  // La musique suit la carte. Le contrôle est fait à chaque image et ne coûte
  // qu'une comparaison de chaînes : c'est le seul endroit où l'on sait à coup
  // sûr sur quelle carte on se trouve.
  //
  // Muette, la musique ne se cherche même pas : le fichier vit hors de
  // l'archive, et le lire pour ne rien en faire coûterait un accès disque à
  // chaque changement de carte.
  if (audio_.music_audible()) {
    music_.Follow(audio_, stage_->name);
  }
  // LE LECTEUR DE MUSIQUE, même muet : un choix fait son coupé se retient.
  pieces_.music_player.Advance(windows_.music_player, music_, audio_, stage_->name);

  // LE SOL UNI, quand l'établi ou GreyWorld le demandent : voir `FlatGroundOf`.
  const ro::render::GreyWorldLook& grey_world = settings_->options().pages().grey_world.look();
  ro::render::TerrainStages stages = TerrainStagesOf(
      settings_->options().graphics(), ro::render::GreyWorldHidesLightmap(grey_world));
  stages.flat_color =
      FlatGroundOf(windows_.staff_tools.look.flat_ground, Rights().staff_tools, grey_world);
  // LE BROUILLARD, pour tout ce que le client peint dessous — voir `SceneFog`.
  const bool fog_shown = settings_->options().graphics().fog();
  const ro::render::SceneFog fog = FogOf(*stage_, fog_shown);
  terrain_.Draw(camera_, pieces_.night_light.light(), aspect_ratio_,
                ClearColorOf(*stage_, background_, fog_shown), stages, fog);
  // LES MODÈLES ANIMÉS partent de l'image zéro à la première image de la carte,
  // comme chez le natif qui les pose à leur chargement.
  if (!model_clock_.started()) {
    model_clock_.Start(stage_->scene.models, now_ms);
  }
  model_clock_.Advance(stage_->scene.models, now_ms);
  models_.SetTextureFilter(settings_->options().graphics().decor_filter());
  ground_unit_models_.SetTextureFilter(settings_->options().graphics().decor_filter());
  // LES FACES À TEXTURE MÊLÉE attendent la passe translucide, plus bas.
  const bool models_shown = !ro::render::GreyWorldHidesModels(grey_world);
  if (models_shown) {
    models_.Draw(camera_, pieces_.night_light.light(), aspect_ratio_,
                 ro::render::ModelStages{.blended_apart = true}, &model_clock_, fog,
                 pieces_.decor_veil.opacities());
  }
  // Les unités posées au sol : ce sont des modèles, et elles ne se téléversent
  // que lorsque l'ensemble posé a bougé. Leur horloge à elles les tient figées
  // jusqu'à ce qu'elles se déclenchent. Elles se PEIGNENT plus bas, sur le
  // cadre de la case.
  if (const std::vector<ro::render::ModelInstances>* placed =
          ground_units_->Changed((*stage_), world_);
      placed != nullptr) {
    if (auto uploaded = ground_unit_models_.Upload(*placed); !uploaded) {
      present_scene();
      return std::unexpected(uploaded.error());
    }
  }

  // LA CASE QUE LE CADRE MARQUE : figée sous l'emprise d'une unité au sol.
  const bool over_unit =
      !interface_->gate().mouse_to_ui &&
      PickGroundUnit(camera_, aspect_ratio_, ground_units_->picks(), mouse_.x, mouse_.y,
                     pick_tuning.ground_floor_pixels, static_cast<float>(shell_.width()),
                     static_cast<float>(shell_.height()))
          .has_value();
  const std::optional<ro::sim::CellPosition> marked_cell =
      CellCursorOf(hovered_cell, over_unit, cell_cursor_);

  // CE QUI SE PEINT SUR LE SOL, dans l'ordre de `kGroundPaintOrder`. L'eau n'en
  // est pas : elle passe après les corps, qu'elle plonge — voir plus bas.
  for (const GroundPaint pass : kGroundPaintOrder) {
    switch (pass) {
      case GroundPaint::kGreyWorld:
        pieces_.effect_ground.Draw(
            camera_, pieces_.grey_world.Cells(*vfs_, grey_world, *stage_, world_, planner_, now_ms),
            aspect_ratio_, fog);
        break;
      case GroundPaint::kGraffiti:
        if (graffiti_.has_value()) {
          const auto scribbles = graffiti_->Compose(
              world_, *stage_, guild_emblems_.has_value() ? &guild_emblems_->plates() : nullptr,
              now_ms);
          if (!scribbles.empty())
            pieces_.effect_ground.Draw(camera_, scribbles, aspect_ratio_, fog);
        }
        break;
      case GroundPaint::kSkillAreas:
        // LA ZONE DES SORTS EN COURS D'INCANTATION, à la même couche que le
        // curseur au sol et pour la même raison : elle marque le TERRAIN, et qui
        // se tient dessus doit la couvrir. ⚠ AVANT le curseur, pour qu'une case
        // désignée reste lisible par-dessus une zone.
        if (skill_areas_.has_value()) {
          const std::span<const ro::render::DrawnGroundQuads> zones =
              skill_areas_->Areas(settings_->options().skill_area(), world_,
                                  pieces_.skill_tree.has_value() ? &*pieces_.skill_tree : nullptr,
                                  *stage_, now_ms, grimoire_.armed(), hovered_cell);
          if (!zones.empty()) {
            pieces_.effect_ground.Draw(camera_, zones, aspect_ratio_, fog);
          }
        }
        break;
      case GroundPaint::kNaviTrail:
        // Par le renderer des lots : chaque direction de flèche apporte son image.
        if (stage_->gat.has_value() && navi_.guide.active() && navi_.prefs.ground_trail) {
          const std::span<const ro::render::DrawnGroundQuads> trail =
              navi_.trail_view.Compose(*vfs_, stage_->scene.gnd, *stage_->gat,
                                       navi_.guide.path_cells(), navi_.prefs.trail_icon, now_ms);
          if (!trail.empty()) pieces_.effect_ground.Draw(camera_, trail, aspect_ratio_, fog);
        }
        break;
      case GroundPaint::kCellCursor:
        // Le cadre sur la case désignée : il couvre le sol, et ce qui se tient
        // dessus le couvre à son tour — la planche d'un piège comme un
        // personnage.
        if (stage_->gat.has_value() && marked_cell.has_value() && pieces_.ground_cursor.ready()) {
          if (const std::optional<ro::render::GroundQuad> marked = ro::render::ComposeGroundCursor(
                  stage_->scene.gnd, *stage_->gat, static_cast<std::int32_t>(marked_cell->x),
                  static_cast<std::int32_t>(marked_cell->y));
              marked.has_value()) {
            pieces_.ground_cursor.Draw(camera_, std::span(&*marked, 1), aspect_ratio_, fog);
          }
        }
        break;
      case GroundPaint::kGroundUnits: {
        // LEUR LUMIÈRE EST FIXE, MAIS LA NUIT LES TEINTE : ce sont des modèles
        // comme le décor.
        ro::render::SceneLight unit_light = ro::render::kGroundUnitModelLight;
        unit_light.tint = pieces_.night_light.light().tint;
        ground_unit_models_.Draw(camera_, unit_light, aspect_ratio_, ro::render::ModelStages(),
                                 &ground_units_->Advance(world_, now_ms), fog);
        break;
      }
    }
  }

  // RIEN À ANIMER : sans garde-robe — les tables du client ne se lisent pas —,
  // aucun acteur, aucun effet, aucune couche d'écran.
  //
  // 🔴 MAIS LE CURSEUR SE COMPOSE QUAND MÊME, et c'est tout l'objet de cette
  // sortie nommée : `LoadCursor` cache celui du système sans condition, si bien
  // que ce retour laisserait sinon l'écran SANS AUCUN pointeur.
  if (!pieces_.wardrobe.has_value()) {
    water_.Draw(camera_, aspect_ratio_, now_ms, fog);
    DrawMapEffects();
    if (models_shown) {
      pieces_.decor_veil.Draw(models_, camera_, *stage_, pieces_.night_light.light(), aspect_ratio_,
                              &model_clock_, fog);
    }
    present_scene();
    ComposeCursor(nullptr, hovered_cell, now_ms);
    return {};
  }

  const auto elapsed_ms = static_cast<float>(SDL_GetTicks() - start_ticks_);
  scene_actors_.Clear();
  scene_actors_.ground_units = ground_units_->picks();
  if (link_.is_open()) {
    // LA TEINTE QUI RESTE se relit avant la scène qui la montre.
    pieces_.ailment_tints.Observe(world_, now_ms);
    TimeComposedScene(settings_->options().graphics(), scene_actors_);
    SceneCensus census =
        ComposeScene(pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr, world_, planner_,
                     (*stage_), camera_.yaw_degrees, now_ms, elapsed_ms, &styles_, scene_actors_,
                     ghosts_, drops_, pieces_.frozen_poses,
                     ActorLightingOf(settings_->options().graphics(), &stage_->ground_light,
                                     &pieces_.ailment_tints));
    // LES ACTEURS EN MODÈLE rejoignent la scène APRÈS les sprites : ils n'y ont
    // qu'une emprise, et leurs triangles se peignent à part, plus bas.
    if (pieces_.actor_models.has_value() && pieces_.wardrobe.has_value()) {
      pieces_.actor_models->Compose(
          ActorModels::Frame{&pieces_.wardrobe->catalog(), &*pieces_.wardrobe, &world_, &planner_,
                             &*stage_, &camera_, &guild_book_,
                             guild_emblems_.has_value() ? &guild_emblems_->plates() : nullptr,
                             now_ms},
          scene_actors_, census);
      if (effects_.has_value()) {
        for (const ro::render::ActorModelMeshEvent& event : pieces_.actor_models->mesh_events()) {
          static_cast<void>(effects_->PlayById(event.effect_id, event.position, now_ms));
        }
      }
    }
    // ⚠ LE BALAYAGE VIENT APRÈS LA COMPOSITION, jamais pendant : oublier un
    // style détruit les tenues qui le portaient, et le parcours les lirait
    // mortes.
    styles_.Sweep(*pieces_.wardrobe, world_.styles());
    ReportCensus(census, reported_census_);
    // LA MARCHE QU'UN GESTE RETENAIT part dès que la motion a changé, d'après ce
    // que cette image vient de composer : voir `DeferredWalk`.
    static_cast<void>(ReleaseDeferredWalk(link_, world_, grimoire_.approach_clock(),
                                          world_mouse_.deferred_walk, scene_actors_.player_gesture,
                                          now_ms));
    ReportMissingSprites(pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr,
                         reported_missing_);
    NoteScreenTargets();
  }

  ApplyPickTuning(pick_tuning, scene_actors_.drawn, scene_actors_.ids, world_);
  scene_actors_.ground_unit_floor_pixels = pick_tuning.ground_floor_pixels;

  // APRÈS le terrain et le décor : le mélange alpha n'est correct que si ce qui
  // se trouve derrière est déjà peint.
  ghosts_.Drawn(now_ms, scene_actors_.behind);
  // ⚠ SEULS LES CORPS ÉCRIVENT LEUR PROFONDEUR. Sans cela rien ne les protège de
  // ce qui se peint après : les vingt-cinq pointes de terre recouvraient
  // entièrement le personnage, là où le client le montre ENTRE elles. Halos et
  // voiles, eux, sont des copies du même sprite : les laisser écrire découperait
  // leur porteur.
  using Silhouette = ro::render::SpriteRenderer::BodySilhouette;
  const auto paint = [this, &fog](std::span<const ro::render::DrawnActor> actors_to_draw,
                                  ro::render::SpriteRenderer::DepthWrite depth,
                                  ro::render::SpriteRenderer::BodySilhouette silhouette =
                                      ro::render::SpriteRenderer::BodySilhouette::kIgnored) {
    if (!actors_to_draw.empty()) {
      sprites_.Draw(camera_, actors_to_draw, aspect_ratio_,
                    ro::render::SpriteRenderer::Blending::kOver, depth,
                    ro::render::SpriteRenderer::DepthTest::kOn, fog,
                    ro::render::SpriteRenderer::DrawOrder::kFarthestFirst, silhouette);
    }
  };
  // LES EFFETS QUI PASSENT DERRIÈRE LES CORPS, avant tout le reste.
  //
  // ⚠ CE LOT EST CELUI DE L'IMAGE PRÉCÉDENTE, et cela suffit : `DrawEffectLayers`
  // le remplit plus bas, après avoir semé ce que cette image demande. Un effet
  // qui naît paraît donc derrière les corps à l'image suivante — un tic de
  // retard sur une colonne qui dure une minute.
  //
  // 🔴 SANS LUI, LE PILIER D'UN BUTIN RARE RECOUVRAIT LE BUTIN qu'il signale :
  // les effets se peignent par-dessus les acteurs, parce qu'ils les
  // ÉCLAIRCISSENT et n'auraient rien à éclaircir plus tôt.
  if (effects_.has_value()) {
    if (const std::span<const ro::render::DrawnEffect> behind_actors =
            effects_->DrawnBehindActors();
        !behind_actors.empty()) {
      effect_renderer_.Draw(camera_, behind_actors, aspect_ratio_);
    }
    // Et les sprites que leur mot range dans la même file : l'ombre d'un
    // costume, que le corps recouvre.
    PaintClassSpritesBehindActors(*effects_, sprites_, camera_, aspect_ratio_, fog);
  }
  // LES MODÈLES AVANT LES SPRITES : ce sont des volumes, qui écrivent leur
  // profondeur, et un sprite qui passe derrière doit s'y couper.
  if (link_.is_open() && pieces_.actor_models.has_value() &&
      pieces_.actor_model_renderer.has_value()) {
    pieces_.actor_model_renderer->Draw(camera_, pieces_.actor_models->drawn(), aspect_ratio_, fog);
  }
  // LES CORPS ET LEURS OMBRES laissent leur silhouette, que l'eau plonge et que
  // les halos évitent.
  paint(scene_actors_.drawn, ro::render::SpriteRenderer::DepthWrite::kOn, Silhouette::kLeft);
  // LES CHARRETTES, juste après les corps et avant leurs voiles : une charrette
  // derrière son maître s'y coupe, une charrette devant lui le couvre. Opaque,
  // elle écrit sa profondeur comme un corps ; translucide, elle s'en abstient,
  // comme le faucon.
  if (link_.is_open()) {
    const GraphicsOptions& graphics = settings_->options().graphics();
    paint(pieces_.carts.Update(CartFrame{&world_, &planner_, &*stage_, vfs_, camera_.yaw_degrees,
                                         now_ms, graphics.summon_translucent(),
                                         graphics.lightmaps() ? &stage_->ground_light : nullptr,
                                         graphics.cold_light()}),
          graphics.summon_translucent() ? ro::render::SpriteRenderer::DepthWrite::kOff
                                        : ro::render::SpriteRenderer::DepthWrite::kOn,
          Silhouette::kLeft);
  }
  // 🔴 LA NAPPE D'EAU APRÈS LES CORPS, LEURS OMBRES ET LES CHARRETTES : éprouvée contre
  // la profondeur qu'ils ont écrite, elle recouvre ce qui est SOUS sa surface — les pieds
  // de qui patauge, son ombre, les marques au sol. Puis cheminées et émetteurs, dessus.
  water_.Draw(camera_, aspect_ratio_, now_ms, fog);
  DrawMapEffects();
  // LES HALOS ET LES RÉMANENCES, sur l'eau mais DERRIÈRE tout corps, dont ils évitent la
  // silhouette. Les voiles viennent plus bas : peints avant leur porteur, il les masquerait.
  paint(scene_actors_.behind, ro::render::SpriteRenderer::DepthWrite::kOff, Silhouette::kAvoided);
  // LA GLACE DES GELÉS, après les corps qu'elle enferme : éprouvée en
  // profondeur, un mur la cache comme il cache le gelé.
  if (link_.is_open()) {
    paint(DrawnFrozenIce(pieces_, now_ms, scene_actors_.drawn, scene_actors_.ids, world_,
                         settings_->options().graphics().summon_translucent()),
          ro::render::SpriteRenderer::DepthWrite::kOff);
    // ET LA MARQUE DES MAUDITS, devant ou derrière le corps selon sa face.
    paint(DrawnCurseMarks(pieces_, now_ms, camera_.yaw_degrees, scene_actors_.drawn,
                          scene_actors_.ids, world_,
                          settings_->options().graphics().summon_translucent()),
          ro::render::SpriteRenderer::DepthWrite::kOff);
  }
  paint(scene_actors_.front, ro::render::SpriteRenderer::DepthWrite::kOff);
  // LES FAUCONS, après les corps : un oiseau translucide peint avant son maître
  // en serait découpé.
  if (link_.is_open()) {
    const GraphicsOptions& graphics = settings_->options().graphics();
    paint(pieces_.falcons.Update(FalconFrame{
              &world_, &planner_, &*stage_, vfs_, camera_.yaw_degrees, now_ms,
              graphics.summon_translucent(), graphics.lightmaps() ? &stage_->ground_light : nullptr,
              graphics.cold_light()}),
          ro::render::SpriteRenderer::DepthWrite::kOff);
  }
  // LA FLÈCHE DE LA CIBLE, un nœud de la scène comme les acteurs qu'elle surmonte.
  DrawSelectionMarker(now_ms);

  // Ce que les acteurs FONT ENTENDRE à cette image. Après la composition, qui
  // seule sait quelle image chacun montre.
  const ro::render::SpriteCatalog* tables =
      pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog() : nullptr;
  // ⚠ LA SCÈNE SONORE SE CONSTRUIT MÊME EN SILENCE : les effets la lisent aussi,
  // plus bas. Seule l'ÉCOUTE se garde — chercher puis décoder ce qu'on
  // n'entendra pas est du travail pur.
  const SoundStage scene{&audio_,
                         tables,
                         &camera_,
                         &*stage_,
                         &world_,
                         &planner_,
                         now_ms,
                         pieces_.wardrobe.has_value() ? &*pieces_.wardrobe : nullptr,
                         effects_.has_value() ? &*effects_ : nullptr,
                         settings_->options().pages().sprite_sounds.mutes(),
                         &scene_actors_.head_shifts,
                         &scene_actors_.hover_pixels};
  if (audio_.sound_audible()) {
    actor_sounds_->Hear(scene, scene_actors_.drawn, scene_actors_.ids);
    actor_sounds_->HearMapAmbience(scene);
  }

  RaiseMapSky(now_ms);

  // LES EFFETS DE LA CARTE, montés à la première image de chaque scène puis
  // laissés vivre. ⚠ ILS SE MONTENT ICI ET NON AU CHARGEMENT : le montage ne
  // coûte rien quand la carte n'a pas d'objet d'effet, et le faire à chaque
  // arrivée demanderait de le répéter sur les deux chemins de chargement.
  AdvanceMapEffects(now_ms);

  // Les effets par-dessus les acteurs : ils ÉCLAIRCISSENT ce qui est déjà peint,
  // et n'auraient rien à éclaircir plus tôt. Leur porte de dessin se juge sur le
  // monde et le groupe de cette image.
  effects_->NoticeDrawGate(world_, &party_);
  DrawEffectLayers(*effects_, *effect_nodes_, *actor_sounds_, footprints_, drops_, scene, sprites_,
                   effect_renderer_, pieces_.effect_ground, aspect_ratio_,
                   static_cast<float>(size_.width), static_cast<float>(size_.height), fog);
  if (const std::optional<std::uint32_t> quake = effects_->TakeScreenQuake()) {
    aim_.shake.Request(*quake);
  }
  SettleActorEffects(world_, effects_->TakeActorEffects());
  // LE DÉCOR VOILÉ DEVANT LE JOUEUR, et les faces mêlées du décor, sur tout ce
  // qui est peint derrière.
  if (models_shown) {
    pieces_.decor_veil.Draw(models_, camera_, *stage_, pieces_.night_light.light(), aspect_ratio_,
                            &model_clock_, fog);
  }

  // LA FIN DU MONDE PEINT : terrain, décor, personnages, effets. Ce qui suit se
  // pose PAR-DESSUS, hors du post-traitement, sans épreuve de profondeur.
  present_scene();

  // LES BULLES DU SOMMEIL, LES POINTS DU SILENCE, LES ÉTOILES DE L'ÉTOURDI, LES
  // MARQUES DE QUÊTE ET LE MARQUEUR DE CIBLE DES COMPAGNONS, en avant de toute
  // la scène.
  for (const std::span<const ro::render::DrawnActor> held :
       DrawnHeldChildSprites(pieces_, now_ms, scene_actors_.drawn, scene_actors_.ids, world_,
                             settings_->options().graphics().summon_translucent())) {
    sprites_.Draw(camera_, held, aspect_ratio_, ro::render::SpriteRenderer::Blending::kOver,
                  ro::render::SpriteRenderer::DepthWrite::kOff,
                  ro::render::SpriteRenderer::DepthTest::kOff);
  }
  // Les bulles au-dessus des têtes, avant les nombres : un coup porté se lit
  // d'abord, une humeur ensuite.
  //
  // ⚠ SANS ÉPREUVE DE PROFONDEUR, comme les nombres : une bulle se pose à
  // l'aplomb d'une tête, donc à SA profondeur, et la silhouette de l'acteur la
  // découperait.
  if (pieces_.emotions.has_value() && link_.is_open() && !scene_actors_.drawn.empty()) {
    const std::span<const ro::render::DrawnActor> bubbles =
        pieces_.emotions->Drawn(now_ms, scene_actors_.drawn, scene_actors_.ids, world_);
    if (!bubbles.empty()) {
      sprites_.Draw(camera_, bubbles, aspect_ratio_, ro::render::SpriteRenderer::Blending::kOver,
                    ro::render::SpriteRenderer::DepthWrite::kOff,
                    ro::render::SpriteRenderer::DepthTest::kOff);
    }
  }
  // Les nombres par-dessus les acteurs : ils vivent au-dessus d'eux.
  //
  // ⚠ ET SANS ÉPREUVE DE PROFONDEUR, PUISQUE L'ACTEUR ÉCRIT LA SIENNE. Un
  // nombre se pose à l'aplomb de sa victime, donc à SA profondeur : le test le
  // rejetterait partout où la silhouette est écrite, et l'on lirait « 11 » et
  // « 10 » de part et d'autre du personnage, les chiffres du milieu mangés. Ce
  // nombre n'est pas un objet de la scène mais ce qu'elle DIT, et rien de la
  // scène ne doit le cacher.
  if (pieces_.numbers.has_value() && link_.is_open()) {
    const std::span<const ro::render::DrawnActor> shown = pieces_.numbers->Drawn(
        now_ms, (*stage_), world_, planner_, settings_->options().damage_look(),
        NumberScreen{&camera_, aspect_ratio_, static_cast<float>(shell_.width()),
                     static_cast<float>(shell_.height())});
    // ⚠ TELS QUE DONNÉS, dans l'ordre de naissance : le dernier sorti par-dessus
    // les autres, quelle que soit la distance où sa dérive les a portés. Voir
    // `HitPopups::Drawn`.
    if (!shown.empty()) {
      sprites_.Draw(camera_, shown, aspect_ratio_, ro::render::SpriteRenderer::Blending::kOver,
                    ro::render::SpriteRenderer::DepthWrite::kOff,
                    ro::render::SpriteRenderer::DepthTest::kOff, ro::render::SceneFog(),
                    ro::render::SpriteRenderer::DrawOrder::kAsGiven);
    }
  }

  // Tant que le bouton reste enfoncé, la marche se refait vers la case que la
  // souris désigne MAINTENANT : c'est ce qui rend le déplacement continu.
  //
  // ⚠ LE DÉLAI EST CE QUI SÉPARE UN CLIC D'UN MAINTIEN, et il n'est pas là pour
  // ménager le serveur. La caméra suit le personnage : une souris immobile
  // désigne une case qui S'ÉLOIGNE avec lui, et refaire la demande à chaque
  // image ferait courir le personnage après elle. Voir kWalkRepeatMs.
  //
  // ⚠ ET LE DEMI-TOUR SE REFAIT AUSSI, à sa cadence à lui. Il n'avance que d'un
  // huitième par demande : sans cette reprise, tenir le bouton ne servirait à
  // rien et chaque huitième coûterait un clic. Le natif rejoue le sien au même
  // endroit, cent millisecondes après le dernier envoi chez le joueur — c'est
  // l'HORLOGE qui déclenche, la case n'étant que ce qu'il relit en passant.
  //
  // ⚠ NI SOUS UNE SOURIS FIGÉE, NI CTRL TENU : la reprise rejuge les deux à
  // chaque fois, comme le clic — voir `WorldTakesClicks` et `GroundClickMoves`.
  if (link_.is_open() && hovered_cell.has_value() && !over_unit &&
      WorldTakesClicks(MouseRouteOf(world_, grimoire_, world_mouse_.engagement, now_ms)) &&
      GroundClickMoves((SDL_GetModState() & SDL_KMOD_CTRL) != 0)) {
    // ⚠ STRICTEMENT PLUS que le délai, comme le plancher de la marche : à
    // égalité, la reprise partirait pour tomber aussitôt sur lui, et la
    // suivante attendrait un délai de plus.
    //
    // Et la reprise ne vise jamais la case où l'on se tient : voir `StandsOn`.
    if (world_mouse_.held_click == HeldClick::kWalk &&
        now_ms - world_mouse_.held_since_ms > kWalkRepeatMs && !StandsOn(world_, *hovered_cell)) {
      RequestGroundWalk(link_, world_, grimoire_.approach_clock(), &world_mouse_.deferred_walk,
                        scene_actors_.player_gesture, *hovered_cell, now_ms);
      // Chaque reprise est une marche demandée : elle fait retomber l'engagement.
      EndDeed(world_mouse_.engagement);
      world_mouse_.held_since_ms = now_ms;
    } else if (world_mouse_.held_click == HeldClick::kTurn &&
               now_ms - world_mouse_.held_since_ms >= kTurnRepeatMs) {
      // La scène de clic, refaite : le demi-tour lit le chemin et la case sous
      // la souris exactement comme à l'appui. ⚠ MAJ N'Y EST PLUS CONSULTÉE —
      // c'est l'appui qui a tranché, voir HeldClick.
      static_cast<void>(TurnTowardCell({.link = &link_,
                                        .world = &world_,
                                        .stage = &*stage_,
                                        .camera = &camera_,
                                        .planner = &planner_,
                                        .now_ms = now_ms},
                                       *hovered_cell));
      world_mouse_.held_since_ms = now_ms;
    }
  }

  // TOUT CE QUI SE PEINT EN COORDONNÉES D'ÉCRAN, dans son ordre. Voir screen_overlays.h.
  const ScreenOverlays overlays{
      .pieces = &pieces_,
      .world = &world_,
      .party = &party_,
      .camera = &camera_,
      .actors = &scene_actors_,
      .book = &grimoire_,
      .bench = &effect_bench_,
      .armed_skill = settings_->options().armed_skill(),
      .pick_boxes_on_screen = windows_.staff_tools.look.picking.boxes_shown && Rights().staff_tools
                                  ? &windows_.pick_boxes
                                  : nullptr,
      .bubbles = settings_->options().bubbles(),
      .bubbles_on_screen = &chat_bubbles_,
      .bubble_texts = &bubble_texts_,
      .item_icon = chat_tabs_.has_value() ? &chat_tabs_->rich().item_icon : nullptr,
      .setting_reach = SettingReachOf(pieces_),
      .entity_bars = settings_->options().entity_bars(),
      .entity_bars_on_screen = &entity_bars_,
      .shop_signboards_shown = settings_->options().shops().signboards_shown(),
      .options_open = windows_.options.open,
      .cast_bars_on_screen = &cast_bars_,
      .entity_names = settings_->options().entity_names(),
      .guild = &guild_book_,
      .emblems = guild_emblems_.has_value() ? &guild_emblems_->plates() : nullptr,
      .pvp_hud = settings_->options().pvp_hud(),
      .battleground = settings_->options().pages().battleground.settings(),
      // L'ADMINISTRATEUR comme le menu d'entité le reconnaît : le niveau de
      // groupe que le serveur nous annonce.
      .sees_concealed_enemies = Rights().staff_tools && world_.group_level() >= kAdminMinGroupLevel,
      .items = pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog().items : nullptr,
      .words = &pieces_.item_words,
      .creators =
          item_description_.has_value() ? item_description_->creator_names() : CreatorNames(),
      .connected = link_.is_open(),
      .talkbox_speakers =
          ground_units_.has_value() ? ground_units_->speakers() : std::span<const GroundUnitPick>(),
      .game_hud = &pieces_.over_world.game_hud()};
  // 🔴 ET LA VEILLE LES EFFACE AUSSI, pas seulement les fenêtres. Chez le client
  // d'origine, les étiquettes de nom et les bulles sortent du MÊME appel que
  // les fenêtres. Ici ce sont deux chemins, et n'en couper qu'un laisserait les
  // noms et les jauges flotter sur un décor qu'on a justement vidé.
  const bool afk_hides_ui = afk_screen_.hiding_ui(settings_->options().afk());
  const HoveredActor designated =
      afk_hides_ui
          ? HoveredActor{}
          : DrawScreenOverlays(overlays, aspect_ratio_, mouse_, static_cast<float>(shell_.width()),
                               static_cast<float>(shell_.height()), now_ms);

  // « QUI EST-CE ? » — ET SEULEMENT DE CE QUE LA SOURIS DÉSIGNE.
  //
  // 🔴 LE SERVEUR NE POUSSE PAS LA CARTE D'UN JOUEUR À L'ENTRÉE EN VUE : il ne
  // l'envoie qu'aux instants où elle CHANGE, si bien qu'un joueur déjà posé
  // quand on arrive n'en enverra jamais. Sans cette demande, aucune guilde,
  // aucun groupe, aucun titre ne paraît jamais — et l'étiquette se réduit au
  // nom que l'apparition portait.
  //
  // ⚠ ET ELLE NE SE POSE QU'UNE FOIS PAR SURVOL : voir `NameCardAsker`, et
  // `ro/net/actor_name.h` pour ce que le serveur refuse de dire.
  if (link_.is_open()) {
    if (const std::uint32_t asked_id = name_card_asker_.AskedFor(designated.actor); asked_id != 0) {
      ReportRefusal(ro::net::RequestActorName(link_, asked_id));
    }
  }

  ComposeCursor(designated.actor, hovered_cell, now_ms);
  return {};
}

// LA TEINTE DE LA VEILLE : ENTRE LE MONDE ET L'INTERFACE.
//
// 🔴 AVANT L'INTERFACE : l'horloge de la veille, peinte ensuite, n'est donc ni
// assombrie ni grenée — et elle doit rester lisible, puisque c'est tout ce qui
// reste à l'écran.
//
// 🔴 ET HORS DE `DrawWorld`, PARCE QU'IL A UNE SORTIE ANTICIPÉE. Une
// installation sans garde-robe rend la main avant la fin, et une teinte posée
// là-bas n'y serait jamais peinte : la veille s'installerait, l'interface
// disparaîtrait, l'image resterait intacte. Entre les deux étapes, aucun
// chemin ne la saute.
//
// ⚠ LA TAILLE EST CELLE DU TAMPON DE DESSIN, pas celle de la fenêtre : sur un
// écran à facteur d'échelle les deux diffèrent, et le transfert d'image à image
// se compte en pixels réels.
void Game::TintForAfk(std::uint32_t now_ms) {
  const ro::render::DrawableSize surface = gl_->drawable_size();
  screen_tint_.Draw(afk_screen_.tint(settings_->options().afk()), surface.width, surface.height,
                    now_ms);
}

void Game::ForgetWhatPlayed() {
  sky_raised_ = false;
  name_card_asker_.Forget();
  // L'horloge des modèles n'est PAS ici : elle ne s'efface qu'avec la scène, et
  // un téléport sur la même carte n'en change pas — voir l'arrivée sur carte.
  if (effects_.has_value()) {
    effects_->Forget();
  }
  if (effect_nodes_.has_value()) {
    effect_nodes_->Forget();
  }
  if (pieces_.numbers.has_value()) {
    pieces_.numbers->Forget();
  }
  if (pieces_.item_toast.has_value()) {
    pieces_.item_toast->Forget();
  }
  if (actor_sounds_.has_value()) {
    actor_sounds_->Forget();
  }
  footprints_.Forget();
  ghosts_.Forget();
  // Ce qui s'effaçait sur l'ancienne carte n'a rien à faire sur la nouvelle.
  scene_actors_.fading.clear();
  scene_actors_.corpses.clear();
  pieces_.falcons.Forget();
  pieces_.carts.Forget();
  pieces_.ailment_tints.Forget();
  pieces_.cart_choice.Forget();
  pieces_.talkbox.Forget();
  pieces_.npc_illustration.Forget();
  drops_.Forget();
  // Le monde purge la cible à l'arrivée ; les gestes en attente partent avec.
  targeting_.Forget();
  // Le cercle et les relances de Quick cast visaient l'ancienne carte.
  quick_cast_.Forget();
  // ⚠ LE SORT EN ATTENTE AUSSI, et c'est le sort AU SOL qui l'exige : une
  // action sur un acteur tombe d'elle-même quand il disparaît, une action sur
  // une case ne tombe jamais — le personnage repartirait vers les mêmes
  // coordonnées, sur la carte où il vient d'arriver, et y poserait le sort.
  // Le curseur armé vise la même carte révolue.
  grimoire_.CancelPending();
  grimoire_.Disarm();
  // LA FLÈCHE RENAÎT CACHÉE, et son animation repart, même carte comprise.
  // L'engagement, lui, ne tombe que sur une AUTRE carte : le nom annoncé est
  // déjà le nouveau, celui qu'on a tenté de charger encore l'ancien.
  EnterMap(world_mouse_.engagement, world_.map_name() == attempted_map_);
  marker_pose_ = MarkerPose();
  marker_epoch_ms_.reset();
  // 🔴 LE MENU NE SURVIT PAS À L'ENTITÉ QU'IL DÉSIGNE, ni la question qu'il
  // pose : sur une autre carte, le même GID peut désigner autre chose. Le dessin
  // suivant referme ce qui reste ouvert.
  windows_.entity_menu.state = ro::ui::EntityMenuState{};
  menu_gestures_.ask.reset();
  // « VOIR L'ÉQUIPEMENT » SE FERME ET OUBLIE : le serveur exige la même carte,
  // et une fiche qu'on ne peut plus redemander serait périmée sans le dire.
  if (viewed_equipment_.has_value()) {
    viewed_equipment_->Forget(inspected_, windows_.view_equip.state);
  }
  // 🔴 LES TEXTURES QUE LA CARTE QUITTÉE A SEULE RÉCLAMÉES S'EN VONT. Les caches
  // des renderers ne se purgent jamais d'eux-mêmes : sans cet appel, chaque
  // image × palette rencontrée resterait en mémoire vidéo toute la séance.
  // L'oubli garde ce qui a servi depuis le précédent, donc la carte qu'on
  // vient de quitter : un aller-retour ne retéléverse rien.
  sprites_.ForgetUnusedTextures();
  effect_renderer_.ForgetUnusedTextures();
  pieces_.overlay.ForgetUnusedTextures();
}

}  // namespace moonclient
