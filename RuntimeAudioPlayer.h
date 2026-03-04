#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "RuntimeAudioPlayer.generated.h"

/**
 * A runtime audio player that loads WAV files from disk and plays them
 * using USoundWaveProcedural (no precaching, no asset import needed).
 *
 * Also provides batch loading from folders for integration with
 * queue/playlist systems.
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

    /** Editable path so you can set it per-instance in the Details panel */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Runtime")
    FString AudioFilePath;

    // -----------------------------------------------------------------
    // Single file operations
    // -----------------------------------------------------------------

    /**
     * Load a single WAV file and return it as a USoundWaveProcedural.
     * Does NOT play it — caller decides what to do with it.
     * This is the core building block for both single playback and batch loading.
     *
     * @param FilePath  Absolute path to a WAV file on disk
     * @return          Loaded sound, or nullptr on failure
     */
    UFUNCTION(BlueprintCallable, Category = "Audio|Runtime")
    USoundWaveProcedural* LoadWavFromFile(const FString& FilePath);

    /**
     * Load a WAV file from disk and play it immediately on this actor's AudioComponent.
     * Convenience wrapper around LoadWavFromFile().
     */
    UFUNCTION(BlueprintCallable, Category = "Audio|Runtime")
    bool PlayWavFromFile(const FString& FilePath);

    // -----------------------------------------------------------------
    // Batch folder loading
    // -----------------------------------------------------------------

    /**
     * Load all WAV files from a folder and return them as an array.
     * Compatible with any system that accepts TArray<USoundBase*> (e.g. Abhay's queue).
     *
     * @param FolderPath    Absolute path to a folder on disk
     * @param bRecursive    If true, also scans all subdirectories
     *                      (handles structures like siteA/mic_02/datetime/audio.wav)
     * @return              Array of loaded sounds (skips any files that fail to load)
     */
    UFUNCTION(BlueprintCallable, Category = "Audio|Runtime")
    TArray<USoundWaveProcedural*> LoadWavsFromFolder(const FString& AudioFolderPath, bool bRecursive = true);

    // -----------------------------------------------------------------
    // Stored results (optional — for Blueprint access after batch load)
    // -----------------------------------------------------------------

    /** All sounds loaded by the most recent LoadWavsFromFolder call */
    UPROPERTY(BlueprintReadOnly, Category = "Audio|Runtime")
    TArray<USoundWaveProcedural*> LoadedSounds;

    /** File paths corresponding to each entry in LoadedSounds (same order) */
    UPROPERTY(BlueprintReadOnly, Category = "Audio|Runtime")
    TArray<FString> LoadedFilePaths;

protected:
    virtual void BeginPlay() override;

    /** Runtime procedural sound wave for single-file playback */
    UPROPERTY()
    USoundWaveProcedural* ProceduralSoundWave;

private:
    /** Parse a WAV file by scanning for fmt and data chunks */
    bool ParseWavFile(const TArray<uint8>& RawFileData, TArray<uint8>& OutPCMData,
                      int32& OutSampleRate, int32& OutNumChannels, int32& OutBitsPerSample);

    /** Convert PCM data of any supported bit depth to 16-bit */
    bool ConvertTo16Bit(const TArray<uint8>& InPCMData, int32 BitsPerSample, TArray<uint8>& Out16BitPCM);
};
