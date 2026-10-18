#pragma once

#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/guild_tab.h"

// LES VOLETS DE L'ONGLET GUILDE, chacun dans son fichier.
//
// En-tête PRIVÉ : personne hors de cette bibliothèque n'a à dessiner un
// sous-onglet de guilde. La coque (`guild_tab.cpp`) décide de la place et de
// l'ordre ; les volets décident du contenu.
namespace ro::ui {

// ── LA PALETTE DE L'ONGLET ──────────────────────────────────────────────────
//
// Ce sont des écarts VOULUS par
// rapport au texte ordinaire, réglés pour le corps clair des fenêtres du jeu.
// Le libellé d'un couple « libellé : valeur », et tout texte secondaire.
inline constexpr ImU32 kGuildLabelColor = IM_COL32(89, 89, 107, 255);
// Ce qui doit trancher : un nom, un titre de section.
inline constexpr ImU32 kGuildBlackColor = IM_COL32(0, 0, 0, 255);
// L'information : le poste du maître, le sujet de l'annonce.
inline constexpr ImU32 kGuildBlueColor = IM_COL32(38, 64, 153, 255);
// Ce qui va bien : en ligne, alliés, guilde créée.
inline constexpr ImU32 kGuildGreenColor = IM_COL32(26, 128, 38, 255);
// Ce qui refuse ou détruit.
inline constexpr ImU32 kGuildRedColor = IM_COL32(153, 31, 31, 255);
// Le fond d'une ligne de membre CONNECTÉ, comme le client la teinte.
inline constexpr ImU32 kGuildOnlineRowColor = IM_COL32(198, 232, 198, 160);

[[nodiscard]] ImVec4 GuildInk(ImU32 color);

// Copie un texte dans un tampon, sans couper un caractère en deux.
void CopyIntoBuffer(std::span<char> buffer, std::string_view text);

// ── LES IDENTIFIANTS DES BOÎTES ─────────────────────────────────────────────
inline constexpr std::string_view kGuildLeaveModalId = "###guild_leave";
inline constexpr std::string_view kGuildDisbandModalId = "###guild_disband";
inline constexpr std::string_view kGuildExpelModalId = "###guild_expel";
inline constexpr std::string_view kGuildTransferModalId = "###guild_transfer";
inline constexpr std::string_view kGuildRelationModalId = "###guild_relation";
inline constexpr std::string_view kGuildEmblemModalId = "###guild_emblem";

// ── LES VOLETS ──────────────────────────────────────────────────────────────

void DrawGuildMembers(GuildTabState& state, const GuildView& view, float list_height,
                      const SkinArt& art, GuildAction& action);
void DrawGuildPositions(GuildTabState& state, const GuildView& view, const SkinArt& art,
                        GuildAction& action);
void DrawGuildSkills(GuildTabState& state, const GuildView& view, bool& smooth_icons,
                     bool staff_allowed, const SkinArt& art, GuildAction& action);
void DrawGuildRelations(GuildTabState& state, const GuildView& view, const SkinArt& art);
void DrawGuildBans(const GuildView& view);

// Les cinq confirmations : quitter, dissoudre, expulser, transférer, rompre.
void DrawGuildConfirmations(GuildTabState& state, const GuildView& view, const SkinArt& art,
                            GuildAction& action);

// L'éditeur d'emblème : la boîte, son onglet de fichiers, et son onglet de
// dessin.
void DrawGuildEmblemModal(GuildTabState& state, const GuildView& view, const SkinArt& art,
                          GuildAction& action);
void DrawEmblemPaintTab(GuildTabState& state, const GuildView& view, const SkinArt& art,
                        GuildAction& action);

}  // namespace ro::ui
