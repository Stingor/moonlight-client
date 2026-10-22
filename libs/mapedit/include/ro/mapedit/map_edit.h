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
#include "ro/mapedit/map_sprite.h"

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
// La marchabilité ne change que par les cases bloquées, que le serveur applique
// comme le client (docs/adr/0131) : c'est lui qui décide où l'on marche.

// L'écart toléré entre la position d'origine qu'une opération attend et celle
// du RSW, en unités du fichier, sur chaque axe : de quoi absorber l'arrondi
// d'une position recopiée à trois décimales.
inline constexpr float kOriginTolerance = 0.01F;

// Le chemin du document d'une carte, nommée sans extension.
[[nodiscard]] std::string MapEditPath(std::string_view map_name);
// LE CHEMIN DU DOCUMENT SERVI PAR LE MIROIR : celui que le serveur tient, sous
// sa révision, livré dans l'installation par la mise à jour. Un dossier à part :
// le miroir y remplace ce qui diffère, et ne doit jamais écraser ce que le
// staff a enregistré sur son poste. Voir docs/adr/0128.
[[nodiscard]] std::string ServedMapEditPath(std::string_view map_name);

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
  // Vrai : les lumières et les effets de son type de modèle ne se lèvent pas
  // sur lui.
  bool unlit = false;
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
  // Comment il joue les images clés de son fichier, comme le demande un .rsw :
  // en boucle sauf avis contraire. Un fichier sans images clés ne bouge pas.
  std::int32_t animation_type = formats::kRswModelPlayLooped;
  float animation_speed = 1.0F;
  // Vrai : les lumières et les effets de son type de modèle ne se lèvent pas
  // sur lui.
  bool unlit = false;
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

// LE DOCUMENT DES LUMIÈRES DE MODÈLE, partagé par toutes les cartes : voir
// docs/adr/0126. Il se range, se sert et se publie comme le document d'une
// carte, sous ce nom réservé, que nulle carte ne porte. Il tient aussi les
// effets de modèle (docs/adr/0129) : le serveur ne lit pas ses sections.
inline constexpr std::string_view kModelLightsDocument = "@model_lights";

// UNE LUMIÈRE DE MODÈLE : une lumière que porte chaque pose d'un fichier de
// modèle, sur toutes les cartes. Sa position est dans le repère de la pose —
// voir ro/mapedit/model_light.h — et la suit quand elle tourne ou grandit.
struct ModelLight {
  // Le fichier, relatif à data\model\, en UTF-8 ; voir `ModelFileKey`.
  std::string model_file;
  AddedLight light;

  friend bool operator==(const ModelLight&, const ModelLight&) = default;
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

// UN EFFET DE MODÈLE : un effet que porte chaque pose d'un fichier de modèle,
// sur toutes les cartes — les flammes de tous les braseros. Il se tient dans le
// document commun `kModelLightsDocument`, avec les lumières de modèle, et sa
// position est dans le repère de la pose, comme la leur : voir
// ro/mapedit/model_light.h et docs/adr/0129.
struct ModelEffect {
  // Le fichier, relatif à data\model\, en UTF-8 ; voir `ModelFileKey`.
  std::string model_file;
  AddedEffect effect;

  friend bool operator==(const ModelEffect&, const ModelEffect&) = default;
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

// LES SONS D'AMBIANCE : les sources de son du RSW — l'eau, le vent, la foule —
// retouchées, et celles que l'éditeur ajoute. Une source rejoue son bruit tous
// les `cycle_seconds` tant que le joueur est à portée ; voir
// ro/audio/ambient_sound.h. Les grandeurs sont celles du RSW : unités du
// fichier pour la place, l'emprise et la portée, de 0 à 1 pour le volume.
inline constexpr float kSoundVolumeLeast = 0.0F;
inline constexpr float kSoundVolumeMost = 1.0F;

// Un volume tient-il dans ses bornes ? Faux pour NaN.
[[nodiscard]] constexpr bool SoundVolumeInBounds(float volume) noexcept {
  return volume >= kSoundVolumeLeast && volume <= kSoundVolumeMost;
}

// UN SON AJOUTÉ par l'éditeur : un `.wav` de l'installation, posé là où le
// staff l'a voulu, que le jeu lève avec les sources du RSW.
struct AddedSound {
  // Son fichier, relatif à data\wav\, en UTF-8.
  std::string wave_file;
  // Sa place, dans le repère du RSW.
  std::array<float, 3> position{};
  float volume = kSoundVolumeMost;
  // L'emprise, un rectangle centré sur sa place, en unités entières comme le
  // RSW les range : une largeur nulle ne sonne jamais.
  std::int32_t width = kDefaultFootprint;
  std::int32_t height = kDefaultFootprint;
  // Jusqu'où il s'entend ; strictement positive.
  float range = kDefaultRange;
  // Tous les combien de secondes il se rejoue ; strictement positif.
  float cycle_seconds = formats::kRswDefaultSoundCycleSeconds;
  // Vrai : il ne sonne qu'à la nuit — ou dans sa plage d'heures.
  bool night_only = false;
  std::optional<LitHours> hours;
  std::string why;

