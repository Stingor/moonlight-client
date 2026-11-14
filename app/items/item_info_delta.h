#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace moonclient {

// LES BONS DE TRAVAIL « FICHE D'OBJET » DU STAFF.
//
// 🔴 CE FICHIER NE DIT PAS CE QU'UN OBJET EST. Il dit ce qu'un membre du staff
// veut changer de sa fiche — son nom, sa ressource, la teinte de son icône, sa
// description —, et rien d'autre.
//
// La vérité est `dist/data/yaml_files/item_info.yml`, dans le dépôt : une
// source éditée (ADR 0112), que le client lit dans l'installation. Le client
// n'y écrit pas : la copie de l'installation serait écrasée à la livraison
// suivante, et celle du dépôt n'est pas à sa portée.
//
// Le geste du staff pose donc une INTENTION ici ;
// `scripts/yaml_tables/apply_item_info_delta.py` la porte dans la table du
// dépôt, relit la table, la recopie dans l'installation et vide le bon.
//
// ⚠ `savedata\item_info_delta.yml` : son nom, sa clé racine et ses champs sont
// un contrat, le script le lit tel quel. Absent, c'est le cas nominal —
// personne n'a rien demandé.
struct ItemInfoDeltaEntry {
  std::uint32_t item_id = 0;
  // Un champ vide n'est pas demandé : la table garde le sien.
  std::optional<std::string> name;
  // En UTF-8, comme la table l'écrit.
  std::optional<std::string> resource;
  // Le mot d'une `ro::formats::IconTint` ; le mot VIDE retire la teinte.
  std::optional<std::string> tint;
  // Les lignes BRUTES, telles que la table les écrit : un `#` final y vaut
  // trait de séparation.
  std::optional<std::vector<std::string>> description;

  [[nodiscard]] bool empty() const noexcept {
    return !name.has_value() && !resource.has_value() && !tint.has_value() &&
           !description.has_value();
  }
  [[nodiscard]] bool operator==(const ItemInfoDeltaEntry& other) const = default;
};

class ItemInfoDelta {
 public:
  explicit ItemInfoDelta(std::filesystem::path client_root);

  [[nodiscard]] const std::vector<ItemInfoDeltaEntry>& entries();
  // L'intention posée sur cet objet, ou nulle.
  [[nodiscard]] const ItemInfoDeltaEntry* Find(std::uint32_t item_id);

  // Pose une intention (elle remplace celle du même objet) et réécrit le
  // fichier ; une intention VIDE retire celle de l'objet. 🔴 REND FAUX SI
  // L'ÉCRITURE A ÉCHOUÉ, et l'état d'avant est alors rétabli : une intention
  // que le disque ne porte pas promettrait un travail que le script ne verra
  // jamais.
  [[nodiscard]] bool Set(const ItemInfoDeltaEntry& entry);
  // Retire l'intention. Retirer une intention absente est un succès.
  [[nodiscard]] bool Clear(std::uint32_t item_id);

  // CHANGE À CHAQUE INTENTION POSÉE OU RETIRÉE.
  [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }

 private:
  void Load();
  [[nodiscard]] bool Flush() const;

  std::filesystem::path path_;
  std::vector<ItemInfoDeltaEntry> entries_;
  bool loaded_ = false;
  std::uint32_t revision_ = 0;
};

}  // namespace moonclient
