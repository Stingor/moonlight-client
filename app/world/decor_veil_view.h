#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "ro/render/camera.h"
#include "ro/render/decor_opacity.h"
#include "ro/render/model_clock.h"
#include "ro/render/model_renderer.h"
#include "ro/render/model_veil.h"
#include "ro/render/scene_fog.h"
#include "ro/render/scene_light.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"

#include "world/map_stage.h"

namespace moonclient {

// CE QU'IL FAUT, À CHAQUE IMAGE, POUR VOILER LE DÉCOR DEVANT LE JOUEUR.
struct DecorVeilFrame {
  // L'option de la page « Graphics ». Éteinte, tout le décor redevient plein.
  bool enabled = false;
  const ro::sim::World* world = nullptr;
  ro::sim::WalkPlanner* planner = nullptr;
  const MapStage* stage = nullptr;
  // L'œil de cette image, déjà remonté au terrain.
  const ro::render::Camera* camera = nullptr;
  float aspect_ratio = 1.0F;
  // La fenêtre, en pixels : l'ancre du joueur s'y compte.
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::uint32_t now_ms = 0;
};

// LE VOILE DU DÉCOR, côté jeu : la carte dont il connaît les boîtes, la place du
// joueur, et le temps qui passe. Voir `ro/render/model_veil.h` pour la règle.
//
// ⚠ UNE OPTION, ET NON LE COMPORTEMENT DU CLIENT D'ORIGINE : celui-ci porte le
// test mais le laisse éteint. C'est un correctif de son exécutable qui l'allume,
// et il ne l'allume que pour la première famille de modèles ; ici les deux
// familles se voilent. Voir la fiche 0084.
//
// Le voile se MÊLE ICI À L'OPACITÉ QUE LE DOCUMENT D'ÉDITION donne aux modèles
// (`MapScene::model_opacities`), voir `ro/render/decor_opacity.h` : l'option
// éteinte, ce sont celles du document seules.
class DecorVeilView {
 public:
  void Advance(const DecorVeilFrame& frame);

  // Les opacités à passer au dessin opaque du décor ; rien quand l'option est
  // éteinte et que le document ne rend rien translucide.
  [[nodiscard]] const ro::render::ModelOpacities* opacities() const noexcept {
    if (composed_) {
      return &decor_.opacities;
    }
    return enabled_ ? &veils_.opacities() : nullptr;
  }

  // Les poses du décor qui portent ombre ; rien quand toutes en portent.
  [[nodiscard]] const ro::render::ModelOpacities* shadow_casting() const noexcept {
    return casting_.empty() ? nullptr : &casting_;
  }

  // LA PASSE TRANSLUCIDE, par-dessus les personnages et les effets.
  //
  // ⚠ ELLE SE DESSINE MÊME L'OPTION ÉTEINTE, sans pose voilée : elle porte aussi
  // les faces à texture mêlée que la passe opaque saute — voir
  // `ModelStages::blended_apart`.
  void Draw(const ro::render::ModelRenderer& models, const ro::render::Camera& camera,
            const MapStage& stage, const ro::render::SceneLight& light, float aspect_ratio,
            const ro::render::ModelClock* clock, const ro::render::SceneFog& fog,
            const ro::render::PointLightSet& points = ro::render::PointLightSet()) const;

  [[nodiscard]] const ro::render::ModelVeils& veils() const noexcept { return veils_; }

 private:
  // Le voile seul, de cette image.
  void AdvanceVeil(const DecorVeilFrame& frame);
  // Le document mêlé au voile.
  void Compose(const MapStage* stage);

  // LA CARTE DONT LES BOÎTES SONT PRÊTES : un changement de carte refait tout, un
  // téléport sur la même carte ne touche à rien.
  struct Prepared {
    std::string map_name;
    const ro::render::ModelInstances* models = nullptr;
    std::size_t model_count = 0;
  };

  ro::render::ModelVeils veils_;
  std::optional<Prepared> prepared_;
  bool enabled_ = false;
  std::optional<std::uint32_t> last_ms_;
  // Les opacités du document, pose par pose, et ce qu'elles font au voile et
  // aux ombres. `composed_` : le document rend au moins une pose translucide.
  ro::render::ModelOpacities document_;
  ro::render::DecorOpacity decor_;
  ro::render::ModelOpacities casting_;
  bool composed_ = false;
};

}  // namespace moonclient
