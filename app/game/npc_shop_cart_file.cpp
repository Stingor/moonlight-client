#include "game/npc_shop_cart_file.h"

#include <algorithm>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

// Les clés du fichier : en changer une rendrait illisibles les fichiers déjà
// écrits chez le joueur.
constexpr const char* kAccountsField = "npc_shop_carts";
constexpr const char* kAccountField = "aid";
constexpr const char* kCartsField = "carts";
constexpr const char* kNameField = "name";
constexpr const char* kItemsField = "items";
constexpr const char* kItemIdField = "id";
constexpr const char* kAmountField = "amount";
constexpr const char* kItemNameField = "item";

constexpr const char* kFileHeader =
    "# Les paniers d'achat enregistres de moonclient, par compte.\n"
    "#\n"
    "# aid est l'identifiant du compte ; chaque ligne d'un panier nomme l'item\n"
    "# (id), la quantite (amount) et le nom montre a l'enregistrement (item).\n";

[[nodiscard]] std::optional<ro::sim::SavedCartLine> ReadLine(const YAML::Node& entry) {
  if (!entry.IsMap()) {
    return std::nullopt;
  }
  const YAML::Node item_id = entry[kItemIdField];
  const YAML::Node amount = entry[kAmountField];
  if (!item_id || !item_id.IsScalar() || !amount || !amount.IsScalar()) {
    return std::nullopt;
  }
  ro::sim::SavedCartLine line;
  line.item_id = item_id.as<std::uint32_t>(0);
  line.amount = amount.as<std::int64_t>(0);
  if (const YAML::Node item_name = entry[kItemNameField]; item_name && item_name.IsScalar()) {
    line.item_name = item_name.Scalar();
  }
  return line;
}

[[nodiscard]] std::optional<ro::sim::SavedCart> ReadCart(const YAML::Node& entry) {
  if (!entry.IsMap()) {
    return std::nullopt;
  }
  const YAML::Node name = entry[kNameField];
  const YAML::Node items = entry[kItemsField];
  if (!name || !name.IsScalar() || !items || !items.IsSequence()) {
    return std::nullopt;
  }
  std::vector<ro::sim::SavedCartLine> read;
  for (const YAML::Node& item : items) {
    if (std::optional<ro::sim::SavedCartLine> line = ReadLine(item); line.has_value()) {
      read.push_back(std::move(*line));
    }
  }
  // Le carnet range le panier comme s'il venait d'être enregistré : nom borné,
  // lignes vides écartées, quantités bornées à une pile.
  ro::sim::SavedCartBook tidy;
  if (tidy.Save(name.Scalar(), read) != ro::sim::SavedCartRefusal::kNone) {
    return std::nullopt;
  }
  return *tidy.at(0);
}

[[nodiscard]] std::vector<ro::sim::SavedCart> ReadCarts(const YAML::Node& list) {
  std::vector<ro::sim::SavedCart> carts;
  if (!list || !list.IsSequence()) {
    return carts;
  }
  for (const YAML::Node& entry : list) {
    if (carts.size() >= ro::sim::kSavedCartLimit) {
      break;
    }
    std::optional<ro::sim::SavedCart> cart = ReadCart(entry);
    if (!cart.has_value()) {
      continue;
    }
    // Deux paniers du même nom : le premier reste, comme le carnet le veut.
    if (std::ranges::find(carts, cart->name, &ro::sim::SavedCart::name) == carts.end()) {
      carts.push_back(std::move(*cart));
    }
  }
  return carts;
}

enum class FileState : std::uint8_t { kMissing, kBroken, kRead };

struct FileRead {
  FileState state = FileState::kMissing;
  SavedCartShelf shelf;
};

