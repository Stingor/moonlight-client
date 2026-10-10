// CE QU'UN POINT DE TÉLÉPORTATION MONTRE AU SOL.
//
// 🔴 UN WARP N'A PAS DE SPRITE, IL A UN EFFET. La table du client nomme la
// classe 45 « 1_ETC_01 » — un villageois, une seule image — et ce n'est
// évidemment pas ce que le jeu dessine à un point de téléportation. Nous y
// posions le sprite du PORTAIL, classe 10007 : un anneau orangé à colonne
// montante, qui n'est pas davantage ce que le client montre.
//
// Ce qu'il fait : quand un PNJ de classe 45 prend son mouvement, le client
// pose l'effet 321, `EF_WARPZONE2`. Deux gardes l'entourent : un LOQUET sur le
// nœud qui n'autorise qu'un seul semis, et la visibilité de l'acteur.
//
// ⚠ LE MÊME CAS FORCE UNE BOÎTE DE SURVOL D'AU MOINS DIX SUR DIX et en
// recalcule les quatre coins. Un warp est donc bien survolable et cliquable
// chez le client : notre curseur de warp et notre clic-pour-marcher ne sont
// pas des inventions, et ils restent.

#include <algorithm>
#include <vector>

#include "ro/render/effect_warp_ring_seed.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"

namespace moonclient {
namespace {

// L'EFFET QU'UN WARP POSE, et le seul. Ce n'est pas une constante de
// commodité : c'est l'effet que le client d'origine pose, et aucun autre.
constexpr std::uint32_t kWarpGroundEffect = 321;

// Un warp est un PNJ, jamais un joueur ni un monstre : sa classe ne suffit pas
// à le reconnaître, parce qu'un identifiant de classe se relit sur n'importe
// quel genre d'acteur.
[[nodiscard]] bool EstUnWarp(const ro::sim::Actor& actor) {
  return actor.kind == ro::sim::ActorKind::kNonPlayer && actor.look.job == ro::sim::kPortalJob;
}

// LES DEUX BITS QUI ÉTEIGNENT UN WARP, et deux seulement.
//
// 🔴 UN WARP CACHÉ ARRIVE QUAND MÊME. Le serveur n'écarte de l'envoi que le PNJ
// rendu invisible ; celui qu'un `hideonnpc` a seulement caché nous est envoyé
// avec ses options, et c'est au client de se taire. Le nôtre montrait donc les
// points de téléportation d'un événement éteint.
//
// Le client d'origine tient un drapeau de visibilité par acteur, que le
// changement d'option éteint sur le bit « caché » (0x2) et sur le bit « cape »
// (0x4). Un acteur naît VISIBLE.
//
// ⚠ LE BIT « INVISIBLE » N'EN EST PAS : il n'éteint pas ce drapeau, et l'acteur
// qui le porte se tait par un autre chemin. L'ajouter ici serait une
// supposition, non une mesure.
constexpr std::uint32_t kWarpHidingOptions = ro::sim::kOptionHide | ro::sim::kOptionCloak;

[[nodiscard]] bool EstCache(const ro::sim::Actor& actor) {
  return (actor.option & kWarpHidingOptions) != 0;
}

// Ce qui mérite des anneaux : un warp, et qui ne se cache pas.
[[nodiscard]] bool ShowsItsRings(const ro::sim::Actor& actor) {
  return EstUnWarp(actor) && !EstCache(actor);
}

}  // namespace

void EffectsInPlay::NoticeWarpActors(const ro::sim::World& world, const MapStage& stage,
                                     std::uint32_t now_ms) {
  // UN WARP PARTI S'OUBLIE, sans quoi celui qui reprendra son identifiant
  // arriverait sans anneaux. Son effet s'en va tout seul : il appartient à
  // l'acteur qui l'a semé, et la troupe le retire avec lui.
  //
  // 🔴 CELUI QUI SE CACHE, LUI, NE PART PAS — et son effet resterait donc à
  // tourner au-dessus d'un point de téléportation éteint, sa durée étant une
  // sentinelle. Le client d'origine ne l'attend pas non plus : dès que l'option
  // bascule, il retire l'effet 321 et ses anneaux sur-le-champ.
  std::vector<std::uint32_t> eteints;
  std::erase_if(per_map_.sown_warps, [&world, &eteints](std::uint32_t id) {
    const ro::sim::Actor* actor = world.Find(id);
    if (actor != nullptr && ShowsItsRings(*actor)) {
      return false;
    }
    // ⚠ ON NE DEMANDE LE RETRAIT QUE D'UN WARP ENCORE LÀ : celui qui a disparu
    // emmène ses anneaux tout seul, et le réclamer coûterait un parcours de la
    // troupe entière pour ce qu'elle a déjà fait.
    if (actor != nullptr) {
      eteints.push_back(id);
    }
    return true;
  });
  if (!eteints.empty()) {
    nodes_->ForgetAbsentBearers(
        [&eteints](std::uint32_t id) { return std::ranges::find(eteints, id) == eteints.end(); });
  }

  for (const auto& [id, actor] : world.actors()) {
    if (!ShowsItsRings(actor)) {
      continue;
    }
    if (per_map_.sown_warps.contains(id)) {
      continue;
    }
    // ⚠ LE LOQUET SE POSE AVANT LE SEMIS, et il se pose même si le semis
    // échoue. C'est ce que fait le client — il pose le loquet, puis sème —, et
    // c'est ce qui empêche un warp dont la texture manque de redemander ses
    // anneaux à chaque image.
    per_map_.sown_warps.insert(id);
    const std::array<float, 3> sol = GroundPositionAt(stage, actor.position.x, actor.position.y);
    // SANS FIN, parce que la durée de l'effet est une sentinelle : le client
    // le laisse vivre jusqu'à ce qu'on retire son porteur.
    constexpr bool kEndless = true;
    PlayById(kWarpGroundEffect, glm::vec3(sol[0], sol[1], sol[2]), now_ms, id, kEndless);
  }
}

}  // namespace moonclient
