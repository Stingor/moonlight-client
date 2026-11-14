// CE QUE COÛTE UNE ÉCRITURE DE RÉGLAGES, et où passe le temps.
//
// 🔴 UN BANC, PAS UN ESSAI : il ne décide de rien, il MESURE. Le joueur voit le
// jeu se figer quand il touche un réglage dans un panneau, et le suspect est
// cette écriture — huit cents clés relues, refondues et réécrites sur le fil qui
// dessine. Une explication plausible ne vaut rien ici : on chronomètre.
//
// ⚠ IL NE S'EXÉCUTE QUE SUR DEMANDE (`[.banc]`), parce qu'un chronomètre dans
// une suite d'intégration rend un verdict différent à chaque machine.

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <catch2/catch_test_macros.hpp>

#include "game/settings_file.h"

namespace {

// L'ordre de grandeur réel : les huit barres de raccourcis, les six jauges
// libres, les six espèces de barres d'entité et le catalogue écrit à la main.
constexpr int kKeysInTheRealFile = 800;
constexpr int kRounds = 20;

[[nodiscard]] double MillisecondsOf(auto&& work) {
  const auto started = std::chrono::steady_clock::now();
  work();
  const auto ended = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(ended - started).count();
}

}  // namespace

TEST_CASE("Banc : ce que coûte une écriture de réglages", "[.banc][reglages]") {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "moonclient_banc_reglages.yaml";
  std::filesystem::remove(path);

  moonclient::SettingsFile file(path);
  std::vector<float> values(kKeysInTheRealFile, 1.0F);
  std::vector<std::string> keys;
  keys.reserve(values.size());
  for (int rank = 0; rank < kKeysInTheRealFile; ++rank) {
    keys.push_back("entite_monstres_reglage_" + std::to_string(rank));
    file.Declare(keys.back(), &values[static_cast<std::size_t>(rank)]);
  }
  file.Load();

  double worst = 0.0;
  double total = 0.0;
  for (int round = 0; round < kRounds; ++round) {
    values[0] = static_cast<float>(round);
    file.Touch();
    const double spent = MillisecondsOf([&file] { file.Flush(); });
    worst = std::max(worst, spent);
    total += spent;
  }

  std::printf("écriture de %d clés : %.1f ms en moyenne, %.1f ms au pire\n", kKeysInTheRealFile,
              total / kRounds, worst);

  // OÙ PASSE LE TEMPS : la relecture du fichier, la refonte en YAML, ou le
  // disque ? La correction n'est pas la même selon la réponse.
  const double parsing = MillisecondsOf([&path] {
    for (int round = 0; round < kRounds; ++round) {
      YAML::Node whole = YAML::LoadFile(path.string());
      static_cast<void>(whole.size());
    }
  });
  const double filling = MillisecondsOf([&keys, &values] {
    for (int round = 0; round < kRounds; ++round) {
      YAML::Node whole;
      for (std::size_t rank = 0; rank < keys.size(); ++rank) {
        whole[keys[rank]] = values[rank];
      }
      static_cast<void>(whole.size());
    }
  });
  YAML::Node whole;
  for (std::size_t rank = 0; rank < keys.size(); ++rank) {
    whole[keys[rank]] = values[rank];
  }
  const double emitting = MillisecondsOf([&whole] {
    for (int round = 0; round < kRounds; ++round) {
      YAML::Emitter out;
      out << whole;
      static_cast<void>(out.size());
    }
  });
  // Et ce que coûterait la MÊME sortie écrite à la main, sans passer par un
  // arbre : c'est un mapping plat, une ligne par clé.
  const double plain = MillisecondsOf([&keys, &values] {
    for (int round = 0; round < kRounds; ++round) {
      std::string text;
      text.reserve(keys.size() * 32);
      for (std::size_t rank = 0; rank < keys.size(); ++rank) {
        text += keys[rank];
        text += ": ";
        text += std::to_string(values[rank]);
        text += "\n";
      }
      static_cast<void>(text.size());
    }
  });
  std::printf(
      "  relecture %.1f ms · remplissage de l'arbre %.1f ms · émission %.1f ms · "
      "à la main %.2f ms\n",
      parsing / kRounds, filling / kRounds, emitting / kRounds, plain / kRounds);

  std::filesystem::remove(path);
  CHECK(worst > 0.0);
}
