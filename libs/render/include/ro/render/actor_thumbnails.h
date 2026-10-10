#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include "ro/render/actor_wardrobe.h"
#include "ro/render/icon_texture.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_vignette.h"

namespace ro::render {

// LES VIGNETTES D'UNE GRILLE DE CHOIX : beaucoup, petites, et FIGÉES.
//
// ⚠ CE N'EST PAS `ActorVignette`, ET LA DIFFÉRENCE N'EST PAS LA TAILLE. Celle-ci
// montre UN personnage qui bouge, et toute sa machinerie sert à refaire la
// texture quand l'image change. Une case de grille, elle, montre un choix : sa
// pose est immobile, son apparence ne bougera jamais, et elle se compose UNE
// FOIS pour toutes. Réemployer l'autre reviendrait à payer une détection de
// changement par case et par image, pour des images qui ne changent pas.
//
// 🔴 ET SURTOUT : IL Y EN A DES CENTAINES. Deux cent cinquante et une couleurs
// de cheveux, cinq cent cinquante-trois teintes de vêtement. Les composer d'un
// coup ouvrirait autant de sprites dans la même image, ce qui se voit — d'où le
// BUDGET par image, qui les étale sur quelques images sans que personne ne
// l'aperçoive.
class ActorThumbnails {
 public:
  // La garde-robe est passée par référence et doit vivre plus longtemps : les
  // vignettes s'y composent au fil du jeu, pas une fois pour toutes.
  explicit ActorThumbnails(ActorWardrobe& wardrobe) : wardrobe_(&wardrobe) {}

  // CE QUE CETTE PIÈCE MONTRE.
  //
  // ⚠ UNE SEULE PIÈCE, jamais une silhouette : une case de grille fait trente
  // pixels, où un personnage entier réduit sa tête à cinq. Voir
  // `ActorWardrobe::HeadOnly` et `BodySwatch`.
  enum class Subject : std::uint8_t { kHead, kBody };

  struct Cell {
    Subject subject = Subject::kHead;
    // L'apparence dont la pièce est tirée : la coupe et la teinte de cheveux
    // pour une tête, la classe et le sexe pour un corps.
    ActorLook look;
    // La teinte de vêtement officielle à montrer ; lue pour un corps
    // seulement.
    std::int16_t palette = 0;
    VignetteBox box;
  };

  // Combien de vignettes NEUVES cette image accepte de composer.
  //
  // 🔴 À APPELER UNE FOIS PAR IMAGE, sans quoi le budget ne se recharge jamais
  // et la grille se fige sur ses premières cases.
  void BeginFrame(int budget) noexcept { budget_ = budget; }

  // La vignette de cette case.
  //
  // Inutilisable dans trois cas qu'il ne faut pas confondre, et que l'appelant
  // traite pourtant de la même façon — une case vide :
  //   * le budget de l'image est épuisé : elle viendra à l'image suivante ;
  //   * l'installation n'a pas ce sprite : tous les numéros n'existent pas,
  //     et la case restera vide pour de bon ;
  //   * la composition n'a rien donné.
  [[nodiscard]] const IconTexture& Get(const Cell& cell);

  // Jette tout. ⚠ À LA FERMETURE DE LA GRILLE : les textures d'une grille
  // refermée ne servent plus à rien, et trois grilles gardées en même temps
  // tiendraient huit cent quatre-vingt-quatre images.
  //
  // ⚠ ENTRE DEUX IMAGES, jamais au milieu d'un dessin : l'interface tient les
  // identifiants de texture rendus.
  void Clear();

  [[nodiscard]] std::size_t loaded_count() const noexcept { return drawn_.size(); }

 private:
  // ⚠ L'ORDRE TOTAL DE LA GARDE-ROBE pour l'apparence, et des champs pour le
  // reste : on ne hache pas pour comparer, deux cases confondues montreraient
  // une coiffure pour une autre.
  struct Before {
    [[nodiscard]] bool operator()(const Cell& left, const Cell& right) const noexcept;
  };

  ActorWardrobe* wardrobe_;
  std::map<Cell, IconTexture, Before> drawn_;
  int budget_ = 0;
  // Ce que rendent les trois cas d'absence. ⚠ UNE SEULE, ET VIDE : la rendre
  // par référence demande qu'elle survive à l'appel.
  IconTexture nothing_;
};

}  // namespace ro::render
