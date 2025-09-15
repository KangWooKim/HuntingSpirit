#include "HSUnlockSystem.h"
#include "../Persistence/HSPersistentProgress.h"
#include "HSMetaCurrency.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Math/UnrealMathUtility.h"

UHSUnlockSystem::UHSUnlockSystem()
{
    // 기본 설정
    UnlockSaveFileName = TEXT("HuntingSpiritUnlocks");
    bAutoSaveOnUnlock = true;
    
    // 캐시 초기화
    CachedOverallProgress = -1.0f;
    CachedUnlockCount = -1;
}

void UHSUnlockSystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // 기본 언락 데이터 초기화
    InitializeDefaultUnlocks();
    
    // 데이터 테이블에서 언락 데이터 로드
    LoadUnlockDataFromTable();
    
    // 카테고리 데이터 로드
    LoadCategoryData();
    
    // 저장된 언락 상태 로드
    if (!LoadUnlockState())
    {
        UE_LOG(LogTemp, Warning, TEXT("저장된 언락 상태를 찾을 수 없습니다. 기본 상태를 사용합니다."));
    }
    
    // 의존성 그래프 검증
    if (!ValidateDependencyGraph())
    {
        UE_LOG(LogTemp, Error, TEXT("언락 시스템의 의존성 그래프에 순환 의존성이 발견되었습니다!"));
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSUnlockSystem 초기화 완료 - 언락 아이템 수: %d, 카테고리 수: %d"), 
           UnlockItems.Num(), Categories.Num());
}

void UHSUnlockSystem::Deinitialize()
{
    // 언락 상태 저장
    if (bAutoSaveOnUnlock)
    {
        SaveUnlockState();
    }
    
    // 캐시 정리
    InvalidateCache();
    
    Super::Deinitialize();
    
    UE_LOG(LogTemp, Log, TEXT("HSUnlockSystem 정리 완료"));
}

bool UHSUnlockSystem::UnlockItem(const FString& UnlockID)
{
    if (UnlockID.IsEmpty() || !UnlockItems.Contains(UnlockID))
    {
        UE_LOG(LogTemp, Warning, TEXT("언락 실패: 존재하지 않는 아이템 ID - %s"), *UnlockID);
        OnUnlockPurchaseAttempt.Broadcast(UnlockID, false);
        return false;
    }
    
    FHSUnlockItem& UnlockItem = UnlockItems[UnlockID];
    
    // 이미 언락된 아이템인지 확인
    if (UnlockItem.bIsUnlocked)
    {
        UE_LOG(LogTemp, Warning, TEXT("언락 실패: 이미 언락된 아이템 - %s"), *UnlockID);
        OnUnlockPurchaseAttempt.Broadcast(UnlockID, false);
        return false;
    }
    
    // 언락 가능한지 확인
    if (!CanUnlockItem(UnlockID))
    {
        UE_LOG(LogTemp, Warning, TEXT("언락 실패: 조건 미충족 - %s"), *UnlockID);
        OnUnlockPurchaseAttempt.Broadcast(UnlockID, false);
        return false;
    }
    
    // 언락 비용 지불
    if (!PayUnlockCost(UnlockItem.UnlockCost))
    {
        UE_LOG(LogTemp, Warning, TEXT("언락 실패: 비용 부족 - %s"), *UnlockID);
        OnUnlockPurchaseAttempt.Broadcast(UnlockID, false);
        return false;
    }
    
    // 언락 처리
    UnlockItem.bIsUnlocked = true;
    UnlockItem.UnlockTime = FDateTime::Now();
    
    // 캐시 무효화
    InvalidateCache();
    
    // 이벤트 발생
    OnItemUnlocked.Broadcast(UnlockID, UnlockItem);
    OnUnlockPurchaseAttempt.Broadcast(UnlockID, true);
    
    // 자동 저장
    if (bAutoSaveOnUnlock)
    {
        SaveUnlockState();
    }
    
    UE_LOG(LogTemp, Log, TEXT("아이템 언락 성공: %s - %s"), *UnlockID, *UnlockItem.DisplayName);
    
    // 연쇄 언락 체크 (새로운 아이템이 언락 가능해졌는지 확인)
    TArray<FHSUnlockItem> AvailableUnlocks = GetAvailableUnlocks(false);
    OnUnlockSystemUpdated.Broadcast(AvailableUnlocks);
    
    return true;
}

