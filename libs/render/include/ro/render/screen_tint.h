#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/render/gl_program.h"

namespace ro::render {

// LA TEINTE D'ÉCRAN : vignette, noir et blanc, grain.
//
// Une passe qui relit l'image déjà peinte et la repeint par-dessus. Elle
// n'existe que pour l'écran de veille, et passe sur l'image ENTIÈRE — noms et
// jauges compris —, là où le post-traitement du joueur (`scene_post_process.h`)
// ne voit que le monde : d'où une classe à part, qui ne porte que ses trois
// réglages.
//
// 🔴 LES FORMULES SONT CELLES DU POST-TRAITEMENT (`post_fx.h`), terme à terme :
// la luminance en 0,299 / 0,587 / 0,114, la vignette en `smoothstep` de 0,8 à
// 0,35 sur la distance au centre EN COORDONNÉES DE TEXTURE, le grain en
// `fract(sin(...) * 43758,5453)` centré puis multiplié par 0,15. D'autres
// formules donneraient une veille qui s'assombrit autrement — et « autrement »
// ne se remarque qu'en côte à côte, c'est-à-dire jamais.
//
// ⚠ ELLE NE TEINTE QUE CE QUI EST DÉJÀ PEINT, donc le MONDE si on l'appelle
// avant l'interface. C'est sa place : l'horloge de la veille, peinte ensuite,
// n'est ni assombrie ni grenée. Elle doit rester lisible : c'est tout ce qui
// reste à l'écran.

// CE QU'UNE TEINTE VAUT. Chacun de zéro à un ; tous à zéro = rien à faire.
struct ScreenTintLook {
  // Les bords s'assombrissent. Zéro = aucune.
  float vignette = 0.0F;
  // Un = noir et blanc complet.
  //
  // 🔴 PAR LA SATURATION ET NON PAR UN FILTRE « niveaux de gris » : un filtre est un interrupteur,
  // il ne se fond pas. Le noir et blanc doit s'installer avec la rampe.
  float desaturate = 0.0F;
  // Le grain argentique.
  float grain = 0.0F;

  // Y a-t-il quelque chose à peindre ? ⚠ UNE PASSE À ZÉRO N'EST PAS NEUTRE :
  // elle recopie l'image à travers un échantillonnage, ce qui coûte une image
  // pleine pour un résultat identique.
  [[nodiscard]] bool visible() const noexcept {
    return vignette > 0.0F || desaturate > 0.0F || grain > 0.0F;
  }
};

class ScreenTint {
 public:
  ScreenTint() = default;
  ScreenTint(const ScreenTint&) = delete;
  ScreenTint& operator=(const ScreenTint&) = delete;
  ScreenTint(ScreenTint&& other) noexcept;
  ScreenTint& operator=(ScreenTint&& other) noexcept;
  ~ScreenTint();

  [[nodiscard]] static core::Result<ScreenTint> Create();

  // Repeint la cible liée à travers la teinte.
  //
  // 🔴 ELLE COPIE AVANT DE REPEINDRE, et il n'y a pas moyen d'y couper. La
  // vignette seule se ferait par un fondu multiplicatif, et le grain par une
  // addition ; le NOIR ET BLANC, lui, a besoin de la luminance du pixel DÉJÀ
  // peint — aucune équation de mélange ne sait la calculer. Une passe qui lit
  // sa propre cible est interdite : on transfère donc l'image dans une texture,
  // puis on la redessine.
  //
  // `now_ms` fait bouger le grain d'une image à l'autre. Un grain figé n'est
  // pas du grain : c'est une salissure sur l'objectif.
  void Draw(const ScreenTintLook& look, int drawable_width, int drawable_height,
            std::uint32_t now_ms);

 private:
  void Release() noexcept;
  // Retaille la texture de copie si la fenêtre a changé de taille.
  [[nodiscard]] bool EnsureCopy(int width, int height);

  GlProgram program_;
  unsigned int vertex_array_ = 0;
  unsigned int framebuffer_ = 0;
  unsigned int copy_ = 0;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace ro::render
