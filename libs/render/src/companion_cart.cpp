#include "ro/render/companion_cart.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>

#include "ro/render/actor_sprite.h"

namespace ro::render {
namespace {

// LES SUPER NOVICES, dans la table des métiers du serveur (`e_job`) : le
// premier, le bébé, et leurs corps d'extension. Leur charrette ne dépend pas
// du type annoncé.
constexpr std::array<std::uint32_t, 10> kSuperNoviceJobs = {23,   4045, 4128, 4172, 4190,
                                                            4191, 4192, 4193, 4307, 4314};

// LES BÉBÉS, par plages de la même table. Un bébé prend la charrette réduite.
struct JobRange {
  std::uint32_t first;
  std::uint32_t last;
};
constexpr std::array<JobRange, 13> kBabyJobs = {
    JobRange{4023, 4029}, JobRange{4030, 4044}, JobRange{4045, 4045}, JobRange{4096, 4112},
    JobRange{4158, 4182}, JobRange{4191, 4191}, JobRange{4193, 4196}, JobRange{4205, 4210},
    JobRange{4220, 4220}, JobRange{4222, 4238}, JobRange{4241, 4242}, JobRange{4244, 4244},
    JobRange{4247, 4248}};

// LA TABLE, rangée par type : la charrette de l'adulte, puis celle du bébé. Le
// rang zéro est celle du super novice.
struct CartRecipe {
  CartArt adult;
  CartArt baby;
};
constexpr std::array<CartRecipe, kMostCartType + 1> kRecipes = {
    CartRecipe{
        {"data\\sprite\\이팩트\\슈노손수레.spr", "data\\sprite\\이팩트\\슈노손수레.act"},
        {"data\\sprite\\이팩트\\슈노손수레.spr", "data\\sprite\\이팩트\\베이비슈노손수레.act"}},
    CartRecipe{{"data\\sprite\\이팩트\\손수레.spr", "data\\sprite\\이팩트\\손수레.act"},
               {"data\\sprite\\이팩트\\손수레.spr", "data\\sprite\\이팩트\\베이비손수레.act"}},
    CartRecipe{{"data\\sprite\\이팩트\\손수레1.spr", "data\\sprite\\이팩트\\손수레1.act"},
               {"data\\sprite\\이팩트\\손수레1.spr", "data\\sprite\\이팩트\\베이비손수레1.act"}},
    CartRecipe{{"data\\sprite\\이팩트\\손수레2.spr", "data\\sprite\\이팩트\\손수레2.act"},
               {"data\\sprite\\이팩트\\손수레2.spr", "data\\sprite\\이팩트\\베이비손수레2.act"}},
    CartRecipe{{"data\\sprite\\이팩트\\손수레3.spr", "data\\sprite\\이팩트\\손수레3.act"},
               {"data\\sprite\\이팩트\\손수레3.spr", "data\\sprite\\이팩트\\베이비손수레3.act"}},
    CartRecipe{{"data\\sprite\\이팩트\\손수레4.spr", "data\\sprite\\이팩트\\손수레4.act"},
               {"data\\sprite\\이팩트\\손수레4.spr", "data\\sprite\\이팩트\\베이비손수레4.act"}},
    CartRecipe{
        {"data\\sprite\\이팩트\\손수레5.spr", "data\\sprite\\이팩트\\손수레5.act"},
        {"data\\sprite\\이팩트\\손수레5베이비.spr", "data\\sprite\\이팩트\\손수레5베이비.act"}},
    CartRecipe{
        {"data\\sprite\\이팩트\\손수레6.spr", "data\\sprite\\이팩트\\손수레6.act"},
        {"data\\sprite\\이팩트\\손수레6베이비.spr", "data\\sprite\\이팩트\\손수레6베이비.act"}},
    CartRecipe{
        {"data\\sprite\\이팩트\\손수레7.spr", "data\\sprite\\이팩트\\손수레7.act"},
        {"data\\sprite\\이팩트\\손수레7베이비.spr", "data\\sprite\\이팩트\\손수레7베이비.act"}},
    CartRecipe{
        {"data\\sprite\\이팩트\\손수레8.spr", "data\\sprite\\이팩트\\손수레8.act"},
        {"data\\sprite\\이팩트\\손수레8베이비.spr", "data\\sprite\\이팩트\\손수레8베이비.act"}},
    CartRecipe{{"data\\sprite\\이팩트\\선물상자카트.spr", "data\\sprite\\이팩트\\선물상자카트.act"},
               {"data\\sprite\\이팩트\\선물상자카트_미니.spr",
                "data\\sprite\\이팩트\\선물상자카트_미니.act"}},
    CartRecipe{{"data\\sprite\\이팩트\\포링싣은카트.spr", "data\\sprite\\이팩트\\포링싣은카트.act"},
               {"data\\sprite\\이팩트\\포링싣은카트_미니.spr",
                "data\\sprite\\이팩트\\포링싣은카트_미니.act"}},
    CartRecipe{
        {"data\\sprite\\이팩트\\포링카트.spr", "data\\sprite\\이팩트\\포링카트.act"},
        {"data\\sprite\\이팩트\\포링카트_미니.spr", "data\\sprite\\이팩트\\포링카트_미니.act"}},
    CartRecipe{
        {"data\\sprite\\이팩트\\마도카트.spr", "data\\sprite\\이팩트\\마도카트.act"},
        {"data\\sprite\\이팩트\\마도카트_미니.spr", "data\\sprite\\이팩트\\마도카트_미니.act"}},
};

constexpr std::uint32_t kOptionHide = 0x2;
constexpr std::uint32_t kOptionCloak = 0x4;
constexpr std::uint32_t kOptionInvisible = 0x40;

constexpr float kDegreesPerHalfTurn = 180.0F;
constexpr std::size_t kChannelsPerPixel = 4;
constexpr std::size_t kAlphaChannel = 3;
constexpr float kChannelMax = 255.0F;

// La version de .act (majeure × 10 + mineure) à partir de laquelle le jeu range
// chaque calque par son COIN haut-gauche et non plus par son centre.
constexpr std::int32_t kCartButtonCornerVersion = 20;
// Au-delà, un coin ne tient plus dans l'entier du jeu, ou sa différence avec un
// autre déborderait : le calque sort du bouton.
constexpr float kMostCornerMagnitude = 1073741824.0F;

// Le coin d'un calque le long d'un axe : son écart, moins la MOITIÉ ARRONDIE
// VERS LE HAUT de sa taille mise à l'échelle, le tout ramené à l'échelle un et
// tronqué vers zéro. Vide pour une échelle nulle, ou un coin hors de portée.
std::optional<std::int32_t> CornerAlong(float offset, std::int32_t size, float scale) noexcept {
  if (scale == 0.0F) {
    return std::nullopt;
  }
  const std::int32_t half_up = size % 2 + size / 2;
  const float corner = (offset - static_cast<float>(half_up) * scale) / scale;
  if (!(std::fabs(corner) < kMostCornerMagnitude)) {
    return std::nullopt;
  }
  return static_cast<std::int32_t>(corner);
}

bool IsSuperNovice(std::uint32_t job_id) noexcept {
  return std::ranges::find(kSuperNoviceJobs, job_id) != kSuperNoviceJobs.end();
}

bool IsBaby(std::uint32_t job_id) noexcept {
  return std::ranges::any_of(kBabyJobs, [job_id](const JobRange& range) {
    return job_id >= range.first && job_id <= range.last;
  });
}

float DistanceXz(const FalconPoint& from, const FalconPoint& to) noexcept {
  return std::hypot(to.x - from.x, to.z - from.z);
}

// Pose `source` dans `into`, son coin haut-gauche en (left, top), par
// recouvrement alpha, sans teinte. Ce qui dépasse est coupé.
//
// ⚠ LE MIROIR RETOURNE LA PARTIE VISIBLE, et non l'image entière : la colonne
// posée le plus à gauche montre la dernière colonne que la coupe laisse.
void Blit(formats::Image& into, const formats::Image& source, std::int32_t left, std::int32_t top,
          bool mirrored) {
  const std::int32_t first_column = std::max(0, -left);
  const std::int32_t end_column = std::min(source.width, into.width - left);
  for (std::int32_t row = 0; row < source.height; ++row) {
    const std::int32_t y = top + row;
    if (y < 0 || y >= into.height) {
      continue;
    }
    for (std::int32_t column = first_column; column < end_column; ++column) {
      const std::int32_t x = left + column;
      const std::int32_t read_column = mirrored ? first_column + end_column - 1 - column : column;
      const std::size_t from =
          (static_cast<std::size_t>(row) * static_cast<std::size_t>(source.width) +
           static_cast<std::size_t>(read_column)) *
          kChannelsPerPixel;
      const std::size_t to = (static_cast<std::size_t>(y) * static_cast<std::size_t>(into.width) +
                              static_cast<std::size_t>(x)) *
                             kChannelsPerPixel;
      const float alpha = static_cast<float>(source.pixels[from + kAlphaChannel]) / kChannelMax;
      if (alpha <= 0.0F) {
        continue;
      }
      for (std::size_t channel = 0; channel < kAlphaChannel; ++channel) {
        const float colour = static_cast<float>(source.pixels[from + channel]);
        const float below = static_cast<float>(into.pixels[to + channel]);
        into.pixels[to + channel] =
            static_cast<std::uint8_t>(std::lround(colour * alpha + below * (1.0F - alpha)));
      }
      const float below_alpha = static_cast<float>(into.pixels[to + kAlphaChannel]) / kChannelMax;
      into.pixels[to + kAlphaChannel] = static_cast<std::uint8_t>(
          std::lround((alpha + below_alpha * (1.0F - alpha)) * kChannelMax));
    }
  }
}

}  // namespace

std::optional<CartArt> CartArtFor(std::int32_t cart_type, std::uint32_t job_id,
                                  std::uint32_t option) noexcept {
  if (cart_type <= 0 || cart_type > kMostCartType) {
    return std::nullopt;
  }
  const std::uint32_t shown = CompanionShownJob(job_id, option);
  const std::size_t rank = IsSuperNovice(shown) ? 0 : static_cast<std::size_t>(cart_type);
  const CartRecipe& recipe = kRecipes[rank];
  return IsBaby(shown) ? recipe.baby : recipe.adult;
}

std::optional<CartArt> CartListArtFor(std::int32_t cart_type) noexcept {
  if (cart_type <= 0 || cart_type > kMostCartType) {
    return std::nullopt;
  }
  return kRecipes[static_cast<std::size_t>(cart_type)].adult;
}

bool CarriesCart(std::int32_t cart_type, std::uint32_t option) noexcept {
  return cart_type > 0 && (option & (kOptionHide | kOptionCloak | kOptionInvisible)) == 0;
}

Cart BornCart(const FalconMaster& master) noexcept {
  Cart cart;
  // Le cap du maître se lit en DEGRÉS ENTIERS : le jeu le tronque pour sa table
  // de sinus, comme à la naissance du faucon.
  const float heading_radians =
      std::trunc(master.heading_degrees) * std::numbers::pi_v<float> / kDegreesPerHalfTurn;
  cart.position.x = master.feet.x - kCartBirthBehindUnits * std::sin(heading_radians);
  cart.position.y = master.feet.y;
  cart.position.z = master.feet.z + kCartBirthBehindUnits * std::cos(heading_radians);
  cart.previous_position = cart.position;
  return cart;
}

float CartFrameDelay(const formats::ActAnimation& animation) noexcept {
  const float declared =
      animation.actions().empty() ? 0.0F : animation.actions().front().speed_units;
  return std::max(declared, kCartMinFrameDelay);
}

void StepCart(Cart& cart, const FalconMaster& master, const FalconGround& ground,
              const CartFrameCount& frame_count, float frame_delay, float camera_yaw_degrees) {
  cart.previous_position = cart.position;
  const float distance = DistanceXz(cart.position, master.feet);
  // L'ACTION SE CHOISIT AVANT QUE LA CHARRETTE NE SE RETOURNE : elle montre le
  // cap du tic précédent.
  cart.shown_heading_degrees = cart.heading_degrees;
  if (distance > kCartLeashUnits) {
    const float before_x = cart.position.x;
    const float before_z = cart.position.z;
    const float away_x = (cart.position.x - master.feet.x) / distance;
    const float away_z = (cart.position.z - master.feet.z) / distance;
    cart.position.x = away_x * kCartLeashUnits + master.feet.x;
    cart.position.z = away_z * kCartLeashUnits + master.feet.z;
    cart.travelled_units += std::hypot(cart.position.x - before_x, cart.position.z - before_z);
    const std::size_t count = frame_count(CartAction(cart, camera_yaw_degrees));
    if (count > 0 && frame_delay > 0.0F) {
      // ⚠ EN FLOTTANTS SIMPLES, et tronqué : le jeu multiplie en simple
      // précision puis coupe vers zéro.
      const float step = cart.travelled_units * kCartFramesPerUnit * kCartFrameStride / frame_delay;
      cart.frame = static_cast<std::size_t>(static_cast<std::int64_t>(step)) % count;
    }
  }
  cart.heading_degrees =
      FalconHeadingToward(cart.position, master.feet.x, master.feet.z, cart.heading_degrees);
  cart.position.y = ground(cart.position.x, cart.position.z);
}

std::size_t CartAction(const Cart& cart, float camera_yaw_degrees) noexcept {
  return FalconScreenDirection(cart.shown_heading_degrees, camera_yaw_degrees);
}

FalconPoint CartDrawnPosition(const Cart& cart, float weight) noexcept {
  return BlendFalconPoints(cart.previous_position, cart.position, weight);
}

std::optional<ButtonLayerCorner> CartButtonLayerCorner(const formats::ActLayer& layer,
                                                       std::int32_t act_version,
                                                       std::int32_t image_width,
                                                       std::int32_t image_height) noexcept {
  if (act_version < kCartButtonCornerVersion) {
    return ButtonLayerCorner{static_cast<std::int32_t>(layer.offset_x),
                             static_cast<std::int32_t>(layer.offset_y)};
  }
  const std::int32_t width = layer.declared_width.value_or(image_width);
  const std::int32_t height = layer.declared_height.value_or(image_height);
  const std::optional<std::int32_t> x = CornerAlong(layer.offset_x, width, layer.scale_x);
  const std::optional<std::int32_t> y = CornerAlong(layer.offset_y, height, layer.scale_y);
  if (!x.has_value() || !y.has_value()) {
    return std::nullopt;
  }
  return ButtonLayerCorner{*x, *y};
}

formats::Image CartButtonImage(const formats::SprSprite& sprite,
                               const formats::ActAnimation& animation) {
  formats::Image image;
  image.width = kCartButtonSidePixels;
  image.height = kCartButtonSidePixels;
  image.pixels.assign(static_cast<std::size_t>(image.width) *
                          static_cast<std::size_t>(image.height) * kChannelsPerPixel,
                      0);
  if (animation.actions().empty() || animation.actions().front().frames.empty()) {
    return image;
  }
  const formats::ActFrame& frame = animation.actions().front().frames.front();
  // Le coin et le demi-format du PREMIER calque. ⚠ ILS RESTENT NULS quand ce
  // calque est vide : les suivants se posent alors depuis le centre du carré.
  ButtonLayerCorner first_corner;
  std::int32_t first_half_width = 0;
  std::int32_t first_half_height = 0;
  // Un premier calque sans coin calculable (échelle nulle) se pose quand même
  // au centre, mais emporte tous les suivants hors du bouton.
  bool first_lost = false;
  for (std::size_t rank = 0; rank < frame.layers.size(); ++rank) {
    const formats::ActLayer& layer = frame.layers[rank];
    if (layer.frame_index < 0) {
      continue;
    }
    const formats::Image source =
        ResolveImage(SpriteImageKey{&sprite, nullptr, layer.section, layer.frame_index});
    if (source.empty()) {
      continue;
    }
    const std::optional<ButtonLayerCorner> corner =
        CartButtonLayerCorner(layer, animation.version(), source.width, source.height);
    if (rank == 0) {
      first_corner = corner.value_or(ButtonLayerCorner{});
      first_lost = !corner.has_value();
      first_half_width = source.width / 2;
      first_half_height = source.height / 2;
    } else if (first_lost || !corner.has_value()) {
      continue;
    }
    const ButtonLayerCorner placed = corner.value_or(first_corner);
    // ⚠ NI TEINTE, NI ÉCHELLE, NI ROTATION : le bouton pose l'image telle que
    // le .spr la donne, à sa taille ; seul le miroir la retourne.
    Blit(image, source, placed.x - first_corner.x + kCartButtonSidePixels / 2 - first_half_width,
         placed.y - first_corner.y + kCartButtonSidePixels / 2 - first_half_height, layer.mirrored);
  }
  return image;
}

}  // namespace ro::render
