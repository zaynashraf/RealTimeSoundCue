// RealTimeSoundCue.h
#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundCue.h"

#include "RealTimeSoundCue.generated.h"

/**
 * A SoundCue extension that can import and play audio files from disk at runtime
 */
UCLASS(Blueprintable, BlueprintType)
class TEST_API URunTimeSoundCue : public USoundCue
{
    GENERATED_BODY()

public:
    URunTimeSoundCue();

    /**
     * Import an audio file from the specified file path
     * @param FilePath - Full path to the audio file on disk (e.g., "C:/Audio/MySound.wav")
     * @return True if the import was successful
     */
    UFUNCTION(BlueprintCallable, Category = "Audio|Runtime")
    bool ImportAudioFile(const FString& FilePath);

    /**
     * Load and set audio file synchronously
     * @param FilePath - Full path to the audio file
     * @return True if successfully loaded
     */
    UFUNCTION(BlueprintCallable, Category = "Audio|Runtime")
    bool LoadAudioFileSync(const FString& FilePath);

    /**
     * Get the current loaded audio file path
     */
    UFUNCTION(BlueprintPure, Category = "Audio|Runtime")
    FString GetLoadedFilePath() const { return LoadedFilePath; }

    /**
     * Check if an audio file is currently loaded
     */
    UFUNCTION(BlueprintPure, Category = "Audio|Runtime")
    bool IsAudioLoaded() const { return bIsLoaded; }

    /**
     * Clear the currently loaded audio
     */
    UFUNCTION(BlueprintCallable, Category = "Audio|Runtime")
    void ClearLoadedAudio();

protected:
    /** The file path of the currently loaded audio */
    UPROPERTY(BlueprintReadOnly, Category = "Audio|Runtime")
    FString LoadedFilePath;

    /** Whether audio is currently loaded */
    UPROPERTY(BlueprintReadOnly, Category = "Audio|Runtime")
    bool bIsLoaded;

    /** The runtime-created sound wave */
    UPROPERTY()
    class USoundWave* RuntimeSoundWave;

private:
    /**
     * Create a SoundWave from raw audio data
     */
    USoundWave* CreateSoundWaveFromFile(const FString& FilePath);

    /**
     * Parse WAV file format
     */
    bool ParseWavFile(const TArray<uint8>& RawFileData, TArray<uint8>& OutPCMData, int32& OutSampleRate, int32& OutNumChannels);
};
