#pragma once

#include <map>
#include <span>
#include <string>

#include "ro/core/error.h"
#include "ro/formats/image.h"
#include "ro/render/camera.h"
#include "ro/render/gl_program.h"
#include "ro/render/gl_texture.h"
#include "ro/render/ground_quad.h"
#include "ro/render/scene_fog.h"

namespace ro::render {

// UN LOT DE QUADS DE SOL et l'image qui les peint.
//
// Le chemin dans l'archive IDENTIFIE la texture, comme pour les quads du monde :
// deux marques qui partagent une image partagent son téléversement.
struct DrawnGroundQuads {
  std::span<const GroundQuad> quads;
  const std::string* texture_path = nullptr;
  const formats::Image* texture = nullptr;
};

// Dessine des quads COUCHÉS DANS LE MONDE, texturés et teintés.
//
// C'est le seul chemin de rendu de cette sorte : le terrain a le sien, et tout
// le reste est sprite ou panneau face à la caméra. Les marques de sol y
// passent, et les surfaces posées à plat qui viennent après elles.
//
// ⚠ DEUX FAÇONS DE LUI DONNER SON IMAGE, et ce n'est pas une hésitation. Une
// marque FIXE — le curseur au sol — pose la sienne une fois par `UseTexture` et
// n'en changera jamais ; un effet, lui, nomme la sienne dans une table et il y
// en aura d'autres. Le second chemin passe donc par des LOTS, chacun avec son
// chemin d'archive, et le renderer retient ce qu'il a déjà téléversé.
//
// ⚠ À dessiner APRÈS le terrain et le décor, AVANT les acteurs. La marque
// couvre le sol, et qui se tient dessus la couvre à son tour : c'est
// l'épreuve de profondeur qui le fait, et elle a besoin des deux dans cet
// ordre.
class GroundQuadRenderer {
 public:
  GroundQuadRenderer() = default;
  GroundQuadRenderer(const GroundQuadRenderer&) = delete;
  GroundQuadRenderer& operator=(const GroundQuadRenderer&) = delete;
  GroundQuadRenderer(GroundQuadRenderer&& other) noexcept;
  GroundQuadRenderer& operator=(GroundQuadRenderer&& other) noexcept;
  ~GroundQuadRenderer();

  [[nodiscard]] static core::Result<GroundQuadRenderer> Create();

  // Téléverse la texture du lot. Un second appel remplace la première.
  [[nodiscard]] core::Result<void> UseTexture(const formats::Image& image);

  // Vrai quand il y a de quoi peindre. Un renderer construit par défaut — celui
  // que garde une structure d'accessoires quand le fichier manque — rend faux,
  // et Draw ne fait rien : l'absence d'une marque n'empêche pas de jouer.
  [[nodiscard]] bool ready() const noexcept { return texture_.valid(); }

  void Draw(const Camera& camera, std::span<const GroundQuad> quads, float aspect_ratio,
            const SceneFog& fog = SceneFog());

  // LES LOTS, chacun avec son image. Un lot dont la texture ne se téléverse pas
  // est sauté ; les autres se peignent.
  //
  // ⚠ IL NE PASSE PAS PAR `ready()` : celui-ci parle de l'image UNIQUE, que les
  // lots n'emploient pas. Un renderer sans image fixe peint des lots sans rien
  // dire, et c'est bien ce qu'on veut.
  void Draw(const Camera& camera, std::span<const DrawnGroundQuads> lots, float aspect_ratio,
            const SceneFog& fog = SceneFog());

  // OUBLIE LES TEXTURES rangées par chemin : le skin a changé, et un même
  // chemin désigne une autre image. Elles se rechargent à leur prochain dessin.
  void ForgetTextures() { textures_.clear(); }

 private:
  void Release() noexcept;

  // Peint des sommets déjà bâtis, sous la texture donnée. Le corps commun aux
  // deux `Draw` : les séparer laisserait les états de mélange diverger.
  void Paint(const Camera& camera, std::span<const GroundQuad> quads, const GlTexture& texture,
             float aspect_ratio, const SceneFog& fog);

  // La texture d'un chemin, téléversée une fois. Rend nullptr quand l'image ne
  // monte pas — l'échec se retient aussi, sans quoi on retenterait à chaque
  // image.
  [[nodiscard]] const GlTexture* TextureFor(const std::string& path, const formats::Image& image);

  GlProgram program_;
  GlTexture texture_;
  std::map<std::string, GlTexture, std::less<>> textures_;
  unsigned int vertex_array_ = 0;
  unsigned int vertex_buffer_ = 0;
};

}  // namespace ro::render
