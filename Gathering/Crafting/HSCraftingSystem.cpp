#include "HSCraftingSystem.h"
#include "../../Items/HSItemBase.h"
#include "../Inventory/HSInventoryComponent.h"
#include "HuntingSpirit/RoguelikeSystem/Progression/HSUnlockSystem.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Templates/Function.h"

UHSCraftingSystem::UHSCraftingSystem()
    : NextJobID(1)
{
}

void UHSCraftingSystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 레시피 데이터 로드
    LoadRecipesFromDataTable();

    // 주기적 업데이트 타이머 설정
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            JobUpdateTimerHandle,
            [this]()
            {
                UpdateCraftingProgress(0.1f); // 0.1초마다 업데이트
            },
            0.1f,
            true
        );

        // 메모리 최적화 타이머
        World->GetTimerManager().SetTimer(
            MemoryOptimizationTimerHandle,
            this,
            &UHSCraftingSystem::OptimizeMemoryUsage,
            JobCleanupInterval,
            true
        );
    }

    UE_LOG(LogTemp, Log, TEXT("HSCraftingSystem::Initialize - 제작 시스템 초기화 완료"));
}

void UHSCraftingSystem::Deinitialize()
{
    // 모든 활성 작업 취소
    TArray<int32> JobsToCancel;
    ActiveJobs.GetKeys(JobsToCancel);
    
    for (int32 JobID : JobsToCancel)
    {
        CancelCrafting(JobID);
    }

    // 타이머 정리
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(JobUpdateTimerHandle);
        World->GetTimerManager().ClearTimer(MemoryOptimizationTimerHandle);
    }

    // 캐시 정리
    CachedRecipes.Empty();
    ActiveJobs.Empty();
    CrafterJobsCache.Empty();
    CategoryRecipesCache.Empty();
    CraftingSkillLevels.Empty();

    Super::Deinitialize();

    UE_LOG(LogTemp, Log, TEXT("HSCraftingSystem::Deinitialize - 제작 시스템 정리 완료"));
}

bool UHSCraftingSystem::LoadRecipesFromDataTable()
{
    if (RecipeDataTable.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCraftingSystem::LoadRecipesFromDataTable - 레시피 데이터 테이블이 설정되지 않음"));
        return false;
    }

    UDataTable* DataTable = RecipeDataTable.LoadSynchronous();
    if (!DataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("HSCraftingSystem::LoadRecipesFromDataTable - 데이터 테이블 로드 실패"));
        return false;
    }

    // 기존 캐시 클리어
    CachedRecipes.Empty();

    // 모든 레시피 로드
    TArray<FName> RowNames = DataTable->GetRowNames();
    for (const FName& RowName : RowNames)
    {
        FHSCraftingRecipeTableRow* Row = DataTable->FindRow<FHSCraftingRecipeTableRow>(RowName, TEXT("LoadRecipes"));
        if (Row && Row->Recipe.IsValid())
        {
            CachedRecipes.Add(Row->Recipe.RecipeID, Row->Recipe);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("HSCraftingSystem::LoadRecipesFromDataTable - 유효하지 않은 레시피: %s"), *RowName.ToString());
        }
    }

    // 카테고리 캐시 구축
    BuildCategoryCache();

    UE_LOG(LogTemp, Log, TEXT("HSCraftingSystem::LoadRecipesFromDataTable - %d개 레시피 로드 완료"), CachedRecipes.Num());
    return true;
}

TArray<FHSCraftingRecipe> UHSCraftingSystem::GetAllRecipes() const
{
    TArray<FHSCraftingRecipe> Recipes;
    CachedRecipes.GenerateValueArray(Recipes);
    return Recipes;
}

TArray<FHSCraftingRecipe> UHSCraftingSystem::GetRecipesByCategory(const FName& Category) const
{
    TArray<FHSCraftingRecipe> CategoryRecipes;
    
    if (const TArray<FName>* RecipeIDs = CategoryRecipesCache.Find(Category))
    {
        for (const FName& RecipeID : *RecipeIDs)
        {
            if (const FHSCraftingRecipe* Recipe = CachedRecipes.Find(RecipeID))
            {
                CategoryRecipes.Add(*Recipe);
            }
        }
    }

    return CategoryRecipes;
}

