#!/bin/bash

# ioLibrary_Driver 서브모듈 초기화 및 업데이트
if [ ! -f "lib/ioLibrary_Driver/README.md" ]; then
    echo "Initializing ioLibrary_Driver submodule..."
    git submodule update --init --recursive
fi

# ioLibrary 패치 적용
echo "Applying ioLibrary patch..."
cd lib/ioLibrary_Driver
git checkout .  # 이전 변경사항 초기화
git apply ../../ioLibrary.patch
if [ $? -eq 0 ]; then
    echo "Patch applied successfully!"
else
    echo "Warning: Patch may already be applied or failed"
fi
cd ../..

# 기존 프로세스 종료
sudo pkill -9 qspi_test

# PIO 코드 컴파일
pioasm ./include/wizchip_qspi_pio.pio ./include/wizchip_qspi_pio.pio.h 

# 프로젝트 빌드
make

# 실행
sudo ./qspi_test