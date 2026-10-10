#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace ro::core {

// Codes d'erreur des couches basses.
//
// Volontairement peu nombreux : l'information utile au diagnostic est dans le
// `detail` de l'Error, pas dans une taxonomie de trente codes que personne ne
// distingue au site d'appel.
enum class ErrorCode {
  kIoFailure,           // le support n'a pas rendu les octets demandés
  kUnexpectedEnd,       // lecture au-delà de la fin du tampon
  kBadSignature,        // l'en-tête n'est pas celui du format attendu
  kUnsupportedVersion,  // format reconnu, révision non gérée
  kCorruptData,         // valeurs incohérentes entre elles
  kEncrypted,           // contenu chiffré, illisible sans la clé
  kNotFound,            // l'entrée demandée n'existe pas
  kInvalidArgument,     // ce que l'APPELANT a fourni ne convient pas
};

[[nodiscard]] std::string_view ToString(ErrorCode code) noexcept;

struct Error {
  ErrorCode code = ErrorCode::kCorruptData;
  std::string detail;
};

// Message d'une ligne, destiné au journal : « donnée corrompue : … ».
[[nodiscard]] std::string Describe(const Error& error);

// Toutes les fonctions faillibles des couches basses rendent un Result.
//
// Pas d'exception : ces parseurs reçoivent des octets venus d'archives que l'on
// ne contrôle pas, et l'échec y est un cas NORMAL, pas exceptionnel. Un chemin
// d'erreur explicite est aussi ce qui rend le fuzzing exploitable — un crash
// devient un vrai signal.
template <typename T>
using Result = std::expected<T, Error>;

// `return Fail(ErrorCode::kBadSignature, "…");` se convertit vers n'importe
// quel Result<T>.
[[nodiscard]] std::unexpected<Error> Fail(ErrorCode code, std::string detail);

}  // namespace ro::core
