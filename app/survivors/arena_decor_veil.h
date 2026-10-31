#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/model_sight.h"
#include "ro/render/model_veil.h"
#include "ro/sim/survivors_rules.h"

#include "survivors/horde_poses.h"
#include "world/lit_stage.h"
#include "world/map_stage.h"

namespace moonclient {

// LE FONDU DU FEUILLAGE DE L'ARÈNE : un modèle du décor qui cache le héros, une
// plante, un objet au sol (coffre compris), une gemme ou un boss s'efface
// jusqu'au tiers de son opacité, puis revient quand plus rien ne s'y cache. La
// règle du voile est celle du jeu (`ro/render/model_veil.h`), un regard par
// cible au lieu du seul joueur.
//
// 🔴 TOUJOURS ALLUMÉ, quelle que soit l'option « Graphics » du voile en partie :
// dans l'arène, une plante ou un coffre caché derrière un arbre est perdu, et
// rien ne le signale. Rendu seul : les règles n'en savent rien.

// AU PLUS AUTANT DE REGARDS PAR IMAGE, les cibles rangées par ce qui compte le
// plus : le héros, les boss, les objets au sol, les plantes, les gemmes. Au
// plafond, ce sont des gemmes qui restent sans regard.
inline constexpr std::size_t kMostArenaVeilTargets = 160;

// LES CIBLES DU REGARD À CETTE IMAGE, dans l'ordre ci-dessus, au plus
// `kMostArenaVeilTargets` : le sol sous chacune, que `ground_of` donne pour une
// position en cases continues. Un boss est un monstre de rang mini-boss ou boss
// dans `rules`.
void ArenaVeilTargets(std::span<const HordePose> poses, const ro::sim::SurvivorsRules& rules,
                      const std::function<glm::vec3(glm::vec2)>& ground_of,
                      std::vector<glm::vec3>& into);

// CE QU'IL FAUT, À CHAQUE IMAGE, POUR FAIRE AVANCER LE FONDU.
struct ArenaVeilFrame {
  const MapStage* stage = nullptr;
  // Le compte des montages de la scène : un autre compte, une autre carte ou
  // une autre fenêtre du sol, dont les boîtes se refont.
  std::uint32_t mount = 0;
  const ro::render::Camera* camera = nullptr;
  float aspect_ratio = 1.0F;
  // La fenêtre : le regard vise au-dessus de l'ancre de chaque cible.
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::span<const glm::vec3> targets;
  std::uint32_t now_ms = 0;
};

class ArenaDecorVeil {
 public:
  // Fait avancer le fondu jusqu'à `frame.now_ms`.
  //
  // 🔴 UNE FENÊTRE NEUVE SE VOILE D'EMBLÉE : ses boîtes refaites, ce que les
  // regards traversent descend au plancher dans la même image. Le recentrage
  // ne fait donc pas réapparaître, pleins, les arbres qui cachaient le héros.
  void Advance(const ArenaVeilFrame& frame);
  // Tout redevient plein, et la carte s'oublie : la partie suivante.
  void Forget();

  // Le voile à passer à `LitStage::Paint` ; vide avant la première image.
  [[nodiscard]] LitDecorVeil decor() const noexcept;

 private:
  ro::render::ModelVeils veils_;
  std::optional<std::uint32_t> prepared_mount_;
  std::optional<std::uint32_t> last_ms_;
  std::vector<ro::render::SightSegment> segments_;
};

}  // namespace moonclient
