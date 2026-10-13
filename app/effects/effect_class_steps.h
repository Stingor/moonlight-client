#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ro/render/effect_class.h"

// LE LISSAGE DES CLASSES D'EFFETS ENTRE DEUX TICS.
//
// Une instance de classe ne se copie pas : elle est polymorphe, et son état lui
// appartient. Ce qu'on garde d'elle est donc ce qu'elle DESSINE. Chaque image
// retient la sortie brute de chaque instance ; quand un tic tombe, la sortie
// de l'image d'avant devient celle du tic précédent, et la sortie de l'image
// se mêle à elle dans la part du tic écoulée — voir
// `ro/render/effect_step_blend.h` pour ce qui se mêle et ce qui ne se mêle pas.
//
// Une sortie se reconnaît par le NUMÉRO DE SON INSTANCE, donné à sa naissance,
// et par son RANG parmi les sorties de même sorte de cette instance. Une
// instance qui ne rend pas le même nombre de sorties d'une sorte aux deux tics
// ne se mêle pas pour cette sorte.
//
// ⚠ L'IMAGE D'UN FICHIER `.str` est discrète, mais une instance qui avance son
// fichier d'UNE image par tic montre le chemin depuis l'image d'avant : la part
// négative de `EffectLayerAt`, qui garde l'image de texture courante.
// ⚠ LA SORTIE D'AVANT LE TIC A ÉTÉ COMPOSÉE avec la caméra de son image, la
// dernière avant le tic : une image d'écart au plus.
namespace moonclient {

class EffectClassSteps {
 public:
  void Enable(bool on) noexcept { enabled_ = on; }
  [[nodiscard]] bool enabled() const noexcept { return enabled_; }

  // L'IMAGE COMMENCE, après ses tics : `ticks` battus, `alpha` la part du tic
  // écoulée depuis le dernier.
  void BeginFrame(std::uint32_t ticks, float alpha);

  // Ce qu'une instance ajoute à `output` entre ces deux appels est à elle.
  void OpenInstance(std::uint64_t serial, const ro::render::EffectClassOutput& output);
  void CloseInstance(const ro::render::EffectClassOutput& output);

  // Retient la sortie brute pour l'image suivante, puis la MÊLE à celle d'avant
  // le dernier tic. Ne fait rien lissage éteint.
  void Blend(ro::render::EffectClassOutput& output);

  void Forget();

 private:
  // Les sortes de sortie, et leur rang dans les bornes d'une instance.
  enum Kind : std::size_t { kStrs, kQuads, kSprites, kScreens, kKinds };
  struct InstanceSpan {
    std::uint64_t serial = 0;
    std::array<std::size_t, kKinds> first{};
    std::array<std::size_t, kKinds> end{};
  };
  // UNE SORTIE GARDÉE PORTE SES NOMS : ceux de la sortie montrent la mémoire de
  // l'instance, qui peut avoir changé à l'image suivante.
  struct KeptQuad {
    std::string texture;
    ro::render::EffectClassQuadDraw draw;
  };
  struct KeptSprite {
    std::string sprite;
    ro::render::EffectClassSpriteDraw draw;
  };
  struct KeptScreen {
    std::string texture;
    ro::render::EffectClassScreenDraw draw;
  };
  struct Kept {
    std::vector<InstanceSpan> spans;
    std::vector<ro::render::EffectClassStrDraw> strs;
    std::vector<KeptQuad> quads;
    std::vector<KeptSprite> sprites;
    std::vector<KeptScreen> screens;
  };

  [[nodiscard]] static std::array<std::size_t, kKinds> Sizes(
      const ro::render::EffectClassOutput& output);
  [[nodiscard]] static Kept Keep(const ro::render::EffectClassOutput& output,
                                 std::vector<InstanceSpan> spans);
  void BlendInstance(const InstanceSpan& now, const InstanceSpan& before,
                     ro::render::EffectClassOutput& output) const;

  bool enabled_ = false;
  bool has_last_ = false;
  bool has_previous_ = false;
  float alpha_ = 0.0F;
  std::vector<InstanceSpan> spans_;
  Kept last_;
  Kept previous_;
};

}  // namespace moonclient
