
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

### Phase 3: W6300 통신 📋
- [ ] W6300 QSPI 프로토콜 분석
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

### 빌드 방법
```bash
# 프로젝트 빌드 (CMake 사용)
make

# 또는 수동 빌드
mkdir build
cd build
cmake ..
make
```

### 실행 방법
```bash
# cnt_pio 예제 실행 (안정적)
sudo ./cnt_pio

# qspi_test 예제 실행 (개발 중)
sudo ./qspi_test

# gpio_test 예제 실행
sudo ./gpio_test

# 클럭 테스트 (10초)
make run

# W5500 테스트 (예정)
make run-w5500
```

## 예제 설명

### cnt_pio (안정적 예제)
- **기능**: GPIO 13-15에 3비트 카운터 출력 (7→6→...→0 반복)
- **PIO 프로그램**: FIFO 기반으로 데이터 입력받아 감소시키면서 출력
- **사용법**: 로직 애널라이저로 GPIO 13-15 확인, 0.1초마다 카운터 반복
- **코드**: `src/cnt_pio.c` – PIO 명령어 직접 구현

### qspi_test (개발 중 예제)
- **기능**: QSPI Quad 모드 데이터 전송 테스트
- **PIO 프로그램**: `wizchip_qspi_pio.pio` 기반, GPIO 0-3에 4비트 출력
- **사용법**: GPIO 0-3, CS(4), CLK(5) 확인
- **코드**: `src/qspi_test.c` – PIO 어셈블리어 레벨 사용

## 현재 상태

- **클럭 생성**: clock_pio 레퍼런스 기반으로 완성
- **프로젝트 구조**: Makefile, CMake 빌드 시스템 준비
- **안정적 예제**: cnt_pio – PIO 기본 동작 확인 가능
- **다음 단계**: 데이터 전송 PIO 프로그램 구현

## 참고

- 기반 프로젝트: `/clock_pio` (절대 수정 금지)
- PIOLib 의존성: `../utils/piolib`


---
# Commit d179216
- QSPI 통신 파형 TEST
- QSPI 를 위한 (PIO 코드 -> HEX코드) 검증
- 
<img width="717" height="568" alt="image" src="https://github.com/user-attachments/assets/a023c2fe-19ab-44a1-8f36-29e32af4a3d1" />


# commit 869d17f
 
## DMA 기능 추가하여 신호 생성 TEST

<img width="788" height="439" alt="image" src="https://github.com/user-attachments/assets/8a737872-4701-4a47-a542-34afdb2191c2" />

## W6300 SPI Frame TEST
- Datesheet
<img width="573" height="259" alt="image" src="https://github.com/user-attachments/assets/7ec57b74-e686-478c-88f3-795d522a5d03" />

- Raspberry Pi 5 signal
<img width="699" height="464" alt="image" src="https://github.com/user-attachments/assets/31c7430c-5199-4927-85f0-df53c125bd16" />



# commit ed73350

## Write / Read TEST
-SIPR 레지스터에  IP 주소를 작성( IP = 12.48.12.48)

<img width="530" height="524" alt="image" src="https://github.com/user-attachments/assets/72b29f6f-2af8-4ac4-8854-a8ec7545e944" />

-SIPR 레지스터에 작성된 IP를 읽기( IP = 12.48.12.48)


<img width="519" height="516" alt="image" src="https://github.com/user-attachments/assets/9801c13d-4829-4002-ab49-7f1948d2e2c6" />


## IoLibrary 연동
IoLibrary에서 사용되는 close() 함수와 동일한 이름을 가진 함수를 리눅스 표준 헤더인 unistd.h에서도 정의해두었고, 중복정의 에러발생
IoLibrary에 전처리기를 이용하여 close() 함수의 이름을 wiz_close()로 변경. 

git clone을한다음 patch를 적용할 수 있도록  파일을 만들어두었음.

```c++
 #define close wiz_close
```



## loopback TEST


<img width="550" height="506" alt="image" src="https://github.com/user-attachments/assets/78842ec2-e648-4271-aaa9-0be1b1b359cb" />

<img width="617" height="429" alt="image" src="https://github.com/user-attachments/assets/d30693bd-d39a-4d7d-ab14-3f83d7cca144" />


## performance 
22Mhz 까지 정상동작하는것을 확인하였음

## How To Start 

```bash 
git clone --recursive https://github.com/seok930927/RPI5_PIO_TEST_example.git
cd RPI5_PIO_TEST_example/
sudo ./patch.sh
cmake . && make
sudo ./qspi_test

```



## 다음 STEP
- 최적화 진행.
- TOE performance  측정
- Linux QSPI(using PIO) 드라이버 작성
- Linux W6300 드라이버 작성 
