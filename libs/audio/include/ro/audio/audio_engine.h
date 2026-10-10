#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "ro/audio/peak_limiter.h"
#include "ro/audio/sound_channel.h"
#include "ro/core/error.h"

namespace ro::audio {

// Le SON du jeu : la musique et les bruits.
//
// ⚠ Cette couche ne lit AUCUN fichier. On lui remet des octets déjà lus, parce
// que les deux sortes de son ne viennent pas du même endroit — les bruits sont
// dans l'archive, la musique dans un dossier qui n'y est pas — et qu'une couche
// de son n'a pas à connaître deux façons d'ouvrir un fichier.
//
// Elle ne connaît pas davantage le monde : une position est trois nombres, pas
// un acteur. Voir docs/adr/0013.

// De combien un son s'éteint avec la distance.
//
// Le client donne à chaque son placé une portée en unités du monde, au-delà
// de laquelle on n'entend plus rien. Deux cent cinquante est la valeur qu'il
// passe partout : trente-cinq cases environ, soit ce que la caméra montre.
inline constexpr float kDefaultHearingRange = 250.0F;

// Le volume, de zéro à un. Au-delà d'un, le mélangeur écrête.
inline constexpr float kFullVolume = 1.0F;

// Combien de bruits peuvent sonner en même temps.
//
// Sans plafond, une mêlée ouvrirait une voix par coup porté : le son se
// brouille bien avant de saturer, et rien ne le libérerait. Trente-deux est ce
// qu'un jeu de cette époque emploie, et de quoi couvrir un écran entier.
inline constexpr std::size_t kSoundVoices = 32;

// La durée du fondu d'un bruit qu'on fait taire avant sa fin, en
// millisecondes : assez pour qu'il ne claque pas en s'arrêtant, trop court pour
// s'entendre comme une traîne.
inline constexpr int kSilencingFadeMs = 30;

// Le mélangeur : un périphérique, la musique, et les bruits en cours.
//
// ⚠ Ouvrir le périphérique peut ÉCHOUER — une machine sans carte son, un
// serveur de son arrêté. Le client doit alors continuer en silence : `Open`
// rend l'erreur, et l'appelant décide. Toutes les autres méthodes ne font rien
// sur un mélangeur fermé, plutôt que d'obliger chaque site d'appel à vérifier.
class AudioEngine {
 public:
  AudioEngine() = default;
  AudioEngine(const AudioEngine&) = delete;
  AudioEngine& operator=(const AudioEngine&) = delete;
  AudioEngine(AudioEngine&& other) noexcept;
  AudioEngine& operator=(AudioEngine&& other) noexcept;
  ~AudioEngine();

  [[nodiscard]] static core::Result<AudioEngine> Open();

  // UN MÉLANGEUR SANS PÉRIPHÉRIQUE, que l'on fait tourner soi-même par
  // `Render` : la même chaîne que celui de `Open`, à `rate_hz` trames par
  // seconde sur `channels` voies, mais dont la sortie se lit au lieu de
  // s'entendre. C'est ce qui met le mélange sous essai, sans carte son.
  [[nodiscard]] static core::Result<AudioEngine> OpenOffline(int rate_hz, int channels);

  // Remplit `pcm` — des flottants entrelacés — de la suite du mélange d'un
  // mélangeur SANS PÉRIPHÉRIQUE. Faux sur tout autre, ou en cas d'échec.
  [[nodiscard]] bool Render(std::span<float> pcm);

  [[nodiscard]] bool is_open() const noexcept { return mixer_ != nullptr; }

