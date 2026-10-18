#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "ro/mapedit/edit_history.h"
#include "ro/mapedit/ground_edit.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/map_scene.h"
#include "ro/ui/map_editor_panel.h"

#include "world/map_edit_store.h"
#include "world/map_editor_gizmo.h"

namespace moonclient {

// LES RACCOURCIS DU MODE ÉDITEUR, Ctrl tenu : Z annule, Y — ou Maj+Z — rétablit,
// S enregistre, D duplique, F centre la caméra, et 1 à 5 choisissent l'outil. `key` est la touche,
// en minuscule ASCII ; rien pour une autre.
[[nodiscard]] std::optional<ro::ui::MapEditorIntent> MapEditorShortcut(char key, bool shift_held);

// LA POUSSÉE D'UNE FLÈCHE, Ctrl tenu : d'une unité du RSW sur x (gauche,
// droite) ou z (haut, bas), d'un demi-carreau avec Maj.
inline constexpr float kNudgeUnits = 1.0F;
inline constexpr float kNudgeWideUnits = 5.0F;
enum class NudgeDirection : std::uint8_t { kLeft, kRight, kUp, kDown };
[[nodiscard]] std::array<float, 3> MapEditorNudge(NudgeDirection direction, bool shift_held);

// LE MODE ÉDITEUR DE CARTE : voir docs/adr/0118.
//
// La session tient ce que le panneau montre et ce que le staff fait : le mode
// ouvert ou non, la sélection, l'historique, le geste en cours. Le document
// lui-même est dans `MapEditStore`. Elle ne dessine rien et ne touche à aucun
// renderer : elle dit seulement quand le décor doit se reposer.
// LA PLAGE D'HEURES, du document au panneau et retour : une plage absente se
// montre décochée, à ses bornes par défaut.
[[nodiscard]] ro::ui::MapEditorHours ShownHours(const std::optional<ro::mapedit::LitHours>& hours);
[[nodiscard]] std::optional<ro::mapedit::LitHours> DocumentHours(
    const ro::ui::MapEditorHours& hours);

// LES ACTIONS RAPIDES du menu de Ctrl+Alt+clic droit sur un modèle de la carte.
enum class QuickModelAction : std::uint8_t {
  // Ouvre l'éditeur, le modèle désigné.
  kOpenEditor,
  // Masque un modèle de la carte, ou le démasque.
  kToggleHidden,
  // Le pose là où se tient le personnage.
  kMoveHere,
  // Rend son placement d'origine ; retire un modèle ajouté.
  kRevert,
};

// Les intentions de la section « Sons » : voir map_editor_sounds.cpp.
template <typename Intent>
inline constexpr bool kIsSoundIntent = std::is_same_v<Intent, ro::ui::map_editor::SelectSound> ||
                                       std::is_same_v<Intent, ro::ui::map_editor::SetSound> ||
                                       std::is_same_v<Intent, ro::ui::map_editor::MoveSound> ||
                                       std::is_same_v<Intent, ro::ui::map_editor::RevertSound> ||
                                       std::is_same_v<Intent, ro::ui::map_editor::AddSound> ||
                                       std::is_same_v<Intent, ro::ui::map_editor::ListenSound>;

class MapEditorSession {
 public:
  // Ce qu'une intention demande à l'appelant.
  struct Outcome {
    // Le document a changé : le décor doit se reposer.
    bool replace_models = false;
    // Ce que le serveur doit apprendre du document : voir docs/adr/0124.
    enum class Publication {
      kNone,
      // Il vient de s'enregistrer : l'envoyer.
      kPublish,
      // Son envoi était périmé : le renvoyer, fondé sur la révision du serveur.
      kOverwrite,
      // Son envoi était périmé : adopter la révision du serveur.
      kReload,
    };
    Publication publication = Publication::kNone;
    // Un son à écouter une fois, hors du monde : « Écouter ».
    std::optional<ro::ui::map_editor::ListenSound> listen;
  };

