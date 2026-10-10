#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// CE QUE L'INSTALLATION SAIT D'UN STATUT : son nom, son icône, son rang.
//
// 🔴 LE SERVEUR NE DIT QUE DES NUMÉROS, et il le fera toujours : un EFST dans
// les icônes au-dessus des têtes, et un EFST dans le champ d'une compétence pour
// les bonus de la fiche. Tout le reste vit dans l'installation.
//
// ⚠ TROIS FICHIERS, ET L'ORDRE EST UNE DÉPENDANCE : les deux derniers écrivent
// `[EFST_IDs.EFST_STONE]`, des clés que seul le premier sait résoudre. Lus
// seuls, ils ne rendent rien — comme la chaîne des noms de métiers.
//
// ⚠ ET UNE SEULE LECTURE POUR LES DEUX TABLES, parce qu'elles parlent du même
// objet : un statut dont on aurait le nom sans l'icône, ou l'inverse, serait une
// occasion de les voir diverger.
inline constexpr std::array<std::string_view, 3> kStatusTableFiles = {
    "data\\luafiles514\\lua files\\stateicon\\efstids.lub",
    "data\\luafiles514\\lua files\\stateicon\\stateiconinfo.lub",
    "data\\luafiles514\\lua files\\stateicon\\stateiconimginfo.lub",
};

inline constexpr std::string_view kStatusNameGlobal = "StateIconList";
inline constexpr std::string_view kStatusIconGlobal = "StateIconImgList";

// 🔴 LA TABLE DES NOMS SE REMPLIT APRÈS AVOIR ÉTÉ POSÉE, et c'est ce qui a
// demandé d'apprendre l'affectation indexée au lecteur Lua : `StateIconList = {}`
// puis huit cent trois lignes `StateIconList[clé] = { … }`. Celle des IMAGES,
// elle, est un constructeur ordinaire — mais à deux étages.

// Où le nom se trouve dans une entrée : sa `descript` est une LISTE DE LIGNES, et
// chaque ligne une liste dont le premier élément est le texte. La première ligne
// porte le nom ; les suivantes portent la durée et l'explication.
inline constexpr std::string_view kStatusDescriptField = "descript";

// LA LIGNE QUI NE PORTE QUE CELA est la PLACE DE LA DURÉE, pas du texte
// manquant : c'est l'emplacement que le client remplit avec le temps restant.
inline constexpr std::string_view kStatusTimePlaceholder = "%s";

// ⚠ RELEVÉ : sept lignes au plus dans cette installation. La borne existe parce
// qu'une table lue par index s'arrête à un trou, et qu'un trou suivi d'une ligne
// perdrait la fin sans rien dire.
inline constexpr double kMostDescriptionLines = 16.0;

// LE RANG D'AFFICHAGE, tel que `stateiconimginfo` le range : ses images ne sont
// pas une liste plate mais CINQ SEAUX, et le seau est l'ordre dans lequel le
// client montre ce qui ne tient pas.
enum class StatusPriority : std::uint8_t {
  kGold = 0,
  kRed = 1,
  kBlue = 2,
  kGreen = 3,
  kWhite = 4,
};

inline constexpr std::size_t kStatusPriorities = 5;

struct StatusInfo {
  // Vide quand la table ne nomme pas ce statut : l'appelant écrit alors son
  // numéro, qui se retrouve dans `efstids`.
  std::string name;

  // ⚠ UN NOM NU, ET C'EST UN PIÈGE DÉJÀ PAYÉ. La table rend « BD_Sleep.tga »,
  // sans dossier, là où la table en dur du client rend un chemin complet. Le
  // client s'en sort par son cache de sprites ; un chargeur qui parle au disque
  // doit remettre le dossier lui-même, sinon TOUTES les icônes échouent — en
  // silence.
  std::string icon_file;

  // CE QUE L'ÉTAT FAIT, en toutes lettres — les lignes 2 et suivantes de sa
  // `descript`, sans le nom et sans la place de la durée.
  //
  // 🔴 SIX CENT TRENTE-TROIS STATUTS EN PORTENT UNE, jusqu'à sept lignes, et
  // c'est ce que le joueur vient lire au survol. Une infobulle qui n'écrit que
  // le nom répète l'icône : elle ne dit pas ce que « Impositio Manus » ajoute.
  std::vector<std::string> description;

  // OÙ LA DURÉE S'INSÈRE dans ces lignes, et c'est un RANG, pas un drapeau.
  //
  // ⚠ LE CLIENT LA MET AU MILIEU, pas en tête : la table lui réserve une ligne,
  // qui ne porte que « %s ». Suivre sa mise en page évite que deux infobulles du
  // même jeu se lisent différemment. Zéro = juste sous le nom, ce qui est aussi
  // le repli quand la table ne réserve rien.
  int time_line = 0;

  StatusPriority priority = StatusPriority::kWhite;
};

struct StatusTable {
  std::unordered_map<std::uint32_t, StatusInfo> by_id;

  // Ce que l'installation sait de ce statut, ou rien. ⚠ RIEN N'EST PAS UNE
  // PANNE : le serveur connaît des statuts que cette installation-ci n'a pas.
  [[nodiscard]] const StatusInfo* Find(std::uint32_t effect_id) const;

  // Le nom seul, pour qui n'a que cela à écrire.
  [[nodiscard]] std::string_view NameOf(std::uint32_t effect_id) const;

  [[nodiscard]] std::size_t size() const noexcept { return by_id.size(); }
  [[nodiscard]] bool empty() const noexcept { return by_id.empty(); }

  // Combien portent une icône. ⚠ MOINS QUE LE COMPTE TOTAL, et ce n'est pas un
  // défaut : les deux tables du client ne se recouvrent pas.
  [[nodiscard]] std::size_t drawn() const noexcept;
};

// Lit la chaîne. Un fichier ABSENT rend une table vide plutôt qu'une erreur : un
// statut sans nom se montre encore par son numéro.
[[nodiscard]] core::Result<StatusTable> ReadStatusTable(Vfs& vfs);

}  // namespace ro::vfs
