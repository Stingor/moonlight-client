#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/formats/bmp.h"

namespace ro::render {

// Texture OpenGL possédée.
class GlTexture {
 public:
  GlTexture() = default;
  GlTexture(const GlTexture&) = delete;
  GlTexture& operator=(const GlTexture&) = delete;
  GlTexture(GlTexture&& other) noexcept;
  GlTexture& operator=(GlTexture&& other) noexcept;
  ~GlTexture();

  // Comment la texture sera échantillonnée.
  //
  // AUCUN mode ne génère de mipmaps, et c'est le point : voir docs/adr/0008.
  // Les textures de RO sont des PLANCHES qui portent plusieurs motifs, et
  // moyenner quatre texels y mélange des motifs voisins qui n'ont rien à voir.
  enum class Filtering : std::uint8_t {
    // Une image qui se RÉPÈTE : l'eau, dont les coordonnées glissent.
    kSharp,
    // Un ATLAS dont les cases sont adressées une par une, sans répétition : au
    // bord, elle irait chercher l'autre extrémité de l'image.
    kAtlas,
    // Une IMAGE SEULE, adressée d'un bord à l'autre — une image d'animation.
    //
    // Comme l'atlas, elle ne se répète pas : à u = 0, le filtrage moyennerait
    // le premier texel avec le DERNIER, et peindrait au bord du quad un liseré
    // de la couleur d'en face. Comme la planche, elle porte une couleur clé, et
    // ses pixels transparents doivent recevoir la couleur opaque voisine.
    kSprite,
    // UNE PIÈCE D'INTERFACE : de l'art au PIXEL, qu'on étire.
    //
    // ⚠ LA TEXTURE SE POSE AU PLUS PROCHE, et c'est la seule. Le cadre d'une
    // fenêtre de RO est dessiné pixel par pixel, et le morceau du milieu d'une
    // barre de titre s'étire sur toute sa largeur : interpolé, il rend une
    // bouillie délavée où le client montre des lignes nettes. C'est aussi
    // pourquoi l'échelle d'interface reste un multiple lisible.
    //
    // 🔴 MAIS SOUS IMGUI, CE N'EST PAS LA TEXTURE QUI DÉCIDE. Son moteur de rendu
    // lie un OBJET SAMPLER au début de son rendu, LINÉAIRE sauf callback
    // contraire — voir la grille du grimoire —, et un sampler lié écrase le
    // filtrage de la texture. Ses bords transparents se remplissent donc comme
    // ceux d'un sprite.
    //
    // Comme le sprite, elle porte une couleur clé et ne se répète pas.
    kPixelArt,
    // Une BMP du sol ou du décor, telle que le jeu la téléverse : bornée à ses
    // bords, et NOIRE là où elle est vide — voir `formats::ApplyTextureColorKey`.
    // Rien ne repeint ces texels : c'est de ce noir que le filtrage tire l'ombre
    // fine du bord d'une découpe.
    kKeyedImage,
  };

  // LE TÉLÉVERSEMENT REMPLIT-IL LES BORDS TRANSPARENTS ? Tous les modes, sauf
  // l'atlas, qui n'a pas de couleur clé, et la BMP du sol ou du décor, dont le
  // noir est voulu. Voir `BleedTransparentEdges`.
  [[nodiscard]] static constexpr bool FillsTransparentEdges(Filtering filtering) noexcept {
    return filtering != Filtering::kAtlas && filtering != Filtering::kKeyedImage;
  }

  [[nodiscard]] static core::Result<GlTexture> Upload(const formats::Image& image,
                                                      Filtering filtering = Filtering::kSharp);

  // Texture unie d'un seul pixel : sert de remplacement lorsqu'une texture du
  // GND est absente ou illisible, pour que le reste de la carte s'affiche
  // quand même au lieu de disparaître.
  [[nodiscard]] static core::Result<GlTexture> Placeholder();

  // Le plus fort filtrage anisotrope que le pilote accepte ; 1 sans extension
  // qui le porte. Un contexte OpenGL doit être courant.
  [[nodiscard]] static int MaxAnisotropy();

  void Bind(int unit) const;
  [[nodiscard]] bool valid() const noexcept { return texture_ != 0; }

  // LES NIVEAUX RÉDUITS, générés à la demande du décor 3D quand le joueur les
  // choisit — jamais au téléversement : voir docs/adr/0008.
  //
  // ⚠ LE FILTRAGE DE LA TEXTURE NE CHANGE PAS : c'est l'échantillonneur lié par
  // celui qui la lit qui décide de s'en servir.
  void GenerateMipmaps();

  // LE NOM OPENGL DE LA TEXTURE, et il ne sort que pour une raison : ImGui
  // dessine lui-même les pièces d'interface, et il lui faut l'identifiant. Rien
  // d'autre n'a à le lire — le rendu du monde passe par `Bind`.
  [[nodiscard]] unsigned int name() const noexcept { return texture_; }

 private:
  explicit GlTexture(unsigned int texture) noexcept : texture_(texture) {}

  unsigned int texture_ = 0;
};

}  // namespace ro::render