  // LE SON EST-IL AUDIBLE ? Coupé, le mélangeur reste OUVERT mais ne joue rien.
  //
  // ⚠ C'est ici que la coupure se garantit, et non chez l'appelant : le même
  // principe que le mélangeur fermé, dont toutes les méthodes ne font rien
  // plutôt que d'obliger chaque site d'appel à vérifier. Un appelant qui
  // oublierait la garde ne peut pas faire de bruit.
  //
  // La MUSIQUE et les BRUITS se coupent séparément, comme dans le jeu : on
  // joue souvent sans musique et avec les bruits, rarement l'inverse.
  void SetMusicAudible(bool audible);
  void SetSoundAudible(bool audible);
  [[nodiscard]] bool music_audible() const noexcept { return music_audible_; }
  [[nodiscard]] bool sound_audible() const noexcept { return sound_audible_; }

  // Ce que le mélangeur sait décoder, dans son ordre à lui.
  //
  // ⚠ La liste dépend de la CONSTRUCTION de la bibliothèque, pas de son
  // interface : sans le bon décodeur compilé dedans, un MP3 est refusé comme un
  // fichier abîmé, et rien ne dit que c'est la construction qui manque. La
  // dire au démarrage évite de chercher ailleurs.
  [[nodiscard]] std::vector<std::string> Decoders() const;

  // Joue une MUSIQUE, en boucle, à la place de celle qui jouait.
  //
  // `bytes` N'EST LU QUE PENDANT L'APPEL : le mélangeur en tire sa propre copie,
  // qu'il décode au fil de la lecture, et referme le flux avant de rendre la
  // main. L'appelant peut libérer les siens au retour.
  //
  // `name` ne sert qu'au journal et à ÉVITER DE RELANCER : deux cartes voisines
  // partagent souvent leur morceau, et le redémarrer à chaque porte franchie
  // s'entendrait.
  [[nodiscard]] core::Result<void> PlayMusic(std::string name, std::span<const std::uint8_t> bytes);

  // Coupe la musique. Sans effet s'il n'y en avait pas.
  void StopMusic();

  [[nodiscard]] const std::string& music_name() const noexcept { return music_name_; }

  // Le volume de la musique, de zéro à un.
  void SetMusicVolume(float volume);

  // LE VOLUME DES BRUITS, de zéro à un. Il MULTIPLIE celui que chaque bruit
  // demande — un pas lointain reste plus discret qu'un coup porté à côté.
  //
  // 🔴 LES BRUITS EN COURS SUIVENT AUSSITÔT : chaque voix retient ce que son
  // bruit demandait et sa famille, et le curseur recalcule le gain de celles
  // qui jouent encore. Un son d'ambiance de trois secondes ne garde donc pas
  // l'ancien volume jusqu'à sa fin.
  void SetSoundVolume(float volume);

  // D'OÙ VIENT UN BRUIT S'ENTEND-IL À GAUCHE OU À DROITE ?
  //
  // 🔴 LE CLIENT D'ORIGINE NE LE DIT PAS, et c'est mesuré : son chemin de son
  // ordinaire passe par `AIL_set_sample_volume`, qui est MONO. Il atténue par
  // la DISTANCE — loi inverse, avec une coupure au-delà d'une portée — et
  // s'arrête là. Son `setup.exe` le nomme d'ailleurs : « Mode Sonore » propose
  // « Aucun Son », « Son 2D » et « Son 3D », et la 3D demande un fournisseur
  // qui ne s'ouvre pas toujours.
  //
  // ⚠ ÉTEINDRE NE REND PAS LE SON PLAT : l'atténuation par la distance ne
  // passe pas par ici — elle est déjà dans le gain que l'appelant calcule.
  // Seul le CÔTÉ disparaît, ce qui est exactement le mode 2D du natif.
  void SetSpatialSound(bool spatial);
  [[nodiscard]] float sound_volume() const noexcept { return sound_volume_; }

  // LE VOLUME D'UNE FAMILLE DE BRUITS, de zéro à un, SOUS le volume général :
  // il multiplie celui-ci, il ne le remplace pas. Comme lui, il s'applique aussi
  // aux bruits en cours de cette famille. Voir `SoundChannel`.
  void SetChannelVolume(SoundChannel channel, float volume);
  [[nodiscard]] float channel_volume(SoundChannel channel) const noexcept {
    return channel_volumes_[IndexOf(channel)];
  }
  [[nodiscard]] float music_volume() const noexcept { return music_volume_; }

