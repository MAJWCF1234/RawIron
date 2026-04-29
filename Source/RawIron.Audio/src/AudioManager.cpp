#include "RawIron/Audio/AudioEnvironment.h"
#include "RawIron/Audio/AudioManager.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ri::audio {

namespace {
constexpr std::size_t kMaxPendingEchoes = 256U;
}

ManagedSound::ManagedSound(SoundId soundId,
                           std::shared_ptr<AudioPlaybackHandle> playback,
                           std::string path,
                           bool loop,
                           AudioManager* manager)
    : soundId_(soundId),
      playback_(std::move(playback)),
      path_(std::move(path)),
      loop_(loop),
      manager_(manager) {}

void ManagedSound::Play() {
    if (playback_ && !unloaded_) {
        playback_->Play();
    }
}

void ManagedSound::Pause() {
    if (playback_ && !unloaded_) {
        playback_->Pause();
    }
}

void ManagedSound::Stop() {
    if (playback_ && !unloaded_) {
        playback_->Stop();
    }
}

void ManagedSound::Unload() {
    if (unloaded_) {
        return;
    }

    const std::shared_ptr<ManagedSound> keepAlive = shared_from_this();
    unloaded_ = true;

    if (playback_) {
        playback_->SetFinishedCallback({});
        playback_->Stop();
        playback_->Unload();
    }

    if (manager_ != nullptr) {
        manager_->UnregisterSound(soundId_);
    }
}

double ManagedSound::GetCurrentTime() const {
    return playback_ ? std::max(0.0, playback_->GetCurrentTime()) : 0.0;
}

void ManagedSound::SetCurrentTime(double value) {
    if (playback_ && !unloaded_) {
        playback_->SetCurrentTime(std::max(0.0, std::isfinite(value) ? value : 0.0));
    }
}

double ManagedSound::GetVolume() const {
    return playback_ ? playback_->GetVolume() : 0.0;
}

void ManagedSound::SetVolume(double value) {
    if (playback_ && !unloaded_) {
        playback_->SetVolume(ClampAudioVolume(value));
    }
}

double ManagedSound::GetPlaybackRate() const {
    return playback_ ? playback_->GetPlaybackRate() : 1.0;
}

void ManagedSound::SetPlaybackRate(double value) {
    if (playback_ && !unloaded_) {
        playback_->SetPlaybackRate(ClampAudioPlaybackRate(value));
    }
}

const std::string& ManagedSound::GetPath() const {
    return path_;
}

double ManagedSound::GetDuration() const {
    return playback_ ? std::max(0.0, playback_->GetDuration()) : 0.0;
}

bool ManagedSound::IsPlaying() const {
    return playback_ && !unloaded_ && playback_->IsPlaying();
}

bool ManagedSound::IsLooping() const {
    return loop_;
}

ManagedSound::SoundId ManagedSound::GetId() const {
    return soundId_;
}

void ManagedSound::ReleaseOnPlaybackFinished() {
    if (unloaded_) {
        return;
    }

    unloaded_ = true;
    if (playback_) {
        playback_->SetFinishedCallback({});
        playback_->Unload();
    }
}

AudioManager::AudioManager(std::shared_ptr<AudioBackend> backend, AudioManagerOptions options)
    : backend_(std::move(backend)),
      muted_(options.muted) {}

void AudioManager::SetMuted(bool muted) {
    muted_ = muted;
}

bool AudioManager::IsMuted() const {
    return muted_;
}

void AudioManager::SetMasterLinearGain(double gain) {
    if (!std::isfinite(gain)) {
        masterLinearGain_ = 1.0;
        return;
    }
    masterLinearGain_ = std::max(0.0, std::min(1.0, gain));
}

double AudioManager::GetMasterLinearGain() const noexcept {
    return masterLinearGain_;
}

std::optional<AudioEnvironmentProfile> AudioManager::NormalizeEnvironmentProfile(
    const std::optional<AudioEnvironmentProfileInput>& profile) const {
    if (!profile.has_value()) {
        return std::nullopt;
    }
    return NormalizeAudioEnvironmentProfile(*profile);
}

bool AudioManager::SetEnvironmentProfile(const std::optional<AudioEnvironmentProfileInput>& profile) {
    const std::optional<AudioEnvironmentProfile> normalized = NormalizeEnvironmentProfile(profile);
    const std::string signature = BuildAudioEnvironmentSignature(normalized);
    if (signature == environmentSignature_) {
        return false;
    }

    environmentProfile_ = normalized;
    environmentSignature_ = signature;
    environmentChanges_ += 1;
    return true;
}

