#include "survivors/arena_ground_compose.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace moonclient {
namespace {

constexpr std::size_t kChannels = 4;
constexpr std::uint8_t kQuarterTurns = 4;
constexpr std::uint8_t kOpaque = 255;
constexpr float kFullChannel = 255.0F;
constexpr float kRounding = 0.5F;

// ── LE MÉLANGE ENTIER du bruit : un pas du finaliseur SplitMix64 par
// composante, aux constantes publiées.
constexpr std::uint64_t kGoldenGamma = 0x9E37'79B9'7F4A'7C15ULL;
constexpr std::uint64_t kMixFirst = 0xBF58'476D'1CE4'E5B9ULL;
constexpr std::uint64_t kMixSecond = 0x94D0'49BB'1331'11EBULL;
constexpr unsigned kMixShiftFirst = 30;
constexpr unsigned kMixShiftSecond = 27;
constexpr unsigned kMixShiftLast = 31;
// Le sel du bruit de la lisière, en ASCII : « BLEND ».
constexpr std::uint64_t kBlendSalt = 0x424C'454E'4400'0000ULL;
constexpr double kUnitScale = 1.0 / 18446744073709551616.0;  // 2^-64

// ── LE TROUBLE DE LA LISIÈRE : deux octaves de bruit de valeur, de période en
// cases, et la luminance de l'herbe, écartée de sa moyenne de
// kGrassLuminanceSpan au plus.
struct NoiseOctave {
  double period_cells;
  float weight;
};
constexpr std::array<NoiseOctave, 2> kNoiseOctaves{{{1.0, 0.6F}, {0.35, 0.4F}}};
// L'amplitude du bruit, de crête à crête, et celle de l'herbe, de part et
// d'autre de zéro.
constexpr float kNoiseSpan = 0.2F;
constexpr float kGrassBias = 0.1F;
constexpr float kGrassLuminanceSpan = 40.0F;
constexpr float kRedLuminance = 0.299F;
constexpr float kGreenLuminance = 0.587F;
constexpr float kBlueLuminance = 0.114F;
static_assert(kNoiseSpan / 2.0F + kGrassBias < kBlendLowCover,
              "sans chemin dans le bloc, le trouble ne doit jamais donner de terre");
static_assert(kNoiseSpan / 2.0F + kGrassBias < 1.0F - kBlendHighCover,
              "tout de chemin, le trouble ne doit jamais donner d'herbe");
// En deçà, au-delà, la couverture troublée reste sous le seuil bas, sur le
// seuil haut : l'herbe, la terre, sûrement.
constexpr float kSurelyGrassCover = kBlendLowCover - kNoiseSpan / 2.0F - kGrassBias;
constexpr float kSurelyDirtCover = kBlendHighCover + kNoiseSpan / 2.0F + kGrassBias;

// L'écart du vert au rouge entre l'herbe et la terre en deçà duquel elles ne
// se distinguent pas assez pour lire le masque d'une tache.
constexpr float kLeastGreenContrast = 8.0F;

constexpr std::int32_t kCellsPerCube = kGroundCellsPerCube;
// Le texel de bord se lit à un demi-texel du cube : le carré de couverture,
// au plus loin, doit rester dans l'anneau d'une case. D'où une texture assez
// fine.
constexpr std::int32_t kLeastCubeTexels = 8;
static_assert(kBlendReachCells + static_cast<float>(kCellsPerCube) / kLeastCubeTexels <= 1.0F,
              "le carré de couverture sortirait du bloc");

[[nodiscard]] std::uint64_t Finalize(std::uint64_t value) noexcept {
  value = (value ^ (value >> kMixShiftFirst)) * kMixFirst;
  value = (value ^ (value >> kMixShiftSecond)) * kMixSecond;
  return value ^ (value >> kMixShiftLast);
}

[[nodiscard]] double Lattice(std::uint64_t seed, std::uint64_t salt, std::int64_t x,
                             std::int64_t y) noexcept {
  std::uint64_t value = Finalize(seed + kGoldenGamma);
  value = Finalize(value ^ (salt + kGoldenGamma));
  value = Finalize(value ^ (static_cast<std::uint64_t>(x) + kGoldenGamma));
  return static_cast<double>(Finalize(value ^ (static_cast<std::uint64_t>(y) + kGoldenGamma))) *
         kUnitScale;
}

[[nodiscard]] float Smooth(float t) noexcept { return t * t * (3.0F - 2.0F * t); }

// De `from` vers `to`, à `t` : exact aux deux bouts pour des valeurs entières.
[[nodiscard]] float Blend(float from, float to, float t) noexcept { return from + t * (to - from); }

[[nodiscard]] std::int64_t FloorMod(std::int64_t value, std::int64_t divisor) noexcept {
  const std::int64_t rest = value % divisor;
  return rest < 0 ? rest + divisor : rest;
}

[[nodiscard]] float Luminance(const std::uint8_t* rgba) noexcept {
  return kRedLuminance * rgba[0] + kGreenLuminance * rgba[1] + kBlueLuminance * rgba[2];
}

[[nodiscard]] float GreenOverRed(const std::uint8_t* rgba) noexcept {
  return static_cast<float>(rgba[1]) - static_cast<float>(rgba[0]);
}

// L'herbe et la terre mêlées, `dirt_share` de terre, dans le texel `out`, opaque.
void Mix(const std::uint8_t* grass, const std::uint8_t* dirt, float dirt_share,
         std::uint8_t* out) noexcept {
  for (std::size_t channel = 0; channel + 1 < kChannels; ++channel) {
    const float mixed =
        Blend(static_cast<float>(grass[channel]), static_cast<float>(dirt[channel]), dirt_share);
    out[channel] = static_cast<std::uint8_t>(std::clamp(mixed + kRounding, 0.0F, kFullChannel));
  }
  out[kChannels - 1] = kOpaque;
}

// LE TEXEL DE L'EMPLACEMENT où un motif de `side` texels, posé avec `turn`
// quarts de tour, montre son texel (x, y) : l'inverse de la rotation du motif,
// texel sur texel, ramené dans l'emplacement — au-delà de son bord, c'est la
// période suivante.
std::pair<std::size_t, std::size_t> PlaceOfTexel(std::int64_t x, std::int64_t y, std::int32_t side,
                                                 std::uint8_t turn) noexcept {
  const std::int64_t last = side - 1;
  std::int64_t place_x = x;
  std::int64_t place_y = y;
  switch (turn % kQuarterTurns) {
    case 1:
      place_x = last - y;
      place_y = x;
      break;
    case 2:
      place_x = last - x;
      place_y = last - y;
      break;
    case 3:
      place_x = y;
      place_y = last - x;
      break;
    default:
      break;
  }
  return {static_cast<std::size_t>(FloorMod(place_x, side)),
          static_cast<std::size_t>(FloorMod(place_y, side))};
}

// Une moyenne glissante de 2 × kPatchMaskReachTexels + 1 valeurs, le long des
// rangées ou des colonnes d'une image carrée de `side`, bornée à ses bords.
std::vector<float> BoxBlur(const std::vector<float>& values, std::size_t side, bool along_rows) {
  std::vector<float> blurred(values.size());
  const auto reach = static_cast<std::ptrdiff_t>(kPatchMaskReachTexels);
  const auto last = static_cast<std::ptrdiff_t>(side) - 1;
  for (std::size_t line = 0; line < side; ++line) {
    for (std::size_t along = 0; along < side; ++along) {
      float total = 0.0F;
      float count = 0.0F;
      for (std::ptrdiff_t step = -reach; step <= reach; ++step) {
        const std::ptrdiff_t at = static_cast<std::ptrdiff_t>(along) + step;
        if (at < 0 || at > last) continue;
        const auto other = static_cast<std::size_t>(at);
        total += along_rows ? values[line * side + other] : values[other * side + line];
        count += 1.0F;
      }
      const std::size_t own = along_rows ? line * side + along : along * side + line;
      blurred[own] = total / count;
    }
  }
  return blurred;
}

// UN AXE DE LA TUILE : pour chaque texel, bord compris, sa coordonnée dans le
// monde en cases, sa colonne (ou rangée) dans la texture source, et les
// poids des quatre cases du bloc dans le carré de couverture.
struct TileAxis {
  std::vector<double> world_cells;
  std::vector<std::int32_t> source;
  std::vector<std::array<float, kPathBlockSide>> cover;
};

TileAxis AxisOf(std::int32_t world_cube, std::int32_t cube_texels) {
  const std::int32_t side = cube_texels + 2 * kComposedBorderTexels;
  const std::int64_t texture_texels = static_cast<std::int64_t>(cube_texels) * kGroundTextureCubes;
  TileAxis axis;
  axis.world_cells.reserve(static_cast<std::size_t>(side));
  axis.source.reserve(static_cast<std::size_t>(side));
  axis.cover.reserve(static_cast<std::size_t>(side));
  const double cells_per_texel = static_cast<double>(kCellsPerCube) / cube_texels;
  for (std::int32_t texel = -kComposedBorderTexels; texel < cube_texels + kComposedBorderTexels;
       ++texel) {
    const double inside =
        (static_cast<double>(texel) + static_cast<double>(kRounding)) * cells_per_texel;
    axis.world_cells.push_back(static_cast<double>(world_cube) * kCellsPerCube + inside);
    axis.source.push_back(static_cast<std::int32_t>(
        FloorMod(static_cast<std::int64_t>(world_cube) * cube_texels + texel, texture_texels)));
    // Le point compté depuis le bord sud-ouest de l'anneau, qui commence une
    // case avant le cube ; le carré [point − portée, point + portée] coupe
    // chaque case du bloc sur une longueur.
    const auto in_block = static_cast<float>(inside) + 1.0F;
    const float low = in_block - kBlendReachCells;
    const float high = in_block + kBlendReachCells;
    std::array<float, kPathBlockSide> weights{};
    for (std::int32_t cell = 0; cell < kPathBlockSide; ++cell) {
      const float overlap =
          std::min(high, static_cast<float>(cell + 1)) - std::max(low, static_cast<float>(cell));
      weights[static_cast<std::size_t>(cell)] = std::max(0.0F, overlap) / (2.0F * kBlendReachCells);
    }
    axis.cover.push_back(weights);
  }
  return axis;
}

// UNE OCTAVE DE BRUIT sur la tuile : les nœuds qu'elle touche, tirés une fois,
// puis pour chaque texel son nœud et son poids sur chaque axe.
class TileNoise {
 public:
  TileNoise(std::uint64_t run_seed, std::uint64_t salt, double period, const TileAxis& east,
            const TileAxis& north)
      : first_x_(static_cast<std::int64_t>(std::floor(east.world_cells.front() / period))),
        first_y_(static_cast<std::int64_t>(std::floor(north.world_cells.front() / period))) {
    const auto last_x = static_cast<std::int64_t>(std::floor(east.world_cells.back() / period));
    const auto last_y = static_cast<std::int64_t>(std::floor(north.world_cells.back() / period));
    width_ = static_cast<std::size_t>(last_x - first_x_ + 2);
    const auto height = static_cast<std::size_t>(last_y - first_y_ + 2);
    nodes_.resize(width_ * height);
    for (std::size_t y = 0; y < height; ++y) {
      for (std::size_t x = 0; x < width_; ++x) {
        nodes_[y * width_ + x] =
            static_cast<float>(Lattice(run_seed, salt, first_x_ + static_cast<std::int64_t>(x),
                                       first_y_ + static_cast<std::int64_t>(y)));
      }
    }
    Place(east, period, first_x_, columns_);
    Place(north, period, first_y_, rows_);
  }