[[nodiscard]] FileRead ReadFile(const std::filesystem::path& client_root) {
  const std::filesystem::path path = NpcShopCartFilePath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return {};
  }
  // ⚠ LE FLUX S'OUVRE PAR LE CHEMIN LARGE : un dossier hors de la page ANSI
  // resterait introuvable par un nom étroit.
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    ro::core::LogWarning("paniers enregistrés : {} ne s'ouvre pas", ro::core::Utf8FromPath(path));
    return FileRead{.state = FileState::kBroken};
  }
  const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
  ro::core::Result<SavedCartShelf> shelf = ReadSavedCartShelf(text);
  if (!shelf) {
    ro::core::LogWarning("paniers enregistrés : {} illisible ({})", ro::core::Utf8FromPath(path),
                         ro::core::Describe(shelf.error()));
    return FileRead{.state = FileState::kBroken};
  }
  return FileRead{.state = FileState::kRead, .shelf = std::move(*shelf)};
}

}  // namespace

std::filesystem::path NpcShopCartFilePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kNpcShopCartFileName;
}

ro::core::Result<SavedCartShelf> ReadSavedCartShelf(std::string_view text) {
  SavedCartShelf shelf;
  try {
    const YAML::Node base = YAML::Load(std::string(text));
    // Un fichier vide, ou dont la racine n'est pas une table, ne range rien.
    if (!base || !base.IsMap()) {
      return shelf;
    }
    const YAML::Node accounts = base[kAccountsField];
    if (!accounts || !accounts.IsSequence()) {
      return shelf;
    }
    for (const YAML::Node& entry : accounts) {
      if (!entry.IsMap() || !entry[kAccountField] || !entry[kAccountField].IsScalar()) {
        continue;
      }
      const auto account_id = entry[kAccountField].as<std::uint32_t>(0);
      if (account_id == 0) {
        continue;
      }
      std::vector<ro::sim::SavedCart> carts = ReadCarts(entry[kCartsField]);
      if (!carts.empty()) {
        shelf[account_id] = std::move(carts);
      }
    }
  } catch (const YAML::Exception& broken) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData, broken.what());
  }
  return shelf;
}

std::string WriteSavedCartShelf(const SavedCartShelf& shelf) {
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kAccountsField << YAML::Value << YAML::BeginSeq;
  for (const auto& [account_id, carts] : shelf) {
    if (carts.empty()) {
      continue;
    }
    out << YAML::BeginMap << YAML::Key << kAccountField << YAML::Value << account_id << YAML::Key
        << kCartsField << YAML::Value << YAML::BeginSeq;
    for (const ro::sim::SavedCart& cart : carts) {
      out << YAML::BeginMap << YAML::Key << kNameField << YAML::Value << cart.name << YAML::Key
          << kItemsField << YAML::Value << YAML::BeginSeq;
      for (const ro::sim::SavedCartLine& line : cart.lines) {
        out << YAML::Flow << YAML::BeginMap << YAML::Key << kItemIdField << YAML::Value
            << line.item_id << YAML::Key << kAmountField << YAML::Value << line.amount << YAML::Key
            << kItemNameField << YAML::Value << line.item_name << YAML::EndMap;
      }
      out << YAML::EndSeq << YAML::EndMap;
    }
    out << YAML::EndSeq << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;
  return std::string(kFileHeader) + out.c_str() + "\n";
}

std::vector<ro::sim::SavedCart> LoadSavedCarts(const std::filesystem::path& client_root,
                                               std::uint32_t account_id) {
  FileRead read = ReadFile(client_root);
  const auto mine = read.shelf.find(account_id);
  if (mine == read.shelf.end()) {
    return {};
  }
  return std::move(mine->second);
}

bool SaveSavedCarts(const std::filesystem::path& client_root, std::uint32_t account_id,
                    std::span<const ro::sim::SavedCart> carts) {
  FileRead read = ReadFile(client_root);
  if (read.state == FileState::kBroken) {
    ro::core::LogWarning(
        "paniers enregistrés : {} n'est pas réécrit tant qu'il reste illisible — il porte "
        "ceux des autres comptes",
        ro::core::Utf8FromPath(NpcShopCartFilePath(client_root)));
    return false;
  }
  if (carts.empty()) {
    read.shelf.erase(account_id);
  } else {
    read.shelf[account_id].assign(carts.begin(), carts.end());
  }
  if (const ro::core::Result<void> written = ro::core::WriteFileAtomically(
          NpcShopCartFilePath(client_root), WriteSavedCartShelf(read.shelf));
      !written) {
    ro::core::LogWarning("paniers enregistrés : écriture impossible ({})",
                         ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