  [[nodiscard]] bool active() const noexcept { return active_; }
  // LE POINT QUE LA CAMÉRA REGARDE au lieu du personnage, dans le repère du RSW,
  // posé par « Centrer » ; rien pour suivre le personnage. Marcher ou quitter
  // l'éditeur le lève.
  [[nodiscard]] const std::optional<std::array<float, 3>>& view_point() const noexcept {
    return view_point_;
  }
  void ReleaseViewPoint() noexcept { view_point_.reset(); }
  void Leave() noexcept;

  // SUIT LA CARTE AFFICHÉE : une autre carte vide l'historique, la sélection
  // et le geste en cours, qui ne valent que pour la leur, et relit les noms de
  // ses modèles pour l'arbre de la scène.
  void FollowMap(std::string_view map_name, const ro::render::MapScene& scene);
  // OUBLIE CE QUI VALAIT POUR LE DOCUMENT : l'historique, la sélection et le
  // geste en cours. Le document a été remplacé par un autre, ou la carte a
  // changé.
  void ForgetDocument() noexcept {
    history_.Clear();
    selection_.Clear();
    selected_light_.reset();
    selected_effect_.reset();
    selected_map_effect_.reset();
    selected_sprite_.reset();
    selected_sound_.reset();
    gesture_before_.reset();
    ground_stroke_.reset();
    designated_point_.reset();
    selected_points_.clear();
    ground_dots_.clear();
    pending_why_.clear();
  }
  // Une nouvelle pour la barre d'état, venue d'ailleurs que de l'éditeur.
  void SetStatus(std::string status) { status_ = std::move(status); }

  // Le modèle désigné, par son rang dans le RSW.
  [[nodiscard]] std::optional<std::size_t> selected() const noexcept {
    return selection_.model_index;
  }

  // Le modèle sous un clic dans le monde, ou rien.
  void Pick(std::optional<std::size_t> model_index);

  // UNE ACTION RAPIDE sur le modèle de rang `model_index`, qui devient le modèle
  // désigné, éditeur ouvert ou non. `here` : où se tient le personnage, dans le
  // repère du RSW.
  Outcome QuickModel(QuickModelAction action, std::size_t model_index,
                     const std::array<float, 3>& here, const ro::render::MapScene& scene,
                     MapEditStore& store);

  // DÉSIGNER MOINS, d'un cran : en mode Sommets du relief, les points du sol
  // sélectionnés se lâchent d'abord ; un sommet désigné se lâche seul, son
  // modèle reste ; sinon le modèle, la lumière ou l'effet désigné se lâche. Rien
  // pendant un glissé. Faux quand il n'y avait rien à lâcher.
  [[nodiscard]] bool Deselect() noexcept;

  // OÙ SE POSE UN MODÈLE AJOUTÉ, dans le repère du RSW : posé par l'appelant à
  // chaque image — le point que la caméra regarde.
  void SetDropPoint(const std::array<float, 3>& rsw_position) { drop_point_ = rsw_position; }

  // LES POIGNÉES DE L'IMAGE, posées par l'appelant : leur repère, et ce
  // qu'elles montrent à l'écran.
  void SetGizmo(const GizmoFrame& frame, std::vector<GizmoHandle> handles);
  [[nodiscard]] ro::ui::MapEditorTool tool() const noexcept { return tool_; }
  // Les traits des poignées, la saisie distinguée.
  [[nodiscard]] std::vector<ro::ui::MapEditorLine> GizmoTrace() const;
  // Un appui en `pointer` saisit-il une poignée ? Le placement de départ est
  // celui que montre `view`, la vue de l'image. `extend` : Maj tenue, un point
  // du sol s'ajoute à la sélection, ou s'en retire.
  [[nodiscard]] bool Grab(glm::vec2 pointer, const ro::ui::MapEditorView& view,
                          bool extend = false);
  [[nodiscard]] bool dragging() const noexcept {
    return drag_.has_value() || ground_stroke_.has_value();
  }
  // Le glissé de la poignée saisie jusqu'en `pointer`, puis son relâcher : ils
  // deviennent des intentions de placement, que `TakeQueued` rend.
  // `snap` : Maj tenue, le placement s'aimante — voir `Snapped`.
  void DragTo(glm::vec2 pointer, bool snap = false);
  void Release();
  [[nodiscard]] std::vector<ro::ui::MapEditorIntent> TakeQueued();
  // Une intention venue du clavier, rendue avec celles des poignées.
  void Queue(ro::ui::MapEditorIntent intent) { queued_.push_back(std::move(intent)); }

