#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/ground_unit_pose.h"
#include "ro/render/model_clock.h"
#include "ro/render/model_mesh.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/vfs.h"

#include "world/ground_unit_pick.h"
#include "world/map_stage.h"

// Les UNITÉS POSÉES AU SOL : pièges, murs, zones.
namespace moonclient {

// Ce que les unités au sol donnent à dessiner.
//
// Une unité n'est ni un acteur ni un effet : c'est un MODÈLE 3D, du même genre
// que l'Emperium. Elle ne marche pas et ne tourne pas : elle est posée sur une
// case, FIGÉE sur l'image de pose de son modèle, et elle attend. Quand elle
// frappe, ou quand son apparence change — le piège se referme —, son modèle
// joue son animation une fois et reste sur la dernière image.
//
// Seule l'unité VISIBLE a un modèle : voir `SkillUnit::visible`.
//
// Le modèle est lu une fois par NUMÉRO d'unité : dix pièges du même type posés
// côte à côte n'en chargent qu'un, et l'échec est retenu comme le succès — un
// fichier absent ne se relit pas soixante fois par seconde.
class GroundUnits {
 public:
  explicit GroundUnits(ro::vfs::Vfs& vfs) : vfs_(&vfs) {}

  // Les instances à téléverser, ou RIEN si elles n'ont pas changé depuis le
  // dernier appel.
  //
  // Le renderer de modèles téléverse un lot entier : le lui redonner à chaque
  // image referait le travail pour un résultat identique. On ne le rend donc
  // que lorsque l'ensemble des unités posées a bougé — une pose, un retrait, un
  // changement de carte.
  [[nodiscard]] const std::vector<ro::render::ModelInstances>* Changed(const MapStage& stage,
                                                                       const ro::sim::World& world);

  [[nodiscard]] std::size_t placed() const noexcept { return placed_; }

  // LE SOL A BOUGÉ SOUS ELLES — le relief de la carte relu sur place : le
  // prochain `Changed` les repose, même si l'ensemble posé est le même.
  void ForgetPlacement() noexcept { witness_.clear(); }

  // Les unités que la souris peut désigner, relevées par le dernier `Changed`
  // — qui passe à chaque image, qu'il rende des instances ou non.
  [[nodiscard]] std::span<const GroundUnitPick> picks() const noexcept { return picks_; }

  // Les unités qui ont DIT quelque chose — une Talkie Box déclenchée —, au point
  // du sol de leur case, relevées par le même `Changed`.
  [[nodiscard]] std::span<const GroundUnitPick> speakers() const noexcept { return speakers_; }

  // L'horloge qui pose les modèles d'unité à leur image, un pas logique plus
  // tard s'il en est passé un. À appeler APRÈS `Changed`, sur le même monde :
  // elle suit les lots que `Changed` a rendus.
  [[nodiscard]] const ro::render::ModelClock& Advance(const ro::sim::World& world,
                                                      std::uint32_t now_ms);

 private:
  // Un modèle lu, ou l'échec retenu : `mesh` reste vide quand rien ne dessine
  // cette unité.
  struct Model {
    ro::render::ModelMesh mesh;
    // Partagées avec les lots qu'on lui fait poser : une recopie par changement
    // de l'ensemble posé ne coûte que des pointeurs.
    std::vector<std::shared_ptr<const ro::formats::Image>> textures;
    std::int32_t rsm_version = 0;
    std::int32_t shade_type = 0;
    // Vide quand rien ne bouge dans le modèle : il se dessine alors d'un bloc.
    ro::render::ModelRig rig;
  };

  // Le modèle BÂTI pour une unité posée, et où il en est de son animation.
  struct Built {
    // Le numéro d'unité dont on a bâti le modèle. ⚠ PAS FORCÉMENT CELUI DE
    // L'UNITÉ : un changement d'apparence vers un numéro sans modèle garde
    // l'ancien, qui joue alors son animation.
    std::uint32_t model_unit_id = 0;
    // L'apparence de l'unité à laquelle on a répondu, et l'instant où le modèle
    // a été bâti — l'horloge de la seconde famille en part.
    std::uint32_t look_seen_ms = 0;
    std::uint32_t built_ms = 0;
    ro::render::GroundUnitPlay play;
    // Où l'horloge le connaît : le lot et le groupe, ou rien pour un modèle
    // qui ne bouge pas.
    std::optional<std::pair<std::size_t, std::size_t>> group;
  };

  [[nodiscard]] const Model* ModelFor(std::uint32_t unit_id);
  // Bâtit, rebâtit ou oublie le modèle de chaque unité selon ce que le monde
  // en dit.
  void Follow(const ro::sim::World& world);

  ro::vfs::Vfs* vfs_ = nullptr;
  std::map<std::uint32_t, Model> models_;
  std::vector<ro::render::ModelInstances> instances_;
  // Par identifiant d'unité posée.
  std::map<std::uint32_t, Built> built_;
  // Ce qui était posé à la dernière construction : le modèle bâti et la case,
  // par identifiant. Deux pièges différents sur la même case doivent la faire
  // changer.
  std::vector<std::uint64_t> witness_;
  std::size_t placed_ = 0;
  std::vector<GroundUnitPick> picks_;
  std::vector<GroundUnitPick> speakers_;
  ro::render::ModelClock clock_;
  // Le départ des pas logiques, et le dernier pas vu.
  std::optional<std::uint32_t> origin_ms_;
  std::uint64_t steps_seen_ = 0;
};

}  // namespace moonclient
