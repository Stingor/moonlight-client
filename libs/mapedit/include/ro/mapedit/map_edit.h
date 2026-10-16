#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/rsm.h"
#include "ro/formats/rsw.h"

namespace ro::mapedit {

// LE DOCUMENT D'ÉDITION D'UNE CARTE : voir docs/adr/0118.
//
// Une carte de l'installation ne se réécrit jamais. Son édition est une liste
// d'OPÉRATIONS que le client applique au chargement, avant de bâtir ses
// maillages ; elle vit dans un fichier par carte, `data\map_edits\<carte>.yml`,
// versionné et livré avec le client.
//
// 🔴 CHAQUE OPÉRATION DÉSIGNE SA CIBLE DE FAÇON VÉRIFIABLE, et dit pourquoi.
// Une cible qui ne correspond plus — un patch a changé la carte — n'est pas
// touchée, et le rapport le dit : toucher le mauvais objet serait pire que ne
// rien faire.
//
// Le visuel seulement : la marchabilité (GAT) n'est jamais touchée, c'est le
// serveur qui décide où l'on marche.

// L'écart toléré entre la position d'origine qu'une opération attend et celle
// du RSW, en unités du fichier, sur chaque axe : de quoi absorber l'arrondi
// d'une position recopiée à trois décimales.
inline constexpr float kOriginTolerance = 0.01F;

// Le chemin du document d'une carte, nommée sans extension.
[[nodiscard]] std::string MapEditPath(std::string_view map_name);

// LA RETOUCHE D'UN MODÈLE : se reposer, ou ne plus se dessiner. Les champs
// absents gardent la valeur du RSW.
struct ModelEdit {
  // Le rang du modèle dans la liste du RSW, compté à partir de zéro.
  std::size_t model_index = 0;
  // Son fichier, relatif à data\model\, en UTF-8.
  std::string model_file;
  // Sa position dans le RSW d'origine, dans le repère du fichier.
  std::array<float, 3> origin{};
  std::optional<std::array<float, 3>> position;
  std::optional<std::array<float, 3>> rotation;
  std::optional<std::array<float, 3>> scale;
  // Vrai : le modèle n'est pas dessiné.
  bool hidden = false;
  // Pourquoi : le document refuse une opération qui ne le dit pas.
  std::string why;

  friend bool operator==(const ModelEdit&, const ModelEdit&) = default;
};

// UN MODÈLE AJOUTÉ par l'éditeur : un fichier de l'installation, posé là où le
// staff l'a voulu. Il prend rang APRÈS les modèles du RSW, dans l'ordre du
// document ; masqué, il garde son rang et ne se dessine pas.
struct AddedModel {
  // Son fichier, relatif à data\model\, en UTF-8.
  std::string model_file;
  std::array<float, 3> position{};
  std::array<float, 3> rotation{};
  std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
  bool hidden = false;
  // Pourquoi : le document refuse un ajout qui ne le dit pas.
  std::string why;

  friend bool operator==(const AddedModel&, const AddedModel&) = default;
};

// UN SOMMET DÉPLACÉ : son nœud, son rang dans le nœud, sa position dans le
// fichier et la nouvelle, toutes deux dans le repère du nœud.
struct VertexMove {
  // Le nom du nœud, en UTF-8.
  std::string node;
  std::size_t vertex = 0;
  std::array<float, 3> origin{};
  std::array<float, 3> position{};

  friend bool operator==(const VertexMove&, const VertexMove&) = default;
};

// LA RETOUCHE DU MAILLAGE D'UN MODÈLE : des sommets déplacés.
//
// Elle désigne sa pose comme une retouche de modèle — rang, fichier, position
// d'origine — et vaut pour cette pose seule, qui se dessine alors avec SON
// maillage, ou pour toutes les poses du fichier. Chaque sommet se vérifie à
// son tour : un nœud, un rang ou une position d'origine qui ne correspondent
// plus ne se déplacent pas.
struct MeshEdit {
  std::size_t model_index = 0;
  std::string model_file;
  std::array<float, 3> origin{};
  // Vrai : toutes les poses du fichier ; faux : celle de rang `model_index`.
  bool all_poses = false;
  // Rangés par nœud, puis par rang.
  std::vector<VertexMove> vertices;
  std::string why;

  // Le déplacement du sommet `vertex` du nœud `node`, ou rien.
  [[nodiscard]] const VertexMove* VertexAt(std::string_view node, std::size_t vertex) const;
  // Pose `move` : il remplace celui du même sommet, ou prend sa place.
  void SetVertex(VertexMove move);
  // Retire le déplacement d'un sommet.
  void RemoveVertex(std::string_view node, std::size_t vertex);

