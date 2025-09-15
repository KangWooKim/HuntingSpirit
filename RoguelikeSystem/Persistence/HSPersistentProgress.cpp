#include "HSPersistentProgress.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Math/UnrealMathUtility.h"

UHSPersistentProgress::UHSPersistentProgress()
{
    // 기본 설정
    SaveFileName = TEXT("HuntingSpiritProgress");
    SaveVersion = 1;
    bAutoSaveEnabled = true;
    AutoSaveInterval = 60.0f;
    
    // 레벨 시스템 설정
    BaseExperienceRequirement = 100;
    ExperienceScalingFactor = 1.5f;
    MaxPlayerLevel = 100;
    
    // 캐시 초기화
    CachedExperienceToNextLevel = -1;
    CachedPlayerLevel = -1;
    CachedWinRate = -1.0f;
    CachedStatisticsHash = -1;
}

void UHSPersistentProgress::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // 기본 데이터 초기화
    InitializeDefaultData();
    
    // 업적 시스템 초기화
    InitializeAchievements();
    
    // 저장된 데이터 로드
    if (!LoadProgressData())
    {
        UE_LOG(LogTemp, Warning, TEXT("저장된 진행도 데이터를 찾을 수 없습니다. 새로운 프로필을 생성합니다."));
        SaveProgressData(); // 기본 데이터 저장
    }
    
    // 자동 저장 설정
    if (bAutoSaveEnabled)
    {
        SetAutoSave(true, AutoSaveInterval);
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSPersistentProgress 초기화 완료 - 플레이어: %s, 레벨: %d"), 
           *PlayerProfile.PlayerName, PlayerProfile.PlayerLevel);
}

void UHSPersistentProgress::Deinitialize()
{
    // 자동 저장 타이머 정리
    if (UWorld* World = GetWorld())
    {
        if (AutoSaveTimerHandle.IsValid())
        {
            World->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
        }
    }
    
    // 최종 저장
    SaveProgressData();
    
    Super::Deinitialize();
    
    UE_LOG(LogTemp, Log, TEXT("HSPersistentProgress 정리 완료"));
}

