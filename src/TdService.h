// R Telegram -- the TDLib client and the model the windows draw from.
//
// TDLib does the protocol, encryption and local database. One thread here
// calls ClientManager::receive() and applies every update to the model under
// a lock; the windows are told what changed with a BMessage and read the
// model back. Requests are sent from any thread.
#pragma once

#include <Locker.h>
#include <Messenger.h>
#include <String.h>

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace td_api = td::td_api;

// What the service tells its targets.
enum {
	kMsgAuthState = 'rtAu',			// "state" int32 (AuthState)
	kMsgChatsChanged = 'rtCh',
	kMsgMessagesChanged = 'rtMs',	// "chat" int64
	kMsgFileReady = 'rtFi',			// "file" int32
	kMsgIncoming = 'rtIn',			// "chat" int64, "title", "text"
	kMsgError = 'rtEr',				// "text"
};

enum class AuthState {
	kStarting,
	kWaitPhone,
	kWaitCode,
	kWaitPassword,
	kWaitRegistration,
	kWaitOther,
	kReady,
	kClosed,
};

struct ChatInfo {
	int64_t id = 0;
	std::string title;
	std::string preview;		// the last message as one line
	int32_t lastDate = 0;
	int32_t unread = 0;
	int64_t order = 0;			// position in the main list; 0 = not in it
	bool pinned = false;
	bool isGroup = false;		// basic groups and supergroups (not channels)
	bool isChannel = false;
	bool muted = false;
	int32_t photoFile = 0;		// the small chat photo, 0 for none
	int32_t accent = -1;		// Telegram's accent colour id
	int64_t userId = 0;			// the other user, in a private chat
	int64_t basicGroupId = 0;
	int64_t supergroupId = 0;
	int64_t lastMessageId = 0;
	int64_t lastSenderUser = 0;
	bool lastOutgoing = false;
	int64_t lastReadOutbox = 0;	// own messages up to this one have been read
};

enum class UserPresence {
	kUnknown,
	kOnline,
	kOffline,		// wasOnline holds when
	kRecently,
	kLastWeek,
	kLastMonth,
};

struct UserInfo {
	int64_t id = 0;
	std::string name;
	int32_t photoFile = 0;
	int32_t accent = -1;
	UserPresence presence = UserPresence::kUnknown;
	int32_t wasOnline = 0;
	bool isBot = false;
	bool deleted = false;
};

struct MessageInfo {
	int64_t id = 0;
	int64_t senderUser = 0;		// 0 when the sender is a chat
	int64_t senderChat = 0;
	bool outgoing = false;
	bool sending = false;		// not yet acknowledged by the server
	bool failed = false;
	int32_t date = 0;
	std::string text;			// text or caption
	std::string placeholder;	// "[Sticker]" and the like, for what is not drawn
	int32_t photoFile = 0;		// file id of the size to show, 0 for none
	int32_t photoWidth = 0;
	int32_t photoHeight = 0;
};

class TdService {
public:
	static TdService& Get();

	// Starts the client with the user's API credentials. Call once.
	void Start(int32_t apiId, const std::string& apiHash);

	void AddTarget(const BMessenger& target);

	// Login.
	AuthState State();
	std::string PasswordHint();
	void SendPhone(const std::string& phone);
	void SendCode(const std::string& code);
	void SendPassword(const std::string& password);
	void LogOut();

	// Chats, ordered as Telegram orders the main list (pinned first).
	std::vector<ChatInfo> Chats();
	bool Chat(int64_t id, ChatInfo* out);
	std::string UserName(int64_t userId);
	bool User(int64_t userId, UserInfo* out);
	// "online", "last seen ...", "12 members": the line under a chat's name.
	std::string ChatStatus(int64_t chatId, bool* online);
	std::string SenderName(const MessageInfo& message);

	// Opening a chat loads its recent history; messages arrive with
	// kMsgMessagesChanged. Older pages load with LoadOlder().
	void OpenChat(int64_t chatId);
	void CloseChat(int64_t chatId);
	void LoadOlder(int64_t chatId);
	std::vector<MessageInfo> Messages(int64_t chatId);
	void MarkRead(int64_t chatId);
	void SendText(int64_t chatId, const std::string& text);

	// A downloaded file's local path, or "" -- starting the download if it
	// has not started. kMsgFileReady says when it is done.
	std::string FilePath(int32_t fileId);

private:
	TdService();

	using Handler = std::function<void(td_api::object_ptr<td_api::Object>)>;
	void Send(td_api::object_ptr<td_api::Function> function,
		Handler handler = nullptr);
	static int32 ReceiveThread(void* data);
	void Receive();
	void Process(td_api::object_ptr<td_api::Object> object);
	void OnAuthorizationState(td_api::AuthorizationState& state);
	void OnNewChat(td_api::chat& chat);
	void OnMessage(td_api::message& message, bool fresh);
	void SetPosition(ChatInfo& chat, td_api::chatPosition& position);
	void SetLastMessage(ChatInfo& chat, td_api::message* message);
	void SetUserStatus(UserInfo& user, td_api::UserStatus* status);
	int32_t SmallPhoto(td_api::file* file);
	MessageInfo Convert(td_api::message& message);
	std::string Preview(td_api::MessageContent* content);
	void AddMessages(int64_t chatId, td_api::messages& messages);
	void Notify(BMessage& message);

	std::unique_ptr<td::ClientManager> fManager;
	int32_t fClientId = 0;
	std::uint64_t fNextRequest = 1;
	int32_t fApiId = 0;
	std::string fApiHash;

	BLocker fLock;
	std::vector<BMessenger> fTargets;
	std::map<std::uint64_t, Handler> fHandlers;
	AuthState fState = AuthState::kStarting;
	std::string fPasswordHint;
	std::map<int64_t, ChatInfo> fChats;
	std::map<int64_t, UserInfo> fUsers;
	std::map<int64_t, int32_t> fBasicGroupMembers;
	std::map<int64_t, int32_t> fSupergroupMembers;
	std::map<int64_t, std::vector<MessageInfo>> fMessages;
	std::map<int64_t, bool> fLoadingHistory;
	std::map<int32_t, std::string> fFiles;		// completed downloads
	std::map<int32_t, bool> fDownloading;
	int64_t fOpenChat = 0;
};
