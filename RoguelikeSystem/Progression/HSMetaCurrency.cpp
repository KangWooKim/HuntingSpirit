#include "HSMetaCurrency.h"
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

UHSMetaCurrency::UHSMetaCurrency()
{
    // 기본 설정
    CurrencySaveFileName = TEXT("HuntingSpiritCurrency");
    bAutoSaveEnabled = true;
    AutoSaveInterval = 60.0f;
    MaxTransactionHistory = 1000;
    bEnableTransactionLogging = true;
    
    // 캐시 초기화
    bCacheValid = false;
}

void UHSMetaCurrency::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // 기본 화폐 타입 초기화
    InitializeDefaultCurrencyTypes();
    
    // 기본 교환 비율 설정
    InitializeDefaultExchangeRates();
    
    // 저장된 데이터 로드
    if (!LoadCurrencyData())
    {
        UE_LOG(LogTemp, Warning, TEXT("저장된 화폐 데이터를 찾을 수 없습니다. 기본 상태를 사용합니다."));
        
        // 기본 화폐 잔액 설정
        for (const auto& CurrencyType : CurrencyTypes)
        {
            CurrencyBalances.Add(CurrencyType.Key, 0);
            TotalEarned.Add(CurrencyType.Key, 0);
            TotalSpent.Add(CurrencyType.Key, 0);
        }
        
        SaveCurrencyData(); // 기본 데이터 저장
    }
    
    // 자동 저장 설정
    if (bAutoSaveEnabled)
    {
        SetAutoSave(true, AutoSaveInterval);
    }
    
    UE_LOG(LogTemp, Log, TEXT("HSMetaCurrency 초기화 완료 - 화폐 종류: %d개"), CurrencyTypes.Num());
}

void UHSMetaCurrency::Deinitialize()
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
    if (bAutoSaveEnabled)
    {
        SaveCurrencyData();
    }
    
    // 캐시 정리
    InvalidateCache();
    
    Super::Deinitialize();
    
    UE_LOG(LogTemp, Log, TEXT("HSMetaCurrency 정리 완료"));
}

int32 UHSMetaCurrency::AddCurrency(const FString& CurrencyID, int32 Amount, const FString& Source)
{
    if (Amount <= 0 || CurrencyID.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 추가 실패: 잘못된 매개변수 - %s, %d"), *CurrencyID, Amount);
        return 0;
    }
    
    if (!IsCurrencyTypeRegistered(CurrencyID))
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 추가 실패: 등록되지 않은 화폐 타입 - %s"), *CurrencyID);
        return 0;
    }
    
    const FHSCurrencyInfo& CurrencyInfo = CurrencyTypes[CurrencyID];
    int32 CurrentAmount = GetCurrency(CurrencyID);
    int32 NewAmount = CurrentAmount + Amount;
    
    // 최대 제한 확인
    if (CurrencyInfo.bHasMaxLimit && NewAmount > CurrencyInfo.MaxAmount)
    {
        int32 ActualAdded = CurrencyInfo.MaxAmount - CurrentAmount;
        if (ActualAdded <= 0)
        {
            OnCurrencyLimitReached.Broadcast(CurrencyID, CurrencyInfo.MaxAmount);
            return 0;
        }
        
        NewAmount = CurrencyInfo.MaxAmount;
        Amount = ActualAdded;
        OnCurrencyLimitReached.Broadcast(CurrencyID, CurrencyInfo.MaxAmount);
    }
    
    // 화폐 업데이트
    CurrencyBalances.Add(CurrencyID, NewAmount);
    
    // 통계 업데이트
    if (TotalEarned.Contains(CurrencyID))
    {
        TotalEarned[CurrencyID] += Amount;
    }
    else
    {
        TotalEarned.Add(CurrencyID, Amount);
    }
    
    // 변경 처리
    ProcessCurrencyChange(CurrencyID, CurrentAmount, NewAmount, TEXT("Add"), Source);
    
    UE_LOG(LogTemp, Log, TEXT("화폐 추가: %s +%d (총: %d) [%s]"), *CurrencyID, Amount, NewAmount, *Source);
    
    return Amount;
}

