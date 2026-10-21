#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/render/gl_program.h"
#include "ro/render/post_fx.h"
#include "ro/render/spin_blur.h"

namespace ro::render {

// OÙ LA SCÈNE SE PEINT, par rapport à la sortie.
struct SceneTargetLook {
  // Les échantillons par pixel de l'anticrénelage matériel. Zéro ou un : aucun.
  // Bornés à ce que la carte graphique accepte.
  int samples = 0;
  // La taille de la scène, en part de celle de la sortie. Au-dessus de un, elle
  // se peint plus grande puis se réduit — du suréchantillonnage ; en dessous,
  // plus petite puis s'agrandit, pour soulager la carte graphique.
  float render_scale = 1.0F;
};

// LE MONDE PEINT À PART, PUIS REPEINT DANS LA SORTIE.
//
// Un FLOU DE ROTATION d'abord, le temps que la caméra tourne — voir
// `spin_blur.h`. Puis deux passes : la COULEUR —
// aberration, netteté, étalonnage, filtre, vignette, grain — puis, si on le
// demande, l'ANTICRÉNELAGE FXAA sur l'image qui en sort. Les formules sont
// celles de `post_fx.h`, et `post_fx_reference.h` en garde le témoin.
//
// 🔴 LA SORTIE EST LE TAMPON LIÉ AU MOMENT DE `Present` : la fenêtre en jeu,
// une cible hors écran dans `roc-cli`. Le monde se peint donc dans une cible
// qui lui appartient, et le post-traitement la repeint là où on le lui dit,
// à la taille de la zone d'affichage en place.
//
// ⚠ SANS RIEN À FAIRE, RIEN NE COÛTE : post-traitement éteint, caméra immobile,
// un échantillon, échelle un — la scène se peint droit dans la sortie, sans cible intermédiaire.
class ScenePostProcess {
 public:
  ScenePostProcess() = default;
  ScenePostProcess(const ScenePostProcess&) = delete;
  ScenePostProcess& operator=(const ScenePostProcess&) = delete;
  ScenePostProcess(ScenePostProcess&& other) noexcept;
  ScenePostProcess& operator=(ScenePostProcess&& other) noexcept;
  ~ScenePostProcess();

  [[nodiscard]] static core::Result<ScenePostProcess> Create();

  // Lie la cible de la scène, à la taille de la sortie multipliée par l'échelle.
  //
  // Rend FAUX quand la scène doit se peindre droit dans la sortie : rien à
  // faire, ou une cible que la carte graphique a refusée. Rien n'est alors lié,
  // et c'est à l'appelant de lier la sortie.
  //
  // `passes_wanted` : le post-traitement ou le flou de rotation ont à peindre
  // à cette image.
  bool Begin(int output_width, int output_height, const SceneTargetLook& look, bool passes_wanted);

  // Peint la scène dans le tampon lié, à travers le post-traitement, sur toute
  // sa zone d'affichage. Rien quand `Begin` a rendu faux.
  //
  // ⚠ LE TAMPON ET LA ZONE D'AFFICHAGE LIÉS À L'ENTRÉE SONT CEUX DE LA SORTIE,
  // et ils le restent à la sortie : ce qui se peint ensuite — noms, jauges,
  // interface — tombe à sa place.
  void Present(const PostFxSettings& settings, std::uint32_t now_ms,
               const SpinBlurFrame& spin = {});

  // Le plus d'échantillons par pixel que la carte graphique accepte.
  [[nodiscard]] int max_samples() const noexcept { return max_samples_; }

 private:
  void Release() noexcept;
  void ReleaseScene() noexcept;
  // Prépare la cible de la scène à cette taille et à ce nombre d'échantillons.
  bool EnsureScene(int width, int height, int samples);
  // Prépare l'image intermédiaire entre les deux passes, à la taille de la sortie.
  bool EnsureChain(int width, int height);
  // Prépare l'image du flou de rotation, à la taille de la scène.
  bool EnsureSpin(int width, int height);
  // Dessine une passe plein écran, la source liée à l'unité zéro.
  void DrawFullScreen(unsigned int source) const;
  // Dessine le flou de rotation dans le tampon et la zone d'affichage liés.
  void DrawSpin(unsigned int source, const SpinBlurFrame& spin) const;

  GlProgram color_program_;
  GlProgram fxaa_program_;
  GlProgram spin_program_;
  unsigned int vertex_array_ = 0;

  // La scène : son tampon, sa couleur et sa profondeur. Multi-échantillonnée,
  // sa couleur est un tampon de rendu qu'une résolution recopie dans
  // `resolved_color_` ; sinon c'est déjà une texture.
  unsigned int scene_framebuffer_ = 0;
  unsigned int scene_color_texture_ = 0;
  unsigned int scene_color_buffer_ = 0;
  unsigned int scene_depth_buffer_ = 0;
  unsigned int resolve_framebuffer_ = 0;
  unsigned int resolved_color_ = 0;
  int scene_width_ = 0;
  int scene_height_ = 0;
  int scene_samples_ = 0;

  // L'image entre la passe de couleur et l'anticrénelage.
  unsigned int chain_framebuffer_ = 0;
  unsigned int chain_color_ = 0;
  int chain_width_ = 0;
  int chain_height_ = 0;

  // L'image du flou, quand le post-traitement la relit.
  unsigned int spin_framebuffer_ = 0;
  unsigned int spin_color_ = 0;
  int spin_width_ = 0;
  int spin_height_ = 0;

  int max_samples_ = 0;
  int max_size_ = 0;
  // Vrai entre un `Begin` qui a lié la scène et le `Present` qui la peint.
  bool active_ = false;
};

}  // namespace ro::render
