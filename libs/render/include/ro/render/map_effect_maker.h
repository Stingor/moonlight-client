#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/render/map_effect_math.h"
#include "ro/render/map_emitter.h"

namespace ro::render {

// ── LE SEMEUR D'EFFETS D'UNE CARTE ──────────────────────────────────────────
//
// Tout objet d'effet du RSW dont le numéro n'est pas celui d'une classe nommée
// devient un semeur : il ne dessine RIEN lui-même et se contente de LANCER, à
// intervalle, l'effet ordinaire de ce numéro à sa place. C'est lui qui allume
// les flammes des torches, les lucioles et les vapeurs des cartes.

// Le seul numéro que la cadence traite à part : les lucioles.
inline constexpr std::int32_t kMapFireflyEffect = 45;

// UN SEMEUR. Sa cadence se tire à la naissance, dans le générateur partagé des
// effets de carte.
//
// 🔴 SON COMPTEUR AVANCE MÊME LOIN DU JOUEUR, et seul le SEMIS est borné à la
// portée : revenir près d'un semeur dont le compte est mûr le fait partir au
// premier pas, sans attendre une période : la garde de distance n'entoure que
// le semis, jamais l'incrément.
class MapEffectMaker {
 public:
  // `emit_speed` est la vitesse de l'objet du RSW, en pas du jeu, et `param`
  // ses quatre paramètres.
  MapEffectMaker(glm::vec3 native_position, std::int32_t effect_id, float emit_speed,
                 std::array<float, 4> param, std::uint32_t& draws);

  // Un pas du jeu. Rend vrai quand le semeur lance son effet à cet instant :
  // seulement à moins de `reach_units` du joueur.
  [[nodiscard]] bool Tick(glm::vec3 native_player, float reach_units = kMapEffectReachUnits);

  [[nodiscard]] std::int32_t effect_id() const noexcept { return effect_id_; }
  [[nodiscard]] glm::vec3 native_position() const noexcept { return position_; }
  // 🔴 LES QUATRE PARAMÈTRES DE L'OBJET, QUE L'EFFET SEMÉ LIT. Le semeur les
  // passe à son instance par le message 46. Le PREMIER est l'échelle
  // d'instance : le bâtisseur d'arc en tire son rayon, et ne retombe sur le
  // rayon constant de son style que s'il n'est pas positif. Les halos de minerai
  // de manuk déclarent ainsi 8, 20, 25 ou 30.
  [[nodiscard]] std::array<float, 4> param() const noexcept { return param_; }
  // La cadence tirée à la naissance, en pas.
  [[nodiscard]] std::uint32_t period_steps() const noexcept { return period_; }
  [[nodiscard]] std::uint32_t counter_steps() const noexcept { return counter_; }

 private:
  glm::vec3 position_{0.0F, 0.0F, 0.0F};
  std::int32_t effect_id_ = 0;
  std::array<float, 4> param_{};
  // ⚠ NON SIGNÉS, ET LA COMPARAISON L'EST AUSSI. Le client convertit sa cadence
  // par une troncature SIGNÉE relue en non signé : une cadence négative ne
  // donne donc pas zéro mais un très grand compte, qui part au premier pas à
  // portée. Voir `NativeFloatToUnsigned`.
  std::uint32_t period_ = 0;
  std::uint32_t counter_ = 0;
};

// LA CONVERSION DU CLIENT : une troncature vers zéro faite EN SIGNÉ, puis relue
// en non signé. `-5` ne rend pas `0` mais `4294967291`. C'est ce qui fait qu'un
// semeur dont la vitesse est plus courte que son retard de départ lance dès le
// premier pas.
[[nodiscard]] std::uint32_t NativeFloatToUnsigned(float value);

}  // namespace ro::render