bool UHSPersistentProgress::LoadProgressData()
{
    FString SaveDirectory = FPaths::ProjectSavedDir() + TEXT("SaveGames/");
    FString FullPath = SaveDirectory + SaveFileName + TEXT(".json");
    
    // 파일 존재 확인
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("저장 파일이 존재하지 않습니다: %s"), *FullPath);
        return false;
    }
    
    // 파일 읽기
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("저장 파일 읽기 실패: %s"), *FullPath);
        return false;
    }
    
    // JSON 파싱
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("JSON 파싱 실패"));
        return false;
    }
    
    // 버전 체크
    int32 LoadedVersion = JsonObject->GetIntegerField(TEXT("Version"));
    if (LoadedVersion != SaveVersion)
    {
        UE_LOG(LogTemp, Warning, TEXT("저장 파일 버전 불일치: %d (현재: %d)"), LoadedVersion, SaveVersion);
        // 버전 마이그레이션이 필요한 경우 여기서 처리
    }
    
    try
    {
        // 플레이어 프로필 로드
        if (const TSharedPtr<FJsonObject>* ProfileObject; JsonObject->TryGetObjectField(TEXT("PlayerProfile"), ProfileObject))
        {
            PlayerProfile.PlayerName = (*ProfileObject)->GetStringField(TEXT("PlayerName"));
            PlayerProfile.PlayerLevel = (*ProfileObject)->GetIntegerField(TEXT("PlayerLevel"));
            PlayerProfile.Experience = (*ProfileObject)->GetIntegerField(TEXT("Experience"));
            PlayerProfile.ExperienceToNextLevel = (*ProfileObject)->GetIntegerField(TEXT("ExperienceToNextLevel"));
            PlayerProfile.FavoriteCharacterClass = (*ProfileObject)->GetStringField(TEXT("FavoriteCharacterClass"));
            PlayerProfile.PreferredDifficulty = (*ProfileObject)->GetIntegerField(TEXT("PreferredDifficulty"));
            
            // 시간 정보
            FString CreationTimeString = (*ProfileObject)->GetStringField(TEXT("CreationTime"));
            FDateTime::Parse(CreationTimeString, PlayerProfile.CreationTime);
            
            FString LastPlayTimeString = (*ProfileObject)->GetStringField(TEXT("LastPlayTime"));
            FDateTime::Parse(LastPlayTimeString, PlayerProfile.LastPlayTime);
        }
        
        // 통계 로드
        if (const TSharedPtr<FJsonObject>* StatsObject; JsonObject->TryGetObjectField(TEXT("Statistics"), StatsObject))
        {
            Statistics.TotalRunsStarted = (*StatsObject)->GetIntegerField(TEXT("TotalRunsStarted"));
            Statistics.TotalRunsCompleted = (*StatsObject)->GetIntegerField(TEXT("TotalRunsCompleted"));
            Statistics.TotalBossesDefeated = (*StatsObject)->GetIntegerField(TEXT("TotalBossesDefeated"));
            Statistics.TotalEnemiesKilled = (*StatsObject)->GetIntegerField(TEXT("TotalEnemiesKilled"));
            Statistics.TotalDeaths = (*StatsObject)->GetIntegerField(TEXT("TotalDeaths"));
            Statistics.TotalPlayTime = (*StatsObject)->GetNumberField(TEXT("TotalPlayTime"));
            Statistics.BestRunTime = (*StatsObject)->GetNumberField(TEXT("BestRunTime"));
            Statistics.BestBossKillTime = (*StatsObject)->GetNumberField(TEXT("BestBossKillTime"));
            Statistics.MostEnemiesKilledInRun = (*StatsObject)->GetIntegerField(TEXT("MostEnemiesKilledInRun"));
            Statistics.LongestSurvivalStreak = (*StatsObject)->GetIntegerField(TEXT("LongestSurvivalStreak"));
            Statistics.HighestDifficultyCleared = (*StatsObject)->GetIntegerField(TEXT("HighestDifficultyCleared"));
            Statistics.TotalCooperativeActions = (*StatsObject)->GetIntegerField(TEXT("TotalCooperativeActions"));
            Statistics.TotalPlayersRevived = (*StatsObject)->GetIntegerField(TEXT("TotalPlayersRevived"));
            Statistics.TotalComboAttacks = (*StatsObject)->GetIntegerField(TEXT("TotalComboAttacks"));
            Statistics.TotalItemsCollected = (*StatsObject)->GetIntegerField(TEXT("TotalItemsCollected"));
            Statistics.TotalResourcesGathered = (*StatsObject)->GetIntegerField(TEXT("TotalResourcesGathered"));
        }
        
        // 메타 화폐 로드
        if (const TSharedPtr<FJsonObject>* CurrencyObject; JsonObject->TryGetObjectField(TEXT("MetaCurrencies"), CurrencyObject))
        {
            MetaCurrencies.Empty();
            for (const auto& Pair : (*CurrencyObject)->Values)
            {
                MetaCurrencies.Add(Pair.Key, static_cast<int32>(Pair.Value->AsNumber()));
            }
        }
        
        // 업적 로드
        if (const TArray<TSharedPtr<FJsonValue>>* AchievementsArray; JsonObject->TryGetArrayField(TEXT("Achievements"), AchievementsArray))
        {
            for (const auto& AchievementValue : *AchievementsArray)
            {
                const TSharedPtr<FJsonObject>& AchievementObj = AchievementValue->AsObject();
                FString AchievementID = AchievementObj->GetStringField(TEXT("AchievementID"));
                
                if (Achievements.Contains(AchievementID))
                {
                    FHSAchievementInfo& Achievement = Achievements[AchievementID];
                    Achievement.bIsUnlocked = AchievementObj->GetBoolField(TEXT("IsUnlocked"));
                    Achievement.Progress = AchievementObj->GetIntegerField(TEXT("Progress"));
                    
                    if (Achievement.bIsUnlocked)
                    {
                        FString UnlockTimeString = AchievementObj->GetStringField(TEXT("UnlockTime"));
                        FDateTime::Parse(UnlockTimeString, Achievement.UnlockTime);
                    }
                }
            }
        }
        
        // 캐시 무효화
        InvalidateCache();
        
        UE_LOG(LogTemp, Log, TEXT("진행도 데이터 로드 완료"));
        return true;
    }
    catch (const std::exception&)
    {
        UE_LOG(LogTemp, Error, TEXT("진행도 데이터 로드 중 오류 발생"));
        return false;
    }
}