bool UHSMetaCurrency::SpendCurrency(const FString& CurrencyID, int32 Amount, const FString& Source)
{
    if (Amount <= 0 || CurrencyID.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 사용 실패: 잘못된 매개변수 - %s, %d"), *CurrencyID, Amount);
        return false;
    }
    
    if (!HasEnoughCurrency(CurrencyID, Amount))
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 사용 실패: 잔액 부족 - %s (필요: %d, 보유: %d)"), 
               *CurrencyID, Amount, GetCurrency(CurrencyID));
        return false;
    }
    
    int32 CurrentAmount = GetCurrency(CurrencyID);
    int32 NewAmount = CurrentAmount - Amount;
    
    // 화폐 업데이트
    CurrencyBalances.Add(CurrencyID, NewAmount);
    
    // 통계 업데이트
    if (TotalSpent.Contains(CurrencyID))
    {
        TotalSpent[CurrencyID] += Amount;
    }
    else
    {
        TotalSpent.Add(CurrencyID, Amount);
    }
    
    // 변경 처리
    ProcessCurrencyChange(CurrencyID, CurrentAmount, NewAmount, TEXT("Spend"), Source);
    
    UE_LOG(LogTemp, Log, TEXT("화폐 사용: %s -%d (남은: %d) [%s]"), *CurrencyID, Amount, NewAmount, *Source);
    
    return true;
}

bool UHSMetaCurrency::SpendMultipleCurrencies(const TMap<FString, int32>& CurrencyAmounts, const FString& Source)
{
    // 먼저 모든 화폐가 충분한지 확인
    if (!HasEnoughMultipleCurrencies(CurrencyAmounts))
    {
        UE_LOG(LogTemp, Warning, TEXT("다중 화폐 사용 실패: 일부 화폐 잔액 부족"));
        return false;
    }
    
    // 모든 화폐를 사용
    for (const auto& Pair : CurrencyAmounts)
    {
        if (!SpendCurrency(Pair.Key, Pair.Value, Source))
        {
            // 이론적으로는 위에서 확인했으므로 발생하지 않아야 함
            UE_LOG(LogTemp, Error, TEXT("다중 화폐 사용 중 오류 발생: %s"), *Pair.Key);
            return false;
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("다중 화폐 사용 성공: %d개 화폐 [%s]"), CurrencyAmounts.Num(), *Source);
    return true;
}

void UHSMetaCurrency::SetCurrency(const FString& CurrencyID, int32 Amount, const FString& Source)
{
    if (CurrencyID.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 설정 실패: 빈 화폐 ID"));
        return;
    }
    
    if (!IsCurrencyTypeRegistered(CurrencyID))
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 설정 실패: 등록되지 않은 화폐 타입 - %s"), *CurrencyID);
        return;
    }
    
    const FHSCurrencyInfo& CurrencyInfo = CurrencyTypes[CurrencyID];
    int32 ClampedAmount = CurrencyInfo.ClampAmount(Amount);
    
    int32 CurrentAmount = GetCurrency(CurrencyID);
    CurrencyBalances.Add(CurrencyID, ClampedAmount);
    
    // 변경 처리
    ProcessCurrencyChange(CurrencyID, CurrentAmount, ClampedAmount, TEXT("Set"), Source);
    
    UE_LOG(LogTemp, Log, TEXT("화폐 설정: %s = %d [%s]"), *CurrencyID, ClampedAmount, *Source);
}

int32 UHSMetaCurrency::GetCurrency(const FString& CurrencyID) const
{
    if (const int32* Amount = CurrencyBalances.Find(CurrencyID))
    {
        return *Amount;
    }
    return 0;
}

bool UHSMetaCurrency::HasEnoughCurrency(const FString& CurrencyID, int32 Amount) const
{
    return GetCurrency(CurrencyID) >= Amount;
}

bool UHSMetaCurrency::HasEnoughMultipleCurrencies(const TMap<FString, int32>& CurrencyAmounts) const
{
    for (const auto& Pair : CurrencyAmounts)
    {
        if (!HasEnoughCurrency(Pair.Key, Pair.Value))
        {
            return false;
        }
    }
    return true;
}

TMap<FString, int32> UHSMetaCurrency::GetAllCurrencies() const
{
    return CurrencyBalances;
}

void UHSMetaCurrency::RegisterCurrencyType(const FHSCurrencyInfo& CurrencyInfo)
{
    if (!CurrencyInfo.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 타입 등록 실패: 유효하지 않은 정보"));
        return;
    }
    
    CurrencyTypes.Add(CurrencyInfo.CurrencyID, CurrencyInfo);
    
    // 기본 잔액 설정 (이미 존재하지 않는 경우)
    if (!CurrencyBalances.Contains(CurrencyInfo.CurrencyID))
    {
        CurrencyBalances.Add(CurrencyInfo.CurrencyID, 0);
        TotalEarned.Add(CurrencyInfo.CurrencyID, 0);
        TotalSpent.Add(CurrencyInfo.CurrencyID, 0);
    }
    
    InvalidateCache();
    
    UE_LOG(LogTemp, Log, TEXT("화폐 타입 등록됨: %s - %s"), *CurrencyInfo.CurrencyID, *CurrencyInfo.DisplayName);
}

