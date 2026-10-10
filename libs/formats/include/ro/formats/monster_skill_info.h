#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ro::formats {

// LE GESTE QU'UNE CRÉATURE FAIT pour une compétence : le numéro d'action, qui
// compte à partir de UN, et la façon de le jouer.
struct MonsterSkillAction {
  std::int32_t action = 0;
  // Zéro joue l'action UNE fois, puis rend l'acteur à l'attente ; toute autre
  // valeur la fait tourner jusqu'au geste suivant.
  std::int32_t loop_type = 0;
};

// LA TABLE DES GESTES DE CRÉATURE : `data\monsterskillinfo.xml`.
//
//   <racine>               la balise racine, dont le nom est fixé
//     <bigfoot>            nom de ressource de l'espèce, en MINUSCULES
//       <_179>             « _ » suivi du numéro de compétence
//         <action>7</action>
//         <loopType>0</loopType>
//       </_179>
//     </bigfoot>
//   </racine>
//
// Une créature ou un PNJ n'a pas de posture d'emploi par état, comme un joueur :
// c'est ce fichier qui dit son geste, et une compétence qu'il ne nomme pas n'en
// donne AUCUN.
//
// 🔴 LA PREMIÈRE ENTRÉE GAGNE, à chaque niveau. Une espèce écrite deux fois ne
// se lit que dans son premier bloc : les compétences du second n'existent pas.
//
// ⚠ CE N'EST PAS UN PARSEUR XML : il suit l'imbrication des balises, après
// avoir retiré les commentaires et les déclarations. Un attribut est ignoré, le
// texte hors des deux feuilles aussi.
class MonsterSkillInfo {
 public:
  // Lit la table. N'ÉCHOUE PAS : un fichier illisible donne une table vide, où
  // aucune créature n'a de geste — ce que fait le client quand le fichier manque.
  [[nodiscard]] static MonsterSkillInfo Parse(std::span<const std::uint8_t> bytes);

  // Le geste de cette espèce pour cette compétence, ou rien. Le nom se compare
  // en MINUSCULES, comme le client l'abaisse avant de chercher.
  [[nodiscard]] std::optional<MonsterSkillAction> Find(std::string_view resource_name,
                                                       std::uint32_t skill_id) const;

  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

 private:
  struct Entry {
    // Le rang du bloc d'espèce dans `monsters_` : deux blocs de même nom ne se
    // confondent pas.
    std::size_t block = 0;
    std::string skill_tag;
    // Rien quand le bloc ne porte pas d'action : il masque quand même ceux qui
    // le suivent sous le même nom.
    std::optional<MonsterSkillAction> action;
  };
  // Les blocs d'espèce, dans l'ordre du fichier.
  std::vector<std::string> monsters_;
  std::vector<Entry> entries_;
};

}  // namespace ro::formats