  // Retient un BRUIT sous un nom, pour le jouer ensuite.
  //
  // Un bruit est court et se rejoue souvent — un coup d'épée, un pas. Il est
  // donc décodé ENTIÈREMENT à la première fois, contrairement à la musique :
  // décoder à chaque coup porté ferait un à-coup par coup.
  //
  // Redonner un nom déjà connu ne fait rien : c'est ce qui permet à l'appelant
  // de l'offrir à chaque fois sans tenir de liste.
  [[nodiscard]] core::Result<void> LoadSound(const std::string& name,
                                             std::span<const std::uint8_t> bytes);

  [[nodiscard]] bool KnowsSound(const std::string& name) const;

  // OUBLIE UN BRUIT RETENU : son nom redevient inconnu, et le décodé se libère.
  // Sans effet sur un nom inconnu.
  //
  // 🔴 SÛR PENDANT QU'UNE VOIX LE JOUE : chaque voix tient sa propre référence
  // au décodé, et le décodé n'est libéré qu'avec la dernière. Une voix au repos
  // rend la sienne aussitôt ; une voix qui le joue l'achève, puis la garde —
  // une piste qui finit ne rend pas son décodé, elle le rend quand on lui en
  // donne un autre. C'est pourquoi `PlaySound` détache d'abord les voix au
  // repos dont le bruit a été oublié : sans cela, la première voix libre
  // servant toujours, une voix plus loin le garderait jusqu'à la fermeture.
  // Et plus aucune voix ne le tient pour le sien : un bruit chargé ensuite,
  // sous ce nom ou à cette adresse, ne s'y fond jamais.
  void ForgetSound(const std::string& name);

  // Les noms des bruits retenus, dans l'ordre de leur nom.
  [[nodiscard]] std::vector<std::string> HeldSoundNames() const;

  // Combien de voix tiennent encore le décodé d'un bruit oublié : celles qui
  // l'achèvent, et celles au repos que `PlaySound` n'a pas encore détachées.
  [[nodiscard]] std::size_t VoicesHoldingForgottenSounds() const;

  // Joue un bruit retenu, à un volume et d'un côté, sous le volume de sa
  // famille. Sans effet si le nom est inconnu ou si le volume est nul — un son
  // qu'on n'entendrait pas ne prend pas une voix.
  //
  // 🔴 LE MÊME BRUIT, RELANCÉ DANS LA MÊME FENÊTRE ET LA MÊME FAMILLE, NE PREND
  // PAS DE VOIX DE PLUS : il se fond dans celle qui vient de partir, dont le
  // gain devient la somme des énergies. Voir `MergeOnset` — c'est ce qui garde
  // les vingt-cinq cases d'une unité au sol de sonner vingt-cinq fois trop
  // fort. Un écart voulu au jeu d'origine, qui les additionne et sature.
  //
  // ⚠ LA FAMILLE N'A PAS DE DÉFAUT : chaque site d'appel dit laquelle il joue.
  // Un défaut rangerait en silence le prochain chemin ajouté dans la mauvaise.
  void PlaySound(const std::string& name, float gain, float pan, SoundChannel channel);

  // FAIT TAIRE LES BRUITS EN COURS D'UNE FAMILLE, par un fondu de
  // `kSilencingFadeMs`. Les autres familles continuent.
  //
  // Un bruit s'arrête d'ordinaire à la fin de son fichier. Celui-ci sert quand
  // ce qui l'a lancé disparaît avant lui : les sources d'une carte lancent des
  // bruits qui durent plus longtemps que le voyage vers la suivante.
  void StopSounds(SoundChannel channel);

 private:
  void Close() noexcept;

  // Types opaques de SDL3_mixer : les déclarer ici ferait entrer son en-tête
  // dans tout ce qui inclut celui-ci.
  struct Mixer;
  struct Track;
  struct Sound;

