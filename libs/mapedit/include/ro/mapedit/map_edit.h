#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
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

// L'OPACITÉ D'UN MODÈLE, de 0 (invisible) à 1 (plein) : absente du document,
// elle est pleine, et le document refuse une valeur hors de ces bornes.
inline constexpr float kNoOpacity = 0.0F;
inline constexpr float kFullOpacity = 1.0F;

// Une opacité tient-elle dans ses bornes ? Faux pour NaN.
[[nodiscard]] constexpr bool OpacityInBounds(float opacity) noexcept {
  return opacity >= kNoOpacity && opacity <= kFullOpacity;
}

// Ce que le document écrit d'une opacité réglée : rien quand elle est pleine,
// sinon elle, ramenée dans ses bornes.
[[nodiscard]] std::optional<float> DocumentOpacity(float opacity) noexcept;

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
  // Translucide, de 0 à 1 ; rien : plein.
  std::optional<float> opacity;
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
  // Translucide, de 0 à 1 ; rien : plein.
  std::optional<float> opacity;
  // Pourquoi : le document refuse un ajout qui ne le dit pas.
  std::string why;

  friend bool operator==(const AddedModel&, const AddedModel&) = default;
};

// LA PLAGE D'HEURES d'un objet « la nuit seulement », en heures du serveur,
// de 0 à 24 : il luit de `from_hour` à `to_hour`, la plage passant minuit
// quand elle commence après sa fin ; deux bornes égales couvrent tout le jour.
// Elle ne vaut que si le ciel a une heure — le cycle jour et nuit, ou l'heure
// que force l'établi ; sinon, la nuit que le serveur pose décide.
struct LitHours {
  float from_hour = kDefaultFromHour;
  float to_hour = kDefaultToHour;

  static constexpr float kDefaultFromHour = 20.0F;
  static constexpr float kDefaultToHour = 6.0F;

  friend bool operator==(const LitHours&, const LitHours&) = default;
};

inline constexpr float kLitHoursPerDay = 24.0F;
// Une lumière monte et descend sur tant d'heures, au-dedans de sa plage ; une
// plage plus courte que deux fondus fond sur sa moitié.
inline constexpr float kLitHoursFadeHours = 0.5F;

// L'heure `hour` tombe-t-elle dans la plage, bornes comprises ?
[[nodiscard]] bool LitHoursCover(const LitHours& hours, float hour);
// Combien une lumière luit à l'heure `hour`, de 0 hors de sa plage à 1 en son
// cœur, par un fondu de `kLitHoursFadeHours` à chaque borne.
[[nodiscard]] float LitHoursStrength(const LitHours& hours, float hour);

// UNE LUMIÈRE PONCTUELLE ajoutée par l'éditeur : elle éclaire le sol et le
// décor autour d'elle, jusqu'à sa portée, où elle s'éteint. La couleur porte
// l'intensité : une composante au-delà de 1 éclaire plus fort.
struct AddedLight {
  // Sa position, dans le repère du RSW.
  std::array<float, 3> position{};
  std::array<float, 3> color{1.0F, 1.0F, 1.0F};
  // Le rayon où elle s'éteint, en unités du RSW ; strictement positif.
  float range = 0.0F;
  // Vrai : elle ne s'allume qu'à la nuit, et plus fort à mesure qu'elle tombe
  // — ou dans sa plage d'heures, quand elle en a une.
  bool night_only = false;
  std::optional<LitHours> hours;
  // Vrai : elle vacille, comme une torche ou un feu.
  bool flicker = false;
  // Vrai : elle projette l'ombre de ce qu'elle éclaire. Faux pour une lumière
  // DANS un objet — une lanterne, un lampion — que l'objet boucherait.
  bool shadow = true;
  std::string why;

  friend bool operator==(const AddedLight&, const AddedLight&) = default;
};

// LA LUEUR D'UN EFFET : la lumière ponctuelle qu'il porte, à sa place, et qui
// suit la même règle de nuit que lui.
struct EffectGlow {
  std::array<float, 3> color{2.0F, 1.4F, 0.7F};
  // En unités du RSW ; strictement positive.
  float range = 50.0F;
  bool flicker = true;
  // Voir `AddedLight::shadow`.
  bool shadow = true;

  friend bool operator==(const EffectGlow&, const EffectGlow&) = default;
};

// UN EFFET DE CARTE ajouté par l'éditeur : un objet d'effet comme ceux du RSW
// — son numéro, sa place, sa cadence et ses paramètres —, que le jeu lève avec
// eux. Les flammes de pvp_n_5-5 sont l'effet 47, cadence 125, paramètres
// [0.6, 1, 0, 0].
struct AddedEffect {
  std::int32_t number = 0;
  // Sa position, dans le repère du RSW.
  std::array<float, 3> position{};
  // Tous les combien l'effet se relance, et ses quatre paramètres, comme le RSW
  // les porte.
  float emit_speed = 0.0F;
  std::array<float, 4> param{};
  // Vrai : il ne se lève qu'à la nuit — ou dans sa plage d'heures.
  bool night_only = false;
  std::optional<LitHours> hours;
  std::optional<EffectGlow> glow;
  std::string why;

  friend bool operator==(const AddedEffect&, const AddedEffect&) = default;
};