bool UHSPersistentProgress::SaveProgressData()
{
    // 데이터 유효성 검증
    if (!ValidateData())
    {
        UE_LOG(LogTemp, Error, TEXT("데이터 유효성 검증 실패"));
        return false;
    }
    
    // JSON 객체 생성
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    
    // 버전 정보
    JsonObject->SetNumberField(TEXT("Version"), SaveVersion);
    JsonObject->SetStringField(TEXT("SaveTime"), FDateTime::Now().ToString());
    
    // 플레이어 프로필 저장
    TSharedPtr<FJsonObject> ProfileObject = MakeShareable(new FJsonObject);
    ProfileObject->SetStringField(TEXT("PlayerName"), PlayerProfile.PlayerName);
    ProfileObject->SetNumberField(TEXT("PlayerLevel"), PlayerProfile.PlayerLevel);
    ProfileObject->SetNumberField(TEXT("Experience"), PlayerProfile.Experience);
    ProfileObject->SetNumberField(TEXT("ExperienceToNextLevel"), PlayerProfile.ExperienceToNextLevel);
    ProfileObject->SetStringField(TEXT("FavoriteCharacterClass"), PlayerProfile.FavoriteCharacterClass);
    ProfileObject->SetNumberField(TEXT("PreferredDifficulty"), PlayerProfile.PreferredDifficulty);
    ProfileObject->SetStringField(TEXT("CreationTime"), PlayerProfile.CreationTime.ToString());
    ProfileObject->SetStringField(TEXT("LastPlayTime"), FDateTime::Now().ToString());
    JsonObject->SetObjectField(TEXT("PlayerProfile"), ProfileObject);
    
    // 통계 저장
    TSharedPtr<FJsonObject> StatsObject = MakeShareable(new FJsonObject);
    StatsObject->SetNumberField(TEXT("TotalRunsStarted"), Statistics.TotalRunsStarted);
    StatsObject->SetNumberField(TEXT("TotalRunsCompleted"), Statistics.TotalRunsCompleted);
    StatsObject->SetNumberField(TEXT("TotalBossesDefeated"), Statistics.TotalBossesDefeated);
    StatsObject->SetNumberField(TEXT("TotalEnemiesKilled"), Statistics.TotalEnemiesKilled);
    StatsObject->SetNumberField(TEXT("TotalDeaths"), Statistics.TotalDeaths);
    StatsObject->SetNumberField(TEXT("TotalPlayTime"), Statistics.TotalPlayTime);
    StatsObject->SetNumberField(TEXT("BestRunTime"), Statistics.BestRunTime);
    StatsObject->SetNumberField(TEXT("BestBossKillTime"), Statistics.BestBossKillTime);
    StatsObject->SetNumberField(TEXT("MostEnemiesKilledInRun"), Statistics.MostEnemiesKilledInRun);
    StatsObject->SetNumberField(TEXT("LongestSurvivalStreak"), Statistics.LongestSurvivalStreak);
    StatsObject->SetNumberField(TEXT("HighestDifficultyCleared"), Statistics.HighestDifficultyCleared);
    StatsObject->SetNumberField(TEXT("TotalCooperativeActions"), Statistics.TotalCooperativeActions);
    StatsObject->SetNumberField(TEXT("TotalPlayersRevived"), Statistics.TotalPlayersRevived);
    StatsObject->SetNumberField(TEXT("TotalComboAttacks"), Statistics.TotalComboAttacks);
    StatsObject->SetNumberField(TEXT("TotalItemsCollected"), Statistics.TotalItemsCollected);
    StatsObject->SetNumberField(TEXT("TotalResourcesGathered"), Statistics.TotalResourcesGathered);
    JsonObject->SetObjectField(TEXT("Statistics"), StatsObject);
    
    // 메타 화폐 저장
    TSharedPtr<FJsonObject> CurrencyObject = MakeShareable(new FJsonObject);
    for (const auto& Pair : MetaCurrencies)
    {
        CurrencyObject->SetNumberField(Pair.Key, Pair.Value);
    }
    JsonObject->SetObjectField(TEXT("MetaCurrencies"), CurrencyObject);
    
    // 업적 저장
    TArray<TSharedPtr<FJsonValue>> AchievementsArray;
    for (const auto& Pair : Achievements)
    {
        TSharedPtr<FJsonObject> AchievementObj = MakeShareable(new FJsonObject);
        AchievementObj->SetStringField(TEXT("AchievementID"), Pair.Key);
        AchievementObj->SetBoolField(TEXT("IsUnlocked"), Pair.Value.bIsUnlocked);
        AchievementObj->SetNumberField(TEXT("Progress"), Pair.Value.Progress);
        if (Pair.Value.bIsUnlocked)
        {
            AchievementObj->SetStringField(TEXT("UnlockTime"), Pair.Value.UnlockTime.ToString());
        }
        
        AchievementsArray.Add(MakeShareable(new FJsonValueObject(AchievementObj)));
    }
    JsonObject->SetArrayField(TEXT("Achievements"), AchievementsArray);
    
    // JSON 문자열로 변환
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
    
    // 파일로 저장
    FString SaveDirectory = FPaths::ProjectSavedDir() + TEXT("SaveGames/");
    FString FullPath = SaveDirectory + SaveFileName + TEXT(".json");
    
    // 디렉토리 생성
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*SaveDirectory))
    {
        PlatformFile.CreateDirectoryTree(*SaveDirectory);
    }
    
    // 파일 저장
    if (!FFileHelper::SaveStringToFile(OutputString, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("저장 파일 쓰기 실패: %s"), *FullPath);
        return false;
    }
    
    // 마지막 플레이 시간 업데이트
    PlayerProfile.LastPlayTime = FDateTime::Now();
    
    UE_LOG(LogTemp, Log, TEXT("진행도 데이터 저장 완료: %s"), *FullPath);
    return true;
}

