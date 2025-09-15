#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "../../Items/HSItemBase.h"
#include "HSCraftingSystem.generated.h"

class UHSItemInstance;
class UHSInventoryComponent;

/**
 * 제작 재료 정보 구조체
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSCraftingMaterial
{
    GENERATED_BODY()

    // 필요한 아이템
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UHSItemInstance> RequiredItem;

    // 필요한 수량
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RequiredQuantity;

    // 소모 여부 (false면 도구로 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsConsumed;

    FHSCraftingMaterial()
        : RequiredQuantity(1)
        , bIsConsumed(true)
    {
    }

    bool IsValid() const
    {
        return !RequiredItem.IsNull() && RequiredQuantity > 0;
    }
};

/**
 * 제작 레시피 정보 구조체
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSCraftingRecipe
{
    GENERATED_BODY()

    // 레시피 고유 ID
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName RecipeID;

    // 레시피 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText RecipeName;

    // 레시피 설명
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText RecipeDescription;

    // 제작 결과물
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UHSItemInstance> ResultItem;

    // 결과물 수량
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ResultQuantity;

    // 필요 재료들
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FHSCraftingMaterial> RequiredMaterials;

    // 제작 시간 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CraftingTime;

    // 제작 비용 (게임 내 화폐)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CraftingCost;

    // 필요 제작 레벨
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RequiredCraftingLevel;

    // 제작 스킬 타입
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName CraftingSkillType;

    // 제작 성공률 (0.0 ~ 1.0)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SuccessRate;

    // 언락 조건
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> UnlockConditions;

    // 카테고리
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Category;

    // 레시피 아이콘
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UTexture2D> RecipeIcon;

    FHSCraftingRecipe()
        : ResultQuantity(1)
        , CraftingTime(1.0f)
        , CraftingCost(0)
        , RequiredCraftingLevel(1)
        , CraftingSkillType(TEXT("General"))
        , SuccessRate(1.0f)
        , Category(TEXT("Misc"))
    {
    }

    bool IsValid() const
    {
        return !RecipeID.IsNone() && !ResultItem.IsNull() && RequiredMaterials.Num() > 0;
    }
};

/**
 * 제작 데이터 테이블 행 구조체
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSCraftingRecipeTableRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FHSCraftingRecipe Recipe;
};

/**
 * 제작 진행 상태
 */
UENUM(BlueprintType)
enum class EHSCraftingState : uint8
{
    Idle                UMETA(DisplayName = "대기 중"),
    InProgress          UMETA(DisplayName = "제작 중"),
    Completed           UMETA(DisplayName = "완료"),
    Failed              UMETA(DisplayName = "실패"),
    Cancelled           UMETA(DisplayName = "취소됨")
};

/**
 * 제작 작업 정보
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSCraftingJob
{
    GENERATED_BODY()

    // 작업 ID
    UPROPERTY(BlueprintReadOnly)
    int32 JobID;

    // 제작자
    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> Crafter;

    // 레시피 정보
    UPROPERTY(BlueprintReadOnly)
    FHSCraftingRecipe Recipe;

    // 현재 상태
    UPROPERTY(BlueprintReadOnly)
    EHSCraftingState State;

    // 시작 시간
    UPROPERTY(BlueprintReadOnly)
    float StartTime;

    // 진행률 (0.0 ~ 1.0)
    UPROPERTY(BlueprintReadOnly)
    float Progress;

    // 제작 수량
    UPROPERTY(BlueprintReadOnly)
    int32 CraftingQuantity;

    FHSCraftingJob()
        : JobID(-1)
        , State(EHSCraftingState::Idle)
        , StartTime(0.0f)
        , Progress(0.0f)
        , CraftingQuantity(1)
    {
    }
};

// 제작 시스템 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCraftingStarted, int32, JobID, const FHSCraftingRecipe&, Recipe);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCraftingProgress, int32, JobID, float, Progress, float, RemainingTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCraftingCompleted, int32, JobID, UHSItemInstance*, ResultItem, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCraftingFailed, int32, JobID, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCraftingCancelled, int32, JobID);

/**
 * 제작 시스템 메인 클래스
 * 게임 전체의 제작 기능을 관리하는 서브시스템
 * 비동기 처리, 메모리 풀링, 캐싱 시스템 적용
 */
UCLASS(BlueprintType)
class HUNTINGSPIRIT_API UHSCraftingSystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSCraftingSystem();

    // 서브시스템 라이프사이클
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

protected:
    // 레시피 데이터베이스 (캐시된 레시피들)
    UPROPERTY()
    TMap<FName, FHSCraftingRecipe> CachedRecipes;

    // 진행 중인 제작 작업들
    UPROPERTY()
    TMap<int32, FHSCraftingJob> ActiveJobs;

    // 레시피 데이터 테이블
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting Settings")
    TSoftObjectPtr<UDataTable> RecipeDataTable;

    // 제작 작업 ID 카운터
    UPROPERTY()
    int32 NextJobID;

    // 성능 최적화를 위한 캐시 (네트워크 복제 불필요)
    TMap<AActor*, TArray<int32>> CrafterJobsCache;
    TMap<FName, TArray<FName>> CategoryRecipesCache;

    // 제작 스킬 레벨 저장 (네트워크 복제 불필요)
    TMap<AActor*, TMap<FName, int32>> CraftingSkillLevels;