FHSCraftingRecipe UHSCraftingSystem::GetRecipeByID(const FName& RecipeID) const
{
    if (const FHSCraftingRecipe* Recipe = CachedRecipes.Find(RecipeID))
    {
        return *Recipe;
    }

    UE_LOG(LogTemp, Warning, TEXT("HSCraftingSystem::GetRecipeByID - 레시피를 찾을 수 없음: %s"), *RecipeID.ToString());
    return FHSCraftingRecipe();
}

TArray<FHSCraftingRecipe> UHSCraftingSystem::GetAvailableRecipes(AActor* Crafter) const
{
    TArray<FHSCraftingRecipe> AvailableRecipes;
    
    if (!Crafter)
    {
        return AvailableRecipes;
    }

    for (const auto& RecipePair : CachedRecipes)
    {
        const FHSCraftingRecipe& Recipe = RecipePair.Value;
        if (CanCraftRecipe(Crafter, Recipe))
        {
            AvailableRecipes.Add(Recipe);
        }
    }

    return AvailableRecipes;
}

bool UHSCraftingSystem::CanCraftRecipe(AActor* Crafter, const FHSCraftingRecipe& Recipe) const
{
    if (!Crafter || !Recipe.IsValid())
    {
        return false;
    }

    // 레벨 확인
    if (!HasRequiredLevel(Crafter, Recipe))
    {
        return false;
    }

    // 언락 상태 확인
    if (!IsRecipeUnlocked(Crafter, Recipe))
    {
        return false;
    }

    // 재료 확인
    UHSInventoryComponent* Inventory = GetInventoryComponent(Crafter);
    if (!HasRequiredMaterials(Inventory, Recipe))
    {
        return false;
    }

    return true;
}

bool UHSCraftingSystem::HasRequiredMaterials(UHSInventoryComponent* Inventory, const FHSCraftingRecipe& Recipe, int32 Quantity) const
{
    if (!Inventory)
    {
        return false;
    }

    for (const FHSCraftingMaterial& Material : Recipe.RequiredMaterials)
    {
        if (!Material.IsValid())
        {
            continue;
        }

        UHSItemInstance* RequiredItem = Material.RequiredItem.LoadSynchronous();
        if (!RequiredItem)
        {
            UE_LOG(LogTemp, Warning, TEXT("HSCraftingSystem::HasRequiredMaterials - 아이템 로드 실패"));
            return false;
        }

        int32 RequiredTotal = Material.RequiredQuantity * Quantity;
        if (!Inventory->HasItem(RequiredItem, RequiredTotal))
        {
            return false;
        }
    }

    return true;
}

bool UHSCraftingSystem::HasRequiredLevel(AActor* Crafter, const FHSCraftingRecipe& Recipe) const
{
    if (!Crafter)
    {
        return false;
    }

    int32 CurrentLevel = GetCraftingSkillLevel(Crafter, Recipe.CraftingSkillType);
    return CurrentLevel >= Recipe.RequiredCraftingLevel;
}

