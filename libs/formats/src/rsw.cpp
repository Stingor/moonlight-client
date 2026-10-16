#include "ro/formats/rsw.h"

#include <string_view>
#include <utility>

#include "ro/core/byte_reader.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::ErrorCode;
using core::Fail;

constexpr std::string_view kRswSignature{"GRSW", 4};

// Longueur des quatre champs de nom de fichier de l'en-tête.
constexpr std::size_t kRswFileNameLength = 40;

// Seuils de version, en 0xMMmm.
constexpr std::int32_t kVersionWaterLevel = 0x0103;
constexpr std::int32_t kVersionAltitudeFile = 0x0104;
constexpr std::int32_t kVersionWaterWaves = 0x0108;
constexpr std::int32_t kVersionTextureCycling = 0x0109;
constexpr std::int32_t kVersionRenderFlag = 0x0202;
constexpr std::int32_t kVersionBuildNumber = 0x0205;
constexpr std::int32_t kVersionWaterMoved = 0x0206;
constexpr std::int32_t kVersionLight = 0x0105;
constexpr std::int32_t kVersionShadowIntensity = 0x0107;

// Plage acceptée par rAthena dans grfio_read_rsw_water_level().
constexpr std::int32_t kServerMinVersion = 0x0104;
constexpr std::int32_t kServerMaxVersion = 0x0205;

constexpr std::int32_t kVersionGround = 0x0106;
constexpr std::int32_t kVersionModelHeader = 0x0103;
constexpr std::int32_t kVersionModelExtraByte = 0x0206;
constexpr std::int32_t kVersionModelExtraInt = 0x0207;
constexpr std::int32_t kVersionExtraBlock = 0x0207;
constexpr std::int32_t kVersionSoundCycle = 0x0200;
// L'octet supplémentaire du modèle n'apparaît qu'à partir de cette révision de
// l'éditeur, pas seulement de cette version de format.
constexpr std::int32_t kBuildModelExtraByte = 186;

// Identifiants des types d'objet dans la liste.
constexpr std::int32_t kObjectModel = 1;
constexpr std::int32_t kObjectLight = 2;
constexpr std::int32_t kObjectSound = 3;
constexpr std::int32_t kObjectEffect = 4;

constexpr std::size_t kModelNameLength = 40;
constexpr std::size_t kModelPathLength = 80;

// Tailles des objets que l'on traverse sans les exploiter. Elles sont écrites
// en somme de leurs champs plutôt qu'en nombre brut : le jour où l'un d'eux
// sera implémenté, la correspondance sera immédiate.
//
//   lumière : nom (80), position (12), couleur (12), portée (4)
//   effet   : nom (80), position (12), numéro (4), vitesse (4), 4 réels (16)
constexpr std::size_t kLightObjectSize = 80 + 12 + 12 + 4;
// Le nom et le fichier d'une source de son tiennent chacun sur quatre-vingts
// octets.
constexpr std::size_t kSoundNameLength = 80;
constexpr std::size_t kSoundWaveLength = 80;
constexpr std::size_t kEffectObjectSize = 80 + 12 + 4 + 4 + 16;
// Le nom d'un émetteur tient sur quatre-vingts octets, comme celui d'un modèle.
constexpr std::size_t kEffectNameLength = 80;

// La seule majeure dont le client lit le quadtree.
constexpr std::uint8_t kQuadTreeMajorVersion = 2;

// Bornes de cohérence : une carte officielle porte quelques milliers d'objets.
constexpr std::int32_t kMaxObjects = 1 << 20;

std::array<float, 3> ReadVector3(ByteReader& reader) {
  std::array<float, 3> value{};
  for (float& component : value) {
    component = reader.F32();
  }
  return value;
}

