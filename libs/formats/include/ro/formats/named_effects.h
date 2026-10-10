#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// LES EFFETS QUE LE CLIENT DÉSIGNE PAR UN NOM, et non par un numéro.
//
// C'est une FAMILLE À PART ENTIÈRE, à côté des `.str` numérotés et des effets
// procéduraux. Huit des douze fabriques d'unité au sol ne nomment pas leur
// effet par un identifiant mais par une chaîne — `kunaiarea_01_start`,
// `jack_frost_nova_loop`, `totem_of_tutelary_bottom_start`. Le client les
// résout dans une table de DONNÉES que l'installation porte, et qui donne pour
// chaque nom un fichier `.str`, un son et sa façon de le jouer.
//
// Ces chemins ne sont pas écrits dans le code du client : seule cette table de
// l'installation les donne.
//
// ⚠ UN NOM DEMANDÉ PAR LE CODE PEUT MANQUER AU FICHIER. `new_cross_rain_bottom`
// en est le cas : le client le nomme, cette installation ne l'a pas. Ce n'est
// pas une erreur de lecture — c'est un effet que ce client connaît et que ces
// données ne décrivent pas.

// Le fichier, tel que le VFS le nomme. L'exécutable l'ouvre sous
// ContentData\EffectData\EZ2STREffect.bson ; son ouvreur de ressources y ajoute
// le dossier des données.
inline constexpr std::string_view kNamedEffectsPath =
    "data\\contentdata\\effectdata\\ez2streffect.bson";

// Le seul champ de la racine, sous lequel tous les noms sont rangés.
inline constexpr std::string_view kNamedEffectsRoot = "EZ2STREffect";

// Ce qu'un nom décrit.
//
// Les chemins sont RELATIFS : le fichier au dossier des effets, le son à celui
// des sons. Le dossier ne se déduit pas du fichier de données, il vient du
// client — et 219 des 223 chemins se retrouvent ainsi dans l'archive du joueur.
struct NamedEffect {
  // Le nom, en minuscules. Le client abaisse la clé avant de la ranger.
  std::string name;
  // FilePath, relatif au dossier des effets. Vide pour la fiche MODÈLE que le
  // fichier porte en dernier — une ligne d'exemple laissée par ses auteurs.
  std::string file;
  // SoundPath, relatif au dossier des sons. Vide pour les 89 fiches muettes.
  std::string sound;

  // PosX et PosY : le décalage que le client écrit dans la position de l'effet,
  // en unités du monde, le troisième axe étant mis à zéro. PosY est la HAUTEUR,
  // et c'est elle qui varie — de -2 à 20 selon les fiches.
  float offset_x = 0.0F;
  float offset_y = 0.0F;

  // StartDelayTime, en MILLISECONDES. Le client le divise par seize pour en
  // faire des tics, ce qui redit la cadence du moteur d'effets par un tout
  // autre chemin ; et le son part un tic APRÈS l'effet.
  std::int32_t start_delay_ms = 0;
  // QuakeDelay, en TICS cette fois — il part tel quel. Négatif veut dire
  // AUCUNE secousse, et les 223 fiches de cette installation valent toutes -1.
  std::int32_t quake_delay_ticks = -1;

  // IsInfinite : l'effet ne meurt pas de lui-même. C'est le drapeau que portent
  // les fiches dont le nom finit par `_loop`.
  bool is_infinite = false;
  // IsFloor et IsAboveUI : deux valeurs du mot de rendu de l'hôte, 13 et 517.
  // Elles s'excluent en pratique — aucune fiche ne porte les deux.
  bool is_floor = false;
  bool is_above_ui = false;
  // IsSoundLoop : le son se répète tant que l'effet dure. Quatre fiches.
  bool is_sound_loop = false;
  // IsToGround : une seule fiche le porte.
  bool is_to_ground = false;
};

// La table entière, triée par nom.
//
// Le tri est celui du client : il range ses fiches dans une table ordonnée par
// la clé minusculée, octet par octet. Suivre le même ordre garde ouverte la
// seule chose qui s'en déduise — le RANG d'une fiche, dont le client fait un
// identifiant en lui ajoutant dix mille.
class NamedEffectTable {
 public:
  [[nodiscard]] static core::Result<NamedEffectTable> Parse(std::span<const std::uint8_t> bytes);

  // Nul si le nom est absent. La recherche abaisse la casse, comme le client.
  [[nodiscard]] const NamedEffect* Find(std::string_view name) const;

  [[nodiscard]] std::span<const NamedEffect> entries() const noexcept { return entries_; }

 private:
  std::vector<NamedEffect> entries_;
};

}  // namespace ro::formats