void UHSMetaCurrency::UnregisterCurrencyType(const FString& CurrencyID)
{
    if (CurrencyTypes.Remove(CurrencyID) > 0)
    {
        // 관련 데이터도 제거
        CurrencyBalances.Remove(CurrencyID);
        TotalEarned.Remove(CurrencyID);
        TotalSpent.Remove(CurrencyID);
        
        InvalidateCache();
        
        UE_LOG(LogTemp, Log, TEXT("화폐 타입 제거됨: %s"), *CurrencyID);
    }
}

FHSCurrencyInfo UHSMetaCurrency::GetCurrencyInfo(const FString& CurrencyID) const
{
    if (const FHSCurrencyInfo* Info = CurrencyTypes.Find(CurrencyID))
    {
        return *Info;
    }
    return FHSCurrencyInfo();
}

TArray<FHSCurrencyInfo> UHSMetaCurrency::GetAllCurrencyTypes(bool bUIVisibleOnly) const
{
    TArray<FHSCurrencyInfo> Result;
    Result.Reserve(CurrencyTypes.Num());
    
    for (const auto& Pair : CurrencyTypes)
    {
        if (!bUIVisibleOnly || Pair.Value.bShowInUI)
        {
            Result.Add(Pair.Value);
        }
    }
    
    // 우선순위로 정렬
    Result.Sort([](const FHSCurrencyInfo& A, const FHSCurrencyInfo& B)
    {
        return A.DisplayPriority < B.DisplayPriority;
    });
    
    return Result;
}

bool UHSMetaCurrency::IsCurrencyTypeRegistered(const FString& CurrencyID) const
{
    return CurrencyTypes.Contains(CurrencyID);
}

bool UHSMetaCurrency::ExchangeCurrency(const FString& FromCurrency, const FString& ToCurrency, int32 Amount)
{
    if (Amount <= 0 || FromCurrency.IsEmpty() || ToCurrency.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 교환 실패: 잘못된 매개변수"));
        return false;
    }
    
    if (!CanExchangeCurrency(FromCurrency, ToCurrency, Amount))
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 교환 실패: 조건 미충족"));
        return false;
    }
    
    float ExchangeRate = GetExchangeRate(FromCurrency, ToCurrency);
    if (ExchangeRate <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 교환 실패: 유효하지 않은 교환 비율"));
        return false;
    }
    
    int32 ReceivedAmount = FMath::FloorToInt(Amount * ExchangeRate);
    
    // 교환 실행
    if (!SpendCurrency(FromCurrency, Amount, TEXT("Exchange")))
    {
        return false;
    }
    
    AddCurrency(ToCurrency, ReceivedAmount, TEXT("Exchange"));
    
    // 이벤트 발생
    OnCurrencyExchanged.Broadcast(FromCurrency, ToCurrency, Amount);
    
    UE_LOG(LogTemp, Log, TEXT("화폐 교환 성공: %s %d -> %s %d (비율: %.2f)"), 
           *FromCurrency, Amount, *ToCurrency, ReceivedAmount, ExchangeRate);
    
    return true;
}

void UHSMetaCurrency::SetExchangeRate(const FHSCurrencyExchangeRate& ExchangeRate)
{
    if (ExchangeRate.FromCurrency.IsEmpty() || ExchangeRate.ToCurrency.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("교환 비율 설정 실패: 빈 화폐 ID"));
        return;
    }
    
    FString ExchangeKey = GenerateExchangeKey(ExchangeRate.FromCurrency, ExchangeRate.ToCurrency);
    ExchangeRates.Add(ExchangeKey, ExchangeRate);
    
    UE_LOG(LogTemp, Log, TEXT("교환 비율 설정: %s -> %s (%.2f)"), 
           *ExchangeRate.FromCurrency, *ExchangeRate.ToCurrency, ExchangeRate.ExchangeRate);
}

float UHSMetaCurrency::GetExchangeRate(const FString& FromCurrency, const FString& ToCurrency) const
{
    FString ExchangeKey = GenerateExchangeKey(FromCurrency, ToCurrency);
    if (const FHSCurrencyExchangeRate* Rate = ExchangeRates.Find(ExchangeKey))
    {
        return Rate->bIsEnabled ? Rate->ExchangeRate : 0.0f;
    }
    return 0.0f;
}

bool UHSMetaCurrency::CanExchangeCurrency(const FString& FromCurrency, const FString& ToCurrency, int32 Amount) const
{
    // 기본 검증
    if (Amount <= 0 || !IsCurrencyTypeRegistered(FromCurrency) || !IsCurrencyTypeRegistered(ToCurrency))
    {
        return false;
    }
    
    // 잔액 확인
    if (!HasEnoughCurrency(FromCurrency, Amount))
    {
        return false;
    }
    
    // 교환 비율 확인
    FString ExchangeKey = GenerateExchangeKey(FromCurrency, ToCurrency);
    const FHSCurrencyExchangeRate* Rate = ExchangeRates.Find(ExchangeKey);
    if (!Rate || !Rate->bIsEnabled)
    {
        return false;
    }
    
    // 최소/최대 교환량 확인
    if (Amount < Rate->MinAmount || Amount > Rate->MaxAmount)
    {
        return false;
    }
    
    return true;
}

