# R Telegram

[English](README.md) | [한국어](README.ko.md)

Haiku와 RenkuOS용 네이티브 Telegram 클라이언트입니다. 프로토콜은 Telegram의
공식 클라이언트 라이브러리 [TDLib](https://github.com/tdlib/td)이 맡고, 모든
창은 Haiku API로 그립니다. Qt는 쓰지 않습니다.

## pkgman으로 설치

| Haiku | 명령 |
| --- | --- |
| 32비트 x86 (x86_gcc2) | `pkgman add-repo https://pkgman.rainygirl.com/x86_gcc2`<br>`pkgman install rtelegram_x86` |
| x86_64 | `pkgman add-repo https://pkgman.rainygirl.com/x86_64`<br>`pkgman install rtelegram` |
| arm64 | `pkgman add-repo http://pkgman.rainygirl.com/arm64`<br>`pkgman install rtelegram` |

설치한 뒤 Deskbar -> 응용 프로그램에서 **R Telegram**을 실행합니다.

## API 키 받기

Telegram 클라이언트는 모두 `api_id`와 `api_hash`가 필요합니다. R Telegram에는
키가 들어 있지 않으므로, Telegram 계정이 있으면 누구나 받을 수 있는 본인의 키를
씁니다.

1. <https://my.telegram.org>에 들어가 전화번호로 로그인합니다. 확인 코드는
   Telegram 앱으로 옵니다.
2. **API development tools**를 누릅니다.
3. 양식을 채웁니다. App title과 Short name은 아무거나 괜찮습니다(예:
   "R Telegram", "rtelegram"). Platform은 "Desktop"을 고르면 됩니다. 앱을
   만듭니다.
4. **App api_id**(숫자)와 **App api_hash**(32자)를 복사합니다.

R Telegram을 실행하면 나오는 창에 붙여 넣습니다. 키는
`~/config/settings/RTelegram/api`에 본인만 읽을 수 있게 저장되고, Telegram에
로그인하는 데에만 쓰입니다. 다른 키를 넣으려면 이 파일을 지우면 됩니다.

`api_hash`는 다른 사람에게 알려 주지 마세요. Telegram에 대해 여러분의 앱을
나타내는 값입니다.

## 로그인

키를 넣고 나면 전화번호, Telegram이 다른 기기(또는 SMS)로 보내는 코드, 2단계
인증 비밀번호(설정한 경우) 순서로 묻습니다. 계정은 공식 앱에서 만들어야 하고,
R Telegram은 이미 있는 계정으로 로그인만 합니다.

세션과 Telegram 로컬 데이터베이스는 `~/config/settings/RTelegram/tdlib`에
있습니다.

## 기능

- Telegram 순서(고정한 대화 먼저)대로 대화 목록, 마지막 메시지와 시각, 읽지 않은
  개수
- 말풍선 메시지(내 메시지는 오른쪽), 그룹에서는 보낸 사람 이름, 사진, 날짜 구분.
  맨 위로 스크롤하면 이전 메시지를 불러옵니다.
- 텍스트 메시지 보내기, 연 대화는 읽음 처리
- 보고 있지 않은 대화에 새 메시지가 오면 알림

아직 없는 것: 음성/영상 통화, 파일과 사진 보내기, 스티커와 GIF 표시("[Sticker]"
등으로 보입니다), 비밀 대화

## 빌드

TDLib을 Haiku 크로스 툴체인으로 정적 라이브러리로 빌드한 뒤, 앱을 그 위에서
컴파일합니다. 자세한 명령은 [README.md](README.md#building)에 있습니다.

## 라이선스

MIT. TDLib은 Boost Software License 1.0입니다.
