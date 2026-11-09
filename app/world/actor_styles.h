#pragma once

#include <cstddef>
#include <cstdint>
#include <map>

#include "ro/render/actor_wardrobe.h"
#include "ro/render/palette_ramps.h"
#include "ro/render/sprite_catalog.h"
#include "ro/render/style_set.h"
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

// LES TROIS NUMÉROS D'UNE RECETTE — teinte de base, couleur de cheveux,
// coiffure — posés sur cette apparence, pour une tête de groupe ou de guilde :
// elle n'a ni costume à teindre ni identifiant de style à tenir. Sans recette
// pour ce corps, l'apparence ressort telle quelle.
//
// ⚠ PAS DE REGISTRE ICI, et c'est voulu : rien n'est enregistré auprès de la
// garde-robe, donc rien n'est à lui rendre.
void ImposeStyleNumbers(std::uint32_t gid, ro::render::ActorLook& look,
                        ro::render::ActorWardrobe& wardrobe, const ro::sim::StyleBook& book);

class ActorStyles {
 public:
  // HABILLE CETTE APPARENCE DES COULEURS DE SON PORTEUR.
  //
  // `look` entre SANS style et en ressort avec ce que la recette pose sur un
  // acteur du monde : l'identifiant que la garde-robe connaît, et la TEINTE DE
  // VÊTEMENT. Celle-ci désigne le fichier de palette sur lequel la base est
  // fusionnée, donc le découpage des rampes : un appelant qui l'oublierait
  // verrait ce joueur autrement que les autres le voient.
  //
  // 🔴 NI LA COIFFURE NI LA COULEUR DE CHEVEUX, et c'est le contrat du serveur :
  // il les a déjà posées par son propre changement d'apparence, et l'acteur les
  // porte. Celles du lot datent de la dernière validation ; après un passage
  // chez le styliste, elles remettraient l'ancienne coupe.
  //
  // ⚠ LA CLÉ DU CORPS SE PREND AVANT, et elle le peut : elle ne dépend que du
  // chemin du `.spr`, que la teinte ne déplace pas.
  //
  // 🔴 LE STYLE EST UN ENSEMBLE : la recette du corps, et les teintures de
  // calques que cette apparence PORTE — celle de sa tête, de ses coiffes, de
  // sa cape, et sa peau. Une teinture d'un chapeau rangé reste au livre : elle
  // n'entre pas dans l'ensemble, et deux joueurs qui ne diffèrent que par elle
  // partagent leur style.
  //
  // 🔴 LES PORTEURS D'UN MÊME ENSEMBLE PARTAGENT UN IDENTIFIANT, donc leurs
  // palettes et leurs textures. L'ensemble se compare réglage par réglage,
  // sans condensé, et les trois numéros de la recette n'y entrent pas : la
  // palette d'un style n'en dépend pas.
  //
  // 🔴 UN CHANGEMENT DONNE UN IDENTIFIANT NEUF. La garde-robe refuse de
  // réécrire une recette en place : une tenue déjà composée retient l'ADRESSE
  // de sa palette, et la repeindre sous le même pointeur ne se verrait qu'à la
  // prochaine éviction.
  //
  // 🔴 ET UN IDENTIFIANT QUE PLUS PERSONNE NE PORTE NE MEURT QU'AU BALAYAGE.
  // `Dress` s'appelle n'importe quand dans l'image — le portrait, le cadre de
  // la cible, la fiche —, alors que la scène de l'image précédente désigne
  // encore ses tenues, et que la désignation à la souris la lit avant la
  // composition suivante.
  void Dress(std::uint32_t gid, ro::render::ActorLook& look, ro::render::ActorWardrobe& wardrobe,
             const ro::sim::StyleBook& book);

  // Oublie les acteurs dont le livre ne parle plus, et rend à la garde-robe les
  // identifiants que plus personne ne porte. ⚠ APRÈS LA COMPOSITION :
  // `ForgetStyle` invalide les tenues du style, et une composition en cours,
  // comme la scène qu'elle remplace, les lirait détruites.
  void Sweep(ro::render::ActorWardrobe& wardrobe, const ro::sim::StyleBook& book);

 private:
  // Un identifiant enregistré auprès de la garde-robe, et combien d'acteurs le
  // portent.
  struct Interned {
    ro::render::StyleSet set;
    std::size_t bearers = 0;
  };

  // L'identifiant qui porte cet ensemble, enregistré s'il n'existe pas ; zéro
  // si la garde-robe le refuse.
  [[nodiscard]] std::uint32_t Intern(const ro::render::StyleSet& set,
                                     ro::render::ActorWardrobe& wardrobe);
  void Release(std::uint32_t style);

  // Ce que chaque acteur porte en ce moment, par son identifiant.
  std::map<std::uint32_t, std::uint32_t> applied_;
  // ⚠ UN STYLE SANS PORTEUR Y RESTE JUSQU'AU BALAYAGE : un acteur qui reprend
  // la même recette avant retrouve le même identifiant.
  std::map<std::uint32_t, Interned> interned_;
};

}  // namespace moonclient