  // Une emprise d'un carreau, une portée de dix carreaux : ce que prend un son
  // neuf.
  static constexpr std::int32_t kDefaultFootprint = 10;
  static constexpr float kDefaultRange = 100.0F;

  friend bool operator==(const AddedSound&, const AddedSound&) = default;
};

// LA RETOUCHE D'UNE SOURCE DE SON DU RSW : déplacée, masquée, son volume, sa
// portée ou son cycle changés, réservée à la nuit. Elle désigne sa source par
// son rang, son fichier et sa position d'origine, et ne vaut plus si la carte a
// changé sous elle. Les champs absents gardent la valeur du RSW.
struct SoundEdit {
  std::size_t sound_index = 0;
  // Le fichier de la source, relatif à data\wav\, en UTF-8.
  std::string wave_file;
  std::array<float, 3> origin{};
  std::optional<std::array<float, 3>> position;
  bool hidden = false;
  std::optional<float> volume;
  std::optional<float> range;
  std::optional<float> cycle_seconds;
  bool night_only = false;
  std::optional<LitHours> hours;
  std::string why;

  friend bool operator==(const SoundEdit&, const SoundEdit&) = default;
};

// La retouche vise-t-elle encore cette source : même fichier, en UTF-8, et même
// position d'origine, à la tolérance du document ?
[[nodiscard]] bool SoundEditTargets(const SoundEdit& edit, std::string_view wave_file,
                                    const std::array<float, 3>& position);

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

// CE QU'UNE CASE BLOQUÉE ARRÊTE : voir docs/adr/0131.
enum class CellBlock : std::uint8_t {
  // Ni la marche ni les tirs, comme une case de mur de la GAT.
  kWall,
  // La marche seulement : les tirs passent, comme une crevasse de la GAT.
  kFence,
};

// UNE CASE BLOQUÉE : une case de la grille de marche, dans le repère de la GAT
// et des paquets, que le serveur bloque comme le client. On ne bloque que : une
// case que la GAT bloque déjà le reste.
struct BlockedCell {
  std::int32_t cell_x = 0;
  std::int32_t cell_y = 0;
  CellBlock block = CellBlock::kWall;

