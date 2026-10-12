#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// CE QUE LE CLIENT SAIT D'UN ÉTAT : son nom, sa description, son icône, son
// rang. Lu dans `data\yaml_files\status_effects.yml`, que
// `scripts/yaml_tables/status_effects.py` tire de l'installation, rangé par les
// identifiants du serveur :
//
//     version: 1
//     statuses:
//       1:
//         constant: EFST_ENDURE
//         icon: Endure.tga
//         priority: gold
//         lines:
//         - {text: Endure, color: [155, 202, 155]}
//         - {text: '%s', color: [255, 176, 98]}
//         - {text: Resists flinching}
//
// 🔴 LE SERVEUR NE DIT QUE DES NUMÉROS : un EFST dans les icônes au-dessus des
// têtes, et un EFST dans le champ d'une compétence pour les bonus de la fiche.
// Tout le reste vient de cette table.
//
// ⚠ UNE SEULE LECTURE POUR LES NOMS ET LES ICÔNES, parce qu'elles parlent du
// même objet : un état dont on aurait le nom sans l'icône, ou l'inverse, serait
// une occasion de les voir diverger.
inline constexpr std::string_view kStatusTablePath = "data\\yaml_files\\status_effects.yml";

// LA LIGNE QUI NE PORTE QUE CELA est la PLACE DE LA DURÉE, pas du texte
// manquant : c'est l'emplacement que le client remplit avec le temps restant.
inline constexpr std::string_view kStatusTimePlaceholder = "%s";

// LE RANG D'AFFICHAGE : les icônes se rangent en CINQ SEAUX, et le seau est
// l'ordre dans lequel le client montre ce qui ne tient pas.
enum class StatusPriority : std::uint8_t {
  kGold = 0,
  kRed = 1,
  kBlue = 2,
  kGreen = 3,
  kWhite = 4,
};

// Les noms des seaux dans la table, dans l'ordre de leur rang.
inline constexpr std::array<std::string_view, 5> kStatusPriorityNames = {"gold", "red", "blue",
                                                                         "green", "white"};
inline constexpr std::size_t kStatusPriorities = kStatusPriorityNames.size();

// La couleur d'une ligne : rouge, vert, bleu.
using StatusColor = std::array<std::uint8_t, 3>;

struct StatusInfo {
  // Vide quand la table ne nomme pas cet état : l'appelant écrit alors son
  // numéro.
  std::string name;
  std::optional<StatusColor> name_color;

  // ⚠ UN NOM NU, ET C'EST UN PIÈGE DÉJÀ PAYÉ. La table rend « BD_Sleep.tga »,
  // sans dossier, là où la table en dur du client rend un chemin complet. Un
  // chargeur qui parle au disque doit remettre le dossier lui-même, sinon
  // TOUTES les icônes échouent — en silence.
  std::string icon_file;

  // CE QUE L'ÉTAT FAIT, en toutes lettres — les lignes 2 et suivantes, sans le
  // nom, sans la place de la durée et sans les lignes vides.
  //
  // 🔴 SIX CENT TRENTE-TROIS ÉTATS EN PORTENT UNE, jusqu'à sept lignes, et
  // c'est ce que le joueur vient lire au survol. Une infobulle qui n'écrit que
  // le nom répète l'icône.
  std::vector<std::string> description;
  // La couleur de chaque ligne de `description`, au même rang.
  std::vector<std::optional<StatusColor>> description_colors;

  // OÙ LA DURÉE S'INSÈRE dans ces lignes, et c'est un RANG, pas un drapeau.
  //
  // ⚠ LE CLIENT LA MET AU MILIEU, pas en tête : la table lui réserve une ligne,
  // qui ne porte que « %s ». Zéro = juste sous le nom, ce qui est aussi le repli
  // quand la table ne réserve rien.
  int time_line = 0;
  std::optional<StatusColor> time_color;

  StatusPriority priority = StatusPriority::kWhite;
};

struct StatusTable {
  std::unordered_map<std::uint32_t, StatusInfo> by_id;

  // Ce que la table sait de cet état, ou rien. ⚠ RIEN N'EST PAS UNE PANNE : le
  // serveur connaît des états que la table ne décrit pas.
  [[nodiscard]] const StatusInfo* Find(std::uint32_t effect_id) const;

  // Le nom seul, pour qui n'a que cela à écrire.
  [[nodiscard]] std::string_view NameOf(std::uint32_t effect_id) const;

  [[nodiscard]] std::size_t size() const noexcept { return by_id.size(); }
  [[nodiscard]] bool empty() const noexcept { return by_id.empty(); }

  // Combien portent une icône. ⚠ MOINS QUE LE COMPTE TOTAL, et ce n'est pas un
  // défaut : un état peut avoir un nom sans icône, ou l'inverse.
  [[nodiscard]] std::size_t drawn() const noexcept;
};

// Le texte de `status_effects.yml`. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, ou dont un état porte un champ inconnu, un seau
// inconnu, une ligne sans texte ou une couleur qui n'est pas trois octets.
//
// La première ligne d'un état est son nom ; un état dont la première ligne est
// vide n'a ni nom ni description. Un état sans nom ni icône n'entre pas.
[[nodiscard]] core::Result<StatusTable> ParseStatusTable(std::string_view yaml_text);

// Lit la table. ABSENTE, elle est vide et ce n'est pas une erreur : un état
// sans nom se montre encore par son numéro, et par l'icône que le client tient
// en dur.
[[nodiscard]] core::Result<StatusTable> ReadStatusTable(Vfs& vfs);

}  // namespace ro::vfs