  [[nodiscard]] float At(std::size_t column, std::size_t row) const noexcept {
    const auto [x, tx] = columns_[column];
    const auto [y, ty] = rows_[row];
    const float* south = &nodes_[y * width_ + x];
    const float* north = south + width_;
    const float low = Blend(south[0], south[1], tx);
    const float high = Blend(north[0], north[1], tx);
    return Blend(low, high, ty);
  }

 private:
  static void Place(const TileAxis& axis, double period, std::int64_t first,
                    std::vector<std::pair<std::size_t, float>>& placed) {
    placed.reserve(axis.world_cells.size());
    for (const double world : axis.world_cells) {
      const double scaled = world / period;
      const double node = std::floor(scaled);
      placed.emplace_back(static_cast<std::size_t>(static_cast<std::int64_t>(node) - first),
                          Smooth(static_cast<float>(scaled - node)));
    }
  }

  std::int64_t first_x_ = 0;
  std::int64_t first_y_ = 0;
  std::size_t width_ = 0;
  std::vector<float> nodes_;
  std::vector<std::pair<std::size_t, float>> columns_;
  std::vector<std::pair<std::size_t, float>> rows_;
};

}  // namespace

GroundBlend GroundBlendOf(std::uint16_t path_block) noexcept {
  if (path_block == 0) return GroundBlend::kGrass;
  if (path_block == kPathBlockFull) return GroundBlend::kDirt;
  return GroundBlend::kComposed;
}