  friend bool operator==(const MeshEdit&, const MeshEdit&) = default;
};

// Le document d'une carte.
class MapEdit {
 public:
  // Les retouches de modèles, rangées par rang.
  [[nodiscard]] const std::vector<ModelEdit>& models() const noexcept { return models_; }
  [[nodiscard]] bool empty() const noexcept {
    return models_.empty() && added_.empty() && meshes_.empty();
  }
  // Les retouches de maillages, rangées par rang de pose.
  [[nodiscard]] const std::vector<MeshEdit>& meshes() const noexcept { return meshes_; }
  [[nodiscard]] const MeshEdit* MeshAt(std::size_t model_index) const;
  void SetMesh(MeshEdit edit);
  void RemoveMesh(std::size_t model_index);
  // Les modèles ajoutés, dans l'ordre de leurs rangs.
  [[nodiscard]] const std::vector<AddedModel>& added() const noexcept { return added_; }
  // Ajoute un modèle au bout de la liste.
  void AddModel(AddedModel model) { added_.push_back(std::move(model)); }
  // Remplace l'ajout de rang `slot` dans la liste ; hors de la liste, rien.
  void SetAdded(std::size_t slot, AddedModel model);
  // Retire l'ajout de rang `slot` : les suivants remontent.
  void RemoveAdded(std::size_t slot);

  // La retouche du modèle de rang `model_index`, ou rien.
  [[nodiscard]] const ModelEdit* ModelAt(std::size_t model_index) const;
  // Pose `edit` : elle remplace celle qui vise le même rang, ou prend sa place
  // dans l'ordre des rangs.
  void SetModel(ModelEdit edit);
  // Retire la retouche du rang `model_index`.
  void RemoveModel(std::size_t model_index);

  friend bool operator==(const MapEdit&, const MapEdit&) = default;

 private:
  std::vector<ModelEdit> models_;
  std::vector<AddedModel> added_;
  std::vector<MeshEdit> meshes_;
};

// Le texte d'un document. Refuse un texte qui n'est pas du YAML, dont la
// version n'est pas gérée, qui nomme une autre carte que `map_name`, ou dont
// une opération est malformée — un champ manquant, un vecteur qui n'a pas
// trois composantes, une raison vide.
[[nodiscard]] core::Result<MapEdit> ParseMapEdit(std::string_view yaml_text,
                                                 std::string_view map_name);

// LE TEXTE DU DOCUMENT, en-tête compris, tel que `ParseMapEdit` le relit à
// l'identique : c'est ce que l'éditeur enregistre.
[[nodiscard]] std::string MapEditToYaml(const MapEdit& edit, std::string_view map_name);

// Ce que l'application a fait : les opérations posées, une ligne par
// opération refusée qui dit pourquoi, et les rangs des modèles masqués.
struct MapEditReport {
  std::size_t applied = 0;
  std::vector<std::string> refused;
  // Les rangs des modèles à ne pas dessiner, croissants.
  std::vector<std::size_t> hidden;
};

// LES NOMS DE FICHIERS, de l'encodage du RSW à l'UTF-8 et retour : `mapedit` ne
// connaît pas les encodages, l'appelant les lui traduit.
using FileNameOf = std::string (*)(std::string_view name);
struct FileNameCodec {
  // Du nom que le RSW cite à l'UTF-8 du document.
  FileNameOf to_utf8 = nullptr;
  // De l'UTF-8 du document au nom que le RSW citerait.
  FileNameOf to_raw = nullptr;
};

// Pose `edit` sur `rsw`, dont chaque cible se vérifie, puis lui ajoute les
// modèles ajoutés, au bout de sa liste. Un modèle masqué RESTE dans le RSW, à
// son rang : il va dans `MapEditReport::hidden`, et le décor ne le dessine pas.
// Les rangs du RSW restent ainsi ceux du fichier, que l'éditeur désigne.
[[nodiscard]] MapEditReport ApplyMapEdit(const MapEdit& edit, formats::RswScene& rsw,
                                         const FileNameCodec& names);

// La cible d'une retouche de maillage correspond-elle au modèle de son rang
// dans `rsw` — fichier et position d'origine ? Rend la raison du refus, vide
// quand elle correspond.
[[nodiscard]] std::string MeshTargetRefusal(const MeshEdit& edit, const formats::RswScene& rsw,
                                            const FileNameCodec& names);

// Pose les sommets de `edit` sur `model` ; un sommet refusé se dit dans le
// rapport, et ne bouge pas.
[[nodiscard]] MapEditReport ApplyMeshEdit(const MeshEdit& edit, formats::RsmModel& model,
                                          const FileNameCodec& names);

}  // namespace ro::mapedit
