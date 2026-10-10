// LE COFFRE DU MOT DE PASSE MOONLIGHT SOUS LINUX : une clé dans le trousseau
// de la session, et un sceau AES-256-GCM.

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <libsecret/secret.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include "web/system_secret_store.h"

namespace moonclient::web {
namespace {

// LE NOM DU SCHÉMA ET L'ATTRIBUT QUI DÉSIGNENT LA CLÉ dans le trousseau.
constexpr const char* kSchemaName = "fr.moonlight-destiny.moonclient";
constexpr const char* kUsageAttribute = "usage";
constexpr const char* kUsageSealKey = "seal-key";

// Ce que les outils du trousseau montrent. ⚠ CE N'EST NI UNE CLÉ NI UN MOT DE
// PASSE : le nommer clairement vaut mieux qu'un blanc dont personne ne saura
// d'où il vient.
constexpr const char* kKeyLabel = "moonclient — clé du mot de passe Moonlight";

constexpr std::size_t kKeyBytes = 32;    // AES-256
constexpr std::size_t kNonceBytes = 12;  // la longueur que GCM attend
constexpr std::size_t kTagBytes = 16;

// LE PREMIER OCTET D'UN SCEAU, pour qu'un format futur se reconnaisse.
constexpr char kSealVersion = '\x01';
constexpr std::size_t kSealOverheadBytes = 1 + kNonceBytes + kTagBytes;

using Key = std::array<unsigned char, kKeyBytes>;

// UNE CLÉ QUI S'EFFACE en quittant la portée. 🔴 `OPENSSL_cleanse` et non
// `memset`, que le compilateur a le droit de retirer.
class KeyHolder {
 public:
  KeyHolder() = default;
  KeyHolder(const KeyHolder&) = delete;
  KeyHolder& operator=(const KeyHolder&) = delete;
  KeyHolder(KeyHolder&&) = delete;
  KeyHolder& operator=(KeyHolder&&) = delete;
  ~KeyHolder() { OPENSSL_cleanse(key.data(), key.size()); }
  Key key = {};
};

[[nodiscard]] SecretSchema SealKeySchema() {
  SecretSchema schema = {};
  schema.name = kSchemaName;
  schema.flags = SECRET_SCHEMA_NONE;
  schema.attributes[0].name = kUsageAttribute;
  schema.attributes[0].type = SECRET_SCHEMA_ATTRIBUTE_STRING;
  return schema;
}

[[nodiscard]] int HexValue(char digit) {
  if (digit >= '0' && digit <= '9') {
    return digit - '0';
  }
  if (digit >= 'a' && digit <= 'f') {
    return digit - 'a' + 10;
  }
  return -1;
}

// La clé que le trousseau garde, en hexadécimal. Faux si elle manque, si le
// trousseau ne répond pas, ou si ce qu'il rend n'est pas une clé.
[[nodiscard]] bool LookupKey(Key& key) {
  const SecretSchema schema = SealKeySchema();
  GError* error = nullptr;
  gchar* stored = secret_password_lookup_sync(&schema, nullptr, &error, kUsageAttribute,
                                              kUsageSealKey, nullptr);
  if (error != nullptr) {
    g_error_free(error);
  }
  if (stored == nullptr) {
    return false;
  }
  const std::string_view hex(stored);
  bool readable = hex.size() == key.size() * 2;
  for (std::size_t index = 0; readable && index < key.size(); ++index) {
    const int high = HexValue(hex[index * 2]);
    const int low = HexValue(hex[index * 2 + 1]);
    readable = high >= 0 && low >= 0;
    key[index] = static_cast<unsigned char>(readable ? (high << 4) | low : 0);
  }
  // `secret_password_free` efface avant de libérer.
  secret_password_free(stored);
  return readable;
}

// Tire une clé neuve et la confie au trousseau. Faux si l'un des deux refuse :
// sans trousseau, on ne sait rien protéger.
[[nodiscard]] bool CreateKey(Key& key) {
  if (RAND_bytes(key.data(), static_cast<int>(key.size())) != 1) {
    return false;
  }
  static constexpr std::string_view kHex = "0123456789abcdef";
  std::string hex(key.size() * 2, '\0');
  for (std::size_t index = 0; index < key.size(); ++index) {
    hex[index * 2] = kHex[key[index] >> 4U];
    hex[index * 2 + 1] = kHex[key[index] & 0x0FU];
  }
  const SecretSchema schema = SealKeySchema();
  GError* error = nullptr;
  const gboolean stored =
      secret_password_store_sync(&schema, SECRET_COLLECTION_DEFAULT, kKeyLabel, hex.c_str(),
                                 nullptr, &error, kUsageAttribute, kUsageSealKey, nullptr);
  OPENSSL_cleanse(hex.data(), hex.size());
  if (error != nullptr) {
    g_error_free(error);
  }
  return stored != FALSE;
}

struct CipherFree {
  void operator()(EVP_CIPHER_CTX* context) const { EVP_CIPHER_CTX_free(context); }
};
using Cipher = std::unique_ptr<EVP_CIPHER_CTX, CipherFree>;

[[nodiscard]] const unsigned char* Bytes(std::string_view text) {
  return reinterpret_cast<const unsigned char*>(text.data());
}

[[nodiscard]] unsigned char* Bytes(std::string& text) {
  return reinterpret_cast<unsigned char*>(text.data());
}

}  // namespace

std::string SystemSecretStore::Seal(std::string_view plain) {
  if (plain.empty() || plain.size() > static_cast<std::size_t>(INT_MAX) - kSealOverheadBytes) {
    return {};
  }
  KeyHolder held;
  if (!LookupKey(held.key) && !CreateKey(held.key)) {
    return {};
  }
  // Le sceau : version, nonce, chiffré, étiquette.
  std::string sealed(kSealOverheadBytes + plain.size(), '\0');
  sealed[0] = kSealVersion;
  unsigned char* nonce = Bytes(sealed) + 1;
  unsigned char* cipher_text = nonce + kNonceBytes;
  unsigned char* tag = cipher_text + plain.size();
  if (RAND_bytes(nonce, static_cast<int>(kNonceBytes)) != 1) {
    return {};
  }
  const Cipher context(EVP_CIPHER_CTX_new());
  int written = 0;
  int finished = 0;
  if (!context ||
      EVP_EncryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, held.key.data(), nonce) != 1 ||
      EVP_EncryptUpdate(context.get(), cipher_text, &written, Bytes(plain),
                        static_cast<int>(plain.size())) != 1 ||
      EVP_EncryptFinal_ex(context.get(), cipher_text + written, &finished) != 1 ||
      EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(kTagBytes), tag) !=
          1) {
    return {};
  }
  return sealed;
}

