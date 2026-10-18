#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/render/actor_vignette.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/palette_ramps.h"
#include "ro/sim/bank.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/style_preview.h"

namespace moonclient {

// L'APERÇU D'UN STYLE REÇU, VU DE LA PARTIE.
//
// 🔴 LA RECETTE SE POSE SUR NOTRE CORPS, jamais sur celui de l'expéditeur —
// que nous n'avons pas. Un code ne porte aucune couleur : il dit des décalages
// PAR RANG DE PIÈCE, et le rang trois d'un corps n'est pas celui d'un autre.
// « Ce que ça donnerait sur moi » est donc la seule question à laquelle on
// puisse répondre, et c'est aussi la seule que se pose celui qui clique.
//
// ⚠ IL A SON PROPRE STYLE DANS LA GARDE-ROBE, distinct de celui de l'éditeur :
// les deux fenêtres peuvent être ouvertes en même temps sur deux recettes
// différentes, et un identifiant partagé ferait porter à l'une la palette de
// l'autre.
class StylePreview {
 public:
  // La garde-robe peut manquer : une installation sans sprites n'a pas de
  // corps à peindre, et la fenêtre le dit plutôt que de laisser un carré vide.
  explicit StylePreview(ro::render::ActorWardrobe* wardrobe);

  // Ouvre l'aperçu sur ce code. Rend faux — sans rien ouvrir — si le code est
  // illisible ou d'une autre version.
  //
  // ⚠ LE CODE EST GARDÉ TEL QU'IL A VOYAGÉ : c'est lui qu'on recopiera et
  // qu'on relaiera, jamais une version ré-encodée.
  [[nodiscard]] bool Open(std::string_view code, std::string_view owner,
                          ro::ui::StylePreviewState& state);

  // Refait le pantin. À appeler une fois par image, AVANT le dessin, et
  // seulement fenêtre ouverte.
  void Refresh(const ro::sim::World& world, ro::ui::StylePreviewState& state);

  [[nodiscard]] const ro::ui::StylePreviewView& view() const noexcept { return view_; }

  // Le code que la fenêtre montre, pour qui veut l'essayer dans l'éditeur.
  [[nodiscard]] const std::string& code() const noexcept { return code_; }

  // Rend l'identifiant d'aperçu à la garde-robe. ⚠ À LA FERMETURE : un style
  // qu'on oublie détruit les tenues qui le portaient, et une composition en
  // cours les lirait mortes.
  void Close();

 private:
  ro::render::ActorWardrobe* wardrobe_ = nullptr;
  std::optional<ro::render::ActorVignette> doll_;

  ro::render::PaletteRecipe recipe_;
  std::string code_;

  // Les pièces du corps porté, et la base sur laquelle la recette s'applique.
  //
  // 🔴 LA BASE SE FUSIONNE AVEC LA TEINTE DU STYLE REÇU, pas avec la nôtre :
  // c'est elle qui décide de la fusion, donc du découpage des rampes.
  // Réutiliser notre base appliquerait les réglages reçus à côté.
  ro::render::ActorWardrobe::BodyRamps body_;
  bool measured_valid_ = false;
  std::uint32_t body_key_ = 0;
  std::int16_t measured_palette_ = -1;

  // L'identifiant du style d'aperçu. ⚠ UN NEUF À CHAQUE CHANGEMENT : la
  // garde-robe refuse de réécrire une palette en place, parce qu'une tenue
  // déjà composée en retient l'adresse.
  std::uint32_t preview_style_ = 0;
  bool painted_ = false;

  ro::ui::StylePreviewView view_;
};

}  // namespace moonclient