TArray<FHSCurrencyTransaction> UHSMetaCurrency::GetTransactionHistory(const FString& CurrencyID, int32 MaxRecords) const
{
    TArray<FHSCurrencyTransaction> Result;
    
    if (CurrencyID.IsEmpty())
    {
        // 모든 거래 반환
        Result = TransactionHistory;
    }
    else
    {
        // 특정 화폐의 거래만 반환
        for (const FHSCurrencyTransaction& Transaction : TransactionHistory)
        {
            if (Transaction.CurrencyID == CurrencyID)
            {
                Result.Add(Transaction);
            }
        }
    }
    
    // 최신 순으로 정렬
    Result.Sort([](const FHSCurrencyTransaction& A, const FHSCurrencyTransaction& B)
    {
        return A.Timestamp > B.Timestamp;
    });
    
    // 최대 기록 수 제한
    if (MaxRecords > 0 && Result.Num() > MaxRecords)
    {
        Result.SetNum(MaxRecords);
    }
    
    return Result;
}

int32 UHSMetaCurrency::GetTotalEarned(const FString& CurrencyID) const
{
    if (const int32* Total = TotalEarned.Find(CurrencyID))
    {
        return *Total;
    }
    return 0;
}

int32 UHSMetaCurrency::GetTotalSpent(const FString& CurrencyID) const
{
    if (const int32* Total = TotalSpent.Find(CurrencyID))
    {
        return *Total;
    }
    return 0;
}

void UHSMetaCurrency::CleanupTransactionHistory(int32 DaysToKeep)
{
    if (DaysToKeep <= 0)
    {
        return;
    }
    
    FDateTime CutoffTime = FDateTime::Now() - FTimespan::FromDays(DaysToKeep);
    
    int32 OriginalCount = TransactionHistory.Num();
    TransactionHistory.RemoveAll([CutoffTime](const FHSCurrencyTransaction& Transaction)
    {
        return Transaction.Timestamp < CutoffTime;
    });
    
    int32 RemovedCount = OriginalCount - TransactionHistory.Num();
    if (RemovedCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("거래 기록 정리 완료: %d개 제거됨"), RemovedCount);
    }
}

bool UHSMetaCurrency::SaveCurrencyData()
{
    // JSON 객체 생성
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    
    // 버전 정보
    JsonObject->SetNumberField(TEXT("Version"), 1);
    JsonObject->SetStringField(TEXT("SaveTime"), FDateTime::Now().ToString());
    
    // 화폐 잔액 저장
    TSharedPtr<FJsonObject> BalancesObject = MakeShareable(new FJsonObject);
    for (const auto& Pair : CurrencyBalances)
    {
        BalancesObject->SetNumberField(Pair.Key, Pair.Value);
    }
    JsonObject->SetObjectField(TEXT("CurrencyBalances"), BalancesObject);
    
    // 총 획득량 저장
    TSharedPtr<FJsonObject> EarnedObject = MakeShareable(new FJsonObject);
    for (const auto& Pair : TotalEarned)
    {
        EarnedObject->SetNumberField(Pair.Key, Pair.Value);
    }
    JsonObject->SetObjectField(TEXT("TotalEarned"), EarnedObject);
    
    // 총 사용량 저장
    TSharedPtr<FJsonObject> SpentObject = MakeShareable(new FJsonObject);
    for (const auto& Pair : TotalSpent)
    {
        SpentObject->SetNumberField(Pair.Key, Pair.Value);
    }
    JsonObject->SetObjectField(TEXT("TotalSpent"), SpentObject);
    
    // 거래 기록 저장 (최근 100개만)
    if (bEnableTransactionLogging && TransactionHistory.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> TransactionsArray;
        int32 MaxToSave = FMath::Min(100, TransactionHistory.Num());
        
        for (int32 i = TransactionHistory.Num() - MaxToSave; i < TransactionHistory.Num(); i++)
        {
            const FHSCurrencyTransaction& Transaction = TransactionHistory[i];
            
            TSharedPtr<FJsonObject> TransactionObj = MakeShareable(new FJsonObject);
            TransactionObj->SetStringField(TEXT("CurrencyID"), Transaction.CurrencyID);
            TransactionObj->SetNumberField(TEXT("Amount"), Transaction.Amount);
            TransactionObj->SetNumberField(TEXT("PreviousAmount"), Transaction.PreviousAmount);
            TransactionObj->SetNumberField(TEXT("NewAmount"), Transaction.NewAmount);
            TransactionObj->SetStringField(TEXT("TransactionType"), Transaction.TransactionType);
            TransactionObj->SetStringField(TEXT("Source"), Transaction.Source);
            TransactionObj->SetStringField(TEXT("Timestamp"), Transaction.Timestamp.ToString());
            
            TransactionsArray.Add(MakeShareable(new FJsonValueObject(TransactionObj)));
        }
        
        JsonObject->SetArrayField(TEXT("TransactionHistory"), TransactionsArray);
    }
    
    // JSON 문자열로 변환
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
    
    // 파일로 저장
    FString SaveDirectory = FPaths::ProjectSavedDir() + TEXT("SaveGames/");
    FString FullPath = SaveDirectory + CurrencySaveFileName + TEXT(".json");
    
    // 디렉토리 생성
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*SaveDirectory))
    {
        PlatformFile.CreateDirectoryTree(*SaveDirectory);
    }
    
    // 파일 저장
    if (!FFileHelper::SaveStringToFile(OutputString, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("화폐 데이터 저장 실패: %s"), *FullPath);
        return false;
    }
    
    UE_LOG(LogTemp, Log, TEXT("화폐 데이터 저장 완료: %s"), *FullPath);
    return true;
}