// LA RETOUCHE D'UN EFFET DE LA CARTE, un objet d'effet du RSW : déplacé,
// masqué, réservé à la nuit, ou doté d'une lueur. Elle désigne son objet par
// son rang, son numéro et sa position d'origine, et ne vaut plus si la carte a
// changé sous elle.
struct EffectEdit {
  std::size_t effect_index = 0;
  std::int32_t number = 0;
  std::array<float, 3> origin{};
  std::optional<std::array<float, 3>> position;
  bool hidden = false;
  bool night_only = false;
  std::optional<LitHours> hours;
  std::optional<EffectGlow> glow;
  std::string why;

  friend bool operator==(const EffectEdit&, const EffectEdit&) = default;
};

// La retouche vise-t-elle encore cet objet : même numéro, même position
// d'origine, à la tolérance du document ?
[[nodiscard]] bool EffectEditTargets(const EffectEdit& edit, const formats::RswEffect& object);

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

// UNE RETOUCHE DU RELIEF : un point de la GRILLE DES COINS du sol, qui compte
// (largeur + 1) × (hauteur + 1) points — le coin bas-gauche du cube (x, y) est
// le point (x, y), son coin haut-droit le point (x + 1, y + 1) —, et sa levée,
// en unités du monde, positive vers le haut. Voir ro/mapedit/ground_edit.h.
struct GroundEdit {
  std::int32_t corner_x = 0;
  std::int32_t corner_y = 0;
  float lift = 0.0F;

  friend bool operator==(const GroundEdit&, const GroundEdit&) = default;
};

// Une levée plus petite que cela, en unités du monde, ne relève plus rien : son
// point quitte le document.
inline constexpr float kGroundLiftEpsilon = 1.0e-4F;

// Le document d'une carte.
class MapEdit {
 public:
  // Les retouches de modèles, rangées par rang.
  [[nodiscard]] const std::vector<ModelEdit>& models() const noexcept { return models_; }
  [[nodiscard]] bool empty() const noexcept {
    return models_.empty() && added_.empty() && meshes_.empty() && lights_.empty() &&
           effects_.empty() && effect_edits_.empty() && ground_.empty();
  }
  // LES RETOUCHES DU RELIEF : une seule par point, rangées par rangée puis par
  // colonne. Une seule raison les couvre toutes : un relief se retouche d'un
  // geste de brosse, pas point par point.
  [[nodiscard]] const std::vector<GroundEdit>& ground() const noexcept { return ground_; }
  [[nodiscard]] const std::string& ground_why() const noexcept { return ground_why_; }
  void SetGroundWhy(std::string why) { ground_why_ = std::move(why); }
  // Ajoute la levée de `edit` à celle de son point, ou lui en donne une ; une
  // levée qui retombe sous `kGroundLiftEpsilon` quitte le document.
  void LiftGround(const GroundEdit& edit);
  // Retire toutes les retouches du relief, et leur raison.
  void ClearGround() {
    ground_.clear();
    ground_why_.clear();
  }
  // Les effets ajoutés, dans l'ordre du document.
  [[nodiscard]] const std::vector<AddedEffect>& effects() const noexcept { return effects_; }
  void AddEffect(AddedEffect effect) { effects_.push_back(std::move(effect)); }
  // Les retouches des effets de la carte, rangées par rang.
  [[nodiscard]] const std::vector<EffectEdit>& effect_edits() const noexcept {
    return effect_edits_;
  }
  [[nodiscard]] const EffectEdit* EffectEditAt(std::size_t effect_index) const;
  void SetEffectEdit(EffectEdit edit);
  void RemoveEffectEdit(std::size_t effect_index);
  // Remplace l'effet de rang `slot` ; hors de la liste, rien.
  void SetEffect(std::size_t slot, AddedEffect effect);
  // Retire l'effet de rang `slot` : les suivants remontent.
  void RemoveEffect(std::size_t slot);
  // Les lumières ajoutées, dans l'ordre du document.
  [[nodiscard]] const std::vector<AddedLight>& lights() const noexcept { return lights_; }
  void AddLight(AddedLight light) { lights_.push_back(std::move(light)); }
  // Remplace la lumière de rang `slot` ; hors de la liste, rien.
  void SetLight(std::size_t slot, AddedLight light);
  // Retire la lumière de rang `slot` : les suivantes remontent.
  void RemoveLight(std::size_t slot);
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
  std::vector<AddedLight> lights_;
  std::vector<AddedEffect> effects_;
  std::vector<EffectEdit> effect_edits_;
  std::vector<GroundEdit> ground_;
  std::string ground_why_;
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

// L'opacité qu'une opération posée donne au modèle de rang `model_index`.
struct ModelOpacity {
  std::size_t model_index = 0;
  float opacity = kFullOpacity;

  friend bool operator==(const ModelOpacity&, const ModelOpacity&) = default;
};

// Ce que l'application a fait : les opérations posées, une ligne par
// opération refusée qui dit pourquoi, les rangs des modèles masqués et
// l'opacité des translucides.
struct MapEditReport {
  std::size_t applied = 0;
  std::vector<std::string> refused;
  // Les rangs des modèles à ne pas dessiner, croissants.
  std::vector<std::size_t> hidden;
  // Les modèles translucides, par rangs croissants : ni les pleins, ni les
  // masqués.
  std::vector<ModelOpacity> opacities;
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