bool UHSUnlockSystem::IsItemUnlocked(const FString& UnlockID) const
{
    if (const FHSUnlockItem* Item = UnlockItems.Find(UnlockID))
    {
        return Item->bIsUnlocked;
    }
    return false;
}

bool UHSUnlockSystem::CanUnlockItem(const FString& UnlockID) const
{
    if (UnlockID.IsEmpty() || !UnlockItems.Contains(UnlockID))
    {
        return false;
    }
    
    const FHSUnlockItem& UnlockItem = UnlockItems[UnlockID];
    
    // 이미 언락된 아이템은 언락 불가
    if (UnlockItem.bIsUnlocked || !UnlockItem.bIsVisible)
    {
        return false;
    }
    
    // 선행 조건 확인
    if (!CheckPrerequisites(UnlockItem.Prerequisites))
    {
        return false;
    }
    
    // 언락 비용 확인
    if (!CanAffordUnlock(UnlockItem.UnlockCost))
    {
        return false;
    }
    
    return true;
}

FHSUnlockItem UHSUnlockSystem::GetUnlockItem(const FString& UnlockID) const
{
    if (const FHSUnlockItem* Item = UnlockItems.Find(UnlockID))
    {
        return *Item;
    }
    return FHSUnlockItem();
}

TArray<FHSUnlockItem> UHSUnlockSystem::GetUnlockedItemsByType(EHSUnlockType UnlockType) const
{
    TArray<FHSUnlockItem> Result;
    
    for (const auto& Pair : UnlockItems)
    {
        const FHSUnlockItem& Item = Pair.Value;
        if (Item.bIsUnlocked && Item.UnlockType == UnlockType)
        {
            Result.Add(Item);
        }
    }
    
    SortUnlockItems(Result);
    return Result;
}

TArray<FHSUnlockItem> UHSUnlockSystem::GetAvailableUnlocks(bool bIncludeUnlocked) const
{
    TArray<FHSUnlockItem> Result;
    
    for (const auto& Pair : UnlockItems)
    {
        const FHSUnlockItem& Item = Pair.Value;
        
        if (!Item.bIsVisible)
        {
            continue;
        }
        
        if (bIncludeUnlocked)
        {
            Result.Add(Item);
        }
        else if (!Item.bIsUnlocked && CanUnlockItem(Pair.Key))
        {
            Result.Add(Item);
        }
    }
    
    SortUnlockItems(Result);
    return Result;
}

TArray<FHSUnlockItem> UHSUnlockSystem::GetUnlocksByCategory(const FString& CategoryID, bool bIncludeUnlocked) const
{
    // 캐시 확인
    FString CacheKey = CategoryID + (bIncludeUnlocked ? TEXT("_All") : TEXT("_Available"));
    if (const TArray<FHSUnlockItem>* CachedResult = CachedCategoryResults.Find(CacheKey))
    {
        return *CachedResult;
    }
    
    TArray<FHSUnlockItem> Result;
    
    for (const auto& Pair : UnlockItems)
    {
        const FHSUnlockItem& Item = Pair.Value;
        
        if (Item.Category != CategoryID || !Item.bIsVisible)
        {
            continue;
        }
        
        if (bIncludeUnlocked)
        {
            Result.Add(Item);
        }
        else if (!Item.bIsUnlocked)
        {
            Result.Add(Item);
        }
    }
    
    SortUnlockItems(Result);
    
    // 캐시에 저장
    CachedCategoryResults.Add(CacheKey, Result);
    
    return Result;
}

bool UHSUnlockSystem::CheckUnlockConditions(const TArray<FHSUnlockCondition>& Conditions) const
{
    for (const FHSUnlockCondition& Condition : Conditions)
    {
        if (!CheckSingleCondition(Condition))
        {
            return false;
        }
    }
    return true;
}

