#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSUnlockSystem.generated.h"

// 언락 타입 열거형
UENUM(BlueprintType)
enum class EHSUnlockType : uint8
{
    CharacterClass      UMETA(DisplayName = "Character Class"),
    Weapon             UMETA(DisplayName = "Weapon"),
    Ability            UMETA(DisplayName = "Ability"),
    PermanentUpgrade   UMETA(DisplayName = "Permanent Upgrade"),
    GameMode           UMETA(DisplayName = "Game Mode"),
    Difficulty         UMETA(DisplayName = "Difficulty"),
    Cosmetic           UMETA(DisplayName = "Cosmetic"),
    Feature            UMETA(DisplayName = "Feature")
};

// 언락 조건 타입
UENUM(BlueprintType)
enum class EHSUnlockConditionType : uint8
{
    Currency           UMETA(DisplayName = "Currency"),
    Achievement        UMETA(DisplayName = "Achievement"),
    Level              UMETA(DisplayName = "Level"),
    Statistic          UMETA(DisplayName = "Statistic"),
    Dependency         UMETA(DisplayName = "Dependency"),
    Time               UMETA(DisplayName = "Time")
};

// 언락 조건 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSUnlockCondition
{
    GENERATED_BODY()

    // 조건 타입
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Condition")
    EHSUnlockConditionType ConditionType = EHSUnlockConditionType::Currency;

    // 조건 키 (화폐 이름, 업적 ID, 통계 이름 등)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Condition")
    FString ConditionKey;

    // 필요한 값
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Condition")
    int32 RequiredValue = 0;

    // 조건 설명
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Condition")
    FString Description;

    // 선택적 파라미터
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Condition")
    FString OptionalParameter;

    FHSUnlockCondition()
    {
        ConditionType = EHSUnlockConditionType::Currency;
        RequiredValue = 0;
    }

    FHSUnlockCondition(EHSUnlockConditionType InType, const FString& InKey, int32 InValue, const FString& InDescription = TEXT(""))
        : ConditionType(InType), ConditionKey(InKey), RequiredValue(InValue), Description(InDescription)
    {
    }
};

// 언락 비용 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSUnlockCost
{
    GENERATED_BODY()

    // 화폐 타입별 비용
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cost")
    TMap<FString, int32> CurrencyCosts;

    // 추가 조건들
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cost")
    TArray<FHSUnlockCondition> AdditionalConditions;

    FHSUnlockCost()
    {
        CurrencyCosts.Empty();
        AdditionalConditions.Empty();
    }

    void AddCurrencyCost(const FString& CurrencyType, int32 Cost)
    {
        if (Cost > 0)
        {
            CurrencyCosts.Add(CurrencyType, Cost);
        }
    }

    void AddCondition(const FHSUnlockCondition& Condition)
    {
        AdditionalConditions.Add(Condition);
    }

    bool IsEmpty() const
    {
        return CurrencyCosts.Num() == 0 && AdditionalConditions.Num() == 0;
    }
};

// 언락 아이템 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSUnlockItem
{
    GENERATED_BODY()

    // 기본 정보
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic")
    FString UnlockID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic")
    FString DisplayName;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic")
    FString Description;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic")
    EHSUnlockType UnlockType = EHSUnlockType::Feature;

    // 언락 비용 및 조건
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Requirements")
    FHSUnlockCost UnlockCost;

    // 선행 조건 (다른 언락 아이템들)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Requirements")
    TArray<FString> Prerequisites;

    // 상태
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "State")
    bool bIsUnlocked = false;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "State")
    bool bIsVisible = true;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "State")
    FDateTime UnlockTime;

    // 우선순위 (UI 정렬용)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI")
    int32 Priority = 0;

    // 카테고리 (UI 그룹핑용)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI")
    FString Category = TEXT("General");

    // 아이콘 경로 (UI용)
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "UI")
    FString IconPath;

    // 게임 로직 관련
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Logic")
    TMap<FString, FString> UnlockParameters;

    FHSUnlockItem()
    {
        UnlockTime = FDateTime::MinValue();
        bIsUnlocked = false;
        bIsVisible = true;
        Priority = 0;
        UnlockType = EHSUnlockType::Feature;
    }

    bool CanBeUnlocked() const
    {
        return !bIsUnlocked && bIsVisible;
    }

    void SetParameter(const FString& Key, const FString& Value)
    {
        UnlockParameters.Add(Key, Value);
    }

    FString GetParameter(const FString& Key, const FString& DefaultValue = TEXT("")) const
    {
        if (const FString* Value = UnlockParameters.Find(Key))
        {
            return *Value;
        }
        return DefaultValue;
    }
};

// 언락 카테고리 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSUnlockCategory
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Category")
    FString CategoryID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Category")
    FString DisplayName;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Category")
    FString Description;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Category")
    int32 SortOrder = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Category")
    bool bIsVisible = true;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Category")
    FString IconPath;

    FHSUnlockCategory()
    {
        SortOrder = 0;
        bIsVisible = true;
    }
};

