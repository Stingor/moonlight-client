#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/formats/gnd.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/ground_light.h"
#include "ro/render/sprite_renderer.h"

// LES SPRITES DE DÉCOR DU DOCUMENT D'ÉDITION, peints : voir
// ro/mapedit/map_sprite.h. Ce ne sont pas des acteurs du monde — ni nom, ni
// jauge, ni clic —, seulement des corps composés par la garde-robe et peints
// avec les corps, ombres et lumières comprises.
namespace moonclient {

// Ce qu'une image donne aux sprites de décor.
struct MapSpriteFrame {
  const ro::mapedit::MapEdit* document = nullptr;
  ro::render::ActorWardrobe* wardrobe = nullptr;
  const ro::formats::GndMap* gnd = nullptr;
  float camera_yaw_degrees = 0.0F;
  std::uint32_t now_ms = 0;
  // La lumière de la carte, quand elle est allumée ; nulle sinon.
  const ro::render::GroundLight* light = nullptr;
  bool cold_light = false;
  // L'ombre ronde sous les pieds, quand celle du soleil ne la remplace pas.
  bool round_shadow = true;
};

// LE ROULIS : chaque coin des calques tourne de `degrees` autour du centre de
// la boîte qui les contient tous, dans le sens des aiguilles d'une montre à
// l'écran. L'image entière pivote ainsi dans son plan, comme sur une broche.
void RollQuads(ro::render::ComposedActor& composed, float degrees);

class MapSprites {
 public:
  // Compose les sprites de décor visibles du document à cet instant.
  void Update(const MapSpriteFrame& frame);

  // Ce qui se peint, les pleins d'abord : ils écrivent leur profondeur, les
  // translucides non. `all` les réunit, pour les passes d'ombre.
  [[nodiscard]] std::span<const ro::render::DrawnActor> opaque() const noexcept {
    return std::span(drawn_).first(opaque_count_);
  }
  [[nodiscard]] std::span<const ro::render::DrawnActor> translucent() const noexcept {
    return std::span(drawn_).subspan(opaque_count_);
  }
  [[nodiscard]] std::span<const ro::render::DrawnActor> all() const noexcept { return drawn_; }

  // CE QUE LE .act D'UN SPRITE PORTE : ses rangs d'animation, et les images de
  // `action` ; zéro et zéro quand le sprite ne se lit pas.
  struct ActCounts {
    std::size_t action_count = 0;
    std::size_t frame_count = 0;
  };
  [[nodiscard]] ActCounts CountsOf(ro::render::ActorWardrobe& wardrobe, std::string_view sprite,
                                   std::uint32_t action);

  // Le changement de carte n'emporte rien que la garde-robe ne garde déjà ;
  // seul le dessin de l'image s'oublie.
  void Forget() {
    drawn_.clear();
    composed_.clear();
    opaque_count_ = 0;
  }

 private:
  // La classe que nomme `sprite` — son numéro, ou son nom dans la table des
  // classes —, ou rien. Retenue par nom : la table se parcourt une fois.
  [[nodiscard]] std::optional<std::uint32_t> ClassOf(const ro::render::SpriteCatalog& catalog,
                                                     std::string_view sprite);
  [[nodiscard]] ro::render::ActorOutfit* OutfitOf(ro::render::ActorWardrobe& wardrobe,
                                                  std::string_view sprite);

  std::map<std::string, std::optional<std::uint32_t>, std::less<>> classes_;
  std::vector<ro::render::ComposedActor> composed_;
  std::vector<ro::render::DrawnActor> drawn_;
  std::size_t opaque_count_ = 0;
};

// LE NOM QUE PREND UN SPRITE SAISI : un numéro de classe que la table des
// classes nomme devient ce nom ; tout le reste se garde tel quel.
[[nodiscard]] std::string MapSpriteName(const ro::render::SpriteCatalog& catalog,
                                        std::string_view typed);

}  // namespace moonclient
