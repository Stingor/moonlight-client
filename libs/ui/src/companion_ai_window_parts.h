#pragma once

// LES SECTIONS DE LA FENÊTRE « IA DU COMPAGNON », chacune dans son fichier.
// Chacune dessine depuis la vue et pose au plus une intention dans `action`.

#include <span>

#include "ro/ui/companion_ai_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui::companion_ai_parts {

// UN PARAGRAPHE À LA LIGNE, qui suit la largeur de la fenêtre SANS compter
// dans sa ligne la plus large : celle-ci plafonne la fenêtre, et un
// paragraphe, qui prend toute la place offerte, la ferait rétrécir d'image en
// image.
void WrappedNote(const char* text);

void DrawSettingsSection(const CompanionAiView& view, const SkinArt& art,
                         CompanionAiWindowAction& action);
void DrawTacticsSection(CompanionAiWindowState& state, const CompanionAiView& view,
                        const SkinArt& art, CompanionAiWindowAction& action);
void DrawRulesSection(CompanionAiWindowState& state, const CompanionAiView& view,
                      const SkinArt& art, CompanionAiWindowAction& action);
void DrawPlayersSection(CompanionAiWindowState& state, const CompanionAiView& view,
                        const SkinArt& art, CompanionAiWindowAction& action);
// L'EN-TÊTE DES PROFILS : le profil édité, ses boutons et leurs saisies, puis
// les trois contextes.
void DrawProfileHeader(CompanionAiWindowState& state, const CompanionAiView& view,
                       const SkinArt& art, CompanionAiWindowAction& action);
// LA PROPOSITION DE RECOPIE, en bannière ; rien sans elle.
void DrawProposalBanner(const CompanionProfilesView& profiles, const SkinArt& art,
                        CompanionAiWindowAction& action);

// LES LIBELLÉS DES RANGS ET DES DÉCLENCHEURS, partagés par les tactiques et les
// conduites.
[[nodiscard]] std::span<const char* const> TacticRankLabels();
[[nodiscard]] std::span<const char* const> TacticTriggerLabels();

// LE NIVEAU D'UN SKILL : « niveau appris » (zéro), ou un niveau de 1 à
// `learned`. Rend vrai quand il a changé.
[[nodiscard]] bool LevelCombo(const char* id, int* level, int learned, const SkinArt& art);

// UN DÉLAI GARDÉ EN MILLISECONDES, tapé en secondes au dixième près, borné à
// `most_ms`. Rend vrai quand il a changé.
[[nodiscard]] bool TypedSeconds(const char* label, int* delay_ms, int most_ms, const SkinArt& art);

}  // namespace ro::ui::companion_ai_parts
