#include "ro/vfs/encoding.h"

#include <algorithm>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>

#include <iconv.h>
#endif

namespace ro::vfs {
namespace {

using core::ErrorCode;
using core::Fail;
using core::Result;

// Page de codes coréenne unifiée (Unified Hangul Code), celle des noms de
// fichiers RO.
constexpr int kCodePageCp949 = 949;

// Un texte ASCII s'écrit de même en UTF-8 et en CP949 : il n'y a rien à
// transcoder.
[[nodiscard]] bool IsAscii(std::string_view text) {
  return std::ranges::all_of(
      text, [](char character) { return static_cast<unsigned char>(character) < 0x80; });
}

#ifdef _WIN32

Result<std::wstring> ToWide(std::string_view text, unsigned int code_page) {
  if (text.empty()) {
    return std::wstring{};
  }
  const int needed =
      MultiByteToWideChar(code_page, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (needed <= 0) {
    return Fail(ErrorCode::kCorruptData, "séquence intraduisible vers UTF-16");
  }
  std::wstring wide(static_cast<std::size_t>(needed), L'\0');
  MultiByteToWideChar(code_page, 0, text.data(), static_cast<int>(text.size()), wide.data(),
                      needed);
  return wide;
}

Result<std::string> FromWide(std::wstring_view wide, unsigned int code_page) {
  if (wide.empty()) {
    return std::string{};
  }
  // ⚠ UN CARACTÈRE ABSENT DE LA PAGE NE FAIT PAS ÉCHOUER L'APPEL : il devient
  // « ? » ou son plus proche voisin, ce qui donne un autre nom. L'UTF-8 écrit tout
  // caractère, et le système y refuse ces deux paramètres.
  const bool can_replace = code_page != CP_UTF8;
  const DWORD flags = can_replace ? WC_NO_BEST_FIT_CHARS : 0;
  BOOL replaced = FALSE;
  const int needed = WideCharToMultiByte(
      code_page, flags, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
  if (needed <= 0) {
    return Fail(ErrorCode::kCorruptData, "séquence intraduisible depuis UTF-16");
  }
  std::string narrow(static_cast<std::size_t>(needed), '\0');
  WideCharToMultiByte(code_page, flags, wide.data(), static_cast<int>(wide.size()), narrow.data(),
                      needed, nullptr, can_replace ? &replaced : nullptr);
  if (replaced != FALSE) {
    return Fail(ErrorCode::kCorruptData, "caractère absent de la page de code");
  }
  return narrow;
}

Result<std::string> Convert(std::string_view input, unsigned int from, unsigned int to) {
  auto wide = ToWide(input, from);
  if (!wide) {
    return std::unexpected(wide.error());
  }
  return FromWide(*wide, to);
}

#else

// glibc nomme la page 949 « CP949 » ; certaines implémentations ne connaissent
// qu'« UHC ». On essaie les deux avant d'abandonner.
Result<std::string> Convert(std::string_view input, const char* from, const char* to) {
  if (input.empty()) {
    return std::string{};
  }

  iconv_t converter = iconv_open(to, from);
  if (converter == reinterpret_cast<iconv_t>(-1)) {
    return Fail(ErrorCode::kIoFailure,
                std::string("iconv ne sait pas convertir ") + from + " vers " + to);
  }

  std::string input_copy(input);
  char* source = input_copy.data();
  std::size_t source_left = input_copy.size();

  // Le CP949 se dilate au plus d'un facteur 3 vers l'UTF-8 ; l'inverse se
  // contracte. Un facteur 4 couvre les deux sens avec de la marge.
  std::string output(input.size() * 4 + 4, '\0');
  char* destination = output.data();
  std::size_t destination_left = output.size();

  const std::size_t converted =
      iconv(converter, &source, &source_left, &destination, &destination_left);
  iconv_close(converter);

  if (converted == static_cast<std::size_t>(-1)) {
    return Fail(ErrorCode::kCorruptData, "séquence intraduisible");
  }
  output.resize(output.size() - destination_left);
  return output;
}

#endif

}  // namespace

Result<std::string> Utf8ToCp949(std::string_view utf8) {
  if (IsAscii(utf8)) {
    return std::string(utf8);
  }
#ifdef _WIN32
  return Convert(utf8, CP_UTF8, kCodePageCp949);
#else
  auto converted = Convert(utf8, "UTF-8", "CP949");
  if (!converted) {
    converted = Convert(utf8, "UTF-8", "UHC");
  }
  return converted;
#endif
}

Result<std::string> Cp949ToUtf8(std::string_view cp949) {
  if (IsAscii(cp949)) {
    return std::string(cp949);
  }
#ifdef _WIN32
  return Convert(cp949, kCodePageCp949, CP_UTF8);
#else
  auto converted = Convert(cp949, "CP949", "UTF-8");
  if (!converted) {
    converted = Convert(cp949, "UHC", "UTF-8");
  }
  return converted;
#endif
}

std::string FileNameOfRaw(std::string_view raw) {
  std::string decoded = Cp949ToUtf8Lossy(raw);
  const core::Result<std::string> back = Utf8ToCp949(decoded);
  return back.has_value() && *back == raw ? std::move(decoded) : std::string(raw);
}

std::string Cp949ToUtf8Lossy(std::string_view cp949) {
  auto converted = Cp949ToUtf8(cp949);
  if (converted) {
    return *converted;
  }
  // Repli caractère par caractère : on garde l'ASCII et on marque le reste.
  std::string lossy;
  lossy.reserve(cp949.size());
  for (const char character : cp949) {
    lossy.push_back(static_cast<unsigned char>(character) < 0x80 ? character : '?');
  }
  return lossy;
}

#ifdef _WIN32
Result<std::wstring> WideFromCodePage(std::string_view bytes, unsigned int code_page) {
  if (bytes.empty()) {
    return std::wstring{};
  }
  const int needed = MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, bytes.data(),
                                         static_cast<int>(bytes.size()), nullptr, 0);
  if (needed <= 0) {
    return Fail(ErrorCode::kCorruptData, "octets invalides dans cette page de code");
  }
  std::wstring wide(static_cast<std::size_t>(needed), L'\0');
  MultiByteToWideChar(code_page, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()),
                      wide.data(), needed);
  return wide;
}
#endif

}  // namespace ro::vfs