void UHSPersistentProgress::SetAutoSave(bool bEnabled, float IntervalSeconds)
{
    bAutoSaveEnabled = bEnabled;
    AutoSaveInterval = IntervalSeconds;
    
    if (UWorld* World = GetWorld())
    {
        // 기존 타이머 정리
        if (AutoSaveTimerHandle.IsValid())
        {
            World->GetTimerManager().ClearTimer(AutoSaveTimerHandle);
        }
        
        // 자동 저장 활성화
        if (bAutoSaveEnabled && IntervalSeconds > 0.0f)
        {
            World->GetTimerManager().SetTimer(AutoSaveTimerHandle, this, &UHSPersistentProgress::PerformAutoSave, IntervalSeconds, true);
            UE_LOG(LogTemp, Log, TEXT("자동 저장 활성화됨 - 간격: %.1f초"), IntervalSeconds);
        }
    }
}

void UHSPersistentProgress::RecordRunStarted()
{
    Statistics.TotalRunsStarted++;
    OnStatisticUpdated.Broadcast(Statistics);
    InvalidateCache();
    
    UE_LOG(LogTemp, Log, TEXT("런 시작 기록됨 - 총 런 수: %d"), Statistics.TotalRunsStarted);
}

void UHSPersistentProgress::RecordRunCompleted(float RunTime, int32 Difficulty, bool bVictory)
{
    if (bVictory)
    {
        Statistics.TotalRunsCompleted++;
        
        // 최고 기록 업데이트
        if (Statistics.BestRunTime == 0.0f || RunTime < Statistics.BestRunTime)
        {
            Statistics.BestRunTime = RunTime;
        }
        
        // 난이도별 통계 업데이트
        if (Statistics.RunsCompletedByDifficulty.Contains(Difficulty))
        {
            Statistics.RunsCompletedByDifficulty[Difficulty]++;
        }
        else
        {
            Statistics.RunsCompletedByDifficulty.Add(Difficulty, 1);
        }
        
        // 난이도별 최고 기록 업데이트
        if (!Statistics.BestTimesByDifficulty.Contains(Difficulty) || RunTime < Statistics.BestTimesByDifficulty[Difficulty])
        {
            Statistics.BestTimesByDifficulty.Add(Difficulty, RunTime);
        }
        
        // 최고 난이도 업데이트
        if (Difficulty > Statistics.HighestDifficultyCleared)
        {
            Statistics.HighestDifficultyCleared = Difficulty;
        }
    }
    
    OnStatisticUpdated.Broadcast(Statistics);
    CheckAchievements();
    InvalidateCache();
    
    UE_LOG(LogTemp, Log, TEXT("런 완료 기록됨 - 시간: %.2f초, 난이도: %d, 승리: %s"), 
           RunTime, Difficulty, bVictory ? TEXT("예") : TEXT("아니오"));
}

