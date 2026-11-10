// R Telegram -- the chat list on the left, the open chat on the right.
#pragma once

#include "TdService.h"

#include <ListItem.h>
#include <View.h>
#include <Window.h>

#include <map>
#include <string>
#include <vector>

class BBitmap;
class BButton;
class BListView;
class BScrollView;
class BStringView;
class BTextControl;

class ChatItem : public BListItem {
public:
	explicit ChatItem(const ChatInfo& chat);
	void Update(BView* owner, const BFont* font) override;
	void DrawItem(BView* owner, BRect frame, bool complete) override;

	int64_t ChatId() const { return fChat.id; }
	const ChatInfo& Info() const { return fChat; }

private:
	ChatInfo fChat;
};

// The open chat's messages as bubbles: own messages on the right, the
// sender's name above incoming ones in groups, photos inline.
class MessageView : public BView {
public:
	MessageView();
	~MessageView() override;

	void SetChat(int64_t chatId, bool isGroup);
	void Reload();					// re-read the model, keep position
	void FileReady(int32_t fileId);

	void Draw(BRect updateRect) override;
	void FrameResized(float width, float height) override;
	void ScrollTo(BPoint where) override;
	void AttachedToWindow() override;

private:
	struct Bubble {
		MessageInfo message;
		std::string name;			// sender, shown in groups
		std::vector<std::string> lines;
		BRect frame;				// the bubble, in view coordinates
		BRect photo;				// inside frame; empty if none
		std::string day;			// a date header before this bubble
	};

	void Layout();
	void UpdateScrollBar();
	BBitmap* Photo(int32_t fileId);
	std::vector<std::string> Wrap(const std::string& text, float width);

	int64_t fChatId = 0;
	bool fIsGroup = false;
	std::vector<Bubble> fBubbles;
	float fHeight = 0;
	bool fAtBottom = true;
	std::map<int32_t, BBitmap*> fPhotos;
};

class MainWindow : public BWindow {
public:
	MainWindow();
	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	void RefreshChats();
	void OpenSelected();
	void SendCompose();
	void NotifyIncoming(BMessage* message);

	BListView* fChats;
	BStringView* fTitle;
	MessageView* fMessages;
	BScrollView* fMessageScroll;
	BTextControl* fCompose;
	BButton* fSend;
	int64_t fOpenChat = 0;
};

enum {
	kMsgLoadOlder = 'rtLo',
};
