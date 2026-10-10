#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace moonclient {

// ── UN ÉCRIVAIN JSON MINIMAL, pour la console de mise au point ──────────────
//
// La console ÉCRIT du JSON, elle n'en lit jamais : ses requêtes sont des lignes
// de mots (voir `debug_request.h`). D'où un écrivain et pas de bibliothèque —
// quelques dizaines de lignes valent mieux qu'une dépendance de plus.
//
// Les virgules se posent seules : chaque valeur sait si elle est la première de
// son conteneur.
class JsonWriter {
 public:
  JsonWriter& BeginObject();
  JsonWriter& EndObject();
  JsonWriter& BeginArray();
  JsonWriter& EndArray();
  // La clé de la prochaine valeur, dans un objet.
  JsonWriter& Key(std::string_view key);
  JsonWriter& String(std::string_view text);
  JsonWriter& Int(std::int64_t number);
  JsonWriter& Bool(bool value);
  JsonWriter& Null();

  [[nodiscard]] const std::string& text() const noexcept { return text_; }

 private:
  // Avant une valeur : la virgule si elle n'est pas la première, rien après une clé.
  void Separate();

  std::string text_;
  // Pour chaque conteneur ouvert : a-t-il déjà une valeur ?
  std::vector<bool> filled_;
  bool after_key_ = false;
};

}  // namespace moonclient
