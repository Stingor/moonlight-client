#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/player_bonuses.h"
#include "ro/sim/skill_tree.h"
#include "ro/ui/character_window.h"
#include "ro/vfs/item_words.h"

namespace moonclient {

// Ce qui sait donner un nom à un identifiant d'objet.
//
// ⚠ UNE FONCTION PLUTÔT QUE LA TABLE : cette bibliothèque ne connaît que `sim`
// et `ui`, et la table des objets vit dans `vfs`.
using ItemNamer = std::function<std::string(std::uint32_t item_id)>;

// Ce qui sait donner un nom à un numéro de statut. ⚠ MÊME RAISON QUE POUR LES
// OBJETS : la table vit dans `vfs`, que cette bibliothèque ne connaît pas.
using StatusNamer = std::function<std::string_view(std::uint32_t effect_id)>;

// CE QUE L'ÉQUIPEMENT APPORTE, mis en rangées.
//
// ⚠ ICI PARCE QUE `ui` NE CONNAÎT NI `sim` NI LE SERVEUR : la fenêtre reçoit une
// liste déjà triée et déjà nommée, comme elle reçoit onze cases déjà remplies.
// Et en bibliothèque à part pour la même raison qu'`equip_slot_map` : l'ordre et
// le tri se vérifient sans monter le jeu, alors qu'en jeu une rangée qui manque
// ne se voit que si l'on portait justement la pièce qui la donne.
void BuildBonusRows(const ro::sim::PlayerBonuses& bonuses,
                    std::vector<ro::ui::CharacterBonusRow>& into);

// CE QUI NE VAUT QUE CONTRE QUELQU'UN : « Résist. vs Undead », « Drop vs Boss ».
//
// ⚠ DEUX SORTIES, ET LA PREMIÈRE N'EST PAS UN DÉTAIL : ces libellés-là se
// COMPOSENT, alors que ceux des apports plats sont des littéraux. Les rangées ne
// portent que des vues ; `labels` est ce qui les fait vivre, et l'appelant le
// garde aussi longtemps que les rangées.
void BuildConditionalRows(const ro::sim::PlayerBonuses& bonuses, const ro::vfs::ItemWords& words,
                          std::vector<std::string>& labels,
                          std::vector<ro::ui::CharacterBonusRow>& into);

// CE QUI TIENT À UNE COMPÉTENCE, OU À UN STATUT. Le catalogue peut manquer : les
// compétences se nomment alors par leur numéro.
void BuildSkillRows(const ro::sim::PlayerBonuses& bonuses, const ro::sim::SkillTree* skills,
                    const StatusNamer& status_of, std::vector<std::string>& labels,
                    std::vector<ro::ui::CharacterBonusRow>& into);

// CE QUI TIENT À UN OBJET : un bonus de butin.
void BuildItemRows(const ro::sim::PlayerBonuses& bonuses, const ItemNamer& name_of,
                   std::vector<std::string>& labels, std::vector<ro::ui::CharacterBonusRow>& into);

}  // namespace moonclient
