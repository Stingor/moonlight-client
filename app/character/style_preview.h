#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/render/actor_wardrobe.h"
#include "ro/render/palette_ramps.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/style_preview.h"

#include "character/style_doll.h"

namespace moonclient {

// L'APERÇU D'UN STYLE REÇU, VU DE LA PARTIE.
//
// 🔴 LA RECETTE SE POSE SUR NOTRE CORPS, jamais sur celui de l'expéditeur —
// que nous n'avons pas. Un code ne porte aucune couleur : il dit des décalages
// PAR RANG DE PIÈCE, et le rang trois d'un corps n'est pas celui d'un autre.
// « Ce que ça donnerait sur moi » est donc la seule question à laquelle on
// puisse répondre, et c'est aussi la seule que se pose celui qui clique.
//
// ⚠ IL A SON PROPRE PANTIN, donc ses propres styles dans la garde-robe : les
// deux fenêtres peuvent être ouvertes en même temps sur deux recettes
// différentes, et un style partagé ferait porter à l'une la palette de l'autre.
// Le dessin, lui, est celui de l'éditeur — voir `StyleDoll`.
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

  // Rend les styles d'aperçu à la garde-robe. ⚠ À LA FERMETURE : un style
  // qu'on oublie détruit les tenues qui le portaient, et une composition en
  // cours les lirait mortes.
  void Close();

 private:
  ro::render::ActorWardrobe* wardrobe_ = nullptr;
  std::optional<StyleDoll> doll_;

  ro::render::PaletteRecipe recipe_;
  // Avance à chaque code ouvert : le pantin se repeint avec.
  std::uint32_t revision_ = 0;
  std::string code_;

  ro::ui::StylePreviewView view_;
};

}  // namespace moonclient
