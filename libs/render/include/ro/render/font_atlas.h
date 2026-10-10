#pragma once

#include <cstdint>
#include <filesystem>
#include <unordered_map>

#include <glm/glm.hpp>

#include "ro/core/error.h"
#include "ro/formats/bmp.h"

namespace ro::render {

// Une police gravée UNE fois dans une image, et ce qu'il faut pour la poser.
//
// POURQUOI GRAVER D'AVANCE
// Un glyphe se dessine en plaquant un morceau d'image ; il faut donc que le
// morceau soit dans une texture. Graver à la demande ferait grandir la texture
// en cours de partie, et obligerait à la reverser à la carte graphique au
// milieu d'une image. Les caractères qu'un nom de personnage emploie sont
// connus d'avance : on les grave tous au chargement, et l'image ne bouge plus.
//
// ⚠ CE QUI EST COUVERT. L'ASCII imprimable et le supplément latin-1 — de quoi
// écrire un nom français accentué. Un caractère hors de cette plage n'est pas
// dessiné ; les alphabets coréen et japonais demanderaient une gravure à la
// demande, donc un atlas qui grandit, et ce n'est pas encore le cas.
class FontAtlas {
 public:
  // Où va un glyphe, et de combien il fait avancer l'écriture.
  struct Glyph {
    // Sa place dans l'image, en pixels.
    glm::vec2 min{0.0F, 0.0F};
    glm::vec2 max{0.0F, 0.0F};
    // Où poser son coin haut-gauche par rapport au point d'écriture, qui est
    // sur la LIGNE DE BASE.
    glm::vec2 bearing{0.0F, 0.0F};
    // De combien le point d'écriture avance ensuite.
    float advance = 0.0F;
  };

  // Grave une police à cette hauteur, en pixels.
  //
  // Échoue si le fichier n'est pas une police lisible : l'appelant s'en passe
  // alors, plutôt que d'afficher des rectangles.
  [[nodiscard]] static core::Result<FontAtlas> Load(const std::filesystem::path& font_path,
                                                    int pixel_height);

  // Le glyphe d'un point de code, ou nul s'il n'a pas été gravé.
  [[nodiscard]] const Glyph* GlyphFor(char32_t code) const;

  // L'image des glyphes : du blanc dont seule l'opacité varie, pour qu'une
  // teinte lui donne sa couleur.
  [[nodiscard]] const formats::Image& image() const noexcept { return image_; }

  // De combien descendre pour écrire la ligne suivante, et de combien la police
  // monte au-dessus de la ligne de base.
  [[nodiscard]] float line_height() const noexcept { return line_height_; }
  [[nodiscard]] float ascent() const noexcept { return ascent_; }

 private:
  formats::Image image_;
  std::unordered_map<char32_t, Glyph> glyphs_;
  float line_height_ = 0.0F;
  float ascent_ = 0.0F;
};

}  // namespace ro::render