  // L'OUTIL SOMMETS : ce que l'appelant dit, à chaque image, du modèle désigné.
  struct VertexNode {
    // Le nom du nœud, en UTF-8.
    std::string name;
    // Du repère du nœud à celui du maillage : voir `ro::render::PosedNode`.
    glm::mat4 to_mesh{1.0F};
    // Les sommets tels que le fichier les porte, avant toute retouche.
    std::vector<std::array<float, 3>> origins;
    // Où l'édition les met, dans le monde.
    std::vector<glm::vec3> world;
  };
  struct VertexContext {
    std::size_t model_index = 0;
    // Le fichier du modèle, en UTF-8, et la position de sa pose dans le RSW.
    std::string model_file;
    std::array<float, 3> pose_origin{};
    // Du repère du maillage au monde : la matrice de la pose.
    glm::mat4 to_world{1.0F};
    ro::render::MapEffectFrame map_frame;
    std::vector<VertexNode> nodes;
  };
  void SetVertexContext(std::optional<VertexContext> context) {
    vertex_context_ = std::move(context);
  }
  // Le point d'un sommet à l'écran.
  struct VertexDot {
    glm::vec2 at{0.0F, 0.0F};
    std::size_t node = 0;
    std::size_t vertex = 0;
  };
  void SetVertexDots(std::vector<VertexDot> dots) { vertex_dots_ = std::move(dots); }
  // Le sommet désigné, par son nœud et son rang.
  [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> selected_vertex() const {
    return vertex_;
  }
  // LE CERCLE D'UNE LUMIÈRE à l'écran, et son rang dans le document : un clic
  // dessus la désigne.
  struct LightDot {
    glm::vec2 at{0.0F, 0.0F};
    std::size_t slot = 0;
  };
  void SetLightDots(std::vector<LightDot> dots) { light_dots_ = std::move(dots); }
  // LE LOSANGE D'UN EFFET à l'écran, et son rang : un clic dessus le désigne.
  void SetEffectDots(std::vector<LightDot> dots) { effect_dots_ = std::move(dots); }
  void SetMapEffectDots(std::vector<LightDot> dots) { map_effect_dots_ = std::move(dots); }
  [[nodiscard]] std::vector<ro::ui::MapEditorLine> MapEffectTrace() const;
  // LE CARRÉ D'UN SPRITE DE DÉCOR à l'écran, et son rang : un clic dessus le
  // désigne. Les carrés, le désigné distingué.
  void SetSpriteDots(std::vector<LightDot> dots) { sprite_dots_ = std::move(dots); }
  [[nodiscard]] std::vector<ro::ui::MapEditorLine> SpriteTrace() const;
  // LE REPÈRE D'UN SON à l'écran, et ce qui le désigne : un clic dessus le
  // désigne.
  struct SoundDot {
    glm::vec2 at{0.0F, 0.0F};
    ro::ui::MapEditorSoundKey key;
  };
  void SetSoundDots(std::vector<SoundDot> dots) { sound_dots_ = std::move(dots); }
  // Les repères des sons, le désigné distingué.
  [[nodiscard]] std::vector<ro::ui::MapEditorLine> SoundTrace() const;
  [[nodiscard]] std::vector<ro::ui::MapEditorLine> EffectTrace() const;
  // Les cercles des lumières, la réglée distinguée.
  [[nodiscard]] std::vector<ro::ui::MapEditorLine> LightTrace() const;
  // Les points des sommets, le désigné distingué.
  [[nodiscard]] std::vector<ro::ui::MapEditorLine> VertexTrace() const;

  // L'OUTIL RELIEF : voir map_editor_ground.cpp.
  //
  // Le point du sol sous la souris, dans la grille des coins — colonne, rangée,
  // fractionnaires —, posé par l'appelant à chaque image ; rien hors du sol.
  void SetGroundPointer(std::optional<std::array<float, 2>> corner_point) {
    ground_pointer_ = corner_point;
  }
  [[nodiscard]] const std::optional<std::array<float, 2>>& ground_pointer() const noexcept {
    return ground_pointer_;
  }
  // LE RELIEF RETOUCHÉ SE VOIT-IL ? Posé par l'appelant à chaque image : vrai
  // sous « Ombres en temps réel ». Faux, l'outil Relief se refuse, et le quitte
  // s'il était en main.
  void SetGroundAllowed(bool allowed);
  // La brosse, son rayon compté en points de la grille des coins.
  [[nodiscard]] ro::mapedit::GroundBrush Brush() const;
  // Un glissé de la brosse est-il en cours ?
  [[nodiscard]] bool stroking_ground() const noexcept { return ground_stroke_.has_value(); }
  // UN PAS DE BROSSE, à chaque image d'un glissé : il dure depuis le pas
  // précédent, `now_ms` sur une horloge qui ne recule pas. `lower` : Maj tenue,
  // la brosse Lever abaisse. Le document change ; il ne devient une entrée de
  // l'historique qu'au relâcher.
  void StrokeGround(double now_ms, bool lower, const ro::render::MapScene& scene,
                    MapEditStore& store);
  // Ce qu'a coûté le dernier remaillage du sol, et s'il suit le glissé en
  // direct : ce que le panneau en dit.
  void SetGroundRemesh(std::optional<double> remesh_ms, bool live) {
    ground_remesh_ms_ = remesh_ms;
    ground_remesh_live_ = live;
  }

  // LE MODE SOMMETS DE L'OUTIL RELIEF : voir map_editor_ground_points.cpp.
  //
  // Un point de la grille des coins : colonne, rangée.
  using CornerPoint = std::array<std::int32_t, 2>;
  // L'outil Relief est en main, en mode Sommets.
  [[nodiscard]] bool ground_points_mode() const noexcept {
    return tool_ == ro::ui::MapEditorTool::kRelief &&
           brush_.mode == ro::ui::MapEditorBrushMode::kPoints;
  }
  // Le point d'un point de la grille à l'écran, posé par l'appelant à chaque
  // image : un clic dessus le désigne. `height` : sa hauteur dans le sens du
  // monde ; `edited` : le document le retouche.
  struct GroundDot {
    glm::vec2 at{0.0F, 0.0F};
    CornerPoint point{};
    float height = 0.0F;
    bool edited = false;
  };
  void SetGroundDots(std::vector<GroundDot> dots) { ground_dots_ = std::move(dots); }
  // Le point désigné — celui qui porte la poignée —, et la sélection entière,
  // le désigné compris, rangée par rangée puis par colonne.
  [[nodiscard]] const std::optional<CornerPoint>& designated_ground_point() const noexcept {
    return designated_point_;
  }
  [[nodiscard]] const std::vector<CornerPoint>& selected_ground_points() const noexcept {
    return selected_points_;
  }
  // Les points de la grille, le retouché, le sélectionné et le désigné
  // distingués.
  [[nodiscard]] std::vector<ro::ui::MapEditorLine> GroundPointTrace() const;

  [[nodiscard]] Outcome Apply(const ro::ui::MapEditorIntent& intent,
                              const ro::render::MapScene& scene, MapEditStore& store);

  // Ce que le panneau montre.
  [[nodiscard]] ro::ui::MapEditorView View(const ro::render::MapScene& scene,
                                           const MapEditStore& store) const;

  [[nodiscard]] const std::string& map_name() const noexcept { return map_name_; }

 private:
  Outcome Place(const ro::ui::map_editor::Place& place, const ro::render::MapScene& scene,
                MapEditStore& store);
  Outcome SetWhy(std::string why, MapEditStore& store);
  Outcome Revert(MapEditStore& store);
  Outcome AddModel(const ro::ui::map_editor::AddModel& asked, const ro::render::MapScene& scene,
                   MapEditStore& store);
  // Une copie du modèle désigné, tel que l'édition le pose, décalée sur x.
  Outcome Duplicate(const ro::render::MapScene& scene, MapEditStore& store);
  // L'OUTIL SOMMETS : voir map_editor_vertices.cpp.
  [[nodiscard]] bool GrabVertex(glm::vec2 pointer);
  Outcome MoveVertex(const ro::ui::map_editor::MoveVertex& move, MapEditStore& store);
  Outcome RevertVertex(MapEditStore& store);
  Outcome SetAllPoses(bool all, MapEditStore& store);
  // La retouche du maillage du modèle désigné dans `document`, ou une neuve.
  [[nodiscard]] ro::mapedit::MeshEdit MeshFor(const ro::mapedit::MapEdit& document) const;
  // Le placement d'un modèle AJOUTÉ, de rang `slot` dans la liste du document.
  Outcome PlaceAdded(std::size_t slot, const ro::ui::map_editor::Place& place, MapEditStore& store);
  // Le rang dans la liste des ajouts du modèle désigné, s'il en est un.
  [[nodiscard]] std::optional<std::size_t> AddedSlot(const ro::render::MapScene& scene) const;
  // Vrai quand le document s'est écrit.
  [[nodiscard]] bool Save(MapEditStore& store);
  // LES LUMIÈRES PONCTUELLES : voir map_editor_lights.cpp.
  // Une lumière à `position`, ou au-dessus du point de pose sans position.
  Outcome AddLight(const std::optional<std::array<float, 3>>& position, MapEditStore& store);
  Outcome SetLight(const ro::ui::map_editor::SetLight& set, MapEditStore& store);
  Outcome RemoveLight(std::size_t slot, MapEditStore& store);
  // LES LIGNES DE L'ARBRE DE LA SCÈNE, rebâties seulement quand le document, la
  // carte ou ses modèles changent : les recopier à chaque image coûtait une
  // chaîne par modèle.
  void RefreshSceneRows(const ro::mapedit::MapEdit& document) const;
  // Ce que la vue montre des lumières.
  void ViewLights(const ro::mapedit::MapEdit& document, ro::ui::MapEditorView& view) const;
  // Retient un geste dans l'historique, s'il a changé quelque chose.
  void Record(std::string label, const ro::mapedit::MapEdit& before,
              const ro::mapedit::MapEdit& after);

  bool active_ = false;
  std::string map_name_;
  ro::mapedit::EditHistory history_;
  ro::mapedit::EditSelection selection_;
  // Le document d'avant le geste qui glisse, jusqu'à son relâcher.
  std::optional<ro::mapedit::MapEdit> gesture_before_;
  // Les raisons saisies pour un modèle qui n'est pas encore retouché : elles
  // rejoignent sa retouche quand elle naît.
  std::map<std::size_t, std::string> pending_why_;
  std::string status_;
  // Le fichier de chaque modèle de la carte, en UTF-8, dans l'ordre du RSW :
  // converti une fois par carte, et non à chaque image.
  std::vector<std::string> model_files_;
  ro::ui::MapEditorTool tool_ = ro::ui::MapEditorTool::kMove;
  std::array<float, 3> drop_point_{};
  std::optional<std::array<float, 3>> view_point_;
  // La lumière qu'on règle, par son rang dans le document, et les cercles de
  // toutes à l'écran.
  std::optional<std::size_t> selected_light_;
  std::vector<LightDot> light_dots_;
  // L'effet qu'on règle, et les losanges de tous à l'écran.
  std::optional<std::size_t> selected_effect_;
  std::vector<LightDot> effect_dots_;
  // L'effet de la carte qu'on règle, et les losanges de tous à l'écran.
  std::optional<std::size_t> selected_map_effect_;
  std::vector<LightDot> map_effect_dots_;
  // LES EFFETS DE LA CARTE : voir map_editor_map_effects.cpp.
  void ViewMapEffects(const ro::render::MapScene& scene, const ro::mapedit::MapEdit& document,
                      ro::ui::MapEditorView& view) const;
  Outcome SetMapEffect(const ro::ui::map_editor::SetMapEffect& set,
                       const ro::render::MapScene& scene, MapEditStore& store);
  Outcome RevertMapEffect(std::size_t index, MapEditStore& store);
  Outcome MoveMapEffect(const ro::ui::map_editor::MoveMapEffect& move,
                        const ro::render::MapScene& scene, MapEditStore& store);
  void ChooseMapEffect(std::optional<std::size_t> index);
  [[nodiscard]] bool GrabMapEffect(glm::vec2 pointer);
  [[nodiscard]] bool GrabMapEffectHandle(glm::vec2 pointer, const ro::ui::MapEditorView& view);
  // Les gestes du clavier sur l'effet de la carte désigné.
  Outcome NudgeMapEffect(const std::array<float, 3>& delta, const ro::render::MapScene& scene,
                         MapEditStore& store);
  Outcome HideMapEffect(const ro::render::MapScene& scene, MapEditStore& store);
  Outcome DuplicateMapEffect(const ro::render::MapScene& scene, MapEditStore& store);
  // LES EFFETS : voir map_editor_effects.cpp.
  void ViewEffects(const ro::mapedit::MapEdit& document, ro::ui::MapEditorView& view) const;
  Outcome AddEffect(std::int32_t number, MapEditStore& store);
  Outcome SetEffect(const ro::ui::map_editor::SetEffect& set, MapEditStore& store);
  Outcome RemoveEffect(std::size_t slot, MapEditStore& store);
  Outcome MoveEffect(const ro::ui::map_editor::MoveEffect& move, MapEditStore& store);
  void ChooseEffect(std::optional<std::size_t> slot);
  [[nodiscard]] bool GrabEffect(glm::vec2 pointer);
  [[nodiscard]] bool GrabEffectHandle(glm::vec2 pointer, const ro::ui::MapEditorView& view);
  // Les gestes du clavier sur l'effet désigné : voir map_editor_gestures.cpp.
  Outcome NudgeEffect(const std::array<float, 3>& delta, MapEditStore& store);
  Outcome DuplicateEffect(MapEditStore& store);
  // LES SPRITES DE DÉCOR : voir map_editor_sprites.cpp.
  void ViewSprites(const ro::mapedit::MapEdit& document, ro::ui::MapEditorView& view) const;
  Outcome AddSprite(std::string sprite, MapEditStore& store);
  Outcome SetSprite(const ro::ui::map_editor::SetSprite& set, MapEditStore& store);
  Outcome MoveSprite(const ro::ui::map_editor::MoveSprite& move, MapEditStore& store);
  Outcome RemoveSprite(std::size_t slot, MapEditStore& store);
  void ChooseSprite(std::optional<std::size_t> slot);
  [[nodiscard]] bool GrabSprite(glm::vec2 pointer);
  [[nodiscard]] bool GrabSpriteHandle(glm::vec2 pointer, const ro::ui::MapEditorView& view);
  Outcome NudgeSprite(const std::array<float, 3>& delta, MapEditStore& store);
  Outcome DuplicateSprite(MapEditStore& store);
  // Le sprite qu'on règle, et les carrés de tous à l'écran.
  std::optional<std::size_t> selected_sprite_;
  std::vector<LightDot> sprite_dots_;
  // LES SONS D'AMBIANCE : voir map_editor_sounds.cpp.
  // Une autre carte lâche le son désigné et relit les fichiers de ses sources.
  void FollowMapSounds(std::string_view map_name, const ro::render::MapScene& scene);
  void ViewSounds(const ro::render::MapScene& scene, const ro::mapedit::MapEdit& document,
                  ro::ui::MapEditorView& view) const;
  Outcome ApplySound(const ro::ui::map_editor::SelectSound& select,
                     const ro::render::MapScene& scene, MapEditStore& store);
  Outcome ApplySound(const ro::ui::map_editor::SetSound& set, const ro::render::MapScene& scene,
                     MapEditStore& store);
  Outcome ApplySound(const ro::ui::map_editor::MoveSound& move, const ro::render::MapScene& scene,
                     MapEditStore& store);
  Outcome ApplySound(const ro::ui::map_editor::RevertSound& revert,
                     const ro::render::MapScene& scene, MapEditStore& store);
  Outcome ApplySound(const ro::ui::map_editor::AddSound& add, const ro::render::MapScene& scene,
                     MapEditStore& store);
  Outcome ApplySound(const ro::ui::map_editor::ListenSound& listen,
                     const ro::render::MapScene& scene, MapEditStore& store);
  void ChooseSound(std::optional<ro::ui::MapEditorSoundKey> key);
  [[nodiscard]] bool GrabSound(glm::vec2 pointer);
  [[nodiscard]] bool GrabSoundHandle(glm::vec2 pointer, const ro::ui::MapEditorView& view);
  // Les gestes du clavier sur le son désigné ; rien quand aucun ne l'est.
  [[nodiscard]] std::optional<Outcome> NudgeSound(const std::array<float, 3>& delta,
                                                  const ro::render::MapScene& scene,
                                                  MapEditStore& store);
  [[nodiscard]] std::optional<Outcome> DuplicateSound(const ro::render::MapScene& scene,
                                                      MapEditStore& store);
  [[nodiscard]] std::optional<Outcome> DeleteSound(const ro::render::MapScene& scene,
                                                   MapEditStore& store);
  // Le point que « Centrer » regarde pour le son désigné ; rien sans lui.
  [[nodiscard]] std::optional<std::array<float, 3>> SoundFocus(const ro::render::MapScene& scene,
                                                               const MapEditStore& store) const;
  std::optional<ro::ui::MapEditorSoundKey> selected_sound_;
  std::vector<SoundDot> sound_dots_;
  // Le fichier de chaque source de son du RSW, en UTF-8, et la carte dont ils
  // viennent : convertis une fois par carte.
  std::vector<std::string> sound_files_;
  std::string sound_files_map_;
  // Le cache de `RefreshSceneRows`, et ce dont il a été bâti.
  mutable std::shared_ptr<const std::vector<ro::ui::MapEditorModelRow>> scene_rows_;
  mutable std::vector<std::size_t> edited_ranks_;
  mutable std::optional<ro::mapedit::MapEdit> rows_document_;
  mutable std::string rows_map_;
  mutable std::size_t rows_model_count_ = 0;
  // La saisie d'une poignée ou d'un sommet du modèle désigné ; puis celle d'un
  // cercle de lumière.
  [[nodiscard]] bool GrabTool(glm::vec2 pointer, const ro::ui::MapEditorView& view);
  [[nodiscard]] bool GrabLight(glm::vec2 pointer);
  // La saisie de la flèche de la lumière désignée.
  [[nodiscard]] bool GrabLightHandle(glm::vec2 pointer, const ro::ui::MapEditorView& view);
  // UN SEUL OBJET DÉSIGNÉ À LA FOIS : désigner une lumière oublie le modèle, et
  // désigner un modèle oublie la lumière — la flèche ne sert qu'un maître.
  void ChooseLight(std::optional<std::size_t> slot);
  void ChooseModel(std::optional<std::size_t> model_index);
  Outcome MoveLight(const ro::ui::map_editor::MoveLight& move, MapEditStore& store);
  // La caméra regardera ce qui est désigné : la lumière, ou la pose du modèle
  // telle que l'édition la place.
  Outcome Focus(const ro::render::MapScene& scene, const MapEditStore& store);
  // Ctrl+flèches : voir `ro::ui::map_editor::Nudge`.
  Outcome Nudge(const std::array<float, 3>& delta, const ro::render::MapScene& scene,
                MapEditStore& store);
  // La touche Suppr : voir `ro::ui::map_editor::Delete`.
  Outcome Delete(const ro::render::MapScene& scene, MapEditStore& store);
  // Le nombre de modèles du RSW d'origine : les ajouts prennent rang après.
  std::size_t original_count_ = 0;
  GizmoFrame gizmo_frame_;
  std::vector<GizmoHandle> handles_;
  // LA POIGNÉE SAISIE : son rang, le point de l'appui, le placement d'alors, et
  // le dernier placement qu'elle a donné.
  struct Drag {
    std::size_t handle = 0;
    GizmoHandle grabbed;
    GizmoFrame frame;
    glm::vec2 from{0.0F, 0.0F};
    ro::ui::map_editor::Place start;
    ro::ui::map_editor::Place last;
    // Le sommet que la poignée déplace, avec l'outil Sommets.
    std::optional<std::pair<std::size_t, std::size_t>> vertex;
    // La lumière que la poignée déplace, ou l'effet.
    std::optional<std::size_t> light;
    std::optional<std::size_t> effect;
    std::optional<std::size_t> map_effect;
    std::optional<std::size_t> sprite;
    std::optional<ro::ui::MapEditorSoundKey> sound;
    // La poignée lève les points du sol sélectionnés ; la hauteur du désigné à
    // l'appui, d'où l'aimantation compte.
    bool ground_points = false;
    float ground_height = 0.0F;
  };
  std::optional<Drag> drag_;
  std::vector<ro::ui::MapEditorIntent> queued_;
  std::optional<VertexContext> vertex_context_;
  std::vector<VertexDot> vertex_dots_;
  std::optional<std::pair<std::size_t, std::size_t>> vertex_;
  bool all_poses_ = false;
  // Les raisons saisies pour un maillage pas encore retouché.
  std::map<std::size_t, std::string> pending_mesh_why_;
  // L'OUTIL RELIEF : voir map_editor_ground.cpp.
  [[nodiscard]] bool GrabGround();
  void EndGroundStroke(MapEditStore& store);
  void RevertGround(MapEditStore& store);
  void SetGroundWhy(std::string why, MapEditStore& store);
  void SetBrush(const ro::ui::MapEditorBrush& brush);
  void ViewGround(const ro::mapedit::MapEdit& document, ro::ui::MapEditorView& view) const;
  // LE GLISSÉ DE LA BROSSE : la hauteur que vise Aplanir, prise sous le centre
  // au premier pas, et l'heure du pas précédent.
  struct GroundStroke {
    std::optional<float> flatten_height;
    std::optional<double> last_ms;
  };
  std::optional<GroundStroke> ground_stroke_;
  std::optional<std::array<float, 2>> ground_pointer_;
  ro::ui::MapEditorBrush brush_;
  std::optional<double> ground_remesh_ms_;
  bool ground_remesh_live_ = true;
  bool ground_allowed_ = true;
  // LE MODE SOMMETS DE L'OUTIL RELIEF : voir map_editor_ground_points.cpp.
  [[nodiscard]] bool GrabGroundPoint(glm::vec2 pointer, bool extend);
  // Le glissé de la poignée verticale, déjà projeté sur son axe : il devient
  // une levée de la sélection. `snap` : Maj tenue.
  void DragGroundPoints(bool snap);
  Outcome LiftGroundPoints(const ro::ui::map_editor::LiftGroundPoints& lift, MapEditStore& store);
  Outcome SetGroundPointHeight(const ro::ui::map_editor::SetGroundPointHeight& set,
                               const ro::render::MapScene& scene, MapEditStore& store);
  Outcome RevertGroundPoints(MapEditStore& store);
  // Ctrl+flèches haut et bas : lever la sélection de `lift` unités du monde.
  Outcome NudgeGroundPoints(float lift, MapEditStore& store);
  // Lâcher les points du sol désignés ; faux quand il n'y en avait pas.
  bool DeselectGroundPoints() noexcept;
  void ViewGroundPoints(const ro::render::MapScene& scene, const ro::mapedit::MapEdit& document,
                        ro::ui::MapEditorView& view) const;
  std::vector<GroundDot> ground_dots_;
  std::optional<CornerPoint> designated_point_;
  std::vector<CornerPoint> selected_points_;
};

}  // namespace moonclient