std::optional<AudioEnvironmentProfile> AudioManager::GetEnvironmentProfile() const {
    return environmentProfile_;
}

AudioResolvedPlayback AudioManager::ApplyEnvironmentToPlayback(const AudioPlaybackRequest& request) const {
    return MixPlaybackWithEnvironment(request, environmentProfile_);
}

std::shared_ptr<ManagedSound> AudioManager::CreateManagedSound(std::string_view filePath,
                                                               double volume,
                                                               bool loop,
                                                               double playbackRate) {
    return CreateManagedSoundInternal(filePath, volume, loop, playbackRate, false, false, false, loop, false, false);
}

std::shared_ptr<ManagedSound> AudioManager::CreateLoopingSound(std::string_view filePath, double volume) {
    return CreateManagedSoundInternal(filePath, volume, true, 1.0, false, false, false, true, false, false);
}

std::shared_ptr<ManagedSound> AudioManager::PlayOneShot(std::string_view filePath, double volume) {
    if (muted_) {
        return nullptr;
    }

    std::shared_ptr<ManagedSound> sound = CreateManagedSoundInternal(
        filePath, volume, false, 1.0, true, true, false, false, true, false);
    if (sound != nullptr) {
        ScheduleEcho(filePath, sound->GetVolume(), sound->GetPlaybackRate(), environmentProfile_);
    }
    return sound;
}

std::shared_ptr<ManagedSound> AudioManager::PlayVoice(std::string_view filePath, double volume) {
    if (activeVoiceId_.has_value()) {
        const auto activeIt = managedSounds_.find(*activeVoiceId_);
        if (activeIt != managedSounds_.end()) {
            activeIt->second->Stop();
            activeIt->second->Unload();
        }
        activeVoiceId_.reset();
    }

    if (muted_) {
        return nullptr;
    }

    std::shared_ptr<ManagedSound> voice = CreateManagedSoundInternal(
        filePath, volume, false, 1.0, true, true, true, false, false, true);
    if (voice != nullptr) {
        ScheduleEcho(filePath, voice->GetVolume(), voice->GetPlaybackRate(), environmentProfile_);
    }
    return voice;
}

void AudioManager::StopManagedSound(const std::shared_ptr<ManagedSound>& sound, bool unload) {
    if (sound == nullptr) {
        return;
    }

    sound->Stop();
    if (unload) {
        sound->Unload();
    }
    if (activeVoiceId_.has_value() && *activeVoiceId_ == sound->GetId()) {
        activeVoiceId_.reset();
    }
}

void AudioManager::Tick(double deltaMs) {
    if (backend_ != nullptr) {
        backend_->PumpHostThreadAudioWork();
    }

    if (!std::isfinite(currentTimeMs_) || currentTimeMs_ < 0.0) {
        currentTimeMs_ = 0.0;
    }
    if (std::isfinite(deltaMs) && deltaMs > 0.0) {
        currentTimeMs_ += deltaMs;
    }

    if (pendingEchoes_.empty()) {
        return;
    }

    std::vector<PendingEcho> remaining;
    remaining.reserve(pendingEchoes_.size());

    for (const PendingEcho& echo : pendingEchoes_) {
        if (echo.fireAtMs > currentTimeMs_) {
            remaining.push_back(echo);
            continue;
        }
        if (muted_) {
            continue;
        }

        (void)CreateManagedSoundInternal(
            echo.filePath, echo.volume, false, echo.playbackRate, true, true, false, false, false, false);
    }

    pendingEchoes_ = std::move(remaining);
}

AudioManagerMetrics AudioManager::GetMetrics() const {
    AudioManagerMetrics metrics{};
    metrics.managedSounds = managedSounds_.size();
    metrics.loopsCreated = loopsCreated_;
    metrics.oneShotsPlayed = oneShotsPlayed_;
    metrics.voicesPlayed = voicesPlayed_;
    metrics.voiceActive = activeVoiceId_.has_value() && managedSounds_.contains(*activeVoiceId_);
    metrics.environmentChanges = environmentChanges_;
    metrics.activeEnvironment = environmentProfile_.has_value() ? environmentProfile_->label : "none";
    metrics.activeEnvironmentMix = environmentProfile_.has_value() ? environmentProfile_->reverbMix : 0.0;
    metrics.pendingEchoes = pendingEchoes_.size();
    metrics.droppedEchoes = droppedEchoes_;
    return metrics;
}

