#include "TdService.h"

#include "Settings.h"

#include <Autolock.h>
#include <Locale.h>
#include <LocaleRoster.h>
#include <Message.h>
#include <OS.h>

#include <algorithm>

namespace {

template <class T, class F>
void Downcast(td_api::object_ptr<T>& object, F&& f)
{
	if (object)
		td_api::downcast_call(*object, std::forward<F>(f));
}

// The visitor helper the TDLib examples use.
template <class... F>
struct Overloaded;

template <class F>
struct Overloaded<F> : public F {
	explicit Overloaded(F f) : F(f) {}
	using F::operator();
};

template <class F, class... Fs>
struct Overloaded<F, Fs...> : public Overloaded<F>, public Overloaded<Fs...> {
	Overloaded(F f, Fs... fs) : Overloaded<F>(f), Overloaded<Fs...>(fs...) {}
	using Overloaded<F>::operator();
	using Overloaded<Fs...>::operator();
};

template <class... F>
auto Overload(F... f)
{
	return Overloaded<F...>(f...);
}

std::string OneLine(std::string text)
{
	for (char& c : text) {
		if (c == '\n' || c == '\r' || c == '\t')
			c = ' ';
	}
	return text;
}

std::string LanguageCode()
{
	BMessage languages;
	if (BLocaleRoster::Default()->GetPreferredLanguages(&languages) == B_OK) {
		const char* code = NULL;
		if (languages.FindString("language", &code) == B_OK && code != NULL) {
			std::string text(code);
			return text.substr(0, text.find_first_of("_-"));
		}
	}
	return "en";
}

}  // namespace

TdService& TdService::Get()
{
	static TdService* instance = new TdService();
	return *instance;
}

TdService::TdService()
	:
	fLock("rtelegram model")
{
}

void TdService::Start(int32_t apiId, const std::string& apiHash)
{
	fApiId = apiId;
	fApiHash = apiHash;
	td::ClientManager::execute(
		td_api::make_object<td_api::setLogVerbosityLevel>(1));
	fManager = std::make_unique<td::ClientManager>();
	fClientId = fManager->create_client_id();
	// Any request makes the client start; getOption is the cheapest.
	Send(td_api::make_object<td_api::getOption>("version"));
	thread_id thread = spawn_thread(ReceiveThread, "tdlib receive",
		B_NORMAL_PRIORITY, this);
	resume_thread(thread);
}

void TdService::AddTarget(const BMessenger& target)
{
	BAutolock guard(fLock);
	fTargets.push_back(target);
}

void TdService::Notify(BMessage& message)
{
	std::vector<BMessenger> targets;
	{
		BAutolock guard(fLock);
		targets = fTargets;
	}
	for (BMessenger& target : targets)
		target.SendMessage(&message);
}

void TdService::Send(td_api::object_ptr<td_api::Function> function,
	Handler handler)
{
	std::uint64_t id;
	{
		BAutolock guard(fLock);
		id = fNextRequest++;
		if (handler)
			fHandlers[id] = std::move(handler);
	}
	fManager->send(fClientId, id, std::move(function));
}

int32 TdService::ReceiveThread(void* data)
{
	static_cast<TdService*>(data)->Receive();
	return 0;
}

void TdService::Receive()
{
	for (;;) {
		td::ClientManager::Response response = fManager->receive(10.0);
		if (!response.object)
			continue;
		if (response.request_id != 0) {
			Handler handler;
			{
				BAutolock guard(fLock);
				auto it = fHandlers.find(response.request_id);
				if (it != fHandlers.end()) {
					handler = std::move(it->second);
					fHandlers.erase(it);
				}
			}
			if (response.object->get_id() == td_api::error::ID) {
				auto& error = static_cast<td_api::error&>(*response.object);
				BMessage message(kMsgError);
				message.AddString("text", error.message_.c_str());
				Notify(message);
			}
			if (handler)
				handler(std::move(response.object));
			continue;
		}
		Process(std::move(response.object));
	}
}

