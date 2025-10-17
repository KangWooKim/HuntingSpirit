#include "HSRecipeDatabase.h"
#include "../../Items/HSItemBase.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformFilemanager.h"

// 정적 멤버 초기화
TMap<FName, TWeakObjectPtr<UHSRecipeDatabase>> UHSRecipeDatabase::DatabaseCache;

UHSRecipeDatabase::UHSRecipeDatabase()
    : DefaultCraftingTime(5.0f)
    , DefaultSuccessRate(1.0f)
    , DefaultRequiredLevel(1)
    , bDataLoaded(false)
{
}

bool UHSRecipeDatabase::LoadAllData()
{
    if (bDataLoaded)
    {
        return true;
    }

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabase::LoadAllData - 데이터베이스 로딩 시작"));

    try
    {
        // 레시피 데이터 로드
        LoadRecipesFromDataTable();

        // 캐시 구축
        BuildCategoryCache();
        BuildGroupCache();

        // 자주 사용되는 데이터 캐시
        CacheFrequentlyAccessedData();

        bDataLoaded = true;
        UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabase::LoadAllData - 데이터베이스 로딩 완료"));
        return true;
    }
    catch (const std::exception&)
    {
        UE_LOG(LogTemp, Error, TEXT("HSRecipeDatabase::LoadAllData - 데이터베이스 로딩 실패"));
        return false;
    }
}

void UHSRecipeDatabase::ClearCache()
{
    CachedRecipes.Empty();
    CachedCategories.Empty();
    CachedGroups.Empty();
    bDataLoaded = false;

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabase::ClearCache - 캐시 클리어 완료"));
}

FHSCraftingRecipe UHSRecipeDatabase::GetRecipe(const FName& RecipeID) const
{
    if (const FHSCraftingRecipe* Recipe = CachedRecipes.Find(RecipeID))
    {
        return *Recipe;
    }

    UE_LOG(LogTemp, Warning, TEXT("HSRecipeDatabase::GetRecipe - 레시피를 찾을 수 없음: %s"), *RecipeID.ToString());
    return FHSCraftingRecipe();
}

TArray<FHSCraftingRecipe> UHSRecipeDatabase::GetAllRecipes() const
{
    TArray<FHSCraftingRecipe> AllRecipes;
    CachedRecipes.GenerateValueArray(AllRecipes);
    return AllRecipes;
}

TArray<FHSCraftingRecipe> UHSRecipeDatabase::GetRecipesByCategory(const FName& CategoryName) const
{
    TArray<FHSCraftingRecipe> CategoryRecipes;

    for (const auto& RecipePair : CachedRecipes)
    {
        if (RecipePair.Value.Category == CategoryName)
        {
            CategoryRecipes.Add(RecipePair.Value);
        }
    }

    // 이름순으로 정렬
    CategoryRecipes.Sort([](const FHSCraftingRecipe& A, const FHSCraftingRecipe& B)
    {
        return A.RecipeName.ToString() < B.RecipeName.ToString();
    });

    return CategoryRecipes;
}

TArray<FHSCraftingRecipe> UHSRecipeDatabase::GetRecipesByGroup(const FName& GroupName) const
{
    TArray<FHSCraftingRecipe> GroupRecipes;

    if (const FHSRecipeGroup* Group = CachedGroups.Find(GroupName))
    {
        for (const FName& RecipeID : Group->RecipeIDs)
        {
            if (const FHSCraftingRecipe* Recipe = CachedRecipes.Find(RecipeID))
            {
                GroupRecipes.Add(*Recipe);
            }
        }
    }

    return GroupRecipes;
}

TArray<FHSCraftingRecipe> UHSRecipeDatabase::SearchRecipes(const FString& SearchTerm) const
{
    TArray<FHSCraftingRecipe> SearchResults;

    if (SearchTerm.IsEmpty())
    {
        return SearchResults;
    }

    FString LowerSearchTerm = SearchTerm.ToLower();

    for (const auto& RecipePair : CachedRecipes)
    {
        const FHSCraftingRecipe& Recipe = RecipePair.Value;
        
        // 이름에서 검색
        if (Recipe.RecipeName.ToString().ToLower().Contains(LowerSearchTerm))
        {
            SearchResults.Add(Recipe);
            continue;
        }

        // 설명에서 검색
        if (Recipe.RecipeDescription.ToString().ToLower().Contains(LowerSearchTerm))
        {
            SearchResults.Add(Recipe);
            continue;
        }

        // 카테고리에서 검색
        if (Recipe.Category.ToString().ToLower().Contains(LowerSearchTerm))
        {
            SearchResults.Add(Recipe);
            continue;
        }

        // 결과 아이템 이름에서 검색
        if (UHSItemInstance* ResultItem = Recipe.ResultItem.LoadSynchronous())
        {
            if (ResultItem->GetItemName().ToLower().Contains(LowerSearchTerm))
            {
                SearchResults.Add(Recipe);
                continue;
            }
        }
    }

    return SearchResults;
}