bool UHSUnlockSystem::CheckSingleCondition(const FHSUnlockCondition& Condition) const
{
    // 캐시 확인
    FString ConditionKey = GenerateConditionKey(Condition);
    bool CachedResult;
    if (GetCachedConditionResult(ConditionKey, CachedResult))
    {
        return CachedResult;
    }
    
    bool Result = false;
    
    switch (Condition.ConditionType)
    {
        case EHSUnlockConditionType::Currency:
        {
            if (UHSMetaCurrency* MetaCurrencySystem = GetGameInstance()->GetSubsystem<UHSMetaCurrency>())
            {
                int32 CurrentAmount = MetaCurrencySystem->GetCurrency(Condition.ConditionKey);
                Result = CurrentAmount >= Condition.RequiredValue;
            }
            break;
        }
        
        case EHSUnlockConditionType::Achievement:
        {
            if (UHSPersistentProgress* ProgressSystem = GetGameInstance()->GetSubsystem<UHSPersistentProgress>())
            {
                Result = ProgressSystem->IsAchievementUnlocked(Condition.ConditionKey);
            }
            break;
        }
        
        case EHSUnlockConditionType::Level:
        {
            if (UHSPersistentProgress* ProgressSystem = GetGameInstance()->GetSubsystem<UHSPersistentProgress>())
            {
                int32 PlayerLevel = ProgressSystem->GetPlayerLevel();
                Result = PlayerLevel >= Condition.RequiredValue;
            }
            break;
        }
        
        case EHSUnlockConditionType::Statistic:
        {
            if (UHSPersistentProgress* ProgressSystem = GetGameInstance()->GetSubsystem<UHSPersistentProgress>())
            {
                const FHSPersistentStatistics& Stats = ProgressSystem->GetPersistentStatistics();
                
                // 통계 타입별 확인
                if (Condition.ConditionKey == TEXT("TotalRunsCompleted"))
                {
                    Result = Stats.TotalRunsCompleted >= Condition.RequiredValue;
                }
                else if (Condition.ConditionKey == TEXT("TotalBossesDefeated"))
                {
                    Result = Stats.TotalBossesDefeated >= Condition.RequiredValue;
                }
                else if (Condition.ConditionKey == TEXT("TotalEnemiesKilled"))
                {
                    Result = Stats.TotalEnemiesKilled >= Condition.RequiredValue;
                }
                else if (Condition.ConditionKey == TEXT("HighestDifficultyCleared"))
                {
                    Result = Stats.HighestDifficultyCleared >= Condition.RequiredValue;
                }
                else if (Condition.ConditionKey == TEXT("TotalCooperativeActions"))
                {
                    Result = Stats.TotalCooperativeActions >= Condition.RequiredValue;
                }
                else if (Condition.ConditionKey == TEXT("TotalPlayTime"))
                {
                    Result = Stats.TotalPlayTime >= static_cast<float>(Condition.RequiredValue);
                }
            }
            break;
        }
        
        case EHSUnlockConditionType::Dependency:
        {
            Result = IsItemUnlocked(Condition.ConditionKey);
            break;
        }
        
        case EHSUnlockConditionType::Time:
        {
            // 시간 기반 조건 (예: 게임 출시 후 특정 시간이 지난 후)
            FDateTime CurrentTime = FDateTime::Now();
            FDateTime RequiredTime;
            if (FDateTime::Parse(Condition.OptionalParameter, RequiredTime))
            {
                Result = CurrentTime >= RequiredTime;
            }
            break;
        }
        
        default:
            Result = false;
            break;
    }
    
    // 결과 캐시
    CacheConditionResult(ConditionKey, Result);
    
    return Result;
}

bool UHSUnlockSystem::CheckPrerequisites(const TArray<FString>& Prerequisites) const
{
    for (const FString& PrerequisiteID : Prerequisites)
    {
        if (!IsItemUnlocked(PrerequisiteID))
        {
            return false;
        }
    }
    return true;
}

