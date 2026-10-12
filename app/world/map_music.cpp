#include "world/map_music.h"

#include <cstdio>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

namespace moonclient {
namespace {

// Lit un fichier de l'installation, hors de toute archive.
std::vector<std::uint8_t> ReadFromDisk(const std::filesystem::path& path) {
  std::vector<std::uint8_t> bytes;
  std::FILE* file = nullptr;
#ifdef _WIN32
  if (_wfopen_s(&file, path.wstring().c_str(), L"rb") != 0 || file == nullptr) {
    return bytes;
  }
#else
  file = std::fopen(path.string().c_str(), "rb");
  if (file == nullptr) {
    return bytes;
  }
#endif
  constexpr std::size_t kChunk = 64 * 1024;
  std::vector<std::uint8_t> chunk(kChunk);
  while (const std::size_t read = std::fread(chunk.data(), 1, chunk.size(), file)) {
    bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(read));
  }
  std::fclose(file);
  return bytes;
}

}  // namespace

ro::audio::AudioEngine OpenAudio() {
  auto opened = ro::audio::AudioEngine::Open();
  if (!opened) {
    ro::core::LogWarning("son : {} — on jouera en silence", ro::core::Describe(opened.error()));
    return {};
  }
  std::string decoders;
  for (const std::string& name : opened->Decoders()) {
    decoders += decoders.empty() ? name : ", " + name;
  }
  // AU DÉTAIL : cette liste sort des décodeurs compilés dans SDL3_mixer. Elle
  // est la même à chaque lancement et pour toutes les installations, donc elle
  // ne distingue aucune partie d'une autre.
  ro::core::LogDebug("son : décodeurs {}", decoders);
  // ⚠ ON NE POSE NI COUPURE NI VOLUME ICI : les options le feront, une fois le
  // fichier de réglages lu. Le faire aux deux endroits ferait gagner celui qui
  // parle en dernier, et ce n'est pas celui qu'on croit.
  return std::move(*opened);
}

MapMusic::MapMusic(ro::vfs::Vfs& vfs, std::filesystem::path client_root)
    : client_root_(std::move(client_root)) {
  auto bytes = vfs.Read(std::string(ro::formats::kBgmTablePath));
  if (!bytes) {
    ro::core::LogWarning("musique : table absente — {}", ro::formats::kBgmTablePath);
    return;
  }
  table_ = ro::formats::BgmTable::Parse(*bytes);
  ro::core::LogInfo("musique : {} carte(s) nommée(s)", table_.size());
}

void MapMusic::Follow(ro::audio::AudioEngine& audio, const std::string& map_name) {
  if (map_name == followed_) {
    return;
  }
  followed_ = map_name;

  const std::string_view track = table_.TrackFor(map_name);
  if (track.empty()) {
    // 🔴 LE SILENCE SE DIT, ET C'EST TOUT L'OBJET DE CETTE LIGNE. Une carte que
    // la table ne nomme pas se joue sans musique — c'est un choix du serveur,
    // pas une panne —, mais ce silence-là est trait pour trait celui d'une
    // musique cassée. Sans cette ligne, « les cartes ne lisent pas leur
    // musique » ne se distingue pas de « celle-ci n'en a pas », et trancher
    // demande d'aller relire `mp3nametable.txt` à la main.
    //
    // ⚠ C'EST LE CAS DES CARTES AJOUTÉES PAR LE SERVEUR : une carte plate
    // d'administration, une arène, un lieu d'événement ne figurent pas dans
    // une table qui vient du client d'origine.
    ro::core::LogInfo("musique : {} n'en déclare aucune — silence", map_name);
    audio.StopMusic();
    return;
  }

  // ⚠ RIEN À LIRE QUAND RIEN NE JOUERA : la musique coupée ou sans carte son,
  // ou le morceau qui joue déjà — deux cartes voisines le partagent souvent.
  // Le fichier fait plusieurs mégaoctets, et sa lecture tombe sur la première
  // image de la carte.
  if (!audio.is_open() || !audio.music_audible()) {
    ro::core::LogInfo("musique : {} — {}, non jouée : son coupé ou absent", map_name, track);
    return;
  }
  if (audio.music_name() == track) {
    ro::core::LogInfo("musique : {} — {}, qui joue déjà", map_name, track);
    return;
  }

  // ⚠ LE NOM VIENT DE LA TABLE DU JEU, « bgm\131.mp3 » : `FindOnDisk` en
  // retrouve le fichier quels que soient les séparateurs et la casse.
  const std::filesystem::path file = ro::vfs::FindOnDisk(client_root_, track);
  const std::vector<std::uint8_t> bytes =
      file.empty() ? std::vector<std::uint8_t>() : ReadFromDisk(file);
  if (bytes.empty()) {
    ro::core::LogWarning("musique : fichier absent — {}",
                         ro::core::Utf8FromPath((client_root_ / std::filesystem::path(track))));
    audio.StopMusic();
    return;
  }
  if (auto played = audio.PlayMusic(std::string(track), bytes); !played) {
    ro::core::LogWarning("musique : {} — {}", track, ro::core::Describe(played.error()));
    return;
  }
  // ⚠ ET CE QUI MARCHE SE DIT AUSSI. Un journal qui ne nomme que les échecs
  // laisse croire, quand il se tait, que rien n'a jamais été tenté — c'est-à-
  // dire exactement ce qu'on cherche à savoir quand on n'entend rien.
  ro::core::LogInfo("musique : {} — {}", map_name, track);
}

}  // namespace moonclient
