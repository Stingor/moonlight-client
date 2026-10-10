#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LE NOM AFFICHABLE D'UN MÉTIER : « Lord Knight », pas « knight_h ».
//
// 🔴 CE N'EST PAS LE NOM DE RESSOURCE, et c'est toute la difficulté. Le
// catalogue de sprites connaît celui des DOSSIERS — en coréen, et sans rapport
// avec ce qu'on montre. Celui-ci vit dans une table à part, que le client charge
// pour sa fiche de personnage et ses commandes d'administration.
//
// ⚠ DEUX FICHIERS, ET L'ORDRE EST UNE DÉPENDANCE : `pcjobname` écrit
// `[JOBID.JT_KNIGHT]`, des clés que seul `jobidentity` sait résoudre. Lu seul, il
// se refuse — comme chez le client, qui enchaîne les siens dans un même état Lua.
inline constexpr std::array<std::string_view, 2> kJobNameFiles = {
    "data\\luafiles514\\lua files\\datainfo\\jobidentity.lub",
    "data\\luafiles514\\lua files\\datainfo\\pcjobname.lub",
};

inline constexpr std::string_view kJobNameGlobal = "PCJobNameTable";

// 🔴 LE NOM QUE LA SECONDE TABLE EMPLOIE N'EST PAS CELUI QUE LA PREMIERE DEFINIT.
// `jobidentity` ecrit `JTtbl` ; `pcjobname` indexe `JOBID`. Rien dans
// l'installation ne pose le second -- releve sur les soixante-dix tables de
// `datainfo`, `admin` et `cls` : deux fichiers l'emploient, AUCUN ne l'ecrit.
// C'est le client qui le cree avant d'evaluer les siens.
inline constexpr std::string_view kJobIdentityGlobal = "JTtbl";
inline constexpr std::string_view kJobIdentityAlias = "JOBID";

// ⚠ LA VARIANTE DE SEXE EXISTE — `pcjobnamegender.lub` — ET NE SERT PAS. C'est
// le nom de BASE que la fiche montre, partout le même : un nom qui changerait
// avec le sexe ferait diverger la fiche de la fenêtre de groupe.

struct JobNames {
  std::unordered_map<std::uint32_t, std::string> by_id;

  // Le nom d'un métier, ou rien. ⚠ RIEN N'EST PAS UNE PANNE : la table ignore
  // les classes que ce client ne connaît pas encore, et la fiche écrit alors ses
  // niveaux seuls.
  [[nodiscard]] std::string_view Find(std::uint32_t job_id) const;

  [[nodiscard]] std::size_t size() const noexcept { return by_id.size(); }
  [[nodiscard]] bool empty() const noexcept { return by_id.empty(); }
};

// Lit la chaîne. Un fichier ABSENT rend une table vide plutôt qu'une erreur : un
// métier sans nom se lit encore par ses niveaux.
[[nodiscard]] core::Result<JobNames> ReadJobNames(Vfs& vfs);

}  // namespace ro::vfs