bool UHSMetaCurrency::LoadCurrencyData()
{
    FString SaveDirectory = FPaths::ProjectSavedDir() + TEXT("SaveGames/");
    FString FullPath = SaveDirectory + CurrencySaveFileName + TEXT(".json");
    
    // 파일 존재 확인
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FullPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 저장 파일이 존재하지 않습니다: %s"), *FullPath);
        return false;
    }
    
    // 파일 읽기
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FullPath))
    {
        UE_LOG(LogTemp, Error, TEXT("화폐 저장 파일 읽기 실패: %s"), *FullPath);
        return false;
    }
    
    // JSON 파싱
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("화폐 데이터 JSON 파싱 실패"));
        return false;
    }
    
    try
    {
        // 화폐 잔액 로드
        if (const TSharedPtr<FJsonObject>* BalancesObject; JsonObject->TryGetObjectField(TEXT("CurrencyBalances"), BalancesObject))
        {
            CurrencyBalances.Empty();
            for (const auto& Pair : (*BalancesObject)->Values)
            {
                CurrencyBalances.Add(Pair.Key, static_cast<int32>(Pair.Value->AsNumber()));
            }
        }
        
        // 총 획득량 로드
        if (const TSharedPtr<FJsonObject>* EarnedObject; JsonObject->TryGetObjectField(TEXT("TotalEarned"), EarnedObject))
        {
            TotalEarned.Empty();
            for (const auto& Pair : (*EarnedObject)->Values)
            {
                TotalEarned.Add(Pair.Key, static_cast<int32>(Pair.Value->AsNumber()));
            }
        }
        
        // 총 사용량 로드
        if (const TSharedPtr<FJsonObject>* SpentObject; JsonObject->TryGetObjectField(TEXT("TotalSpent"), SpentObject))
        {
            TotalSpent.Empty();
            for (const auto& Pair : (*SpentObject)->Values)
            {
                TotalSpent.Add(Pair.Key, static_cast<int32>(Pair.Value->AsNumber()));
            }
        }
        
        // 거래 기록 로드
        if (const TArray<TSharedPtr<FJsonValue>>* TransactionsArray; JsonObject->TryGetArrayField(TEXT("TransactionHistory"), TransactionsArray))
        {
            TransactionHistory.Empty();
            for (const auto& TransactionValue : *TransactionsArray)
            {
                const TSharedPtr<FJsonObject>& TransactionObj = TransactionValue->AsObject();
                
                FHSCurrencyTransaction Transaction;
                Transaction.CurrencyID = TransactionObj->GetStringField(TEXT("CurrencyID"));
                Transaction.Amount = TransactionObj->GetIntegerField(TEXT("Amount"));
                Transaction.PreviousAmount = TransactionObj->GetIntegerField(TEXT("PreviousAmount"));
                Transaction.NewAmount = TransactionObj->GetIntegerField(TEXT("NewAmount"));
                Transaction.TransactionType = TransactionObj->GetStringField(TEXT("TransactionType"));
                Transaction.Source = TransactionObj->GetStringField(TEXT("Source"));
                
                FString TimestampString = TransactionObj->GetStringField(TEXT("Timestamp"));
                FDateTime::Parse(TimestampString, Transaction.Timestamp);
                
                TransactionHistory.Add(Transaction);
            }
        }
        
        // 캐시 무효화
        InvalidateCache();
        
        UE_LOG(LogTemp, Log, TEXT("화폐 데이터 로드 완료"));
        return true;
    }
    catch (const std::exception&)
    {
        UE_LOG(LogTemp, Error, TEXT("화폐 데이터 로드 중 오류 발생"));
        return false;
    }
}