public:
    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Crafting Events")
    FOnCraftingStarted OnCraftingStarted;

    UPROPERTY(BlueprintAssignable, Category = "Crafting Events")
    FOnCraftingProgress OnCraftingProgress;

    UPROPERTY(BlueprintAssignable, Category = "Crafting Events")
    FOnCraftingCompleted OnCraftingCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Crafting Events")
    FOnCraftingFailed OnCraftingFailed;

    UPROPERTY(BlueprintAssignable, Category = "Crafting Events")
    FOnCraftingCancelled OnCraftingCancelled;

    // 레시피 관리
    UFUNCTION(BlueprintCallable, Category = "Crafting System")
    bool LoadRecipesFromDataTable();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    TArray<FHSCraftingRecipe> GetAllRecipes() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    TArray<FHSCraftingRecipe> GetRecipesByCategory(const FName& Category) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    FHSCraftingRecipe GetRecipeByID(const FName& RecipeID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    TArray<FHSCraftingRecipe> GetAvailableRecipes(AActor* Crafter) const;

    // 제작 조건 확인
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    bool CanCraftRecipe(AActor* Crafter, const FHSCraftingRecipe& Recipe) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    bool HasRequiredMaterials(UHSInventoryComponent* Inventory, const FHSCraftingRecipe& Recipe, int32 Quantity = 1) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    bool HasRequiredLevel(AActor* Crafter, const FHSCraftingRecipe& Recipe) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    bool IsRecipeUnlocked(AActor* Crafter, const FHSCraftingRecipe& Recipe) const;

    // 제작 실행
    UFUNCTION(BlueprintCallable, Category = "Crafting System")
    int32 StartCrafting(AActor* Crafter, const FName& RecipeID, int32 Quantity = 1);

    UFUNCTION(BlueprintCallable, Category = "Crafting System")
    bool CancelCrafting(int32 JobID);

    UFUNCTION(BlueprintCallable, Category = "Crafting System")
    void UpdateCraftingProgress(float DeltaTime);

    // 제작 작업 조회
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    TArray<FHSCraftingJob> GetActiveCraftingJobs(AActor* Crafter = nullptr) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    FHSCraftingJob GetCraftingJob(int32 JobID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    bool IsCrafting(AActor* Crafter) const;

    // 스킬 시스템
    UFUNCTION(BlueprintCallable, Category = "Crafting System")
    void SetCraftingSkillLevel(AActor* Crafter, const FName& SkillType, int32 Level);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    int32 GetCraftingSkillLevel(AActor* Crafter, const FName& SkillType) const;

    UFUNCTION(BlueprintCallable, Category = "Crafting System")
    void AddCraftingExperience(AActor* Crafter, const FName& SkillType, int32 Experience);

    // 유틸리티 함수
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Crafting System")
    TArray<FName> GetAllCategories() const;

    UFUNCTION(BlueprintCallable, Category = "Crafting System")
    void RefreshRecipeCache();

    // 디버그 및 개발자 도구
    UFUNCTION(BlueprintCallable, Category = "Crafting System", CallInEditor)
    void ValidateAllRecipes();

    UFUNCTION(BlueprintCallable, Category = "Crafting System", CallInEditor)
    void ExportRecipesToCSV(const FString& FilePath);

protected:
    // 내부 헬퍼 함수
    bool ConsumeMaterials(UHSInventoryComponent* Inventory, const FHSCraftingRecipe& Recipe, int32 Quantity);
    void GiveResultItems(UHSInventoryComponent* Inventory, const FHSCraftingRecipe& Recipe, int32 Quantity);
    bool RollCraftingSuccess(const FHSCraftingRecipe& Recipe, AActor* Crafter) const;
    void CompleteCraftingJob(int32 JobID);
    void FailCraftingJob(int32 JobID, const FString& Reason);
    UHSInventoryComponent* GetInventoryComponent(AActor* Actor) const;

    // 캐시 관리
    void UpdateCrafterJobsCache(AActor* Crafter, int32 JobID);
    void RemoveFromCrafterJobsCache(AActor* Crafter, int32 JobID);
    void BuildCategoryCache();

    // 성능 최적화
    void OptimizeMemoryUsage();
    void ClearExpiredJobs();

private:
    // 성능 최적화 상수
    static constexpr int32 MaxActiveJobs = 100;
    static constexpr float JobCleanupInterval = 30.0f;
    
    // 타이머 핸들
    FTimerHandle JobUpdateTimerHandle;
    FTimerHandle MemoryOptimizationTimerHandle;
};
