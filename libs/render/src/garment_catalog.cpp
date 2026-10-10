#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/render/sprite_catalog.h"

#include "sprite_catalog_internal.h"

namespace ro::render {
namespace {

using catalog_internal::BodyJob;
using catalog_internal::IsDoram;
using catalog_internal::Join;
using catalog_internal::kSeparator;
using catalog_internal::kSpriteRoot;
using catalog_internal::PlayerBodyName;
using catalog_internal::SexToken;
using catalog_internal::TableName;
using catalog_internal::TableNumber;

// ── Le chemin d'une cape ────────────────────────────────────────────────────
//
// Trois gabarits :
//
//   sprite\로브\<robe>\<robe>                  à plat
//   sprite\로브\<robe>\<robe>_Doram            à plat, pour un Doram
//   sprite\로브\<robe>\<sexe>\<corps>_<sexe>   par métier
//
// Le .spr se cherche à plat d'abord — la variante Doram REMPLACE la plate, elle
// ne s'y ajoute pas — puis par métier ; le .act, toujours par métier. Le nom de
// la robe vient de RobeNameTable (ReqRobSprName) ; le corps est celui que le
// sprite de corps nomme — monture comprise —, et un costume ne le change pas.
constexpr std::string_view kGarmentFolder = "로브";
constexpr std::string_view kDoramGarmentSuffix = "_Doram";

// Le client ne dessine pas de cape à une femme de ces métiers : le Madogear,
// sous ses trois formes.
constexpr std::array<std::uint32_t, 3> kGarmentlessFemaleJobs = {4086, 4087, 4112};
constexpr std::uint8_t kFemale = 0;

// ── Devant ou derrière (_New_DrawOnTop, spreditinfo\_new_2dlayerdir_f) ──────
//
// Le client le demande à Lua pour la robe, le sexe, le métier, l'action et
// l'image. La cape passe DERRIÈRE quand l'image
// figure dans la liste de son action, dans la table du métier — celui dont le
// sien hérite, par LAYER_ALL_TABLE_LIST —, petite ou grande selon
// LayerSizeTypeList, qui ne nomme que la grande. Tout ce qui manque dessine
// devant : c'est aussi la réponse du client quand Lua se tait.

// La table de couches d'une robe et d'un sexe, ou nullptr si elle manque.
const formats::LuaTable* LayerTable(const SpriteCatalog& catalog, const ActorLook& look) {
  const std::optional<double> size = TableNumber(catalog, kLayerSizeTypeListGlobal, look.robe);
  const bool small = size.has_value() && *size == kSmallLayerSizeType;
  const LayerTableGlobals& names = small ? kSmallLayerTableGlobals : kBigLayerTableGlobals;
  const formats::LuaValue* table =
      catalog.globals.Find(look.sex == kFemale ? names.female : names.male);
  return table != nullptr ? table->table.get() : nullptr;
}

// Le métier dont un métier prend la table de couches : lui-même, sauf héritage.
std::uint32_t LayerJob(const SpriteCatalog& catalog, std::uint32_t job) {
  const std::optional<double> inherited = TableNumber(catalog, kLayerJobInheritanceGlobal, job);
  return inherited.has_value() ? static_cast<std::uint32_t>(*inherited) : job;
}

}  // namespace

std::optional<ActorPieceFiles> GarmentFiles(const SpriteCatalog& catalog, const ActorLook& look) {
  if (!look.player || look.robe == 0) {
    return std::nullopt;
  }
  if (look.sex == kFemale &&
      std::ranges::find(kGarmentlessFemaleJobs, look.job) != kGarmentlessFemaleJobs.end()) {
    return std::nullopt;
  }
  const std::optional<std::string> robe = TableName(catalog, kRobeNameTableGlobal, look.robe);
  if (!robe.has_value() || robe->empty()) {
    return std::nullopt;
  }
  const std::string_view sex = SexToken(look.sex);
  const std::string folder = Join({kSpriteRoot, kGarmentFolder, kSeparator, *robe, kSeparator});

  ActorPieceFiles files;
  files.part = ActorPart::kGarment;
  files.optional = true;
  files.candidates.push_back(
      Join({folder, *robe, IsDoram(look.job) ? kDoramGarmentSuffix : std::string_view()}));
  files.animation_path = Join({folder, sex, kSeparator, PlayerBodyName(BodyJob(look)), "_", sex});
  files.candidates.push_back(files.animation_path);
  return files;
}

GarmentPlacement GarmentPlacementFor(const SpriteCatalog& catalog, const ActorLook& look) {
  GarmentPlacement placement;
  if (!look.player || look.robe == 0) {
    return placement;
  }
  const formats::LuaTable* table = LayerTable(catalog, look);
  if (table == nullptr) {
    return placement;
  }
  const formats::LuaValue* actions =
      table->Find(static_cast<double>(LayerJob(catalog, BodyJob(look))));
  if (actions == nullptr || actions->table == nullptr) {
    return placement;
  }
  for (const auto& [action, frames] : actions->table->by_index) {
    if (frames.table == nullptr || action < 0) {
      continue;
    }
    std::vector<std::size_t>& behind = placement.behind_frames[static_cast<std::size_t>(action)];
    for (const auto& listed : frames.table->by_index) {
      const formats::LuaValue& frame = listed.second;
      if (frame.kind == formats::LuaValue::Kind::kNumber && frame.number >= 0) {
        behind.push_back(static_cast<std::size_t>(frame.number));
      }
    }
  }
  return placement;
}

}  // namespace ro::render
