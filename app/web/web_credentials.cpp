#include "web/web_credentials.h"

#include <cstddef>
#include <exception>
#include <system_error>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/save_folder.h"

namespace moonclient::web {
namespace {

// Les clés du fichier. ⚠ GRAVÉES : les changer perd la mémoire de tous les
// joueurs sans que rien ne le dise.
constexpr const char* kMemberField = "login";
constexpr const char* kSealedField = "sealed_secret";

constexpr const char* kFileHeader =
    "# Ce que le client retient de la porte Moonlight.\n"
    "#\n"
    "# `login` est le pseudonyme du site, en clair : ce n'est qu'un nom.\n"
    "# `sealed_secret` est le mot de passe du SITE, scelle par Windows pour CE\n"
    "# compte d'ouverture de session : copie ailleurs, ce fichier ne rend rien.\n"
    "#\n"
    "# Le mot de passe du JEU n'est jamais ici, ni nulle part. Effacer ce\n"
    "# fichier ne perd rien d'autre que d'avoir a retaper.\n";

// LE SCELLÉ S'ÉCRIT EN HEXADÉCIMAL, parce que le fichier est du TEXTE.
//
// ⚠ ET CE N'EST PAS UN ORNEMENT : ce que le système rend est un bloc d'octets
// quelconques, l'octet nul compris. L'y poser tel quel demanderait au lecteur
// de deviner un encodage, et un nul couperait la valeur en deux sans que rien
// ne s'en plaigne.
[[nodiscard]] std::string HexOf(std::string_view bytes) {
  constexpr std::string_view kDigits = "0123456789abcdef";
  std::string written;
  written.reserve(bytes.size() * 2);
  for (const char byte : bytes) {
    const auto value = static_cast<unsigned char>(byte);
    written.push_back(kDigits[value >> 4U]);
    written.push_back(kDigits[value & 0x0FU]);
  }
  return written;
}

// Le chiffre que porte ce caractère, ou -1.
[[nodiscard]] int DigitOf(char letter) {
  if (letter >= '0' && letter <= '9') {
    return letter - '0';
  }
  if (letter >= 'a' && letter <= 'f') {
    return letter - 'a' + 10;
  }
  return -1;
}

// Rend les octets, ou VIDE si le texte n'est pas de l'hexadécimal entier.
//
// ⚠ ELLE REFUSE PLUTÔT QUE DE DEVINER : un fichier abîmé qui rendrait des
// octets à moitié justes irait jusqu'au système, qui refuserait de l'ouvrir —
// et le joueur lirait un message sur son compte Windows pour un fichier
// simplement tronqué.
[[nodiscard]] std::string BytesOf(std::string_view hex) {
  if (hex.empty() || hex.size() % 2 != 0) {
    return {};
  }
  std::string bytes;
  bytes.reserve(hex.size() / 2);
  for (std::size_t rank = 0; rank + 1 < hex.size(); rank += 2) {
    const int high = DigitOf(hex[rank]);
    const int low = DigitOf(hex[rank + 1]);
    if (high < 0 || low < 0) {
      return {};
    }
    bytes.push_back(static_cast<char>(static_cast<unsigned char>(high * 16 + low)));
  }
  return bytes;
}

// Le champ, ou une chaîne vide — sans jamais lancer sur un fichier mal formé.
[[nodiscard]] std::string ScalarOf(const YAML::Node& whole, const char* field) {
  if (!whole.IsMap()) {
    return {};
  }
  const YAML::Node one = whole[field];
  if (!one || !one.IsScalar()) {
    return {};
  }
  return one.as<std::string>(std::string());
}

}  // namespace

std::filesystem::path WebCredentialsPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kWebCredentialsFileName;
}

WebCredentials LoadWebCredentials(const std::filesystem::path& client_root, SecretStore& vault) {
  WebCredentials found;
  const std::filesystem::path path = WebCredentialsPath(client_root);
  YAML::Node whole;
  try {
    std::error_code quietly;
    if (!std::filesystem::exists(path, quietly)) {
      return found;
    }
    whole = YAML::LoadFile(path.string());
  } catch (const std::exception& trouble) {
    // ⚠ UN FICHIER ILLISIBLE N'EMPÊCHE PAS DE JOUER : on repart d'une mémoire
    // vide, et la prochaine connexion la réécrit proprement.
    ro::core::LogWarning("compte Moonlight retenu : fichier illisible ({})", trouble.what());
    return found;
  }

  found.member = ScalarOf(whole, kMemberField);
  // 🔴 LE DRAPEAU SE DÉDUIT DE CE QU'ON A TROUVÉ, il ne se lit pas. Une case
  // cochée dans le fichier, sans la valeur qui va avec, ferait promettre à
  // l'écran une mémoire qui n'est plus là.
  found.remember_member = !found.member.empty();

  const std::string hex = ScalarOf(whole, kSealedField);
  if (hex.empty() || hex.size() > kSealedSecretMaxBytes * 2) {
    return found;
  }
  const std::string sealed = BytesOf(hex);
  if (sealed.empty()) {
    ro::core::LogWarning("mot de passe Moonlight retenu : le secret est abîmé — il est oublié");
    return found;
  }
  // ⚠ LE SYSTÈME PEUT REFUSER, ET CE N'EST PAS UNE PANNE : un fichier venu
  // d'une autre machine ou d'un autre compte Windows ne s'ouvre pas. Cela vaut
  // « rien de retenu », et le joueur retape.
  found.password = vault.Unseal(sealed);
  found.remember_password = !found.password.empty();
  found.remember_member = found.remember_member || found.remember_password;
  return found;
}

bool SaveWebCredentials(const std::filesystem::path& client_root, SecretStore& vault,
                        const WebCredentials& wanted) {
  const std::filesystem::path path = WebCredentialsPath(client_root);
  const bool keep_member = wanted.remember_member && !wanted.member.empty();
  // ⚠ LE SECRET SUIT L'IDENTIFIANT : sans nom à remplir, un mot de passe
  // retenu ne servirait personne et resterait sur le disque pour rien.
  const bool keep_password = keep_member && wanted.remember_password && !wanted.password.empty();

  std::string hex;
  if (keep_password) {
    const std::string sealed = vault.Seal(wanted.password);
    if (sealed.empty()) {
      // 🔴 LE SYSTÈME A REFUSÉ DE SCELLER, ET L'ON N'ÉCRIT RIEN. Se rabattre
      // sur le clair poserait sur le disque, sans le dire, exactement ce que
      // le joueur avait demandé de protéger.
      ro::core::LogWarning(
          "mot de passe Moonlight : le système a refusé de le sceller — il n'est pas retenu");
    } else {
      hex = HexOf(sealed);
    }
  }

  if (!keep_member && hex.empty()) {
    // ⚠ LE FICHIER DISPARAÎT, IL NE SE VIDE PAS. Un fichier vide laisserait
    // croire, au prochain dépannage, qu'une mémoire existe et qu'elle est
    // cassée.
    std::error_code quietly;
    std::filesystem::remove(path, quietly);
    return true;
  }

  YAML::Node whole(YAML::NodeType::Map);
  if (keep_member) {
    whole[kMemberField] = wanted.member;
  }
  if (!hex.empty()) {
    whole[kSealedField] = hex;
  }
  YAML::Emitter out;
  out << whole;
  std::string content = kFileHeader;
  content += out.c_str();
  content += "\n";
  if (const ro::core::Result<void> written = ro::core::WriteFileAtomically(path, content);
      !written) {
    ro::core::LogWarning("compte Moonlight retenu : {}", ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient::web
