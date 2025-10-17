#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "HSCraftingSystem.h"
#include "../../Items/HSItemBase.h"
#include "HSRecipeDatabase.generated.h"

class UHSItemInstance;

/**
 * 레시피 그룹 정보
 * 연관된 레시피들을 그룹화하여 관리
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSRecipeGroup
{
    GENERATED_BODY()

    // 그룹 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName GroupName;

    // 그룹 설명
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText GroupDescription;

    // 그룹에 속한 레시피들
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> RecipeIDs;

    // 그룹 언락 조건
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> UnlockConditions;

    // 그룹 우선순위 (정렬용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Priority;

    FHSRecipeGroup()
        : Priority(0)
    {
    }
};

/**
 * 제작 카테고리 정보
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSCraftingCategory
{
    GENERATED_BODY()

    // 카테고리 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName CategoryName;

    // 카테고리 표시 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    // 카테고리 설명
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Description;

    // 카테고리 아이콘
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UTexture2D> CategoryIcon;

    // 부모 카테고리 (계층 구조)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ParentCategory;

    // 정렬 순서
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SortOrder;

    FHSCraftingCategory()
        : SortOrder(0)
    {
    }
};

/**
 * 제작 재료 템플릿
 * 자주 사용되는 재료 조합을 템플릿화
 */
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSMaterialTemplate
{
    GENERATED_BODY()

    // 템플릿 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName TemplateName;

    // 재료 목록
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FHSCraftingMaterial> Materials;

    // 템플릿 설명
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Description;
};

/**
 * 레시피 데이터베이스
 * 게임의 모든 제작 레시피와 관련 데이터를 관리하는 데이터 에셋
 * 지연 로딩과 캐싱을 활용해 메모리를 효율적으로 사용합니다
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSRecipeDatabase : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UHSRecipeDatabase();

protected:
    // 레시피 데이터 테이블
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recipe Data")
    TSoftObjectPtr<UDataTable> RecipeDataTable;

    // 카테고리 정보
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Categories")
    TArray<FHSCraftingCategory> Categories;

    // 레시피 그룹 정보
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recipe Groups")
    TArray<FHSRecipeGroup> RecipeGroups;

    // 재료 템플릿
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Templates")
    TArray<FHSMaterialTemplate> MaterialTemplates;

    // 기본 제작 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings")
    float DefaultCraftingTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings")
    float DefaultSuccessRate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default Settings")
    int32 DefaultRequiredLevel;

    // 캐시된 데이터 (런타임에서만 사용)
    UPROPERTY(Transient)
    TMap<FName, FHSCraftingRecipe> CachedRecipes;

    UPROPERTY(Transient)
    TMap<FName, FHSCraftingCategory> CachedCategories;

    UPROPERTY(Transient)
    TMap<FName, FHSRecipeGroup> CachedGroups;

    UPROPERTY(Transient)
    bool bDataLoaded;

public:
    // 데이터 로딩 및 초기화
    UFUNCTION(BlueprintCallable, Category = "Recipe Database")
    bool LoadAllData();

    UFUNCTION(BlueprintCallable, Category = "Recipe Database")
    void ClearCache();

    UFUNCTION(BlueprintCallable, Category = "Recipe Database")
    bool IsDataLoaded() const { return bDataLoaded; }

    // 레시피 조회
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    FHSCraftingRecipe GetRecipe(const FName& RecipeID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    TArray<FHSCraftingRecipe> GetAllRecipes() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    TArray<FHSCraftingRecipe> GetRecipesByCategory(const FName& CategoryName) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    TArray<FHSCraftingRecipe> GetRecipesByGroup(const FName& GroupName) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    TArray<FHSCraftingRecipe> SearchRecipes(const FString& SearchTerm) const;

    // 카테고리 조회
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    FHSCraftingCategory GetCategory(const FName& CategoryName) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    TArray<FHSCraftingCategory> GetAllCategories() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    TArray<FHSCraftingCategory> GetRootCategories() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    TArray<FHSCraftingCategory> GetSubCategories(const FName& ParentCategory) const;

    // 그룹 조회
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    FHSRecipeGroup GetRecipeGroup(const FName& GroupName) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    TArray<FHSRecipeGroup> GetAllRecipeGroups() const;

    // 재료 템플릿 조회
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    FHSMaterialTemplate GetMaterialTemplate(const FName& TemplateName) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    TArray<FHSMaterialTemplate> GetAllMaterialTemplates() const;

    // 유틸리티 함수
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    TArray<FName> GetRecipeIDsByResultItem(UHSItemInstance* ResultItem) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    TArray<FName> GetRecipeIDsByMaterial(UHSItemInstance* Material) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    bool DoesRecipeExist(const FName& RecipeID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Recipe Database")
    int32 GetRecipeCount() const;

    // 에디터 전용 함수
    UFUNCTION(BlueprintCallable, Category = "Recipe Database", CallInEditor)
    void ValidateDatabase();

    UFUNCTION(BlueprintCallable, Category = "Recipe Database", CallInEditor)
    void GenerateRecipeReport();

    UFUNCTION(BlueprintCallable, Category = "Recipe Database", CallInEditor)
    void ExportToJSON(const FString& FilePath);

    UFUNCTION(BlueprintCallable, Category = "Recipe Database", CallInEditor)
    bool ImportFromJSON(const FString& FilePath);

    // 런타임 최적화
    UFUNCTION(BlueprintCallable, Category = "Recipe Database")
    void PreloadFrequentlyUsedRecipes(const TArray<FName>& RecipeIDs);

    UFUNCTION(BlueprintCallable, Category = "Recipe Database")
    void OptimizeMemoryUsage();

protected:
    // 내부 헬퍼 함수
    void LoadRecipesFromDataTable();
    void BuildCategoryCache();
    void BuildGroupCache();
    bool ValidateRecipe(const FHSCraftingRecipe& Recipe, FString& OutErrorMessage) const;
    bool ValidateCategory(const FHSCraftingCategory& Category, FString& OutErrorMessage) const;
    bool ValidateGroup(const FHSRecipeGroup& Group, FString& OutErrorMessage) const;

    // 성능 최적화 함수
    void AsyncLoadRecipeData();
    void CacheFrequentlyAccessedData();

private:
    // 성능 최적화를 위한 정적 캐시
    static TMap<FName, TWeakObjectPtr<UHSRecipeDatabase>> DatabaseCache;
    
    // 메모리 관리
    void CleanupUnusedReferences();
};

/**
 * 레시피 데이터베이스 매니저
 * 여러 데이터베이스를 관리하고 통합된 인터페이스 제공
 */