void TdService::Process(td_api::object_ptr<td_api::Object> object)
{
	bool chatsChanged = false;
	int64_t messagesChanged = 0;

	td_api::downcast_call(*object, Overload(
		[this](td_api::updateAuthorizationState& update) {
			OnAuthorizationState(*update.authorization_state_);
		},
		[this, &chatsChanged](td_api::updateNewChat& update) {
			OnNewChat(*update.chat_);
			chatsChanged = true;
		},
		[this, &chatsChanged](td_api::updateChatTitle& update) {
			BAutolock guard(fLock);
			fChats[update.chat_id_].title = update.title_;
			chatsChanged = true;
		},
		[this, &chatsChanged](td_api::updateChatPosition& update) {
			BAutolock guard(fLock);
			SetPosition(fChats[update.chat_id_], *update.position_);
			chatsChanged = true;
		},
		[this, &chatsChanged](td_api::updateChatLastMessage& update) {
			BAutolock guard(fLock);
			ChatInfo& chat = fChats[update.chat_id_];
			if (update.last_message_) {
				chat.preview = Preview(update.last_message_->content_.get());
				chat.lastDate = update.last_message_->date_;
			}
			for (auto& position : update.positions_)
				SetPosition(chat, *position);
			chatsChanged = true;
		},
		[this, &chatsChanged](td_api::updateChatReadInbox& update) {
			BAutolock guard(fLock);
			fChats[update.chat_id_].unread = update.unread_count_;
			chatsChanged = true;
		},
		[this](td_api::updateUser& update) {
			BAutolock guard(fLock);
			std::string name = update.user_->first_name_;
			if (!update.user_->last_name_.empty())
				name += " " + update.user_->last_name_;
			fUsers[update.user_->id_] = name;
		},
		[this, &messagesChanged](td_api::updateNewMessage& update) {
			messagesChanged = update.message_->chat_id_;
			OnMessage(*update.message_, true);
		},
		[this, &messagesChanged](td_api::updateMessageSendSucceeded& update) {
			BAutolock guard(fLock);
			int64_t chatId = update.message_->chat_id_;
			auto& list = fMessages[chatId];
			for (MessageInfo& info : list) {
				if (info.id == update.old_message_id_)
					info = Convert(*update.message_);
			}
			messagesChanged = chatId;
		},
		[this, &messagesChanged](td_api::updateMessageContent& update) {
			BAutolock guard(fLock);
			auto& list = fMessages[update.chat_id_];
			for (MessageInfo& info : list) {
				if (info.id != update.message_id_)
					continue;
				td_api::message stub;
				stub.content_ = std::move(update.new_content_);
				MessageInfo fresh = Convert(stub);
				info.text = fresh.text;
				info.placeholder = fresh.placeholder;
				info.photoFile = fresh.photoFile;
				info.photoWidth = fresh.photoWidth;
				info.photoHeight = fresh.photoHeight;
			}
			messagesChanged = update.chat_id_;
		},
		[this, &messagesChanged](td_api::updateDeleteMessages& update) {
			if (!update.is_permanent_)
				return;
			BAutolock guard(fLock);
			auto& list = fMessages[update.chat_id_];
			for (int64_t id : update.message_ids_) {
				list.erase(std::remove_if(list.begin(), list.end(),
					[id](const MessageInfo& m) { return m.id == id; }),
					list.end());
			}
			messagesChanged = update.chat_id_;
		},
		[this](td_api::updateFile& update) {
			td_api::file& file = *update.file_;
			if (!file.local_->is_downloading_completed_)
				return;
			{
				BAutolock guard(fLock);
				fFiles[file.id_] = file.local_->path_;
				fDownloading.erase(file.id_);
			}
			BMessage message(kMsgFileReady);
			message.AddInt32("file", file.id_);
			Notify(message);
		},
		[](auto&) {}));

	if (chatsChanged) {
		BMessage message(kMsgChatsChanged);
		Notify(message);
	}
	if (messagesChanged != 0) {
		BMessage message(kMsgMessagesChanged);
		message.AddInt64("chat", messagesChanged);
		Notify(message);
	}
}

