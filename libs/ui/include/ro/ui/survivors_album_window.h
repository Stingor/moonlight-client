#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include <imgui.h>

namespace ro::ui {

// L'ALBUM DES SURVIVANTS (Q12) : une grille des cartes de l'arène — trouvée,
// son illustration et son nombre d'exemplaires ; non trouvée, le dos de carte,
// et le monstre qui la laisse tomber au survol —, le détail d'une carte au
// clic ou par Entrée, et sous la grille les skills à débloquer avec leur
// progression.
//
// 🔴 UNE CARTE NON TROUVÉE NE MONTRE JAMAIS SON ILLUSTRATION, ni dans la
// grille ni dans le détail : le trou est ce qui donne envie de la chercher.
//
// L'Album est l'onglet Cartes du Codex, qui dessine son corps. Il se lit
// seulement : il ne rend aucune intention. Touches, fenêtre au premier plan :
// flèches dans la grille, Entrée ouvre le détail.

// Une carte de l'arène, déjà résolue par `app`.
struct SurvivorsAlbumEntry {
  ImTextureID illustration = 0;
  ImVec2 illustration_size = ImVec2(0.0F, 0.0F);
  const char* name = "";  // tiré des tables du jeu
  // L'effet dans le mode. ⚠ EN FRANÇAIS, marqué par `TrText` : traduit ici.
  const char* description = "";
  const char* dropped_by = "";       // le monstre qui la laisse tomber, tiré des tables du jeu
  std::uint32_t found = 0;           // les exemplaires trouvés ; 0 : non trouvée
  std::int64_t first_found_day = 0;  // jours depuis le 1970-01-01 UTC
};

// Ce que mesure le compteur d'un exploit.
enum class SurvivorsFeatMeasure : std::uint8_t {
  kSurvivedMs,  // la plus longue partie survécue, en millisecondes
  kCount,       // un compte au cumul
};

// Un skill à débloquer par exploit.
struct SurvivorsFeatEntry {
  const char* key = "";  // la clé de l'exploit : elle choisit le texte du but
  ImTextureID icon = 0;
  const char* name = "";  // tiré des tables du jeu
  SurvivorsFeatMeasure measure = SurvivorsFeatMeasure::kCount;
  std::uint64_t progress = 0;
  std::uint64_t threshold = 0;
  bool reached = false;
  // Atteint par la partie que montre l'écran de fin : lui seul s'y annonce en
  // couleur, un exploit plus ancien s'y dit atteint en gris.
  bool fresh = false;
  bool soon = false;  // un exploit d'un contenu à venir : grisé, « bientôt »
};

struct SurvivorsAlbumView {
  std::span<const SurvivorsAlbumEntry> cards;
  ImTextureID card_back = 0;
  ImVec2 card_back_size = ImVec2(0.0F, 0.0F);
  std::span<const SurvivorsFeatEntry> feats;
};

struct SurvivorsAlbumState {
  std::size_t cursor = 0;
  std::optional<std::size_t> detail;
};

// LE CORPS DE L'ALBUM — grille, détail, skills à débloquer —, dans la fenêtre
// courante : l'onglet Cartes du Codex le dessine.
void DrawSurvivorsAlbumBody(SurvivorsAlbumState& state, const SurvivorsAlbumView& view);

// LA PROGRESSION D'UN EXPLOIT, « 312 / 500 » ou « 03:12 / 05:00 ».
inline constexpr std::size_t kSurvivorsFeatProgressTextSize = 48;
[[nodiscard]] std::array<char, kSurvivorsFeatProgressTextSize> SurvivorsFeatProgressText(
    const SurvivorsFeatEntry& feat);

}  // namespace ro::ui
