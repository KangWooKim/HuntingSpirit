#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSMetaCurrency.generated.h"

// 메타 화폐 타입 열거형
UENUM(BlueprintType)
enum class EHSCurrencyType : uint8
{
    None            UMETA(DisplayName = "None"),           // 없음
    MetaSouls       UMETA(DisplayName = "Meta Souls"),     // 메타 소울 (주요 화폐)
    EssencePoints   UMETA(DisplayName = "Essence Points"), // 에센스 포인트 (스킬 업그레이드용)
    UnlockPoints    UMETA(DisplayName = "Unlock Points"),  // 언락 포인트 (컨텐츠 해금용)
    CraftingTokens  UMETA(DisplayName = "Crafting Tokens"), // 제작 토큰 (고급 제작용)
    RuneShards      UMETA(DisplayName = "Rune Shards"),   // 룬 파편 (장비 강화용)
    ArcaneOrbs      UMETA(DisplayName = "Arcane Orbs"),    // 아케인 오브 (마법 업그레이드용)
    HeroicMedals    UMETA(DisplayName = "Heroic Medals"),  // 영웅 메달 (보스 처치 보상)
    DivineFragments UMETA(DisplayName = "Divine Fragments"), // 신성한 파편 (최고급 화폐)
    
    // 시즌 및 이벤트 화폐
    EventTokens     UMETA(DisplayName = "Event Tokens"),   // 이벤트 토큰
    SeasonCoins     UMETA(DisplayName = "Season Coins"),   // 시즌 코인
    
    MAX             UMETA(Hidden)
};

// 화폐 타입 정보 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSCurrencyInfo
{
    GENERATED_BODY()

    // 기본 정보
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic")
    FString CurrencyID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic")
    FString DisplayName;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic")
    FString Description;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Basic")
    FString IconPath;

    // 제한 설정
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Limits")
    int32 MaxAmount = 999999;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Limits")
    int32 MinAmount = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Limits")
    bool bHasMaxLimit = false;

    // 표시 설정
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Display")
    int32 DisplayPriority = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Display")
    bool bShowInUI = true;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Display")
    FString UICategory = TEXT("Primary");

    // 게임플레이 설정
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Gameplay")
    bool bCanBeTraded = false;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Gameplay")
    bool bLoseOnDeath = false;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Gameplay")
    float DeathLossPercentage = 0.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Gameplay")
    bool bAutoSave = true;

    FHSCurrencyInfo()
    {
        MaxAmount = 999999;
        MinAmount = 0;
        bHasMaxLimit = false;
        DisplayPriority = 0;
        bShowInUI = true;
        bCanBeTraded = false;
        bLoseOnDeath = false;
        DeathLossPercentage = 0.0f;
        bAutoSave = true;
    }

    bool IsValid() const
    {
        return !CurrencyID.IsEmpty() && !DisplayName.IsEmpty();
    }

    bool IsAmountValid(int32 Amount) const
    {
        if (Amount < MinAmount)
        {
            return false;
        }
        if (bHasMaxLimit && Amount > MaxAmount)
        {
            return false;
        }
        return true;
    }

    int32 ClampAmount(int32 Amount) const
    {
        Amount = FMath::Max(Amount, MinAmount);
        if (bHasMaxLimit)
        {
            Amount = FMath::Min(Amount, MaxAmount);
        }
        return Amount;
    }
};

// 화폐 거래 기록 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSCurrencyTransaction
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transaction")
    FString CurrencyID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transaction")
    int32 Amount = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transaction")
    int32 PreviousAmount = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transaction")
    int32 NewAmount = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transaction")
    FString TransactionType;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transaction")
    FString Source;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Transaction")
    FDateTime Timestamp;

    FHSCurrencyTransaction()
    {
        Amount = 0;
        PreviousAmount = 0;
        NewAmount = 0;
        Timestamp = FDateTime::Now();
    }

    FHSCurrencyTransaction(const FString& InCurrencyID, int32 InAmount, int32 InPreviousAmount, 
                          const FString& InType, const FString& InSource)
        : CurrencyID(InCurrencyID)
        , Amount(InAmount)
        , PreviousAmount(InPreviousAmount)
        , NewAmount(InPreviousAmount + InAmount)
        , TransactionType(InType)
        , Source(InSource)
        , Timestamp(FDateTime::Now())
    {
    }
};

