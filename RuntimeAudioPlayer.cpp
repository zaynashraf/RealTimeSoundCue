#include "RuntimeAudioPlayer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

ARuntimeAudioPlayer::ARuntimeAudioPlayer()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create root component so AudioComponent has something to attach to
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // Create Audio Component
    AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
    AudioComponent->SetupAttachment(RootComponent);
    AudioComponent->bAutoActivate = false;
}

void ARuntimeAudioPlayer::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("RuntimeAudioPlayer::BeginPlay fired"));

    FString PathToUse = AudioFilePath;
    if (PathToUse.IsEmpty())
    {
        PathToUse = TEXT("/Users/zaynashraf/Downloads/XC1073469 - Screaming Piha - Lipaugus vociferans.wav");
    }

    if (PlayWavFromFile(PathToUse))
    {
        UE_LOG(LogTemp, Warning, TEXT("RuntimeAudioPlayer: Audio playback started successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("RuntimeAudioPlayer: Failed to play audio"));
    }
}

bool ARuntimeAudioPlayer::PlayWavFromFile(const FString& FilePath)
{
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("File not found: %s"), *FilePath);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("Loading WAV file: %s"), *FilePath);

    // Load raw file bytes
    TArray<uint8> RawData;
    if (!FFileHelper::LoadFileToArray(RawData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *FilePath);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("File loaded: %d bytes"), RawData.Num());

    // Parse WAV header and extract PCM data
    TArray<uint8> PCMData;
    int32 SampleRate = 0;
    int32 NumChannels = 0;
    int32 BitsPerSample = 0;

    if (!ParseWavFile(RawData, PCMData, SampleRate, NumChannels, BitsPerSample))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse WAV: %s"), *FilePath);
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("WAV parsed: %d Hz, %d ch, %d-bit, %d bytes PCM"),
           SampleRate, NumChannels, BitsPerSample, PCMData.Num());

    // Convert to 16-bit if needed (USoundWaveProcedural expects 16-bit PCM)
    TArray<uint8> FinalPCM;
    if (!ConvertTo16Bit(PCMData, BitsPerSample, FinalPCM))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to convert audio to 16-bit PCM"));
        return false;
    }

    // ---------------------------------------------------------------
    // Create USoundWaveProcedural
    //
    // This completely bypasses UE5's asset decompression / precache
    // pipeline. No RawData, no DTYPE_RealTime, no streaming buffers.
    // We just hand it raw 16-bit PCM and it plays.
    // ---------------------------------------------------------------
    ProceduralSoundWave = NewObject<USoundWaveProcedural>(this);
    if (!ProceduralSoundWave)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create USoundWaveProcedural"));
        return false;
    }

    ProceduralSoundWave->SetSampleRate(SampleRate);
    ProceduralSoundWave->NumChannels = NumChannels;
    ProceduralSoundWave->Duration = (float)FinalPCM.Num() / (SampleRate * NumChannels * sizeof(int16));
    ProceduralSoundWave->SoundGroup = SOUNDGROUP_Default;
    ProceduralSoundWave->bLooping = false;

    // Queue the entire PCM buffer — the procedural wave will consume it during playback
    ProceduralSoundWave->QueueAudio(FinalPCM.GetData(), FinalPCM.Num());

    UE_LOG(LogTemp, Log, TEXT("Procedural SoundWave created: %.2fs, %d bytes queued"),
           ProceduralSoundWave->Duration, FinalPCM.Num());

    // Play
    AudioComponent->SetSound(ProceduralSoundWave);
    AudioComponent->Play();

    UE_LOG(LogTemp, Log, TEXT("AudioComponent->Play() called"));

    return true;
}

// ---------------------------------------------------------------------------
// 24-bit / 32-bit → 16-bit conversion
// ---------------------------------------------------------------------------
bool ARuntimeAudioPlayer::ConvertTo16Bit(const TArray<uint8>& InPCMData, int32 BitsPerSample, TArray<uint8>& Out16BitPCM)
{
    if (BitsPerSample == 16)
    {
        Out16BitPCM = InPCMData;
        return true;
    }
    else if (BitsPerSample == 24)
    {
        // 24-bit: 3 bytes per sample (little-endian signed)
        // Keep the top 2 bytes → 16-bit
        int32 NumSamples = InPCMData.Num() / 3;
        Out16BitPCM.SetNum(NumSamples * 2);

        for (int32 i = 0; i < NumSamples; i++)
        {
            Out16BitPCM[i * 2 + 0] = InPCMData[i * 3 + 1]; // mid → low byte of 16-bit
            Out16BitPCM[i * 2 + 1] = InPCMData[i * 3 + 2]; // high → high byte of 16-bit
        }

        UE_LOG(LogTemp, Log, TEXT("Converted 24-bit -> 16-bit PCM: %d -> %d bytes"),
               InPCMData.Num(), Out16BitPCM.Num());
        return true;
    }
    else if (BitsPerSample == 32)
    {
        // 32-bit integer PCM: keep top 2 bytes
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

// ---------------------------------------------------------------------------
// Chunk-scanning WAV parser
// ---------------------------------------------------------------------------
bool ARuntimeAudioPlayer::ParseWavFile(const TArray<uint8>& RawFileData, TArray<uint8>& OutPCMData,
                                        int32& OutSampleRate, int32& OutNumChannels, int32& OutBitsPerSample)
{
    if (RawFileData.Num() < 44)
    {
        UE_LOG(LogTemp, Error, TEXT("WAV file too small: %d bytes"), RawFileData.Num());
        return false;
    }

    // Verify RIFF header
    if (RawFileData[0] != 'R' || RawFileData[1] != 'I' ||
        RawFileData[2] != 'F' || RawFileData[3] != 'F')
    {
        UE_LOG(LogTemp, Error, TEXT("Not a RIFF file"));
        return false;
    }

    // Verify WAVE format
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

    if (AudioFormat != 1) // 1 = PCM
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
