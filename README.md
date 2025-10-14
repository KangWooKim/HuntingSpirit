# 🎮 HuntingSpirit

**협동 보스 사냥 로그라이크 RPG**

![Development Status](https://img.shields.io/badge/Status-In%20Development%20(80%25)-brightgreen)
![Engine](https://img.shields.io/badge/Engine-Unreal%20Engine%205-blue)
![Platform](https://img.shields.io/badge/Platform-PC-green)
![Language](https://img.shields.io/badge/Language-C%2B%2B-red)
![Multiplayer](https://img.shields.io/badge/Multiplayer-Dedicated%20Server-purple)

---

## 📖 게임 소개

HuntingSpirit는 플레이어들이 협동하여 절차적으로 생성된 위험 지역을 돌파하고, 강력한 레이드 보스를 사냥하는 로그라이크 RPG입니다. 매 라운드마다 세계와 적 구성이 새롭게 생성되며, 영구 성장 요소를 통해 반복적인 도전을 유도합니다.

---

## 🧠 아키텍처 하이라이트

본 프로젝트는 대규모 협동 플레이를 염두에 둔 **네트워크 인프라**, **퍼포먼스 최적화**, **운영 자동화**를 중점으로 설계되었습니다. 아래는 현재 코드베이스에 구현된 핵심 기술 요약입니다.

### 1. 🌐 지능형 매치메이킹 파이프라인
**소스**: `Networking/Matchmaking/HSMatchmakingSystem.*`

- 실측 핑 데이터를 기반으로 지역 가중치를 계산하고, 세션 검색 결과에서 최적의 매치를 선별합니다.
- 세션 검색/참여를 Unreal Online Subsystem에 직접 위임해 실제 서버에 접속합니다.
- 매치 수락/거절 타임아웃, 지역별 대역폭 추적, 자동 재검색 로직을 포함합니다.

```cpp
// 지역별 핑 통계를 축적해 최적의 매치 후보를 고릅니다.
FHSMatchInfo MatchInfo = BuildMatchInfoFromResult(*BestResult, BestRegion);
UpdateMatchmakingStatus(EHSMatchmakingStatus::MatchFound);
OnMatchFound.Broadcast(MatchInfo);
```

### 2. 🎮 세션 라이프사이클 자동화
**소스**: `Networking/SessionHandling/HSSessionManager.*`

- 빠른 매칭은 검색 완료 시 조건에 맞는 세션을 자동 선택·참여합니다.
- 플레이어 추방/금지 시 OnlineSubsystem ID를 조회하여 실제 세션에서 제거합니다.
- 핑/패킷 손실률을 실시간 측정하고, 재연결 시도 시 마지막 검색 결과를 활용합니다.
- 24시간이 지난 밴 레코드와 오래된 세션 검색 결과를 자동 정리합니다.

```cpp
if (SessionInterface->KickPlayer(*LocalPlayerId, NAME_GameSession, *TargetPlayerId))
{
    UE_LOG(LogTemp, Log, TEXT("플레이어 추방 성공 - %s"), *PlayerName);
}
```

### 3. 🛠️ 전용 서버 오케스트레이션
**소스**: `Networking/DedicatedServer/HSDedicatedServerManager.*`

- CPU/메모리/네트워크/지연 시간 데이터를 플랫폼 API(Windows `GetSystemTimes`, Linux `/proc/stat`)로 측정합니다.
- JWT/Basic 토큰을 파싱해 인증하며, 보안 이벤트는 `Saved/Logs/SecurityEvents.log`에 축적됩니다.
- 서버 설정을 JSON으로 직렬화하여 `Saved/Server/HSServerConfig.json`에 저장합니다.

```cpp
if (ServerConfig->SaveConfigurationToFile(ConfigFilePath))
{
    UE_LOG(LogTemp, Log, TEXT("서버 설정 저장 완료 - %s"), *ConfigFilePath);
}
```

### 4. 🔁 거리 기반 어댑티브 복제
**소스**: `Networking/Replication/HSReplicationComponent.*`

- Critical 데이터는 즉시 전송, 일반 데이터는 배치 처리하여 대역폭을 절약합니다.
- 멀티캐스트 시 수신자와의 거리를 계산해 범위 밖 클라이언트를 자동 필터링합니다.
- 채널별 사용량과 대역폭 초과 이벤트를 추적합니다.

```cpp
const float Distance = FVector::Dist(Origin, TargetLocation);
if (Distance <= MaxDistance && DispatchPacketToClient(Connection, Packet, Payload))
{
    ++SuccessfulDispatches;
}
```

### 5. 🌍 프로시저럴 월드 & 내비게이션 스트리밍(구현중)
**소스**: `World/Generation/HSWorldGenerator.*`, `World/Navigation/HSNavMeshGenerator.*`

- 노이즈 기반 바이옴 결정, 멀티스레드 청크 생성, 런타임 메시 구축을 결합하여 무한 월드를 구성합니다.
- 생성된 지형에 맞춰 내비메시를 즉시 재빌드하며, 필요한 자원 노드(`HSResourceNode`)와 상호작용 오브젝트를 스폰합니다.
- 스트리밍 진행 상황을 델리게이트로 브로드캐스트해 로딩 UI와 동기화합니다.

```cpp
ProcessChunkGeneration();
if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
{
    if (APawn* PlayerPawn = PC->GetPawn())
    {
        UpdateChunksAroundPlayer(PlayerPawn->GetActorLocation());
    }
}
CleanupDistantChunks();
```

### 6. 🧠 다단계 보스 페이즈 시스템
**소스**: `Enemies/Bosses/HSBossPhaseSystem.*`, `Enemies/Bosses/HSBossAbilitySystem.*`, `AI/HSBossAIController.*`

- 보스 체력/기믹 조건에 따라 페이즈를 전환하며, 전환 중 무적 처리·연출 타이밍을 자동 관리합니다.
- 각 페이즈는 능력 세트와 패턴을 독립적으로 보유하고 RPC를 통해 모든 클라이언트와 공유됩니다.
- 페이즈 전이 횟수, 디버그 로그, 재진입 제한 등 레이드 밸런스 옵션을 제공합니다.

```cpp
if (NewPhase != CurrentPhase && CanTransitionToPhase(NewPhase))
{
    InternalSetPhase(NewPhase, /*bForce=*/false);
}
```

### 7. 💾 로그라이크 메타 진행 & 저장소
**소스**: `RoguelikeSystem/RunManagement/HSRunManager.*`, `RoguelikeSystem/Persistence/HSPersistentProgress.*`, `RoguelikeSystem/Progression/HSUnlockSystem.*`

- 런 시작/일시정지/종료를 서브시스템으로 관리하며, 보상 계산·통계 집계를 자동화합니다.
- 진행도는 JSON 기반으로 `Saved/SaveGames/`에 저장되며, 자동 저장 타이머와 버전 마이그레이션 후킹을 제공합니다.
- 승리/패배에 따라 메타 화폐, 해금 상태, 난이도 기록을 업데이트하여 반복 플레이 동기를 강화합니다.

```cpp
CurrentRun.Rewards = CalculateRunRewards();
OnRunCompleted.Broadcast(Result, CurrentRun.Rewards);
SaveProgressData();
```

### 8. 🤝 협동 시너지 네트워크(구현중)
**소스**: `Cooperation/TeamFormation/HSTeamFormationSystem.*`, `Cooperation/SharedAbilities/HSSharedAbilitySystem.*`, `Cooperation/Communication/HSCommunicationSystem.*`

- 팀 매칭 후 역할 배분, 합동 스킬 쿨다운, 복합 보상 분배를 서브시스템 단위로 끊어 관리합니다.
- 실시간 핑/핑백 툴, 합동 능력 발동 등 협력 액션을 멀티캐스트 RPC로 동기화합니다.
- `HSSynchronizationSystem`과 연계해 팀 버프·합동 공격의 타이밍을 서버 기준으로 보정합니다.

```cpp
if (SharedAbilitySystem->TryActivateSharedAbility(AbilityId, Participants))
{
    OnSharedAbilityActivated.Broadcast(AbilityId, Participants);
}
```

### 9. 🚀 성능 최적화 툴킷
**소스**: `Optimization/HSPerformanceOptimizer.*`

- 플랫폼별 CPU 사용률을 캐싱해 HUD/서버 모니터링에 제공하며, Windows/Linus 모두 지원합니다.
- `FHighPerformanceObjectPool`은 활성/최대 용량을 추적하며, 동적 확장과 비트마스크 기반 상태 플래그를 제공합니다.
- SIMD 벡터 배치 연산, XOR 델타 압축, 병렬 플레이어 업데이트 등 대량 데이터 처리를 최적화했습니다.

```cpp
float UHSPerformanceOptimizer::GetCurrentCPUUsage()
{
    float Usage = FPlatformProcess::GetCPUUsage();
    if (Usage >= 0.0f) { return CacheResult(Usage); }
    return QueryPlatformSpecificUsage();
}
```

### 10. 📦 오브젝트 풀링
**소스**: `Optimization/ObjectPool/HSObjectPool.*`

- 인터페이스 기반으로 액터 생성/비활성화 생명주기를 관리하며, 필요 시 자동 확장합니다.
- 풀링된 액터는 생성 즉시 숨김·충돌 비활성화 상태로 전환되어 즉시 재사용 가능합니다.

```cpp
AActor* AHSObjectPool::CreateNewPooledObject()
{
    AActor* NewObject = GetWorld()->SpawnActor<AActor>(PooledObjectClass, SpawnParams);
    NewObject->SetActorHiddenInGame(true);
    IHSPoolableObject::Execute_OnCreated(NewObject);
    return NewObject;
}
```

---

## 🧪 테스트 & 운영 체크리스트

- **Dedicated Server**: `HuntingSpiritServer` 타깃을 빌드하고 `-log`, `-port=<포트>` 옵션으로 실행합니다.
- **클라이언트 매칭**: `HSMatchmakingSystem`으로 빠른 매치/수동 매치를 호출해 지역 자동선택과 세션 참가가 동작하는지 검증하세요.
- **성능 모니터링**: 서버 실행 중 `HSDedicatedServerManager::OnPerformanceMetricsUpdated` 델리게이트를 구독해 실시간 데이터를 확인합니다.
- **풀 사용률 확인**: `UHSAdvancedMemoryManager::GetPoolUsageRatio`를 통해 런타임 오브젝트 풀 상태를 모니터링합니다.

---

## 📁 폴더 구조

```
Source/HuntingSpirit
├── AI/
│   ├── HSAIControllerBase.*
│   └── HSBossAIController.*
├── Characters/
│   ├── Base/HSCharacterBase.*
│   ├── Components/{HSAnimationComponent.*, HSCameraComponent.*}
│   ├── Player/
│   │   ├── HSPlayerCharacter.*, HSPlayerTypes.h
│   │   ├── Mage/HSMageCharacter.*
│   │   ├── Warrior/HSWarriorCharacter.*
│   │   └── Thief/HSThiefCharacter.*
│   └── Stats/{HSAttributeSet.*, HSLevelSystem.*, HSStatsComponent.*, HSStatsManager.*, HSLevelDataTable.h}
├── Combat/
│   ├── Damage/HSDamageType.h
│   ├── Projectiles/HSMagicProjectile.*
│   └── Weapons/HSWeaponBase.*
├── Cooperation/
│   ├── Communication/HSCommunicationSystem.*
│   ├── Rewards/HSRewardsSystem.*
│   ├── SharedAbilities/HSSharedAbilitySystem.*
│   ├── Synchronization/HSSynchronizationSystem.*
│   └── TeamFormation/HSTeamFormationSystem.*
├── Enemies/
│   ├── Base/HSEnemyBase.*
│   ├── Bosses/{HSBossBase.*, HSBossPhaseSystem.*, HSBossAbilitySystem.*}
│   ├── Regular/{HSBasicMeleeEnemy.*, HSBasicRangedEnemy.*}
│   └── Spawning/{HSWaveManager.*, HSEnemySpawner.*, HSSpawnPoint.*}
├── Gathering/
│   ├── Crafting/{HSCraftingSystem.*, HSRecipeDatabase.*}
│   ├── Harvesting/HSGatheringComponent.*
│   └── Inventory/{HSInventoryComponent.*, HSInventoryUI.*}
├── Items/HSItemBase.*
├── Networking/
│   ├── DedicatedServer/HSDedicatedServerManager.*
│   ├── Matchmaking/HSMatchmakingSystem.*
│   ├── Replication/HSReplicationComponent.*
│   └── SessionHandling/HSSessionManager.*
├── Optimization/
│   ├── HSPerformanceOptimizer.*
│   └── ObjectPool/HSObjectPool.*
├── RoguelikeSystem/
│   ├── Persistence/HSPersistentProgress.*
│   ├── Progression/{HSMetaCurrency.*, HSUnlockSystem.*}
│   └── RunManagement/HSRunManager.*
├── UI/
│   ├── HUD/HSGameHUD.*
│   ├── Menus/HSMainMenuWidget.*
│   └── Widgets/{HSHealthBarWidget.*, HSStaminaBarWidget.*, HSDamageNumberWidget.*}
└── World/
    ├── Environment/HSInteractableObject.*
    ├── Generation/{HSWorldGenerator.*, HSLevelChunk.*, HSProceduralMeshGenerator.*, HSBiomeData.*}
    ├── Navigation/{HSNavMeshGenerator.*, HSNavigationIntegration.*, HSRuntimeNavigation.*}
    └── Resources/HSResourceNode.*
```

---

## 🤝 기여 가이드 (요약)

- 기능 추가 시 관련 서브시스템(매칭/세션/서버/복제/최적화)별 책임을 명확히 구분해주세요.
- 새 API는 반드시 로그 카테고리(`LogHSPerformance`, `LogTemp`, …)를 지정해 디버깅 정보를 제공해야 합니다.
- 플랫폼 종속 기능(예: CPU 사용률)은 `#if PLATFORM_*` 매크로로 구분하고 대체 경로를 마련합니다.

---

## 📬 문의

- Discord Dev Room: `#huntingspirit-dev`
- Issue Tracker: 다중 플랫폼 성능, 네트워크 회귀, 매치메이킹 버그 중심으로 태깅해주세요.

_게임의 핵심은 협동과 도전입니다. 안정적인 네트워크와 최적화된 퍼포먼스 위에서 최고의 협동 경험을 제공하겠습니다._