bool UHSUnlockSystem::CanAffordUnlock(const FHSUnlockCost& UnlockCost) const
{
    // 화폐 비용 확인
    if (UHSMetaCurrency* MetaCurrencySystem = GetGameInstance()->GetSubsystem<UHSMetaCurrency>())
    {
        for (const auto& CostPair : UnlockCost.CurrencyCosts)
        {
            if (!MetaCurrencySystem->HasEnoughCurrency(CostPair.Key, CostPair.Value))
            {
                return false;
            }
        }
    }
    else
    {
        // 메타 화폐 시스템이 없으면 화폐 비용이 있는 경우 실패
        if (UnlockCost.CurrencyCosts.Num() > 0)
        {
            return false;
        }
    }
    
    // 추가 조건들 확인
    return CheckUnlockConditions(UnlockCost.AdditionalConditions);
}

TArray<FHSUnlockCategory> UHSUnlockSystem::GetAllCategories() const
{
    TArray<FHSUnlockCategory> Result;
    Result.Reserve(Categories.Num());
    
    for (const auto& Pair : Categories)
    {
        if (Pair.Value.bIsVisible)
        {
            Result.Add(Pair.Value);
        }
    }
    
    // 정렬 순서대로 정렬
    Result.Sort([](const FHSUnlockCategory& A, const FHSUnlockCategory& B)
    {
        return A.SortOrder < B.SortOrder;
    });
    
    return Result;
}

FHSUnlockCategory UHSUnlockSystem::GetCategory(const FString& CategoryID) const
{
    if (const FHSUnlockCategory* Category = Categories.Find(CategoryID))
    {
        return *Category;
    }
    return FHSUnlockCategory();
}

float UHSUnlockSystem::GetOverallProgress() const
{
    // 캐시 확인
    if (CachedOverallProgress >= 0.0f)
    {
        return CachedOverallProgress;
    }
    
    int32 TotalItems = 0;
    int32 UnlockedItems = 0;
    
    for (const auto& Pair : UnlockItems)
    {
        const FHSUnlockItem& Item = Pair.Value;
        if (Item.bIsVisible)
        {
            TotalItems++;
            if (Item.bIsUnlocked)
            {
                UnlockedItems++;
            }
        }
    }
    
    float Progress = 0.0f;
    if (TotalItems > 0)
    {
        Progress = static_cast<float>(UnlockedItems) / TotalItems;
    }
    
    // 캐시 업데이트
    CachedOverallProgress = Progress;
    
    return Progress;
}

float UHSUnlockSystem::GetCategoryProgress(const FString& CategoryID) const
{
    int32 TotalItems = 0;
    int32 UnlockedItems = 0;
    
    for (const auto& Pair : UnlockItems)
    {
        const FHSUnlockItem& Item = Pair.Value;
        if (Item.Category == CategoryID && Item.bIsVisible)
        {
            TotalItems++;
            if (Item.bIsUnlocked)
            {
                UnlockedItems++;
            }
        }
    }
    
    if (TotalItems > 0)
    {
        return static_cast<float>(UnlockedItems) / TotalItems;
    }
    return 0.0f;
}

int32 UHSUnlockSystem::GetUnlockedItemCount() const
{
    // 캐시 확인
    if (CachedUnlockCount >= 0)
    {
        return CachedUnlockCount;
    }
    
    int32 Count = 0;
    for (const auto& Pair : UnlockItems)
    {
        if (Pair.Value.bIsUnlocked && Pair.Value.bIsVisible)
        {
            Count++;
        }
    }
    
    // 캐시 업데이트
    CachedUnlockCount = Count;
    
    return Count;
}

int32 UHSUnlockSystem::GetTotalItemCount() const
{
    int32 Count = 0;
    for (const auto& Pair : UnlockItems)
    {
        if (Pair.Value.bIsVisible)
        {
            Count++;
        }
    }
    return Count;
}