std::optional<GroundComposeSources> MakeGroundComposeSources(const ro::formats::Image& grass,
                                                             const ro::formats::Image& dirt) {
  if (grass.empty() || dirt.empty() || grass.width != grass.height || dirt.width != grass.width ||
      dirt.height != grass.height || grass.width % kGroundTextureCubes != 0 ||
      grass.width / kGroundTextureCubes < kLeastCubeTexels) {
    return std::nullopt;
  }
  GroundComposeSources sources{
      .grass = grass, .dirt = dirt, .cube_texels = grass.width / kGroundTextureCubes};
  double luminance = 0.0;
  double grass_green = 0.0;
  double dirt_green = 0.0;
  for (std::size_t texel = 0; texel + kChannels <= grass.pixels.size(); texel += kChannels) {
    luminance += static_cast<double>(Luminance(&grass.pixels[texel]));
    grass_green += static_cast<double>(GreenOverRed(&grass.pixels[texel]));
    dirt_green += static_cast<double>(GreenOverRed(&dirt.pixels[texel]));
  }
  const auto texels = static_cast<double>(grass.pixels.size() / kChannels);
  sources.grass_mean_luminance = static_cast<float>(luminance / texels);
  sources.grass_green_over_red = static_cast<float>(grass_green / texels);
  sources.dirt_green_over_red = static_cast<float>(dirt_green / texels);
  return sources;
}

