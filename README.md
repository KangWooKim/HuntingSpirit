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

- `DetectOptimalRegion`에서 누적 핑 통계를 사용해 지역을 재선택하고, `SkillToleranceBase`를 기점으로 허용 범위를 자동 확장합니다.
- 큐 처리/예상 대기시간/매치 수락 타이머를 분리해 UI와 서버 로그가 동일한 상태를 공유합니다.
- 검색 결과는 `PendingSessionResult`·`bHasPendingSessionResult`로 캐싱돼 재검색 없이도 세션 참여를 이어갑니다.

```cpp
GetWorld()->GetTimerManager().SetTimer(
    MatchmakingTimerHandle,
    this,
    &UHSMatchmakingSystem::ProcessMatchmakingQueue,
    1.0f,
    true
);
```

1초 주기의 큐 틱이 매치 상태를 지속적으로 재평가해 수동 새로고침 없이도 최신 정보를 유지합니다.

### 2. 🎮 세션 라이프사이클 자동화
**소스**: `Networking/SessionHandling/HSSessionManager.*`

- 모든 퍼블릭 API는 `ChangeSessionState`를 통해 상태 머신을 거치며, 실패 시 `HandleSessionError`가 재시도·로그를 처리합니다.
- 세션 생성 이후 하트비트, 재연결, 밴 기록 정리를 타이머로 구동해 장시간 플레이 시 리소스를 자동으로 정리합니다.
- 빠른 매치, 수동 검색, 초대 세션을 동일한 코드 경로에서 처리하고 `LastSearchResults`로 검색 결과를 재활용합니다.

```cpp
World->GetTimerManager().SetTimer(SessionHeartbeatTimer, this,
    &UHSSessionManager::ProcessSessionHeartbeat,
    SessionHeartbeatInterval, true);
```

세션 하트비트는 서버 응답이 없을 때 자동 탈퇴·재접속을 유도해 세션 드리프트를 막습니다.

### 3. 🛠️ 전용 서버 오케스트레이션
**소스**: `Networking/DedicatedServer/HSDedicatedServerManager.*`

- 환경별 설정(`LoadServerConfig`)을 읽어 서버 자원 할당·네트워크 리스너 초기화·플랫폼별 부트스트랩을 한 번에 수행합니다.
- 성능·네트워크·플레이어 메트릭을 주기적으로 수집해 `OnPerformanceMetricsUpdated` 델리게이트로 푸시합니다.
- 자동 세션 정리, 플레이어 타임아웃, 성능 최적화 루프를 토글 가능한 관리 기능으로 제공합니다.

```cpp
GetWorld()->GetTimerManager().SetTimer(
    PerformanceMonitoringTimerHandle,
    this,
    &UHSDedicatedServerManager::UpdatePerformanceMetrics,
    PerformanceUpdateInterval,
    true
);
```

주기적인 메트릭 업데이트가 CPU/메모리/네트워크 상황을 캐시해 게임 내 진단 화면과 운영 툴에서 바로 활용할 수 있습니다.

### 4. 🔁 거리 기반 어댑티브 복제
**소스**: `Networking/Replication/HSReplicationComponent.*`

- 거리 기반 우선순위(`UpdatePriorityBasedOnDistance`)와 채널별 빈도 테이블을 조합해 대역폭을 상황에 맞게 분배합니다.
- 압축·델타·배치 전송을 지원하는 `PacketQueue`로 패킷 발송을 지연시켜 스파이크를 완화합니다.
- `AdjustQualityBasedOnBandwidth`가 실시간 대역폭 사용량을 감시해 품질을 올리거나 낮춥니다.

```cpp
if (MinDistance < MaxReplicationDistance * 0.2f)
{
    NewPriority = EHSReplicationPriority::RP_VeryHigh;
}
SetReplicationPriority(NewPriority);
```

