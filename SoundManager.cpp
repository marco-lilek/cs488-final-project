#include "SoundManager.hpp"
#include <cstring>
#include <audio/wave.h>
#include <thread>
#include <mutex>

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

using namespace std;

static const size_t NUM_BG_BUFFERS = 16;
static const size_t DATA_CHUNK_SIZE = 4096;

static string assetFilePath = "./Assets/";

static std::mutex sourceLock;

std::string printError(ALenum error) {
    if(error == AL_INVALID_NAME)
    {
        return "Invalid name";
    }
    else if(error == AL_INVALID_ENUM)
    {
        return " Invalid enum ";
    }
    else if(error == AL_INVALID_VALUE)
    {
        return " Invalid value ";
    }
    else if(error == AL_INVALID_OPERATION)
    {
        return " Invalid operation ";
    }
    else if(error == AL_OUT_OF_MEMORY)
    {
        return " Out of memory like! ";
    }

    return " Don't know ";
}

SoundManager::SoundManager(uint8_t numSources) {
  m_soundBufferMap["test1.wav"] = 0;

  m_soundNameMap["bazinga"] = "test1.wav";

  m_bgSoundNameMap["bg1"] = "test1.ogg";
}

SoundManager::~SoundManager() {
  delete[] m_buffers;
}

static void list_audio_devices(const ALCchar *devices)
{
        const ALCchar *device = devices, *next = devices + 1;
        size_t len = 0;

        fprintf(stdout, "Devices list:\n");
        fprintf(stdout, "----------\n");
        while (device && *device != '\0' && next && *next != '\0') {
                fprintf(stdout, "%s\n", device);
                len = strlen(device);
                device += (len + 1);
                next += (len + 2);
        }
        fprintf(stdout, "----------\n");
}

void SoundManager::init() {
  //list_audio_devices(alcGetString(NULL, ALC_DEVICE_SPECIFIER));
  initContext();
  loadSounds();
}

static ALenum getFormat(int numChannels, int bitDepth) {
  bool stereo = numChannels > 1;
  switch (bitDepth) {
    case 16:
      return stereo ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
    case 8:
      return stereo ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
    default:
      assert(0);
  }
}

void setupSource(ALuint sourceId) {
  alSourcef(sourceId, AL_PITCH, 1);
  alSourcef(sourceId, AL_GAIN, 1);
  alSource3f(sourceId, AL_POSITION, 0, 0, 0);
  alSource3f(sourceId, AL_VELOCITY, 0, 0, 0);
  alSourcei(sourceId, AL_LOOPING, AL_FALSE);
  CHECK_SOUND_ERRORS;
}

void SoundManager::initContext() {
  m_device = alcOpenDevice(alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER));
  m_context = alcCreateContext(m_device, nullptr);
  assert(alcMakeContextCurrent(m_context));
  CHECK_SOUND_ERRORS;

  ALfloat listenerOri[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };

  alListener3f(AL_POSITION, 0, 0, 1.0f);
  // check for errors
  alListener3f(AL_VELOCITY, 0, 0, 0);
  // check for errors
  alListenerfv(AL_ORIENTATION, listenerOri);
  // check for errors
}
void SoundManager::loadSounds() {
  m_buffers = new ALuint[m_soundBufferMap.size()];
  alGenBuffers(m_soundBufferMap.size(), m_buffers);
  int i = 0;
  for (map<string, ALuint>::iterator it = m_soundBufferMap.begin(); it != m_soundBufferMap.end(); it++, i++) {
    auto elm = *it;
    WaveInfo *wave = WaveOpenFileForReading((assetFilePath + elm.first).c_str());
    assert(wave);
    assert(WaveSeekFile(0, wave) != -1);
    char *bufferData = (char*)malloc(wave->dataSize);
    assert(bufferData);
    assert(WaveReadFile(bufferData, wave->dataSize, wave) != -1);
    alBufferData(m_buffers[0], getFormat(wave->channels, wave->bitsPerSample), bufferData, wave->dataSize, wave->sampleRate);
    CHECK_SOUND_ERRORS;

    m_soundBufferMap[elm.first] = m_buffers[i];
  }
}

