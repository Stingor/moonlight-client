#pragma once

#include <cstdint>
#include <optional>

#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/description_window.h"

#include "items/doll_body.h"
#include "world/actor_styles.h"

namespace moonclient {

// LE PERSONNAGE DU JOUEUR QUI PORTE UNE PIÈCE, pour l'aperçu du « ViewID ».
//
// Le pantin est le corps et la tête du joueur, rien d'autre, plus la pièce à
// sa place : une coiffe haute, du milieu ou basse, ou une cape — costumes
// compris. ⚠ LA COIFFE QUI COUVRE LES TROIS PLACES se pose sur la couche
// BASSE : c'est ce que le client dessine.
//
// Il vit à l'image : marche, repos, assis, deux secondes et demie chacun, et la
// molette le tourne.
//
// 🔴 UNE PIÈCE QUI N'A QU'UN EFFET DE CHAPEAU — un costume sans apparence —
// montre le pantin NU : c'est l'effet, peint par `DollEffects`, qu'on vient
// voir, et il lui faut quelqu'un à qui s'accrocher.
class ItemDoll {
 public:
  // La garde-robe peut manquer : sans sprites, aucun aperçu ne se propose.
  explicit ItemDoll(ro::render::ActorWardrobe* wardrobe);

  // La pièce de ce masque d'emplacement se montre-t-elle sur un pantin ?
  [[nodiscard]] bool Shows(std::uint32_t wearable_at) const noexcept;

  // Compose le pantin dans `state`, quand la ligne le demande ; l'efface sinon,
  // son effet compris. `with_effect` : la pièce a un effet de chapeau, et le
  // pantin se montre même sans elle.
  //
  // `dressing` : le registre des couleurs de la carte, pour que le pantin porte
  // celles du joueur. ⚠ LE MÊME QUE LA CARTE, et non un à lui : deux registres
  // donneraient deux identifiants pour une recette, donc deux tenues.
  void Refresh(const ro::sim::World& world, std::uint16_t view_id, std::uint32_t wearable_at,
               bool with_effect, ro::ui::DescriptionDollState& state, ActorStyles& dressing,
               std::uint32_t now_ms);

  // OÙ LE PANTIN EST TOMBÉ DANS SA BOÎTE : c'est ce que l'effet de chapeau suit.
  // Inutilisable tant que rien n'a été dessiné.
  [[nodiscard]] ro::render::VignetteFraming framing() const noexcept;

  // LE CORPS QUE LE PANTIN MONTRE À CETTE IMAGE, ou nul s'il n'en montre pas :
  // c'est lui qu'on repeint quand un effet de chapeau change l'acteur. Voir
  // `DollEffects`.
  [[nodiscard]] const DollBody* body() const noexcept {
    return body_.has_value() ? &*body_ : nullptr;
  }

 private:
  ro::render::ActorWardrobe* wardrobe_ = nullptr;
  std::optional<ro::render::ActorVignette> doll_;
  // ⚠ UNE COPIE : la tenue refait sa composition à chaque appel, et le corps
  // doit vivre jusqu'à la peinture, après toute l'interface.
  std::optional<DollBody> body_;
};

}  // namespace moonclient
