# 릴리스와 자동 업데이트

앱은 `https://github.com/Zernia01/Z_media/releases/latest/download/update.json`을 백그라운드에서 읽습니다. 더 높은 안정 버전이 있으면 일반 설정의 자동 다운로드 옵션에 따라 설치기를 다운로드하고 SHA-256을 검증합니다. 다운로드는 사용자 설정 폴더에 저장하며, 설치 실행은 사용자가 버튼을 눌렀을 때 수행합니다. 포터블에서 설치 버튼을 누르면 사용자 계정용 설치형으로 전환됩니다.

일반 설정에서 시작 시 확인과 자동 다운로드를 각각 끌 수 있습니다. 정보 화면에서도 수동 확인·다운로드·설치가 가능합니다. 새 버전이 있으면 상단 설정 아이콘에 보라색 표시가 생깁니다. 현재 0.1.0에서는 같은 0.1.0 릴리스를 최신으로 판단합니다.

다음 릴리스 절차:

1. CMakeLists.txt의 프로젝트 버전과 resources/windows.rc의 숫자/문자열 버전, 릴리스 노트를 새 버전으로 변경합니다.
2. `scripts/build.cmd Release`로 빌드와 테스트를 수행합니다.
3. `scripts/build-installer.ps1`로 설치기와 포터블 ZIP을 생성합니다. 파일명과 Inno Setup 버전은 CMake 버전에서 읽습니다.
4. 소스를 커밋하고 GitHub main에 push합니다.
5. PowerShell 7에서 `scripts/publish-release.ps1`를 실행합니다. 기존 Git Credential Manager 인증을 사용하며, 토큰은 파일에 저장하지 않습니다.

게시 스크립트는 설치기 해시로 update.json과 SHA256SUMS.txt를 만들고 draft 릴리스에 네 파일을 모두 올린 뒤 공개합니다. 공개된 같은 버전의 파일을 덮어쓰지 않습니다. 저장소 주소/태그/설치기 이름이 다른 메타데이터, prerelease 문자열, 중복 키와 잘못된 SHA-256은 앱에서 거부합니다. TLS 인증서 검증을 유지하며 내려받은 설치기는 실행 직전 다시 검증합니다.

업데이트 테스트는 기본 CTest에 포함됩니다. 게시 후 `build/Release/zm_update_tests.exe --live`로 실제 최신 버전 응답을 확인합니다. `--download test-media/update-cache/`는 테스트 실행 파일에서만 현재 버전을 0.0.0으로 간주해 설치기 다운로드·해시 검증을 검사하며 설치기는 실행하지 않습니다. 배포 ZM.exe에는 이 테스트 환경 변수 경로가 포함되지 않습니다.