std::optional<ro::formats::Image> ComposePatchTexture(const GroundComposeSources& sources,
                                                      const ro::formats::Image& patch,
                                                      std::uint8_t turn) {
  const std::int32_t side = sources.grass.width;
  const float contrast = sources.grass_green_over_red - sources.dirt_green_over_red;
  if (patch.width != side || patch.height != side ||
      patch.pixels.size() != sources.grass.pixels.size() || contrast < kLeastGreenContrast) {
    return std::nullopt;
  }
  const auto count = static_cast<std::size_t>(side);
  // La part de terre de chaque texel de la tache, puis lissée d'un carré de
  // 2 × kPatchMaskReachTexels + 1 de côté, borné à l'image.
  std::vector<float> dirtness(count * count);
  for (std::size_t texel = 0; texel < dirtness.size(); ++texel) {
    dirtness[texel] = std::clamp(
        (sources.grass_green_over_red - GreenOverRed(&patch.pixels[texel * kChannels])) / contrast,
        0.0F, 1.0F);
  }
  const std::vector<float> mask = BoxBlur(BoxBlur(dirtness, count, true), count, false);
  ro::formats::Image image{.width = side, .height = side, .file = ro::formats::ImageFile::kBmp};
  image.pixels.resize(count * count * kChannels);
  const std::size_t last = count - 1;
  for (std::size_t y = 0; y < count; ++y) {
    for (std::size_t x = 0; x < count; ++x) {
      const auto [place_x, place_y] =
          PlaceOfTexel(static_cast<std::int64_t>(x), static_cast<std::int64_t>(y), side, turn);
      const std::size_t rim = std::min({x, y, last - x, last - y});
      const float share = rim < static_cast<std::size_t>(kPatchRimTexels)
                              ? 0.0F
                              : Smooth(std::clamp((mask[y * count + x] - kBlendLowCover) /
                                                      (kBlendHighCover - kBlendLowCover),
                                                  0.0F, 1.0F));
      const std::size_t source = (place_y * count + place_x) * kChannels;
      Mix(&sources.grass.pixels[source], &sources.dirt.pixels[source], share,
          &image.pixels[(y * count + x) * kChannels]);
    }
  }
  return image;
}

