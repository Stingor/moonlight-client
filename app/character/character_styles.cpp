#include "character/character_styles.h"

#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"
#include "ro/net/style_outfits.h"
#include "ro/sim/style_code.h"

#include "character/style_yaml.h"

namespace moonclient {
namespace {

// Les champs du fichier. ⚠ GRAVÉS : les changer rend à tous les joueurs une
// tablée sans couleurs, sans que rien ne le dise.
constexpr const char* kCharactersField = "characters";
constexpr const char* kServedField = "served";
constexpr const char* kOutfitField = "outfit";
// Les anciens champs, ceux des codes v7 : lus, jamais plus écrits.
constexpr const char* kFallbackField = "fallback";
constexpr const char* kCodesField = "styles";

constexpr const char* kFileHeader =
    "# Les couleurs de chaque personnage joue sur ce poste, pour la selection.\n"
    "#\n"
    "# Un cache : le serveur de personnages envoie ces couleurs quand il sait le\n"
    "# faire (served: 1). Ce fichier sert tant qu'il ne repond pas, et hors ligne.\n"
    "# Il se reecrit a chaque fois que le serveur les redit. L'effacer ne perd rien.\n"
    "#\n"
    "# Chaque personnage porte sa tenue : les corps de la v7 et les teintures qu'il\n"
    "# affiche, en hexadecimal, au format des tenues partagees.\n";

constexpr std::string_view kHexDigits = "0123456789abcdef";
constexpr unsigned kNibbleBits = 4;
constexpr unsigned kNibbleMask = 0x0F;
// Ce que vaut « a » : les dix chiffres décimaux le précèdent.
constexpr int kDecimalDigits = 10;

[[nodiscard]] std::string HexOf(const std::vector<std::uint8_t>& bytes) {
  std::string hex;
  hex.reserve(bytes.size() * 2);
  for (const std::uint8_t byte : bytes) {
    hex.push_back(kHexDigits[byte >> kNibbleBits]);
    hex.push_back(kHexDigits[byte & kNibbleMask]);
  }
  return hex;
}

[[nodiscard]] std::optional<unsigned> NibbleOf(char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<unsigned>(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return static_cast<unsigned>(c - 'a' + kDecimalDigits);
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<std::vector<std::uint8_t>> BytesOfHex(std::string_view hex) {
  if (hex.size() % 2 != 0 || hex.size() > 2 * ro::net::kOutfitBytesMost) {
    return std::nullopt;
  }
  std::vector<std::uint8_t> bytes;
  bytes.reserve(hex.size() / 2);
  for (std::size_t at = 0; at < hex.size(); at += 2) {
    const std::optional<unsigned> high = NibbleOf(hex[at]);
    const std::optional<unsigned> low = NibbleOf(hex[at + 1]);
    if (!high.has_value() || !low.has_value()) {
      return std::nullopt;
    }
    bytes.push_back(static_cast<std::uint8_t>((*high << kNibbleBits) | *low));
  }
  return bytes;
}

// UNE TENUE TELLE QU'ELLE S'ÉCRIT, ou vide si elle ne s'écrit pas. ⚠ AU-DELÀ DE
// CE QU'UNE TENUE PORTE, les dernières entrées partent : ce sont les exemplaires,
// que le serveur envoie après la tête et les corps.
[[nodiscard]] std::string WrittenOf(const ro::sim::StyleOutfit& outfit) {
  ro::sim::StyleOutfit kept = outfit;
  if (kept.layers.size() > ro::net::kOutfitEntriesMost) {
    kept.layers.resize(ro::net::kOutfitEntriesMost);
  }
  const ro::core::Result<std::vector<std::uint8_t>> bytes = ro::net::EncodeOutfit(kept);
  return bytes ? HexOf(*bytes) : std::string();
}

// L'ANCIENNE FORME : des codes v7 et leur repli.
[[nodiscard]] std::optional<ro::sim::StyleOutfit> FromCodes(const YAML::Node& entry) {
  if (!entry[kCodesField] || !entry[kCodesField].IsSequence()) {
    return std::nullopt;
  }
  ro::sim::StyleVariants variants;
  for (const YAML::Node& code : entry[kCodesField]) {
    const std::optional<ro::sim::StyleCode> read = StyleCodeAt(code);
    // ⚠ UN CODE SANS CORPS NE DÉSIGNE AUCUNE VARIANTE.
    if (read.has_value() && read->body_key != 0) {
      variants.by_body[read->body_key] = read->recipe;
    }
  }
  if (variants.empty()) {
    return std::nullopt;
  }
  const auto fallback = entry[kFallbackField] ? entry[kFallbackField].as<std::uint32_t>(0) : 0U;
  variants.fallback_body =
      variants.by_body.contains(fallback) ? fallback : variants.by_body.begin()->first;
  return OutfitOf(&variants, nullptr);
}

[[nodiscard]] std::optional<ro::sim::StyleOutfit> OutfitAt(const YAML::Node& entry) {
  if (!entry.IsMap()) {
    return std::nullopt;
  }
  if (!entry[kOutfitField]) {
    return FromCodes(entry);
  }
  const std::optional<std::vector<std::uint8_t>> bytes =
      BytesOfHex(entry[kOutfitField].as<std::string>(std::string()));
  if (!bytes.has_value()) {
    return std::nullopt;
  }
  ro::core::Result<ro::sim::StyleOutfit> outfit = ro::net::DecodeOutfit(*bytes);
  if (!outfit || outfit->empty()) {
    return std::nullopt;
  }
  return std::move(*outfit);
}

[[nodiscard]] bool Save(const std::filesystem::path& client_root, const KeptCharacterStyles& kept) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  if (kept.served) {
    out << YAML::Key << kServedField << YAML::Value << 1;
  }
  out << YAML::Key << kCharactersField << YAML::Value << YAML::BeginMap;
  for (const auto& [id, outfit] : kept.characters) {
    const std::string written = WrittenOf(outfit);
    if (written.empty()) {
      continue;
    }
    out << YAML::Key << id << YAML::Value << YAML::BeginMap << YAML::Key << kOutfitField
        << YAML::Value << written << YAML::EndMap;
  }
  out << YAML::EndMap << YAML::EndMap;
  return WriteStyleFile(CharacterStylesPath(client_root), kFileHeader, out,
                        "couleurs des personnages");
}

// Range ou retire `outfit` sous `character_id` dans `kept`. Rend vrai si cela
// change ce qui s'écrit.
bool Put(KeptCharacterStyles& kept, std::uint32_t character_id,
         const ro::sim::StyleOutfit* outfit) {
  const auto known = kept.characters.find(character_id);
  const std::string wanted =
      outfit != nullptr && !outfit->empty() ? WrittenOf(*outfit) : std::string();
  if (wanted.empty()) {
    if (known == kept.characters.end()) {
      return false;
    }
    kept.characters.erase(known);
    return true;
  }
  if (known != kept.characters.end() && WrittenOf(known->second) == wanted) {
    return false;
  }
  kept.characters.insert_or_assign(character_id, *outfit);
  return true;
}

}  // namespace

std::filesystem::path CharacterStylesPath(const std::filesystem::path& client_root) {
  return StyleFilePath(client_root, kCharacterStylesFileName);
}

ro::sim::StyleOutfit OutfitOf(const ro::sim::StyleVariants* variants,
                              const std::vector<ro::sim::StyleLayerRecipe>* layers) {
  ro::sim::StyleOutfit outfit;
  if (variants != nullptr) {
    for (const auto& [body_key, recipe] : variants->by_body) {
      outfit.bodies.emplace_back(body_key, recipe);
    }
    outfit.fallback_body = variants->by_body.contains(variants->fallback_body)
                               ? variants->fallback_body
                               : (outfit.bodies.empty() ? 0U : outfit.bodies.front().first);
  }
  if (layers != nullptr) {
    outfit.layers = *layers;
  }
  return outfit;
}

ro::sim::StyleVariants VariantsOfOutfit(const ro::sim::StyleOutfit& outfit) {
  ro::sim::StyleVariants variants;
  for (const auto& [body_key, recipe] : outfit.bodies) {
    if (body_key != 0) {
      variants.by_body[body_key] = recipe;
    }
  }
  if (!variants.empty()) {
    variants.fallback_body = variants.by_body.contains(outfit.fallback_body)
                                 ? outfit.fallback_body
                                 : variants.by_body.begin()->first;
  }
  return variants;
}

KeptCharacterStyles LoadCharacterStyles(const std::filesystem::path& client_root) {
  KeptCharacterStyles kept;
  const std::optional<YAML::Node> whole =
      ReadStyleFile(CharacterStylesPath(client_root), "couleurs des personnages");
  if (!whole.has_value()) {
    return kept;
  }
  try {
    kept.served = (*whole)[kServedField] && (*whole)[kServedField].as<int>(0) > 0;
    const YAML::Node characters = (*whole)[kCharactersField];
    if (!characters || !characters.IsMap()) {
      return kept;
    }
    for (const auto& entry : characters) {
      // ⚠ ZÉRO N'EST PAS UN PERSONNAGE : c'est ce qu'une clé illisible rend.
      const auto character_id = entry.first.as<std::uint32_t>(0);
      if (character_id == 0) {
        continue;
      }
      if (std::optional<ro::sim::StyleOutfit> outfit = OutfitAt(entry.second)) {
        kept.characters.emplace(character_id, std::move(*outfit));
      }
    }
  } catch (const std::exception& trouble) {
    ro::core::LogWarning("couleurs des personnages : entrée illisible ({})", trouble.what());
  }
  return kept;
}

bool NoteCharacterStyles(const std::filesystem::path& client_root, std::uint32_t character_id,
                         const ro::sim::StyleOutfit* outfit) {
  if (character_id == 0) {
    return false;
  }
  KeptCharacterStyles kept = LoadCharacterStyles(client_root);
  return Put(kept, character_id, outfit) && Save(client_root, kept);
}

bool NoteAccountStyles(const std::filesystem::path& client_root,
                       std::span<const std::uint32_t> listed,
                       const std::map<std::uint32_t, ro::sim::StyleOutfit>& told) {
  KeptCharacterStyles kept = LoadCharacterStyles(client_root);
  bool changed = false;
  for (const std::uint32_t character_id : listed) {
    if (character_id == 0) {
      continue;
    }
    const auto found = told.find(character_id);
    changed = Put(kept, character_id, found != told.end() ? &found->second : nullptr) || changed;
  }
  return changed && Save(client_root, kept);
}

bool NoteCharacterStylesServed(const std::filesystem::path& client_root) {
  KeptCharacterStyles kept = LoadCharacterStyles(client_root);
  if (kept.served) {
    return false;
  }
  kept.served = true;
  return Save(client_root, kept);
}

}  // namespace moonclient
