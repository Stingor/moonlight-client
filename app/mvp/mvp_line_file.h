#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace moonclient {

// LES LIGNES DÉTACHÉES DU CARNET DE CHASSE MVP, sur le disque :
// `savedata\mvp_tracker_lines.yaml`, clés anglaises, HORS des profils — une
// ligne posée à l'écran tient à ce poste, pas à un jeu de réglages.
inline constexpr const char* kMvpLineFileName = "mvp_tracker_lines.yaml";

// UNE LIGNE DÉTACHÉE : la clé STABLE (monstre, carte) — jamais le rang du
// créneau, que le serveur refait à chaque démarrage —, et sa place à l'écran
// en pixels.
struct MvpPinnedLine {
  // Zéro : créneau scripté, que la carte identifie.
  std::uint16_t mob_id = 0;
  std::string map;
  int x = 0;
  int y = 0;
};

[[nodiscard]] std::string WriteMvpLines(const std::vector<MvpPinnedLine>& lines);
// Faux : texte illisible, et `into` intact. Une entrée sans carte, ou de x ou y
// négatif, est sautée.
[[nodiscard]] bool ReadMvpLines(std::string_view text, std::vector<MvpPinnedLine>& into);
// Absent : liste vide ET succès. Illisible : faux — et le fichier ne doit pas
// être réécrit tant qu'on ne l'a pas relu proprement, pour ne pas effacer ce
// que le joueur corrigerait à la main.
[[nodiscard]] bool LoadMvpLines(const std::filesystem::path& client_root,
                                std::vector<MvpPinnedLine>& into);
[[nodiscard]] bool SaveMvpLines(const std::filesystem::path& client_root,
                                const std::vector<MvpPinnedLine>& lines);

}  // namespace moonclient