void UHSPersistentProgress::RecordBossKill(float KillTime)
{
    Statistics.TotalBossesDefeated++;
    
    // 최고 기록 업데이트
    if (Statistics.BestBossKillTime == 0.0f || KillTime < Statistics.BestBossKillTime)
    {
        Statistics.BestBossKillTime = KillTime;
    }
    
    OnStatisticUpdated.Broadcast(Statistics);
    CheckAchievements();
    InvalidateCache();
}

void UHSPersistentProgress::RecordEnemyKills(int32 Count)
{
    if (Count > 0)
    {
        Statistics.TotalEnemiesKilled += Count;
        OnStatisticUpdated.Broadcast(Statistics);
        CheckAchievements();
        InvalidateCache();
    }
}

void UHSPersistentProgress::RecordPlayerDeath()
{
    Statistics.TotalDeaths++;
    OnStatisticUpdated.Broadcast(Statistics);
    InvalidateCache();
}

void UHSPersistentProgress::AddPlayTime(float PlayTime)
{
    if (PlayTime > 0.0f)
    {
        Statistics.TotalPlayTime += PlayTime;
        OnStatisticUpdated.Broadcast(Statistics);
        InvalidateCache();
    }
}

void UHSPersistentProgress::RecordCooperativeStats(int32 CooperativeActions, int32 PlayersRevived, int32 ComboAttacks)
{
    Statistics.TotalCooperativeActions += CooperativeActions;
    Statistics.TotalPlayersRevived += PlayersRevived;
    Statistics.TotalComboAttacks += ComboAttacks;
    
    OnStatisticUpdated.Broadcast(Statistics);
    CheckAchievements();
    InvalidateCache();
}

void UHSPersistentProgress::RecordCollectionStats(int32 ItemsCollected, int32 ResourcesGathered)
{
    Statistics.TotalItemsCollected += ItemsCollected;
    Statistics.TotalResourcesGathered += ResourcesGathered;
    
    OnStatisticUpdated.Broadcast(Statistics);
    CheckAchievements();
    InvalidateCache();
}

void UHSPersistentProgress::AddExperience(int32 Experience)
{
    if (Experience <= 0 || PlayerProfile.PlayerLevel >= MaxPlayerLevel)
    {
        return;
    }
    
    PlayerProfile.Experience += Experience;
    
    // 레벨업 체크
    while (PlayerProfile.Experience >= PlayerProfile.ExperienceToNextLevel && PlayerProfile.PlayerLevel < MaxPlayerLevel)
    {
        PlayerProfile.Experience -= PlayerProfile.ExperienceToNextLevel;
        PlayerProfile.PlayerLevel++;
        PlayerProfile.ExperienceToNextLevel = CalculateExperienceForLevel(PlayerProfile.PlayerLevel + 1);
        
        OnMetaPlayerLevelUp.Broadcast(PlayerProfile.PlayerLevel);
        
        UE_LOG(LogTemp, Log, TEXT("레벨업! 새 레벨: %d"), PlayerProfile.PlayerLevel);
    }
    
    // 최대 레벨에서 경험치 오버플로우 방지
    if (PlayerProfile.PlayerLevel >= MaxPlayerLevel)
    {
        PlayerProfile.Experience = 0;
        PlayerProfile.ExperienceToNextLevel = 0;
    }
    
    InvalidateCache();
}

int32 UHSPersistentProgress::GetPlayerLevel() const
{
    return PlayerProfile.PlayerLevel;
}

int32 UHSPersistentProgress::GetCurrentExperience() const
{
    return PlayerProfile.Experience;
}

int32 UHSPersistentProgress::GetExperienceToNextLevel() const
{
    if (CachedPlayerLevel == PlayerProfile.PlayerLevel && CachedExperienceToNextLevel != -1)
    {
        return CachedExperienceToNextLevel;
    }
    
    CachedPlayerLevel = PlayerProfile.PlayerLevel;
    CachedExperienceToNextLevel = PlayerProfile.ExperienceToNextLevel;
    
    return CachedExperienceToNextLevel;
}

float UHSPersistentProgress::GetLevelProgress() const
{
    if (PlayerProfile.PlayerLevel >= MaxPlayerLevel)
    {
        return 1.0f;
    }
    
    if (PlayerProfile.ExperienceToNextLevel <= 0)
    {
        return 0.0f;
    }
    
    int32 CurrentLevelExp = CalculateExperienceForLevel(PlayerProfile.PlayerLevel);
    int32 NextLevelExp = CalculateExperienceForLevel(PlayerProfile.PlayerLevel + 1);
    int32 TotalExpForLevel = NextLevelExp - CurrentLevelExp;
    
    return FMath::Clamp(static_cast<float>(PlayerProfile.Experience) / TotalExpForLevel, 0.0f, 1.0f);
}

