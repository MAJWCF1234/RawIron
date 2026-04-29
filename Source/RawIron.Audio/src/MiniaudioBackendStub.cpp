#include "RawIron/Audio/AudioBackendMiniaudio.h"

namespace ri::audio {

std::shared_ptr<AudioBackend> CreateMiniaudioAudioBackend(std::string* errorMessage,
                                                          const MiniaudioBackendSettings* /*settings*/) {
    if (errorMessage != nullptr) {
        errorMessage->assign("RAWIRON_AUDIO_MINIAUDIO=OFF at configure time");
    }
    return nullptr;
}

} // namespace ri::audio
