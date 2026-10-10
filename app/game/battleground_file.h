#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/error.h"

#include "game/yaml_text.h"

namespace moonclient {

// ── LE FICHIER DES TERRAINS DU CHAMP DE BATAILLE ────────────────────────────
//
// `data\yaml_files\battleground.yml`, livré par le patcheur : ce que la fenêtre
// d'inscription montre de chaque terrain — son nom pour le serveur, ses textes
// dans chaque langue, ses effectifs, ses modes d'inscription, son niveau
// d'entrée, ses récompenses et sa minimap. La source vit dans le dépôt, sous
// `dist/data/yaml_files/battleground.yml`, qui en documente le schéma.
//
// 🔴 LE NOM DU SERVEUR NE SE TRADUIT PAS : il repart tel quel sur le fil, et le
// serveur le compare au `Name` de sa base des champs de bataille.

inline constexpr std::string_view kBattlegroundFilePath = "data\\yaml_files\\battleground.yml";

// La seule version de schéma que ce lecteur comprend.
inline constexpr int kBattlegroundFileVersion = 1;

// UN TEXTE DANS PLUSIEURS LANGUES : voir `YamlText`.
using BattlegroundText = YamlText;

// UNE RÉCOMPENSE : un objet de la base, compté, ou un texte libre pour ce qui
// n'est pas un objet.
struct BattlegroundReward {
  // Zéro : pas un objet, `text` dit la récompense.
  std::uint32_t item_id = 0;
  int count = 1;
  BattlegroundText text;
};

// UN TERRAIN. ⚠ LES TEXTES SONT EN UTF-8.
struct BattlegroundField {
  // CE QUI PART SUR LE FIL : le nom que le serveur reconnaît.
  std::string server_name;
  // Ce que le joueur lit ; vide, il lit `server_name`.
  BattlegroundText name;
  BattlegroundText objective;
  // Ce qu'il faut savoir du terrain ; facultatif.
  BattlegroundText help;
  // L'image de la minimap, sous le dossier des minimaps du jeu.
  std::string minimap_file;
  // Les joueurs par camp. Zéro : non dit.
  int team_min = 0;
  int team_max = 0;
  bool solo_apply = false;
  bool party_apply = false;
  bool guild_apply = false;
  // Le niveau de base d'entrée. Zéro : pas de borne de ce côté.
  int level_min = 0;
  int level_max = 0;
  std::vector<BattlegroundReward> reward_win;
  std::vector<BattlegroundReward> reward_draw;
  std::vector<BattlegroundReward> reward_lose;
};

class BattlegroundFile {
 public:
  // Lit le texte du fichier. Illisible, d'une autre version ou sans liste de
  // terrains : une erreur. ⚠ UNE ENTRÉE FAUTIVE NE CONDAMNE PAS LES AUTRES :
  // elle est sautée, et `problems` dit pourquoi.
  [[nodiscard]] static ro::core::Result<BattlegroundFile> Parse(std::string_view text);

  // Dans l'ordre du fichier : l'ordre de la liste.
  [[nodiscard]] std::span<const BattlegroundField> fields() const noexcept { return fields_; }
  // Nul si aucun terrain ne porte ce nom.
  [[nodiscard]] const BattlegroundField* FindByServerName(std::string_view server_name) const;
  // Ce que la lecture a écarté ou jugé suspect, une ligne par fait, à écrire
  // au journal.
  [[nodiscard]] std::span<const std::string> problems() const noexcept { return problems_; }

 private:
  std::vector<BattlegroundField> fields_;
  std::vector<std::string> problems_;
};

}  // namespace moonclient
