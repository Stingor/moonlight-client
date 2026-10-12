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
// la robe vient de garment_sprites.yml ; le corps est celui que le
// sprite de corps nomme — monture comprise —, et un costume ne le change pas.
constexpr std::string_view kGarmentFolder = "로브";
constexpr std::string_view kDoramGarmentSuffix = "_Doram";

// Le client ne dessine pas de cape à une femme de ces métiers : le Madogear,
// sous ses trois formes.
constexpr std::array<std::uint32_t, 3> kGarmentlessFemaleJobs = {4086, 4087, 4112};
constexpr std::uint8_t kFemale = 0;

// ── Devant ou derrière (garment_layers.yml) ──────────────────────────────────
//
// Le client le demande pour la robe, le sexe, le métier, l'action et l'image.
// La cape passe DERRIÈRE quand l'image figure dans la liste de son action,
// dans la table du métier — celui dont le sien hérite, par `inherited_jobs` —,
// petite ou grande selon la robe (`small_layer` de garment_sprites.yml), la
// grande quand rien n'est dit. Tout ce qui manque dessine devant : c'est aussi
// la réponse du client quand sa table se tait.

// La table de couches d'une robe et d'un sexe.
const GarmentLayerTable& LayerTable(const SpriteCatalog& catalog, const ActorLook& look) {
  const SpriteTables& tables = catalog.tables;
  if (tables.small_layer_garments.contains(look.robe)) {
    return look.sex == kFemale ? tables.small_female_layers : tables.small_male_layers;
  }
  return look.sex == kFemale ? tables.big_female_layers : tables.big_male_layers;
}

// Le métier dont un métier prend la table de couches : lui-même, sauf héritage.
std::uint32_t LayerJob(const SpriteCatalog& catalog, std::uint32_t job) {
  const std::uint32_t* inherited = FindSpriteEntry(catalog.tables.layer_inherited_jobs, job);
  return inherited != nullptr ? *inherited : job;
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
  const std::optional<std::string> robe = TableName(catalog.tables.garment_sprites, look.robe);
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
  const GarmentLayerFrames* frames =
      FindSpriteEntry(LayerTable(catalog, look), LayerJob(catalog, BodyJob(look)));
  if (frames != nullptr) {
    placement.behind_frames = *frames;
  }
  return placement;
}

}  // namespace ro::render
