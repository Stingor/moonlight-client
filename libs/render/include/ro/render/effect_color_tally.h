#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>

#include "ro/formats/image.h"
#include "ro/render/effect_dye.h"

namespace ro::render {

// LA COULEUR D'ORIGINE DE CE QU'UN EFFET PEINT : la moyenne de ses texels,
// pesés par leur opacité et leur éclat, teints par la couleur de leur quad ou de
// leurs clés.
//
// Elle sert deux fois : sur le fichier d'un effet, couche par couche, et sur ce
// que le renderer peint vraiment pour un effet dont les textures vivent dans le
// code. Là, seuls comptent les quads que marque la teinture `probe` : celui qui
// mesure en pose une que nul autre effet ne porte. La teinture elle-même n'entre
// pas dans la moyenne : c'est la couleur d'ORIGINE qu'on cherche.
class EffectColorTally {
 public:
  EffectDye probe;

  // Ajoute l'image, teinte de `tint` (rouge, vert, bleu, opacité), à la moyenne.
  void Add(const formats::Image& image, std::array<std::uint8_t, 4> tint);
  void Add(const formats::Image& image, const std::array<float, 4>& tint);

  // Vrai quand ce quad, de cette teinture, doit être compté.
  [[nodiscard]] bool Counts(const std::optional<EffectDye>& dye) const {
    return dye.has_value() && *dye == probe;
  }

  // ⚠ PORTÉE À PLEIN ÉCLAT : une moyenne de halos sur fond noir serait presque
  // noire, et l'on cherche une couleur. Rien tant que rien de coloré n'a compté.
  [[nodiscard]] std::optional<std::array<std::uint8_t, 3>> Color() const;

  // Les images où quelque chose a compté depuis la remise à zéro.
  [[nodiscard]] std::uint32_t images_counted() const noexcept { return images_counted_; }
  void NoteImage() noexcept { ++images_counted_; }

  // Repart de zéro, pour cette sonde.
  void Reset(const EffectDye& new_probe);

 private:
  // LA MOYENNE D'UNE TEXTURE, calculée une fois : un quad la repeint à chaque
  // image, et la parcourir à chaque fois coûterait sa taille.
  struct ImageMean {
    std::array<double, 3> sum = {};
    double weight = 0.0;
  };
  const ImageMean& MeanOf(const formats::Image& image);

  std::map<const formats::Image*, ImageMean> means_;
  std::array<double, 3> sum_ = {};
  double weight_ = 0.0;
  std::uint32_t images_counted_ = 0;
};

}  // namespace ro::render