std::size_t AudioManager::PendingEchoCount() const {
    return pendingEchoes_.size();
}

std::shared_ptr<ManagedSound> AudioManager::CreateManagedSoundInternal(std::string_view filePath,
                                                                       double volume,
                                                                       bool loop,
                                                                       double playbackRate,
                                                                       bool autoPlay,
                                                                       bool autoReleaseOnFinish,
                                                                       bool trackAsVoice,
                                                                       bool countLoopMetric,
                                                                       bool countOneShotMetric,
                                                                       bool countVoiceMetric) {
    if (!backend_ || filePath.empty()) {
        return nullptr;
    }

    const AudioResolvedPlayback playback = MixPlaybackWithEnvironment(
        AudioPlaybackRequest{.volume = volume, .playbackRate = playbackRate},
        environmentProfile_);

    const AudioClipRequest request{
        .path = std::string(filePath),
        .loop = loop,
        .volume = ClampAudioVolume(playback.volume * masterLinearGain_, 0.0),
        .playbackRate = playback.playbackRate,
        .pan = playback.pan,
        .spatialized = playback.distanceMeters > 0.001 || playback.occlusion > 0.001,
        .positionX = static_cast<float>(playback.distanceMeters),
        .positionY = 0.0f,
        .positionZ = 0.0f,
        .minDistance = 1.0,
        .maxDistance = 64.0,
    };

    std::shared_ptr<AudioPlaybackHandle> handle = backend_->CreatePlayback(request);
    if (handle == nullptr) {
        return nullptr;
    }

    const ManagedSound::SoundId soundId = nextSoundId_++;
    std::shared_ptr<ManagedSound> sound(new ManagedSound(soundId, handle, request.path, loop, this));
    managedSounds_[soundId] = sound;

    if (trackAsVoice) {
        activeVoiceId_ = soundId;
    }
    if (countLoopMetric) {
        loopsCreated_ += 1;
    }
    if (countOneShotMetric) {
        oneShotsPlayed_ += 1;
    }
    if (countVoiceMetric) {
        voicesPlayed_ += 1;
    }

    if (autoReleaseOnFinish) {
        handle->SetFinishedCallback([this, soundId, trackAsVoice]() {
            HandlePlaybackFinished(soundId, trackAsVoice);
        });
    }

    if (autoPlay) {
        sound->Play();
    }
    return sound;
}

void AudioManager::ScheduleEcho(std::string_view filePath,
                                double volume,
                                double playbackRate,
                                const std::optional<AudioEnvironmentProfile>& profile) {
    if (muted_ || !profile.has_value()) {
        return;
    }
    const std::optional<AudioEchoLayer> layer = TryResolveAudioEchoLayer(volume, playbackRate, *profile);
    if (!layer.has_value()) {
        return;
    }

    pendingEchoes_.push_back(PendingEcho{
        .filePath = std::string(filePath),
        .volume = layer->volume,
        .playbackRate = layer->playbackRate,
        .fireAtMs = currentTimeMs_ + profile->echoDelayMs,
    });
    std::sort(pendingEchoes_.begin(), pendingEchoes_.end(), [](const PendingEcho& lhs, const PendingEcho& rhs) {
        return lhs.fireAtMs < rhs.fireAtMs;
    });
    if (pendingEchoes_.size() > kMaxPendingEchoes) {
        const std::size_t dropCount = pendingEchoes_.size() - kMaxPendingEchoes;
        pendingEchoes_.erase(pendingEchoes_.begin(), pendingEchoes_.begin() + dropCount);
        droppedEchoes_ += dropCount;
    }
}

void AudioManager::UnregisterSound(ManagedSound::SoundId soundId) {
    managedSounds_.erase(soundId);
    if (activeVoiceId_.has_value() && *activeVoiceId_ == soundId) {
        activeVoiceId_.reset();
    }
}

void AudioManager::HandlePlaybackFinished(ManagedSound::SoundId soundId, bool trackAsVoice) {
    const auto found = managedSounds_.find(soundId);
    if (found == managedSounds_.end()) {
        return;
    }

    found->second->ReleaseOnPlaybackFinished();
    managedSounds_.erase(found);

    if (trackAsVoice && activeVoiceId_.has_value() && *activeVoiceId_ == soundId) {
        activeVoiceId_.reset();
    }
}

} // namespace ri::audio