RswModel ReadModel(ByteReader& reader, std::int32_t version, std::int32_t build_number) {
  RswModel model;
  if (version >= kVersionModelHeader) {
    model.name = reader.FixedString(kModelNameLength);
    model.animation_type = reader.I32();
    model.animation_speed = reader.F32();
    // Une vitesse nulle, négative ou au-delà de cent est une donnée abîmée : le
    // client la ramène à 1 en chargeant le modèle. Zéro en fait partie — il
    // ne fige pas l'objet —, et cent est gardé. Une valeur non numérique, qu'on
    // ne trouve dans aucune carte, retombe aussi à 1 là où le natif la garderait.
    if (!(model.animation_speed > 0.0F) || model.animation_speed > 100.0F) {
      model.animation_speed = 1.0F;
    }
    model.block_type = reader.I32();

    if (version >= kVersionModelExtraByte && build_number >= kBuildModelExtraByte) {
      reader.Skip(1);
    }
    if (version >= kVersionModelExtraInt) {
      reader.Skip(4);
    }
  }

  model.model_name = reader.FixedString(kModelPathLength);
  model.node_name = reader.FixedString(kModelPathLength);
  model.position = ReadVector3(reader);
  model.rotation = ReadVector3(reader);
  model.scale = ReadVector3(reader);
  return model;
}

}  // namespace

