#pragma once

#include <cstdint>
#include <string>

#include "ro/sim/skill_book.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/item_table.h"

namespace ro::sim {
class SkillTree;
}  // namespace ro::sim

namespace moonclient {

// CE QU'UN REFUS DE COMPÉTENCE DIT DANS LA CONVERSATION.
//
// 🔴 LA PHRASE SE COMPOSE ICI, et non là où le paquet se lit : elle nomme la
// compétence par son nom AFFICHÉ, que portent les tables du client, et l'objet
// manquant par un LIEN D'ITEM, que seule la table des objets sait bâtir. `net`
// n'a ni l'un ni l'autre ; le grimoire garde entre les deux le fait daté.
//
// Le nom de la compétence précède la phrase, sauf quand la phrase le cite
// déjà : sans lui, « Le skill a échoué. » ne dit pas lequel, et le joueur qui
// lance plusieurs skills d'affilée ne sait pas lequel corriger.
class SkillRefusalReport {
 public:
  // Écrit les refus nouveaux depuis le dernier passage, chacun daté de sa
  // réception. `skills` donne les noms affichés et `items` les liens d'item ;
  // l'un ou l'autre peut manquer, la ligne se rabat alors sur le nom interne
  // ou le numéro.
  //
  // ⚠ À APPELER DANS LE POMPAGE RÉSEAU : une image sautée ne doit pas perdre
  // une ligne.
  //
  // `skill_fail_shown` est `/skillfail` : éteint, les refus que le jeu tait
  // alors passent sans rien écrire — voir `RefusalFollowsSkillFailSwitch`.
  void Notice(ro::sim::World& world, const ro::sim::SkillTree* skills,
              const ro::vfs::ItemTable* items, bool skill_fail_shown);

 private:
  std::uint64_t seen_seq_ = 0;
};

// La ligne d'un refus, traduite, ou VIDE quand le client se tait. Exposée pour
// les essais.
[[nodiscard]] std::string SkillRefusalLine(const ro::sim::SkillRefusal& refusal,
                                           const ro::sim::World& world,
                                           const ro::sim::SkillTree* skills,
                                           const ro::vfs::ItemTable* items);

// LE ROUGE DU CLIENT pour un refus, en `0xRRGGBB` : il écrit `0x0000FF`, un
// `COLORREF` dont le rouge est l'octet de poids faible.
inline constexpr std::uint32_t kSkillRefusalColor = 0xFF0000;

}  // namespace moonclient