bool UHSCraftingSystem::IsRecipeUnlocked(AActor* Crafter, const FHSCraftingRecipe& Recipe) const
{
    if (!Crafter)
    {
        return false;
    }

    if (Recipe.UnlockConditions.Num() == 0)
    {
        return true;
    }

    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    UHSUnlockSystem* UnlockSystem = GameInstance ? GameInstance->GetSubsystem<UHSUnlockSystem>() : nullptr;

    const auto EvaluateSkillRequirement = [this, Crafter](const FString& Expression) -> bool
    {
        FString Normalized = Expression;
        Normalized.TrimStartAndEndInline();

        if (Normalized.IsEmpty())
        {
            return false;
        }

        auto EvaluateComparison = [this, Crafter](const FString& SkillNameString, const FString& LevelString, TFunctionRef<bool(int32, int32)> Comparator)
        {
            FString SkillName = SkillNameString;
            FString LevelValue = LevelString;
            SkillName.TrimStartAndEndInline();
            LevelValue.TrimStartAndEndInline();

            if (SkillName.IsEmpty() || LevelValue.IsEmpty())
            {
                return false;
            }

            const int32 RequiredLevel = FCString::Atoi(*LevelValue);
            if (RequiredLevel <= 0)
            {
                return false;
            }

            const int32 CurrentLevel = GetCraftingSkillLevel(Crafter, FName(*SkillName));
            return Comparator(CurrentLevel, RequiredLevel);
        };

        FString SkillName;
        FString LevelString;

        if (Normalized.Split(TEXT(">="), &SkillName, &LevelString))
        {
            return EvaluateComparison(SkillName, LevelString, [](int32 Current, int32 Required) { return Current >= Required; });
        }

        if (Normalized.Split(TEXT("<="), &SkillName, &LevelString))
        {
            return EvaluateComparison(SkillName, LevelString, [](int32 Current, int32 Required) { return Current <= Required; });
        }

        if (Normalized.Split(TEXT(">"), &SkillName, &LevelString))
        {
            return EvaluateComparison(SkillName, LevelString, [](int32 Current, int32 Required) { return Current > Required; });
        }

        if (Normalized.Split(TEXT("<"), &SkillName, &LevelString))
        {
            return EvaluateComparison(SkillName, LevelString, [](int32 Current, int32 Required) { return Current < Required; });
        }

        if (Normalized.Split(TEXT("="), &SkillName, &LevelString))
        {
            return EvaluateComparison(SkillName, LevelString, [](int32 Current, int32 Required) { return Current == Required; });
        }

        const int32 CurrentLevel = GetCraftingSkillLevel(Crafter, FName(*Normalized));
        return CurrentLevel > 0;
    };

    for (const FName& ConditionName : Recipe.UnlockConditions)
    {
        if (ConditionName.IsNone())
        {
            continue;
        }

        const FString ConditionString = ConditionName.ToString();
        bool bConditionMet = false;

        if (UnlockSystem)
        {
            if (ConditionString.StartsWith(TEXT("Unlock:"), ESearchCase::IgnoreCase))
            {
                const FString UnlockID = ConditionString.Mid(7).TrimStartAndEnd();
                bConditionMet = UnlockID.IsEmpty() || UnlockSystem->IsItemUnlocked(UnlockID);
            }
            else if (ConditionString.StartsWith(TEXT("CanUnlock:"), ESearchCase::IgnoreCase))
            {
                const FString UnlockID = ConditionString.Mid(10).TrimStartAndEnd();
                bConditionMet = UnlockID.IsEmpty() || UnlockSystem->IsItemUnlocked(UnlockID) || UnlockSystem->CanUnlockItem(UnlockID);
            }
            else
            {
                bConditionMet = UnlockSystem->IsItemUnlocked(ConditionString);
            }
        }
        else
        {
            UE_LOG(LogTemp, VeryVerbose, TEXT("HSCraftingSystem::IsRecipeUnlocked - Unlock system unavailable while evaluating condition %s"), *ConditionString);
        }

        if (!bConditionMet)
        {
            if (!UnlockSystem && (ConditionString.StartsWith(TEXT("Unlock:"), ESearchCase::IgnoreCase) || ConditionString.StartsWith(TEXT("CanUnlock:"), ESearchCase::IgnoreCase)))
            {
                bConditionMet = true;
            }
            else if (ConditionString.StartsWith(TEXT("Tag:"), ESearchCase::IgnoreCase))
            {
                const FString TagName = ConditionString.Mid(4).TrimStartAndEnd();
                if (!TagName.IsEmpty())
                {
                    bConditionMet = Crafter->ActorHasTag(FName(*TagName));
                }
            }
            else if (ConditionString.StartsWith(TEXT("Skill:"), ESearchCase::IgnoreCase))
            {
                const FString Requirement = ConditionString.Mid(6);
                bConditionMet = EvaluateSkillRequirement(Requirement);
            }
            else
            {
                bConditionMet = EvaluateSkillRequirement(ConditionString);
            }
        }

        if (!bConditionMet)
        {
            return false;
        }
    }

    return true;
}