UCLASS(BlueprintType)
class HUNTINGSPIRIT_API UHSRecipeDatabaseManager : public UObject
{
    GENERATED_BODY()

public:
    UHSRecipeDatabaseManager();

protected:
    // 등록된 데이터베이스들
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Database Manager")
    TArray<TSoftObjectPtr<UHSRecipeDatabase>> RegisteredDatabases;

    // 활성 데이터베이스
    UPROPERTY(BlueprintReadOnly, Category = "Database Manager")
    TObjectPtr<UHSRecipeDatabase> ActiveDatabase;

    // 로드된 데이터베이스 캐시
    UPROPERTY(Transient)
    TArray<TObjectPtr<UHSRecipeDatabase>> LoadedDatabases;

public:
    // 데이터베이스 관리
    UFUNCTION(BlueprintCallable, Category = "Database Manager")
    bool LoadDatabase(TSoftObjectPtr<UHSRecipeDatabase> DatabaseAsset);

    UFUNCTION(BlueprintCallable, Category = "Database Manager")
    void SetActiveDatabase(UHSRecipeDatabase* Database);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Database Manager")
    UHSRecipeDatabase* GetActiveDatabase() const { return ActiveDatabase; }

    UFUNCTION(BlueprintCallable, Category = "Database Manager")
    void LoadAllRegisteredDatabases();

    // 통합 조회 인터페이스
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Database Manager")
    FHSCraftingRecipe FindRecipeInAllDatabases(const FName& RecipeID) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Database Manager")
    TArray<FHSCraftingRecipe> GetAllRecipesFromAllDatabases() const;

    // 성능 최적화
    UFUNCTION(BlueprintCallable, Category = "Database Manager")
    void OptimizeAllDatabases();

    UFUNCTION(BlueprintCallable, Category = "Database Manager")
    void PreloadCriticalData();

protected:
    void OnDatabaseLoaded(UHSRecipeDatabase* LoadedDatabase);
};