void TdService::OnAuthorizationState(td_api::AuthorizationState& state)
{
	AuthState next = AuthState::kWaitOther;
	td_api::downcast_call(state, Overload(
		[this, &next](td_api::authorizationStateWaitTdlibParameters&) {
			next = AuthState::kStarting;
			auto parameters = td_api::make_object<td_api::setTdlibParameters>();
			parameters->database_directory_ = settings::DatabaseDirectory();
			parameters->files_directory_ = settings::FilesDirectory();
			parameters->use_file_database_ = true;
			parameters->use_chat_info_database_ = true;
			parameters->use_message_database_ = true;
			parameters->use_secret_chats_ = false;
			parameters->api_id_ = fApiId;
			parameters->api_hash_ = fApiHash;
			parameters->system_language_code_ = LanguageCode();
			parameters->device_model_ = "Haiku";
			parameters->system_version_ = "RenkuOS";
			parameters->application_version_ = "0.1.0";
			Send(std::move(parameters));
		},
		[&next](td_api::authorizationStateWaitPhoneNumber&) {
			next = AuthState::kWaitPhone;
		},
		[&next](td_api::authorizationStateWaitCode&) {
			next = AuthState::kWaitCode;
		},
		[this, &next](td_api::authorizationStateWaitPassword& wait) {
			BAutolock guard(fLock);
			fPasswordHint = wait.password_hint_;
			next = AuthState::kWaitPassword;
		},
		[&next](td_api::authorizationStateWaitRegistration&) {
			next = AuthState::kWaitRegistration;
		},
		[this, &next](td_api::authorizationStateReady&) {
			next = AuthState::kReady;
			Send(td_api::make_object<td_api::loadChats>(
				td_api::make_object<td_api::chatListMain>(), 100));
		},
		[&next](td_api::authorizationStateClosed&) {
			next = AuthState::kClosed;
		},
		[](auto&) {}));

	{
		BAutolock guard(fLock);
		fState = next;
	}
	BMessage message(kMsgAuthState);
	message.AddInt32("state", (int32)next);
	Notify(message);
}

void TdService::OnNewChat(td_api::chat& chat)
{
	BAutolock guard(fLock);
	ChatInfo& info = fChats[chat.id_];
	info.id = chat.id_;
	info.title = chat.title_;
	info.unread = chat.unread_count_;
	if (chat.last_message_) {
		info.preview = Preview(chat.last_message_->content_.get());
		info.lastDate = chat.last_message_->date_;
	}
	for (auto& position : chat.positions_)
		SetPosition(info, *position);
	if (chat.type_) {
		switch (chat.type_->get_id()) {
			case td_api::chatTypeBasicGroup::ID:
				info.isGroup = true;
				break;
			case td_api::chatTypeSupergroup::ID:
				info.isGroup = !static_cast<td_api::chatTypeSupergroup&>(
					*chat.type_).is_channel_;
				break;
		}
	}
}

void TdService::SetPosition(ChatInfo& chat, td_api::chatPosition& position)
{
	if (!position.list_
		|| position.list_->get_id() != td_api::chatListMain::ID)
		return;
	chat.order = position.order_;
	chat.pinned = position.is_pinned_;
}

std::string TdService::Preview(td_api::MessageContent* content)
{
	if (content == NULL)
		return "";
	td_api::message stub;
	MessageInfo info;
	switch (content->get_id()) {
		case td_api::messageText::ID:
			return OneLine(static_cast<td_api::messageText*>(content)
				->text_->text_);
		case td_api::messagePhoto::ID: {
			auto* photo = static_cast<td_api::messagePhoto*>(content);
			std::string caption = photo->caption_ ? photo->caption_->text_ : "";
			return caption.empty() ? "Photo" : "Photo, " + OneLine(caption);
		}
		case td_api::messageVideo::ID:
			return "Video";
		case td_api::messageSticker::ID:
			return "Sticker";
		case td_api::messageAnimation::ID:
			return "GIF";
		case td_api::messageVoiceNote::ID:
			return "Voice message";
		case td_api::messageDocument::ID:
			return "File";
		default:
			return "Message";
	}
}

