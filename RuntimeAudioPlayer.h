#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "RuntimeAudioPlayer.generated.h"

/**
 * A runtime audio player that loads WAV files from disk and plays them
 * using USoundWaveProcedural (no precaching, no asset import needed).
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

    /** Editable path so you can set it per-instance in the Details panel */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Runtime")
    FString AudioFilePath;

protected:
    virtual void BeginPlay() override;

    /** Runtime procedural sound wave — UPROPERTY prevents GC */
    UPROPERTY()
    USoundWaveProcedural* ProceduralSoundWave;

private:
    /** Parse a WAV file by scanning for fmt and data chunks */
    bool ParseWavFile(const TArray<uint8>& RawFileData, TArray<uint8>& OutPCMData,
                      int32& OutSampleRate, int32& OutNumChannels, int32& OutBitsPerSample);

    /** Convert PCM data of any supported bit depth to 16-bit */
    bool ConvertTo16Bit(const TArray<uint8>& InPCMData, int32 BitsPerSample, TArray<uint8>& Out16BitPCM);
};