FHSCraftingCategory UHSRecipeDatabase::GetCategory(const FName& CategoryName) const
{
    if (const FHSCraftingCategory* Category = CachedCategories.Find(CategoryName))
    {
        return *Category;
    }

    return FHSCraftingCategory();
}

TArray<FHSCraftingCategory> UHSRecipeDatabase::GetAllCategories() const
{
    TArray<FHSCraftingCategory> AllCategories;
    CachedCategories.GenerateValueArray(AllCategories);

    // 정렬 순서에 따라 정렬
    AllCategories.Sort([](const FHSCraftingCategory& A, const FHSCraftingCategory& B)
    {
        return A.SortOrder < B.SortOrder;
    });

    return AllCategories;
}

TArray<FHSCraftingCategory> UHSRecipeDatabase::GetRootCategories() const
{
    TArray<FHSCraftingCategory> RootCategories;

    for (const auto& CategoryPair : CachedCategories)
    {
        if (CategoryPair.Value.ParentCategory.IsNone())
        {
            RootCategories.Add(CategoryPair.Value);
        }
    }

    // 정렬 순서에 따라 정렬
    RootCategories.Sort([](const FHSCraftingCategory& A, const FHSCraftingCategory& B)
    {
        return A.SortOrder < B.SortOrder;
    });

    return RootCategories;
}

TArray<FHSCraftingCategory> UHSRecipeDatabase::GetSubCategories(const FName& ParentCategory) const
{
    TArray<FHSCraftingCategory> SubCategories;

    for (const auto& CategoryPair : CachedCategories)
    {
        if (CategoryPair.Value.ParentCategory == ParentCategory)
        {
            SubCategories.Add(CategoryPair.Value);
        }
    }

    // 정렬 순서에 따라 정렬
    SubCategories.Sort([](const FHSCraftingCategory& A, const FHSCraftingCategory& B)
    {
        return A.SortOrder < B.SortOrder;
    });

    return SubCategories;
}

FHSRecipeGroup UHSRecipeDatabase::GetRecipeGroup(const FName& GroupName) const
{
    if (const FHSRecipeGroup* Group = CachedGroups.Find(GroupName))
    {
        return *Group;
    }

    return FHSRecipeGroup();
}

TArray<FHSRecipeGroup> UHSRecipeDatabase::GetAllRecipeGroups() const
{
    TArray<FHSRecipeGroup> AllGroups;
    CachedGroups.GenerateValueArray(AllGroups);

    // 우선순위에 따라 정렬
    AllGroups.Sort([](const FHSRecipeGroup& A, const FHSRecipeGroup& B)
    {
        return A.Priority > B.Priority;
    });

    return AllGroups;
}

FHSMaterialTemplate UHSRecipeDatabase::GetMaterialTemplate(const FName& TemplateName) const
{
    for (const FHSMaterialTemplate& Template : MaterialTemplates)
    {
        if (Template.TemplateName == TemplateName)
        {
            return Template;
        }
    }

    return FHSMaterialTemplate();
}

TArray<FHSMaterialTemplate> UHSRecipeDatabase::GetAllMaterialTemplates() const
{
    return MaterialTemplates;
}

TArray<FName> UHSRecipeDatabase::GetRecipeIDsByResultItem(UHSItemInstance* ResultItem) const
{
    TArray<FName> RecipeIDs;

    if (!ResultItem)
    {
        return RecipeIDs;
    }

    for (const auto& RecipePair : CachedRecipes)
    {
        const FHSCraftingRecipe& Recipe = RecipePair.Value;
        UHSItemInstance* RecipeResultItem = Recipe.ResultItem.LoadSynchronous();
        
        if (RecipeResultItem && RecipeResultItem->GetClass() == ResultItem->GetClass())
        {
            RecipeIDs.Add(Recipe.RecipeID);
        }
    }

    return RecipeIDs;
}

