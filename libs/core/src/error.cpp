#include "ro/core/error.h"

#include <utility>

namespace ro::core {

std::string_view ToString(ErrorCode code) noexcept {
  switch (code) {
    case ErrorCode::kIoFailure:
      return "erreur d'entrée/sortie";
    case ErrorCode::kUnexpectedEnd:
      return "fin de données prématurée";
    case ErrorCode::kBadSignature:
      return "signature inattendue";
    case ErrorCode::kUnsupportedVersion:
      return "version non gérée";
    case ErrorCode::kCorruptData:
      return "donnée corrompue";
    case ErrorCode::kEncrypted:
      return "contenu chiffré";
    case ErrorCode::kNotFound:
      return "introuvable";
    case ErrorCode::kInvalidArgument:
      return "argument invalide";
  }
  return "erreur inconnue";
}

std::string Describe(const Error& error) {
  std::string message(ToString(error.code));
  if (!error.detail.empty()) {
    message += " : ";
    message += error.detail;
  }
  return message;
}

std::unexpected<Error> Fail(ErrorCode code, std::string detail) {
  return std::unexpected<Error>(Error{code, std::move(detail)});
}

}  // namespace ro::core
