#include "RuntimeAudioPlayer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"

ARuntimeAudioPlayer::ARuntimeAudioPlayer()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create Audio Component
    AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
    AudioComponent->SetupAttachment(RootComponent);
    AudioComponent->bAutoActivate = false; // We'll activate manually
}

bool ARuntimeAudioPlayer::PlayWavFromFile(const FString& FilePath)
{
    if (!FPaths::FileExists(FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("File not found: %s"), *FilePath);
        return false;
    }

    TArray<uint8> RawData;
    if (!FFileHelper::LoadFileToArray(RawData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load file: %s"), *FilePath);
        return false;
    }

    TArray<uint8> PCMData;
    int32 SampleRate = 0;
    int32 NumChannels = 0;

    if (!ParseWavFile(RawData, PCMData, SampleRate, NumChannels))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse WAV: %s"), *FilePath);
        return false;
    }

    RuntimeSoundWave = CreateSoundWave(PCMData, SampleRate, NumChannels);
    if (!RuntimeSoundWave)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create SoundWave"));
        return false;
    }

    // Play sound
    AudioComponent->SetSound(RuntimeSoundWave);
    AudioComponent->Play();

    return true;
}

USoundWave* ARuntimeAudioPlayer::CreateSoundWave(const TArray<uint8>& PCMData, int32 SampleRate, int32 NumChannels)
{
    USoundWave* SoundWave = NewObject<USoundWave>(this);

    if (!SoundWave)
        return nullptr;

    SoundWave->RawPCMDataSize = PCMData.Num();
    SoundWave->RawPCMData = (uint8*)FMemory::Malloc(PCMData.Num());
    FMemory::Memcpy(SoundWave->RawPCMData, PCMData.GetData(), PCMData.Num());

    SoundWave->Duration = (float)PCMData.Num() / (SampleRate * NumChannels * sizeof(int16));
    SoundWave->SetSampleRate(SampleRate);
    SoundWave->NumChannels = NumChannels;

    SoundWave->AddToRoot(); // Prevent GC
    return SoundWave;
}

bool ARuntimeAudioPlayer::ParseWavFile(const TArray<uint8>& RawFileData, TArray<uint8>& OutPCMData, int32& OutSampleRate, int32& OutNumChannels)
{
    // Minimal WAV parsing
    if (RawFileData.Num() < 44)
        return false;

    OutNumChannels = *reinterpret_cast<const uint16*>(&RawFileData[22]);
    OutSampleRate = *reinterpret_cast<const uint32*>(&RawFileData[24]);

    int32 DataOffset = 44; // assume no extra headers
    int32 DataSize = *reinterpret_cast<const uint32*>(&RawFileData[40]);

    if (DataOffset + DataSize > RawFileData.Num())
        return false;

    OutPCMData.SetNum(DataSize);
    FMemory::Memcpy(OutPCMData.GetData(), &RawFileData[DataOffset], DataSize);

    return true;
}