ro::formats::Image BorderMotif(const ro::formats::Image& texture, const ro::formats::Image& beyond,
                               std::uint8_t turn) {
  if (texture.empty() || texture.width != texture.height || beyond.width != texture.width ||
      beyond.height != texture.height) {
    return texture;
  }
  const std::int32_t side = texture.width;
  const std::int32_t bordered = side + 2 * kMotifBorderTexels;
  const auto count = static_cast<std::size_t>(bordered);
  ro::formats::Image image{.width = bordered, .height = bordered, .file = texture.file};
  image.pixels.resize(count * count * kChannels);
  for (std::int32_t y = -kMotifBorderTexels; y < side + kMotifBorderTexels; ++y) {
    for (std::int32_t x = -kMotifBorderTexels; x < side + kMotifBorderTexels; ++x) {
      const bool inside = x >= 0 && y >= 0 && x < side && y < side;
      const std::uint8_t* from = nullptr;
      if (inside) {
        from = &texture.pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(side) +
                                static_cast<std::size_t>(x)) *
                               kChannels];
      } else {
        const auto [place_x, place_y] = PlaceOfTexel(x, y, side, turn);
        from = &beyond.pixels[(place_y * static_cast<std::size_t>(side) + place_x) * kChannels];
      }
      const std::size_t to = (static_cast<std::size_t>(y + kMotifBorderTexels) * count +
                              static_cast<std::size_t>(x + kMotifBorderTexels)) *
                             kChannels;
      std::copy_n(from, kChannels, image.pixels.begin() + static_cast<std::ptrdiff_t>(to));
    }
  }
  return image;
}

std::int32_t ComposedTileSide(const GroundComposeSources& sources) noexcept {
  return sources.cube_texels + 2 * kComposedBorderTexels;
}

void ComposeCubeTile(const GroundComposeSources& sources, std::uint64_t run_seed,
                     std::int32_t world_cube_x, std::int32_t world_cube_y, std::uint16_t path_block,
                     std::span<std::uint8_t> tile) {
  const std::int32_t side = ComposedTileSide(sources);
  const auto side_size = static_cast<std::size_t>(side);
  if (tile.size() < side_size * side_size * kChannels) return;
  const TileAxis east = AxisOf(world_cube_x, sources.cube_texels);
  const TileAxis north = AxisOf(world_cube_y, sources.cube_texels);
  std::vector<TileNoise> octaves;
  octaves.reserve(kNoiseOctaves.size());
  for (std::size_t rank = 0; rank < kNoiseOctaves.size(); ++rank) {
    octaves.emplace_back(run_seed, kBlendSalt + rank, kNoiseOctaves[rank].period_cells, east,
                         north);
  }
  const auto texture_side = static_cast<std::size_t>(sources.grass.width);
  for (std::size_t row = 0; row < side_size; ++row) {
    const std::size_t source_row = static_cast<std::size_t>(north.source[row]) * texture_side;
    // La couverture de la rangée, case du bloc par case : la somme sur les
    // colonnes se fait au texel.
    std::array<float, kPathBlockSide> row_cover{};
    for (std::int32_t column = 0; column < kPathBlockSide; ++column) {
      for (std::int32_t cell_row = 0; cell_row < kPathBlockSide; ++cell_row) {
        const auto bit = static_cast<unsigned>(cell_row * kPathBlockSide + column);
        if (((path_block >> bit) & 1U) == 0U) continue;
        row_cover[static_cast<std::size_t>(column)] +=
            north.cover[row][static_cast<std::size_t>(cell_row)];
      }
    }
    for (std::size_t column = 0; column < side_size; ++column) {
      float cover = 0.0F;
      for (std::size_t cell = 0; cell < kPathBlockSide; ++cell) {
        cover += east.cover[column][cell] * row_cover[cell];
      }
      const std::size_t source =
          (source_row + static_cast<std::size_t>(east.source[column])) * kChannels;
      const std::uint8_t* grass = &sources.grass.pixels[source];
      const std::uint8_t* dirt = &sources.dirt.pixels[source];
      std::uint8_t* out = &tile[(row * side_size + column) * kChannels];
      // LOIN DE LA LISIÈRE, le trouble ne peut rien changer : l'herbe ou la
      // terre telles quelles, sans tirer le bruit.
      if (cover <= kSurelyGrassCover || cover >= kSurelyDirtCover) {
        std::copy_n(cover <= kSurelyGrassCover ? grass : dirt, kChannels, out);
        out[kChannels - 1] = kOpaque;
        continue;
      }
      float noise = 0.0F;
      for (std::size_t rank = 0; rank < octaves.size(); ++rank) {
        noise += kNoiseOctaves[rank].weight * octaves[rank].At(column, row);
      }
      const float grass_bias = std::clamp(
          (Luminance(grass) - sources.grass_mean_luminance) / kGrassLuminanceSpan, -1.0F, 1.0F);
      const float troubled = cover + kNoiseSpan * (noise - kRounding) + kGrassBias * grass_bias;
      const float dirt_share = Smooth(
          std::clamp((troubled - kBlendLowCover) / (kBlendHighCover - kBlendLowCover), 0.0F, 1.0F));
      Mix(grass, dirt, dirt_share, out);
    }
  }
}

