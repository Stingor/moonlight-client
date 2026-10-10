#pragma once

#include "ro/core/error.h"
#include "ro/formats/image.h"
#include "ro/render/offscreen_target.h"

namespace ro::render {

// ── UN RECTANGLE DE L'ÉCRAN, RELU ET RÉDUIT ─────────────────────────────────
//
// 🔴 CE QU'IL PREND CONTIENT NOTRE INTERFACE, et c'est tout son objet. La
// relecture a lieu à la FIN de l'image, une fois les fenêtres dessinées et avant
// de présenter : montrer une manipulation, c'est montrer les fenêtres dans
// lesquelles elle se fait.
//
// ⚠ CONSÉQUENCE DIRECTE, ET NON NÉGOCIABLE : tout ce qu'on dessine pendant un
// enregistrement atterrit dans les images. Le cadre de la zone et le témoin se
// tiennent donc EN DEHORS d'elle — voir `zone_capture.h`.
//
// 🔴 LA RÉDUCTION SE FAIT SUR LA CARTE, PAS DANS LE PROCESSEUR. Relire une zone
// de 1 200 sur 700 pour la ramener à 640 de large coûterait trois millions de
// pixels traversant le bus à chaque prise, puis un rééchantillonnage ; le
// transfert d'image à image le fait gratuitement, et ne rend que ce qu'on garde.

// Un rectangle de l'écran, en pixels du TAMPON DE DESSIN.
//
// 🔴 RANGÉE 0 EN HAUT, comme partout ailleurs dans ce dépôt — et c'est
// l'inverse d'OpenGL, dont l'origine est en BAS. La conversion se fait une fois,
// ici ; la laisser à l'appelant donnerait une capture à l'envers un jour sur
// deux.
struct ScreenRegion {
  int left = 0;
  int top = 0;
  int width = 0;
  int height = 0;

  [[nodiscard]] bool operator==(const ScreenRegion&) const noexcept = default;
  [[nodiscard]] bool empty() const noexcept { return width <= 0 || height <= 0; }
};

class ScreenGrab {
 public:
  // `width` et `height` sont la taille de SORTIE : celle des images rendues, pas
  // celle de la zone lue.
  [[nodiscard]] static core::Result<ScreenGrab> Create(int width, int height);

  // Relit `zone` de l'écran et la rend à la taille de sortie.
  //
  // `drawable_height` est la hauteur du tampon de dessin — celle que le système
  // donne, qui n'est pas forcément celle qu'on a demandée à la fenêtre.
  [[nodiscard]] core::Result<formats::Image> Take(const ScreenRegion& zone,
                                                  int drawable_height) const;

  [[nodiscard]] int width() const noexcept { return target_.width(); }
  [[nodiscard]] int height() const noexcept { return target_.height(); }

 private:
  explicit ScreenGrab(OffscreenTarget target) : target_(std::move(target)) {}

  OffscreenTarget target_;
};

}  // namespace ro::render
