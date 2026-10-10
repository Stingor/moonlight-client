#include "ro/audio/sound_channel.h"

namespace ro::audio {

float VolumeAmplitude(float ratio) { return ratio * ratio * ratio; }

float MixedSoundGain(float asked, float master_volume, float channel_volume) {
  return asked * VolumeAmplitude(master_volume) * VolumeAmplitude(channel_volume);
}

}  // namespace ro::audio