void UHSMetaCurrency::SetAutoSave(bool bEnabled, float IntervalSeconds)
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
            World->GetTimerManager().SetTimer(AutoSaveTimerHandle, this, &UHSMetaCurrency::PerformAutoSave, IntervalSeconds, true);
            UE_LOG(LogTemp, Log, TEXT("화폐 자동 저장 활성화됨 - 간격: %.1f초"), IntervalSeconds);
        }
    }
}

void UHSMetaCurrency::ProcessDeathLoss()
{
    bool bAnyLoss = false;
    
    for (const auto& CurrencyType : CurrencyTypes)
    {
        const FHSCurrencyInfo& Info = CurrencyType.Value;
        
        if (Info.bLoseOnDeath && Info.DeathLossPercentage > 0.0f)
        {
            int32 CurrentAmount = GetCurrency(Info.CurrencyID);
            if (CurrentAmount > 0)
            {
                int32 LossAmount = FMath::FloorToInt(CurrentAmount * Info.DeathLossPercentage);
                if (LossAmount > 0)
                {
                    SpendCurrency(Info.CurrencyID, LossAmount, TEXT("Death"));
                    bAnyLoss = true;
                }
            }
        }
    }
    
    if (bAnyLoss)
    {
        UE_LOG(LogTemp, Log, TEXT("사망으로 인한 화폐 손실 처리됨"));
    }
}

void UHSMetaCurrency::ResetAllCurrencies()
{
    for (auto& Pair : CurrencyBalances)
    {
        int32 OldAmount = Pair.Value;
        Pair.Value = 0;
        
        ProcessCurrencyChange(Pair.Key, OldAmount, 0, TEXT("Reset"), TEXT("Debug"));
    }
    
    // 통계도 초기화
    for (auto& Pair : TotalEarned)
    {
        Pair.Value = 0;
    }
    for (auto& Pair : TotalSpent)
    {
        Pair.Value = 0;
    }
    
    // 거래 기록 정리
    TransactionHistory.Empty();
    
    InvalidateCache();
    
    UE_LOG(LogTemp, Warning, TEXT("모든 화폐가 초기화되었습니다!"));
}

bool UHSMetaCurrency::ValidateCurrencyData() const
{
    bool bIsValid = true;
    
    // 각 화폐의 유효성 검증
    for (const auto& Pair : CurrencyBalances)
    {
        const FString& CurrencyID = Pair.Key;
        int32 Amount = Pair.Value;
        
        if (!IsCurrencyTypeRegistered(CurrencyID))
        {
            UE_LOG(LogTemp, Error, TEXT("유효성 검증 실패: 등록되지 않은 화폐 타입 - %s"), *CurrencyID);
            bIsValid = false;
            continue;
        }
        
        if (!ValidateCurrencyAmount(CurrencyID, Amount))
        {
            UE_LOG(LogTemp, Error, TEXT("유효성 검증 실패: 잘못된 화폐 양 - %s: %d"), *CurrencyID, Amount);
            bIsValid = false;
        }
    }
    
    // 통계 일관성 검증
    for (const auto& Pair : TotalEarned)
    {
        if (Pair.Value < 0)
        {
            UE_LOG(LogTemp, Error, TEXT("유효성 검증 실패: 음수 획득량 - %s: %d"), *Pair.Key, Pair.Value);
            bIsValid = false;
        }
    }
    
    for (const auto& Pair : TotalSpent)
    {
        if (Pair.Value < 0)
        {
            UE_LOG(LogTemp, Error, TEXT("유효성 검증 실패: 음수 사용량 - %s: %d"), *Pair.Key, Pair.Value);
            bIsValid = false;
        }
    }
    
    return bIsValid;
}

