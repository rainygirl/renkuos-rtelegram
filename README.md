# R Telegram

[English](README.md) | [한국어](README.ko.md)

A native Telegram client for Haiku and RenkuOS. It uses
[TDLib](https://github.com/tdlib/td), Telegram's own client library, for the
protocol, and the Haiku API for every window: no Qt.

## Install with pkgman

| Haiku | Commands |
| --- | --- |
| 32-bit x86 (x86_gcc2) | `pkgman add-repo https://pkgman.rainygirl.com/x86_gcc2`<br>`pkgman install rtelegram_x86` |
| x86_64 | `pkgman add-repo https://pkgman.rainygirl.com/x86_64`<br>`pkgman install rtelegram` |
| arm64 | `pkgman add-repo http://pkgman.rainygirl.com/arm64`<br>`pkgman install rtelegram` |

Then start **R Telegram** from Deskbar -> Applications.

## Your API credentials

Every Telegram client needs an `api_id` and an `api_hash`. R Telegram does not
ship any: you use your own, which Telegram gives to anyone with an account.

1. Open <https://my.telegram.org> and sign in with your phone number. The code
   arrives in your Telegram app.
2. Choose **API development tools**.
3. Fill in the form. Any title and short name will do (for example
   "R Telegram" and "rtelegram"); the platform can be "Desktop". Create the
   application.
4. Copy **App api_id** (a number) and **App api_hash** (32 characters).

Start R Telegram and paste them into the window it shows. They are saved in
`~/config/settings/RTelegram/api`, readable only by you, and used for nothing
but signing in to Telegram. Delete that file to enter different ones.

Keep the `api_hash` to yourself: it identifies your application to Telegram.

## Signing in

After the credentials, R Telegram asks for your phone number, then the code
Telegram sends to your other devices (or by SMS), then your two-step
verification password if you set one. Accounts are created in the official
apps; R Telegram only signs in to existing ones.

The session and Telegram's local database live in
`~/config/settings/RTelegram/tdlib`.

## What it does

- Chat list in Telegram's order (pinned first), with the last message, its
  time and the unread count.
- Messages as bubbles, your own on the right; sender names in groups; photos
  inline; dates between days. Scrolling to the top loads older messages.
- Sending text messages; opened chats are marked as read.
- A notification for a new message that is not in the chat you are reading.

Not yet: voice and video calls, sending files and photos, stickers and GIFs
drawn (they show as "[Sticker]" and the like), secret chats.

## Building

TDLib is built as static libraries with the Haiku cross toolchain, then the
app is compiled against it:

    docker cp tools/build-tdlib.sh haiku-builder:/tmp/
    docker cp tools/patch-tdlib.py haiku-builder:/tmp/
    docker exec haiku-builder sh /tmp/build-tdlib.sh arm64
    tools/cross-build.sh arm64

`tools/patch-tdlib.py` is the whole of the Haiku port of TDLib: Haiku is a
POSIX system with `poll()` and pthreads, but defines `__HAIKU__` rather than
`__unix__`, which TDLib and its bundled SQLite test for.

## License

MIT. TDLib is under the Boost Software License 1.0.
