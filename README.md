# QSPI PIO 통신

Raspberry Pi 5 (RP1 칩) 전용 PIO 기반 QSPI 통신 구현

## 개발 단계

### Phase 1: 클럭 생성 ✅
- [x] 50MHz 정확한 클럭 생성
- [x] 50% 듀티비 보장
- [x] clock_pio 레퍼런스 완성

### Phase 2: QSPI 기본 구조 🚧
- [x] 프로젝트 구조 복사
- [x] 기본 클럭 테스트
- [ ] 데이터 라인 PIO 프로그램
- [ ] CS 제어 구현

### Phase 3: W5500 통신 📋
- [ ] W5500 QSPI 프로토콜 분석
- [ ] 읽기/쓰기 명령 구현
- [ ] 레지스터 접근 테스트

## 핀 배치

| 신호 | GPIO | 설명 |
|------|------|------|
| CLK  | 6    | 50MHz 클럭 |
| DQ0  | 7    | 데이터 0 |
| DQ1  | 8    | 데이터 1 |  
| DQ2  | 9    | 데이터 2 |
| DQ3  | 10   | 데이터 3 |
| CS   | 11   | Chip Select |

## 빌드 및 실행

```bash
# 빌드
make

# 클럭 테스트 (10초)
make run

# W5500 테스트 (예정)
make run-w5500
```

## 현재 상태

- **클럭 생성**: clock_pio 레퍼런스 기반으로 완성
- **프로젝트 구조**: Makefile, CMake 빌드 시스템 준비
- **다음 단계**: 데이터 전송 PIO 프로그램 구현

## 참고

- 기반 프로젝트: `/clock_pio` (절대 수정 금지)
- PIOLib 의존성: `../utils/piolib`
