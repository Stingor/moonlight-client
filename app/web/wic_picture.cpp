// LE DÉCODAGE D'UNE IMAGE PAR WINDOWS IMAGING COMPONENT.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "web/webp_picture.h"

// clang-format off
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
// clang-format on

namespace moonclient {
namespace {

using Microsoft::WRL::ComPtr;

constexpr std::size_t kChannels = 4;

// 🔴 LA GARDE DU DÉCODAGE, celle des images du web : un en-tête de quelques
// octets peut annoncer un gigaoctet de pixels.
constexpr std::size_t kMostDecodedBytes = ro::formats::kWebPictureMostDecodedBytes;
constexpr auto kMostSide = static_cast<UINT>(ro::formats::kWebPictureMostSide);

[[nodiscard]] std::unexpected<ro::core::Error> Refused(const char* why, HRESULT result) {
  std::string detail = std::string("WIC : ") + why;
  if (FAILED(result)) {
    constexpr int kHexDigits = 8;
    std::string code(kHexDigits + 1, '\0');
    std::snprintf(code.data(), code.size(), "%08lX", static_cast<unsigned long>(result));
    code.resize(kHexDigits);
    detail += " (0x" + code + ")";
  }
  return ro::core::Fail(ro::core::ErrorCode::kUnsupportedVersion, std::move(detail));
}

// COM, pour le fil qui appelle : WIC en est. Un fil qui l'a déjà ouvert dans
// un autre mode le garde, et on ne le referme alors pas.
class ComScope {
 public:
  ComScope() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
  ComScope(const ComScope&) = delete;
  ComScope& operator=(const ComScope&) = delete;
  ~ComScope() {
    if (SUCCEEDED(result_)) {
      CoUninitialize();
    }
  }
  [[nodiscard]] bool usable() const noexcept {
    return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
  }

 private:
  HRESULT result_;
};

}  // namespace

ro::core::Result<ro::formats::DecodedWebPicture> DecodeWebpPicture(
    std::span<const std::uint8_t> bytes) {
  // ⚠ LE WEBP SEUL : le système sait lire bien d'autres formats, et chacun est
  // un décodeur de plus exposé à des octets venus de n'importe où.
  if (ro::formats::SniffWebPicture(bytes) != ro::formats::WebPictureFormat::kWebp) {
    return ro::core::Fail(ro::core::ErrorCode::kBadSignature, "WIC : seul le WebP passe ici");
  }
  // ⚠ ENTRE PARENTHÈSES : <windows.h> fait de `max` une macro.
  if (bytes.size() > (std::numeric_limits<DWORD>::max)()) {
    return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument, "WIC : image trop lourde");
  }

  const ComScope com;
  if (!com.usable()) {
    return Refused("COM indisponible", E_FAIL);
  }
  ComPtr<IWICImagingFactory> factory;
  HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&factory));
  if (FAILED(result)) {
    return Refused("fabrique absente", result);
  }
  ComPtr<IWICStream> stream;
  result = factory->CreateStream(&stream);
  if (FAILED(result)) {
    return Refused("flux refusé", result);
  }
  // ⚠ WIC NE MODIFIE PAS CES OCTETS, mais son interface n'est pas `const`.
  result = stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),
                                        static_cast<DWORD>(bytes.size()));
  if (FAILED(result)) {
    return Refused("flux refusé", result);
  }
  // 🔴 LE DÉCODEUR WEBP NOMMÉMENT, et non celui que le système devinerait :
  // deviner, c'est confier les octets au premier codec qui les réclame. Il peut
  // manquer : Windows N, LTSC, Server.
  ComPtr<IWICBitmapDecoder> decoder;
  result = factory->CreateDecoder(GUID_ContainerFormatWebp, nullptr, &decoder);
  if (FAILED(result)) {
    return Refused("aucun décodeur WebP sur ce système", result);
  }
  result = decoder->Initialize(stream.Get(), WICDecodeMetadataCacheOnDemand);
  if (FAILED(result)) {
    return Refused("WebP illisible", result);
  }
  ComPtr<IWICBitmapFrameDecode> frame;
  result = decoder->GetFrame(0, &frame);
  if (FAILED(result)) {
    return Refused("première image illisible", result);
  }
  UINT width = 0;
  UINT height = 0;
  result = frame->GetSize(&width, &height);
  if (FAILED(result) || width == 0 || height == 0) {
    return Refused("taille illisible", result);
  }
  if (width > kMostSide || height > kMostSide ||
      static_cast<std::size_t>(width) * height * kChannels > kMostDecodedBytes) {
    return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument,
                          "WIC : image trop grande pour être décodée sans risque");
  }

  ComPtr<IWICFormatConverter> converter;
  result = factory->CreateFormatConverter(&converter);
  if (FAILED(result)) {
    return Refused("conversion refusée", result);
  }
  // RGBA8 non prémultiplié, comme tout ce que le client téléverse.
  result = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                 nullptr, 0.0, WICBitmapPaletteTypeCustom);
  if (FAILED(result)) {
    return Refused("conversion refusée", result);
  }
  ro::formats::Image image;
  image.width = static_cast<std::int32_t>(width);
  image.height = static_cast<std::int32_t>(height);
  image.pixels.resize(static_cast<std::size_t>(width) * height * kChannels);
  const auto stride = static_cast<UINT>(static_cast<std::size_t>(width) * kChannels);
  result = converter->CopyPixels(nullptr, stride, static_cast<UINT>(image.pixels.size()),
                                 image.pixels.data());
  if (FAILED(result)) {
    return Refused("pixels illisibles", result);
  }

  ro::formats::DecodedWebPicture decoded;
  decoded.frames.push_back(std::move(image));
  decoded.delays_ms.push_back(0);
  return decoded;
}

}  // namespace moonclient
