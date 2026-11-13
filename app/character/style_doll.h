#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/actor_sprite.h"
#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/icon_texture.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/style_set.h"

namespace moonclient {

// LE PANTIN D'UN STYLE : notre corps, peint d'une recette, tel que l'éditeur et
// l'aperçu d'un style reçu le montrent.
//
// ⚠ PARTAGÉ PLUTÔT QUE RECOPIÉ : les deux fenêtres montrent LA MÊME CHOSE, et
// deux copies de la mesure et de la peinture divergeraient au premier réglage
// retouché d'un seul côté — le joueur comparerait alors deux pantins qui ne se
// ressemblent plus.
//
// 🔴 RIEN NE TOUCHE AU PERSONNAGE. Le pantin vit sous des STYLES À PART, posés
// sur la garde-robe sous leurs propres identifiants ; l'acteur en scène garde
// celui que le serveur lui a donné. C'est ce qui rend l'essai gratuit — tout
// dérégler puis fermer ne laisse aucune trace.
class StyleDoll {
 public:
  // ⚠ LA GARDE-ROBE DOIT VIVRE PLUS LONGTEMPS que le pantin, qui y pose ses
  // styles et y compose.
  explicit StyleDoll(ro::render::ActorWardrobe& wardrobe);

  // ⚠ NI COPIE NI DÉPLACEMENT : le dernier dessin se désigne par adresse.
  StyleDoll(const StyleDoll&) = delete;
  StyleDoll& operator=(const StyleDoll&) = delete;

  // LES PIÈCES DE CE CORPS, et la base sur laquelle elles se règlent.
  //
  // 🔴 REMESURÉES QUAND LE CORPS OU LA TEINTE DE BASE CHANGENT — deux témoins,
  // pas un. Le corps décide du sprite, la teinte de base de la palette sur
  // laquelle il est fusionné, donc du découpage des rampes. `look` porte déjà
  // ce que la recette impose ; `body_key` est celle de l'apparence nue.
  const ro::render::ActorWardrobe::BodyRamps& Measure(const ro::render::ActorLook& look,
                                                      std::uint32_t body_key);
  // La dernière mesure.
  [[nodiscard]] const ro::render::ActorWardrobe::BodyRamps& body() const noexcept { return body_; }
  // Avance à chaque mesure neuve : ce qui se lit sur les pièces se refait avec
  // elle.
  [[nodiscard]] std::uint32_t generation() const noexcept { return generation_; }

  // CE QU'ON VEUT VOIR.
  struct Shot {
    // L'apparence mesurée, recette imposée : le corps seul, ou avec les pièces
    // dont on veut voir la teinture — tête, coiffes, cape.
    ro::render::ActorLook look;
    const ro::render::PaletteRecipe* recipe = nullptr;
    // La révision de la recette ET des teintures : leurs palettes ne se
    // repeignent qu'à son changement.
    std::uint32_t revision = 0;
    // La pièce du CORPS qui PULSE, ou moins un.
    int highlight = -1;
    std::uint32_t now_ms = 0;
    std::uint8_t direction = 0;
    // La pose, en rang du .act d'un joueur, et si elle défile.
    std::size_t animation = 0;
    bool animated = false;
    ro::render::VignetteBox box;
    // LES TEINTURES DE CALQUES à peindre avec la recette du corps, ou nulles :
    // celles que le joueur règle, ou celles reçues d'un code. Chacune se pose
    // sur la pièce de `look` qui porte sa clé, comme sur un acteur du monde.
    const std::vector<ro::render::LayerDye>* layers = nullptr;
    // LE CALQUE QU'ON REGARDE, qui décide du cadrage : la tête et les coiffes
    // en portrait, la cape de dos, le reste en pied dans `direction`.
    ro::render::StyleLayer focus = ro::render::StyleLayer::kBody;
  };

