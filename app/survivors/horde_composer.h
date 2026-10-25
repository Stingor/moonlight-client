#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_renderer.h"

#include "survivors/horde_poses.h"

namespace moonclient {

// CE QU'EST UN CORPS DESSINÉ POUR LA SIMULATION, au même rang que lui.
struct HordeTag {
  HordeBodyKind kind = HordeBodyKind::kFoe;
  std::uint32_t sim_id = 0;
  [[nodiscard]] bool operator==(const HordeTag&) const = default;
};

// Une apparence de la horde : un objet pour une gemme ou un objet au sol, une
// classe pour le reste.
struct HordeLookRef {
  HordeBodyKind kind = HordeBodyKind::kFoe;
  std::uint16_t look_id = 0;
  [[nodiscard]] bool operator==(const HordeLookRef&) const = default;
};

// LES OMBRES DE L'IMAGE.
struct HordeShadows {
  // L'ombre du soleil est peinte à cette image (`LitFrame::sun_shadows`) : la
  // page « Graphics » la demande, et la carte graphique ne l'a pas refusée.
  bool sun_shadows = false;
  // L'OPTION U27 : la horde entre aussi dans l'ombre du soleil. Fausse, seul
  // le héros y entre, et la horde garde son ombre ronde.
  bool horde_sun_shadows = false;
};

// LE COMPOSITEUR DE LA HORDE : il habille les poses par la garde-robe, et
// remplit à chaque image une liste de corps réutilisée.
//
// 🔴 LES CORPS ET LEURS ÉTIQUETTES SORTENT APPARIÉS : le corps de rang i est
// celui de l'étiquette de rang i. Une pose dont l'apparence n'a pas de corps
// est sautée dans les deux à la fois ; une entrée de plus dans l'une seule
// donnerait à un monstre le serial de son voisin.
//
// La composition est partagée par apparence, motion, image et direction : la
// tenue garde ce qu'elle a composé, et dix Porings qui marchent du même pas
// coûtent un Poring.
class HordeComposer {
 public:
  // Le sol sous une pose, en coordonnées du monde.
  using GroundOf = std::function<glm::vec3(const HordePose& pose)>;

  // ⚠ LA GARDE-ROBE DOIT SURVIVRE AU COMPOSITEUR : il en garde les tenues.
  explicit HordeComposer(ro::render::ActorWardrobe& wardrobe) noexcept;

  HordeComposer(const HordeComposer&) = delete;
  HordeComposer& operator=(const HordeComposer&) = delete;

  // L'apparence du héros : voir `HeroLook`. Sans elle, le héros n'a pas de
  // corps.
  void SetHeroLook(const ro::render::ActorLook& look);

  // Les durées des motions d'une apparence, lues sur son `.act` ; zéro pour
  // une apparence sans corps.
  [[nodiscard]] HordeMotionTimes TimesOf(HordeBodyKind kind, std::uint16_t look_id);
  // `TimesOf`, prêt à passer à `HordePoses::Pose`.
  [[nodiscard]] HordeTimesOf TimesSource();

  // HABILLE LES POSES de cette image. Le héros, s'il est posé en tête, reste
  // en tête des corps.
  //
  // Ombre ronde : aucune pour un mort ; sous l'ombre du soleil, aucune pour le
  // héros, ni pour la horde quand l'option U27 l'y fait entrer.
  void Compose(std::span<const HordePose> poses, const GroundOf& ground_of, HordeShadows shadows);

  [[nodiscard]] std::span<const ro::render::DrawnActor> drawn() const noexcept { return drawn_; }
  [[nodiscard]] std::span<const HordeTag> tags() const noexcept { return tags_; }
  // LES CORPS QUI PORTENT L'OMBRE DU SOLEIL : tous avec l'option U27, le héros
  // seul sinon. Vide sans ombre du soleil.
  [[nodiscard]] std::span<const ro::render::DrawnActor> sun_casters() const noexcept;

  // Les apparences demandées qui n'ont pas de corps dans l'installation.
  [[nodiscard]] std::span<const HordeLookRef> missing() const noexcept { return missing_; }

 private:
  // Une apparence déjà résolue, sa tenue (nulle sans corps) et les durées de
  // ses motions, lues une fois.
  struct KnownOutfit {
    HordeBodyKind kind = HordeBodyKind::kFoe;
    std::uint16_t look_id = 0;
    ro::render::ActorOutfit* outfit = nullptr;
    HordeMotionTimes times;
  };

  // L'apparence d'un corps qui n'est pas le héros, résolue à sa première
  // demande.
  [[nodiscard]] const KnownOutfit& KnownOf(HordeBodyKind kind, std::uint16_t look_id);
  [[nodiscard]] ro::render::ActorOutfit* OutfitOf(HordeBodyKind kind, std::uint16_t look_id);

  ro::render::ActorWardrobe* wardrobe_;
  ro::render::ActorOutfit* hero_outfit_ = nullptr;
  // Quelques dizaines d'apparences au plus, cherchées en ligne.
  std::vector<KnownOutfit> known_;
  std::vector<ro::render::DrawnActor> drawn_;
  std::vector<HordeTag> tags_;
  std::vector<HordeLookRef> missing_;
  // Les corps qui portent l'ombre du soleil, en tête de `drawn_`.
  std::size_t sun_casting_ = 0;
};

}  // namespace moonclient