int32 UHSCraftingSystem::StartCrafting(AActor* Crafter, const FName& RecipeID, int32 Quantity)
{
    if (!Crafter || Quantity <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("HSCraftingSystem::StartCrafting - 유효하지 않은 제작자 또는 수량"));
        return -1;
    }

    FHSCraftingRecipe Recipe = GetRecipeByID(RecipeID);
    if (!Recipe.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("HSCraftingSystem::StartCrafting - 유효하지 않은 레시피: %s"), *RecipeID.ToString());
        return -1;
    }

    if (!CanCraftRecipe(Crafter, Recipe))
    {
        UE_LOG(LogTemp, Warning, TEXT("HSCraftingSystem::StartCrafting - 제작 조건 미충족: %s"), *RecipeID.ToString());
        return -1;
    }

    // 재료 소모
    UHSInventoryComponent* Inventory = GetInventoryComponent(Crafter);
    if (!ConsumeMaterials(Inventory, Recipe, Quantity))
    {
        UE_LOG(LogTemp, Error, TEXT("HSCraftingSystem::StartCrafting - 재료 소모 실패"));
        return -1;
    }

    // 새 제작 작업 생성
    FHSCraftingJob NewJob;
    NewJob.JobID = NextJobID++;
    NewJob.Crafter = Crafter;
    NewJob.Recipe = Recipe;
    NewJob.State = EHSCraftingState::InProgress;
    NewJob.StartTime = GetWorld()->GetTimeSeconds();
    NewJob.Progress = 0.0f;
    NewJob.CraftingQuantity = Quantity;

    // 활성 작업에 추가
    ActiveJobs.Add(NewJob.JobID, NewJob);
    UpdateCrafterJobsCache(Crafter, NewJob.JobID);

    // 이벤트 브로드캐스트
    OnCraftingStarted.Broadcast(NewJob.JobID, Recipe);

    UE_LOG(LogTemp, Log, TEXT("HSCraftingSystem::StartCrafting - 제작 시작: Job %d, Recipe %s, Quantity %d"), 
           NewJob.JobID, *RecipeID.ToString(), Quantity);

    return NewJob.JobID;
}

bool UHSCraftingSystem::CancelCrafting(int32 JobID)
{
    FHSCraftingJob* Job = ActiveJobs.Find(JobID);
    if (!Job)
    {
        return false;
    }

    // 일부 재료 반환 (진행률에 따라)
    if (Job->Progress < 0.5f) // 50% 미만이면 재료 일부 반환
    {
        UHSInventoryComponent* Inventory = GetInventoryComponent(Job->Crafter.Get());
        if (Inventory)
        {
            for (const FHSCraftingMaterial& Material : Job->Recipe.RequiredMaterials)
            {
                if (Material.bIsConsumed)
                {
                    UHSItemInstance* Item = Material.RequiredItem.LoadSynchronous();
                    if (Item)
                    {
                        int32 ReturnQuantity = FMath::FloorToInt(Material.RequiredQuantity * Job->CraftingQuantity * 0.7f);
                        if (ReturnQuantity > 0)
                        {
                            int32 OutSlotIndex;
                            Inventory->AddItem(Item, ReturnQuantity, OutSlotIndex);
                        }
                    }
                }
            }
        }
    }

    // 작업 상태 변경
    Job->State = EHSCraftingState::Cancelled;

    // 캐시에서 제거
    if (Job->Crafter.IsValid())
    {
        RemoveFromCrafterJobsCache(Job->Crafter.Get(), JobID);
    }

    // 활성 작업에서 제거
    ActiveJobs.Remove(JobID);

    // 이벤트 브로드캐스트
    OnCraftingCancelled.Broadcast(JobID);

    UE_LOG(LogTemp, Log, TEXT("HSCraftingSystem::CancelCrafting - 제작 취소: Job %d"), JobID);
    return true;
}