  // Le pantin, ou rien tant qu'il n'y a rien à montrer : pas de boîte, pas de
  // corps — ni, quand on regarde le corps, de pièce.
  //
  // 🔴 LA PULSATION GARDE SES PALIERS. La pièce survolée s'éclaircit par paliers
  // quantifiés ; chacun a son style et sa vignette, gardés tant que la révision
  // et la pièce ne changent pas. Repeindre et recomposer le pantin à chaque
  // palier le refaisait six fois par seconde pour rien.
  [[nodiscard]] const ro::render::IconTexture* Draw(const Shot& shot);

  // LA PIÈCE SOUS CE POINT DE LA BOÎTE, d'après le dernier dessin, ou rien.
  //
  // 🔴 ELLE NE COMPARE AUCUNE COULEUR : elle lit l'INDEX du pixel source, et sa
  // PARTIE. Un index de la tête cherché dans les pièces du corps allumerait une
  // pièce que le curseur ne désigne pas : seul le corps répond ici.
  [[nodiscard]] std::optional<std::size_t> RampUnder(glm::vec2 in_box, glm::vec2 shown);

  // CE QUE LA PIPETTE DÉSIGNE SOUS CE POINT, toutes pièces confondues : la
  // partie et l'index du pixel source, d'après le dernier dessin. C'est ce que
  // l'éditeur des calques cherche ensuite dans les rampes de la pièce.
  //
  // `shown` : la taille sur laquelle l'interface étire l'image du pantin. Voir
  // `VignetteSampleUnder` : la pipette lit le pixel AFFICHÉ.
  [[nodiscard]] std::optional<ro::render::PalettePick> PickUnder(glm::vec2 in_box, glm::vec2 shown);

  // Rend les styles et les textures. ⚠ ENTRE DEUX IMAGES : oublier un style
  // détruit les tenues qui le portaient, et une composition en cours les lirait
  // mortes.
  void Close();

  // L'APPUI DES EFFETS DE CHAPEAU sur le dernier dessin : son cadrage, le corps
  // composé, sa durée d'animation, le déplacement de sa tête et sa direction.
  // Rien sans dessin.
  struct EffectSeat {
    ro::render::VignetteFraming framing;
    const ro::render::ComposedActor* composed = nullptr;
    float animation_duration_ms = 0.0F;
    std::optional<glm::vec2> head_shift;
    std::size_t sprite_direction = 0;
  };
  [[nodiscard]] std::optional<EffectSeat> EffectSeatOf();

 private:
  // Les paliers de la pulsation.
  static constexpr std::size_t kPulseSteps = 4;

  // Une palette peinte, posée sous son propre style, et la vignette qui la
  // montre.
  struct Painted {
    std::uint32_t style = 0;
    std::optional<ro::render::ActorVignette> vignette;
  };

  // Oublie les styles peints — tous, ou ceux de la pulsation seulement.
  void ForgetPainted();
  void ForgetPulse();
  void Forget(Painted& painted);

  ro::render::ActorWardrobe* wardrobe_;

  ro::render::ActorWardrobe::BodyRamps body_;
  bool measured_ = false;
  std::uint32_t body_key_ = 0;
  std::int16_t measured_palette_ = -1;
  std::uint32_t generation_ = 0;

  // ⚠ UN STYLE NEUF À CHAQUE PALETTE : la garde-robe refuse de réécrire une
  // palette en place, parce qu'une tenue déjà composée en retient l'adresse.
  Painted plain_;
  std::array<Painted, kPulseSteps> pulse_;
  // Ce que ces palettes peignent : la mesure, la révision, la pièce.
  std::uint32_t painted_generation_ = 0;
  std::uint32_t painted_revision_ = 0;
  int pulse_ramp_ = -1;

  // Le dernier dessin, que la pipette interroge.
  Painted* shown_ = nullptr;
  ro::render::ActorLook shown_look_;
  std::uint8_t shown_direction_ = 0;
  // La pose dessinée : la pipette interroge la même image.
  std::size_t shown_animation_ = 0;
  std::optional<std::size_t> shown_held_;
  ro::render::VignetteBox shown_box_;
  float shown_elapsed_ms_ = 0.0F;
};

}  // namespace moonclient
