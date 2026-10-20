#pragma once

#include <deque>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/actor_sprite.h"
#include "ro/render/sprite_renderer.h"

// LES PROJECTILES EN VOL QUI PORTENT OMBRE — la flèche d'un arc ou d'une
// créature, celles d'Arrow Shower, le trait d'une baguette, l'âme d'un Doram,
// la munition d'un monstre —, tels que la dernière composition des nœuds les a
// posés.
//
// Ils portent LA MÊME OMBRE QU'UN ACTEUR, dans le même mode graphique :
//
//   - SOUS LE SOLEIL, leur silhouette entre dans sa carte d'ombre, et dans
//     celles des lumières ponctuelles quand les ombres en temps réel sont
//     demandées — à la hauteur où ils volent, si bien que l'ombre tombe sur le
//     sol là où le soleil la porte ;
//   - SANS LUI, l'ombre ronde du client se pose au sol, à l'aplomb du
//     projectile, et ne se peint pas sous le soleil : sa silhouette en porte une.
//
// La lueur élémentaire qui vole avec un projectile n'en porte aucune : c'est de
// la lumière.
//
// ⚠ L'IMAGE LIT CEUX DE L'IMAGE PRÉCÉDENTE : les ombres se peignent avant les
// corps, et la troupe ne se compose qu'après eux. L'ombre a donc une image de
// retard sur son projectile.
namespace moonclient {

// L'OMBRE RONDE D'UN PROJECTILE, à l'échelle de celle d'un objet au sol : c'est
// sa taille, bien plus que celle d'un corps.
inline constexpr float kProjectileShadowScale = 0.4F;

class ProjectileCasters {
 public:
  ProjectileCasters() = default;
  ProjectileCasters(const ProjectileCasters&) = delete;
  ProjectileCasters& operator=(const ProjectileCasters&) = delete;
  ProjectileCasters(ProjectileCasters&&) noexcept = default;
  ProjectileCasters& operator=(ProjectileCasters&&) noexcept = default;
  ~ProjectileCasters() = default;

  // Oublie les projectiles de la composition précédente. L'ombre ronde reste.
  void Clear();

  // L'OMBRE RONDE, composée une fois depuis `data\sprite\shadow` : la première
  // image de sa première action, comme celle d'un acteur. Nul quand il ne se
  // lit pas : les projectiles n'ont alors que leur silhouette.
  [[nodiscard]] bool round_shadow_known() const noexcept { return round_shadow_known_; }
  void ComposeRoundShadow(const ro::render::LoadedActorSprite* shadow_sprite);

  // Un projectile composé, posé à `flight_position` ; `ground_height` est
  // l'altitude du sol sous lui, rien pour la hauteur même de son vol.
  void Add(const ro::render::ComposedActor& projectile, glm::vec3 flight_position,
           std::optional<float> ground_height);

  // LEURS SILHOUETTES, pour l'ombre du soleil et celles des lumières ponctuelles.
  [[nodiscard]] std::span<const ro::render::DrawnActor> silhouettes() const noexcept {
    return silhouettes_;
  }
  // LEURS OMBRES RONDES, sans corps : aucune sous le soleil, voir l'en-tête.
  [[nodiscard]] std::span<const ro::render::DrawnActor> GroundShadows(
      bool sun_shadows) const noexcept;

 private:
  // ⚠ UNE FILE ET NON UN TABLEAU : les silhouettes pointent leurs éléments, et
  // une file ne déplace pas ce qu'elle tient en grandissant par la fin.
  std::deque<ro::render::ComposedActor> projectiles_;
  std::vector<ro::render::DrawnActor> silhouettes_;
  std::vector<ro::render::DrawnActor> ground_shadows_;
  // Sur le tas, pour que les ombres qui la pointent survivent à un déplacement.
  std::unique_ptr<ro::render::ComposedActor> round_shadow_;
  bool round_shadow_known_ = false;
};

// LES LOTS QUI PORTENT OMBRE HORS DES CORPS — charrettes, faucons, sprites de
// décor, projectiles —, réunis en un seul : la passe d'ombre ne reçoit que deux
// lots d'acteurs, voir `ro::render::kSunShadowActorLots`.
[[nodiscard]] std::vector<ro::render::DrawnActor> JoinedCasterLots(
    std::initializer_list<std::span<const ro::render::DrawnActor>> lots);

}  // namespace moonclient
