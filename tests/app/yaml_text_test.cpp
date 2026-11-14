// UN TEXTE DE NOS TABLES YAML : nu ou par langue, et choisi à la demande.

#include "game/yaml_text.h"

#include <catch2/catch_test_macros.hpp>

using moonclient::ReadYamlText;
using moonclient::YamlText;

TEST_CASE("Texte YAML : une chaîne nue vaut pour toutes les langues", "[app][yaml]") {
  const YamlText text = ReadYamlText(YAML::Load("Krieger"));
  CHECK(text.In("en") == "Krieger");
  CHECK(text.In("es", "en") == "Krieger");
}

TEST_CASE("Texte YAML : la langue demandée, puis celle du fichier, puis le français",
          "[app][yaml]") {
  const YamlText text = ReadYamlText(YAML::Load("{ es: Hola, fr: Salut, en: Hello }"));
  CHECK(text.In("en") == "Hello");
  CHECK(text.In("de", "es") == "Hola");
  CHECK(text.In("de") == "Salut");
  // Sans français, la première langue écrite.
  const YamlText foreign = ReadYamlText(YAML::Load("{ es: Hola, en: Hello }"));
  CHECK(foreign.In("de") == "Hola");
}

TEST_CASE("Texte YAML : une langue vide est ignorée, une autre forme rend un vide", "[app][yaml]") {
  const YamlText partial = ReadYamlText(YAML::Load("{ en: '', fr: Salut }"));
  CHECK(partial.In("en") == "Salut");
  CHECK(ReadYamlText(YAML::Load("[1, 2]")).empty());
  CHECK(ReadYamlText(YAML::Node()).empty());
  CHECK(ReadYamlText(YAML::Node()).In("fr").empty());
}
