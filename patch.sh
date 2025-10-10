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