TArray<FName> UHSRecipeDatabase::GetRecipeIDsByMaterial(UHSItemInstance* Material) const
{
    TArray<FName> RecipeIDs;

    if (!Material)
    {
        return RecipeIDs;
    }

    for (const auto& RecipePair : CachedRecipes)
    {
        const FHSCraftingRecipe& Recipe = RecipePair.Value;
        
        for (const FHSCraftingMaterial& RecipeMaterial : Recipe.RequiredMaterials)
        {
            UHSItemInstance* RequiredItem = RecipeMaterial.RequiredItem.LoadSynchronous();
            if (RequiredItem && RequiredItem->GetClass() == Material->GetClass())
            {
                RecipeIDs.Add(Recipe.RecipeID);
                break;
            }
        }
    }

    return RecipeIDs;
}

bool UHSRecipeDatabase::DoesRecipeExist(const FName& RecipeID) const
{
    return CachedRecipes.Contains(RecipeID);
}

int32 UHSRecipeDatabase::GetRecipeCount() const
{
    return CachedRecipes.Num();
}

void UHSRecipeDatabase::ValidateDatabase()
{
    int32 ValidRecipes = 0;
    int32 InvalidRecipes = 0;
    int32 ValidCategories = 0;
    int32 InvalidCategories = 0;
    int32 ValidGroups = 0;
    int32 InvalidGroups = 0;

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabase::ValidateDatabase - 데이터베이스 검증 시작"));

    // 레시피 검증
    for (const auto& RecipePair : CachedRecipes)
    {
        FString ErrorMessage;
        if (ValidateRecipe(RecipePair.Value, ErrorMessage))
        {
            ValidRecipes++;
        }
        else
        {
            InvalidRecipes++;
            UE_LOG(LogTemp, Error, TEXT("유효하지 않은 레시피 %s: %s"), 
                   *RecipePair.Key.ToString(), *ErrorMessage);
        }
    }

    // 카테고리 검증
    for (const auto& CategoryPair : CachedCategories)
    {
        FString ErrorMessage;
        if (ValidateCategory(CategoryPair.Value, ErrorMessage))
        {
            ValidCategories++;
        }
        else
        {
            InvalidCategories++;
            UE_LOG(LogTemp, Error, TEXT("유효하지 않은 카테고리 %s: %s"), 
                   *CategoryPair.Key.ToString(), *ErrorMessage);
        }
    }

    // 그룹 검증
    for (const auto& GroupPair : CachedGroups)
    {
        FString ErrorMessage;
        if (ValidateGroup(GroupPair.Value, ErrorMessage))
        {
            ValidGroups++;
        }
        else
        {
            InvalidGroups++;
            UE_LOG(LogTemp, Error, TEXT("유효하지 않은 그룹 %s: %s"), 
                   *GroupPair.Key.ToString(), *ErrorMessage);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabase::ValidateDatabase - 검증 완료"));
    UE_LOG(LogTemp, Log, TEXT("레시피: 유효 %d, 무효 %d"), ValidRecipes, InvalidRecipes);
    UE_LOG(LogTemp, Log, TEXT("카테고리: 유효 %d, 무효 %d"), ValidCategories, InvalidCategories);
    UE_LOG(LogTemp, Log, TEXT("그룹: 유효 %d, 무효 %d"), ValidGroups, InvalidGroups);
}

void UHSRecipeDatabase::GenerateRecipeReport()
{
    FString ReportContent;
    ReportContent += TEXT("=== HuntingSpirit 제작 레시피 보고서 ===\n");
    ReportContent += FString::Printf(TEXT("생성 시간: %s\n"), *FDateTime::Now().ToString());
    ReportContent += FString::Printf(TEXT("총 레시피 수: %d\n"), CachedRecipes.Num());
    ReportContent += FString::Printf(TEXT("총 카테고리 수: %d\n"), CachedCategories.Num());
    ReportContent += FString::Printf(TEXT("총 그룹 수: %d\n"), CachedGroups.Num());
    ReportContent += TEXT("\n");

    // 카테고리별 레시피 수
    ReportContent += TEXT("=== 카테고리별 레시피 분포 ===\n");
    TMap<FName, int32> CategoryCount;
    for (const auto& RecipePair : CachedRecipes)
    {
        int32& Count = CategoryCount.FindOrAdd(RecipePair.Value.Category);
        Count++;
    }

    for (const auto& CountPair : CategoryCount)
    {
        ReportContent += FString::Printf(TEXT("%s: %d개\n"), 
                                       *CountPair.Key.ToString(), CountPair.Value);
    }

    ReportContent += TEXT("\n");

    // 제작 시간 통계
    ReportContent += TEXT("=== 제작 시간 통계 ===\n");
    float TotalTime = 0.0f;
    float MinTime = FLT_MAX;
    float MaxTime = 0.0f;
    
    for (const auto& RecipePair : CachedRecipes)
    {
        float Time = RecipePair.Value.CraftingTime;
        TotalTime += Time;
        MinTime = FMath::Min(MinTime, Time);
        MaxTime = FMath::Max(MaxTime, Time);
    }

    float AverageTime = CachedRecipes.Num() > 0 ? TotalTime / CachedRecipes.Num() : 0.0f;
    ReportContent += FString::Printf(TEXT("평균 제작 시간: %.2f초\n"), AverageTime);
    ReportContent += FString::Printf(TEXT("최소 제작 시간: %.2f초\n"), MinTime);
    ReportContent += FString::Printf(TEXT("최대 제작 시간: %.2f초\n"), MaxTime);

    // 파일로 저장
    FString FilePath = FPaths::ProjectSavedDir() / TEXT("RecipeReport.txt");
    FFileHelper::SaveStringToFile(ReportContent, *FilePath);

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabase::GenerateRecipeReport - 보고서 생성 완료: %s"), *FilePath);
}

void UHSRecipeDatabase::ExportToJSON(const FString& FilePath)
{
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);

    // 레시피 데이터 내보내기
    TArray<TSharedPtr<FJsonValue>> RecipeArray;
    for (const auto& RecipePair : CachedRecipes)
    {
        const FHSCraftingRecipe& Recipe = RecipePair.Value;
        TSharedPtr<FJsonObject> RecipeObject = MakeShareable(new FJsonObject);

        RecipeObject->SetStringField(TEXT("RecipeID"), Recipe.RecipeID.ToString());
        RecipeObject->SetStringField(TEXT("RecipeName"), Recipe.RecipeName.ToString());
        RecipeObject->SetStringField(TEXT("Description"), Recipe.RecipeDescription.ToString());
        RecipeObject->SetNumberField(TEXT("CraftingTime"), Recipe.CraftingTime);
        RecipeObject->SetNumberField(TEXT("RequiredLevel"), Recipe.RequiredCraftingLevel);
        RecipeObject->SetNumberField(TEXT("SuccessRate"), Recipe.SuccessRate);
        RecipeObject->SetStringField(TEXT("Category"), Recipe.Category.ToString());

        // 재료 배열
        TArray<TSharedPtr<FJsonValue>> MaterialArray;
        for (const FHSCraftingMaterial& Material : Recipe.RequiredMaterials)
        {
            TSharedPtr<FJsonObject> MaterialObject = MakeShareable(new FJsonObject);
            MaterialObject->SetStringField(TEXT("Item"), Material.RequiredItem.ToString());
            MaterialObject->SetNumberField(TEXT("Quantity"), Material.RequiredQuantity);
            MaterialObject->SetBoolField(TEXT("IsConsumed"), Material.bIsConsumed);

            MaterialArray.Add(MakeShareable(new FJsonValueObject(MaterialObject)));
        }
        RecipeObject->SetArrayField(TEXT("Materials"), MaterialArray);

        RecipeArray.Add(MakeShareable(new FJsonValueObject(RecipeObject)));
    }
    RootObject->SetArrayField(TEXT("Recipes"), RecipeArray);

    // JSON 문자열로 변환
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    // 파일로 저장
    FFileHelper::SaveStringToFile(OutputString, *FilePath);

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabase::ExportToJSON - JSON 내보내기 완료: %s"), *FilePath);
}

bool UHSRecipeDatabase::ImportFromJSON(const FString& FilePath)
{
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("HSRecipeDatabase::ImportFromJSON - 파일 읽기 실패: %s"), *FilePath);
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("HSRecipeDatabase::ImportFromJSON - JSON 파싱 실패"));
        return false;
    }

    // 기존 캐시 클리어
    CachedRecipes.Empty();

    // 레시피 데이터 가져오기
    const TArray<TSharedPtr<FJsonValue>>* RecipeArray;
    if (RootObject->TryGetArrayField(TEXT("Recipes"), RecipeArray))
    {
        for (const TSharedPtr<FJsonValue>& RecipeValue : *RecipeArray)
        {
            const TSharedPtr<FJsonObject>* RecipeObject;
            if (RecipeValue->TryGetObject(RecipeObject))
            {
                FHSCraftingRecipe Recipe;
                
                FString RecipeID;
                if ((*RecipeObject)->TryGetStringField(TEXT("RecipeID"), RecipeID))
                {
                    Recipe.RecipeID = FName(*RecipeID);
                }

                FString RecipeName;
                if ((*RecipeObject)->TryGetStringField(TEXT("RecipeName"), RecipeName))
                {
                    Recipe.RecipeName = FText::FromString(RecipeName);
                }

                (*RecipeObject)->TryGetNumberField(TEXT("CraftingTime"), Recipe.CraftingTime);
                (*RecipeObject)->TryGetNumberField(TEXT("RequiredLevel"), Recipe.RequiredCraftingLevel);
                (*RecipeObject)->TryGetNumberField(TEXT("SuccessRate"), Recipe.SuccessRate);

                // 유효한 레시피면 캐시에 추가
                if (Recipe.IsValid())
                {
                    CachedRecipes.Add(Recipe.RecipeID, Recipe);
                }
            }
        }
    }

    bDataLoaded = true;
    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabase::ImportFromJSON - JSON 가져오기 완료: %d개 레시피"), 
           CachedRecipes.Num());
    return true;
}

