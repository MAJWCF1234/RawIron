#include "RawIron/Audio/AmbientLoopBank.h"

#include <algorithm>

namespace ri::audio {

void AmbientLoopBank::Sync(AudioManager& manager,
                           const std::filesystem::path& contentRoot,
                           const std::vector<AmbientLoopIntent>& intents,
                           const std::size_t voiceLimit) {
    std::unordered_map<std::string, bool> keepIds{};
    const std::size_t limit = std::max<std::size_t>(0U, voiceLimit);
    for (std::size_t index = 0; index < intents.size() && index < limit; ++index) {
        const AmbientLoopIntent& intent = intents[index];
        if (intent.id.empty() || intent.audioPath.empty()) {
            continue;
        }
        keepIds[intent.id] = true;
        auto found = loops_.find(intent.id);
        if (found == loops_.end() || found->second == nullptr) {
            const std::filesystem::path clipPath = contentRoot / intent.audioPath;
            std::shared_ptr<ManagedSound> loop =
                manager.CreateLoopingSound(clipPath.string(), intent.desiredVolume);
            if (loop != nullptr) {
                loop->Play();
            }
            loops_[intent.id] = std::move(loop);
            found = loops_.find(intent.id);
        }
        if (found != loops_.end() && found->second != nullptr) {
            found->second->SetVolume(intent.desiredVolume);
            found->second->SetPlaybackRate(1.0 + (static_cast<double>(intent.normalizedFalloff) * 0.03));
        }
    }
    for (auto it = loops_.begin(); it != loops_.end();) {
        if (keepIds.contains(it->first)) {
            ++it;
            continue;
        }
        if (it->second != nullptr) {
            manager.StopManagedSound(it->second, true);
        }
        it = loops_.erase(it);
    }
}

void AmbientLoopBank::Clear(AudioManager& manager) {
    for (auto& [id, sound] : loops_) {
        (void)id;
        if (sound != nullptr) {
            manager.StopManagedSound(sound, true);
        }
    }
    loops_.clear();
}

} // namespace ri::audio