void UHSPersistentProgress::AddMetaCurrency(const FString& CurrencyType, int32 Amount)
{
    if (Amount <= 0)
    {
        return;
    }
    
    if (MetaCurrencies.Contains(CurrencyType))
    {
        MetaCurrencies[CurrencyType] += Amount;
    }
    else
    {
        MetaCurrencies.Add(CurrencyType, Amount);
    }
    
    OnMetaCurrencyChanged.Broadcast(CurrencyType, MetaCurrencies[CurrencyType]);
    
    UE_LOG(LogTemp, Log, TEXT("메타 화폐 추가: %s +%d (총: %d)"), *CurrencyType, Amount, MetaCurrencies[CurrencyType]);
}

bool UHSPersistentProgress::SpendMetaCurrency(const FString& CurrencyType, int32 Amount)
{
    if (Amount <= 0 || !HasEnoughMetaCurrency(CurrencyType, Amount))
    {
        return false;
    }
    
    MetaCurrencies[CurrencyType] -= Amount;
    OnMetaCurrencyChanged.Broadcast(CurrencyType, MetaCurrencies[CurrencyType]);
    
    UE_LOG(LogTemp, Log, TEXT("메타 화폐 사용: %s -%d (남은: %d)"), *CurrencyType, Amount, MetaCurrencies[CurrencyType]);
    return true;
}

int32 UHSPersistentProgress::GetMetaCurrency(const FString& CurrencyType) const
{
    if (const int32* Amount = MetaCurrencies.Find(CurrencyType))
    {
        return *Amount;
    }
    return 0;
}

bool UHSPersistentProgress::HasEnoughMetaCurrency(const FString& CurrencyType, int32 Amount) const
{
    return GetMetaCurrency(CurrencyType) >= Amount;
}

void UHSPersistentProgress::UpdateAchievementProgress(const FString& AchievementID, int32 Progress)
{
    if (!Achievements.Contains(AchievementID))
    {
        return;
    }
    
    FHSAchievementInfo& Achievement = Achievements[AchievementID];
    
    if (Achievement.bIsUnlocked)
    {
        return; // 이미 언락된 업적
    }
    
    Achievement.Progress = FMath::Max(Achievement.Progress, Progress);
    
    // 업적 완료 체크
    if (Achievement.IsCompleted())
    {
        UnlockAchievement(AchievementID);
    }
}

bool UHSPersistentProgress::IsAchievementUnlocked(const FString& AchievementID) const
{
    if (const FHSAchievementInfo* Achievement = Achievements.Find(AchievementID))
    {
        return Achievement->bIsUnlocked;
    }
    return false;
}

FHSAchievementInfo UHSPersistentProgress::GetAchievementInfo(const FString& AchievementID) const
{
    if (const FHSAchievementInfo* Achievement = Achievements.Find(AchievementID))
    {
        return *Achievement;
    }
    return FHSAchievementInfo();
}

TArray<FHSAchievementInfo> UHSPersistentProgress::GetAllAchievements() const
{
    TArray<FHSAchievementInfo> Result;
    Result.Reserve(Achievements.Num());
    
    for (const auto& Pair : Achievements)
    {
        Result.Add(Pair.Value);
    }
    
    return Result;
}

int32 UHSPersistentProgress::GetUnlockedAchievementCount() const
{
    int32 Count = 0;
    for (const auto& Pair : Achievements)
    {
        if (Pair.Value.bIsUnlocked)
        {
            Count++;
        }
    }
    return Count;
}

const FHSPersistentMetaProfile& UHSPersistentProgress::GetPlayerProfile() const
{
    return PlayerProfile;
}

void UHSPersistentProgress::SetPlayerName(const FString& PlayerName)
{
    if (!PlayerName.IsEmpty())
    {
        PlayerProfile.PlayerName = PlayerName;
        UE_LOG(LogTemp, Log, TEXT("플레이어 이름 변경: %s"), *PlayerName);
    }
}

void UHSPersistentProgress::SetFavoriteCharacterClass(const FString& CharacterClass)
{
    if (!CharacterClass.IsEmpty())
    {
        PlayerProfile.FavoriteCharacterClass = CharacterClass;
    }
}

