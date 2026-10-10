#pragma once

#include <string>
#include <string_view>

#include "web/secret_store.h"

namespace moonclient::web {

// LE COFFRE DU SYSTÈME.
//
// - Sous Windows, DPAPI (`CryptProtectData`) : la clé est celle du compte
//   d'ouverture de session, et le système ne la rend jamais.
// - Sous Linux, le trousseau de la session (Secret Service : GNOME Keyring,
//   KWallet) garde une clé aléatoire, et le secret est scellé par AES-256-GCM.
//   Le trousseau la déverrouille avec la session et ne la rend qu'à elle.
//
// Dans les deux cas, un fichier copié sur une autre machine, ou lu depuis un
// autre compte, ne rend rien — et c'est aussi tout ce que cela achète : voir
// `SecretStore`, qui dit la propriété exacte.
//
// ⚠ SANS TROUSSEAU — une session sans Secret Service, comme sous WSL —, sceller
// échoue, et rien n'est retenu : c'est le contrat de `SecretStore`.
//
// ⚠ AUCUNE ENTROPIE À NOUS, DÉLIBÉRÉMENT. La nôtre devrait vivre dans le
// binaire pour qu'on puisse rouvrir : elle serait donc à la portée de qui lit
// le fichier. Une serrure dont la clé est peinte sur la porte se compte pour
// zéro, et la compter pour une est pire.
//
// ⚠ SANS ÉTAT : deux instances valent une, et la classe n'existe que parce que
// la politique parle à une INTERFACE — c'est elle qui rend `web_credentials`
// essayable là où le système n'a pas de coffre.
class SystemSecretStore final : public SecretStore {
 public:
  [[nodiscard]] std::string Seal(std::string_view plain) override;
  [[nodiscard]] std::string Unseal(std::string_view sealed) override;
};

}  // namespace moonclient::web
