#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ro/render/item_drop.h"
#include "ro/sim/actor.h"
#include "ro/sim/world_fwd.h"

#include "effects/effect_view.h"
#include "world/actor_sounds.h"
#include "world/item_dropper.h"
#include "world/item_pillar_memory.h"
#include "world/map_stage.h"

// LE BUTIN QUI TOMBE : ce qu'un objet au sol fait entre l'instant où le serveur
// l'annonce et celui où il se pose.
//
// Deux comportements, et l'un remplace l'autre — voir
// `ro::render::ItemDropSettings`, qui les commande :
//
//   * la CHUTE du client natif — l'objet naît quinze unités au-dessus de sa
//     case et descend à la verticale en quelque quatre cents millisecondes ;
//     rien ne le fait jaillir de qui l'a lâché ;
//   * le JAILLISSEMENT — l'objet part de l'entité qui vient de le lâcher,
//     s'élève, retombe sur sa case et rebondit deux fois.
//
// Purement visuel dans les deux cas : la case où le serveur a posé l'objet,
// celle qu'on ramasse, ne change pas d'un pouce.
namespace moonclient {

// CE QU'UN OBJET AU SOL MONTRE À CETTE IMAGE.
struct ItemDropPose {
  // Où il se tient, en coordonnées du monde. C'est sa case tant qu'il ne tombe
  // pas, et un point de sa trajectoire tant qu'il vole.
  std::array<float, 3> ground{0.0F, 0.0F, 0.0F};
  // Vrai aux images où il porte la couleur de l'éclat.
  bool sparkling = false;
};

// Au-delà de ce nombre d'arcs vivants, on fait le ménage des départs qu'aucune
// fin d'arc n'a effacés — un objet ramassé en plein vol s'en va sans prévenir.
// Le seuil est haut exprès : la purge est un filet, pas le régime normal, et un
// arc s'efface de lui-même en moins d'une seconde.
inline constexpr std::size_t kItemArcSweepAbove = 64;
inline constexpr std::uint32_t kItemArcMaxAgeMs = 5000;

// D'OÙ CHAQUE OBJET EN VOL EST PARTI.
//
// 🔴 UN ÉTAT, ET IL LE FAUT : le paquet ne nomme personne — ni créature, ni
// joueur — et aucun octet de réseau n'a été ajouté pour le dire. C'est à la
// NAISSANCE de l'objet, et à ce seul instant, que l'entité qui l'a lâché est
// encore là : rAthena envoie les butins sans délai et ne retire l'unité qu'à
// +250 ms. Recalculer la source à chaque image la verrait disparaître en
// chemin, et l'objet changerait de trajectoire en plein vol.
class ItemDrops {
 public:
  // Où l'objet se tient à cet instant, et s'il scintille.
  //
  // `resting_ground` est la position de sa CASE, celle que la composition a
  // déjà calculée : c'est là qu'il se pose, et c'est de là que se déduit tout
  // le reste. Un acteur qui n'est pas un objet au sol la reçoit telle quelle.
  [[nodiscard]] ItemDropPose PoseAt(const ro::sim::World& world, const MapStage& stage,
                                    const ro::sim::Actor& item,
                                    const std::array<float, 3>& resting_ground,
                                    std::uint32_t now_ms);

  // LE PILIER DE LUMIÈRE des butins qui viennent de tomber, et les sons à
  // faire entendre avec — l'appelant les passe à `ActorSounds::HearNamedEffects`,
  // qui seul sait où sont les oreilles.
  //
  // 🔴 LE PILIER TIENT TOUTE LA VIE DU BUTIN, ET SON FICHIER NE DURE QU'UNE
  // SECONDE. Chez le client, l'instance d'effet vit tant que son objet au sol
  // vit — une minute chez ce serveur (`flooritem_lifetime`) —, et elle REBOUCLE
  // son fichier. Le nôtre boucle de même. Le resemer laisserait un clignotement
  // régulier : un trou d'une image sans recouvrement, un FLASH avec, le mélange
  // des effets étant additif.
  //
  // ⚠ IL SUIT LE BUTIN DANS SA CHUTE, ET LE CLIENT NE LE FAIT PAS. Celui-ci
  // plante sa colonne à l'altitude du terrain sous la case d'arrivée et l'y
  // laisse, si bien qu'un butin qui jaillit passerait par-dessus la sienne.
  // C'est un écart VOULU, demandé après l'avoir vu en jeu.
  //
  // ⚠ LE SON NE PART QU'À LA NAISSANCE. La boucle ne le rejoue pas : un
  // battement par seconde sur chaque butin rare de la carte s'entendrait comme
  // un défaut. Un butin RÉANNONCÉ — à l'arrivée sur une carte, au retour en vue
  // — retrouve sa colonne, et reste muet : il ne tombe pas.
  [[nodiscard]] std::vector<SownNamedSound> NoticePillars(const ro::sim::World& world,
                                                          const MapStage& stage,
                                                          EffectsInPlay& effects,
                                                          std::uint32_t now_ms);

