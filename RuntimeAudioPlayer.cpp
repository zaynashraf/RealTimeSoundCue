#include "RuntimeAudioPlayer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

ARuntimeAudioPlayer::ARuntimeAudioPlayer()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
    AudioComponent->SetupAttachment(RootComponent);
    AudioComponent->bAutoActivate = false;
}

void ARuntimeAudioPlayer::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("RuntimeAudioPlayer::BeginPlay fired"));

    // Single-file test playback (if AudioFilePath is set in Details panel)
    if (!AudioFilePath.IsEmpty())
    {
        if (PlayWavFromFile(AudioFilePath))
        {
            UE_LOG(LogTemp, Warning, TEXT("RuntimeAudioPlayer: Audio playback started successfully"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("RuntimeAudioPlayer: Failed to play audio"));
        }
    }
}

// =============================================================================
// Single file: Load (without playing)
// =============================================================================
USoundWaveProcedural* ARuntimeAudioPlayer::LoadWavFromFile(const FString& FilePath)
{
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("File not found: %s"), *FilePath);
        return nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("Loading WAV file: %s"), *FilePath);

    // Load raw file bytes
    TArray<uint8> RawData;
    if (!FFileHelper::LoadFileToArray(RawData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *FilePath);
        return nullptr;
    }

    // Parse WAV header and extract PCM data
    TArray<uint8> PCMData;
    int32 SampleRate = 0;
    int32 NumChannels = 0;
    int32 BitsPerSample = 0;

    if (!ParseWavFile(RawData, PCMData, SampleRate, NumChannels, BitsPerSample))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse WAV: %s"), *FilePath);
        return nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("WAV parsed: %d Hz, %d ch, %d-bit, %d bytes PCM"),
           SampleRate, NumChannels, BitsPerSample, PCMData.Num());

    // Convert to 16-bit if needed
    TArray<uint8> FinalPCM;
    if (!ConvertTo16Bit(PCMData, BitsPerSample, FinalPCM))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to convert audio to 16-bit PCM"));
        return nullptr;
    }

    // Create USoundWaveProcedural
    USoundWaveProcedural* SoundWave = NewObject<USoundWaveProcedural>(this);
    if (!SoundWave)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create USoundWaveProcedural"));
        return nullptr;
    }

    SoundWave->SetSampleRate(SampleRate);
    SoundWave->NumChannels = NumChannels;
    SoundWave->Duration = (float)FinalPCM.Num() / (SampleRate * NumChannels * sizeof(int16));
    SoundWave->SoundGroup = SOUNDGROUP_Default;
    SoundWave->bLooping = false;

    // Queue the entire PCM buffer
    SoundWave->QueueAudio(FinalPCM.GetData(), FinalPCM.Num());

    UE_LOG(LogTemp, Log, TEXT("Loaded: %s (%.2fs)"), *FPaths::GetCleanFilename(FilePath), SoundWave->Duration);

    return SoundWave;
}

// =============================================================================
// Single file: Load and play immediately
// =============================================================================
bool ARuntimeAudioPlayer::PlayWavFromFile(const FString& FilePath)
{
    ProceduralSoundWave = LoadWavFromFile(FilePath);
    if (!ProceduralSoundWave)
    {
        return false;
    }

    AudioComponent->SetSound(ProceduralSoundWave);
    AudioComponent->Play();

    UE_LOG(LogTemp, Log, TEXT("AudioComponent->Play() called"));
    return true;
}