플레이어와의 거리에 따라 우선순위를 재조정해 전투 중 핵심 액터가 항상 최신 상태로 복제됩니다.

### 5. 🌍 프로시저럴 월드 & 내비게이션 스트리밍
**소스**: `HuntingSpiritGameMode.cpp`, `World/Generation/HSWorldGenerator.*`, `World/Navigation/HSNavMeshGenerator.*`

- `AHuntingSpiritGameMode::InitializeWorldGeneration`이 월드 생성기와 스폰러를 런타임에 스폰하고 진행 상황을 로깅합니다.
- `AHSWorldGenerator`는 청크 큐, 바이옴 맵, 보스 스폰 청크를 관리하며 플레이어 주변만 유지합니다.
- 생성된 지형에 맞춰 내비메시·자원 노드·상호작용 오브젝트를 즉시 갱신해 탐험 흐름을 끊지 않습니다.

```cpp
while (!ChunkGenerationQueue.IsEmpty() && ChunksGeneratedThisFrame < GenerationSettings.MaxChunksToGeneratePerFrame)
{
    ChunkGenerationQueue.Dequeue(ChunkToGenerate);
    GenerateChunk(ChunkToGenerate);
    ++ChunksGeneratedThisFrame;
}
```

프레임당 생성량을 제한하는 큐 기반 스트리밍이 긴 로딩 없이도 넓은 월드를 유지하게 합니다.

### 6. 🧠 다단계 보스 페이즈 시스템
**소스**: `Enemies/Bosses/HSBossPhaseSystem.*`, `Enemies/Bosses/HSBossAbilitySystem.*`, `AI/HSBossAIController.*`

- 체력 기반 페이즈 결정, 전환 중 무적, 연출 타이밍을 `FHSPhaseTransitionInfo`로 선언적으로 관리합니다.
- RPC로 클라이언트에 페이즈 상태를 복제하고, 능력 시스템과 연계해 각 페이즈별 스킬 세트를 교체합니다.
- 디버그 로깅·재진입 제한·페이즈 횟수 누적 등 레이드 튜닝 옵션을 제공해 반복 테스트를 단순화합니다.

```cpp
if (NewPhase != CurrentPhase && CanTransitionToPhase(NewPhase))
{
    InternalSetPhase(NewPhase, /*bForce=*/false);
}
```

헬스 퍼센트 변화가 조건을 충족하면 서버가 즉시 전환을 트리거해 모든 클라이언트가 동일한 연출을 보게 됩니다.

### 7. 💾 로그라이크 메타 진행 & 저장소
**소스**: `RoguelikeSystem/RunManagement/HSRunManager.*`, `RoguelikeSystem/Persistence/HSPersistentProgress.*`, `RoguelikeSystem/Progression/HSUnlockSystem.*`

- 런 수명주기와 통계를 `HSRunManager`가 관리하고, 메타 화폐·해금 상태는 `HSUnlockSystem`으로 모듈화했습니다.
- 진행 데이터는 JSON 기반으로 `Saved/SaveGames/HuntingSpiritProgress.json`에 저장되며 버전 마이그레이션 훅을 제공합니다.
- 자동 저장·수동 저장 모두 `SaveProgressData` 경로를 사용해 UI와 운영 도구의 일관성을 확보했습니다.

```cpp
CurrentRun.Rewards = CalculateRunRewards();
OnRunCompleted.Broadcast(Result, CurrentRun.Rewards);
SaveProgressData();
```

런 종료 시 계산된 보상과 통계가 즉시 저장돼 충돌이나 강제 종료에서도 메타 진행이 유지됩니다.

### 8. 🤝 협동 시너지 네트워크
**소스**: `Cooperation/HSTeamManager.*`, `Cooperation/SharedAbilities/HSSharedAbilitySystem.*`, `Cooperation/Communication/HSCommunicationSystem.*`, `Cooperation/Synchronization/HSSynchronizationSystem.*`

