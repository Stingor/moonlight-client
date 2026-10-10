#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/actor_effect_table.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/afterimage_blur.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/actor.h"

// LES RÉMANENCES : les copies du sprite qu'un effet sème derrière un acteur.
//
// C'est le mécanisme de Bunsinjyutsu, de l'hallucination, de la feinte et de
// Kagemusha — huit identifiants, une seule chose. Le client construit une COPIE
// de l'acteur, la pose dans la liste de scène à l'endroit où il se tenait, et
// cesse de la dessiner quand elle a fini son tour d'animation.
namespace moonclient {

// Combien de rémanences la scène porte à la fois.
//
// ⚠ RIEN NE LES EFFACE SI ON NE LES COMPTE PAS. Kagemusha en sème une par tic,
// soit soixante par seconde ; sans plafond, une seule compétence tenue en main
// remplirait la mémoire. Au-delà, la plus ancienne s'en va.
inline constexpr std::size_t kMaxActorGhosts = 96;

// CE QUE LA TRAÎNÉE ÉCRIT SUR LE CORPS, au rang de l'effet dans la liste de
// l'acteur — voir `ro::render::TintEveryTickWrite` et
// `ro::render::AfterimageBlurWrittenTint`. `draw` est le tirage de ce tic, qui
// ne sert qu'au clignotement. Rien quand l'acteur ne porte pas la traînée.
[[nodiscard]] std::optional<ro::render::TintEveryTickWrite> BlurTintWriteOf(
    const ro::sim::Actor& actor, const ro::render::AfterimageBlur& blur, std::uint32_t draw);

class ActorGhosts {
 public:
  // Sème une copie si un effet de cet acteur le demande à ce tic.
  //
  // `body` est l'image que l'acteur montre MAINTENANT : elle est COPIÉE, parce
  // qu'une tenue n'en garde qu'une par animation et par direction. Garder une
  // référence ferait vieillir la rémanence avec son porteur, ce qui est le
  // contraire de ce qu'elle est.
  //
  // `tint` est la teinte du porteur, lumière de la carte comprise : une copie la
  // garde, sauf quand son effet lui donne la sienne — l'ombre de Kagemusha est
  // grise et translucide —, qui la MULTIPLIE alors composante par composante.
  // Et une copie peut naître DERRIÈRE son porteur, sur l'axe de son corps, à
  // l'angle que `BlurOf` a noté.
  //
  // `life_ms` est le tour d'animation de l'acteur : une rémanence vit
  // exactement cela, et non un fondu (voir ActorEffectChange::clone_period_ticks).
  void Notice(std::uint32_t actor_id, std::span<const ro::render::ActorEffectPlay> played,
              const ro::render::ComposedActor& body, const glm::vec3& ground,
              const std::array<std::uint8_t, 4>& tint, float life_ms, std::uint32_t now_ms);

  // CE QUE LES RÉMANENCES LISENT SUR L'ACTEUR LUI-MÊME, à chaque image, avant
  // `Notice` : la traînée qu'il porte, ou rien — voir
  // `ro/render/afterimage_blur.h` —, et l'angle de son corps, gardé pour les
  // copies qui naissent derrière lui.
  //
  // `native_place` est l'endroit où il se tient, dans le repère du client. Une
  // traînée qu'aucun état n'a posée prend pour couleur la place de sa
  // NAISSANCE : elle est donc relevée la première fois qu'on la voit, et gardée
  // tant que l'effet n'est pas renoté.
  //
  // ⚠ L'ANGLE EST CELUI DU REGARD QUE LA SIMULATION TIENT, la marche suivie en
  // ligne droite : la copie d'un porteur qui contourne un obstacle naît
  // derrière la direction de ce trait, non derrière son pas.
  [[nodiscard]] std::optional<ro::render::AfterimageBlur> BlurOf(std::uint32_t actor_id,
                                                                 const ro::sim::Actor& actor,
                                                                 const glm::vec3& native_place,
                                                                 std::uint32_t now_ms);

  // Sème la copie de la traînée, si elle en sème une à ce tic. `tint` est la
  // teinte de la copie à sa naissance, lumière de la carte comprise ; son alpha
  // décroît ensuite de tic en tic.
  void NoticeBlur(std::uint32_t actor_id, const ro::render::AfterimageBlur& blur,
                  ro::render::Motion motion, const ro::render::ComposedActor& body,
                  const glm::vec3& ground, const std::array<std::uint8_t, 4>& tint,
                  std::uint32_t now_ms);

  // Celles qui se voient encore, ajoutées à `behind` — elles se peignent AVANT
  // les corps. Les autres s'en vont.
  void Drawn(std::uint32_t now_ms, std::vector<ro::render::DrawnActor>& behind);

  [[nodiscard]] std::size_t size() const noexcept { return ghosts_.size(); }

  // Plus aucune rémanence : le changement de carte emporte tout ce qui se
  // voyait, comme chez le client natif.
  void Forget() {
    ghosts_.clear();
    sown_.clear();
    blur_seen_ages_.clear();
    blur_places_.clear();
    body_degrees_.clear();
  }

 private:
  struct Ghost {
    ro::render::ComposedActor body;
    glm::vec3 ground{0.0F, 0.0F, 0.0F};
    std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
    std::uint32_t born_ms = 0;
    float life_ms = 0.0F;
    // Une copie de traînée s'efface de tic en tic au lieu de s'éteindre d'un
    // coup : voir `ro::render::AfterimageGhostAlphaAt`.
    bool fades = false;
  };

  void Sow(const ro::render::ComposedActor& body, const glm::vec3& ground,
           const std::array<std::uint8_t, 4>& tint, float life_ms, bool fades,
           std::uint32_t now_ms);

  std::vector<Ghost> ghosts_;
  // L'ÂGE AUQUEL chaque couple (acteur, effet) a semé pour la dernière fois.
  //
  // ⚠ UN TIC DURE PLUS QU'UNE IMAGE. Sans ce témoin, un effet de période cinq
  // sèmerait à chaque image des quatre ou cinq que dure son cinquième tic, et
  // la rémanence serait cinq fois trop dense.
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> sown_;
  // L'ÂGE DE LA TRAÎNÉE de chaque acteur à la dernière image qui l'a regardée,
  // semée ou non : voir `ro::render::AfterimageBlurSowsSince`.
  std::map<std::uint32_t, std::uint32_t> blur_seen_ages_;
  // LA PLACE DE NAISSANCE de chaque traînée sans état, et l'instant de l'effet
  // qu'elle vaut : renoté, l'effet renaît ailleurs.
  struct BlurPlace {
    std::uint32_t since_ms = 0;
    std::array<float, 3> color = {0.0F, 0.0F, 0.0F};
  };
  std::map<std::uint32_t, BlurPlace> blur_places_;
  // L'ANGLE DU CORPS de chaque acteur, en degrés du client, tel que `BlurOf`
  // l'a lu en dernier : c'est derrière lui qu'une copie reculée se pose.
  std::map<std::uint32_t, float> body_degrees_;
};

}  // namespace moonclient
