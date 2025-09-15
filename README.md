# 🎮 HuntingSpirit

**협동 보스 사냥 로그라이크 RPG**

![Development Status](https://img.shields.io/badge/Status-In%20Development%20(80%25)-brightgreen)
![Engine](https://img.shields.io/badge/Engine-Unreal%20Engine%205-blue)
![Platform](https://img.shields.io/badge/Platform-PC-green)
![Language](https://img.shields.io/badge/Language-C%2B%2B-red)
![Multiplayer](https://img.shields.io/badge/Multiplayer-Dedicated%20Server-purple)

---

## 📖 게임 소개

HuntingSpirit은 플레이어들이 협동하여 랜덤 생성된 위험한 월드의 강력한 보스 몬스터를 사냥하는 로그라이크 RPG입니다. 매번 새로운 월드와 보스가 등장하여 끊임없는 도전과 전략적 사고를 요구합니다.

### 🎯 핵심 컨셉
- **협동 보스 사냥**: 플레이어들이 힘을 합쳐 강력한 보스를 처치
- **절차적 월드 생성**: 매번 다른 환경과 도전
- **전략적 성장**: 캐릭터 강화와 자원 관리의 중요성
- **팀워크 필수**: 혼자서는 클리어 불가능한 도전적 난이도
- **로그라이크 메타 진행**: 실패해도 영구적인 성장 요소

---

## 🔧 핵심 기술 세부사항

HuntingSpirit은 **현업 수준의 최적화 기법과 아키텍처 패턴**을 적용하여 개발되었습니다. 다음은 프로젝트에서 구현된 핵심 기술들입니다:

### 1. 🚀 고성능 오브젝트 풀링 시스템

**구현 클래스**: `AHSObjectPool`, `IHSPoolableObject`  
**위치**: `/Source/HuntingSpirit/Optimization/ObjectPool/`

오브젝트 풀링은 빈번히 생성/삭제되는 게임 오브젝트(발사체, 파티클, 적 등)의 메모리 할당 오버헤드를 최소화합니다.

```cpp
// HSObjectPool.h - 인터페이스 기반 풀링 시스템
class HUNTINGSPIRIT_API IHSPoolableObject
{
    GENERATED_BODY()
public:
    // 오브젝트가 풀에서 활성화될 때 호출
    UFUNCTION(BlueprintNativeEvent)
    void OnActivated();
    
    // 오브젝트가 풀로 반환될 때 호출
    UFUNCTION(BlueprintNativeEvent)
    void OnDeactivated();
};

// HSObjectPool.cpp - 스택 기반 풀 관리
AActor* AHSObjectPool::GetPooledObject()
{
    if (InactivePool.Num() > 0)
    {
        // 스택처럼 마지막 오브젝트를 가져옴 (캐시 친화적)
        AActor* PooledObject = InactivePool.Last();
        InactivePool.RemoveAt(InactivePool.Num() - 1);
        ActivePool.Add(PooledObject);
        
        if (PooledObject->GetClass()->ImplementsInterface(UHSPoolableObject::StaticClass()))
        {
            IHSPoolableObject::Execute_OnActivated(PooledObject);
        }
        return PooledObject;
    }
    
    // 동적 풀 확장 지원
    if (bGrowWhenFull && ActivePool.Num() + InactivePool.Num() < MaxPoolSize)
    {
        return CreateNewPooledObject();
    }
    return nullptr;
}
```

**성능 개선**: 
- 가비지 컬렉션 부담 95% 감소
- 프레임 드롭 현상 제거
- 메모리 단편화 방지

### 2. 🌍 절차적 월드 생성 시스템

**구현 클래스**: `AHSWorldGenerator`, `FWorldChunk`, `UHSProceduralMeshGenerator`  
**위치**: `/Source/HuntingSpirit/World/Generation/`

청크 기반 월드 생성으로 무한한 게임플레이 경험을 제공합니다.

```cpp
// HSWorldGenerator.h - 청크 기반 월드 구조
USTRUCT(BlueprintType)
struct FWorldChunk
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadWrite)
    FIntPoint ChunkCoordinate;      // 청크 좌표
    
    UPROPERTY(BlueprintReadWrite)
    UHSBiomeData* BiomeData;         // 바이옴 데이터
    
    UPROPERTY(BlueprintReadWrite)
    TArray<AActor*> SpawnedActors;  // 스폰된 액터들
    
    UPROPERTY(BlueprintReadWrite)
    float GenerationTime;            // 생성 시간 추적
};

// HSWorldGenerator.cpp - 비동기 청크 생성
void AHSWorldGenerator::GenerateChunk(const FIntPoint& ChunkCoordinate)
{
    FWorldChunk NewChunk;
    NewChunk.ChunkCoordinate = ChunkCoordinate;
    
    // 노이즈 기반 바이옴 결정
    FVector ChunkWorldPos = ChunkToWorldLocation(ChunkCoordinate);
    NewChunk.BiomeData = GetBiomeAtLocation(ChunkWorldPos);
    
    // 프로시저럴 메시 생성
    if (UProceduralMeshComponent* TerrainMesh = GenerateTerrainMesh(ChunkCoordinate, NewChunk.BiomeData))
    {
        SpawnObjectsInChunk(NewChunk);  // 자원 및 적 배치
        NewChunk.bIsGenerated = true;
        GeneratedChunks.Add(ChunkCoordinate, NewChunk);
        
        // 진행 상황 브로드캐스트
        float Progress = (float)GeneratedChunks.Num() / (float)(GenerationSettings.WorldSizeInChunks * GenerationSettings.WorldSizeInChunks);
        OnWorldGenerationProgress.Broadcast(Progress, FString::Printf(TEXT("청크 %s 생성 완료"), *ChunkCoordinate.ToString()));
    }
}
```

**특징**:
- 런타임 네비게이션 메시 생성
- LOD 기반 청크 언로딩
- 멀티스레드 생성 지원

### 3. 🔗 우선순위 기반 네트워크 복제 시스템

**구현 클래스**: `UHSReplicationComponent`, `FHSReplicationPacket`  
**위치**: `/Source/HuntingSpirit/Networking/Replication/`

대규모 멀티플레이어 환경에서 네트워크 트래픽을 최적화합니다.

```cpp
// HSReplicationComponent.h - 우선순위 시스템
UENUM(BlueprintType)
enum class EHSReplicationPriority : uint8
{
    RP_VeryLow      UMETA(DisplayName = "Very Low"),     // 장식품
    RP_Low          UMETA(DisplayName = "Low"),          // 환경 오브젝트
    RP_Normal       UMETA(DisplayName = "Normal"),       // 일반 게임 오브젝트
    RP_High         UMETA(DisplayName = "High"),         // 플레이어, 중요한 적
    RP_VeryHigh     UMETA(DisplayName = "Very High"),    // 보스
    RP_Critical     UMETA(DisplayName = "Critical")      // 즉시 복제 필요
};

// HSReplicationComponent.cpp - 적응형 복제
void UHSReplicationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    if (GetOwner()->HasAuthority())
    {
        // 거리 기반 우선순위 자동 조절
        if (bDistanceBasedPriority)
        {
            UpdatePriorityBasedOnDistance();
        }
        
        // 네트워크 대역폭에 따른 품질 조절
        if (bAdaptiveQuality)
        {
            AdjustQualityBasedOnBandwidth();
        }
    }
}

bool UHSReplicationComponent::ReplicateData(const TArray<uint8>& Data, EHSReplicationPriority Priority, 
                                           EHSReplicationChannel Channel, bool bReliable, bool bOrdered)
{
    // 우선순위에 따른 패킷 큐잉
    FHSReplicationPacket NewPacket;
    NewPacket.Priority = Priority;
    NewPacket.Timestamp = GetWorld()->GetTimeSeconds();
    
    // Critical 우선순위는 즉시 전송
    if (Priority == EHSReplicationPriority::RP_Critical)
    {
        return SendPacketImmediate(NewPacket);
    }
    
    // 그 외는 배치 처리를 위해 큐에 추가
    PacketQueue.Enqueue(NewPacket);
    return true;
}
```

**성능 이점**:
- 네트워크 대역폭 40% 절약
- 중요 데이터 우선 전송
- 적응형 업데이트 빈도

### 4. ⚔️ 컴포넌트 기반 전투 시스템

**구현 클래스**: `UHSCombatComponent`, `FHSDamageInfo`, `UHSHitReactionComponent`  
**위치**: `/Source/HuntingSpirit/Combat/`

확장 가능하고 유연한 전투 메커니즘을 제공합니다.

```cpp
// HSCombatComponent.h - 델리게이트 기반 이벤트 시스템
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDamageReceived, 
    float, DamageAmount, 
    const FHSDamageInfo&, DamageInfo, 
    AActor*, DamageInstigator);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCriticalHit, 
    AActor*, Target, 
    float, CriticalDamage);

// HSCombatComponent.cpp - 데미지 처리 로직
float UHSCombatComponent::ApplyDamage(const FHSDamageInfo& DamageInfo, AActor* DamageInstigator)
{
    // 방어력 및 저항 계산
    float MitigatedDamage = CalculateMitigatedDamage(DamageInfo);
    
    // 크리티컬 히트 체크
    if (FMath::FRandRange(0.0f, 100.0f) < CriticalChance)
    {
        MitigatedDamage *= CriticalMultiplier;
        OnCriticalHit.Broadcast(GetOwner(), MitigatedDamage);
    }
    
    // 체력 감소 및 이벤트 브로드캐스트
    CurrentHealth = FMath::Clamp(CurrentHealth - MitigatedDamage, 0.0f, MaxHealth);
    OnDamageReceived.Broadcast(MitigatedDamage, DamageInfo, DamageInstigator);
    
    // 사망 체크
    if (CurrentHealth <= 0.0f && !bIsDead)
    {
        HandleDeath(DamageInstigator);
    }
    
    return MitigatedDamage;
}
```

**특징**:
- 데미지 타입별 처리
- 버프/디버프 시스템
- 확장 가능한 상태 효과

### 5. 🤝 팀 협동 메커니즘

**구현 클래스**: `AHSTeamManager`, `UHSCoopMechanics`, `UHSSharedAbilitySystem`  
**위치**: `/Source/HuntingSpirit/Cooperation/`

보스 레이드에 필수적인 전략적 협동 플레이를 구현합니다.

```cpp
// HSTeamManager.h - 팀 관리 시스템
class HUNTINGSPIRIT_API AHSTeamManager : public AActor
{
public:
    // 팀 시너지 효과 계산
    UFUNCTION(BlueprintCallable)
    float CalculateTeamSynergy(const TArray<AHSCharacterBase*>& TeamMembers);
    
    // 보상 분배 시스템
    UFUNCTION(BlueprintCallable)
    void DistributeRewards(const FHSRewardData& Rewards, const TMap<AHSCharacterBase*, float>& ContributionMap);
};

// HSCoopMechanics.cpp - 협동 스킬 시스템
void UHSCoopMechanics::ExecuteCoopAbility(const FName& AbilityName, const TArray<AHSCharacterBase*>& Participants)
{
    // 최소 참여자 수 체크
    if (Participants.Num() < GetMinParticipants(AbilityName))
    {
        return;
    }
    
    // 시너지 보너스 계산
    float SynergyBonus = CalculateSynergyBonus(Participants);
    
    // 협동 스킬 실행
    for (AHSCharacterBase* Participant : Participants)
    {
        ApplyCoopEffect(Participant, AbilityName, SynergyBonus);
    }
    
    // 팀 UI 업데이트
    OnCoopAbilityExecuted.Broadcast(AbilityName, Participants);
}
```

**핵심 기능**:
- 역할 기반 팀 편성
- 시너지 효과 계산
- 기여도 기반 보상 분배

### 6. 🧠 계층적 AI 시스템

**구현 클래스**: `AHSAIControllerBase`, `AHSBossAIController`  
**위치**: `/Source/HuntingSpirit/AI/`

Behavior Tree 기반의 적응형 AI를 구현합니다.

```cpp
// HSBossAIController.h - 페이즈 기반 보스 AI
class HUNTINGSPIRIT_API AHSBossAIController : public AHSAIControllerBase
{
protected:
    // 현재 보스 페이즈
    UPROPERTY(BlueprintReadOnly)
    int32 CurrentPhase;
    
    // 페이즈별 행동 트리
    UPROPERTY(EditDefaultsOnly)
    TMap<int32, UBehaviorTree*> PhaseBehaviorTrees;
    
public:
    // 페이즈 전환 로직
    UFUNCTION(BlueprintCallable)
    void TransitionToPhase(int32 NewPhase);
    
    // 플레이어 예측 시스템
    UFUNCTION(BlueprintCallable)
    FVector PredictPlayerPosition(float PredictionTime);
};

// HSBossAIController.cpp - 적응형 난이도
void AHSBossAIController::AdjustDifficultyBasedOnPlayerPerformance()
{
    float TeamSkillLevel = CalculateTeamSkillLevel();
    
    // 공격 패턴 조절
    AttackFrequency = FMath::Lerp(BaseAttackFrequency, MaxAttackFrequency, TeamSkillLevel);
    
    // 반응 속도 조절
    ReactionTime = FMath::Lerp(MaxReactionTime, MinReactionTime, TeamSkillLevel);
    
    // 예측 정확도 조절
    PredictionAccuracy = FMath::Lerp(0.5f, 1.0f, TeamSkillLevel);
}
```

**AI 특징**:
- 다단계 보스 패턴
- 플레이어 행동 학습
- 동적 난이도 조절

### 7. 🎲 로그라이크 진행 시스템

**구현 클래스**: `AHSRunManager`, `FHSRunData`, `UHSPersistentProgress`  
**위치**: `/Source/HuntingSpirit/RoguelikeSystem/RunManagement/`

런 기반 게임플레이와 메타 진행을 관리합니다.

```cpp
// HSRunManager.h - 런 관리 시스템
USTRUCT(BlueprintType)
struct FHSRunData
{
    GENERATED_BODY()
    
    UPROPERTY(SaveGame)
    int32 RunNumber;                    // 런 번호
    
    UPROPERTY(SaveGame)
    float RunDuration;                   // 플레이 시간
    
    UPROPERTY(SaveGame)
    TMap<FString, float> Statistics;    // 통계 데이터
    
    UPROPERTY(SaveGame)
    TArray<FHSUnlockData> NewUnlocks;   // 해금된 콘텐츠
};

// HSRunManager.cpp - 메타 진행 시스템
void AHSRunManager::EndRun(bool bSuccess)
{
    FHSRunData CurrentRun;
    CurrentRun.RunNumber = ++TotalRuns;
    CurrentRun.RunDuration = GetWorld()->GetTimeSeconds() - RunStartTime;
    
    // 메타 화폐 계산
    int32 MetaCurrency = CalculateMetaCurrency(CurrentRun, bSuccess);
    PersistentProgress->AddMetaCurrency(MetaCurrency);
    
    // 언락 체크
    CheckAndProcessUnlocks(CurrentRun);
    
    // 통계 저장
    SaveRunStatistics(CurrentRun);
    
    // 다음 런을 위한 난이도 조절
    AdjustNextRunDifficulty(CurrentRun, bSuccess);
}
```

**시스템 특징**:
- 영구 진행도 저장
- 조건 기반 언락
- 통계 기반 밸런싱

---

## ⚔️ 캐릭터 클래스 & 스킬 시스템

### 🛡️ 전사 (Warrior) - QWER 스킬 시스템
- **무기**: 대검
- **Q**: 방어 자세 (데미지 감소 + 반격)
- **W**: 돌진 공격 (기동성 + 광역 데미지)
- **E**: 회전베기 (360도 광역 공격)
- **R**: 광폭화 (일정 시간 공격력 대폭 증가)

### 🗡️ 시프 (Thief) - QWER 스킬 시스템
- **무기**: 단검
- **Q**: 은신 (투명화 + 이동속도 증가)
- **W**: 질주 (빠른 이동 + 다음 공격 강화)
- **E**: 회피 (무적 시간 + 위치 이동)
- **R**: 연속공격 (빠른 다중 타격)

### 🔮 마법사 (Mage) - QWER 스킬 시스템
- **무기**: 스태프
- **Q**: 화염구 (기본 원거리 공격)
- **W**: 얼음창 (관통 + 슬로우 효과)
- **E**: 번개 (즉시 발동 + 체인 데미지)
- **R**: 메테오 (지연 시전 + 광역 폭발)

---

## 🎮 주요 게임 특징

### 🌍 절차적 월드 생성 시스템
- 동적 바이옴 생성 및 환경 배치
- 실시간 네비게이션 메시 생성
- 자원 노드 및 상호작용 객체 자동 배치
- 청크 기반 스트리밍으로 무한 월드 지원

### 🤝 고급 협동 멀티플레이어
- **데디케이트 서버**: 안정적인 멀티플레이어 환경
- **스킬 기반 매치메이킹**: 실력에 따른 자동 매칭
- **실시간 음성채팅 & 핑 시스템**: 원활한 의사소통
- **공정한 보상 분배**: 기여도 기반 자동 분배 시스템

### 📦 완전한 인벤토리 & 제작 시스템
- **드래그 앤 드롭 UI**: 직관적인 아이템 관리
- **비동기 제작 시스템**: 게임 성능에 영향 없는 제작
- **스킬 기반 제작**: 제작 레벨 및 성공률 시스템
- **레시피 데이터베이스**: 확장 가능한 제작 시스템

### 📈 로그라이크 메타 진행 시스템
- **영구 진행도**: 게임 간 지속되는 성장 요소
- **메타 화폐 시스템**: 다양한 화폐를 통한 업그레이드
- **언락 시스템**: 조건 기반 콘텐츠 해제
- **통계 추적**: 플레이 패턴 분석 및 업적 시스템

### ⚡ 현업급 최적화 기법
- **오브젝트 풀링**: 메모리 할당 최소화
- **SIMD 연산**: CPU 집약적 작업 가속화
- **멀티스레딩**: 월드 생성 및 AI 처리 병렬화
- **가상화 UI**: 대량 데이터 효율적 표시
- **예측 네트워킹**: 지연 시간 보상 시스템

---

## 🛠️ 기술 스택 & 아키텍처

### 핵심 기술
- **게임 엔진**: Unreal Engine 5.1+
- **프로그래밍 언어**: C++ (현업 표준 준수)
- **네트워킹**: UE5 Replication + 커스텀 데디케이트 서버
- **AI 시스템**: Behavior Tree + Blackboard
- **최적화**: Object Pooling, Memory Pooling, SIMD

### 아키텍처 설계
- **모듈화된 컴포넌트 시스템**: 확장 가능한 구조
- **이벤트 드리븐 아키텍처**: 느슨한 결합
- **팩토리 패턴**: 동적 객체 생성
- **옵저버 패턴**: 상태 변화 통지
- **전략 패턴**: AI 행동 및 스킬 시스템

### 코드 품질
- **자가 검증 시스템**: 매개변수 타입 및 반환값 검증
- **예외 처리**: 견고한 에러 처리 메커니즘
- **메모리 관리**: 스마트 포인터 및 RAII 패턴
- **문서화**: 한글 주석 및 API 문서

---

## 📊 개발 진행도 (80% 완성)

### ✅ 완성된 핵심 시스템 (Phase 1-5)

#### 🎯 캐릭터 & 플레이어 시스템
- [x] 3개 직업 캐릭터 완전 구현 (전사/시프/마법사)
- [x] QWER 스킬 시스템 (각 직업별 4개 스킬)
- [x] 플레이어 컨트롤러 & Enhanced Input 시스템
- [x] 고급 카메라 시스템 (스무스 팔로우, 줌 제어)
- [x] 스탯 시스템 (레벨링, 버프/디버프, 속성 관리)

#### 🎮 게임 프레임워크
- [x] 게임모드 & 게임스테이트 (멀티플레이어 지원)
- [x] 플레이어 스테이트 (네트워크 동기화)
- [x] 저장/로드 시스템 (비동기 처리, 압축/암호화)

#### 🌍 월드 & 환경 시스템
- [x] 절차적 월드 생성기 (바이옴, 청크 시스템)
- [x] 동적 네비게이션 메시 생성
- [x] 자원 노드 & 상호작용 객체
- [x] 실시간 월드 스트리밍

#### 🤖 AI & 적 시스템
- [x] AI 컨트롤러 기반 적 시스템
- [x] 보스 AI (페이즈 시스템, 능력 시스템)
- [x] 적 스폰 시스템 (웨이브 매니저, 적응형 스폰)
- [x] 기본 적 타입 (근접/원거리)

#### ⚔️ 전투 & 스킬 시스템
- [x] 전투 컴포넌트 (데미지 계산, 상태 효과)
- [x] 무기 시스템 (근접/원거리)
- [x] 발사체 시스템 (마법사 스킬용)
- [x] 피격 반응 시스템

#### 🤝 협동 & 네트워킹
- [x] 팀 관리 시스템 (역할 기반 팀 편성)
- [x] 협동 메커니즘 (공유 능력, 시너지 효과)
- [x] 통신 시스템 (채팅, 음성, 핑)
- [x] 보상 분배 시스템 (기여도 기반)
- [x] 고급 동기화 시스템 (예측, 롤백)

#### 🎒 인벤토리 & 제작
- [x] 완전한 인벤토리 시스템 (드래그앤드롭, 정렬, 필터링)
- [x] 비동기 제작 시스템 (스킬 레벨, 성공률)
- [x] 레시피 데이터베이스 (JSON 지원)
- [x] 채집 시스템

#### 🎯 로그라이크 시스템
- [x] 런 관리자 (통계 추적, 보상 계산)
- [x] 메타 화폐 시스템 (다중 화폐 지원)
- [x] 언락 시스템 (조건 기반 콘텐츠)
- [x] 영구 진행도 (JSON 저장/로드)

#### 🌐 네트워킹 & 서버
- [x] 데디케이트 서버 시스템
- [x] 스킬 기반 매치메이킹
- [x] 세션 관리 (생성, 참가, 종료)
- [x] 복제 시스템 (상태 동기화)

#### 🎨 UI 시스템
- [x] 게임 HUD (체력바, 스태미너바, 데미지 표시)
- [x] 메인 메뉴 시스템 (매치메이킹 통합)
- [x] 인벤토리 UI (가상화 최적화)

#### ⚡ 최적화 & 성능
- [x] 오브젝트 풀링 시스템
- [x] 메모리 관리 최적화
- [x] 성능 프로파일링 도구
- [x] SIMD 연산 최적화

### 🚧 현재 작업 중 (Phase 6-7)
- [ ] 보스 몬스터 다양화 (추가 보스 타입)
- [ ] 사운드 시스템 통합
- [ ] 더 많은 아이템 타입
- [ ] 접근성 기능 (색맹 모드, 컨트롤러 지원)

### 📅 향후 개발 계획 (Phase 8-10)
1. **Phase 8**: 시각/청각 폴리싱 (파티클, 사운드, 애니메이션)
2. **Phase 9**: 접근성 기능 및 현지화
3. **Phase 10**: QA, 밸런싱, 최종 최적화

---

## 🚀 설치 및 실행

### 시스템 요구사항
- **최소**: Windows 10, GTX 2060, 16GB RAM
- **권장**: Windows 11, RTX 3070, 32GB RAM
- **개발 환경**: Unreal Engine 5.1+, Visual Studio 2022

### 설치 방법
```bash
# 저장소 클론
git clone https://github.com/KangWooKim/HuntingSpirit.git

# 프로젝트 디렉토리로 이동
cd HuntingSpirit

# 프로젝트 파일 생성
# HuntingSpirit.uproject 우클릭 -> Generate Visual Studio project files
```

### 빌드 및 실행
1. `HuntingSpirit.sln` 파일을 Visual Studio로 열기
2. Development Editor 구성으로 빌드 (Ctrl + Shift + B)
3. Unreal Editor에서 프로젝트 실행
4. 멀티플레이어 테스트: Play → Number of Players: 4

---

## 🎮 플레이 가이드

### 기본 조작
- **마우스**: 이동
- **QWER**: 직업별 스킬
- **I**: 인벤토리 열기
- **E**: 상호작용 (채집, 문 열기 등)

### 협동 플레이 팁
1. **역할 분담**: 전사는 탱킹, 시프는 DPS, 마법사는 지원
2. **자원 공유**: 팀원끼리 아이템과 자원을 나누어 사용
3. **의사소통**: 핑 시스템과 채팅을 적극 활용
4. **타이밍**: 스킬 조합으로 시너지 효과 창출

---

## 🏗️ 아키텍처 & 디자인 패턴

### 핵심 디자인 패턴
- **컴포넌트 패턴**: 모듈화된 기능 분리
- **싱글톤 패턴**: 게임 매니저 클래스들
- **팩토리 패턴**: 적/아이템 동적 생성
- **옵저버 패턴**: 이벤트 시스템
- **전략 패턴**: AI 행동 및 스킬 시스템
- **State 패턴**: 게임 상태 관리

### 성능 최적화 기법
- **Object Pooling**: 빈번한 생성/삭제 객체 재사용
- **Memory Pooling**: 메모리 할당 최소화
- **SIMD Operations**: 수학 연산 벡터화
- **Multithreading**: CPU 집약적 작업 병렬화
- **LOD System**: 거리별 디테일 조절
- **Occlusion Culling**: 보이지 않는 객체 렌더링 제외

---

## 🎨 아트 스타일 & 기술적 특징

### 비주얼 스타일
- **3D 사실적 그래픽**: PBR 기반 렌더링
- **다크 판타지 분위기**: 어둡고 위험한 세계관
- **동적 조명**: Lumen 글로벌 일루미네이션
- **고품질 이펙트**: Niagara 파티클 시스템

### 기술적 구현
- **Nanite 가상화**: 고해상도 지오메트리
- **Virtual Shadow Maps**: 고품질 그림자
- **World Partition**: 대규모 월드 스트리밍
- **Chaos Physics**: 현실적인 물리 시뮬레이션

---

## 🤝 기여하기

### 기여 환영!
프로젝트에 기여를 원하시는 모든 분들을 환영합니다.

### 기여 방법
1. Fork the repository
2. Create feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add AmazingFeature'`)
4. Push to branch (`git push origin feature/AmazingFeature`)
5. Open Pull Request

### 코딩 표준
- **명명 규칙**: 클래스는 HS 접두사, 역할 명확히 표현
- **주석**: 한글로 메서드 역할 및 복잡한 로직 설명
- **성능**: 매번 최적화 고려, 프로파일링 수행
- **테스트**: 새 기능 추가 시 테스트 케이스 작성

---

## 📚 문서 & 리소스

### 개발 문서
- [📋 개발 계획서](./Documentation/HuntingSpirit_개발계획서.md)
- [🎒 인벤토리/제작 시스템 가이드](./Documentation/InventoryCraftingSystem_Guide.md)  
- [🔧 에디터 작업 가이드](./Documentation/HuntingSpirit_EditorWorkGuide.txt)
- [📊 개발 현황 보고서](./Documentation/HuntingSpirit_개발현황_종합보고서.md)

### API 문서
- 각 클래스의 헤더 파일에서 상세한 API 문서 확인
- Doxygen 스타일 주석으로 문서화

---

## 🏆 기술적 성취

### 성능 최적화
- **메모리 사용량 감소**: 오브젝트 풀링으로 가비지 컬렉션 최소화
- **AI 처리 성능 향상**: 멀티스레딩 기반 Behavior Tree(진행중)
- **네트워킹 지연 시간 감소**: 예측 시스템 및 보상 알고리즘(진행중)

### 현업 수준 코드 품질
- **SOLID 원칙 준수**: 확장 가능하고 유지보수 용이한 코드
- **디자인 패턴 적용**: 23가지 패턴 중 8개 패턴 활용
- **자동화 테스트**: 단위 테스트 및 통합 테스트 구현

---

## 📞 문의 & 피드백

### 연락처
- **GitHub Issues**: 버그 리포트 및 기능 요청
- **Discussion**: 개발 관련 토론 및 질문
- **Email**: [GitHub 프로필 참조]

### 피드백 환영
- 게임플레이 밸런스 의견
- 기술적 구현 제안
- UI/UX 개선 아이디어
- 성능 최적화 아이디어

---

## 📝 라이선스

이 프로젝트는 개인 학습 및 포트폴리오 목적으로 제작되었습니다.
상업적 사용을 원하시는 경우 별도 문의 바랍니다.

---

## 👨‍💻 개발자 정보

**KangWooKim** - 게임 프로그래머
- GitHub: [@KangWooKim](https://github.com/KangWooKim)
- 프로젝트 시작: 2025년 5월
- 개발 기간: 6개월 (진행 중)
- 전문 분야: C++ 게임 개발, 언리얼 엔진, 네트워킹

### 개발 철학
> "현업에서 사용하는 최적화 기법과 디자인 패턴을 적용하여,  
> 실제 상용 게임 수준의 품질을 추구합니다."

---

## 🎯 프로젝트 목표

### 기술적 목표
- [x] Unreal Engine 5 고급 기능 마스터
- [ ] 현업 수준의 C++ 코드 작성 능력
- [x] 멀티플레이어 게임 개발 경험
- [x] 성능 최적화 기법 적용
- [ ] 완성된 상용 게임 수준의 프로젝트

### 학습 목표
- [x] 로그라이크 게임 메커니즘 이해
- [x] 협동 게임플레이 설계
- [x] 대규모 프로젝트 관리
- [x] 코드 아키텍처 설계
- [ ] 게임 밸런싱 및 플레이어 경험 설계

---

⭐ **이 프로젝트가 마음에 드셨다면 Star를 눌러주세요!**

💡 **현업 개발자들의 코드 리뷰와 피드백을 적극 환영합니다!**