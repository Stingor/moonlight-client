#pragma once

#include <cstdint>

#include "ro/render/gl_context.h"
#include "ro/render/post_fx.h"
#include "ro/render/scene_post_process.h"
#include "ro/render/screen_tint.h"

#include "options/graphics_options.h"

namespace moonclient {

// CE QUI SE POSE ENTRE LA VILLE DE FOND ET L'INTERFACE : la cible de la scène
// et le post-traitement que le joueur règle, puis la vignette de la mise en
// scène.
//
// 🔴 L'ORDRE EST CELUI DE LA PARTIE : la scène se peint dans sa cible, se
// repeint dans la fenêtre à travers le post-traitement, et l'interface vient
// ensuite, droit dans la fenêtre — elle n'est pas étalonnée. La vignette, elle,
// appartient à la mise en scène : elle se pose sur l'image étalonnée, sous le
// formulaire.
//
// ⚠ SES PIÈCES SE MONTENT À LA PREMIÈRE IMAGE QUI A UNE VILLE À HABILLER, et
// non à l'ouverture de l'écran, où elles manqueraient à qui recoche « Ville en
// fond » en cours de route : rien ne monte le décor au démarrage quand la case
// est décochée. Le dessin les demande, et elles n'existent que s'il y a une
// ville.
//
// ⚠ UN ÉCHEC NE FERME PAS L'ÉCRAN : un pilote qui refuse un programme laisse
// une ville sans post-traitement ou sans vignette, pas un joueur sans
// connexion. Et rien ne se redemande — le pilote ne changera pas d'avis.
class BackdropLens {
 public:
  // Lie la cible où la ville se peint : celle de la scène quand la page
  // « Graphics » en demande une — MSAA, échelle de rendu, post-traitement —,
  // la fenêtre sinon.
  void Begin(ro::render::GlContext& gl, const GraphicsOptions& graphics);

  // Repeint la ville dans la fenêtre à travers le post-traitement lu par
  // `Begin`, puis pose la vignette. La fenêtre reste liée à la sortie : ce qui
  // se peint ensuite tombe à sa place.
  void Finish(ro::render::GlContext& gl, std::uint32_t now_ms);

 private:
  // Monte le post-traitement et la vignette, une fois.
  void EnsurePieces();

  ro::render::ScenePostProcess post_;
  ro::render::ScreenTint tint_;
  // Le post-traitement de l'image en cours, lu par `Begin` : la même image se
  // repeint avec les réglages qui l'ont ouverte.
  ro::render::PostFxSettings post_fx_;
  bool pieces_tried_ = false;
};

}  // namespace moonclient
