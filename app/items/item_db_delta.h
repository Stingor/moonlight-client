#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace moonclient {

// LES BONS DE TRAVAIL « ITEM_DB » DU STAFF.
//
// 🔴 CE FICHIER NE DIT PAS CE QU'UN ITEM EST POUR LE SERVEUR. Il dit ce qu'un
// membre du staff veut changer de son entrée dans l'item_db — un nombre,
// `Refineable`, un script —, et rien d'autre.
//
// La vérité est SERVEUR : son item_db et ses imports. La table
// `item_db.yml` que ce client lit en est EXPORTÉE : on ne l'édite jamais, une
// édition y serait écrasée à la régénération suivante et promettrait ce que le
// serveur ne fait pas.
//
// Le geste du staff pose donc une INTENTION ici ;
// `scripts/yaml_tables/apply_item_db_delta.py` la porte dans le fichier
// d'import du serveur qui définit l'item, recharge la base pour vérifier,
// réexporte les tables du client et vide le bon. Ensuite : `@reloaditemdb`.
//
// ⚠ `savedata\item_db_delta.yml` : son nom, sa clé racine et ses champs sont un
// contrat, le script le lit tel quel. Les clés des champs sont celles de
// l'item_db (`ro::ui::kItemSheetServerNumbers`, `kItemSheetRefineableKey`,
// `kItemSheetServerScripts`). Absent, c'est le cas nominal.
struct ItemDbDeltaEntry {
  std::uint32_t item_id = 0;
  // Clé de l'item_db -> valeur voulue, en texte : un entier en décimal,
  // `true` ou `false`, ou le texte d'un script — vide : le script retiré.
  std::vector<std::pair<std::string, std::string>> fields;

  [[nodiscard]] bool operator==(const ItemDbDeltaEntry& other) const = default;
};

class ItemDbDelta {
 public:
  explicit ItemDbDelta(std::filesystem::path client_root);

  [[nodiscard]] const std::vector<ItemDbDeltaEntry>& entries();
  // L'intention posée sur cet item, ou nulle.
  [[nodiscard]] const ItemDbDeltaEntry* Find(std::uint32_t item_id);

  // Pose une intention (elle remplace celle du même item) et réécrit le
  // fichier ; une intention sans champ retire celle de l'item. 🔴 REND FAUX SI
  // L'ÉCRITURE A ÉCHOUÉ, et l'état d'avant est alors rétabli.
  [[nodiscard]] bool Set(const ItemDbDeltaEntry& entry);
  // Retire l'intention. Retirer une intention absente est un succès.
  [[nodiscard]] bool Clear(std::uint32_t item_id);

 private:
  void Load();
  [[nodiscard]] bool Flush() const;

  std::filesystem::path path_;
  std::vector<ItemDbDeltaEntry> entries_;
  bool loaded_ = false;
};

// UN CHAMP S'ÉCRIT-IL EN TEXTE ? Les scripts oui, entre guillemets ; les nombres
// et `Refineable` nus, pour que le script les relise comme le serveur.
[[nodiscard]] bool ItemDbFieldIsText(std::string_view key);

}  // namespace moonclient