void UHSMetaCurrency::InitializeDefaultCurrencyTypes()
{
    // 메타소울 (기본 화폐)
    {
        FHSCurrencyInfo Currency;
        Currency.CurrencyID = TEXT("MetaSouls");
        Currency.DisplayName = TEXT("메타소울");
        Currency.Description = TEXT("사냥으로 얻은 영혼의 힘. 영구적인 업그레이드에 사용됩니다.");
        Currency.MaxAmount = 999999;
        Currency.bHasMaxLimit = true;
        Currency.DisplayPriority = 0;
        Currency.bShowInUI = true;
        Currency.UICategory = TEXT("Primary");
        Currency.bCanBeTraded = false;
        Currency.bLoseOnDeath = true;
        Currency.DeathLossPercentage = 0.1f; // 10% 손실
        Currency.bAutoSave = true;
        RegisterCurrencyType(Currency);
    }
    
    // 에센스 포인트 (협동 화폐)
    {
        FHSCurrencyInfo Currency;
        Currency.CurrencyID = TEXT("EssencePoints");
        Currency.DisplayName = TEXT("에센스 포인트");
        Currency.Description = TEXT("팀워크와 협동으로 얻은 정수. 특별한 능력 언락에 사용됩니다.");
        Currency.MaxAmount = 99999;
        Currency.bHasMaxLimit = true;
        Currency.DisplayPriority = 1;
        Currency.bShowInUI = true;
        Currency.UICategory = TEXT("Primary");
        Currency.bCanBeTraded = false;
        Currency.bLoseOnDeath = false; // 협동 보상이므로 손실되지 않음
        Currency.bAutoSave = true;
        RegisterCurrencyType(Currency);
    }
    
    // 언락 포인트 (업적 화폐)
    {
        FHSCurrencyInfo Currency;
        Currency.CurrencyID = TEXT("UnlockPoints");
        Currency.DisplayName = TEXT("언락 포인트");
        Currency.Description = TEXT("업적 달성으로 얻은 포인트. 새로운 컨텐츠 언락에 사용됩니다.");
        Currency.MaxAmount = 9999;
        Currency.bHasMaxLimit = true;
        Currency.DisplayPriority = 2;
        Currency.bShowInUI = true;
        Currency.UICategory = TEXT("Secondary");
        Currency.bCanBeTraded = false;
        Currency.bLoseOnDeath = false; // 업적 보상이므로 손실되지 않음
        Currency.bAutoSave = true;
        RegisterCurrencyType(Currency);
    }
    
    UE_LOG(LogTemp, Log, TEXT("기본 화폐 타입 초기화 완료 - %d개"), CurrencyTypes.Num());
}

void UHSMetaCurrency::InitializeDefaultExchangeRates()
{
    // 메타소울 -> 에센스 포인트 교환 (10:1)
    {
        FHSCurrencyExchangeRate Rate;
        Rate.FromCurrency = TEXT("MetaSouls");
        Rate.ToCurrency = TEXT("EssencePoints");
        Rate.ExchangeRate = 0.1f; // 10 메타소울 = 1 에센스 포인트
        Rate.MinAmount = 10;
        Rate.MaxAmount = 1000;
        Rate.bIsEnabled = true;
        SetExchangeRate(Rate);
    }
    
    // 에센스 포인트 -> 메타소울 교환 (1:5)
    {
        FHSCurrencyExchangeRate Rate;
        Rate.FromCurrency = TEXT("EssencePoints");
        Rate.ToCurrency = TEXT("MetaSouls");
        Rate.ExchangeRate = 5.0f; // 1 에센스 포인트 = 5 메타소울
        Rate.MinAmount = 1;
        Rate.MaxAmount = 100;
        Rate.bIsEnabled = true;
        SetExchangeRate(Rate);
    }
    
    UE_LOG(LogTemp, Log, TEXT("기본 교환 비율 초기화 완료"));
}

void UHSMetaCurrency::RecordTransaction(const FHSCurrencyTransaction& Transaction)
{
    if (!bEnableTransactionLogging)
    {
        return;
    }
    
    TransactionHistory.Add(Transaction);
    
    // 최대 기록 수 제한
    if (TransactionHistory.Num() > MaxTransactionHistory)
    {
        int32 RemoveCount = TransactionHistory.Num() - MaxTransactionHistory;
        TransactionHistory.RemoveAt(0, RemoveCount);
    }
}

void UHSMetaCurrency::ProcessCurrencyChange(const FString& CurrencyID, int32 OldAmount, int32 NewAmount, 
                                           const FString& TransactionType, const FString& Source)
{
    // 거래 기록
    FHSCurrencyTransaction Transaction(CurrencyID, NewAmount - OldAmount, OldAmount, TransactionType, Source);
    RecordTransaction(Transaction);
    
    // 이벤트 발생
    OnCurrencyChanged.Broadcast(CurrencyID, OldAmount, NewAmount);
    
    if (TransactionType == TEXT("Add"))
    {
        OnCurrencyAdded.Broadcast(Transaction);
    }
    else if (TransactionType == TEXT("Spend"))
    {
        OnCurrencySpent.Broadcast(Transaction);
    }
    
    // 캐시 무효화
    InvalidateCache();
}

void UHSMetaCurrency::PerformAutoSave()
{
    if (bAutoSaveEnabled)
    {
        SaveCurrencyData();
        UE_LOG(LogTemp, VeryVerbose, TEXT("화폐 자동 저장 수행됨"));
    }
}

