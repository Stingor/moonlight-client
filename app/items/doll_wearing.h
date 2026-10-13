#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/sprite_vignette.h"
#include "ro/sim/actor.h"

namespace moonclient {

// CE QUE PORTE LE PORTEUR FACTICE D'UN PANTIN, et où il se tient : ses effets
// de chapeau, le métier que le joueur montre, sa monture, sa carte. Voir
// `DollEffects`.
//
// 🔴 TOUT CHANGEMENT REPART DE ZÉRO : un effet ajouté ou retiré ne se greffe pas
// sur ceux qui jouent, le porteur renaît avec l'ensemble neuf — comme un autre
// rang survolé, une autre carte, une monture prise ou quittée, un autre corps
// montré. Le métier montré place l'ancre d'un chapeau et la taille du corps :
// un porteur qui garderait celui de sa naissance laisserait l'aura à la
// hauteur d'un corps à pied sous un joueur monté.
struct DollWearing {
  // ⚠ RANGÉS ET SANS DOUBLON : deux ensembles égaux se comparent alors champ
  // à champ.
  std::vector<ro::sim::HatEffectId> worn;
  // Le métier que le joueur MONTRE — monture, costume — : voir `ShownJobOf`.
  std::uint32_t shown_job = 0;
  bool mounted = false;
  std::string map_name;

  [[nodiscard]] bool operator==(const DollWearing&) const = default;
};

// CE QUE LE PORTEUR DOIT PORTER POUR DOUBLER `player` sur cette carte : ces
// effets — déjà réduits à ceux qui se montrent —, le métier qu'il montre, et
// sa monture du shop, que l'ancre d'un chapeau lit.
[[nodiscard]] DollWearing WearingOf(const ro::sim::Actor& player,
                                    std::vector<ro::sim::HatEffectId> worn,
                                    std::string_view map_name);

// LES EFFETS DE CHAPEAU QUE LE PORTRAIT DE STATUT MONTRE : tous ceux que le
// joueur porte, comme la partie les montre sur lui. Aucun sur une classe
// dessinée en modèle 3D : le portrait n'a pas de sprite où les poser.
[[nodiscard]] std::vector<ro::sim::HatEffectId> PortraitHatEffects(const ro::sim::Actor& self,
                                                                   bool drawn_as_model);

// LA TOILE OÙ L'EFFET D'UN PANTIN SE PEINT, autour de sa boîte.
//
// La boîte est en pixels d'interface, ceux mêmes du cadrage du pantin
// (`VignetteFraming`) : c'est ce qui fait tomber l'effet sur le personnage.
// La marge élargit la toile de chaque côté, en boîtes : une aura déborde
// l'aperçu d'une pièce, quand le portrait coupe au bord de son cadre.
struct DollCanvas {
  glm::vec2 box_px{0.0F, 0.0F};
  float margin_boxes = 0.0F;

  [[nodiscard]] bool usable() const noexcept { return box_px.x > 0.0F && box_px.y > 0.0F; }
  // La marge d'un côté, en pixels d'interface.
  [[nodiscard]] glm::vec2 margin_px() const noexcept { return box_px * margin_boxes; }
  // La toile entière, marges comprises.
  [[nodiscard]] glm::vec2 size_px() const noexcept;
};

// LE CADRAGE DU PANTIN REPORTÉ SUR SA TOILE : même échelle, pieds décalés de la
// marge. C'est lui que la caméra de l'effet suit — voir `DollCamera`.
[[nodiscard]] ro::render::VignetteFraming OnDollCanvas(const ro::render::VignetteFraming& framing,
                                                       const DollCanvas& canvas);

}  // namespace moonclient
