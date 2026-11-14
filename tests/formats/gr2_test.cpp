#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gr2_arith.h"
#include "ro/formats/gr2_codecs.h"
#include "ro/formats/gr2_model.h"

#include "support/gr2_builder.h"

namespace {

using Catch::Approx;

// Des octets qui se répètent par endroits et pas ailleurs : de quoi exercer
// les copies, les littéraux, et la division des fréquences d'un modèle qui a
// beaucoup vu.
std::vector<std::uint8_t> MixedBytes(std::size_t count) {
  std::vector<std::uint8_t> bytes;
  std::uint32_t state = 12345;
  const std::string phrase = "le squelette tient debout, ";
  while (bytes.size() < count) {
    state = state * 1103515245U + 12345U;
    if ((state >> 16) % 3 == 0) {
      bytes.insert(bytes.end(), phrase.begin(), phrase.end());
    } else {
      for (int index = 0; index < 40; ++index) {
        state = state * 1103515245U + 12345U;
        bytes.push_back(static_cast<std::uint8_t>(state >> 24));
      }
    }
  }
  bytes.resize(count);
  return bytes;
}

TEST_CASE("Granny : une section compressée se relit à l'identique", "[formats][gr2]") {
  const std::vector<std::uint8_t> data = MixedBytes(40000);
  const std::array<std::uint32_t, 2> stops = {7000, 29000};
  const std::vector<std::uint8_t> packed = ro::test_support::CompressGr2Section(data, stops);
  CHECK(packed.size() < data.size());
  const auto unpacked = ro::formats::DecompressGr2Section(packed, stops, 40000);
  REQUIRE(unpacked.has_value());
  CHECK(*unpacked == data);
}

TEST_CASE("Granny : une section sans paramètres est refusée", "[formats][gr2]") {
  const std::vector<std::uint8_t> packed(10, 0);
  CHECK_FALSE(ro::formats::DecompressGr2Section(packed, {4, 4}, 4).has_value());
}

TEST_CASE("Granny : le modèle adaptatif apprend puis retrouve ses symboles", "[formats][gr2]") {
  ro::test_support::Gr2RangeEncoder encoder;
  ro::formats::Gr2AdaptiveModel writing(10);
  const std::vector<std::uint16_t> symbols = {3, 3, 9, 3, 0, 9, 9, 7, 3, 1, 2, 4, 5, 6, 8, 3};
  for (const std::uint16_t symbol : symbols) {
    encoder.Symbol(writing, symbol, 10);
  }
  const std::vector<std::uint8_t> stream = encoder.Finish();
  ro::formats::Gr2RangeDecoder decoder(stream, 0);
  ro::formats::Gr2AdaptiveModel reading(10);
  for (const std::uint16_t symbol : symbols) {
    const auto decoded = reading.Decode(decoder);
    std::uint16_t value = decoded.value;
    if (decoded.escaped) {
      value = static_cast<std::uint16_t>(decoder.Uniform(10));
      reading.Learn(value);
    }
    CHECK(value == symbol);
  }
}

ro::test_support::Gr2Spec SampleSpec() {
  ro::test_support::Gr2Spec spec;
  spec.model_name = "Dummy01";
  spec.bones = {{"Dummy01", -1, {0.0F, 0.0F, 0.0F}}, {"Bip01", 0, {0.0F, 0.0F, 12.0F}}};
  ro::test_support::Gr2TextureSpec texture;
  texture.file_name = "coffre.tif";
  texture.width = 2;
  texture.height = 1;
  texture.alpha = false;
  texture.rgba = {10, 20, 30, 0, 40, 50, 60, 0};
  spec.textures.push_back(texture);
  ro::test_support::Gr2MeshSpec body;
  body.name = "corps";
  body.bone_names = {"Dummy01", "Bip01"};
  body.diffuse_texture = "coffre.tif";
  ro::test_support::Gr2VertexSpec vertex;
  vertex.position = {1.0F, 2.0F, 3.0F};
  vertex.uv = {0.25F, 0.75F};
  vertex.bone_indices = {1, 0, 0, 0};
  vertex.bone_weights = {200, 55, 0, 0};
  body.vertices = {vertex, vertex, vertex};
  body.indices = {0, 2, 1};
  spec.meshes.push_back(body);
  ro::test_support::Gr2MeshSpec bare;
  bare.name = "bras";
  bare.bone_names = {"Bip01"};
  bare.vertices = {vertex};
  spec.meshes.push_back(bare);
  ro::test_support::Gr2AnimationSpec animation;
  animation.duration_seconds = 2.5F;
  ro::test_support::Gr2TrackSpec track;
  track.name = "Bip01";
  track.position = {0, {0.0F}, {1.0F, 2.0F, 3.0F}};
  track.orientation = {2, {0.0F, 1.0F}, {0, 0, 0, 1, 0, 0, 1, 0}};
  animation.tracks.push_back(track);
  spec.animations.push_back(animation);
  return spec;
}

void CheckSample(const ro::formats::GrannyScene& scene) {
  CHECK(scene.model_name == "Dummy01");
  REQUIRE(scene.bones.size() == 2);
  CHECK(scene.bones[1].name == "Bip01");
  CHECK(scene.bones[1].parent_index == 0);
  CHECK(scene.bones[1].local.position[2] == Approx(12.0F));
  CHECK(scene.bones[1].local.orientation[3] == Approx(1.0F));
  CHECK(scene.bones[1].inverse_world[14] == Approx(-12.0F));

  REQUIRE(scene.textures.size() == 1);
  const ro::formats::GrannyTextureImage& texture = scene.textures[0];
  CHECK(texture.file_name == "coffre.tif");
  CHECK_FALSE(texture.has_alpha);
  CHECK(texture.rgba == std::vector<std::uint8_t>{10, 20, 30, 255, 40, 50, 60, 255});

  REQUIRE(scene.meshes.size() == 2);
  const ro::formats::GrannyMesh& body = scene.meshes[0];
  CHECK(body.name == "corps");
  CHECK(body.texture == 0);
  CHECK_FALSE(body.rigid);
  CHECK(body.bone_names == std::vector<std::string>{"Dummy01", "Bip01"});
  CHECK(body.indices == std::vector<std::uint16_t>{0, 2, 1});
  REQUIRE(body.vertices.size() == 3);
  CHECK(body.vertices[1].position[1] == Approx(2.0F));
  CHECK(body.vertices[1].uv[1] == Approx(0.75F));
  CHECK(body.vertices[1].bone_indices[0] == 1);
  CHECK(body.vertices[1].bone_weights[0] == 200);
  CHECK(body.vertices[1].bone_weights[1] == 55);
  // Un maillage qui ne cite qu'un os est rigide ; sans matériau, il n'a pas de
  // texture.
  CHECK(scene.meshes[1].rigid);
  CHECK(scene.meshes[1].texture == -1);

  REQUIRE(scene.animations.size() == 1);
  const ro::formats::GrannyAnimation& animation = scene.animations[0];
  CHECK(animation.duration_seconds == Approx(2.5F));
  REQUIRE(animation.track_groups.size() == 1);
  CHECK(animation.track_groups[0].name == "Dummy01");
  REQUIRE(animation.track_groups[0].tracks.size() == 1);
  const ro::formats::GrannyTrack& track = animation.track_groups[0].tracks[0];
  CHECK(track.name == "Bip01");
  CHECK(track.position.dimension == 3);
  CHECK(track.orientation.degree == 2);
  CHECK(track.orientation.dimension == 4);
  CHECK(track.orientation.knots.size() == 2);
  CHECK(track.scale_shear.knots.empty());
}

TEST_CASE("Granny : un modèle se lit par les noms de ses champs", "[formats][gr2]") {
  const auto scene = ro::formats::ReadGrannyScene(ro::test_support::BuildGr2(SampleSpec()));
  REQUIRE(scene.has_value());
  CheckSample(*scene);
}

TEST_CASE("Granny : un modèle compressé se lit de même", "[formats][gr2]") {
  ro::test_support::Gr2Spec spec = SampleSpec();
  spec.compress = true;
  const auto scene = ro::formats::ReadGrannyScene(ro::test_support::BuildGr2(spec));
  REQUIRE(scene.has_value());
  CheckSample(*scene);
}

TEST_CASE("Granny : un fichier abîmé est refusé", "[formats][gr2]") {
  std::vector<std::uint8_t> file = ro::test_support::BuildGr2(SampleSpec());
  std::vector<std::uint8_t> unsigned_file = file;
  unsigned_file[0] ^= 0xFFU;
  CHECK_FALSE(ro::formats::ReadGrannyScene(unsigned_file).has_value());
  CHECK_FALSE(
      ro::formats::ReadGrannyScene(std::vector<std::uint8_t>(file.begin(), file.begin() + 40))
          .has_value());
  file.resize(file.size() / 2);
  CHECK_FALSE(ro::formats::ReadGrannyScene(file).has_value());
}

TEST_CASE("Granny : une texture brute se lit sous son pas de ligne", "[formats][gr2]") {
  const auto read = [](std::int32_t width, std::int32_t height, std::int32_t stride,
                       std::vector<std::uint8_t> rgba) {
    ro::test_support::Gr2Spec spec = SampleSpec();
    spec.textures[0].width = width;
    spec.textures[0].height = height;
    spec.textures[0].alpha = true;
    spec.textures[0].rgba = std::move(rgba);
    spec.textures[0].raw_stride = stride;
    const auto scene = ro::formats::ReadGrannyScene(ro::test_support::BuildGr2(spec));
    REQUIRE(scene.has_value());
    REQUIRE(scene->textures.size() == 1);
    return scene->textures[0].rgba;
  };
  // Deux pixels sur deux rangées, sans marge : le pas est la rangée même.
  const std::vector<std::uint8_t> packed = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  CHECK(read(2, 2, 8, packed) == packed);
  // Une marge de quatre octets en fin de rangée se saute.
  const std::vector<std::uint8_t> padded = {1, 2, 3, 4,  5,  6,  7,  8,  0,  0,
                                            0, 0, 9, 10, 11, 12, 13, 14, 15, 16};
  CHECK(read(2, 2, 12, padded) == packed);
  // 🔴 UN PAS PLUS COURT QU'UNE RANGÉE se refuse : la dernière se lisait
  // au-delà des pixels, alors que `pas × hauteur` tenait dans le tampon.
  CHECK(read(4, 2, 1, packed).empty());
}

TEST_CASE("Granny : une petite texture est rangée brute", "[formats][gr2]") {
  const std::vector<std::uint8_t> rgb = {1, 2, 3, 4, 5, 6};
  const auto opaque = ro::formats::DecodeGr2WaveletTexture(rgb, 2, 1, false);
  REQUIRE(opaque.has_value());
  CHECK(*opaque == std::vector<std::uint8_t>{1, 2, 3, 255, 4, 5, 6, 255});
  CHECK_FALSE(ro::formats::DecodeGr2WaveletTexture(rgb, 2, 1, true).has_value());
}

TEST_CASE("Granny : une texture unie traverse les quatre niveaux", "[formats][gr2]") {
  // Chaque niveau du filtre long divise la bande grossière par √2 dans chaque
  // sens : quatre niveaux la divisent par seize.
  const std::array<std::int16_t, 4> coarse = {16 * 100, 16 * 40, 0, 16 * 200};
  const std::vector<std::uint8_t> stream = ro::test_support::EncodeFlatGr2Texture(256, coarse);
  const auto rgba = ro::formats::DecodeGr2WaveletTexture(stream, 256, 256, true);
  REQUIRE(rgba.has_value());
  REQUIRE(rgba->size() == 256U * 256U * 4U);
  // Le vert retire le quart des deux chrominances ; le rouge ajoute la sienne.
  CHECK(std::abs(static_cast<int>((*rgba)[0]) - 130) <= 1);
  CHECK(std::abs(static_cast<int>((*rgba)[1]) - 90) <= 1);
  CHECK(std::abs(static_cast<int>((*rgba)[2]) - 90) <= 1);
  CHECK(std::abs(static_cast<int>((*rgba)[3]) - 200) <= 1);
  bool uniform = true;
  for (std::size_t pixel = 0; pixel < rgba->size(); pixel += 4) {
    for (std::size_t lane = 0; lane < 4; ++lane) {
      uniform = uniform && (*rgba)[pixel + lane] == (*rgba)[lane];
    }
  }
  CHECK(uniform);
}

TEST_CASE("Granny : une texture démesurée est refusée avant toute allocation", "[formats][gr2]") {
  // 🔴 QUELQUES OCTETS QUI ANNONCENT 32767×32767 demandaient une dizaine de
  // gigaoctets avant le premier contrôle du flux.
  const std::vector<std::uint8_t> tiny = {0, 0, 0, 0, 1, 2, 3, 4};
  CHECK_FALSE(ro::formats::DecodeGr2WaveletTexture(tiny, 32767, 32767, true).has_value());
  CHECK_FALSE(ro::formats::DecodeGr2WaveletTexture(tiny, 4097, 16, false).has_value());
}

TEST_CASE("Granny : une texture tronquée est refusée", "[formats][gr2]") {
  const std::array<std::int16_t, 3> coarse = {1600, 0, 0};
  std::vector<std::uint8_t> stream = ro::test_support::EncodeFlatGr2Texture(32, coarse);
  stream.resize(stream.size() / 2);
  CHECK_FALSE(ro::formats::DecodeGr2WaveletTexture(stream, 32, 32, false).has_value());
}

}  // namespace

TEST_CASE("Granny : un flux de zéros ne fige pas le décodage", "[formats][gr2]") {
  // Des longueurs annoncées nulles passent le contrôle de taille, et le décodeur
  // lit alors des plages vides : le décodage doit tout de même se terminer.
  constexpr std::size_t kScratchAndLengths = 12;
  const std::vector<std::uint8_t> zeros(kScratchAndLengths, 0);
  static_cast<void>(ro::formats::DecodeGr2WaveletTexture(zeros, 32, 32, false));
  SUCCEED("le décodage s'est terminé");
}
