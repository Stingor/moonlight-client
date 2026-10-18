#include "game/vending_memory.h"

#include <algorithm>
#include <optional>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"
#include "ro/sim/vending.h"

#include "game/yaml_load.h"

namespace moonclient {
namespace {

// Les champs du fichier. ⚠ GRAVÉS : les changer fait oublier à tous les
// marchands leur dernière échoppe, sans que rien ne le dise.
constexpr const char* kGridField = "grille";
constexpr const char* kConfirmField = "confirmer_prix_eleves";
constexpr const char* kShopsField = "echoppes";
constexpr const char* kCharacterField = "personnage";
constexpr const char* kTitleField = "titre";
constexpr const char* kLinesField = "lignes";
constexpr const char* kItemField = "item";
constexpr const char* kRefineField = "raffinement";
constexpr const char* kCardsField = "cartes";
constexpr const char* kAmountField = "quantite";
constexpr const char* kPriceField = "prix";

constexpr const char* kFileHeader =
    "# Les échoppes de vente du joueur, telles que le client les retient.\n"
    "#\n"
    "# « grille » et « confirmer_prix_eleves » sont deux réglages de la fenêtre\n"
    "# de préparation. Sous « echoppes », la DERNIÈRE échoppe ouverte par chaque\n"
    "# personnage : son titre, et chaque ligne avec son objet (identifiant,\n"
    "# raffinement, cartes), sa quantité et son prix unitaire.\n"
    "#\n"
    "# ⚠ TOUT CECI EST AU CLIENT : le serveur n'en sait rien.\n";

bool SameItem(const RememberedVendingLine& line, const ro::sim::InventoryItem& item) {
  return item.item_id == line.item_id && item.refine_level == line.refine_level &&
         item.cards == line.cards;
}

}  // namespace

const RememberedVendingShop* VendingMemory::LastOf(std::string_view character) const {
  const auto found = std::ranges::find_if(shops, [character](const RememberedVendingShop& shop) {
    return shop.character == character;
  });
  return found == shops.end() ? nullptr : &*found;
}

void VendingMemory::Remember(RememberedVendingShop shop) {
  std::erase_if(shops, [&shop](const RememberedVendingShop& kept) {
    return kept.character == shop.character;
  });
  shops.push_back(std::move(shop));
}

std::vector<ResumedVendingLine> ResumeVendingLines(const RememberedVendingShop& shop,
                                                   std::span<const ro::sim::InventoryItem> cart,
                                                   std::size_t permitted_lines) {
  std::vector<ResumedVendingLine> resumed;
  for (const RememberedVendingLine& line : shop.lines) {
    if (resumed.size() >= permitted_lines) {
      break;
    }
    // La première pile de cet objet qu'aucune ligne n'a déjà prise : une pile ne
    // se met en vente qu'une fois.
    const auto stack = std::ranges::find_if(cart, [&](const ro::sim::InventoryItem& item) {
      return item.amount > 0 && SameItem(line, item) &&
             std::ranges::none_of(resumed, [&item](const ResumedVendingLine& taken) {
               return taken.cart_slot == item.client_slot;
             });
    });
    if (stack == cart.end() || line.amount <= 0) {
      continue;
    }
    resumed.push_back(ResumedVendingLine{.cart_slot = stack->client_slot,
                                         .amount = std::min<int>(line.amount, stack->amount),
                                         .price_zeny = line.price_zeny});
  }
  return resumed;
}

std::filesystem::path VendingMemoryPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kVendingMemoryFileName;
}

void LoadVendingMemory(const std::filesystem::path& client_root, VendingMemory& into) {
  std::optional<YAML::Node> whole =
      LoadYamlMap(VendingMemoryPath(client_root), "échoppes", "les défauts s'appliquent");
  if (!whole.has_value()) {
    return;
  }
  try {
    into.grid = (*whole)[kGridField].as<bool>(into.grid);
    into.confirm_high_prices = (*whole)[kConfirmField].as<bool>(into.confirm_high_prices);
    const YAML::Node listed = (*whole)[kShopsField];
    if (!listed || !listed.IsSequence()) {
      return;
    }
    for (const YAML::Node& entry : listed) {
      if (!entry.IsMap()) {
        continue;
      }
      RememberedVendingShop shop;
      shop.character = entry[kCharacterField].as<std::string>(std::string());
      shop.title = entry[kTitleField].as<std::string>(std::string());
      const YAML::Node lines = entry[kLinesField];
      if (lines && lines.IsSequence()) {
        for (const YAML::Node& kept : lines) {
          if (!kept.IsMap() || shop.lines.size() >= ro::sim::kVendingMostLines) {
            continue;
          }
          RememberedVendingLine line;
          line.item_id = kept[kItemField].as<std::uint32_t>(0U);
          line.refine_level = kept[kRefineField].as<int>(0);
          line.amount = kept[kAmountField].as<int>(0);
          line.price_zeny =
              std::min(kept[kPriceField].as<std::uint32_t>(0U), ro::sim::kVendingMostPriceZeny);
          const YAML::Node cards = kept[kCardsField];
          if (cards && cards.IsSequence()) {
            for (std::size_t rank = 0; rank < line.cards.size() && rank < cards.size(); ++rank) {
              line.cards[rank] = cards[rank].as<std::uint32_t>(0U);
            }
          }
          if (line.item_id != 0) {
            shop.lines.push_back(line);
          }
        }
      }
      if (!shop.character.empty()) {
        into.Remember(std::move(shop));
      }
    }
  } catch (const YAML::Exception& broken) {
    // Un champ d'un autre type : ce qui a été lu reste, le reste aux défauts.
    ro::core::LogWarning("échoppes : champ illisible ({})", broken.what());
  }
}

bool SaveVendingMemory(const std::filesystem::path& client_root, const VendingMemory& from) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kGridField << YAML::Value << from.grid;
  out << YAML::Key << kConfirmField << YAML::Value << from.confirm_high_prices;
  out << YAML::Key << kShopsField << YAML::Value << YAML::BeginSeq;
  for (const RememberedVendingShop& shop : from.shops) {
    out << YAML::BeginMap;
    out << YAML::Key << kCharacterField << YAML::Value << shop.character;
    out << YAML::Key << kTitleField << YAML::Value << shop.title;
    out << YAML::Key << kLinesField << YAML::Value << YAML::BeginSeq;
    for (const RememberedVendingLine& line : shop.lines) {
      out << YAML::Flow << YAML::BeginMap;
      out << YAML::Key << kItemField << YAML::Value << line.item_id;
      if (line.refine_level != 0) {
        out << YAML::Key << kRefineField << YAML::Value << line.refine_level;
      }
      if (std::ranges::any_of(line.cards, [](std::uint32_t card) { return card != 0; })) {
        out << YAML::Key << kCardsField << YAML::Value << YAML::Flow << YAML::BeginSeq;
        for (const std::uint32_t card : line.cards) {
          out << card;
        }
        out << YAML::EndSeq;
      }
      out << YAML::Key << kAmountField << YAML::Value << line.amount;
      out << YAML::Key << kPriceField << YAML::Value << line.price_zeny;
      out << YAML::EndMap;
    }
    out << YAML::EndSeq << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;

  return SaveYamlFile(VendingMemoryPath(client_root), kFileHeader, out, "échoppes");
}

}  // namespace moonclient