// 화폐 교환 비율 구조체
USTRUCT(BlueprintType)
struct HUNTINGSPIRIT_API FHSCurrencyExchangeRate
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Exchange")
    FString FromCurrency;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Exchange")
    FString ToCurrency;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Exchange")
    float ExchangeRate = 1.0f;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Exchange")
    int32 MinAmount = 1;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Exchange")
    int32 MaxAmount = 1000;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Exchange")
    bool bIsEnabled = true;

    FHSCurrencyExchangeRate()
    {
        ExchangeRate = 1.0f;
        MinAmount = 1;
        MaxAmount = 1000;
        bIsEnabled = true;
    }
};

// 메타 화폐 델리게이트들
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCurrencyChanged, const FString&, CurrencyID, int32, OldAmount, int32, NewAmount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrencyAdded, const FHSCurrencyTransaction&, Transaction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrencySpent, const FHSCurrencyTransaction&, Transaction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCurrencyExchanged, const FString&, FromCurrency, const FString&, ToCurrency, int32, Amount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCurrencyLimitReached, const FString&, CurrencyID, int32, MaxAmount);

/**
 * 메타 화폐 관리 시스템
 * - 다양한 종류의 메타 화폐 관리 (MetaSouls, EssencePoints, UnlockPoints 등)
 * - 화폐 획득, 사용, 교환 처리
 * - 화폐별 제한 및 규칙 적용
 * - 거래 기록 및 통계
 * - 자동 저장/로드 시스템
 * - 성능 최적화 및 무결성 보장
 */
