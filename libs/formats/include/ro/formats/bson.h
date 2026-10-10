#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// LES FICHIERS DE DONNÉES BINAIRES DU CLIENT, au format BSON.
//
// Le client range trois tables sous `data\contentdata` dans ce format plutôt
// qu'en Lua : la table des effets NOMMÉS et les deux tables de réputation. Ce
// sont des données que rien d'autre ne porte — ni l'exécutable, ni les `.lub`.
//
// LE SOUS-ENSEMBLE EST MESURÉ, PAS SUPPOSÉ. BSON en compte une vingtaine de
// types ; les trois fichiers de l'installation n'en emploient que six — double,
// chaîne, document, tableau, booléen et entier 32 bits. Le reste est REFUSÉ
// plutôt que sauté : un type inconnu veut dire que la lecture a dérapé, et
// continuer rendrait une table qui a l'air complète.
//
// ⚠ LES CHAÎNES NE SONT PAS TRANSCODÉES. La spécification les dit en UTF-8, et
// le client les convertit pourtant DEPUIS sa page de codes (CP949). Rendre les
// octets tels quels laisse l'appelant trancher — un chemin d'archive se lit
// avec ReadRaw, un libellé affiché se transcode.

struct BsonDocument;

// Une valeur BSON.
//
// Un agrégat étiqueté plutôt qu'un variant : c'est la forme qu'a déjà LuaValue,
// et elle se lit sans détour au site d'appel. Les champs inutiles à la sorte
// portée gardent leur valeur par défaut.
struct BsonValue {
  enum class Kind : std::uint8_t { kNumber, kText, kBoolean, kInteger, kDocument, kArray };

  Kind kind = Kind::kNumber;
  bool boolean = false;
  double number = 0.0;
  std::int32_t integer = 0;
  std::string text;
  std::shared_ptr<BsonDocument> document;
};

// Un document BSON : des champs nommés, dans l'ordre du fichier.
//
// Aucun index : un document de ces fichiers porte onze champs au plus, et la
// table entière ne se lit qu'une fois au démarrage. Un index coûterait plus à
// construire qu'il ne ferait gagner.
struct BsonDocument {
  std::vector<std::pair<std::string, BsonValue>> fields;

  // Nul si le champ est absent. Une absence est un cas NORMAL : ces fichiers
  // sont écrits à la main et un champ facultatif y manque parfois.
  [[nodiscard]] const BsonValue* Find(std::string_view name) const;
};

// Jusqu'où les documents peuvent s'emboîter.
//
// Les trois fichiers de l'installation tiennent en trois étages — la racine, la
// table, la fiche. La borne est là pour les octets qu'on ne contrôle pas : un
// document qui se dit imbriqué mille fois ferait déborder la pile d'un lecteur
// récursif, et c'est un plantage plutôt qu'un refus.
inline constexpr std::size_t kMaxBsonDepth = 32;

// Lit un document BSON complet.
//
// Échoue si le tampon est tronqué, si une taille annoncée déborde de son
// document parent, si un document ne finit pas par son octet nul, ou si un type
// d'élément sort du sous-ensemble mesuré.
[[nodiscard]] core::Result<BsonDocument> ParseBson(std::span<const std::uint8_t> bytes);

}  // namespace ro::formats