void UHSUnlockSystem::RefreshUnlockData()
{
    // 캐시 무효화
    InvalidateCache();
    
    // 데이터 다시 로드
    LoadUnlockDataFromTable();
    LoadCategoryData();
    
    // 이벤트 발생
    TArray<FHSUnlockItem> AvailableUnlocks = GetAvailableUnlocks(false);
    OnUnlockSystemUpdated.Broadcast(AvailableUnlocks);
    
    UE_LOG(LogTemp, Log, TEXT("언락 데이터 새로고침 완료"));
}

void UHSUnlockSystem::AddUnlockItem(const FHSUnlockItem& UnlockItem)
{
    if (!UnlockItem.UnlockID.IsEmpty())
    {
        UnlockItems.Add(UnlockItem.UnlockID, UnlockItem);
        InvalidateCache();
        
        UE_LOG(LogTemp, Log, TEXT("언락 아이템 추가됨: %s"), *UnlockItem.UnlockID);
    }
}

void UHSUnlockSystem::RemoveUnlockItem(const FString& UnlockID)
{
    if (UnlockItems.Remove(UnlockID) > 0)
    {
        InvalidateCache();
        UE_LOG(LogTemp, Log, TEXT("언락 아이템 제거됨: %s"), *UnlockID);
    }
}

void UHSUnlockSystem::SetItemVisibility(const FString& UnlockID, bool bVisible)
{
    if (FHSUnlockItem* Item = UnlockItems.Find(UnlockID))
    {
        Item->bIsVisible = bVisible;
        InvalidateCache();
    }
}

bool UHSUnlockSystem::SaveUnlockState()
{
    // JSON 객체 생성
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    
    // 버전 정보
    JsonObject->SetNumberField(TEXT("Version"), 1);
    JsonObject->SetStringField(TEXT("SaveTime"), FDateTime::Now().ToString());
    
    // 언락된 아이템들 저장
    TArray<TSharedPtr<FJsonValue>> UnlockedItems;
    for (const auto& Pair : UnlockItems)
    {
        if (Pair.Value.bIsUnlocked)
        {
            TSharedPtr<FJsonObject> ItemObj = MakeShareable(new FJsonObject);
            ItemObj->SetStringField(TEXT("UnlockID"), Pair.Key);
            ItemObj->SetStringField(TEXT("UnlockTime"), Pair.Value.UnlockTime.ToString());
            
            UnlockedItems.Add(MakeShareable(new FJsonValueObject(ItemObj)));
        }
    }
    JsonObject->SetArrayField(TEXT("UnlockedItems"), UnlockedItems);
    
    // JSON 문자열로 변환
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
    
    // 파일로 저장
    FString SaveDirectory = FPaths::ProjectSavedDir() + TEXT("SaveGames/");
    FString FullPath = SaveDirectory + UnlockSaveFileName + TEXT(".json");
    
    // 디렉토리 생성
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*SaveDirectory))
    {
        PlatformFile.CreateDirectoryTree(*SaveDirectory);
    }
    
    // 파일 저장
    if (!FFileHelper::SaveStringToFile(OutputString, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("언락 상태 저장 실패: %s"), *FullPath);
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("언락 상태 저장 완료: %s"), *FullPath);
    return true;
}

bool UHSUnlockSystem::LoadUnlockState()
{
    FString SaveDirectory = FPaths::ProjectSavedDir() + TEXT("SaveGames/");
    FString FullPath = SaveDirectory + UnlockSaveFileName + TEXT(".json");
    
    // 파일 존재 확인
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("언락 상태 파일이 존재하지 않습니다: %s"), *FullPath);
        return false;
    }
    
    // 파일 읽기
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("언락 상태 파일 읽기 실패: %s"), *FullPath);
        return false;
    }
    
    // JSON 파싱
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("언락 상태 JSON 파싱 실패"));
        return false;
    }
    
    try
    {
        // 언락된 아이템들 로드
        if (const TArray<TSharedPtr<FJsonValue>>* UnlockedItemsArray; JsonObject->TryGetArrayField(TEXT("UnlockedItems"), UnlockedItemsArray))
        {
            for (const auto& ItemValue : *UnlockedItemsArray)
            {
                const TSharedPtr<FJsonObject>& ItemObj = ItemValue->AsObject();
                FString UnlockID = ItemObj->GetStringField(TEXT("UnlockID"));
                
                if (FHSUnlockItem* Item = UnlockItems.Find(UnlockID))
                {
                    Item->bIsUnlocked = true;
                    
                    FString UnlockTimeString = ItemObj->GetStringField(TEXT("UnlockTime"));
                    FDateTime::Parse(UnlockTimeString, Item->UnlockTime);
                }
            }
        }
        
        // 캐시 무효화
        InvalidateCache();
        
        UE_LOG(LogTemp, Log, TEXT("언락 상태 로드 완료"));
        return true;
    }
    catch (const std::exception&)
    {
        UE_LOG(LogTemp, Error, TEXT("언락 상태 로드 중 오류 발생"));
        return false;
    }
}

