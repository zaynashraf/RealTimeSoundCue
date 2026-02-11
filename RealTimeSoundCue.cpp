// RealTimeSoundCue.cpp
#include "RealTimeSoundCue.h"
#include "Sound/SoundWave.h"
#include "AudioDevice.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

URunTimeSoundCue::URunTimeSoundCue()
    : bIsLoaded(false)
    , RuntimeSoundWave(nullptr)
{
}

bool URunTimeSoundCue::ImportAudioFile(const FString& FilePath)
{
    return LoadAudioFileSync(FilePath);
}

bool URunTimeSoundCue::LoadAudioFileSync(const FString& FilePath)
{
    // Validate file path
    if (FilePath.IsEmpty())
    {
        UE_LOG(LogAudio, Error, TEXT("RuntimeSoundCue: File path is empty"));
        return false;
    }

    // Check if file exists
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogAudio, Error, TEXT("RuntimeSoundCue: File does not exist: %s"), *FilePath);
        return false;
    }

    // Clear any previously loaded audio
    ClearLoadedAudio();

    // Create sound wave from file
    RuntimeSoundWave = CreateSoundWaveFromFile(FilePath);

    if (!RuntimeSoundWave)
    {
        UE_LOG(LogAudio, Error, TEXT("RuntimeSoundCue: Failed to create SoundWave from file: %s"), *FilePath);
        return false;
    }

    // Store the file path
    LoadedFilePath = FilePath;
    bIsLoaded = true;

    // Update the SoundCue to use this sound wave
    // Note: You'll need to set up the SoundCue node graph programmatically
    // This is a simplified approach - in practice you might want to create a proper node graph
    FirstNode = RuntimeSoundWave;

    UE_LOG(LogAudio, Log, TEXT("RuntimeSoundCue: Successfully loaded audio file: %s"), *FilePath);
    return true;
}

void URunTimeSoundCue::ClearLoadedAudio()
{
    if (RuntimeSoundWave)
    {
        RuntimeSoundWave->RemoveFromRoot(); // Check if RemoveFromRoot frees memory
        RuntimeSoundWave = nullptr;
    }

    LoadedFilePath.Empty();
    bIsLoaded = false;
    FirstNode = nullptr;
}

USoundWave* URunTimeSoundCue::CreateSoundWaveFromFile(const FString& FilePath)
{
    // Load the file data
    TArray<uint8> RawFileData;
    if (!FFileHelper::LoadFileToArray(RawFileData, *FilePath))
    {
        UE_LOG(LogAudio, Error, TEXT("Failed to load file data from: %s"), *FilePath);
        return nullptr;
    }

    // Check file extension
    FString Extension = FPaths::GetExtension(FilePath).ToLower();

    USoundWave* SoundWave = NewObject<USoundWave>(this);
    if (!SoundWave)
    {
        UE_LOG(LogAudio, Error, TEXT("Failed to create SoundWave object"));
        return nullptr;
    }

    // Handle WAV files
    if (Extension == TEXT("wav"))
    {
        TArray<uint8> PCMData;
        int32 SampleRate = 0;
        int32 NumChannels = 0;

        if (!ParseWavFile(RawFileData, PCMData, SampleRate, NumChannels))
        {
            UE_LOG(LogAudio, Error, TEXT("Failed to parse WAV file: %s"), *FilePath);
            return nullptr;
        }

        // Set up the sound wave
        SoundWave->RawPCMDataSize = PCMData.Num();
        SoundWave->RawPCMData = (uint8*)FMemory::Malloc(PCMData.Num());
        FMemory::Memcpy(SoundWave->RawPCMData, PCMData.GetData(), PCMData.Num());

        SoundWave->Duration = (float)PCMData.Num() / (SampleRate * NumChannels * sizeof(int16));
        SoundWave->SetSampleRate(SampleRate);
        SoundWave->NumChannels = NumChannels;
        SoundWave->RawData.UpdatePayload(FSharedBuffer::Clone(RawFileData.GetData(), RawFileData.Num()));
    }
    else if (Extension == TEXT("ogg") || Extension == TEXT("mp3")) // Test these extensions to see if it works
    {
        // For compressed formats, store the raw data
        // Unreal will decompress on demand
        SoundWave->RawData.UpdatePayload(FSharedBuffer::Clone(RawFileData.GetData(), RawFileData.Num()));
    }
    else
    {
        UE_LOG(LogAudio, Error, TEXT("Unsupported audio format: %s"), *Extension);
        return nullptr;
    }

    // Prevent garbage collection
    SoundWave->AddToRoot();

    return SoundWave;
}

bool URunTimeSoundCue::ParseWavFile(const TArray<uint8>& RawFileData, TArray<uint8>& OutPCMData, int32& OutSampleRate, int32& OutNumChannels) // Might be better to have library do it for us
{
    // WAV file header structure
    struct FWaveFormatEx
    {
        uint16 FormatTag;
        uint16 NumChannels;
        uint32 SamplesPerSec;
        uint32 AvgBytesPerSec;
        uint16 BlockAlign;
        uint16 BitsPerSample;
    };

    if (RawFileData.Num() < 44) // Minimum WAV header size
    {
        return false;
    }

    // Verify RIFF header
    if (RawFileData[0] != 'R' || RawFileData[1] != 'I' || RawFileData[2] != 'F' || RawFileData[3] != 'F')
    {
        return false;
    }

    // Verify WAVE format
    if (RawFileData[8] != 'W' || RawFileData[9] != 'A' || RawFileData[10] != 'V' || RawFileData[11] != 'E')
    {
        return false;
    }

    // Find fmt chunk
    int32 Offset = 12;
    while (Offset < RawFileData.Num() - 8)
    {
        if (RawFileData[Offset] == 'f' && RawFileData[Offset + 1] == 'm' &&
            RawFileData[Offset + 2] == 't' && RawFileData[Offset + 3] == ' ')
        {
            break;
        }
        Offset++;
    }

    if (Offset >= RawFileData.Num() - 24)
    {
        return false;
    }

    // Parse format chunk
    Offset += 8; // Skip 'fmt ' and chunk size
    const FWaveFormatEx* WaveFormat = reinterpret_cast<const FWaveFormatEx*>(&RawFileData[Offset]);
    
    OutSampleRate = WaveFormat->SamplesPerSec;
    OutNumChannels = WaveFormat->NumChannels;

    // Find data chunk
    Offset = 12;
    while (Offset < RawFileData.Num() - 8)
    {
        if (RawFileData[Offset] == 'd' && RawFileData[Offset + 1] == 'a' &&
            RawFileData[Offset + 2] == 't' && RawFileData[Offset + 3] == 'a')
        {
            break;
        }
        Offset++;
    }

    if (Offset >= RawFileData.Num() - 8)
    {
        return false;
    }

    // Get data size
    Offset += 4;
    const uint32 DataSize = *reinterpret_cast<const uint32*>(&RawFileData[Offset]);
    Offset += 4;

    // Copy PCM data
    if (Offset + DataSize > (uint32)RawFileData.Num())
    {
        return false;
    }

    OutPCMData.SetNum(DataSize);
    FMemory::Memcpy(OutPCMData.GetData(), &RawFileData[Offset], DataSize);

    return true;
}