// =============================================================================
// Batch folder loading
// =============================================================================
TArray<USoundWaveProcedural*> ARuntimeAudioPlayer::LoadWavsFromFolder(const FString& AudioFolderPath, bool bRecursive)
{
    // Clear previous results
    LoadedSounds.Empty();
    LoadedFilePaths.Empty();

    // Validate folder
    if (!FPaths::DirectoryExists(AudioFolderPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Folder not found: %s"), *AudioFolderPath);
        return LoadedSounds;
    }

    UE_LOG(LogTemp, Warning, TEXT("Scanning folder for WAVs: %s (recursive: %s)"),
           *AudioFolderPath, bRecursive ? TEXT("yes") : TEXT("no"));

    // Find all .wav files
    TArray<FString> FoundFiles;
    IFileManager& FileManager = IFileManager::Get();

    if (bRecursive)
    {
        // Recursive: find WAVs in all subdirectories
        FileManager.FindFilesRecursive(FoundFiles, *AudioFolderPath, TEXT("*.wav"), true, false);
    }
    else
    {
        // Non-recursive: only this folder
        FString SearchPattern = FPaths::Combine(AudioFolderPath, TEXT("*.wav"));
        FileManager.FindFiles(FoundFiles, *SearchPattern, true, false);

        // FindFiles returns filenames only — prepend the folder path
        for (FString& FileName : FoundFiles)
        {
            FileName = FPaths::Combine(AudioFolderPath, FileName);
        }
    }

    // Sort for consistent ordering
    FoundFiles.Sort();

    UE_LOG(LogTemp, Warning, TEXT("Found %d WAV files"), FoundFiles.Num());

    // Load each file
    int32 SuccessCount = 0;
    int32 FailCount = 0;

    for (const FString& WavPath : FoundFiles)
    {
        USoundWaveProcedural* Sound = LoadWavFromFile(WavPath);
        if (Sound)
        {
            LoadedSounds.Add(Sound);
            LoadedFilePaths.Add(WavPath);
            SuccessCount++;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Skipped (failed to load): %s"), *WavPath);
            FailCount++;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Batch load complete: %d loaded, %d failed, %d total"),
           SuccessCount, FailCount, FoundFiles.Num());

    return LoadedSounds;
}

// =============================================================================
// 24-bit / 32-bit → 16-bit conversion
// =============================================================================
bool ARuntimeAudioPlayer::ConvertTo16Bit(const TArray<uint8>& InPCMData, int32 BitsPerSample, TArray<uint8>& Out16BitPCM)
{
    if (BitsPerSample == 16)
    {
        Out16BitPCM = InPCMData;
        return true;
    }
    else if (BitsPerSample == 24)
    {
        int32 NumSamples = InPCMData.Num() / 3;
        Out16BitPCM.SetNum(NumSamples * 2);

        for (int32 i = 0; i < NumSamples; i++)
        {
            Out16BitPCM[i * 2 + 0] = InPCMData[i * 3 + 1];
            Out16BitPCM[i * 2 + 1] = InPCMData[i * 3 + 2];
        }

        UE_LOG(LogTemp, Log, TEXT("Converted 24-bit -> 16-bit PCM: %d -> %d bytes"),
               InPCMData.Num(), Out16BitPCM.Num());
        return true;
    }
    else if (BitsPerSample == 32)
    {
        int32 NumSamples = InPCMData.Num() / 4;
        Out16BitPCM.SetNum(NumSamples * 2);

        for (int32 i = 0; i < NumSamples; i++)
        {
            Out16BitPCM[i * 2 + 0] = InPCMData[i * 4 + 2];
            Out16BitPCM[i * 2 + 1] = InPCMData[i * 4 + 3];
        }

        UE_LOG(LogTemp, Log, TEXT("Converted 32-bit -> 16-bit PCM: %d -> %d bytes"),
               InPCMData.Num(), Out16BitPCM.Num());
        return true;
    }

    UE_LOG(LogTemp, Error, TEXT("Unsupported bit depth: %d"), BitsPerSample);
    return false;
}

// =============================================================================
// Chunk-scanning WAV parser
// =============================================================================
bool ARuntimeAudioPlayer::ParseWavFile(const TArray<uint8>& RawFileData, TArray<uint8>& OutPCMData,
                                        int32& OutSampleRate, int32& OutNumChannels, int32& OutBitsPerSample)
{
    if (RawFileData.Num() < 44)
    {
        UE_LOG(LogTemp, Error, TEXT("WAV file too small: %d bytes"), RawFileData.Num());
        return false;
    }

    if (RawFileData[0] != 'R' || RawFileData[1] != 'I' ||
        RawFileData[2] != 'F' || RawFileData[3] != 'F')
    {
        UE_LOG(LogTemp, Error, TEXT("Not a RIFF file"));
        return false;
    }

    if (RawFileData[8] != 'W' || RawFileData[9] != 'A' ||
        RawFileData[10] != 'V' || RawFileData[11] != 'E')
    {
        UE_LOG(LogTemp, Error, TEXT("Not a WAVE file"));
        return false;
    }

    // --- Scan for 'fmt ' chunk ---
    int32 FmtOffset = -1;
    {
        int32 Offset = 12;
        while (Offset < RawFileData.Num() - 8)
        {
            if (RawFileData[Offset] == 'f' && RawFileData[Offset + 1] == 'm' &&
                RawFileData[Offset + 2] == 't' && RawFileData[Offset + 3] == ' ')
            {
                FmtOffset = Offset;
                break;
            }
            Offset++;
        }
    }

    if (FmtOffset < 0 || FmtOffset + 24 > RawFileData.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("Could not find 'fmt ' chunk"));
        return false;
    }

    int32 FmtDataOffset = FmtOffset + 8;

    uint16 AudioFormat  = *reinterpret_cast<const uint16*>(&RawFileData[FmtDataOffset + 0]);
    OutNumChannels      = *reinterpret_cast<const uint16*>(&RawFileData[FmtDataOffset + 2]);
    OutSampleRate       = *reinterpret_cast<const uint32*>(&RawFileData[FmtDataOffset + 4]);
    OutBitsPerSample    = *reinterpret_cast<const uint16*>(&RawFileData[FmtDataOffset + 14]);

    if (AudioFormat != 1)
    {
        UE_LOG(LogTemp, Error, TEXT("WAV is not PCM format (format tag: %d). Only PCM is supported."), AudioFormat);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("fmt chunk: format=%d, channels=%d, sampleRate=%d, bitsPerSample=%d"),
           AudioFormat, OutNumChannels, OutSampleRate, OutBitsPerSample);

    // --- Scan for 'data' chunk ---
    int32 DataOffset = -1;
    uint32 DataSize = 0;
    {
        int32 Offset = 12;
        while (Offset < RawFileData.Num() - 8)
        {
            if (RawFileData[Offset] == 'd' && RawFileData[Offset + 1] == 'a' &&
                RawFileData[Offset + 2] == 't' && RawFileData[Offset + 3] == 'a')
            {
                DataSize = *reinterpret_cast<const uint32*>(&RawFileData[Offset + 4]);
                DataOffset = Offset + 8;
                break;
            }
            Offset++;
        }
    }

    if (DataOffset < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not find 'data' chunk"));
        return false;
    }

    if (DataOffset + (int32)DataSize > RawFileData.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("data chunk size (%u) exceeds file bounds, clamping"), DataSize);
        DataSize = RawFileData.Num() - DataOffset;
    }

    UE_LOG(LogTemp, Log, TEXT("data chunk at offset %d, size %u bytes"), DataOffset, DataSize);

    OutPCMData.SetNum(DataSize);
    FMemory::Memcpy(OutPCMData.GetData(), &RawFileData[DataOffset], DataSize);

    return true;
}