  // UNE VOIX, et ce que son bruit demandait : le gain du site d'appel, avant
  // tout réglage du joueur, et sa famille. C'est de quoi recalculer son gain
  // quand un curseur bouge pendant qu'elle joue.
  struct Voice {
    Track* track = nullptr;
    float asked_gain = 0.0F;
    SoundChannel channel = SoundChannel::kEffects;
    // Le côté demandé, de moins un à un : une fusion le déplace.
    float asked_pan = 0.0F;
    // Le bruit qu'elle joue, et l'instant de son départ en millisecondes de
    // l'horloge de SDL : de quoi reconnaître un second départ du même bruit.
    const Sound* sound = nullptr;
    std::uint64_t started_ms = 0;
    // Sa piste tient le décodé d'un bruit oublié : voir `ForgetSound`.
    bool holds_forgotten = false;
  };

  // Un bruit retenu, et s'il faut l'ouvrir en fondu : voir `OpeningStep`.
  struct LoadedSound {
    Sound* audio = nullptr;
    bool opens_on_step = false;
  };

  // Commun aux deux ouvertures : la réserve du mélange et le limiteur qui la
  // rattrape en bout de chaîne. Voir `kMixHeadroom`.
  [[nodiscard]] static core::Result<AudioEngine> Adopt(Mixer* mixer, bool owns_device);

  // Pose sur la voix le côté qu'elle demande, ou le centre sans son spatial.
  void ApplySide(const Voice& voice) const;

  // Une voix libre pour un bruit, ou rien si toutes sont prises. Les bruits se
  // partagent un nombre FIXE de voix : sans plafond, une mêlée en ouvrirait
  // autant qu'il y a de coups, et le son se brouillerait avant de saturer.
  [[nodiscard]] Voice* FreeVoice();

  // Détache les voix au repos qui tiennent le décodé d'un bruit oublié : leur
  // piste le garderait jusqu'au prochain bruit qu'on lui confie.
  void ReleaseForgottenVoices();

  // Le gain qu'une voix doit porter selon les réglages du moment : nul quand
  // les bruits sont coupés.
  [[nodiscard]] float GainOf(const Voice& voice) const;

  // Repose le gain des voix qui jouent encore, après un changement de réglage.
  void RemixPlayingVoices();

  Mixer* mixer_ = nullptr;
  Track* music_track_ = nullptr;
  Sound* music_sound_ = nullptr;
  std::string music_name_;
  float music_volume_ = kFullVolume;
  float sound_volume_ = kFullVolume;
  SoundChannelVolumes channel_volumes_ = kFullSoundChannelVolumes;
  // ⚠ AUDIBLES PAR DÉFAUT : une bibliothèque de son qui part muette
  // surprendrait qui la construit. C'est au client de décider, et il le fait en
  // un seul endroit — voir `GameOptions`, qui pose ce que le joueur a choisi.
  bool music_audible_ = true;
  bool sound_audible_ = true;
  // ⚠ UN AJOUT AU NATIF, ACTIF PAR DÉFAUT : c'est lui qui rend un monde
  // audible. Qui veut la conduite du client d'origine le coupe — voir le réglage
  // ci-dessus.
  bool spatial_sound_ = true;

  // Les bruits retenus, déjà décodés.
  std::map<std::string, LoadedSound> sounds_;
  std::vector<Voice> voices_;

  // Vrai quand ce mélangeur a ouvert le sous-système audio de SDL, et doit donc
  // le refermer ; faux pour celui de `OpenOffline`.
  bool owns_device_ = false;
  // ⚠ SUR LE TAS, et c'est nécessaire : le fil audio de SDL le reçoit par
  // adresse, et le mélangeur se rend par valeur. Un limiteur logé dans l'objet
  // changerait d'adresse au premier déplacement, sous le fil qui l'emploie.
  std::unique_ptr<PeakLimiter> limiter_;
};

}  // namespace ro::audio
