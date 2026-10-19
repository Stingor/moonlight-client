// LE MODE ÉDITEUR DE CARTE, dans la boucle du jeu : voir docs/adr/0118 et
// `MapEditorSession`.
//
// Le panneau rend des intentions ; ici, elles passent à la session, le décor se
// repose quand le document change, et le panneau reçoit sa vue de l'image.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <SDL3/SDL_clipboard.h>

#include "ro/audio/audio_engine.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/render/map_scene.h"
#include "ro/render/skill_sound_table.h"
#include "ro/render/terrain_mesh.h"
#include "ro/ui/entity_menu.h"
#include "ro/ui/model_clipboard.h"
#include "ro/vfs/encoding.h"

#include "game/game.h"
#include "world/map_editor_mesh.h"
#include "world/map_editor_overlay.h"
#include "world/map_editor_sound_marks.h"
#include "world/map_sprites.h"

namespace moonclient {
namespace {

// UN SPRITE SAISI PAR SON NUMÉRO s'écrit sous son nom, quand la table des
// classes le connaît : le document se relit mieux ainsi.
void NameMapSprite(ro::ui::MapEditorIntent& intent, const ro::render::SpriteCatalog* catalog) {
  if (catalog == nullptr) {
    return;
  }
  if (auto* added = std::get_if<ro::ui::map_editor::AddSprite>(&intent)) {
    added->sprite = MapSpriteName(*catalog, added->sprite);
  } else if (auto* set = std::get_if<ro::ui::map_editor::SetSprite>(&intent)) {
    set->sprite.sprite = MapSpriteName(*catalog, set->sprite.sprite);
  }
}

constexpr std::string_view kSoundExtension = ".wav";

// LES SONS DE L'INSTALLATION, relatifs à data\wav\ : ce que la section « Sons »
// propose.
std::vector<std::string> SoundLibrary(ro::vfs::Vfs& vfs) {
  std::vector<std::string> files = vfs.List(ro::render::kSoundFolder, kSoundExtension);
  for (std::string& file : files) {
    if (file.size() > ro::render::kSoundFolder.size()) {
      file.erase(0, ro::render::kSoundFolder.size());
    }
  }
  return files;
}

// « ÉCOUTER » : le son une fois, au centre, à son volume, dans la famille des
// sons d'ambiance — sous le nom que leur donnent les sources de la carte, si
// bien qu'un son écouté n'est pas lu deux fois.
void Listen(ro::audio::AudioEngine& audio, ro::vfs::Vfs& vfs,
            const ro::ui::map_editor::ListenSound& listen) {
  if (!audio.is_open() || listen.file.empty()) {
    return;
  }
  if (!audio.KnowsSound(listen.file)) {
    auto bytes = vfs.Read(std::string(ro::render::kSoundFolder) + listen.file);
    if (!bytes) {
      ro::core::LogWarning("éditeur : son {} absent", listen.file);
      return;
    }
    if (auto held = audio.LoadSound(listen.file, *bytes); !held) {
      ro::core::LogWarning("éditeur : son {} illisible — {}", listen.file,
                           ro::core::Describe(held.error()));
      return;
    }
  }
  constexpr float kCentered = 0.0F;
  audio.PlaySound(listen.file, listen.volume, kCentered, ro::audio::SoundChannel::kAmbience);
}

}  // namespace

void Game::ServeMapEditor() {
  ServeMapEditorTools();
  // LES LUMIÈRES SE MONTRENT TOUJOURS, quel que soit l'outil : un cercle sur
  // chacune.
  ro::ui::MapEditorView& view = windows_.staff_tools.map_editor;
  if (!map_editor_.active() || !stage_.has_value()) {
    map_editor_.SetLightDots({});
    map_editor_.SetEffectDots({});
    map_editor_.SetMapEffectDots({});
    map_editor_.SetSpriteDots({});
    map_editor_.SetSoundDots({});
    return;
  }
  const auto width = static_cast<float>(shell_.width());
  const auto height = static_cast<float>(shell_.height());
  // EN MODE SOMMETS DU RELIEF, la seule poignée est celle du point désigné :
  // les flèches des lumières et des effets ne se montrent pas.
  const bool ground_points = map_editor_.ground_points_mode();
  // LE CERCLE DE LA BROSSE, au sol sous la souris.
  if (const std::optional<std::array<float, 2>>& pointer = map_editor_.ground_pointer();
      pointer.has_value()) {
    const std::vector<ro::ui::MapEditorLine> ring =
        BrushRing(stage_->scene, *pointer, map_editor_.Brush().radius_cubes, camera_, aspect_ratio_,
                  width, height);
    view.lines.insert(view.lines.end(), ring.begin(), ring.end());
  }
  map_editor_.SetEffectDots(EffectDots(view, stage_->scene, camera_, aspect_ratio_, width, height));
  const std::vector<ro::ui::MapEditorLine> effect_marks = map_editor_.EffectTrace();
  view.lines.insert(view.lines.end(), effect_marks.begin(), effect_marks.end());
  map_editor_.SetMapEffectDots(
      MapEffectDots(view, stage_->scene, camera_, aspect_ratio_, width, height));
  const std::vector<ro::ui::MapEditorLine> map_effect_marks = map_editor_.MapEffectTrace();
  view.lines.insert(view.lines.end(), map_effect_marks.begin(), map_effect_marks.end());
  // LES SPRITES DE DÉCOR, un carré sur chacun ; le désigné a la flèche.
  map_editor_.SetSpriteDots(SpriteDots(view, stage_->scene, camera_, aspect_ratio_, width, height));
  const std::vector<ro::ui::MapEditorLine> sprite_marks = map_editor_.SpriteTrace();
  view.lines.insert(view.lines.end(), sprite_marks.begin(), sprite_marks.end());
  if (view.selected_sprite.has_value() && *view.selected_sprite < view.sprites.size() &&
      !view.selected.has_value() && !ground_points) {
    const ro::render::MapEffectFrame map_frame{stage_->scene.gnd.width(),
                                               stage_->scene.gnd.height()};
    const GizmoFrame frame =
        GizmoFrameOf(map_frame, view.sprites[*view.selected_sprite].position, camera_);
    map_editor_.SetGizmo(frame, GizmoHandles(ro::ui::MapEditorTool::kMove, frame, camera_,
                                             aspect_ratio_, width, height));
    const std::vector<ro::ui::MapEditorLine> handles = map_editor_.GizmoTrace();
    view.lines.insert(view.lines.end(), handles.begin(), handles.end());
  }
  // L'EFFET DE LA CARTE DÉSIGNÉ a aussi la flèche de déplacement.
  if (view.selected_map_effect.has_value() && *view.selected_map_effect < view.map_effects.size() &&
      !view.selected.has_value() && !ground_points) {
    const ro::render::MapEffectFrame map_frame{stage_->scene.gnd.width(),
                                               stage_->scene.gnd.height()};
    const GizmoFrame frame =
        GizmoFrameOf(map_frame, view.map_effects[*view.selected_map_effect].position, camera_);
    map_editor_.SetGizmo(frame, GizmoHandles(ro::ui::MapEditorTool::kMove, frame, camera_,
                                             aspect_ratio_, width, height));
    const std::vector<ro::ui::MapEditorLine> handles = map_editor_.GizmoTrace();
    view.lines.insert(view.lines.end(), handles.begin(), handles.end());
  }
  // L'EFFET DÉSIGNÉ a la flèche de déplacement, comme une lumière.
  if (view.selected_effect.has_value() && *view.selected_effect < view.effects.size() &&
      !view.selected.has_value() && !ground_points) {
    const ro::render::MapEffectFrame map_frame{stage_->scene.gnd.width(),
                                               stage_->scene.gnd.height()};
    const GizmoFrame frame =
        GizmoFrameOf(map_frame, view.effects[*view.selected_effect].position, camera_);
    map_editor_.SetGizmo(frame, GizmoHandles(ro::ui::MapEditorTool::kMove, frame, camera_,
                                             aspect_ratio_, width, height));
    const std::vector<ro::ui::MapEditorLine> handles = map_editor_.GizmoTrace();
    view.lines.insert(view.lines.end(), handles.begin(), handles.end());
  }
  map_editor_.SetLightDots(LightDots(view, stage_->scene, camera_, aspect_ratio_,
                                     static_cast<float>(shell_.width()),
                                     static_cast<float>(shell_.height())));
  const std::vector<ro::ui::MapEditorLine> marks = map_editor_.LightTrace();
  view.lines.insert(view.lines.end(), marks.begin(), marks.end());
  // LA LUMIÈRE DÉSIGNÉE a la flèche de déplacement, comme une pose : aucun
  // modèle n'est alors désigné.
  if (view.selected_light.has_value() && *view.selected_light < view.lights.size() &&
      !view.selected.has_value() && !ground_points) {
    const ro::render::MapEffectFrame map_frame{stage_->scene.gnd.width(),
                                               stage_->scene.gnd.height()};
    const GizmoFrame frame =
        GizmoFrameOf(map_frame, view.lights[*view.selected_light].position, camera_);
    map_editor_.SetGizmo(frame, GizmoHandles(ro::ui::MapEditorTool::kMove, frame, camera_,
                                             aspect_ratio_, static_cast<float>(shell_.width()),
                                             static_cast<float>(shell_.height())));
    const std::vector<ro::ui::MapEditorLine> handles = map_editor_.GizmoTrace();
    view.lines.insert(view.lines.end(), handles.begin(), handles.end());
    // ET SON CERCLE DE PORTÉE : où elle s'éteint.
    const std::vector<ro::ui::MapEditorLine> ring =
        LightRangeRing(view.lights[*view.selected_light], stage_->scene, camera_, aspect_ratio_,
                       static_cast<float>(shell_.width()), static_cast<float>(shell_.height()));
    view.lines.insert(view.lines.end(), ring.begin(), ring.end());
  }
  // LES SONS : un carré sur chacun ; le désigné a la flèche, le cercle de sa
  // portée et son emprise.
  map_editor_.SetSoundDots(SoundDots(view, stage_->scene, camera_, aspect_ratio_, width, height));
  const std::vector<ro::ui::MapEditorLine> sound_marks = map_editor_.SoundTrace();
  view.lines.insert(view.lines.end(), sound_marks.begin(), sound_marks.end());
  if (view.selected_sound.has_value() && !view.selected.has_value() && !ground_points) {
    const auto shown =
        std::ranges::find(view.sounds, *view.selected_sound, &ro::ui::MapEditorSound::key);
    if (shown != view.sounds.end()) {
      const ro::render::MapEffectFrame map_frame{stage_->scene.gnd.width(),
                                                 stage_->scene.gnd.height()};
      const GizmoFrame frame = GizmoFrameOf(map_frame, shown->position, camera_);
      map_editor_.SetGizmo(frame, GizmoHandles(ro::ui::MapEditorTool::kMove, frame, camera_,
                                               aspect_ratio_, width, height));
      const std::vector<ro::ui::MapEditorLine> handles = map_editor_.GizmoTrace();
      view.lines.insert(view.lines.end(), handles.begin(), handles.end());
      const std::vector<ro::ui::MapEditorLine> reach =
          SoundReach(*shown, stage_->scene, camera_, aspect_ratio_, width, height);
      view.lines.insert(view.lines.end(), reach.begin(), reach.end());
    }
  }
  ServeGroundPoints(width, height);
}

void Game::ServeMapEditorTools() {
  ro::ui::StaffToolsState& tools = windows_.staff_tools;
  // LES ÉDITIONS QUE LE SERVEUR TIENT, chez tout joueur, staff ou non.
  if (stage_.has_value()) {
    const MapEditSync::Report synced =
        pieces_.map_edit_sync.Serve(stage_->name, map_edits_, *vfs_, link_);
    FollowServedMapEdit(synced);
  }
  // SANS LE DROIT, le mode se ferme : il peut se perdre en cours de session.
  if (!Rights().staff_tools || !stage_.has_value()) {
    map_editor_.Leave();
    editor_libraries_.models.Clear();
    tools.map_editor_intents.clear();
    tools.map_editor = ro::ui::MapEditorView();
    return;
  }
  map_editor_.FollowMap(stage_->name, stage_->scene);
  const ro::render::MapEffectFrame map_frame{stage_->scene.gnd.width(), stage_->scene.gnd.height()};
  // UN MODÈLE AJOUTÉ SE POSE où regarde la caméra.
  const glm::vec3 drop = map_frame.ToNative(camera_.look_at);
  map_editor_.SetDropPoint({drop.x, drop.y, drop.z});
  // LA BROSSE DE RELIEF, avant les intentions : un relâcher de cette image
  // clôt le glissé après son dernier pas.
  ServeGroundBrush();
  bool replace = false;
  const auto apply = [&](const ro::ui::MapEditorIntent& intent) {
    const MapEditorSession::Outcome outcome = map_editor_.Apply(intent, stage_->scene, map_edits_);
    replace |= PublishMapEdit(outcome);
    if (outcome.listen.has_value()) {
      Listen(audio_, *vfs_, *outcome.listen);
    }
  };
  editor_libraries_.models.NoteIntents(tools.map_editor_intents);
  for (ro::ui::MapEditorIntent& intent : tools.map_editor_intents) {
    NameMapSprite(intent, pieces_.wardrobe.has_value() ? &pieces_.wardrobe->catalog() : nullptr);
    apply(intent);
  }
  tools.map_editor_intents.clear();
  // LES GESTES DES POIGNÉES ET DU CLAVIER, mis en file.
  for (const ro::ui::MapEditorIntent& intent : map_editor_.TakeQueued()) {
    apply(intent);
  }
  if (replace) {
    ReplaceMapModels();
  }
  // LE SOL SUIT LE DOCUMENT, quel que soit le geste qui l'a changé : un pas de
  // brosse, une annulation, un retour au relief de la carte.
  FollowMapGround();
  tools.map_editor = map_editor_.View(stage_->scene, map_edits_);
  tools.map_editor.server_conflict = pieces_.map_edit_sync.Conflicted(stage_->name);
  // CE QUE LE .act DE CHAQUE SPRITE PORTE, que le panneau borne.
  if (pieces_.wardrobe.has_value()) {
    for (ro::ui::MapEditorSprite& sprite : tools.map_editor.sprites) {
      const MapSprites::ActCounts counts =
          pieces_.map_sprites.CountsOf(*pieces_.wardrobe, sprite.sprite, sprite.action);
      sprite.action_count = counts.action_count;
      sprite.frame_count = counts.frame_count;
    }
  }
  // LES SONS, lus une fois dans l'installation, à la première ouverture.
  if (map_editor_.active() && editor_libraries_.sounds == nullptr) {
    editor_libraries_.sounds =
        std::make_shared<const std::vector<std::string>>(SoundLibrary(*vfs_));
  }
  tools.map_editor.sound_library = editor_libraries_.sounds;
  // LA BIBLIOTHÈQUE DE MODÈLES ET LES VIGNETTES que le panneau demandait à
  // l'image précédente.
  if (map_editor_.active()) {
    editor_libraries_.models.Serve(*vfs_, tools.map_editor, tools.map_editor_panel);
  } else {
    editor_libraries_.models.Clear();
  }
  if (!map_editor_.active() || !map_editor_.selected().has_value()) {
    map_editor_.SetGizmo(GizmoFrame(), {});
    map_editor_.SetVertexContext(std::nullopt);
    vertex_context_.Forget();
    return;
  }
  const auto width = static_cast<float>(shell_.width());
  const auto height = static_cast<float>(shell_.height());
  tools.map_editor.lines = SelectionOutline(stage_->scene, *map_editor_.selected(), camera_,
                                            aspect_ratio_, width, height);
  tools.map_editor.selected_center = SelectionCenter(stage_->scene, *map_editor_.selected());
  if (map_editor_.tool() == ro::ui::MapEditorTool::kVertices) {
    ServeVertexTool(map_frame, width, height);
    return;
  }
  map_editor_.SetVertexContext(std::nullopt);
  map_editor_.SetVertexDots({});
  vertex_context_.Forget();
  // LA BROSSE DE RELIEF n'a pas de poignée : le modèle désigné garde sa boîte.
  if (map_editor_.tool() == ro::ui::MapEditorTool::kRelief) {
    map_editor_.SetGizmo(GizmoFrame(), {});
    return;
  }
  // LES POIGNÉES, sur le pivot du placement que montre la vue.
  const GizmoFrame frame = GizmoFrameOf(map_frame, tools.map_editor.position, camera_);
  map_editor_.SetGizmo(
      frame, GizmoHandles(map_editor_.tool(), frame, camera_, aspect_ratio_, width, height));
  const std::vector<ro::ui::MapEditorLine> handles = map_editor_.GizmoTrace();
  tools.map_editor.lines.insert(tools.map_editor.lines.end(), handles.begin(), handles.end());
}

void Game::ServeVertexTool(const ro::render::MapEffectFrame& map_frame, float width, float height) {
  ro::ui::MapEditorView& view = windows_.staff_tools.map_editor;
  if (vertex_context_.Refresh(stage_->scene, *map_editor_.selected(),
                              map_edits_.Document(stage_->name), *vfs_)) {
    map_editor_.SetVertexContext(vertex_context_.context());
  }
  const std::optional<MapEditorSession::VertexContext>& context = vertex_context_.context();
  if (!context.has_value()) {
    map_editor_.SetVertexContext(std::nullopt);
    map_editor_.SetVertexDots({});
    map_editor_.SetGizmo(GizmoFrame(), {});
    return;
  }
  map_editor_.SetVertexDots(VertexDots(*context, camera_, aspect_ratio_, width, height));
  // LA FLÈCHE, sur le sommet désigné.
  std::vector<GizmoHandle> handles;
  GizmoFrame frame;
  if (const auto vertex = map_editor_.selected_vertex();
      vertex.has_value() && vertex->first < context->nodes.size() &&
      vertex->second < context->nodes[vertex->first].world.size()) {
    const glm::vec3 native =
        map_frame.ToNative(context->nodes[vertex->first].world[vertex->second]);
    frame = GizmoFrameOf(map_frame, {native.x, native.y, native.z}, camera_);
    handles =
        GizmoHandles(ro::ui::MapEditorTool::kMove, frame, camera_, aspect_ratio_, width, height);
  }
  map_editor_.SetGizmo(frame, std::move(handles));
  // LA VUE SE RECOMPOSE : elle a été faite avant le contexte, et ne savait pas
  // encore nommer le sommet désigné. Ses traits gardent la boîte du modèle.
  std::vector<ro::ui::MapEditorLine> lines = std::move(view.lines);
  ro::ui::MapEditorPreview preview = std::move(view.preview);
  ro::ui::ModelLibraryView library = std::move(view.library);
  view = map_editor_.View(stage_->scene, map_edits_);
  view.library = std::move(library);
  view.sound_library = editor_libraries_.sounds;
  view.preview = std::move(preview);
  const std::vector<ro::ui::MapEditorLine> dots = map_editor_.VertexTrace();
  const std::vector<ro::ui::MapEditorLine> arrows = map_editor_.GizmoTrace();
  lines.insert(lines.end(), dots.begin(), dots.end());
  lines.insert(lines.end(), arrows.begin(), arrows.end());
  view.lines = std::move(lines);
}

void Game::ReplaceMapModels() {
  const auto started = std::chrono::steady_clock::now();
  ro::render::MapScene& scene = stage_->scene;
  const ro::mapedit::MapEditReport report =
      ro::render::ReplaceSceneModels(scene, map_edits_.Document(stage_->name), &*vfs_);
  for (const std::string& refusal : report.refused) {
    ro::core::LogWarning("édition de {} refusée — {}", stage_->name, refusal);
  }
  if (auto uploaded = models_.Upload(scene.models); !uploaded) {
    ro::core::LogError("édition de {} : le décor ne se téléverse pas — {}", stage_->name,
                       ro::core::Describe(uploaded.error()));
  }
  // L'horloge des modèles animés repart sur les nouvelles poses.
  model_clock_ = ro::render::ModelClock();
  // LE BILAN, une fois par seconde de glissé : ce que coûte de reposer le décor.
  const auto finished = std::chrono::steady_clock::now();
  const double elapsed_ms = std::chrono::duration<double, std::milli>(finished - started).count();
  const double now_ms =
      std::chrono::duration<double, std::milli>(finished.time_since_epoch()).count();
  if (replace_tally_.Add(elapsed_ms, models_.baked_batches(), now_ms)) {
    ro::core::LogDebug(
        "éditeur de {} : décor reposé {} fois, {:.1f} ms en moyenne, {:.1f} au pire, {} lot(s) "
        "recuit(s) sur {} modèle(s)",
        stage_->name, replace_tally_.count, replace_tally_.average_ms(), replace_tally_.worst_ms,
        replace_tally_.baked_batches, scene.models.size());
    replace_tally_ = ReplaceTally();
  }
}

namespace {

// LES LIGNES DU MENU D'UN MODÈLE, que `PlayMapModelVerb` reconnaît.
enum class MapModelVerb : int {
  kOpenEditor = 1,
  kToggleHidden,
  kMoveHere,
  kRevert,
  kSave,
  kCopy,
};

}  // namespace

void Game::OpenMapModelMenu(std::size_t model_index, std::uint32_t now_ms) {
  if (!stage_.has_value() || model_index >= stage_->scene.rsw.models().size()) {
    return;
  }
  map_editor_.FollowMap(stage_->name, stage_->scene);
  const ro::mapedit::MapEdit& document = map_edits_.Document(stage_->name);
  // UN MODÈLE AJOUTÉ prend rang après ceux de la carte : il se retire au lieu
  // de se masquer ou de se restaurer.
  const bool added = model_index >= stage_->scene.unfixed_models.size();
  const ro::mapedit::ModelEdit* edit = added ? nullptr : document.ModelAt(model_index);
  const bool unsaved = map_edits_.Unsaved(stage_->name);
  ro::ui::EntityMenuView built;
  built.subject = static_cast<std::uint32_t>(model_index);
  built.name =
      std::format("#{} {}", model_index,
                  ro::vfs::Cp949ToUtf8Lossy(stage_->scene.rsw.models()[model_index].model_name));
  built.family = ro::core::Tr("Modèle de la carte");
  built.hangul_name = true;
  built.items.push_back(ro::ui::EntityMenuItem{.label = ro::core::Tr("Ouvrir avec l'éditeur"),
                                               .verb = static_cast<int>(MapModelVerb::kOpenEditor),
                                               .staff = true});
  if (!added) {
    built.items.push_back(ro::ui::EntityMenuItem{
        .label = edit != nullptr && edit->hidden ? ro::core::Tr("Afficher le modèle")
                                                 : ro::core::Tr("Masquer le modèle"),
        .verb = static_cast<int>(MapModelVerb::kToggleHidden),
        .staff = true});
  }
  built.items.push_back(ro::ui::EntityMenuItem{.label = ro::core::Tr("Déplacer le modèle sur moi"),
                                               .verb = static_cast<int>(MapModelVerb::kMoveHere),
                                               .staff = true});
  if (added) {
    built.items.push_back(ro::ui::EntityMenuItem{.label = ro::core::Tr("Retirer le modèle ajouté"),
                                                 .verb = static_cast<int>(MapModelVerb::kRevert),
                                                 .staff = true,
                                                 .danger = true});
  } else {
    built.items.push_back(ro::ui::EntityMenuItem{
        .label = ro::core::Tr("Restaurer le modèle"),
        .verb = static_cast<int>(MapModelVerb::kRevert),
        .staff = true,
        .disabled = edit == nullptr,
        .tip = edit == nullptr ? ro::core::Tr("Le modèle n'est pas retouché.") : std::string()});
  }
  built.items.push_back(ro::ui::EntityMenuItem{
      .label = ro::core::Tr("Copier ce modèle"),
      .verb = static_cast<int>(MapModelVerb::kCopy),
      .staff = true,
      .tip =
          ro::core::Tr("Pour le coller dans une autre carte : Ctrl+V dans l'éditeur, ou « Coller "
                       "le modèle » dans sa bibliothèque.")});
  built.items.push_back(
      ro::ui::EntityMenuItem{.label = ro::core::Tr("Enregistrer la carte"),
                             .verb = static_cast<int>(MapModelVerb::kSave),
                             .separator = true,
                             .staff = true,
                             .disabled = !unsaved,
                             .tip = unsaved ? std::string() : ro::core::Tr("Rien à enregistrer.")});
  menu_model_ = model_index;
  menu_view_ = std::move(built);
  windows_.entity_menu.state.open_requested = true;
  menu_asked_ms_ = now_ms;
}

void Game::PlayMapModelVerb(int verb) {
  const std::optional<std::size_t> model = std::exchange(menu_model_, std::nullopt);
  if (!model.has_value() || !stage_.has_value()) {
    return;
  }
  // OÙ SE TIENT LE PERSONNAGE : le centre de sa case, dans le repère du RSW.
  std::array<float, 3> here{};
  if (const ro::sim::Actor* self = world_.player(); self != nullptr) {
    const std::array<float, 3> ground = ro::render::CellCenterPosition(
        stage_->scene.gnd, static_cast<std::int32_t>(self->position.x),
        static_cast<std::int32_t>(self->position.y));
    const glm::vec3 native =
        ro::render::MapEffectFrame{stage_->scene.gnd.width(), stage_->scene.gnd.height()}.ToNative(
            glm::vec3(ground[0], ground[1], ground[2]));
    here = {native.x, native.y, native.z};
  }
  MapEditorSession::Outcome outcome;
  switch (static_cast<MapModelVerb>(verb)) {
    case MapModelVerb::kOpenEditor:
      outcome = map_editor_.QuickModel(QuickModelAction::kOpenEditor, *model, here, stage_->scene,
                                       map_edits_);
      break;
    case MapModelVerb::kToggleHidden:
      outcome = map_editor_.QuickModel(QuickModelAction::kToggleHidden, *model, here, stage_->scene,
                                       map_edits_);
      break;
    case MapModelVerb::kMoveHere:
      outcome = map_editor_.QuickModel(QuickModelAction::kMoveHere, *model, here, stage_->scene,
                                       map_edits_);
      break;
    case MapModelVerb::kRevert:
      outcome = map_editor_.QuickModel(QuickModelAction::kRevert, *model, here, stage_->scene,
                                       map_edits_);
      break;
    case MapModelVerb::kSave:
      outcome = map_editor_.Apply(ro::ui::map_editor::Save{}, stage_->scene, map_edits_);
      break;
    case MapModelVerb::kCopy: {
      const ro::formats::RswModel& placed = stage_->scene.rsw.models()[*model];
      const ro::mapedit::MapEdit& document = map_edits_.Document(stage_->name);
      ro::ui::ClipboardModel copied{.model_file = ro::vfs::Cp949ToUtf8Lossy(placed.model_name),
                                    .rotation = placed.rotation,
                                    .scale = placed.scale,
                                    .animation_type = placed.animation_type,
                                    .animation_speed = placed.animation_speed};
      const std::size_t originals = stage_->scene.unfixed_models.size();
      if (*model >= originals && *model - originals < document.added().size()) {
        copied.opacity = document.added()[*model - originals].opacity;
      } else if (const ro::mapedit::ModelEdit* edit = document.ModelAt(*model)) {
        copied.opacity = edit->opacity;
      }
      SDL_SetClipboardText(ro::ui::ModelClipboardText(copied).c_str());
      break;
    }
  }
  if (PublishMapEdit(outcome)) {
    ReplaceMapModels();
  }
}

bool Game::PublishMapEdit(const MapEditorSession::Outcome& outcome) {
  using Publication = MapEditorSession::Outcome::Publication;
  MapEditSync& sync = pieces_.map_edit_sync;
  switch (outcome.publication) {
    case Publication::kNone:
      break;
    case Publication::kPublish:
      map_editor_.SetStatus(sync.Publish(stage_->name, map_edits_, link_));
      break;
    case Publication::kOverwrite:
      map_editor_.SetStatus(sync.Overwrite(stage_->name, map_edits_, link_));
      break;
    case Publication::kReload:
      FollowServedMapEdit(sync.Reload(stage_->name, stage_->name, map_edits_, link_));
      break;
  }
  return outcome.replace_models;
}

void Game::FollowServedMapEdit(const MapEditSync::Report& report) {
  // UN DOCUMENT REMPLACÉ : le décor se repose, le sol, les effets et les
  // lumières le suivent d'eux-mêmes, et l'historique de l'éditeur ne vaut plus.
  if (report.replaced) {
    map_editor_.ForgetDocument();
    ReplaceMapModels();
  }
  if (!report.status.empty()) {
    map_editor_.SetStatus(report.status);
  }
}

}  // namespace moonclient
