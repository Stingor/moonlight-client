#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/icon_texture.h"
#include "ro/render/portrait_framing.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/sprite_frame.h"
#include "ro/render/sprite_vignette.h"

namespace ro::render {

// LA VIGNETTE D'UN ACTEUR, TÉLÉVERSÉE : ce que l'interface pose comme une
// image, là où le monde pose des quads.
//
// ⚠ ELLE NE SE REFAIT QUE QUAND L'IMAGE CHANGE, et c'est tout l'intérêt de la
// classe : composer, écraser et téléverser à chaque image de jeu coûterait
// cent fois ce que la vignette vaut. Le témoin est la PROVENANCE des calques
// dessinés — quelle image de quel sprite, pour chacun —, comparée champ par
// champ à celle de la dernière fois.
//
// 🔴 CE TÉMOIN COUVRE LES COIFFES ANIMÉES. Un chapeau vit sur sa propre horloge
// pendant que le personnage tient sa pose : la provenance de SON calque change
// seule, et c'est ce qui redessine la vignette. Un témoin qui ne regarderait
// que l'image du corps le figerait.
class ActorVignette {
 public:
  // La garde-robe est passée par référence et doit vivre plus longtemps : la
  // vignette s'y recompose au fil du jeu, pas une fois pour toutes.
  explicit ActorVignette(ActorWardrobe& wardrobe) : wardrobe_(&wardrobe) {}

  // CE QU'ON VEUT VOIR.
  struct Pose {
    ActorLook look;
    // Le RANG d'animation, tel que le .act les range — voir `sprite_frame.h`.
    std::size_t animation = kAnimationIdle;
    // La direction du .ACT : zéro est la face tournée vers nous, et le tour se
    // fait dans le sens des aiguilles d'une montre. ⚠ CE N'EST PAS CELLE DE LA
    // BOUSSOLE du serveur, qui tourne dans l'autre sens en partant du nord.
    std::size_t direction = 0;
    // Vrai : l'animation défile. Faux : le CORPS tient son image zéro, et ce
    // qu'il porte continue de vivre — une coiffe animée bouge sur un
    // personnage immobile, comme en jeu.
    bool animated = false;
    float elapsed_ms = 0.0F;
    VignetteBox box;
    // L'IMAGE IMPOSÉE, pour tout l'acteur : elle l'emporte sur `animated`.
    // ⚠ PARTAGÉE PAR LE CORPS ET LA TÊTE, comme en jeu — c'est aussi par elle
    // que la tête se tourne. À l'appelant de la borner au nombre d'images.
    std::optional<std::size_t> held_frame;
    // LE CADRAGE DE PORTRAIT, ou rien pour la vignette en pied. Présent, il
    // remplace celui de la boîte : la vignette se cadre sur ce que la BOUCLE
    // ENTIÈRE de la pose peint de la tête et du corps, mesurée une fois par
    // pose. Voir `portrait_framing.h`.
    std::optional<PortraitAim> aim;
  };

  // La vignette de cette pose. Inutilisable tant que la tenue n'a pas de corps
  // à montrer, ou que la boîte est vide — et l'appelant garde alors sa place
  // vide, qui ne ment pas.
  [[nodiscard]] const IconTexture& Get(const Pose& pose);

  // OÙ LA SILHOUETTE EST TOMBÉE DANS SA BOÎTE, et de combien elle a grandi.
  //
  // 🔴 C'EST L'INVERSE DU CADRAGE, et c'est ce qui permet de remonter d'un
  // point de la boîte jusqu'aux unités de l'acteur : une soustraction et une
  // division, sans rien à redeviner. Sans cela, la pipette devrait refaire le
  // cadrage de son côté — donc le faire DIVERGER un jour.
  //
  // Inutilisable tant que rien n'a été dessiné.
  [[nodiscard]] const VignetteFraming& framing() const noexcept { return framing_; }

  // L'ENVELOPPE DE LA TENUE en cours, en pixels de sprite : c'est elle qui dit
  // où tombe le haut du corps, là où ce qui se pose au-dessus de la tête
  // s'ancre. Inutilisable tant que rien n'a été dessiné.
  [[nodiscard]] const OutfitSpan& span() const noexcept { return span_; }

  // CE QUE L'IMAGE MONTRE À CET INSTANT, toutes pièces comprises — tête,
  // coiffes, arme —, en pixels de sprite. ⚠ SEULS LES CALQUES PEINTS COMPTENT,
  // comme pour l'emprise cliquable du jeu : c'est d'elle qu'on s'en sert.
  [[nodiscard]] const SpriteBounds& drawn_bounds() const noexcept { return drawn_bounds_; }

  // LE CORPS QUE LE DERNIER `Get` A COMPOSÉ, ou nul s'il n'a rien composé.
  //
  // ⚠ IL APPARTIENT À LA TENUE, qui le refait à chaque composition : qui veut
  // le garder au-delà de l'appel suivant le COPIE.
  [[nodiscard]] const ComposedActor* composed() const noexcept { return composed_; }
  // La durée d'un tour de l'animation composée, en millisecondes ; zéro sans
  // composition.
  [[nodiscard]] float animation_duration_ms() const noexcept { return animation_duration_ms_; }

 private:
  ActorWardrobe* wardrobe_;
  IconTexture texture_;
  // Ce que la texture montre déjà. ⚠ `drawn_` VIDE VEUT DIRE « RIEN DE
  // DESSINÉ » : c'est ce qui force la première composition.
  std::vector<SpriteImageKey> drawn_;
  // Et où, dans quel sens et quelle teinte : voir `SameQuads`.
  std::vector<SpriteQuad> drawn_quads_;
  ActorLook look_;
  std::size_t animation_ = 0;
  std::size_t direction_ = 0;
  VignetteBox box_;
  // L'enveloppe et le cadrage de la tenue en cours, refaits quand l'apparence,
  // l'animation ou la boîte changent, et la direction aussi au cadrage centré,
  // qui n'en mesure qu'une — jamais à chaque image.
  OutfitSpan span_;
  VignetteFraming framing_;
  // Le cadrage de la texture déjà téléversée : un portrait se recadre sans que
  // ses calques changent.
  VignetteFraming drawn_framing_;
  // Le dernier `Get` cadrait-il un portrait ? Le cadrage en pied se refait
  // alors au suivant, même boîte et même tenue.
  bool aimed_ = false;
  // Les boîtes de la boucle du portrait, gardées tant que la pose ne change
  // pas.
  PortraitFramer portrait_framer_;
  SpriteBounds drawn_bounds_;
  const ComposedActor* composed_ = nullptr;
  float animation_duration_ms_ = 0.0F;
};

}  // namespace ro::render
