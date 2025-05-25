# 🎮 HuntingSpirit

**협동 보스 사냥 로그라이크 RPG**

![Development Status](https://img.shields.io/badge/Status-In%20Development%20(25%25)-orange)
![Engine](https://img.shields.io/badge/Engine-Unreal%20Engine%205-blue)
![Platform](https://img.shields.io/badge/Platform-PC-green)
![Language](https://img.shields.io/badge/Language-C%2B%2B-red)

---

## 📖 게임 소개

HuntingSpirit은 플레이어들이 협동하여 랜덤 생성된 위험한 월드의 강력한 보스 몬스터를 사냥하는 로그라이크 RPG입니다. 매번 새로운 월드와 보스가 등장하여 끊임없는 도전과 전략적 사고를 요구합니다.

### 🎯 핵심 컨셉
- **협동 보스 사냥**: 플레이어들이 힘을 합쳐 강력한 보스를 처치
- **절차적 월드 생성**: 매번 다른 환경과 도전
- **전략적 성장**: 캐릭터 강화와 자원 관리의 중요성
- **팀워크 필수**: 혼자서는 클리어 불가능한 도전적 난이도

---

## ⚔️ 캐릭터 클래스

### 🛡️ 전사 (Warrior)
- **무기**: 대검
- **특징**: 높은 체력과 방어력, 근접 탱킹 역할
- **핵심 능력**: 강력한 근접 공격, 방어 기술

### 🗡️ 시프 (Thief)  
- **무기**: 단검
- **특징**: 빠른 속도와 높은 기동성
- **핵심 능력**: 연속 공격, 회피 기술

### 🔮 마법사 (Mage)
- **무기**: 스태프
- **특징**: 원거리 마법 공격, 지원 능력
- **핵심 능력**: 다양한 속성 마법, 팀 버프

---

## 🎮 게임 특징

### 🌍 절차적 월드 생성
- 매 게임마다 랜덤하게 생성되는 월드
- 다양한 바이옴과 환경
- 예측 불가능한 보스 등장

### 🤝 협동 멀티플레이어
- 최대 4명까지 협동 플레이
- 데디케이트 서버 지원
- 실시간 상태 동기화

### 📈 로그라이크 진행
- 캐릭터 성장 시스템
- 자원 수집 및 제작
- 메타 진행도 시스템

### ⚡ 최적화된 성능
- 오브젝트 풀링 시스템
- 메모리 풀링 적용
- 효율적인 리소스 관리

---

## 🛠️ 기술 스택

- **게임 엔진**: Unreal Engine 5
- **프로그래밍 언어**: C++
- **네트워킹**: UE5 기본 네트워킹 + 데디케이트 서버
- **최소 요구사양**: GTX 2060 이상
- **플랫폼**: PC (Windows)

---

## 📊 개발 진행도 (25% 완성)

### ✅ 완성된 시스템
- [x] 캐릭터 기본 시스템 (3개 클래스)
- [x] 플레이어 컨트롤러 & 입력 시스템
- [x] 카메라 시스템
- [x] 기본 애니메이션 시스템
- [x] 오브젝트 풀링 시스템
- [x] 스태미너 시스템

### 🚧 현재 작업 중
- [ ] 전투 시스템 구현
- [ ] AI 적 캐릭터 시스템
- [ ] 기본 UI/HUD 시스템

### 📅 향후 개발 계획
1. **Phase 2**: UI 및 스탯 시스템 (2주)
2. **Phase 3**: 월드 생성 시스템 (3-4주)
3. **Phase 4**: 보스 시스템 (2-3주)
4. **Phase 5**: 멀티플레이어 네트워킹 (4-5주)

[전체 개발 계획 보기](./Documentation/개발계획서.md)

---

## 🚀 설치 및 실행

### 요구사항
- Unreal Engine 5.1+
- Visual Studio 2022
- Windows 10/11

### 설치 방법
```bash
# 저장소 클론
git clone https://github.com/KangWooKim/HuntingSpirit.git

# 프로젝트 디렉토리로 이동
cd HuntingSpirit

# 프로젝트 파일 생성 (우클릭 -> Generate Project Files)
# 또는 UnrealBuildTool 사용
```

### 빌드 및 실행
1. `HuntingSpirit.sln` 파일을 Visual Studio로 열기
2. 솔루션 빌드 (Ctrl + Shift + B)
3. Unreal Editor에서 프로젝트 실행

---

## 🎨 아트 스타일

- **3D 사실적 그래픽**: 현실적인 캐릭터 모델링
- **다크 판타지 분위기**: 어둡고 위험한 세계관
- **유혈 표현**: 성인 대상의 현실적 전투
- **미니멀 UI**: 몰입감을 위한 간소한 인터페이스

---

## 🤝 기여하기

프로젝트에 기여를 원하시는 분들을 환영합니다!

### 기여 방법
1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

### 코딩 규약
- 메서드와 변수명은 역할을 명확히 나타내도록 작성
- 주석은 한글로 작성하여 메서드의 역할 명시
- UE5 코딩 표준 준수
- 성능 최적화 고려

---

## 📝 라이선스

이 프로젝트는 개인 학습 및 포트폴리오 목적으로 제작되었습니다.

---

## 👨‍💻 개발자

**KangWooKim**
- GitHub: [@KangWooKim](https://github.com/KangWooKim)
- 프로젝트 시작일: 2025년 4월

---

## 📞 문의

프로젝트에 대한 문의사항이나 피드백이 있으시면 Issues 탭을 통해 연락해 주세요.

---

⭐ **이 프로젝트가 마음에 드셨다면 Star를 눌러주세요!**