void UHSUnlockSystem::InitializeDefaultUnlocks()
{
    // 기본 카테고리 생성
    {
        FHSUnlockCategory Category;
        Category.CategoryID = TEXT("Character");
        Category.DisplayName = TEXT("캐릭터");
        Category.Description = TEXT("새로운 캐릭터 클래스");
        Category.SortOrder = 0;
        Categories.Add(Category.CategoryID, Category);
    }
    
    {
        FHSUnlockCategory Category;
        Category.CategoryID = TEXT("Weapon");
        Category.DisplayName = TEXT("무기");
        Category.Description = TEXT("새로운 무기 및 장비");
        Category.SortOrder = 1;
        Categories.Add(Category.CategoryID, Category);
    }
    
    {
        FHSUnlockCategory Category;
        Category.CategoryID = TEXT("Upgrade");
        Category.DisplayName = TEXT("영구 업그레이드");
        Category.Description = TEXT("영구적인 능력 향상");
        Category.SortOrder = 2;
        Categories.Add(Category.CategoryID, Category);
    }
    
    {
        FHSUnlockCategory Category;
        Category.CategoryID = TEXT("Difficulty");
        Category.DisplayName = TEXT("난이도");
        Category.Description = TEXT("새로운 도전");
        Category.SortOrder = 3;
        Categories.Add(Category.CategoryID, Category);
    }
    
    // 기본 언락 아이템들 생성
    
    // 시프 클래스 언락
    {
        FHSUnlockItem Item;
        Item.UnlockID = TEXT("UnlockThief");
        Item.DisplayName = TEXT("시프 클래스");
        Item.Description = TEXT("빠른 공격과 민첩성을 자랑하는 시프 클래스를 언락합니다.");
        Item.UnlockType = EHSUnlockType::CharacterClass;
        Item.Category = TEXT("Character");
        Item.Priority = 1;
        
        Item.UnlockCost.AddCurrencyCost(TEXT("MetaSouls"), 100);
        Item.UnlockCost.AddCondition(FHSUnlockCondition(EHSUnlockConditionType::Statistic, TEXT("TotalRunsCompleted"), 5, TEXT("런을 5회 완료하세요")));
        
        Item.SetParameter(TEXT("CharacterClass"), TEXT("Thief"));
        UnlockItems.Add(Item.UnlockID, Item);
    }
    
    // 마법사 클래스 언락
    {
        FHSUnlockItem Item;
        Item.UnlockID = TEXT("UnlockMage");
        Item.DisplayName = TEXT("마법사 클래스");
        Item.Description = TEXT("강력한 마법 공격을 사용하는 마법사 클래스를 언락합니다.");
        Item.UnlockType = EHSUnlockType::CharacterClass;
        Item.Category = TEXT("Character");
        Item.Priority = 2;
        
        Item.UnlockCost.AddCurrencyCost(TEXT("MetaSouls"), 200);
        Item.UnlockCost.AddCondition(FHSUnlockCondition(EHSUnlockConditionType::Statistic, TEXT("TotalBossesDefeated"), 3, TEXT("보스를 3마리 처치하세요")));
        Item.Prerequisites.Add(TEXT("UnlockThief"));
        
        Item.SetParameter(TEXT("CharacterClass"), TEXT("Mage"));
        UnlockItems.Add(Item.UnlockID, Item);
    }
    
    // 하드 난이도 언락
    {
        FHSUnlockItem Item;
        Item.UnlockID = TEXT("UnlockHardDifficulty");
        Item.DisplayName = TEXT("하드 난이도");
        Item.Description = TEXT("더 어려운 도전을 원하는 사냥꾼을 위한 하드 난이도를 언락합니다.");
        Item.UnlockType = EHSUnlockType::Difficulty;
        Item.Category = TEXT("Difficulty");
        Item.Priority = 1;
        
        Item.UnlockCost.AddCurrencyCost(TEXT("MetaSouls"), 150);
        Item.UnlockCost.AddCondition(FHSUnlockCondition(EHSUnlockConditionType::Statistic, TEXT("HighestDifficultyCleared"), 1, TEXT("노말 난이도를 클리어하세요")));
        
        Item.SetParameter(TEXT("Difficulty"), TEXT("Hard"));
        UnlockItems.Add(Item.UnlockID, Item);
    }
    
    // 영구 체력 업그레이드
    {
        FHSUnlockItem Item;
        Item.UnlockID = TEXT("UpgradeMaxHealth");
        Item.DisplayName = TEXT("체력 증강");
        Item.Description = TEXT("최대 체력을 영구적으로 10% 증가시킵니다.");
        Item.UnlockType = EHSUnlockType::PermanentUpgrade;
        Item.Category = TEXT("Upgrade");
        Item.Priority = 1;
        
        Item.UnlockCost.AddCurrencyCost(TEXT("EssencePoints"), 50);
        Item.UnlockCost.AddCondition(FHSUnlockCondition(EHSUnlockConditionType::Level, TEXT("PlayerLevel"), 5, TEXT("레벨 5에 도달하세요")));
        
        Item.SetParameter(TEXT("UpgradeType"), TEXT("MaxHealth"));
        Item.SetParameter(TEXT("UpgradeValue"), TEXT("0.1"));
        UnlockItems.Add(Item.UnlockID, Item);
    }
    
    // 영구 스태미너 업그레이드
    {
        FHSUnlockItem Item;
        Item.UnlockID = TEXT("UpgradeMaxStamina");
        Item.DisplayName = TEXT("스태미너 증강");
        Item.Description = TEXT("최대 스태미너를 영구적으로 15% 증가시킵니다.");
        Item.UnlockType = EHSUnlockType::PermanentUpgrade;
        Item.Category = TEXT("Upgrade");
        Item.Priority = 2;
        
        Item.UnlockCost.AddCurrencyCost(TEXT("EssencePoints"), 75);
        Item.UnlockCost.AddCondition(FHSUnlockCondition(EHSUnlockConditionType::Statistic, TEXT("TotalCooperativeActions"), 25, TEXT("협동 행동을 25회 수행하세요")));
        
        Item.SetParameter(TEXT("UpgradeType"), TEXT("MaxStamina"));
        Item.SetParameter(TEXT("UpgradeValue"), TEXT("0.15"));
        UnlockItems.Add(Item.UnlockID, Item);
    }
    
    UE_LOG(LogTemp, Log, TEXT("기본 언락 데이터 초기화 완료 - 아이템 수: %d"), UnlockItems.Num());
}

