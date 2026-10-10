#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/model_mesh.h"
#include "ro/render/second_family_pose.h"

namespace ro::render {

// L'horloge des modèles animés d'une carte : l'image de chaque groupe de poses
// qui jouent de la même façon, et à cette image, la pose de ses nœuds et les
// grandeurs de ses textures.
//
// Les deux familles ne comptent pas pareil. La première avance d'un CRAN
// entier par pas logique ; la seconde lit une HORLOGE en millisecondes, et
// repart de zéro quand sa boucle s'achève.
//
// ⚠ UN SEUL CRAN PAR APPEL, même quand plusieurs pas logiques se sont écoulés.
// Le natif fait ainsi : il appelle l'avance une fois par image dessinée,
// pourvu qu'au moins un pas de 16 ms soit passé, et ne rattrape jamais. Une
// image qui traîne ralentit donc le décor de la première famille au lieu de le
// faire sauter.
//
// ⚠ Écart assumé : le natif n'avance que les objets d'un nœud VISIBLE de son
// arbre de scène, si bien que deux fontaines identiques se désynchronisent dès
// que l'une a quitté l'écran. Ici, tout avance ensemble.
class ModelClock {
 public:
  // Toutes les images repartent de zéro : c'est l'arrivée sur la carte.
  void Start(const std::vector<ModelInstances>& models, std::uint32_t now_ms);

  // Une avance pour chaque groupe si un pas logique au moins s'est écoulé
  // depuis la dernière, rien sinon.
  void Advance(const std::vector<ModelInstances>& models, std::uint32_t now_ms);

  // Pose un groupe à `frame`, hors de toute avance : pour un modèle dont
  // l'image se décide ailleurs — une unité au sol, que son déclenchement anime.
  // Le groupe naît au besoin, et rien ne se recalcule tant que l'image ne
  // change pas.
  void Hold(const std::vector<ModelInstances>& models, std::size_t model_index,
            std::size_t group_index, float frame);

  [[nodiscard]] bool started() const noexcept { return started_; }

  // L'image courante d'un groupe, ou zéro hors de ce que l'horloge connaît.
  [[nodiscard]] float Frame(std::size_t model_index, std::size_t group_index) const;

  // Les matrices des nœuds d'un groupe à son image courante, indexées comme
  // les nœuds de son gréement ; vide hors de ce que l'horloge connaît.
  [[nodiscard]] std::span<const glm::mat4> Pose(std::size_t model_index,
                                                std::size_t group_index) const;

  // Les grandeurs de texture d'un groupe, lot par lot comme RigTextureMotions.
  [[nodiscard]] std::span<const TextureMotion> TextureMotions(std::size_t model_index,
                                                              std::size_t group_index) const;

 private:
  struct Group {
    float frame = 0.0F;
    // Seconde famille : le départ de la boucle en cours.
    std::uint32_t start_ms = 0;
    std::vector<glm::mat4> pose;
    std::vector<TextureMotion> texture_motions;
  };

  [[nodiscard]] const Group* Find(std::size_t model_index, std::size_t group_index) const;

  // Indexé comme les modèles de la carte, puis comme leurs groupes animés.
  std::vector<std::vector<Group>> groups_;
  std::uint32_t origin_ms_ = 0;
  std::uint64_t steps_seen_ = 0;
  bool started_ = false;
};

}  // namespace ro::render
