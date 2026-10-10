#pragma once

#include <cstdint>
#include <string>

namespace ro::sim {

// UN COFFRE QUE CE PERSONNAGE PEUT OUVRIR.
//
// 🔴 LE CLIENT N'EN CONNAÎT AUCUN D'AVANCE. Les coffres alternatifs vivent dans
// `conf/inter_server.yml`, et la liste que le joueur peut atteindre dépend de
// ses DROITS — le serveur la filtre commande par commande (`pc_can_use_command`
// sur « storage », « storagealt1 »…). Elle est donc poussée, comme celle des
// canaux : le serveur possède la liste et ses droits, le client dessine ce
// qu'il reçoit.
//
// ⚠ ET L'ORDRE EST CELUI DU SERVEUR, par numéro croissant et non par ordre de
// table de hachage : il doit être le même d'une ouverture à l'autre et d'un
// joueur à l'autre, sans quoi les onglets danseraient.
struct StorageShelf {
  std::uint8_t id = 0;
  std::string name;

  [[nodiscard]] friend bool operator==(const StorageShelf&, const StorageShelf&) = default;
};

// Le coffre PRINCIPAL, celui qu'« @storage » ouvre.
inline constexpr std::uint8_t kMainStorageId = 0;

// « Aucun coffre ouvert », tel que le serveur l'écrit dans sa liste.
//
// ⚠ CE N'EST PAS ZÉRO, et confondre les deux ferait passer le coffre principal
// pour fermé — ou l'inverse. Zéro EST un coffre.
inline constexpr std::uint8_t kNoStorageOpen = 0xFF;

}  // namespace ro::sim