ComposedGround ComposeGround(const GroundComposeSources& sources, std::uint64_t run_seed,
                             const GroundPlan& plan) {
  ComposedGround composed;
  composed.tiles.resize(plan.cubes.size());
  const std::int32_t side = ComposedTileSide(sources);
  const auto side_size = static_cast<std::size_t>(side);
  constexpr std::size_t kPerPage =
      static_cast<std::size_t>(kComposedAtlasColumns) * kComposedAtlasRows;
  std::vector<std::size_t> wanted;
  for (std::size_t rank = 0; rank < plan.cubes.size() && rank < plan.path_blocks.size(); ++rank) {
    if (GroundBlendOf(plan.path_blocks[rank]) == GroundBlend::kComposed) wanted.push_back(rank);
  }
  const std::size_t kept = std::min(wanted.size(), kPerPage * kComposedAtlasPages);
  composed.overflow = wanted.size() - kept;
  // Chaque page aussi haute que ses rangées employées.
  for (std::size_t page = 0; page < kComposedAtlasPages; ++page) {
    const std::size_t first = page * kPerPage;
    if (first >= kept) break;
    const std::size_t tiles = std::min(kPerPage, kept - first);
    const std::size_t rows = (tiles + kComposedAtlasColumns - 1) / kComposedAtlasColumns;
    ro::formats::Image& image = composed.pages[page];
    image.width = kComposedAtlasColumns * side;
    image.height = static_cast<std::int32_t>(rows) * side;
    image.pixels.assign(
        static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * kChannels,
        0);
    // Le sol et le décor se lisent en BMP : la composée se lit pareil.
    image.file = ro::formats::ImageFile::kBmp;
  }
  std::vector<std::uint8_t> tile(side_size * side_size * kChannels);
  const std::int32_t world_cube_x = plan.origin.x_cells / kGroundCellsPerCube;
  const std::int32_t world_cube_y = plan.origin.y_cells / kGroundCellsPerCube;
  for (std::size_t order = 0; order < kept; ++order) {
    const std::size_t rank = wanted[order];
    const auto i = static_cast<std::int32_t>(rank % kGroundWindowCubes);
    const auto j = static_cast<std::int32_t>(rank / kGroundWindowCubes);
    ComposeCubeTile(sources, run_seed, world_cube_x + i, world_cube_y + j, plan.path_blocks[rank],
                    tile);
    const std::size_t page = order / kPerPage;
    const std::size_t slot = order % kPerPage;
    ro::formats::Image& image = composed.pages[page];
    const std::size_t left = (slot % kComposedAtlasColumns) * side_size;
    const std::size_t bottom = (slot / kComposedAtlasColumns) * side_size;
    const auto width = static_cast<std::size_t>(image.width);
    for (std::size_t row = 0; row < side_size; ++row) {
      std::copy_n(tile.begin() + static_cast<std::ptrdiff_t>(row * side_size * kChannels),
                  side_size * kChannels,
                  image.pixels.begin() +
                      static_cast<std::ptrdiff_t>(((bottom + row) * width + left) * kChannels));
    }
    const auto page_width = static_cast<float>(image.width);
    const auto page_height = static_cast<float>(image.height);
    const auto inner_left = static_cast<float>(left + kComposedBorderTexels);
    const auto inner_bottom = static_cast<float>(bottom + kComposedBorderTexels);
    const auto cube = static_cast<float>(sources.cube_texels);
    composed.tiles[rank] = ComposedTileRef{
        .page = static_cast<std::uint8_t>(page),
        .south_west = {inner_left / page_width, inner_bottom / page_height},
        .north_east = {(inner_left + cube) / page_width, (inner_bottom + cube) / page_height}};
  }
  return composed;
}

}  // namespace moonclient