/*void playSoundThread(ALuint *sources, mutex *sourceLocks, int sourceId, ALuint buffer, string s) {
  sourceLocks[sourceId].lock();
  alSourcei(sources[sourceId], AL_BUFFER, buffer);
  alSourcePlay(sources[sourceId]);
  while (true) {
    ALint state;
    alGetSourcei(sources[sourceId], AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) break;
  }
  cerr << "played " << s << endl;
  sourceLocks[sourceId].unlock();
}*/

void playSoundNewSourceThread(ALuint source, ALuint buffer, string s) {
  setupSource(source);
  alSourcei(source, AL_BUFFER, buffer);
  alSourcePlay(source);
  while (true) {
    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) break;
  }

  sourceLock.lock();
  alDeleteSources(1, &source);
  sourceLock.unlock();

  CHECK_SOUND_ERRORS;
  cerr << "played " << s << endl;
}

/*void SoundManager::playSound(const string &s, Source source) {
  assert(source < NUMSOURCES);
  assert(m_soundNameMap.count(s) > 0);
  thread t(playSoundThread, m_sources, m_sourceLocks, source, m_soundBufferMap[m_soundNameMap[s]], s); 
  t.detach();

  CHECK_SOUND_ERRORS;
}*/

ALuint SoundManager::playSound(const string &s) {
  ALuint source;
  alGenSources(1, &source);
  assert(m_soundNameMap.count(s) > 0);
  thread t(playSoundNewSourceThread, source, m_soundBufferMap[m_soundNameMap[s]], s); 
  t.detach();

  CHECK_SOUND_ERRORS;
  return source;
}

void SoundManager::stopSound(ALuint source) {
  sourceLock.lock();
  if (alIsSource(source))
    alSourceStop(source);
  sourceLock.unlock();
}

static void playBackgroundTh(ALuint source, string fname) {
  setupSource(source);
  ALuint *buffers = new ALuint[NUM_BG_BUFFERS];
  alGenBuffers(NUM_BG_BUFFERS, buffers);

  OggVorbis_File vf;
  int current_section;
  int eof = 0;
  FILE *fp = fopen(fname.c_str(), "rb");
  assert(fp != NULL);
  assert(ov_open_callbacks(fp, &vf, NULL, 0, OV_CALLBACKS_DEFAULT)==0);

  vorbis_info *vi = ov_info(&vf, -1);

  char *dataChunk = new char[DATA_CHUNK_SIZE];
  int bitstream;
  for (int i = 0; i < NUM_BG_BUFFERS; i++) {
    long ret = ov_read(&vf, dataChunk, DATA_CHUNK_SIZE, 0, 2, 1, &bitstream);
    if (ret == 0) break;
    alBufferData(buffers[i], AL_FORMAT_STEREO16, dataChunk, ret, vi->rate);
  }

  alSourceQueueBuffers(source, NUM_BG_BUFFERS, buffers);
  CHECK_SOUND_ERRORS;
  alSourcePlay(source);
  CHECK_SOUND_ERRORS;

  int iBuffersProcessed = 0;
  ALuint uqBufferId = 0;
  while (true) {
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &iBuffersProcessed);
    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);

    if (iBuffersProcessed == 0 && state != AL_PLAYING) break;
    while (iBuffersProcessed) {
      alSourceUnqueueBuffers(source, 1, &uqBufferId);
      iBuffersProcessed--;

      long ret = ov_read(&vf, dataChunk, DATA_CHUNK_SIZE, 0, 2, 1, &bitstream);
      if (ret == 0) continue;
      alBufferData(uqBufferId, AL_FORMAT_STEREO16, dataChunk, ret, vi->rate);
      alSourceQueueBuffers(source, 1, &uqBufferId);
    }
  }

  CHECK_SOUND_ERRORS;
  
  sourceLock.lock();
  cerr << "played BG " << fname << endl;
  delete[] dataChunk;
  fclose(fp);
  alDeleteSources(1, &source);
  alDeleteBuffers(NUM_BG_BUFFERS, buffers);
  delete[] buffers;
  sourceLock.unlock();
}

ALuint SoundManager::playBackground(const string &s) {
  ALuint source;
  alGenSources(1, &source);
  assert(m_bgSoundNameMap.count(s) > 0);
  thread t(playBackgroundTh, source, assetFilePath + m_bgSoundNameMap[s]);
  t.detach();

  CHECK_SOUND_ERRORS;
  return source;
}