void UHSUnlockSystem::LoadUnlockDataFromTable()
{
    // 데이터 테이블이 설정되어 있다면 로드
    if (UnlockDataTable.IsValid())
    {
        UDataTable* DataTable = UnlockDataTable.LoadSynchronous();
        if (DataTable)
        {
            // 데이터 테이블에서 언락 데이터를 로드하는 로직
            // 여기서는 기본 구현만 제공 (실제로는 데이터 테이블 구조에 따라 구현)
            UE_LOG(LogTemp, Log, TEXT("데이터 테이블에서 언락 데이터 로드됨"));
        }
    }
}

void UHSUnlockSystem::LoadCategoryData()
{
    // 카테고리 데이터 테이블이 설정되어 있다면 로드
    if (CategoryDataTable.IsValid())
    {
        UDataTable* DataTable = CategoryDataTable.LoadSynchronous();
        if (DataTable)
        {
            // 데이터 테이블에서 카테고리 데이터를 로드하는 로직
            UE_LOG(LogTemp, Log, TEXT("데이터 테이블에서 카테고리 데이터 로드됨"));
        }
    }
}

bool UHSUnlockSystem::PayUnlockCost(const FHSUnlockCost& UnlockCost)
{
    // 메타 화폐 시스템을 통해 비용 지불
    if (UHSMetaCurrency* MetaCurrencySystem = GetGameInstance()->GetSubsystem<UHSMetaCurrency>())
    {
        return MetaCurrencySystem->SpendMultipleCurrencies(UnlockCost.CurrencyCosts);
    }
    
    // 메타 화폐 시스템이 없으면 화폐 비용이 없는 경우에만 성공
    return UnlockCost.CurrencyCosts.Num() == 0;
}

