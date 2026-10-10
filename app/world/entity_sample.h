#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/entity_sample.h"

#include "world/actor_styles.h"

namespace moonclient {

// ── L'ENTITÉ DU CADRE D'ESSAI, dans la page « Entités » ─────────────────────
//
// Un représentant de l'espèce qu'on règle, dessiné comme sur la carte : un écart
// de douze pixels sous les pieds se juge contre un corps de la taille qu'il a
// en jeu, au zoom courant.

// Le représentant de ce que le cadre montre : le personnage du joueur pour lui,
// son groupe et les autres joueurs ; un Poring, un Baphomet, une Red Potion au
// sol ; et pour les PNJ, tour à tour une Kafra, un familier, un homoncule et un
// mercenaire. Un novice quand le joueur n'est pas encore là.
[[nodiscard]] ro::render::ActorLook SampleLookOf(const ro::ui::EntitySampleShown& drawn,
                                                 const ro::sim::World& world, std::uint32_t now_ms);

// La largeur et la hauteur de la vignette, en pixels. ⚠ ASSEZ POUR LE PLUS
// GRAND : ce qui sort de la boîte serait coupé.
inline constexpr std::int32_t kSampleBoxPixels = 512;

// Combien de représentants l'onglet PNJ fait tourner.
inline constexpr std::size_t kOtherSampleCount = 4;

class EntitySampler {
 public:
  // La vignette de ce que le cadre montre, animée, ses ancrages et les mots de
  // son étiquette. Une image vide sans garde-robe : le cadre pose alors ses
  // barres et son nom sur une entité absente.
  //
  // ⚠ COMME SUR LA CARTE, AU ZOOM COURANT : `sprite_scale` est l'échelle d'un
  // pixel de sprite à l'écran — voir `ro::render::ActorSpriteScreenScale` —,
  // `anchor_scale` celle des écarts — voir `ro::render::ScreenAnchorScale`.
  // `dressing` est le registre des couleurs de la carte : le joueur porte les
  // siennes. Nul, il porte la teinte officielle.
  [[nodiscard]] ro::ui::EntitySample Compose(ro::render::ActorWardrobe* wardrobe,
                                             ActorStyles* dressing,
                                             const ro::ui::EntitySampleShown& drawn,
                                             const ro::sim::World& world, float sprite_scale,
                                             float anchor_scale, std::uint32_t now_ms);

 private:
  // Dessine la pose et relève où ses ancrages sont tombés dans l'image.
  [[nodiscard]] ro::ui::EntitySample Draw(ro::render::ActorWardrobe& wardrobe,
                                          const ro::render::ActorVignette::Pose& pose,
                                          ro::ui::EntitySample sample);

  // LA STATURE DU PLUS GRAND des représentants de l'onglet PNJ, en pixels de
  // sprite : voir `ro::ui::EntitySample::reserved_stature`.
  [[nodiscard]] float OtherStature(ro::render::ActorWardrobe& wardrobe);

  // Le haut de chacun, mesuré une fois : mesurer une tenue compose toutes ses
  // images, ce qui ne se refait pas à chaque image de jeu.
  std::array<std::optional<float>, kOtherSampleCount> other_tops_{};

  // La garde-robe de la vignette, pour la refaire si elle en change.
  ro::render::ActorWardrobe* wardrobe_ = nullptr;
  std::optional<ro::render::ActorVignette> vignette_;
};

}  // namespace moonclient