MessageInfo TdService::Convert(td_api::message& message)
{
	MessageInfo info;
	info.id = message.id_;
	info.outgoing = message.is_outgoing_;
	info.date = message.date_;
	if (message.sender_id_) {
		if (message.sender_id_->get_id() == td_api::messageSenderUser::ID) {
			info.senderUser = static_cast<td_api::messageSenderUser&>(
				*message.sender_id_).user_id_;
		} else {
			info.senderChat = static_cast<td_api::messageSenderChat&>(
				*message.sender_id_).chat_id_;
		}
	}
	td_api::MessageContent* content = message.content_.get();
	if (content == NULL)
		return info;
	switch (content->get_id()) {
		case td_api::messageText::ID:
			info.text = static_cast<td_api::messageText*>(content)->text_->text_;
			break;
		case td_api::messagePhoto::ID: {
			auto* photo = static_cast<td_api::messagePhoto*>(content);
			if (photo->caption_)
				info.text = photo->caption_->text_;
			// The largest size no wider than 640 px, else the smallest.
			td_api::photoSize* best = NULL;
			if (photo->photo_) {
				for (auto& size : photo->photo_->sizes_) {
					if (size->width_ <= 640
						&& (best == NULL || size->width_ > best->width_)) {
						best = size.get();
					}
				}
				if (best == NULL && !photo->photo_->sizes_.empty())
					best = photo->photo_->sizes_.front().get();
			}
			if (best != NULL && best->photo_) {
				info.photoFile = best->photo_->id_;
				info.photoWidth = best->width_;
				info.photoHeight = best->height_;
				if (best->photo_->local_
					&& best->photo_->local_->is_downloading_completed_) {
					fFiles[info.photoFile] = best->photo_->local_->path_;
				}
			} else {
				info.placeholder = "[Photo]";
			}
			break;
		}
		default:
			info.placeholder = "[" + Preview(content) + "]";
			break;
	}
	return info;
}

void TdService::OnMessage(td_api::message& message, bool fresh)
{
	int64_t chatId = message.chat_id_;
	std::string title;
	std::string text;
	bool notify = false;
	{
		BAutolock guard(fLock);
		MessageInfo info = Convert(message);
		auto it = fMessages.find(chatId);
		// Only chats whose history is loaded keep a message list; the rest
		// learn about the message from updateChatLastMessage.
		if (it != fMessages.end()) {
			bool known = false;
			for (const MessageInfo& m : it->second)
				known = known || m.id == info.id;
			if (!known)
				it->second.push_back(info);
		}
		if (fresh && !message.is_outgoing_) {
			ChatInfo& chat = fChats[chatId];
			title = chat.title;
			text = info.text.empty() ? Preview(message.content_.get())
				: OneLine(info.text);
			notify = true;
		}
	}
	if (notify) {
		BMessage incoming(kMsgIncoming);
		incoming.AddInt64("chat", chatId);
		incoming.AddString("title", title.c_str());
		incoming.AddString("text", text.c_str());
		Notify(incoming);
	}
}

AuthState TdService::State()
{
	BAutolock guard(fLock);
	return fState;
}

std::string TdService::PasswordHint()
{
	BAutolock guard(fLock);
	return fPasswordHint;
}

void TdService::SendPhone(const std::string& phone)
{
	Send(td_api::make_object<td_api::setAuthenticationPhoneNumber>(phone,
		td_api::make_object<td_api::phoneNumberAuthenticationSettings>()));
}

void TdService::SendCode(const std::string& code)
{
	Send(td_api::make_object<td_api::checkAuthenticationCode>(code));
}

void TdService::SendPassword(const std::string& password)
{
	Send(td_api::make_object<td_api::checkAuthenticationPassword>(password));
}

void TdService::LogOut()
{
	Send(td_api::make_object<td_api::logOut>());
}

std::vector<ChatInfo> TdService::Chats()
{
	std::vector<ChatInfo> result;
	{
		BAutolock guard(fLock);
		for (auto& entry : fChats) {
			if (entry.second.order != 0)
				result.push_back(entry.second);
		}
	}
	std::sort(result.begin(), result.end(),
		[](const ChatInfo& a, const ChatInfo& b) {
			if (a.order != b.order)
				return a.order > b.order;
			return a.id < b.id;
		});
	return result;
}

bool TdService::Chat(int64_t id, ChatInfo* out)
{
	BAutolock guard(fLock);
	auto it = fChats.find(id);
	if (it == fChats.end())
		return false;
	*out = it->second;
	return true;
}

std::string TdService::UserName(int64_t userId)
{
	BAutolock guard(fLock);
	auto it = fUsers.find(userId);
	return it == fUsers.end() ? std::string() : it->second;
}

