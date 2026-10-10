#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <imgui.h>

#include "ro/render/camera.h"
#include "ro/render/effect_catalog.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/ground_quad_renderer.h"
#include "ro/render/sprite_renderer.h"
#include "ro/render/sprite_vignette.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"
#include "ro/ui/description_window.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "world/map_stage.h"

namespace moonclient {

// L'EFFET DE CHAPEAU SUR LE PANTIN D'UN APERÇU : l'aura d'un costume, sa pluie
// d'or, son anneau — ce que la pièce montre en plus de son sprite, ou à sa place.
//
// 🔴 LES MÊMES PIÈCES QUE LA PARTIE, dans un monde à part. Un porteur factice,
// posé à la case du joueur, porte l'effet ; le moteur d'effets le sème par
// `NoticeHatEffects` comme sur n'importe qui, et `DrawEffectLayers` en a l'ordre
// de peinture. Les deux familles passent donc par là — le fichier `.str` comme
// l'effet dessiné en code —, sans rien toucher au vrai personnage ni au monde du
// jeu : le porteur n'existe que dans ce monde-ci.
//
// 🔴 ET LA CAMÉRA TOMBE SUR LE PANTIN : un pixel de sprite vaut un septième
// d'unité, pour un effet comme pour un corps, et le cadrage du pantin dit de
// combien il a grandi et où sont ses pieds. Voir `effects/doll_camera.h`.
//
// ⚠ LA PEINTURE SE FAIT PAR UN RAPPEL DE L'INTERFACE, pas ici : voir
// `ro::ui::DollOverlay`.
//
// ⚠ NI DÉPLACÉ NI COPIÉ : le moteur d'effets garde l'adresse du catalogue et de
// la troupe, et le rappel celle de l'objet.
class DollEffects {
 public:
  explicit DollEffects(ro::vfs::Vfs& vfs);
  DollEffects(const DollEffects&) = delete;
  DollEffects& operator=(const DollEffects&) = delete;
  DollEffects(DollEffects&&) = delete;
  DollEffects& operator=(DollEffects&&) = delete;
  ~DollEffects() = default;

  // FAIT VIVRE L'EFFET DE CE RANG sur le pantin de `doll`, et y pose le rappel
  // qui le peindra. Rien sans rang, sans pantin ou sans joueur : l'effet d'avant
  // s'oublie alors.
  //
  // ⚠ APRÈS LE PANTIN, dont `framing` est le cadrage : c'est lui qui efface
  // chaque image le rappel de la précédente.
  void Advance(ro::ui::DescriptionDollState& doll, std::int16_t ordinal,
               const ro::render::VignetteFraming& framing, const ro::sim::World& world,
               const MapStage& stage, std::uint32_t now_ms);

  // Oublie ce qui joue : plus aucun pantin ne montre d'effet.
  void Rest();
  // OUBLIE LES TEXTURES téléversées : le skin a changé, et un même chemin
  // désigne une autre image. ⚠ ENTRE DEUX IMAGES.
  void ForgetTextures();

 private:
  // Le rappel de l'interface, et ce qu'il fait une fois son peintre retrouvé.
  static void Paint(const ImDrawList* list, const ImDrawCmd* command);
  void PaintInto(const ro::ui::DollOverlayCall& call);
  // Jette ce que la partie entendrait ou appliquerait : un pantin est muet, et
  // son porteur n'existe pas.
  void Drain();

  ro::render::EffectCatalog catalog_;
  EffectNodesInPlay nodes_;
  EffectsInPlay effects_;
  // Sans renderer — un pilote qui refuse le programme —, le pantin se montre
  // sans effet, comme la partie.
  std::optional<ro::render::EffectRenderer> effect_renderer_;
  std::optional<ro::render::SpriteRenderer> sprites_;
  std::optional<ro::render::GroundQuadRenderer> ground_quads_;
  // LE MONDE DU PORTEUR, et son planificateur : sans grille, il ne marche pas.
  ro::sim::World stand_;
  ro::sim::WalkPlanner planner_{nullptr};
  ro::render::Camera camera_;
  // Ce que le moteur a rendu à cette image, peint par le rappel.
  std::span<const ro::render::DrawnEffect> played_;
  // La carte où le porteur se tient, lue au moment de peindre les cases de
  // terrain. Nulle au repos.
  const MapStage* stage_ = nullptr;
  // Le rang porté, zéro au repos, et la carte où il a été posé : l'un ou
  // l'autre qui change repart de zéro.
  std::int16_t worn_ = 0;
  std::string map_name_;
};

}  // namespace moonclient
