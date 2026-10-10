#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/map_effect_table.h"
#include "ro/render/sprite_billboard.h"

namespace ro::render {

// À partir de quelle distance du JOUEUR un effet de carte cesse de vivre, en
// unités du monde : vingt-six cases. Tout effet la reçoit de son constructeur
// de base, et l'épreuve de proximité la lit. Le point de référence est la place
// du personnage.
inline constexpr float kMapEffectReachUnits = 130.0F;

// LE PAS D'UN EFFET DE CARTE, en secondes : un soixantième, écrit en dur dans
// le constructeur. Toutes les grandeurs de la table
// sont PAR SECONDE et se multiplient par lui à chaque pas.
inline constexpr float kMapEffectStepSeconds = 1.0F / 60.0F;

// UN ÉMETTEUR DE CARTE : l'effet 974.
//
// ⚠ SES PARTICULES SONT UNE RÉSERVE FIXE, pas une liste qui grandit. Le client
// en alloue `max_count` d'un coup, toutes LIBRES, et chaque pas en reprend
// quelques-unes à la réserve : une particule qui meurt y
// retourne et renaîtra. Le plafond est donc celui de la réserve, et une
// cadence plus forte que ce qu'elle peut servir ne produit rien de plus.
//
// ⚠ ET IL LES DESSINE TOUTES, vivantes ou non : une particule morte garde sa dernière pose et son
// dernier alpha. Une particule qui s'éteint en fondu est à zéro quand elle meurt ; une éternelle,
// elle, reste visible là où elle est tombée.
class MapEmitter {
 public:
  // L'émetteur posé par un objet 974 du RSW, réglé par son entrée de table.
  // `native_position` est la place de l'objet, dans le repère du client.
  MapEmitter(glm::vec3 native_position, MapEmitterSetting setting, std::uint32_t& draws);

  // UN PAS DU JEU, dans l'ordre du client : le
  // tri par distance, puis, s'il ne cache pas l'émetteur, sa mise à jour.
  void Tick(glm::vec3 native_player, std::uint32_t& draws);

  // Faux quand le client ne le dessine pas : loin du joueur ET sans plus
  // aucune particule vivante — ou pas encore mis à jour une seule fois, ce que
  // le client ne dessine jamais, puisqu'il met à jour avant de peindre.
  [[nodiscard]] bool drawn() const noexcept { return ticked_ && !hidden_; }

  // Les quads de toutes ses particules, dans NOTRE repère.
  void AppendQuads(const MapEffectFrame& frame, const BillboardBasis& basis,
                   std::vector<EffectWorldQuad>& quads) const;

  [[nodiscard]] const MapEmitterSetting& setting() const noexcept { return setting_; }
  [[nodiscard]] glm::vec3 native_position() const noexcept { return position_; }
  [[nodiscard]] std::size_t particle_count() const noexcept { return particles_.size(); }
  [[nodiscard]] std::size_t living() const noexcept { return used_; }

 private:
  // LA POSE qu'une particule a reçue à sa dernière mise à jour VIVANTE.
  enum class Pose : std::uint8_t {
    // Jamais mise à jour vivante : la matrice du constructeur, l'identité.
    kIdentity,
    // Tournée vers l'œil : seule son échelle est gardée, la caméra se lit au
    // moment de peindre.
    kFacing,
    // Couchée par ses trois angles : sa matrice entière.
    kPlaced,
  };

  struct Particle {
    // Champs 208 à 216 : l'écart au point d'émission.
    glm::vec3 offset{0.0F, 0.0F, 0.0F};
    // Champs 48 à 56.
    glm::vec3 velocity{0.0F, 0.0F, 0.0F};
    // Champs 36 à 44 : le point d'émission, plus l'écart quand elle vit.
    glm::vec3 position{0.0F, 0.0F, 0.0F};
    float size = 1.0F;
    float life_seconds = 0.0F;
    std::int32_t age_steps = 0;
    // Le constructeur d'une particule la veut opaque et blanche ; la naissance
    // remet l'alpha à zéro, la vie seule la teinte.
    std::int32_t alpha = 255;
    std::array<std::int16_t, 3> rgb{255, 255, 255};
    Pose pose = Pose::kIdentity;
    NativeMatrix matrix = kNativeIdentity;
    glm::vec2 facing_scale{1.0F, 1.0F};
    // Tirée de la réserve et pas encore rendue.
    bool used = false;
  };

  void InitParticle(Particle& particle, std::uint32_t& draws) const;
  void UpdateParticle(Particle& particle);

  glm::vec3 position_{0.0F, 0.0F, 0.0F};
  MapEmitterSetting setting_;
  std::vector<Particle> particles_;
  std::size_t used_ = 0;
  // Champ 252 : ce que la cadence a dû et pas encore servi.
  float carried_ = 0.0F;
  // Champ 193 : vrai tant que le joueur est à portée.
  bool near_ = false;
  // Champ 52 : vrai quand le client ne met plus l'émetteur à jour ni ne le peint.
  bool hidden_ = false;
  bool ticked_ = false;
};

}  // namespace ro::render