void UHSMetaCurrency::InvalidateCache()
{
    bCacheValid = false;
    CachedBalances.Empty();
}

FString UHSMetaCurrency::GenerateExchangeKey(const FString& FromCurrency, const FString& ToCurrency) const
{
    return FromCurrency + TEXT("_to_") + ToCurrency;
}

bool UHSMetaCurrency::ValidateCurrencyAmount(const FString& CurrencyID, int32 Amount) const
{
    if (const FHSCurrencyInfo* Info = CurrencyTypes.Find(CurrencyID))
    {
        return Info->IsAmountValid(Amount);
    }
    return false;
}

// === Enum 변환 함수들 구현 ===

FString UHSMetaCurrency::CurrencyTypeToString(EHSCurrencyType CurrencyType)
{
    switch (CurrencyType)
    {
        case EHSCurrencyType::MetaSouls:       return TEXT("MetaSouls");
        case EHSCurrencyType::EssencePoints:   return TEXT("EssencePoints");
        case EHSCurrencyType::UnlockPoints:    return TEXT("UnlockPoints");
        case EHSCurrencyType::CraftingTokens:  return TEXT("CraftingTokens");
        case EHSCurrencyType::RuneShards:      return TEXT("RuneShards");
        case EHSCurrencyType::ArcaneOrbs:      return TEXT("ArcaneOrbs");
        case EHSCurrencyType::HeroicMedals:    return TEXT("HeroicMedals");
        case EHSCurrencyType::DivineFragments: return TEXT("DivineFragments");
        case EHSCurrencyType::EventTokens:     return TEXT("EventTokens");
        case EHSCurrencyType::SeasonCoins:     return TEXT("SeasonCoins");
        case EHSCurrencyType::None:
        default:
            return TEXT("");
    }
}

EHSCurrencyType UHSMetaCurrency::StringToCurrencyType(const FString& CurrencyString)
{
    if (CurrencyString == TEXT("MetaSouls"))       return EHSCurrencyType::MetaSouls;
    if (CurrencyString == TEXT("EssencePoints"))   return EHSCurrencyType::EssencePoints;
    if (CurrencyString == TEXT("UnlockPoints"))    return EHSCurrencyType::UnlockPoints;
    if (CurrencyString == TEXT("CraftingTokens"))  return EHSCurrencyType::CraftingTokens;
    if (CurrencyString == TEXT("RuneShards"))      return EHSCurrencyType::RuneShards;
    if (CurrencyString == TEXT("ArcaneOrbs"))      return EHSCurrencyType::ArcaneOrbs;
    if (CurrencyString == TEXT("HeroicMedals"))    return EHSCurrencyType::HeroicMedals;
    if (CurrencyString == TEXT("DivineFragments")) return EHSCurrencyType::DivineFragments;
    if (CurrencyString == TEXT("EventTokens"))     return EHSCurrencyType::EventTokens;
    if (CurrencyString == TEXT("SeasonCoins"))     return EHSCurrencyType::SeasonCoins;
    
    return EHSCurrencyType::None;
}

int32 UHSMetaCurrency::AddCurrencyByType(EHSCurrencyType CurrencyType, int32 Amount, const FString& Source)
{
    FString CurrencyID = CurrencyTypeToString(CurrencyType);
    if (CurrencyID.IsEmpty() || CurrencyType == EHSCurrencyType::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 추가 실패: 유효하지 않은 화폐 타입"));
        return 0;
    }
    
    return AddCurrency(CurrencyID, Amount, Source);
}

bool UHSMetaCurrency::SpendCurrencyByType(EHSCurrencyType CurrencyType, int32 Amount, const FString& Source)
{
    FString CurrencyID = CurrencyTypeToString(CurrencyType);
    if (CurrencyID.IsEmpty() || CurrencyType == EHSCurrencyType::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("화폐 사용 실패: 유효하지 않은 화폐 타입"));
        return false;
    }
    
    return SpendCurrency(CurrencyID, Amount, Source);
}

int32 UHSMetaCurrency::GetCurrencyByType(EHSCurrencyType CurrencyType) const
{
    FString CurrencyID = CurrencyTypeToString(CurrencyType);
    if (CurrencyID.IsEmpty() || CurrencyType == EHSCurrencyType::None)
    {
        return 0;
    }
    
    return GetCurrency(CurrencyID);
}

bool UHSMetaCurrency::HasEnoughCurrencyByType(EHSCurrencyType CurrencyType, int32 Amount) const
{
    FString CurrencyID = CurrencyTypeToString(CurrencyType);
    if (CurrencyID.IsEmpty() || CurrencyType == EHSCurrencyType::None)
    {
        return false;
    }
    
    return HasEnoughCurrency(CurrencyID, Amount);
}
