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

// CE QUE LA POSE MONTRE entre deux pas logiques.
enum class ModelClockBlend {
  // L'image du dernier pas, telle quelle : le décor avance par crans.
  kWholeSteps,
  // Une image entre celle du pas précédent et celle du pas courant, au poids
  // du temps écoulé depuis : voir `tick_blend.h`. Les crans restent ceux du
  // natif ; seule la pose glisse de l'un à l'autre.
  kBetweenSteps,
};

class ModelClock {
 public:
  // Toutes les images repartent de zéro : c'est l'arrivée sur la carte.
  void Start(const std::vector<ModelInstances>& models, std::uint32_t now_ms);

  // Une avance pour chaque groupe si un pas logique au moins s'est écoulé
  // depuis la dernière, rien sinon. Entre deux pas, la pose se refait à chaque
  // appel quand `blend` le demande.
  //
  // ⚠ UNE BOUCLE QUI REPART — l'image du pas courant SOUS celle du précédent —
  // ne se mélange pas : la pose montre l'image du pas courant, au lieu de
  // rejouer la boucle à rebours en un pas.
  void Advance(const std::vector<ModelInstances>& models, std::uint32_t now_ms,
               ModelClockBlend blend = ModelClockBlend::kWholeSteps);

  // Pose un groupe à `frame`, hors de toute avance : pour un modèle dont
  // l'image se décide ailleurs — une unité au sol, que son déclenchement anime.
  // Le groupe naît au besoin, et rien ne se recalcule tant que l'image ne
  // change pas.
  void Hold(const std::vector<ModelInstances>& models, std::size_t model_index,
            std::size_t group_index, float frame);

  // LE MÊME DÉCOR SOUS UNE AUTRE LISTE DE MODÈLES : les groupes de `to` qui ont
  // leur pareil dans `from` — même fichier, même variante, même façon de jouer —
  // en reprennent l'image et la pose ; les autres naissent au repos. L'origine
  // et les pas comptés restent : rien ne repart de zéro. Sans effet avant
  // `Start`.
  void Rebase(const std::vector<ModelInstances>& from, const std::vector<ModelInstances>& to);

  [[nodiscard]] bool started() const noexcept { return started_; }

  // L'image courante d'un groupe, ou zéro hors de ce que l'horloge connaît.
  [[nodiscard]] float Frame(std::size_t model_index, std::size_t group_index) const;

  // L'image où la pose du groupe est prise : `Frame` en crans entiers, une image
  // entre deux pas sinon. Zéro hors de ce que l'horloge connaît.
  [[nodiscard]] float ShownFrame(std::size_t model_index, std::size_t group_index) const;

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
    // L'image du pas précédent, et celle où la pose est prise.
    float previous_frame = 0.0F;
    float shown_frame = 0.0F;
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
  // Vrai quand une pose a pu être prise entre deux pas : couper le mélange la
  // ramène alors à l'image entière, au premier appel.
  bool blend_shown_ = false;
  bool started_ = false;
};

}  // namespace ro::render
