#pragma once

#include <AL/al.h>
#include <AL/alc.h>
#include <cassert>
#include <string>
#include <map>
#include <iostream>
#include <mutex>
#include <vector>
#include <set>

#define CHECK_SOUND_ERRORS {if (alGetError() != AL_NO_ERROR) {std::cerr << "sound error: " << printError(alGetError()) << endl; assert(0);}}

std::string printError(ALenum error);

typedef int BGSoundId;

struct SoundManager {

  SoundManager(uint8_t numSources);
  ~SoundManager();
  void init();
  void initContext();
  void loadSounds();
  ALuint playSound(const std::string &s);
  void stopSound(ALuint source);

  BGSoundId playBackground(const std::string &s);
  void stopBGSound(BGSoundId id);

private:
  std::map<std::string, ALuint> m_soundBufferMap;
  std::map<std::string, std::string> m_soundNameMap;
  std::map<std::string, std::string> m_bgSoundNameMap;
  std::map<BGSoundId, ALuint> m_bgSoundIdSourceId;
  std::set<BGSoundId> m_stoppedSounds;

  ALCdevice *m_device;
  ALCcontext *m_context;
  
  ALuint *m_buffers;
};