void UHSPersistentProgress::SetPreferredDifficulty(int32 Difficulty)
{
    if (Difficulty >= 0)
    {
        PlayerProfile.PreferredDifficulty = Difficulty;
    }
}

const FHSPersistentStatistics& UHSPersistentProgress::GetPersistentStatistics() const
{
    return Statistics;
}

float UHSPersistentProgress::GetWinRate() const
{
    // 캐시 확인
    int32 CurrentHash = CalculateStatisticsHash();
    if (CurrentHash == CachedStatisticsHash && CachedWinRate != -1.0f)
    {
        return CachedWinRate;
    }
    
    float WinRate = 0.0f;
    if (Statistics.TotalRunsStarted > 0)
    {
        WinRate = static_cast<float>(Statistics.TotalRunsCompleted) / Statistics.TotalRunsStarted;
    }
    
    // 캐시 업데이트
    CachedWinRate = WinRate;
    CachedStatisticsHash = CurrentHash;
    
    return WinRate;
}

float UHSPersistentProgress::GetAverageRunTime() const
{
    if (Statistics.TotalRunsCompleted > 0 && Statistics.TotalPlayTime > 0.0f)
    {
        return Statistics.TotalPlayTime / Statistics.TotalRunsCompleted;
    }
    return 0.0f;
}

void UHSPersistentProgress::InitializeDefaultData()
{
    // 플레이어 프로필 초기화
    PlayerProfile = FHSPersistentMetaProfile();
    PlayerProfile.PlayerName = TEXT("신규 사냥꾼");
    PlayerProfile.PlayerLevel = 1;
    PlayerProfile.Experience = 0;
    PlayerProfile.ExperienceToNextLevel = BaseExperienceRequirement;
    
    // 통계 초기화
    Statistics = FHSPersistentStatistics();
    
    // 기본 메타 화폐 설정
    MetaCurrencies.Empty();
    MetaCurrencies.Add(TEXT("MetaSouls"), 0);
    MetaCurrencies.Add(TEXT("EssencePoints"), 0);
    MetaCurrencies.Add(TEXT("UnlockPoints"), 0);
}

void UHSPersistentProgress::InitializeAchievements()
{
    Achievements.Empty();
    
    // 기본 업적들 초기화
    {
        FHSAchievementInfo Achievement;
        Achievement.AchievementID = TEXT("FirstSteps");
        Achievement.Title = TEXT("첫 걸음");
        Achievement.Description = TEXT("첫 번째 런을 시작하세요");
        Achievement.RequiredProgress = 1;
        Achievement.RewardMetaSouls = 10;
        Achievements.Add(Achievement.AchievementID, Achievement);
    }
    
    {
        FHSAchievementInfo Achievement;
        Achievement.AchievementID = TEXT("FirstVictory");
        Achievement.Title = TEXT("첫 승리");
        Achievement.Description = TEXT("첫 번째 런을 성공적으로 완료하세요");
        Achievement.RequiredProgress = 1;
        Achievement.RewardMetaSouls = 50;
        Achievement.RewardUnlockPoints = 1;
        Achievements.Add(Achievement.AchievementID, Achievement);
    }
    
    {
        FHSAchievementInfo Achievement;
        Achievement.AchievementID = TEXT("BossSlayer");
        Achievement.Title = TEXT("보스 처치자");
        Achievement.Description = TEXT("보스를 10마리 처치하세요");
        Achievement.RequiredProgress = 10;
        Achievement.RewardMetaSouls = 100;
        Achievement.RewardUnlockPoints = 2;
        Achievements.Add(Achievement.AchievementID, Achievement);
    }
    
    {
        FHSAchievementInfo Achievement;
        Achievement.AchievementID = TEXT("Survivor");
        Achievement.Title = TEXT("생존자");
        Achievement.Description = TEXT("사망 없이 런을 5회 연속 완료하세요");
        Achievement.RequiredProgress = 5;
        Achievement.RewardMetaSouls = 200;
        Achievement.RewardUnlockPoints = 3;
        Achievements.Add(Achievement.AchievementID, Achievement);
    }
    
    {
        FHSAchievementInfo Achievement;
        Achievement.AchievementID = TEXT("TeamPlayer");
        Achievement.Title = TEXT("팀 플레이어");
        Achievement.Description = TEXT("협동 행동을 100회 수행하세요");
        Achievement.RequiredProgress = 100;
        Achievement.RewardMetaSouls = 150;
        Achievement.RewardUnlockPoints = 2;
        Achievements.Add(Achievement.AchievementID, Achievement);
    }
    
    UE_LOG(LogTemp, Log, TEXT("업적 시스템 초기화 완료 - 총 %d개 업적"), Achievements.Num());
}