void UHSCraftingSystem::UpdateCraftingProgress(float DeltaTime)
{
    float CurrentTime = GetWorld()->GetTimeSeconds();
    TArray<int32> CompletedJobs;

    for (auto& JobPair : ActiveJobs)
    {
        FHSCraftingJob& Job = JobPair.Value;
        
        if (Job.State != EHSCraftingState::InProgress)
        {
            continue;
        }

        // 제작자가 유효한지 확인
        if (!Job.Crafter.IsValid())
        {
            FailCraftingJob(Job.JobID, TEXT("제작자가 사라졌습니다"));
            continue;
        }

        // 진행률 업데이트
        float ElapsedTime = CurrentTime - Job.StartTime;
        float TotalTime = Job.Recipe.CraftingTime * Job.CraftingQuantity;
        Job.Progress = FMath::Clamp(ElapsedTime / TotalTime, 0.0f, 1.0f);

        // 진행률 이벤트 브로드캐스트
        float RemainingTime = FMath::Max(0.0f, TotalTime - ElapsedTime);
        OnCraftingProgress.Broadcast(Job.JobID, Job.Progress, RemainingTime);

        // 완료 확인
        if (Job.Progress >= 1.0f)
        {
            CompletedJobs.Add(Job.JobID);
        }
    }

    // 완료된 작업 처리
    for (int32 JobID : CompletedJobs)
    {
        CompleteCraftingJob(JobID);
    }
}

TArray<FHSCraftingJob> UHSCraftingSystem::GetActiveCraftingJobs(AActor* Crafter) const
{
    TArray<FHSCraftingJob> Jobs;

    if (Crafter)
    {
        // 특정 제작자의 작업만 반환
        if (const TArray<int32>* CrafterJobs = CrafterJobsCache.Find(Crafter))
        {
            for (int32 JobID : *CrafterJobs)
            {
                if (const FHSCraftingJob* Job = ActiveJobs.Find(JobID))
                {
                    Jobs.Add(*Job);
                }
            }
        }
    }
    else
    {
        // 모든 활성 작업 반환
        ActiveJobs.GenerateValueArray(Jobs);
    }

    return Jobs;
}

FHSCraftingJob UHSCraftingSystem::GetCraftingJob(int32 JobID) const
{
    if (const FHSCraftingJob* Job = ActiveJobs.Find(JobID))
    {
        return *Job;
    }

    return FHSCraftingJob();
}

bool UHSCraftingSystem::IsCrafting(AActor* Crafter) const
{
    if (!Crafter)
    {
        return false;
    }

    if (const TArray<int32>* CrafterJobs = CrafterJobsCache.Find(Crafter))
    {
        return CrafterJobs->Num() > 0;
    }

    return false;
}

void UHSCraftingSystem::SetCraftingSkillLevel(AActor* Crafter, const FName& SkillType, int32 Level)
{
    if (!Crafter)
    {
        return;
    }

    TMap<FName, int32>& SkillMap = CraftingSkillLevels.FindOrAdd(Crafter);
    SkillMap.Add(SkillType, FMath::Max(1, Level));

    UE_LOG(LogTemp, Log, TEXT("HSCraftingSystem::SetCraftingSkillLevel - %s 스킬 레벨 설정: %d"), 
           *SkillType.ToString(), Level);
}

int32 UHSCraftingSystem::GetCraftingSkillLevel(AActor* Crafter, const FName& SkillType) const
{
    if (!Crafter)
    {
        return 1;
    }

    if (const TMap<FName, int32>* SkillMap = CraftingSkillLevels.Find(Crafter))
    {
        if (const int32* Level = SkillMap->Find(SkillType))
        {
            return *Level;
        }
    }

    return 1; // 기본 레벨
}

