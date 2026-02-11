#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"
#include "RuntimeAudioPlayer.generated.h"

/**
 * A simple runtime audio player that can import and play WAV files.
 */
UCLASS(Blueprintable)
class TEST_API ARuntimeAudioPlayer : public AActor
{
    GENERATED_BODY()
    
public:
    ARuntimeAudioPlayer();

    /** Audio component used to play runtime-loaded audio */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
    UAudioComponent* AudioComponent;

    /** Load a WAV file from disk and play it immediately */
    UFUNCTION(BlueprintCallable, Category = "Audio|Runtime")
    bool PlayWavFromFile(const FString& FilePath);

protected:
    /** Runtime-created SoundWave */
    UPROPERTY()
    USoundWave* RuntimeSoundWave;

private:
    /** Parse a WAV file and return PCM data, sample rate, and channel count */
    bool ParseWavFile(const TArray<uint8>& RawFileData, TArray<uint8>& OutPCMData, int32& OutSampleRate, int32& OutNumChannels);

    /** Create a USoundWave from raw PCM data */
    USoundWave* CreateSoundWave(const TArray<uint8>& PCMData, int32 SampleRate, int32 NumChannels);
};
