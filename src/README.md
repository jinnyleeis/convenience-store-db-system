# README – 편의점 체인 데이터베이스 시연 프로그램

---

## 1. 개요

본 프로그램은 과제 2에서 설계·구현한 **MySQL 8.0** 기반 편의점 체인 데이터베이스를 대상으로
7 개 핵심 업무 질의를 콘솔 메뉴로 제공하는 C++ 애플리케이션입니다.
소스는 단일 파일(**src/main.cpp**)입니다.

---

## 2. 개발·실행 환경

| 구분     | 값                                                     |
| -------- | ------------------------------------------------------ |
| OS       | macOS 15.5 Sonoma (arm64)                              |
| DBMS     | MySQL 8.0.42-arm64 (Server & Client SDK)               |
| 컴파일러 | g++ 13.x (Homebrew gcc) - _Apple Clang 15 이상도 가능_ |
| IDE      | Visual Studio Code (extensions: _C/C++_, _CodeLLDB_)   |

> **확인**
> 시스템 환경설정 ▸ _MySQL_ 패널에서 **MySQL 8.0.42-arm64** 인스턴스가
> _Running_ 상태(녹색 불)인지 먼저 확인합니다.

## ![alt text](내컴퓨터의mysql.png)

---

## 3. 컴파일 방법

```bash
main.cpp이 존재하는 디렉토리에서 아래의 명령어를 통해 컴파일 후 실행하였습니다.

g++ -std=c++17 main.cpp \
    $(mysql_config --cflags) \
    -L/usr/local/mysql/lib -lmysqlclient \
    -Wl,-rpath,/usr/local/mysql/lib \
    -o main


이후 실행은, ./main으로 진행했습니다.
```

---

## 5. 프로그램 실행

```bash
./main
```

실행 직후 테이블별 레코드 수를 요약 출력해 초기 데이터 적재 여부를 자동 점검합니다.
화면에 표시되는 **숫자(0\~7)** 를 입력하여 각 질의를 수행할 수 있습니다.
종료는 **0 입력 후 Enter**.

### 5.1 메뉴 구성

| 번호 | 기능                            | 비고                  |
| ---- | ------------------------------- | --------------------- |
| 1    | 특정 상품 재고 현황             | UPC / 상품명 / 브랜드 |
| 2    | 최근 1 개월 지점별 베스트셀러   | 윈도 함수 + CTE       |
| 3    | 해당 분기 최고 매출 지점        | 집계 서브쿼리         |
| 4    | 공급사별 상품 종류·판매량       | LEFT JOIN             |
| 5    | 재주문 임계치 이하 재고         | CHECK + 보조 인덱스   |
| 6    | VIP 커피 구매 시 동반상품 Top 3 | 다단 CTE              |
| 7    | 소유형태별 상품 다양성 1위 지점 | 그룹 최댓값 조인      |

---

## 6. 중요 설정 파일 (VS Code)

- **.vscode/c_cpp_properties.json**
  MySQL 헤더 경로
  `"/usr/local/mysql-8.0.42-macos15-arm64/include"`

- **.vscode/launch.json**
  디버깅 대상 `program` 경로를 `./main` 또는
  `${workspaceFolder}/src/main` 으로 교체해 사용합니다.

---

## 7. 실행하면서 발생했던 오류와 해결책

| 오류 메시지                                      | 원인                        | 조치                                              |
| ------------------------------------------------ | --------------------------- | ------------------------------------------------- |
| `Connection fail : Access denied`                | 잘못된 DB 계정/비밀번호     | `main.cpp` 상단 상수를 실 계정으로 변경           |
| `dyld: Library not loaded: libmysqlclient.dylib` | 런타임 라이브러리 탐색 실패 | `export DYLD_LIBRARY_PATH=/usr/local/mysql/lib`   |
| `ld: library was built for newer macOS`          | SDK-OS 버전 차이 경고       | `-mmacosx-version-min=14.0` 추가 또는 무시합니다. |

---
