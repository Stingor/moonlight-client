#pragma once

#include <cstdint>
#include <span>

namespace ro::audio {

// LE DÉPART D'UN BRUIT : deux défauts qui ne tiennent pas au mélange mais au
// moment où un bruit commence, et que le client corrige ici bien que le jeu
// d'origine les laisse passer.

// ── Un même bruit lancé plusieurs fois au même instant ──────────────────────
//
// Une unité au sol couvre souvent vingt-cinq cases, et chaque case lance son
// effet — donc son bruit — au même tic que ses voisines. Vingt-cinq copies
// ALIGNÉES À L'ÉCHANTILLON s'additionnent en amplitude : vingt-cinq fois plus
// fort, vingt-huit décibels, ce qu'aucune sortie ne tient sans écrêter. C'est
// le grésillement de Quagmire. Et chacune prend une voix, si bien qu'un
// seul sort en occupe vingt-cinq sur trente-deux.
//
// Ces copies sont FUSIONNÉES en une voix, qui prend la somme de leurs
// ÉNERGIES : c'est ainsi que s'ajoutent des sources distinctes, dont les
// phases ne coïncident pas. Vingt-cinq sources égales valent cinq fois
// l'amplitude d'une seule, et non vingt-cinq ; le gain final reste de toute
// façon plafonné à la pleine échelle.

// La fenêtre en deçà de laquelle deux départs du même bruit n'en font qu'un,
// en millisecondes : deux images de jeu, et moins que l'écart à partir duquel
// l'oreille entend un écho plutôt qu'un seul son.
inline constexpr std::uint64_t kOnsetMergeWindowMs = 30;

// Vrai si un bruit lancé à `now_ms` part avec celui lancé à `first_ms`. Une
// horloge qui recule ne fusionne pas.
[[nodiscard]] bool SameOnset(std::uint64_t first_ms, std::uint64_t now_ms);

// Ce qu'une voix porte après avoir absorbé un départ de plus.
struct MergedOnset {
  // La somme des énergies, en amplitude : √(g₁² + g₂²). NON PLAFONNÉE, pour
  // que la fusion suivante parte de la vraie somme.
  float gain = 0.0F;
  // Le côté, moyenné selon l'énergie de chaque départ : le plus fort tire.
  float pan = 0.0F;
};

// Fusionne un départ (`added_gain`, `added_pan`) dans la voix qui porte déjà
// (`held_gain`, `held_pan`). Fusionner un à un revient à fusionner tout d'un
// coup : l'énergie et la moyenne pondérée se composent.
[[nodiscard]] MergedOnset MergeOnset(float held_gain, float held_pan, float added_gain,
                                     float added_pan);

// ── Un bruit qui commence en pleine forme d'onde ────────────────────────────
//
// Un fichier dont le premier échantillon n'est pas nul fait sauter la sortie
// du silence à cette valeur d'un coup : un claquement à chaque départ. Celui de
// Quagmire part à −17 dB de la pleine échelle. Ces bruits-là entrent par un
// fondu très court ; les autres partent tels quels.

// Le premier échantillon au-delà duquel un départ claque, en fraction de la
// pleine échelle : quarante décibels dessous.
inline constexpr float kOpeningStepThreshold = 0.01F;

// La durée du fondu d'entrée d'un bruit qui claque, en millisecondes : assez
// pour effacer la marche, trop court pour émousser l'attaque.
inline constexpr int kOpeningFadeMs = 4;

// La valeur absolue du premier échantillon d'un fichier WAV, en fraction de la
// pleine échelle — la plus forte de ses voies. Zéro pour ce qui n'est pas du
// PCM 8 ou 16 bits lisible : sans le savoir, on ne touche à rien.
[[nodiscard]] float OpeningStep(std::span<const std::uint8_t> wav_bytes);

}  // namespace ro::audio
