// LE COFFRE DU MOT DE PASSE MOONLIGHT : DPAPI, et rien d'autre.

#include <vector>

#include "web/system_secret_store.h"

// clang-format off
#include <windows.h>
#include <wincrypt.h>
#include <dpapi.h>
// clang-format on

namespace moonclient::web {
namespace {

// CE QUE WINDOWS RANGE À CÔTÉ DU SECRET, pour dire de quoi il s'agit.
//
// ⚠ CE N'EST NI UNE CLÉ NI UN MOT DE PASSE : il se relit en clair par qui
// ouvre le bloc, et ne sert qu'aux outils du système. Le nommer clairement
// vaut mieux qu'un blanc dont personne ne saura d'où il vient.
constexpr const wchar_t* kSecretLabel = L"moonclient — mot de passe Moonlight";

// UN BLOC QUE L'API A REMPLI, RECOPIÉ PUIS EFFACÉ SUR PLACE.
//
// 🔴 `SecureZeroMemory` ET NON `memset` : le compilateur a le droit de retirer
// un effacement dont il voit que personne ne relira le résultat, et c'est
// exactement le cas ici. Cet appel-là ne s'optimise pas.
[[nodiscard]] std::string TakeAndWipe(DATA_BLOB& blob) {
  if (blob.pbData == nullptr) {
    return {};
  }
  std::string taken(reinterpret_cast<const char*>(blob.pbData), blob.cbData);
  SecureZeroMemory(blob.pbData, blob.cbData);
  LocalFree(blob.pbData);
  blob.pbData = nullptr;
  blob.cbData = 0;
  return taken;
}

// L'ENTRÉE DE L'API, QUI LA VEUT MODIFIABLE.
//
// ⚠ UNE COPIE PLUTÔT QU'UN `const_cast` : l'API ne promet pas de laisser son
// entrée intacte, et notre appelant, lui, tient une vue sur la sienne.
[[nodiscard]] std::vector<BYTE> Mutable(std::string_view bytes) {
  return std::vector<BYTE>(reinterpret_cast<const BYTE*>(bytes.data()),
                           reinterpret_cast<const BYTE*>(bytes.data()) + bytes.size());
}

}  // namespace

std::string SystemSecretStore::Seal(std::string_view plain) {
  if (plain.empty()) {
    return {};
  }
  std::vector<BYTE> given = Mutable(plain);
  DATA_BLOB from{.cbData = static_cast<DWORD>(given.size()), .pbData = given.data()};
  DATA_BLOB sealed{};
  const bool done =
      CryptProtectData(&from, kSecretLabel, nullptr, nullptr, nullptr, 0, &sealed) != FALSE;
  // ⚠ LA COPIE EN CLAIR S'EFFACE, qu'on ait réussi ou non : elle a vécu le
  // temps d'un appel, et rien ne l'oblige à survivre dans la pile du dessous.
  SecureZeroMemory(given.data(), given.size());
  if (!done) {
    return {};
  }
  return TakeAndWipe(sealed);
}

std::string SystemSecretStore::Unseal(std::string_view sealed) {
  if (sealed.empty()) {
    return {};
  }
  std::vector<BYTE> given = Mutable(sealed);
  DATA_BLOB from{.cbData = static_cast<DWORD>(given.size()), .pbData = given.data()};
  DATA_BLOB plain{};
  // ⚠ UN ÉCHEC EST UN CAS ORDINAIRE : fichier d'un autre compte Windows,
  // d'une autre machine, ou abîmé. L'appelant le lit comme « rien de retenu ».
  if (CryptUnprotectData(&from, nullptr, nullptr, nullptr, nullptr, 0, &plain) == FALSE) {
    return {};
  }
  return TakeAndWipe(plain);
}

}  // namespace moonclient::web