  friend bool operator==(const BlockedCell&, const BlockedCell&) = default;
};

// Le document d'une carte.
class MapEdit {
 public:
  // Les retouches de modèles, rangées par rang.
  [[nodiscard]] const std::vector<ModelEdit>& models() const noexcept { return models_; }
  [[nodiscard]] bool empty() const noexcept {
    return models_.empty() && added_.empty() && meshes_.empty() && lights_.empty() &&
           effects_.empty() && effect_edits_.empty() && ground_.empty() && sprites_.empty() &&
           sounds_.empty() && sound_edits_.empty() && model_lights_.empty() &&
           model_effects_.empty() && blocked_cells_.empty();
  }
  // LES CASES BLOQUÉES : une seule par case, rangées par rangée puis par
  // colonne. Une seule raison les couvre toutes : elles se peignent au pinceau.
  [[nodiscard]] const std::vector<BlockedCell>& blocked_cells() const noexcept {
    return blocked_cells_;
  }
  [[nodiscard]] const std::string& blocked_cells_why() const noexcept { return blocked_cells_why_; }
  void SetBlockedCellsWhy(std::string why) { blocked_cells_why_ = std::move(why); }
  // La case bloquée en (x, y), ou rien.
  [[nodiscard]] const BlockedCell* BlockedCellAt(std::int32_t cell_x, std::int32_t cell_y) const;
  // Bloque la case de `cell`, ou change ce qu'elle arrête.
  void BlockCell(const BlockedCell& cell);
  // Retire la case (x, y) de la liste.
  void FreeCell(std::int32_t cell_x, std::int32_t cell_y);
  // Retire toutes les cases bloquées, et leur raison.
  void ClearBlockedCells() {
    blocked_cells_.clear();
    blocked_cells_why_.clear();
  }
  // LES EFFETS DE MODÈLE, dans l'ordre du document : seul celui de
  // `kModelLightsDocument` en porte.
  [[nodiscard]] const std::vector<ModelEffect>& model_effects() const noexcept {
    return model_effects_;
  }
  void AddModelEffect(ModelEffect effect) { model_effects_.push_back(std::move(effect)); }
  // Remplace l'effet de modèle de rang `slot` ; hors de la liste, rien.
  void SetModelEffect(std::size_t slot, ModelEffect effect);
  // Retire l'effet de modèle de rang `slot` : les suivants remontent.
  void RemoveModelEffect(std::size_t slot);
  // LES LUMIÈRES DE MODÈLE, dans l'ordre du document : seul celui de
  // `kModelLightsDocument` en porte.
  [[nodiscard]] const std::vector<ModelLight>& model_lights() const noexcept {
    return model_lights_;
  }
  void AddModelLight(ModelLight light) { model_lights_.push_back(std::move(light)); }
  // Remplace la lumière de modèle de rang `slot` ; hors de la liste, rien.
  void SetModelLight(std::size_t slot, ModelLight light);
  // Retire la lumière de modèle de rang `slot` : les suivantes remontent.
  void RemoveModelLight(std::size_t slot);
  // Les sons ajoutés, dans l'ordre du document.
  [[nodiscard]] const std::vector<AddedSound>& sounds() const noexcept { return sounds_; }
  void AddSound(AddedSound sound) { sounds_.push_back(std::move(sound)); }
  // Remplace le son de rang `slot` ; hors de la liste, rien.
  void SetSound(std::size_t slot, AddedSound sound);
  // Retire le son de rang `slot` : les suivants remontent.
  void RemoveSound(std::size_t slot);
  // Les retouches des sources de son du RSW, rangées par rang.
  [[nodiscard]] const std::vector<SoundEdit>& sound_edits() const noexcept { return sound_edits_; }
  [[nodiscard]] const SoundEdit* SoundEditAt(std::size_t sound_index) const;
  void SetSoundEdit(SoundEdit edit);
  void RemoveSoundEdit(std::size_t sound_index);
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
  // LES SPRITES DE DÉCOR, dans l'ordre du document : voir ro/mapedit/map_sprite.h.
  [[nodiscard]] const std::vector<MapSprite>& sprites() const noexcept { return sprites_; }
  void AddSprite(MapSprite sprite) { sprites_.push_back(std::move(sprite)); }
  // Remplace le sprite de rang `slot` ; hors de la liste, rien.
  void SetSprite(std::size_t slot, MapSprite sprite);
  // Retire le sprite de rang `slot` : les suivants remontent.
  void RemoveSprite(std::size_t slot);
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
  std::vector<MapSprite> sprites_;
  std::vector<AddedSound> sounds_;
  std::vector<SoundEdit> sound_edits_;
  std::vector<ModelLight> model_lights_;
  std::vector<ModelEffect> model_effects_;
  std::vector<BlockedCell> blocked_cells_;
  std::string blocked_cells_why_;
};

// Le texte d'un document. Refuse un texte qui n'est pas du YAML, dont la
// version n'est pas gérée, qui nomme une autre carte que `map_name`, dont une
// opération est malformée — un champ manquant, un vecteur qui n'a pas trois
// composantes, une raison vide —, ou qui porte des lumières ou des effets de
// modèle sans être `kModelLightsDocument`.
[[nodiscard]] core::Result<MapEdit> ParseMapEdit(std::string_view yaml_text,
                                                 std::string_view map_name);

// LE TEXTE DU DOCUMENT, en-tête compris, tel que `ParseMapEdit` le relit à
// l'identique : c'est ce que l'éditeur enregistre.
[[nodiscard]] std::string MapEditToYaml(const MapEdit& edit, std::string_view map_name);

// UN DOCUMENT ET SA RÉVISION : le numéro que le serveur lui a donné, 0 pour un
// document jamais servi (docs/adr/0124). La révision ne fait pas partie du
// document : l'historique de l'éditeur ne la reprend pas en annulant.
struct RevisedMapEdit {
  MapEdit edit;
  std::uint32_t revision = 0;
};
// Comme `ParseMapEdit`, la révision en plus ; une révision qui n'est pas un
// entier positif refuse le document.
[[nodiscard]] core::Result<RevisedMapEdit> ParseRevisedMapEdit(std::string_view yaml_text,
                                                               std::string_view map_name);
// Comme `MapEditToYaml`, la révision écrite en tête quand elle n'est pas nulle.
[[nodiscard]] std::string RevisedMapEditToYaml(const MapEdit& edit, std::string_view map_name,
                                               std::uint32_t revision);

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
  // Les rangs des modèles où les lumières et les effets de modèle ne se lèvent pas,
  // croissants.
  std::vector<std::size_t> unlit;
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