void UHSRecipeDatabase::PreloadFrequentlyUsedRecipes(const TArray<FName>& RecipeIDs)
{
    for (const FName& RecipeID : RecipeIDs)
    {
        if (const FHSCraftingRecipe* Recipe = CachedRecipes.Find(RecipeID))
        {
            // 결과 아이템 미리 로드
            if (!Recipe->ResultItem.IsNull())
            {
                Recipe->ResultItem.LoadSynchronous();
            }

            // 재료 아이템들 미리 로드
            for (const FHSCraftingMaterial& Material : Recipe->RequiredMaterials)
            {
                if (!Material.RequiredItem.IsNull())
                {
                    Material.RequiredItem.LoadSynchronous();
                }
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabase::PreloadFrequentlyUsedRecipes - %d개 레시피 미리 로드 완료"), 
           RecipeIDs.Num());
}

void UHSRecipeDatabase::OptimizeMemoryUsage()
{
    // 사용되지 않는 참조 정리
    CleanupUnusedReferences();

    // 가비지 컬렉션 요청
    if (GEngine)
    {
        GEngine->ForceGarbageCollection(true);
    }

    UE_LOG(LogTemp, VeryVerbose, TEXT("HSRecipeDatabase::OptimizeMemoryUsage - 메모리 최적화 완료"));
}

// 내부 헬퍼 함수들
void UHSRecipeDatabase::LoadRecipesFromDataTable()
{
    if (RecipeDataTable.IsNull())
    {
        UE_LOG(LogTemp, Warning, TEXT("HSRecipeDatabase::LoadRecipesFromDataTable - 데이터 테이블이 설정되지 않음"));
        return;
    }

    UDataTable* DataTable = RecipeDataTable.LoadSynchronous();
    if (!DataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("HSRecipeDatabase::LoadRecipesFromDataTable - 데이터 테이블 로드 실패"));
        return;
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
            UE_LOG(LogTemp, Warning, TEXT("HSRecipeDatabase::LoadRecipesFromDataTable - 유효하지 않은 레시피: %s"), 
                   *RowName.ToString());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabase::LoadRecipesFromDataTable - %d개 레시피 로드 완료"), 
           CachedRecipes.Num());
}

void UHSRecipeDatabase::BuildCategoryCache()
{
    CachedCategories.Empty();

    for (const FHSCraftingCategory& Category : Categories)
    {
        if (!Category.CategoryName.IsNone())
        {
            CachedCategories.Add(Category.CategoryName, Category);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabase::BuildCategoryCache - %d개 카테고리 캐시 구축 완료"), 
           CachedCategories.Num());
}

void UHSRecipeDatabase::BuildGroupCache()
{
    CachedGroups.Empty();

    for (const FHSRecipeGroup& Group : RecipeGroups)
    {
        if (!Group.GroupName.IsNone())
        {
            CachedGroups.Add(Group.GroupName, Group);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabase::BuildGroupCache - %d개 그룹 캐시 구축 완료"), 
           CachedGroups.Num());
}

bool UHSRecipeDatabase::ValidateRecipe(const FHSCraftingRecipe& Recipe, FString& OutErrorMessage) const
{
    if (!Recipe.IsValid())
    {
        OutErrorMessage = TEXT("기본 유효성 검사 실패");
        return false;
    }

    if (Recipe.CraftingTime <= 0.0f)
    {
        OutErrorMessage = TEXT("제작 시간이 0 이하");
        return false;
    }

    if (Recipe.SuccessRate < 0.0f || Recipe.SuccessRate > 1.0f)
    {
        OutErrorMessage = TEXT("성공률이 0~1 범위를 벗어남");
        return false;
    }

    if (Recipe.RequiredCraftingLevel < 1)
    {
        OutErrorMessage = TEXT("필요 레벨이 1 미만");
        return false;
    }

    return true;
}

bool UHSRecipeDatabase::ValidateCategory(const FHSCraftingCategory& Category, FString& OutErrorMessage) const
{
    if (Category.CategoryName.IsNone())
    {
        OutErrorMessage = TEXT("카테고리 이름이 없음");
        return false;
    }

    if (Category.DisplayName.IsEmpty())
    {
        OutErrorMessage = TEXT("표시 이름이 없음");
        return false;
    }

    return true;
}

bool UHSRecipeDatabase::ValidateGroup(const FHSRecipeGroup& Group, FString& OutErrorMessage) const
{
    if (Group.GroupName.IsNone())
    {
        OutErrorMessage = TEXT("그룹 이름이 없음");
        return false;
    }

    if (Group.RecipeIDs.Num() == 0)
    {
        OutErrorMessage = TEXT("그룹에 레시피가 없음");
        return false;
    }

    // 그룹의 모든 레시피가 실제로 존재하는지 확인
    for (const FName& RecipeID : Group.RecipeIDs)
    {
        if (!CachedRecipes.Contains(RecipeID))
        {
            OutErrorMessage = FString::Printf(TEXT("존재하지 않는 레시피 참조: %s"), *RecipeID.ToString());
            return false;
        }
    }

    return true;
}

void UHSRecipeDatabase::AsyncLoadRecipeData()
{
    if (bDataLoaded)
    {
        return;
    }

    if (RecipeDataTable.IsNull())
    {
        LoadAllData();
        return;
    }

    const FSoftObjectPath DataTablePath = RecipeDataTable.ToSoftObjectPath();
    if (!DataTablePath.IsValid())
    {
        LoadAllData();
        return;
    }

    FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
    TWeakObjectPtr<UHSRecipeDatabase> WeakThis(this);

    Streamable.RequestAsyncLoad(DataTablePath, FStreamableDelegate::CreateLambda([WeakThis]()
    {
        if (!WeakThis.IsValid())
        {
            return;
        }

        WeakThis->LoadAllData();
    }));
}

void UHSRecipeDatabase::CacheFrequentlyAccessedData()
{
    // 자주 사용되는 데이터 미리 로드
    // 예: 기본 제작 레시피들의 아이템 에셋 미리 로드
    
    TArray<FName> FrequentRecipes = {
        TEXT("BasicSword"),
        TEXT("IronPickaxe"),
        TEXT("HealthPotion"),
        TEXT("Torch")
    };

    PreloadFrequentlyUsedRecipes(FrequentRecipes);
}

void UHSRecipeDatabase::CleanupUnusedReferences()
{
    // 사용되지 않는 약한 참조들 정리
    for (auto It = DatabaseCache.CreateIterator(); It; ++It)
    {
        if (!It.Value().IsValid())
        {
            It.RemoveCurrent();
        }
    }
}

// UHSRecipeDatabaseManager 구현
UHSRecipeDatabaseManager::UHSRecipeDatabaseManager()
    : ActiveDatabase(nullptr)
{
}

bool UHSRecipeDatabaseManager::LoadDatabase(TSoftObjectPtr<UHSRecipeDatabase> DatabaseAsset)
{
    if (DatabaseAsset.IsNull())
    {
        return false;
    }

    UHSRecipeDatabase* LoadedDatabase = DatabaseAsset.LoadSynchronous();
    if (!LoadedDatabase)
    {
        UE_LOG(LogTemp, Error, TEXT("HSRecipeDatabaseManager::LoadDatabase - 데이터베이스 로드 실패"));
        return false;
    }

    if (!LoadedDatabase->LoadAllData())
    {
        UE_LOG(LogTemp, Error, TEXT("HSRecipeDatabaseManager::LoadDatabase - 데이터베이스 데이터 로드 실패"));
        return false;
    }

    LoadedDatabases.AddUnique(LoadedDatabase);
    OnDatabaseLoaded(LoadedDatabase);

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabaseManager::LoadDatabase - 데이터베이스 로드 완료"));
    return true;
}

void UHSRecipeDatabaseManager::SetActiveDatabase(UHSRecipeDatabase* Database)
{
    if (Database && LoadedDatabases.Contains(Database))
    {
        ActiveDatabase = Database;
        UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabaseManager::SetActiveDatabase - 활성 데이터베이스 변경"));
    }
}

void UHSRecipeDatabaseManager::LoadAllRegisteredDatabases()
{
    for (const TSoftObjectPtr<UHSRecipeDatabase>& DatabaseAsset : RegisteredDatabases)
    {
        LoadDatabase(DatabaseAsset);
    }

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabaseManager::LoadAllRegisteredDatabases - %d개 데이터베이스 로드 완료"), 
           LoadedDatabases.Num());
}

FHSCraftingRecipe UHSRecipeDatabaseManager::FindRecipeInAllDatabases(const FName& RecipeID) const
{
    // 먼저 활성 데이터베이스에서 검색
    if (ActiveDatabase)
    {
        FHSCraftingRecipe Recipe = ActiveDatabase->GetRecipe(RecipeID);
        if (Recipe.IsValid())
        {
            return Recipe;
        }
    }

    // 다른 데이터베이스들에서 검색
    for (UHSRecipeDatabase* Database : LoadedDatabases)
    {
        if (Database && Database != ActiveDatabase)
        {
            FHSCraftingRecipe Recipe = Database->GetRecipe(RecipeID);
            if (Recipe.IsValid())
            {
                return Recipe;
            }
        }
    }

    return FHSCraftingRecipe();
}

TArray<FHSCraftingRecipe> UHSRecipeDatabaseManager::GetAllRecipesFromAllDatabases() const
{
    TArray<FHSCraftingRecipe> AllRecipes;

    for (UHSRecipeDatabase* Database : LoadedDatabases)
    {
        if (Database)
        {
            TArray<FHSCraftingRecipe> DatabaseRecipes = Database->GetAllRecipes();
            AllRecipes.Append(DatabaseRecipes);
        }
    }

    return AllRecipes;
}

void UHSRecipeDatabaseManager::OptimizeAllDatabases()
{
    for (UHSRecipeDatabase* Database : LoadedDatabases)
    {
        if (Database)
        {
            Database->OptimizeMemoryUsage();
        }
    }

    UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabaseManager::OptimizeAllDatabases - 모든 데이터베이스 최적화 완료"));
}

void UHSRecipeDatabaseManager::PreloadCriticalData()
{
    if (ActiveDatabase)
    {
        // 중요한 레시피들 미리 로드
        TArray<FName> CriticalRecipes = {
            TEXT("BasicSword"),
            TEXT("HealthPotion"),
            TEXT("Torch")
        };

        ActiveDatabase->PreloadFrequentlyUsedRecipes(CriticalRecipes);
    }
}

void UHSRecipeDatabaseManager::OnDatabaseLoaded(UHSRecipeDatabase* LoadedDatabase)
{
    if (LoadedDatabase)
    {
        // 첫 번째 로드된 데이터베이스를 활성으로 설정
        if (!ActiveDatabase)
        {
            ActiveDatabase = LoadedDatabase;
        }

        UE_LOG(LogTemp, Log, TEXT("HSRecipeDatabaseManager::OnDatabaseLoaded - 데이터베이스 로드 완료: %d개 레시피"), 
               LoadedDatabase->GetRecipeCount());
    }
}