std::string SystemSecretStore::Unseal(std::string_view sealed) {
  // ⚠ UN ÉCHEC EST UN CAS ORDINAIRE : sceau d'un autre compte, d'une autre
  // machine, d'un trousseau remis à zéro, ou abîmé. L'appelant le lit comme
  // « rien de retenu ».
  if (sealed.size() <= kSealOverheadBytes || sealed[0] != kSealVersion ||
      sealed.size() > static_cast<std::size_t>(INT_MAX)) {
    return {};
  }
  KeyHolder held;
  if (!LookupKey(held.key)) {
    return {};
  }
  const std::size_t plain_bytes = sealed.size() - kSealOverheadBytes;
  const unsigned char* nonce = Bytes(sealed) + 1;
  const unsigned char* cipher_text = nonce + kNonceBytes;
  std::array<unsigned char, kTagBytes> tag = {};
  for (std::size_t index = 0; index < kTagBytes; ++index) {
    tag[index] = cipher_text[plain_bytes + index];
  }
  std::string plain(plain_bytes, '\0');
  const Cipher context(EVP_CIPHER_CTX_new());
  int written = 0;
  int finished = 0;
  const bool opened =
      context &&
      EVP_DecryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, held.key.data(), nonce) == 1 &&
      EVP_DecryptUpdate(context.get(), Bytes(plain), &written, cipher_text,
                        static_cast<int>(plain_bytes)) == 1 &&
      EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(kTagBytes),
                          tag.data()) == 1 &&
      // 🔴 C'EST ICI QUE L'ÉTIQUETTE SE VÉRIFIE : un sceau retouché d'un seul
      // bit ne s'ouvre pas, et rien de déchiffré ne sort.
      EVP_DecryptFinal_ex(context.get(), Bytes(plain) + written, &finished) == 1;
  if (!opened) {
    OPENSSL_cleanse(plain.data(), plain.size());
    return {};
  }
  return plain;
}

}  // namespace moonclient::web