int32 UHSPersistentProgress::CalculateExperienceForLevel(int32 Level) const
{
    if (Level <= 1)
    {
        return BaseExperienceRequirement;
    }
    
    return FMath::RoundToInt(BaseExperienceRequirement * FMath::Pow(ExperienceScalingFactor, Level - 1));
}

void UHSPersistentProgress::CheckAchievements()
{
    // 첫 걸음 업적
    UpdateAchievementProgress(TEXT("FirstSteps"), Statistics.TotalRunsStarted);
    
    // 첫 승리 업적
    UpdateAchievementProgress(TEXT("FirstVictory"), Statistics.TotalRunsCompleted);
    
    // 보스 처치자 업적
    UpdateAchievementProgress(TEXT("BossSlayer"), Statistics.TotalBossesDefeated);
    
    // 생존자 업적 (연속 무사망 런 - 간단화된 버전)
    if (Statistics.TotalRunsCompleted > 0 && Statistics.TotalDeaths == 0)
    {
        UpdateAchievementProgress(TEXT("Survivor"), Statistics.TotalRunsCompleted);
    }
    
    // 팀 플레이어 업적
    UpdateAchievementProgress(TEXT("TeamPlayer"), Statistics.TotalCooperativeActions);
}

void UHSPersistentProgress::UnlockAchievement(const FString& AchievementID)
{
    if (!Achievements.Contains(AchievementID))
    {
        return;
    }
    
    FHSAchievementInfo& Achievement = Achievements[AchievementID];
    
    if (Achievement.bIsUnlocked)
    {
        return; // 이미 언락됨
    }
    
    Achievement.bIsUnlocked = true;
    Achievement.UnlockTime = FDateTime::Now();
    
    // 보상 지급
    if (Achievement.RewardMetaSouls > 0)
    {
        AddMetaCurrency(TEXT("MetaSouls"), Achievement.RewardMetaSouls);
    }
    if (Achievement.RewardUnlockPoints > 0)
    {
        AddMetaCurrency(TEXT("UnlockPoints"), Achievement.RewardUnlockPoints);
    }
    
    OnAchievementUnlocked.Broadcast(AchievementID, Achievement);
    
    UE_LOG(LogTemp, Log, TEXT("업적 언락: %s - %s"), *Achievement.Title, *Achievement.Description);
}

void UHSPersistentProgress::PerformAutoSave()
{
    if (bAutoSaveEnabled)
    {
        SaveProgressData();
        UE_LOG(LogTemp, VeryVerbose, TEXT("자동 저장 수행됨"));
    }
}

bool UHSPersistentProgress::ValidateData() const
{
    // 기본적인 데이터 유효성 검증
    if (PlayerProfile.PlayerName.IsEmpty())
    {
        return false;
    }
    
    if (PlayerProfile.PlayerLevel < 1 || PlayerProfile.PlayerLevel > MaxPlayerLevel)
    {
        return false;
    }
    
    if (PlayerProfile.Experience < 0)
    {
        return false;
    }
    
    // 통계 검증
    if (Statistics.TotalRunsCompleted > Statistics.TotalRunsStarted)
    {
        return false;
    }
    
    if (Statistics.TotalPlayTime < 0.0f)
    {
        return false;
    }
    
    return true;
}

int32 UHSPersistentProgress::CalculateStatisticsHash() const
{
    // 간단한 해시 계산 (통계의 주요 필드들을 조합)
    int32 Hash = 0;
    Hash ^= Statistics.TotalRunsStarted;
    Hash ^= Statistics.TotalRunsCompleted << 1;
    Hash ^= Statistics.TotalBossesDefeated << 2;
    Hash ^= Statistics.TotalEnemiesKilled << 3;
    return Hash;
}

void UHSPersistentProgress::InvalidateCache()
{
    CachedExperienceToNextLevel = -1;
    CachedPlayerLevel = -1;
    CachedWinRate = -1.0f;
    CachedStatisticsHash = -1;
}