// 언락 시스템 델리게이트들
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemUnlocked, const FString&, UnlockID, const FHSUnlockItem&, UnlockedItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnlockSystemUpdated, const TArray<FHSUnlockItem>&, AvailableUnlocks);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUnlockPurchaseAttempt, const FString&, UnlockID, bool, bSuccess);

/**
 * 로그라이크 언락 시스템
 * - 메타 진행도를 통한 영구적인 업그레이드 관리
 * - 캐릭터 클래스, 무기, 능력, 난이도 등의 언락
 * - 복잡한 언락 조건 및 의존성 처리
 * - 데이터 테이블 기반 설정 시스템
 * - 성능 최적화 및 캐싱 시스템
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSUnlockSystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSUnlockSystem();

    // USubsystem 인터페이스
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 언락 관리 메서드
    
    /**
     * 아이템을 언락합니다
     * @param UnlockID 언락할 아이템 ID
     * @return 언락 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Unlock System")
    bool UnlockItem(const FString& UnlockID);

    /**
     * 아이템이 언락되었는지 확인합니다
     * @param UnlockID 확인할 아이템 ID
     * @return 언락 여부
     */
    UFUNCTION(BlueprintPure, Category = "Unlock System")
    bool IsItemUnlocked(const FString& UnlockID) const;

    /**
     * 아이템이 언락 가능한지 확인합니다
     * @param UnlockID 확인할 아이템 ID
     * @return 언락 가능 여부
     */
    UFUNCTION(BlueprintPure, Category = "Unlock System")
    bool CanUnlockItem(const FString& UnlockID) const;

    /**
     * 언락 아이템 정보를 가져옵니다
     * @param UnlockID 아이템 ID
     * @return 언락 아이템 정보
     */
    UFUNCTION(BlueprintPure, Category = "Unlock System")
    FHSUnlockItem GetUnlockItem(const FString& UnlockID) const;

    /**
     * 특정 타입의 언락된 아이템들을 가져옵니다
     * @param UnlockType 언락 타입
     * @return 언락된 아이템 목록
     */
    UFUNCTION(BlueprintPure, Category = "Unlock System")
    TArray<FHSUnlockItem> GetUnlockedItemsByType(EHSUnlockType UnlockType) const;

    /**
     * 언락 가능한 아이템들을 가져옵니다
     * @param bIncludeUnlocked 언락된 아이템 포함 여부
     * @return 언락 가능한 아이템 목록
     */
    UFUNCTION(BlueprintPure, Category = "Unlock System")
    TArray<FHSUnlockItem> GetAvailableUnlocks(bool bIncludeUnlocked = false) const;

    /**
     * 카테고리별 언락 아이템들을 가져옵니다
     * @param CategoryID 카테고리 ID
     * @param bIncludeUnlocked 언락된 아이템 포함 여부
     * @return 카테고리 내 아이템 목록
     */
    UFUNCTION(BlueprintPure, Category = "Unlock System")
    TArray<FHSUnlockItem> GetUnlocksByCategory(const FString& CategoryID, bool bIncludeUnlocked = true) const;

    // 조건 확인 메서드
    
    /**
     * 언락 조건들을 확인합니다
     * @param Conditions 확인할 조건들
     * @return 모든 조건 충족 여부
     */
    UFUNCTION(BlueprintPure, Category = "Unlock System")
    bool CheckUnlockConditions(const TArray<FHSUnlockCondition>& Conditions) const;

    /**
     * 단일 언락 조건을 확인합니다
     * @param Condition 확인할 조건
     * @return 조건 충족 여부
     */
    UFUNCTION(BlueprintPure, Category = "Unlock System")
    bool CheckSingleCondition(const FHSUnlockCondition& Condition) const;

    /**
     * 선행 조건들을 확인합니다
     * @param Prerequisites 선행 조건 언락 ID들
     * @return 모든 선행 조건 충족 여부
     */
    UFUNCTION(BlueprintPure, Category = "Unlock System")
    bool CheckPrerequisites(const TArray<FString>& Prerequisites) const;

    /**
     * 언락 비용을 확인합니다
     * @param UnlockCost 언락 비용
     * @return 비용 지불 가능 여부
     */
    UFUNCTION(BlueprintPure, Category = "Unlock System")
    bool CanAffordUnlock(const FHSUnlockCost& UnlockCost) const;

    // 카테고리 관리
    
    /**
     * 모든 언락 카테고리를 가져옵니다
     * @return 카테고리 목록
     */
    UFUNCTION(BlueprintPure, Category = "Categories")
    TArray<FHSUnlockCategory> GetAllCategories() const;

    /**
     * 카테고리 정보를 가져옵니다
     * @param CategoryID 카테고리 ID
     * @return 카테고리 정보
     */
    UFUNCTION(BlueprintPure, Category = "Categories")
    FHSUnlockCategory GetCategory(const FString& CategoryID) const;

    // 진행률 및 통계
    
    /**
     * 전체 언락 진행률을 계산합니다
     * @return 진행률 (0.0 ~ 1.0)
     */
    UFUNCTION(BlueprintPure, Category = "Progress")
    float GetOverallProgress() const;

    /**
     * 카테고리별 언락 진행률을 계산합니다
     * @param CategoryID 카테고리 ID
     * @return 진행률 (0.0 ~ 1.0)
     */
    UFUNCTION(BlueprintPure, Category = "Progress")
    float GetCategoryProgress(const FString& CategoryID) const;

    /**
     * 언락된 아이템 수를 반환합니다
     * @return 언락된 아이템 수
     */
    UFUNCTION(BlueprintPure, Category = "Progress")
    int32 GetUnlockedItemCount() const;

    /**
     * 전체 언락 아이템 수를 반환합니다
     * @return 전체 아이템 수
     */
    UFUNCTION(BlueprintPure, Category = "Progress")
    int32 GetTotalItemCount() const;

    // 데이터 관리
    
    /**
     * 언락 데이터를 새로고침합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Data Management")
    void RefreshUnlockData();

    /**
     * 언락 아이템을 동적으로 추가합니다
     * @param UnlockItem 추가할 언락 아이템
     */
    UFUNCTION(BlueprintCallable, Category = "Data Management")
    void AddUnlockItem(const FHSUnlockItem& UnlockItem);

    /**
     * 언락 아이템을 제거합니다
     * @param UnlockID 제거할 아이템 ID
     */
    UFUNCTION(BlueprintCallable, Category = "Data Management")
    void RemoveUnlockItem(const FString& UnlockID);

    /**
     * 언락 아이템의 가시성을 설정합니다
     * @param UnlockID 아이템 ID
     * @param bVisible 가시성 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Data Management")
    void SetItemVisibility(const FString& UnlockID, bool bVisible);

    // 저장/로드
    
    /**
     * 언락 상태를 저장합니다
     * @return 저장 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool SaveUnlockState();

    /**
     * 언락 상태를 로드합니다
     * @return 로드 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool LoadUnlockState();

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnItemUnlocked OnItemUnlocked;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnUnlockSystemUpdated OnUnlockSystemUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnUnlockPurchaseAttempt OnUnlockPurchaseAttempt;

protected:
    // 언락 데이터
    UPROPERTY(BlueprintReadOnly, Category = "Data")
    TMap<FString, FHSUnlockItem> UnlockItems;

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    TMap<FString, FHSUnlockCategory> Categories;

    // 설정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    TSoftObjectPtr<UDataTable> UnlockDataTable;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    TSoftObjectPtr<UDataTable> CategoryDataTable;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    FString UnlockSaveFileName = TEXT("HuntingSpiritUnlocks");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    bool bAutoSaveOnUnlock = true;

    // 캐싱 (성능 최적화)
    mutable TMap<FString, bool> CachedConditionResults;
    mutable TMap<FString, TArray<FHSUnlockItem>> CachedCategoryResults;
    mutable float CachedOverallProgress = -1.0f;
    mutable int32 CachedUnlockCount = -1;

private:
    // 내부 메서드들
    
    /**
     * 기본 언락 데이터를 초기화합니다
     */
    void InitializeDefaultUnlocks();

    /**
     * 데이터 테이블에서 언락 데이터를 로드합니다
     */
    void LoadUnlockDataFromTable();

    /**
     * 카테고리 데이터를 로드합니다
     */
    void LoadCategoryData();

    /**
     * 언락 비용을 지불합니다
     * @param UnlockCost 지불할 비용
     * @return 지불 성공 여부
     */
    bool PayUnlockCost(const FHSUnlockCost& UnlockCost);

    /**
     * 언락 아이템을 정렬합니다
     * @param Items 정렬할 아이템들
     */
    void SortUnlockItems(TArray<FHSUnlockItem>& Items) const;

    /**
     * 의존성 그래프를 검증합니다
     * @return 순환 의존성이 없으면 true
     */
    bool ValidateDependencyGraph() const;

    /**
     * 캐시를 무효화합니다
     */
    void InvalidateCache();

    /**
     * 조건 결과를 캐시합니다
     * @param ConditionKey 조건 키
     * @param Result 결과
     */
    void CacheConditionResult(const FString& ConditionKey, bool Result) const;

    /**
     * 캐시된 조건 결과를 가져옵니다
     * @param ConditionKey 조건 키
     * @param OutResult 결과 출력
     * @return 캐시된 결과가 있으면 true
     */
    bool GetCachedConditionResult(const FString& ConditionKey, bool& OutResult) const;

    /**
     * 조건 키를 생성합니다
     * @param Condition 조건
     * @return 조건 키
     */
    FString GenerateConditionKey(const FHSUnlockCondition& Condition) const;

    /**
     * 의존성 그래프 검증 헬퍼 함수
     * @param UnlockID 확인할 언락 ID
     * @param Visited 방문한 노드들
     * @param RecursionStack 재귀 스택
     * @return 순환 의존성이 없으면 true
     */
    bool ValidateDependencyGraphHelper(const FString& UnlockID, TSet<FString>& Visited, TSet<FString>& RecursionStack) const;
};