  // OÙ UN ACTEUR EST DESSINÉ, ET DE QUELLE TAILLE, tels que la composition
  // vient de les calculer.
  //
  // 🔴 C'EST LA SEULE SOURCE JUSTE POUR LES DEUX, et rien d'autre ne l'est.
  //
  //   * la TAILLE dépend de la tenue, de l'image d'animation et de l'échelle,
  //     que ce module ne connaît pas — et une hauteur fixe passe au-dessus d'un
  //     Poring et sous les genoux d'un Baphomet ;
  //   * la POSITION vient du chemin PLANIFIÉ, celui qui contourne les
  //     obstacles. La recalculer ici donnerait celle du chemin DROIT, et le
  //     butin partirait d'un point où le monstre n'est pas. Cela ne se voit
  //     pas sur un lâcheur immobile — mais une créature qui meurt en
  //     poursuivant raterait son départ pendant que `/butin` tombe juste.
  //
  // ⚠ ELLES VIENNENT DE L'IMAGE PRÉCÉDENTE quand le butin naît, et c'est sans
  // conséquence : ce qui lâche un butin était visible avant de le lâcher, et un
  // mort ne bouge plus.
  void NoteBody(std::uint32_t actor_id, const std::array<float, 3>& ground, float pixels) {
    bodies_[actor_id] = Body{ground, pixels};
  }

  // Plus aucun arc : le changement de carte emporte tous les objets, et les
  // départs qu'on gardait parlent d'un monde qui n'existe plus.
  //
  // ⚠ LE SOUVENIR DES PILIERS RESTE : voir `ItemPillarMemory`. Les butins
  // restés à terre reviennent par un paquet qui ne les dit plus.
  void Forget() {
    starts_.clear();
    lit_.clear();
    bodies_.clear();
  }

  [[nodiscard]] ro::render::ItemDropSettings& settings() noexcept { return settings_; }
  [[nodiscard]] const ro::render::ItemDropSettings& settings() const noexcept { return settings_; }

  [[nodiscard]] std::size_t flying() const noexcept { return starts_.size(); }

 private:
  // Le point de départ d'un arc, en coordonnées du monde, l'âge auquel il a été
  // fixé — celui-ci pour la purge — et de combien le butin part AU-DESSUS de ce
  // point, d'après la taille du corps qui l'a lâché.
  struct Start {
    std::array<float, 3> ground{0.0F, 0.0F, 0.0F};
    std::uint32_t noticed_ms = 0;
    float up_units = 0.0F;
  };

  [[nodiscard]] const Start& StartFor(const ro::sim::World& world, const MapStage& stage,
                                      const ro::sim::Actor& item,
                                      const std::array<float, 3>& resting_ground,
                                      std::uint32_t now_ms);

  std::unordered_map<std::uint32_t, Start> starts_;
  // Les butins dont le pilier a déjà jailli. Ils s'y vident de ceux qui ont
  // quitté le monde : un identifiant que le serveur réattribue ne doit pas
  // passer pour déjà servi.
  std::unordered_set<std::uint32_t> lit_;
  // Où chaque acteur est dessiné, et de quelle taille. Se vide au changement de
  // carte, avec le reste.
  struct Body {
    std::array<float, 3> ground{0.0F, 0.0F, 0.0F};
    float pixels = 0.0F;
  };

  std::unordered_map<std::uint32_t, Body> bodies_;
  ItemPillarMemory pillars_;
  ro::render::ItemDropSettings settings_;
};

}  // namespace moonclient