- `HSTeamManager`가 팀 데이터와 슬롯 제한을 복제하고, 주기적 정리 타이머로 비활성 팀을 제거합니다.
- `HSSharedAbilitySystem`은 조건·쿨다운·참여자 검증을 통과한 경우에만 합동 능력을 활성화합니다.
- `HSCommunicationSystem`과 `HSSynchronizationSystem`이 핑·음성·지연 보상을 결합해 협동 액션을 안정적으로 송수신합니다.

```cpp
if (!CanActivateSharedAbility(AbilityID, Participants, FailureReason))
{
    OnSharedAbilityFailed.Broadcast(AbilityID, FailureReason);
    return false;
}
```

합동 능력은 서버에서 조건을 최종 검증하므로 지연 상황에서도 잘못된 발동을 사전에 차단합니다.

### 9. 🚀 성능 최적화 툴킷
**소스**: `Optimization/HSPerformanceOptimizer.*`

- SIMD 벡터 연산·병렬 처리·델타 압축으로 대량 좌표와 회전을 빠르게 처리합니다.
- 플랫폼별 성능 API를 래핑해 CPU/메모리/네트워크 사용량을 캐시하고, HUD·서버 콘솔에서 재활용할 수 있습니다.
- 고성능 오브젝트 풀(`FHighPerformanceObjectPool`)이 프레임 중간 할당을 줄여 스파이크를 방지합니다.

```cpp
float UHSPerformanceOptimizer::GetCurrentCPUUsage()
{
    float Usage = FPlatformProcess::GetCPUUsage();
    if (Usage >= 0.0f) { return CacheResult(Usage); }
    return QueryPlatformSpecificUsage();
}
```

미리 캐시된 성능 수치 덕분에 모니터링 UI는 최소한의 비용으로 최신 리소스 상태를 표시합니다.

### 10. 📦 오브젝트 풀링
**소스**: `Optimization/ObjectPool/HSObjectPool.*`

- `IHSPoolableObject` 인터페이스를 통해 액터 생성/비활성/재활성 이벤트를 통일했습니다.
- 풀 비어 있음과 확장 한도를 감싼 `GetPooledObject`가 생성 전략을 결정하고, 재활성 시 기본 상태를 자동으로 세팅합니다.
- 런타임에 클래스를 교체하거나 사이즈를 조정해도 기존 액터를 재사용하도록 설계했습니다.

```cpp
if (InactivePool.Num() > 0)
{
    AActor* PooledObject = InactivePool.Pop();
    ActivePool.Add(PooledObject);
    IHSPoolableObject::Execute_OnActivated(PooledObject);
    return PooledObject;
}
```

재사용 경로가 명시돼 있어 스폰량이 급증하는 구간에서도 GC 히트 없이 안정적인 프레임을 유지합니다.

---

## 🧪 테스트 & 운영 체크리스트

- **Dedicated Server**: `HuntingSpiritServer` 타깃을 빌드해 실행하고 운영 툴에서 성능 메트릭 브로드캐스트를 수신하세요.
- **매치메이킹 & 세션**: `HSMatchmakingSystem::StartMatchmaking` 후 하트비트가 유지되는지 `HSSessionManager::ProcessSessionHeartbeat` 로그로 확인합니다.
- **월드 스트리밍**: 플레이어를 스폰해 `HSWorldGenerator::OnWorldGenerationProgress` 델리게이트가 청크 로딩과 언로드를 올바르게 보고하는지 검증합니다.
- **레이드 협동**: 보스 전투 중 페이즈 전환과 `HSSharedAbilitySystem::TryActivateSharedAbility` 호출을 동시에 테스트해 동기화 안정성을 확인합니다.
- **진행도 저장**: 런을 완료해 `HSPersistentProgress::SaveProgressData`가 즉시 실행되는지, 저장 파일이 최신 상태로 유지되는지 점검합니다.

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
