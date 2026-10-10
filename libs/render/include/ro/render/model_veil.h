#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/formats/gat.h"
#include "ro/render/map_scene.h"
#include "ro/render/model_sight.h"
#include "ro/render/sight_quad_tree.h"

namespace ro::render {

// LE VOILE DU DÉCOR : une pose que le segment du regard traverse s'efface par
// crans jusqu'au tiers de son opacité, puis revient quand plus rien ne la
// marque.
//
// La règle du client, image par image, pour chaque pose qu'il dessine :
//
//   - marquée et au-dessus du plancher : cinq crans de moins, et la marque
//     RESTE — une pose effleurée une seule image descend donc jusqu'au bout ;
//   - marquée au plancher ou dessous : la marque tombe, l'opacité ne bouge pas ;
//   - non marquée : cinq crans de plus, jusqu'au plein.
//
// Le segment la re-marque à chaque image tant qu'il la traverse : elle se tient
// alors au plancher, où la marque tombe et revient d'une image à l'autre.
inline constexpr std::uint8_t kVeilOpaque = 255;
inline constexpr std::uint8_t kVeilFloor = 90;
inline constexpr std::uint8_t kVeilStep = 5;

// UNE IMAGE DU CLIENT, en millisecondes. Le voile avance d'un cran par image
// qu'il dessine, sans horloge : on compte soixante images par seconde, la même
// hypothèse que celle de la poursuite de la caméra.
inline constexpr float kVeilFrameMs = 1000.0F / 60.0F;

struct ModelVeil {
  std::uint8_t opacity = kVeilOpaque;
  bool marked = false;
};

// Une image du client pour une pose qu'il dessine.
void StepVeil(ModelVeil& veil);

// UNE POSE VOILÉE, et ce que son dessin doit savoir.
struct VeiledPose {
  // Rang du modèle dans MapScene::models.
  std::size_t model_index = 0;
  // Vide pour une pose figée ; sinon le groupe animé qui la porte.
  std::optional<std::size_t> group;
  // Dans les poses figées du modèle, ou dans celles de son groupe.
  std::size_t index = 0;
  std::uint8_t opacity = kVeilOpaque;
  // À cheval sur un bord de la vue : le client y écarte alors les faces
  // entièrement hors de l'écran.
  bool across = false;
};

// L'OPACITÉ DE CHAQUE POSE, par modèle : les figées dans leur ordre, puis les
// animées groupe par groupe. Vide pour un modèle dont rien ne se voile.
using ModelOpacities = std::vector<std::vector<std::uint8_t>>;

// LE VOILE DE TOUTES LES POSES D'UNE CARTE.
class ModelVeils {
 public:
  // Les boîtes et le quadtree de la carte ; tout repart opaque.
  void Prepare(const MapScene& scene, const formats::GatMap* gat);

  // Tout redevient opaque, sans rien avancer : l'option qu'on éteint.
  void Clear();

  // Avance d'autant d'images du client que le temps écoulé en contient. Sans
  // segment — aucun joueur —, rien ne se marque, mais ce qui s'effaçait
  // revient. `frustum` décide des poses que le client dessinerait : les
  // autres restent où elles en sont.
  void Advance(float elapsed_ms, const std::optional<SightSegment>& segment,
               const SightFrustum& frustum);

  [[nodiscard]] const ModelOpacities& opacities() const noexcept { return opacities_; }
  // Les poses voilées que la vue montre, pour la passe translucide.
  [[nodiscard]] std::span<const VeiledPose> veiled() const noexcept { return veiled_; }
  [[nodiscard]] std::span<const SightBox> boxes() const noexcept { return boxes_; }
  [[nodiscard]] std::span<const ModelVeil> states() const noexcept { return veils_; }

 private:
  struct Pose {
    std::size_t model_index = 0;
    std::optional<std::size_t> group;
    std::size_t index = 0;
    // Rang dans l'opacité de son modèle.
    std::size_t slot = 0;
  };

  void Publish(std::span<const FrustumSide> sides);

  std::vector<Pose> poses_;
  std::vector<SightBox> boxes_;
  std::vector<ModelVeil> veils_;
  SightQuadTree tree_;
  ModelOpacities opacities_;
  std::vector<VeiledPose> veiled_;
  std::vector<std::uint32_t> crossed_;
  std::vector<FrustumSide> sides_;
  float pending_ms_ = 0.0F;
};

}  // namespace ro::render
