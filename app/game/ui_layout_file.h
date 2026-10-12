#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "ro/ui/window_places.h"

#include "game/profile_book.h"

namespace moonclient {

// ── LES PROFILS DE DISPOSITION SUR LE DISQUE : `savedata/ui_layouts.yml` ──
//
// Des profils nommés, chacun une disposition entière — voir
// `ro::ui::LayoutProfile` —, et pour chaque personnage le nom du profil qu'il
// pose en entrant en jeu. Voir `ProfileBook`.
//
// ⚠ LE FORMAT EST VERSIONNÉ : un fichier d'une version inconnue ne se relit
// pas, et ne s'écrase pas.
//
// ⚠ LA SECTION « minimap » D'UN PROFIL EST FACULTATIVE, et n'a pas changé la
// version : un fichier qui n'en a pas se lit, ses profils héritant alors de
// `navigation.yml` ; un client qui ne la connaît pas la saute.

inline constexpr int kUiLayoutFileVersion = 1;
inline constexpr std::string_view kUiLayoutFileName = "ui_layouts.yml";

using UiLayoutBook = ProfileBook<ro::ui::LayoutProfile>;

[[nodiscard]] std::filesystem::path UiLayoutPath(const std::filesystem::path& client_root);

// LE TEXTE DU FICHIER, et sa lecture. Séparés du disque pour les essais.
[[nodiscard]] std::string WriteUiLayouts(const UiLayoutBook& book);
// Faux pour un texte illisible ou d'une version inconnue : `into` reste alors
// tel quel. Une entrée illisible est sautée, le reste se lit.
bool ReadUiLayouts(std::string_view text, UiLayoutBook& into);

// Un fichier absent laisse le livre vide, et rend vrai. Faux pour un fichier
// qu'on ne doit pas écraser.
bool LoadUiLayouts(const std::filesystem::path& client_root, UiLayoutBook& into);
// Vrai si le fichier est écrit.
bool SaveUiLayouts(const std::filesystem::path& client_root, const UiLayoutBook& from);

}  // namespace moonclient