std::string TdService::SenderName(const MessageInfo& message)
{
	if (message.senderUser != 0)
		return UserName(message.senderUser);
	BAutolock guard(fLock);
	auto it = fChats.find(message.senderChat);
	return it == fChats.end() ? std::string() : it->second.title;
}

void TdService::OpenChat(int64_t chatId)
{
	{
		BAutolock guard(fLock);
		fOpenChat = chatId;
		fMessages[chatId];
	}
	Send(td_api::make_object<td_api::openChat>(chatId));
	LoadOlder(chatId);
}

void TdService::CloseChat(int64_t chatId)
{
	{
		BAutolock guard(fLock);
		if (fOpenChat == chatId)
			fOpenChat = 0;
	}
	Send(td_api::make_object<td_api::closeChat>(chatId));
}

void TdService::LoadOlder(int64_t chatId)
{
	int64_t from = 0;
	{
		BAutolock guard(fLock);
		if (fLoadingHistory[chatId])
			return;
		fLoadingHistory[chatId] = true;
		auto& list = fMessages[chatId];
		if (!list.empty())
			from = list.front().id;
	}
	// TDLib answers the first call from its cache, often with a single
	// message; asking again from the oldest one fills the rest.
	Send(td_api::make_object<td_api::getChatHistory>(chatId, from, 0, 50,
			false),
		[this, chatId](td_api::object_ptr<td_api::Object> object) {
			bool more = false;
			if (object && object->get_id() == td_api::messages::ID) {
				auto& messages = static_cast<td_api::messages&>(*object);
				more = !messages.messages_.empty();
				AddMessages(chatId, messages);
			}
			size_t count;
			{
				BAutolock guard(fLock);
				fLoadingHistory[chatId] = false;
				count = fMessages[chatId].size();
			}
			BMessage message(kMsgMessagesChanged);
			message.AddInt64("chat", chatId);
			Notify(message);
			if (more && count < 30)
				LoadOlder(chatId);
		});
}

void TdService::AddMessages(int64_t chatId, td_api::messages& messages)
{
	BAutolock guard(fLock);
	auto& list = fMessages[chatId];
	for (auto& message : messages.messages_) {
		if (!message)
			continue;
		MessageInfo info = Convert(*message);
		bool known = false;
		for (const MessageInfo& m : list)
			known = known || m.id == info.id;
		if (!known)
			list.push_back(info);
	}
	std::sort(list.begin(), list.end(),
		[](const MessageInfo& a, const MessageInfo& b) { return a.id < b.id; });
}

std::vector<MessageInfo> TdService::Messages(int64_t chatId)
{
	BAutolock guard(fLock);
	auto it = fMessages.find(chatId);
	return it == fMessages.end() ? std::vector<MessageInfo>() : it->second;
}

void TdService::MarkRead(int64_t chatId)
{
	std::vector<std::int64_t> ids;
	{
		BAutolock guard(fLock);
		auto& list = fMessages[chatId];
		size_t start = list.size() > 50 ? list.size() - 50 : 0;
		for (size_t i = start; i < list.size(); i++)
			ids.push_back(list[i].id);
	}
	if (ids.empty())
		return;
	Send(td_api::make_object<td_api::viewMessages>(chatId, std::move(ids),
		td_api::make_object<td_api::messageSourceChatHistory>(), true));
}

void TdService::SendText(int64_t chatId, const std::string& text)
{
	auto content = td_api::make_object<td_api::inputMessageText>();
	content->text_ = td_api::make_object<td_api::formattedText>();
	content->text_->text_ = text;
	auto send = td_api::make_object<td_api::sendMessage>();
	send->chat_id_ = chatId;
	send->input_message_content_ = std::move(content);
	Send(std::move(send));
}

std::string TdService::FilePath(int32_t fileId)
{
	{
		BAutolock guard(fLock);
		auto it = fFiles.find(fileId);
		if (it != fFiles.end())
			return it->second;
		if (fDownloading[fileId])
			return "";
		fDownloading[fileId] = true;
	}
	Send(td_api::make_object<td_api::downloadFile>(fileId, 1, 0, 0, false));
	return "";
}