UCLASS(BlueprintType, Blueprintable)
class HUNTINGSPIRIT_API UHSMetaCurrency : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UHSMetaCurrency();

    // USubsystem 인터페이스
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 화폐 관리 메서드
    
    /**
     * 화폐를 추가합니다
     * @param CurrencyID 화폐 종류
     * @param Amount 추가할 양 (양수)
     * @param Source 출처 (로그용)
     * @return 실제로 추가된 양
     */
    UFUNCTION(BlueprintCallable, Category = "Meta Currency")
    int32 AddCurrency(const FString& CurrencyID, int32 Amount, const FString& Source = TEXT("Unknown"));

    /**
     * 화폐를 사용합니다
     * @param CurrencyID 화폐 종류
     * @param Amount 사용할 양 (양수)
     * @param Source 용도 (로그용)
     * @return 사용 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Meta Currency")
    bool SpendCurrency(const FString& CurrencyID, int32 Amount, const FString& Source = TEXT("Unknown"));

    /**
     * 여러 화폐를 동시에 사용합니다
     * @param CurrencyAmounts 화폐별 사용량 맵
     * @param Source 용도 (로그용)
     * @return 모든 화폐 사용 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Meta Currency")
    bool SpendMultipleCurrencies(const TMap<FString, int32>& CurrencyAmounts, const FString& Source = TEXT("Unknown"));

    /**
     * 화폐를 설정합니다
     * @param CurrencyID 화폐 종류
     * @param Amount 설정할 양
     * @param Source 출처 (로그용)
     */
    UFUNCTION(BlueprintCallable, Category = "Meta Currency")
    void SetCurrency(const FString& CurrencyID, int32 Amount, const FString& Source = TEXT("Set"));

    /**
     * 화폐 잔액을 조회합니다
     * @param CurrencyID 화폐 종류
     * @return 현재 잔액
     */
    UFUNCTION(BlueprintPure, Category = "Meta Currency")
    int32 GetCurrency(const FString& CurrencyID) const;

    /**
     * 화폐가 충분한지 확인합니다
     * @param CurrencyID 화폐 종류
     * @param Amount 필요한 양
     * @return 충분한지 여부
     */
    UFUNCTION(BlueprintPure, Category = "Meta Currency")
    bool HasEnoughCurrency(const FString& CurrencyID, int32 Amount) const;

    /**
     * 여러 화폐가 충분한지 확인합니다
     * @param CurrencyAmounts 화폐별 필요량 맵
     * @return 모든 화폐가 충분한지 여부
     */
    UFUNCTION(BlueprintPure, Category = "Meta Currency")
    bool HasEnoughMultipleCurrencies(const TMap<FString, int32>& CurrencyAmounts) const;

    /**
     * 모든 화폐 정보를 가져옵니다
     * @return 화폐 ID와 잔액의 맵
     */
    UFUNCTION(BlueprintPure, Category = "Meta Currency")
    TMap<FString, int32> GetAllCurrencies() const;

    // 화폐 타입 관리
    
    /**
     * 새로운 화폐 타입을 등록합니다
     * @param CurrencyInfo 화폐 정보
     */
    UFUNCTION(BlueprintCallable, Category = "Currency Types")
    void RegisterCurrencyType(const FHSCurrencyInfo& CurrencyInfo);

    /**
     * 화폐 타입을 제거합니다
     * @param CurrencyID 화폐 ID
     */
    UFUNCTION(BlueprintCallable, Category = "Currency Types")
    void UnregisterCurrencyType(const FString& CurrencyID);

    /**
     * 화폐 타입 정보를 가져옵니다
     * @param CurrencyID 화폐 ID
     * @return 화폐 정보
     */
    UFUNCTION(BlueprintPure, Category = "Currency Types")
    FHSCurrencyInfo GetCurrencyInfo(const FString& CurrencyID) const;

    /**
     * 등록된 모든 화폐 타입을 가져옵니다
     * @param bUIVisibleOnly UI에 표시되는 것만 가져올지 여부
     * @return 화폐 정보 배열
     */
    UFUNCTION(BlueprintPure, Category = "Currency Types")
    TArray<FHSCurrencyInfo> GetAllCurrencyTypes(bool bUIVisibleOnly = false) const;

    /**
     * 화폐 타입이 존재하는지 확인합니다
     * @param CurrencyID 화폐 ID
     * @return 존재 여부
     */
    UFUNCTION(BlueprintPure, Category = "Currency Types")
    bool IsCurrencyTypeRegistered(const FString& CurrencyID) const;

    // 화폐 교환
    
    /**
     * 화폐를 교환합니다
     * @param FromCurrency 교환할 화폐
     * @param ToCurrency 받을 화폐
     * @param Amount 교환할 양
     * @return 교환 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Currency Exchange")
    bool ExchangeCurrency(const FString& FromCurrency, const FString& ToCurrency, int32 Amount);

    /**
     * 교환 비율을 설정합니다
     * @param ExchangeRate 교환 비율 정보
     */
    UFUNCTION(BlueprintCallable, Category = "Currency Exchange")
    void SetExchangeRate(const FHSCurrencyExchangeRate& ExchangeRate);

    /**
     * 교환 비율을 가져옵니다
     * @param FromCurrency 교환할 화폐
     * @param ToCurrency 받을 화폐
     * @return 교환 비율 (0이면 교환 불가)
     */
    UFUNCTION(BlueprintPure, Category = "Currency Exchange")
    float GetExchangeRate(const FString& FromCurrency, const FString& ToCurrency) const;

    /**
     * 교환 가능한지 확인합니다
     * @param FromCurrency 교환할 화폐
     * @param ToCurrency 받을 화폐
     * @param Amount 교환할 양
     * @return 교환 가능 여부
     */
    UFUNCTION(BlueprintPure, Category = "Currency Exchange")
    bool CanExchangeCurrency(const FString& FromCurrency, const FString& ToCurrency, int32 Amount) const;

    // 거래 기록 및 통계
    
    /**
     * 거래 기록을 가져옵니다
     * @param CurrencyID 특정 화폐 (빈 문자열이면 모든 화폐)
     * @param MaxRecords 최대 기록 수
     * @return 거래 기록 배열
     */
    UFUNCTION(BlueprintPure, Category = "Transaction History")
    TArray<FHSCurrencyTransaction> GetTransactionHistory(const FString& CurrencyID = TEXT(""), int32 MaxRecords = 100) const;

    /**
     * 총 획득량을 가져옵니다
     * @param CurrencyID 화폐 ID
     * @return 총 획득량
     */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    int32 GetTotalEarned(const FString& CurrencyID) const;

    /**
     * 총 사용량을 가져옵니다
     * @param CurrencyID 화폐 ID
     * @return 총 사용량
     */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    int32 GetTotalSpent(const FString& CurrencyID) const;

    /**
     * 거래 기록을 정리합니다
     * @param DaysToKeep 보관할 일수
     */
    UFUNCTION(BlueprintCallable, Category = "Transaction History")
    void CleanupTransactionHistory(int32 DaysToKeep = 30);

    // 저장/로드
    
    /**
     * 화폐 데이터를 저장합니다
     * @return 저장 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool SaveCurrencyData();

    /**
     * 화폐 데이터를 로드합니다
     * @return 로드 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Save System")
    bool LoadCurrencyData();

    /**
     * 자동 저장을 설정합니다
     * @param bEnabled 자동 저장 활성화 여부
     * @param IntervalSeconds 저장 간격 (초)
     */
    UFUNCTION(BlueprintCallable, Category = "Save System")
    void SetAutoSave(bool bEnabled, float IntervalSeconds = 60.0f);

    // 특수 기능
    
    /**
     * 사망 시 화폐 손실을 처리합니다
     */
    UFUNCTION(BlueprintCallable, Category = "Special")
    void ProcessDeathLoss();

    /**
     * 모든 화폐를 초기화합니다 (디버그용)
     */
    UFUNCTION(BlueprintCallable, Category = "Debug", CallInEditor)
    void ResetAllCurrencies();

    /**
     * 화폐 데이터 유효성을 검증합니다
     * @return 유효성 검증 결과
     */
    UFUNCTION(BlueprintCallable, Category = "Debug")
    bool ValidateCurrencyData() const;

    // === Enum 변환 헬퍼 함수들 ===
    
    /**
     * EHSCurrencyType을 문자열로 변환합니다
     * @param CurrencyType 변환할 화폐 타입
     * @return 화폐 ID 문자열
     */
    UFUNCTION(BlueprintPure, Category = "Currency Conversion")
    static FString CurrencyTypeToString(EHSCurrencyType CurrencyType);
    
    /**
     * 문자열을 EHSCurrencyType으로 변환합니다
     * @param CurrencyString 변환할 문자열
     * @return 화폐 타입 (실패 시 None)
     */
    UFUNCTION(BlueprintPure, Category = "Currency Conversion")
    static EHSCurrencyType StringToCurrencyType(const FString& CurrencyString);
    
    /**
     * EHSCurrencyType으로 화폐를 추가합니다
     * @param CurrencyType 화폐 타입
     * @param Amount 추가할 양
     * @param Source 출처
     * @return 실제로 추가된 양
     */
    UFUNCTION(BlueprintCallable, Category = "Meta Currency")
    int32 AddCurrencyByType(EHSCurrencyType CurrencyType, int32 Amount, const FString& Source = TEXT("Unknown"));
    
    /**
     * EHSCurrencyType으로 화폐를 사용합니다
     * @param CurrencyType 화폐 타입
     * @param Amount 사용할 양
     * @param Source 용도
     * @return 사용 성공 여부
     */
    UFUNCTION(BlueprintCallable, Category = "Meta Currency")
    bool SpendCurrencyByType(EHSCurrencyType CurrencyType, int32 Amount, const FString& Source = TEXT("Unknown"));
    
    /**
     * EHSCurrencyType으로 화폐 잔액을 조회합니다
     * @param CurrencyType 화폐 타입
     * @return 현재 잔액
     */
    UFUNCTION(BlueprintPure, Category = "Meta Currency")
    int32 GetCurrencyByType(EHSCurrencyType CurrencyType) const;
    
    /**
     * EHSCurrencyType으로 화폐가 충분한지 확인합니다
     * @param CurrencyType 화폐 타입
     * @param Amount 필요한 양
     * @return 충분한지 여부
     */
    UFUNCTION(BlueprintPure, Category = "Meta Currency")
    bool HasEnoughCurrencyByType(EHSCurrencyType CurrencyType, int32 Amount) const;

    // 델리게이트
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCurrencyChanged OnCurrencyChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCurrencyAdded OnCurrencyAdded;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCurrencySpent OnCurrencySpent;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCurrencyExchanged OnCurrencyExchanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCurrencyLimitReached OnCurrencyLimitReached;

