#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/point_lights.h"

namespace ro::render {

// LA LUMIÈRE QU'UN ACTEUR PORTE, et qui le suit : la lampe d'un casque, la
// flamme d'un monstre de feu, les feux de Sight. Voir docs/adr/0120.
struct ActorLightSpec {
  // Sa couleur, intensité comprise : au-delà de un, elle sature.
  glm::vec3 color{0.0F};
  // Le rayon où elle s'éteint, en unités du monde.
  float range_world = 0.0F;
  // Sa hauteur au-dessus des pieds de l'acteur, en unités du monde, pour un
  // acteur à sa taille ordinaire : un acteur grandi la porte d'autant plus haut.
  float height_world = 0.0F;
  // Elle vacille comme une flamme : voir `FlickerFactor`.
  bool flicker = false;
  // Elle porte ombre, une ombre DOUCE comme celle d'un sort : voir
  // `kEffectShadowDepth`.
  bool casts_shadow = false;
};

// UN MONSTRE QUI PORTE SA LUMIÈRE, désigné par le nom de son SPRITE — celui de
// `class_sprites.yml` —, et non par sa classe : les copies d'un monstre (un
// gardien, une variante d'événement) partagent son sprite, donc sa lumière.
struct SpriteLight {
  std::string_view sprite;
  ActorLightSpec light;
};

// UN ÉTAT QUI ÉCLAIRE TANT QU'IL EST PORTÉ, désigné par son bit d'option
// (e_option, avec les valeurs de src/map/status.hpp).
struct OptionLight {
  std::uint32_t option_mask = 0;
  ActorLightSpec light;
};

// Les catalogues : voir actor_light_catalogue.cpp.
[[nodiscard]] std::span<const SpriteLight> SpriteLightCatalogue();
[[nodiscard]] std::span<const OptionLight> OptionLightCatalogue();

// LES LUMIÈRES DE MONSTRE, rangées par CLASSE : le catalogue des sprites lu à
// travers la table des classes, une fois. Le nom d'un sprite s'y compare sans
// égard à la casse.
class ActorLightCatalogue {
 public:
  ActorLightCatalogue() = default;
  [[nodiscard]] static ActorLightCatalogue FromClassSprites(
      const std::unordered_map<std::uint32_t, std::string>& class_sprites);

  // La lumière que porte un acteur montré sous la classe `job`, ou rien.
  [[nodiscard]] const ActorLightSpec* OfJob(std::uint32_t job) const;

  [[nodiscard]] std::size_t size() const noexcept { return by_job_.size(); }

 private:
  std::unordered_map<std::uint32_t, const ActorLightSpec*> by_job_;
};

// UN ACTEUR DE L'IMAGE QUI PEUT PORTER UNE LUMIÈRE, tel que la scène l'a
// composé : données pures, le rendu ne connaît pas la simulation.
struct ActorLightBearer {
  // Ce qui décale son vacillement de celui de ses voisins : son identifiant.
  std::uint32_t seed = 0;
  // La classe sous laquelle il se MONTRE, et son mot d'option.
  std::uint32_t job = 0;
  std::uint32_t option = 0;
  // Ses pieds dans le monde, soulèvement compris.
  glm::vec3 feet_world{0.0F};
  // L'opacité de son corps, de zéro à un : sa lumière s'éteint avec lui quand
  // il fond, apparaît, sort de vue ou que son cadavre s'efface.
  float opacity = 1.0F;
  // De combien son corps est grandi.
  float scale = 1.0F;
};

// En deçà de cette opacité, un acteur n'éclaire plus : il ne prend pas de place.
inline constexpr float kActorLightMinOpacity = 0.02F;

// LES LUMIÈRES DES ACTEURS DE L'IMAGE, ajoutées à `out` : celle de sa classe,
// puis celle de chaque état qu'il porte. Chacune se tient à sa hauteur
// au-dessus de ses pieds, de sa couleur fois son opacité, vacillement compris à
// l'instant `time_ms`.
void ActorLightsOf(std::span<const ActorLightBearer> bearers, const ActorLightCatalogue& catalogue,
                   double time_ms, std::vector<PointLight>& out);

// LA PLACE DES ACTEURS dans le jeu de l'image : au plus tant de lumières, les
// plus proches de ce que regarde la caméra. Une galerie de mine compte des
// dizaines de mineurs ; les plus lointains s'éteignent d'abord.
inline constexpr std::size_t kMaxActorLights = 12;

// LE JEU DE L'IMAGE : `set` tel quel, puis les `kMaxActorLights` lumières
// d'`actors` les plus proches de `eye`.
[[nodiscard]] PointLightSet WithActorLights(const PointLightSet& set,
                                            std::span<const PointLight> actors, glm::vec3 eye);

}  // namespace ro::render
