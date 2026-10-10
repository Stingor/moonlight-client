#pragma once

#include <cstdint>
#include <map>

#include "ro/render/actor_wardrobe.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/style.h"

namespace moonclient {

// LE CHAÎNON ENTRE CE QUE LE SERVEUR DIT ET CE QUE LA GARDE-ROBE PEINT.
//
// Le fil apporte des recettes par joueur et par CORPS (`ro/sim/style.h`) ; la
// garde-robe, elle, connaît des STYLES désignés par un identifiant
// (`ActorWardrobe::SetStyle`). Ce registre fait la conversion, et c'est lui
// qui tient les deux choses que ni l'une ni l'autre ne peut tenir :
//
//   * QUELLE variante s'applique — celle du corps que cet acteur porte en ce
//     moment, ce qui demande la clé de son `.spr` ;
//   * QUAND un identifiant devient périmé.
//
// ⚠ IL VIT DANS `app`, comme toute traduction entre `sim` et `render` : le
// rendu ne connaît pas la simulation, et l'éditeur de liens le dit.
// LES SEULS NUMÉROS D'UNE RECETTE — teinte de base, couleur de cheveux,
// coiffure — posés sur cette apparence, pour qui n'a que faire des rampes : une
// tête de groupe n'a ni costume à teindre ni identifiant de style à tenir.
// Sans recette pour ce corps, l'apparence ressort telle quelle.
//
// ⚠ PAS DE REGISTRE ICI, et c'est voulu : rien n'est enregistré auprès de la
// garde-robe, donc rien n'est à lui rendre.
void ImposeStyleNumbers(std::uint32_t gid, ro::render::ActorLook& look,
                        ro::render::ActorWardrobe& wardrobe, const ro::sim::StyleBook& book);

class ActorStyles {
 public:
  // HABILLE CETTE APPARENCE DES COULEURS DE SON PORTEUR.
  //
  // `look` entre SANS style et en ressort avec tout ce que la recette impose :
  // l'identifiant que la garde-robe connaît, et les TROIS NUMÉROS — teinte de
  // base, couleur de cheveux, coiffure. Le registre enregistre la recette
  // auprès de la garde-robe la première fois qu'il la voit, et rend ensuite le
  // même identifiant tant que rien ne change.
  //
  // 🔴 UN SEUL POINT D'ENTRÉE POUR LES QUATRE, ET CE N'EST PAS DU CONFORT. Les
  // trois numéros ne décorent pas l'identifiant : la teinte de base désigne le
  // fichier de palette sur lequel la base est fusionnée, donc le découpage des
  // rampes, donc ce que les réglages eux-mêmes produisent. Les distribuer par
  // un second appel laisserait un appelant les oublier — et ce joueur se
  // verrait alors autrement qu'on le voit, sans que rien ne le signale.
  //
  // ⚠ LA CLÉ DU CORPS SE PREND AVANT, et elle le peut : elle ne dépend que du
  // chemin du `.spr`, qu'aucun de ces numéros ne déplace.
  //
  // 🔴 UN CHANGEMENT DONNE UN IDENTIFIANT NEUF, et l'ancien est oublié. La
  // garde-robe refuse de réécrire une recette en place — une tenue déjà
  // composée retient l'ADRESSE de sa palette, et la repeindre sous le même
  // pointeur ne se verrait qu'à la prochaine éviction. Le registre prend donc
  // le refus au sérieux au lieu de le contourner.
  void Dress(std::uint32_t gid, ro::render::ActorLook& look, ro::render::ActorWardrobe& wardrobe,
             const ro::sim::StyleBook& book);

  // Oublie les acteurs dont le livre ne parle plus, et rend à la garde-robe ce
  // qu'ils tenaient. ⚠ ENTRE DEUX IMAGES : `ForgetStyle` invalide les tenues
  // du style, et une composition en cours les lirait détruites.
  void Sweep(ro::render::ActorWardrobe& wardrobe, const ro::sim::StyleBook& book);

 private:
  // Ce qu'un acteur porte en ce moment, et sur quoi cela a été décidé.
  struct Applied {
    std::uint32_t style = 0;
    std::uint32_t body_key = 0;
    // ⚠ LA RECETTE ELLE-MÊME, et on la COMPARE champ par champ : c'est le seul
    // moyen de savoir qu'un lot a changé quelque chose. Un condensé
    // collisionnerait un jour, et deux couleurs échangées en silence sont
    // exactement ce que ce chantier doit éviter.
    ro::sim::StyleRecipe recipe;
  };

  std::map<std::uint32_t, Applied> applied_;
};

}  // namespace moonclient