void UHSCraftingSystem::AddCraftingExperience(AActor* Crafter, const FName& SkillType, int32 Experience)
{
    if (!Crafter || Experience <= 0)
    {
        return;
    }

    // 간단한 경험치 시스템 구현
    int32 CurrentLevel = GetCraftingSkillLevel(Crafter, SkillType);
    int32 ExperienceNeeded = CurrentLevel * 100; // 레벨 * 100 경험치 필요

    // 레벨업 체크 (실제로는 더 복잡한 경험치 시스템 필요)
    if (Experience >= ExperienceNeeded / 10) // 필요 경험치의 10%를 얻으면 레벨업
    {
        SetCraftingSkillLevel(Crafter, SkillType, CurrentLevel + 1);
        UE_LOG(LogTemp, Log, TEXT("HSCraftingSystem::AddCraftingExperience - 레벨업! %s 스킬 레벨: %d"), 
               *SkillType.ToString(), CurrentLevel + 1);
    }
}

TArray<FName> UHSCraftingSystem::GetAllCategories() const
{
    TArray<FName> Categories;
    CategoryRecipesCache.GetKeys(Categories);
    return Categories;
}

void UHSCraftingSystem::RefreshRecipeCache()
{
    LoadRecipesFromDataTable();
}

void UHSCraftingSystem::ValidateAllRecipes()
{
    int32 ValidRecipes = 0;
    int32 InvalidRecipes = 0;

    for (const auto& RecipePair : CachedRecipes)
    {
        const FHSCraftingRecipe& Recipe = RecipePair.Value;
        
        bool bIsValid = true;
        FString ValidationErrors;

        // 기본 유효성 검사
        if (!Recipe.IsValid())
        {
            bIsValid = false;
            ValidationErrors += TEXT("기본 유효성 검사 실패; ");
        }

        // 결과 아이템 확인
        if (Recipe.ResultItem.IsNull())
        {
            bIsValid = false;
            ValidationErrors += TEXT("결과 아이템 없음; ");
        }

        // 재료 확인
        for (const FHSCraftingMaterial& Material : Recipe.RequiredMaterials)
        {
            if (!Material.IsValid())
            {
                bIsValid = false;
                ValidationErrors += FString::Printf(TEXT("유효하지 않은 재료; "));
                break;
            }
        }

        if (bIsValid)
        {
            ValidRecipes++;
        }
        else
        {
            InvalidRecipes++;
            UE_LOG(LogTemp, Error, TEXT("유효하지 않은 레시피: %s - %s"), 
                   *Recipe.RecipeID.ToString(), *ValidationErrors);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSCraftingSystem::ValidateAllRecipes - 유효: %d, 무효: %d"), 
           ValidRecipes, InvalidRecipes);
}

void UHSCraftingSystem::ExportRecipesToCSV(const FString& FilePath)
{
    FString CSVContent = TEXT("RecipeID,RecipeName,Category,CraftingTime,RequiredLevel,SuccessRate\n");

    for (const auto& RecipePair : CachedRecipes)
    {
        const FHSCraftingRecipe& Recipe = RecipePair.Value;
        CSVContent += FString::Printf(TEXT("%s,%s,%s,%.2f,%d,%.2f\n"),
            *Recipe.RecipeID.ToString(),
            *Recipe.RecipeName.ToString(),
            *Recipe.Category.ToString(),
            Recipe.CraftingTime,
            Recipe.RequiredCraftingLevel,
            Recipe.SuccessRate
        );
    }

    FFileHelper::SaveStringToFile(CSVContent, *FilePath);
    UE_LOG(LogTemp, Log, TEXT("HSCraftingSystem::ExportRecipesToCSV - 레시피 CSV 내보내기 완료: %s"), *FilePath);
}

// 내부 헬퍼 함수들
bool UHSCraftingSystem::ConsumeMaterials(UHSInventoryComponent* Inventory, const FHSCraftingRecipe& Recipe, int32 Quantity)
{
    if (!Inventory)
    {
        return false;
    }

    // 먼저 모든 재료가 충분한지 다시 확인
    if (!HasRequiredMaterials(Inventory, Recipe, Quantity))
    {
        return false;
    }

    // 재료 소모
    for (const FHSCraftingMaterial& Material : Recipe.RequiredMaterials)
    {
        if (Material.bIsConsumed)
        {
            UHSItemInstance* Item = Material.RequiredItem.LoadSynchronous();
            if (Item)
            {
                int32 ConsumeQuantity = Material.RequiredQuantity * Quantity;
                if (!Inventory->RemoveItem(Item, ConsumeQuantity))
                {
                    UE_LOG(LogTemp, Error, TEXT("HSCraftingSystem::ConsumeMaterials - 재료 소모 실패: %s"), 
                           *Item->GetItemName());
                    return false;
                }
            }
        }
    }

    return true;
}

void UHSCraftingSystem::GiveResultItems(UHSInventoryComponent* Inventory, const FHSCraftingRecipe& Recipe, int32 Quantity)
{
    if (!Inventory)
    {
        return;
    }

    UHSItemInstance* ResultItem = Recipe.ResultItem.LoadSynchronous();
    if (ResultItem)
    {
        int32 TotalQuantity = Recipe.ResultQuantity * Quantity;
        int32 OutSlotIndex;
        
        if (!Inventory->AddItem(ResultItem, TotalQuantity, OutSlotIndex))
        {
            UE_LOG(LogTemp, Warning, TEXT("HSCraftingSystem::GiveResultItems - 결과 아이템 추가 실패: %s"), 
                   *ResultItem->GetItemName());
        }
    }
}

bool UHSCraftingSystem::RollCraftingSuccess(const FHSCraftingRecipe& Recipe, AActor* Crafter) const
{
    float SuccessChance = Recipe.SuccessRate;

    // 스킬 레벨에 따른 성공률 보너스
    int32 SkillLevel = GetCraftingSkillLevel(Crafter, Recipe.CraftingSkillType);
    float SkillBonus = (SkillLevel - Recipe.RequiredCraftingLevel) * 0.02f; // 레벨당 2% 보너스
    SuccessChance = FMath::Clamp(SuccessChance + SkillBonus, 0.1f, 1.0f);

    return FMath::RandRange(0.0f, 1.0f) <= SuccessChance;
}

void UHSCraftingSystem::CompleteCraftingJob(int32 JobID)
{
    FHSCraftingJob* Job = ActiveJobs.Find(JobID);
    if (!Job)
    {
        return;
    }

    // 성공/실패 판정
    bool bSuccess = RollCraftingSuccess(Job->Recipe, Job->Crafter.Get());
    
    if (bSuccess)
    {
        // 결과 아이템 지급
        UHSInventoryComponent* Inventory = GetInventoryComponent(Job->Crafter.Get());
        GiveResultItems(Inventory, Job->Recipe, Job->CraftingQuantity);

        // 경험치 지급
        int32 ExperienceGain = Job->Recipe.CraftingTime * Job->CraftingQuantity * 10;
        AddCraftingExperience(Job->Crafter.Get(), Job->Recipe.CraftingSkillType, ExperienceGain);

        // 상태 변경
        Job->State = EHSCraftingState::Completed;

        // 이벤트 브로드캐스트
        UHSItemInstance* ResultItem = Job->Recipe.ResultItem.LoadSynchronous();
        int32 TotalQuantity = Job->Recipe.ResultQuantity * Job->CraftingQuantity;
        OnCraftingCompleted.Broadcast(JobID, ResultItem, TotalQuantity);

        UE_LOG(LogTemp, Log, TEXT("HSCraftingSystem::CompleteCraftingJob - 제작 완료: Job %d"), JobID);
    }
    else
    {
        FailCraftingJob(JobID, TEXT("제작에 실패했습니다"));
    }

    // 캐시에서 제거
    if (Job->Crafter.IsValid())
    {
        RemoveFromCrafterJobsCache(Job->Crafter.Get(), JobID);
    }

    // 활성 작업에서 제거
    ActiveJobs.Remove(JobID);
}

void UHSCraftingSystem::FailCraftingJob(int32 JobID, const FString& Reason)
{
    FHSCraftingJob* Job = ActiveJobs.Find(JobID);
    if (!Job)
    {
        return;
    }

    Job->State = EHSCraftingState::Failed;

    // 이벤트 브로드캐스트
    OnCraftingFailed.Broadcast(JobID, Reason);

    UE_LOG(LogTemp, Warning, TEXT("HSCraftingSystem::FailCraftingJob - 제작 실패: Job %d, Reason: %s"), 
           JobID, *Reason);

    // 캐시에서 제거
    if (Job->Crafter.IsValid())
    {
        RemoveFromCrafterJobsCache(Job->Crafter.Get(), JobID);
    }

    // 활성 작업에서 제거
    ActiveJobs.Remove(JobID);
}

UHSInventoryComponent* UHSCraftingSystem::GetInventoryComponent(AActor* Actor) const
{
    if (!Actor)
    {
        return nullptr;
    }

    return Actor->FindComponentByClass<UHSInventoryComponent>();
}

void UHSCraftingSystem::UpdateCrafterJobsCache(AActor* Crafter, int32 JobID)
{
    if (!Crafter)
    {
        return;
    }

    TArray<int32>& CrafterJobs = CrafterJobsCache.FindOrAdd(Crafter);
    CrafterJobs.AddUnique(JobID);
}

void UHSCraftingSystem::RemoveFromCrafterJobsCache(AActor* Crafter, int32 JobID)
{
    if (!Crafter)
    {
        return;
    }

    if (TArray<int32>* CrafterJobs = CrafterJobsCache.Find(Crafter))
    {
        CrafterJobs->Remove(JobID);
        
        // 비어있으면 전체 엔트리 제거
        if (CrafterJobs->Num() == 0)
        {
            CrafterJobsCache.Remove(Crafter);
        }
    }
}

void UHSCraftingSystem::BuildCategoryCache()
{
    CategoryRecipesCache.Empty();

    for (const auto& RecipePair : CachedRecipes)
    {
        const FHSCraftingRecipe& Recipe = RecipePair.Value;
        TArray<FName>& CategoryRecipes = CategoryRecipesCache.FindOrAdd(Recipe.Category);
        CategoryRecipes.Add(Recipe.RecipeID);
    }

    UE_LOG(LogTemp, Log, TEXT("HSCraftingSystem::BuildCategoryCache - %d개 카테고리 캐시 구축 완료"), 
           CategoryRecipesCache.Num());
}

void UHSCraftingSystem::OptimizeMemoryUsage()
{
    // 완료된 작업들 정리
    ClearExpiredJobs();

    // 유효하지 않은 제작자 정리
    TArray<AActor*> InvalidCrafters;
    for (const auto& CrafterPair : CrafterJobsCache)
    {
        if (!IsValid(CrafterPair.Key))
        {
            InvalidCrafters.Add(CrafterPair.Key);
        }
    }

    for (AActor* InvalidCrafter : InvalidCrafters)
    {
        CrafterJobsCache.Remove(InvalidCrafter);
        CraftingSkillLevels.Remove(InvalidCrafter);
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("HSCraftingSystem::OptimizeMemoryUsage - 메모리 최적화 완료"));
}

void UHSCraftingSystem::ClearExpiredJobs()
{
    float CurrentTime = GetWorld()->GetTimeSeconds();
    TArray<int32> JobsToRemove;

    for (const auto& JobPair : ActiveJobs)
    {
        const FHSCraftingJob& Job = JobPair.Value;
        
        // 1시간 이상 지난 완료/실패/취소된 작업 제거
        if (Job.State != EHSCraftingState::InProgress && 
            (CurrentTime - Job.StartTime) > 3600.0f)
        {
            JobsToRemove.Add(Job.JobID);
        }
    }

    for (int32 JobID : JobsToRemove)
    {
        ActiveJobs.Remove(JobID);
    }

    if (JobsToRemove.Num() > 0)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("HSCraftingSystem::ClearExpiredJobs - %d개 만료된 작업 정리"), 
               JobsToRemove.Num());
    }
}