protected:
    // 화폐 데이터
    UPROPERTY(BlueprintReadOnly, Category = "Data")
    TMap<FString, int32> CurrencyBalances;

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    TMap<FString, FHSCurrencyInfo> CurrencyTypes;

    UPROPERTY(BlueprintReadOnly, Category = "Data")
    TMap<FString, FHSCurrencyExchangeRate> ExchangeRates;

    // 통계 데이터
    UPROPERTY(BlueprintReadOnly, Category = "Statistics")
    TMap<FString, int32> TotalEarned;

    UPROPERTY(BlueprintReadOnly, Category = "Statistics")
    TMap<FString, int32> TotalSpent;

    // 거래 기록
    UPROPERTY(BlueprintReadOnly, Category = "Transactions")
    TArray<FHSCurrencyTransaction> TransactionHistory;

    // 설정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    FString CurrencySaveFileName = TEXT("HuntingSpiritCurrency");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    bool bAutoSaveEnabled = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    float AutoSaveInterval = 60.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    int32 MaxTransactionHistory = 1000;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Configuration")
    bool bEnableTransactionLogging = true;

private:
    // 타이머
    FTimerHandle AutoSaveTimerHandle;

    // 캐싱 (성능 최적화)
    mutable TMap<FString, int32> CachedBalances;
    mutable bool bCacheValid = false;

    // 내부 메서드들
    
    /**
     * 기본 화폐 타입들을 초기화합니다
     */
    void InitializeDefaultCurrencyTypes();

    /**
     * 기본 교환 비율을 설정합니다
     */
    void InitializeDefaultExchangeRates();

    /**
     * 거래를 기록합니다
     * @param Transaction 거래 정보
     */
    void RecordTransaction(const FHSCurrencyTransaction& Transaction);

    /**
     * 화폐 변경을 처리합니다
     * @param CurrencyID 화폐 ID
     * @param OldAmount 이전 양
     * @param NewAmount 새로운 양
     * @param TransactionType 거래 타입
     * @param Source 출처
     */
    void ProcessCurrencyChange(const FString& CurrencyID, int32 OldAmount, int32 NewAmount, 
                              const FString& TransactionType, const FString& Source);

    /**
     * 자동 저장을 수행합니다
     */
    void PerformAutoSave();

    /**
     * 캐시를 무효화합니다
     */
    void InvalidateCache();

    /**
     * 교환 키를 생성합니다
     * @param FromCurrency 교환할 화폐
     * @param ToCurrency 받을 화폐
     * @return 교환 키
     */
    FString GenerateExchangeKey(const FString& FromCurrency, const FString& ToCurrency) const;

    /**
     * 화폐 유효성을 검증합니다
     * @param CurrencyID 화폐 ID
     * @param Amount 양
     * @return 유효성 여부
     */
    bool ValidateCurrencyAmount(const FString& CurrencyID, int32 Amount) const;
};
