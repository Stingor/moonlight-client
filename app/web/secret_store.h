#pragma once

#include <string>
#include <string_view>

namespace moonclient::web {

// SCELLER UN SECRET AVANT DE L'ÉCRIRE SUR LE DISQUE, ET LE ROUVRIR.
//
// 🔴 CE N'EST PAS UN CHIFFREMENT À NOUS, ET CE NE DOIT PAS L'ÊTRE. Un secret
// qu'un programme rouvre tout seul ne peut pas être protégé par une clé que ce
// même programme porte : elle serait dans le binaire, donc à la portée de qui
// lit le fichier. La seule protection qui vaille vient du SYSTÈME, qui tient
// une clé par compte d'ouverture de session et ne la rend qu'à lui — DPAPI
// sous Windows.
//
// 🔴 CE QU'ELLE ACHÈTE, ET CE QU'ELLE N'ACHÈTE PAS. Elle achète qu'un fichier
// copié sur une autre machine, ou lu depuis un autre compte Windows, ne rende
// rien. Elle n'achète RIEN contre un programme qui tourne déjà sous le compte
// du joueur : celui-là rouvre le secret exactement comme nous. C'est la
// propriété — ni plus, ni moins — de « se souvenir du mot de passe » dans un
// navigateur, et c'est pourquoi la case est décochée par défaut.
//
// ⚠ UNE INTERFACE ET NON UNE FONCTION, pour la même raison que `WebTransport` :
// la plateforme d'un côté, la politique de l'autre, et la seconde s'éprouve là
// où la première n'existe pas.
class SecretStore {
 public:
  SecretStore() = default;
  virtual ~SecretStore() = default;
  SecretStore(const SecretStore&) = delete;
  SecretStore& operator=(const SecretStore&) = delete;
  SecretStore(SecretStore&&) = delete;
  SecretStore& operator=(SecretStore&&) = delete;

  // Les octets scellés, ou VIDE quand le système refuse.
  //
  // 🔴 VIDE VEUT DIRE « JE NE SAIS PAS PROTÉGER CECI », et l'appelant n'écrit
  // alors RIEN. Se rabattre sur le clair écrirait sur le disque, sans le dire,
  // exactement ce qu'on avait demandé de protéger.
  [[nodiscard]] virtual std::string Seal(std::string_view plain) = 0;

  // Le secret en clair, ou VIDE. ⚠ VIDE N'EST PAS UNE PANNE : un fichier venu
  // d'un autre compte, d'une autre machine, ou simplement abîmé, ne s'ouvre
  // pas — et cela vaut « pas de mot de passe retenu ».
  [[nodiscard]] virtual std::string Unseal(std::string_view sealed) = 0;
};

}  // namespace moonclient::web