core::Result<RswScene> RswScene::Parse(std::span<const std::uint8_t> data) {
  ByteReader reader(data);

  const std::span<const std::uint8_t> signature_bytes = reader.Bytes(kRswSignature.size());
  const std::string_view signature(reinterpret_cast<const char*>(signature_bytes.data()),
                                   signature_bytes.size());
  if (signature != kRswSignature) {
    return Fail(ErrorCode::kBadSignature, "en-tête RSW attendu « GRSW »");
  }

  RswScene scene;
  scene.header_.major_version = reader.U8();
  scene.header_.minor_version = reader.U8();
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "en-tête RSW tronqué");
  }
  const std::int32_t version = scene.header_.version();

  if (version >= kVersionBuildNumber) {
    scene.header_.build_number = reader.I32();
  }
  if (version >= kVersionRenderFlag) {
    scene.header_.render_flag = reader.U8();
  }

  scene.header_.ini_file = reader.FixedString(kRswFileNameLength);
  scene.header_.ground_file = reader.FixedString(kRswFileNameLength);
  if (version >= kVersionAltitudeFile) {
    scene.header_.altitude_file = reader.FixedString(kRswFileNameLength);
  }
  scene.header_.source_file = reader.FixedString(kRswFileNameLength);

  // À partir de 2.6, la section eau ne se trouve plus ici.
  if (version < kVersionWaterMoved && version >= kVersionWaterLevel) {
    scene.water_.present = true;
    scene.water_.level = reader.F32();
    if (version >= kVersionWaterWaves) {
      scene.water_.type = reader.I32();
      scene.water_.wave_height = reader.F32();
      scene.water_.wave_speed = reader.F32();
      scene.water_.wave_pitch = reader.F32();
    }
    if (version >= kVersionTextureCycling) {
      scene.water_.texture_cycling = reader.I32();
    }
  }

  // Tout ce qui précède est obligatoire : le contrôle se fait ICI, avant
  // d'aborder une section facultative, sinon un échec de celle-ci se
  // confondrait avec un échec de celles-là.
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "RSW tronqué avant la fin de la section eau");
  }

  // La lumière suit immédiatement l'eau. Un fichier qui s'arrête avant n'est
  // pas invalide pour autant : l'en-tête et l'eau restent exploitables, et
  // refuser toute la carte pour un champ que l'on sait remplacer serait
  // disproportionné.
  if (version >= kVersionLight) {
    RswLight light;
    light.longitude = reader.I32();
    light.latitude = reader.I32();
    for (float& channel : light.diffuse) {
      channel = reader.F32();
    }
    for (float& channel : light.ambient) {
      channel = reader.F32();
    }
    if (version >= kVersionShadowIntensity) {
      light.shadow_intensity = reader.F32();
    }
    // Tout ou rien : une lumière à moitié lue porte des valeurs arbitraires,
    // qui teinteraient la carte plus sûrement que les valeurs neutres.
    if (reader.ok()) {
      light.present = true;
      scene.light_ = light;
    }
  }

  // Emprise du sol. Son contenu ne sert à rien, sa PLACE si : la sauter
  // décalerait toute la liste d'objets qui suit.
  if (version >= kVersionGround) {
    RswGround ground;
    ground.top = reader.I32();
    ground.bottom = reader.I32();
    ground.left = reader.I32();
    ground.right = reader.I32();
    if (reader.ok()) {
      ground.present = true;
      scene.ground_ = ground;
    }
  }

  // Bloc d'entiers supplémentaire apparu en 2.7, de contenu inconnu.
  if (version >= kVersionExtraBlock) {
    const std::int32_t extra = reader.I32();
    if (reader.ok() && extra > 0 && extra <= kMaxObjects) {
      reader.Skip(static_cast<std::size_t>(extra) * 4);
    }
  }

  const std::int32_t object_count = reader.I32();
  if (!reader.ok() || object_count < 0 || object_count > kMaxObjects) {
    // Pas de liste d'objets exploitable : la carte reste utilisable pour son
    // terrain, son eau et sa lumière.
    return scene;
  }

  for (std::int32_t index = 0; index < object_count; ++index) {
    const std::int32_t type = reader.I32();
    if (!reader.ok()) {
      break;
    }

    switch (type) {
      case kObjectModel: {
        RswModel model = ReadModel(reader, version, scene.header_.build_number);
        if (!reader.ok()) {
          break;
        }
        scene.models_.push_back(std::move(model));
        break;
      }
      case kObjectLight:
        reader.Skip(kLightObjectSize);
        ++scene.object_counts_.lights;
        break;
      case kObjectSound: {
        RswSound sound;
        sound.name = reader.FixedString(kSoundNameLength);
        sound.wave_name = reader.FixedString(kSoundWaveLength);
        sound.position = ReadVector3(reader);
        sound.volume = reader.F32();
        sound.width = reader.I32();
        sound.height = reader.I32();
        sound.range = reader.F32();
        if (version >= kVersionSoundCycle) {
          sound.cycle_seconds = reader.F32();
        }
        if (!reader.ok()) {
          break;
        }
        scene.sounds_.push_back(std::move(sound));
        ++scene.object_counts_.sounds;
        break;
      }
      case kObjectEffect: {
        RswEffect effect;
        effect.name = reader.FixedString(kEffectNameLength);
        effect.position = ReadVector3(reader);
        effect.number = reader.I32();
        effect.emit_speed = reader.F32();
        for (float& value : effect.param) {
          value = reader.F32();
        }
        if (!reader.ok()) {
          break;
        }
        scene.effects_.push_back(std::move(effect));
        ++scene.object_counts_.effects;
        break;
      }
      default:
        // Type inconnu : sa taille l'est aussi, donc il n'y a aucun moyen de se
        // replacer. Continuer lirait n'importe quoi comme des modèles ; on
        // s'arrête et on le signale.
        ++scene.object_counts_.unknown;
        reader.Invalidate();
        break;
    }

    if (!reader.ok()) {
      break;
    }
  }

  // LE QUADTREE SUIT LA LISTE D'OBJETS, à la majeure 2 dès que la mineure n'est
  // pas nulle — c'est la condition du client, et une autre majeure ne le lit pas.
  if (reader.ok() && scene.header_.major_version == kQuadTreeMajorVersion &&
      scene.header_.minor_version != 0) {
    std::vector<RswQuadTreeNode> nodes(kRswQuadTreeNodeCount);
    for (RswQuadTreeNode& node : nodes) {
      node.max = ReadVector3(reader);
      node.min = ReadVector3(reader);
      node.half_size = ReadVector3(reader);
      node.center = ReadVector3(reader);
    }
    if (reader.ok()) {
      scene.quad_tree_ = std::move(nodes);
    }
  }
  return scene;
}

std::optional<float> RswScene::ServerWaterLevel() const noexcept {
  const std::int32_t version = header_.version();
  if (version < kServerMinVersion || version > kServerMaxVersion) {
    return std::nullopt;
  }
  if (!water_.present) {
    return std::nullopt;
  }
  return water_.level;
}

void RswScene::MoveEffect(std::size_t effect_index, const std::array<float, 3>& position) noexcept {
  if (effect_index < effects_.size()) {
    effects_[effect_index].position = position;
  }
}

void RswScene::PlaceModel(std::size_t model_index, const std::array<float, 3>& position,
                          const std::array<float, 3>& rotation,
                          const std::array<float, 3>& scale) noexcept {
  if (model_index < models_.size()) {
    models_[model_index].position = position;
    models_[model_index].rotation = rotation;
    models_[model_index].scale = scale;
  }
}

}  // namespace ro::formats