void UHSUnlockSystem::SortUnlockItems(TArray<FHSUnlockItem>& Items) const
{
    Items.Sort([](const FHSUnlockItem& A, const FHSUnlockItem& B)
    {
        // 우선순위로 먼저 정렬
        if (A.Priority != B.Priority)
        {
            return A.Priority < B.Priority;
        }
        
        // 언락 상태로 정렬 (언락된 것이 뒤로)
        if (A.bIsUnlocked != B.bIsUnlocked)
        {
            return !A.bIsUnlocked && B.bIsUnlocked;
        }
        
        // 이름으로 정렬
        return A.DisplayName < B.DisplayName;
    });
}

bool UHSUnlockSystem::ValidateDependencyGraph() const
{
    // 간단한 순환 의존성 검사 (깊이 우선 탐색)
    TSet<FString> Visited;
    TSet<FString> RecursionStack;
    
    for (const auto& Pair : UnlockItems)
    {
        if (!Visited.Contains(Pair.Key))
        {
            if (!ValidateDependencyGraphHelper(Pair.Key, Visited, RecursionStack))
            {
                return false;
            }
        }
    }
    
    return true;
}

bool UHSUnlockSystem::ValidateDependencyGraphHelper(const FString& UnlockID, TSet<FString>& Visited, TSet<FString>& RecursionStack) const
{
    Visited.Add(UnlockID);
    RecursionStack.Add(UnlockID);
    
    if (const FHSUnlockItem* Item = UnlockItems.Find(UnlockID))
    {
        for (const FString& Prerequisite : Item->Prerequisites)
        {
            if (!Visited.Contains(Prerequisite))
            {
                if (!ValidateDependencyGraphHelper(Prerequisite, Visited, RecursionStack))
                {
                    return false;
                }
            }
            else if (RecursionStack.Contains(Prerequisite))
            {
                // 순환 의존성 발견
                UE_LOG(LogTemp, Error, TEXT("순환 의존성 발견: %s -> %s"), *UnlockID, *Prerequisite);
                return false;
            }
        }
    }
    
    RecursionStack.Remove(UnlockID);
    return true;
}

void UHSUnlockSystem::InvalidateCache()
{
    CachedConditionResults.Empty();
    CachedCategoryResults.Empty();
    CachedOverallProgress = -1.0f;
    CachedUnlockCount = -1;
}

void UHSUnlockSystem::CacheConditionResult(const FString& ConditionKey, bool Result) const
{
    CachedConditionResults.Add(ConditionKey, Result);
}

bool UHSUnlockSystem::GetCachedConditionResult(const FString& ConditionKey, bool& OutResult) const
{
    if (const bool* CachedResult = CachedConditionResults.Find(ConditionKey))
    {
        OutResult = *CachedResult;
        return true;
    }
    return false;
}

FString UHSUnlockSystem::GenerateConditionKey(const FHSUnlockCondition& Condition) const
{
    return FString::Printf(TEXT("%d_%s_%d_%s"), 
                          static_cast<int32>(Condition.ConditionType),
                          *Condition.ConditionKey,
                          Condition.RequiredValue,
                          *Condition.OptionalParameter);
}
