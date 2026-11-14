#include "ro/formats/assist_address.h"

#include <catch2/catch_test_macros.hpp>

using ro::formats::AssistOrigin;

TEST_CASE("L'origine prend http quand l'adresse n'a pas de schéma", "[formats][assist_address]") {
  CHECK(AssistOrigin("moonlight-destiny.fr:8888") == "http://moonlight-destiny.fr:8888");
  CHECK(AssistOrigin("https://site.example/") == "https://site.example");
  CHECK(AssistOrigin("  ").empty());
}
