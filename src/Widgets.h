// R Telegram -- the small views the main window is built from.
#pragma once

#include <GroupView.h>
#include <ListView.h>
#include <TextView.h>

#include <cstdint>
#include <string>

class BMessage;

// A borderless BTextView with a greyed placeholder while it is empty. It can
// post a message on Enter (Shift+Enter still breaks the line), post another
// on every change, and grow with its text between a number of lines.
class PlainTextView : public BTextView {
public:
	PlainTextView(const char* name, const char* placeholder);
	~PlainTextView() override;

	void SetEnterMessage(BMessage* message);
	void SetChangeMessage(BMessage* message);
	void SetAutoGrow(int minLines, int maxLines);

	void Draw(BRect updateRect) override;
	void KeyDown(const char* bytes, int32 numBytes) override;
	void FrameResized(float width, float height) override;
	void MakeFocus(bool focus = true) override;

protected:
	void InsertText(const char* text, int32 length, int32 offset,
		const text_run_array* runs) override;
	void DeleteText(int32 fromOffset, int32 toOffset) override;

private:
	void Changed();
	void FitHeight();

	std::string fPlaceholder;
	BMessage* fEnterMessage = NULL;
	BMessage* fChangeMessage = NULL;
	int fMinLines = 0;
	int fMaxLines = 0;
	float fFittedHeight = -1;
};

// A white strip with an optional hairline on its top or bottom edge.
class Bar : public BGroupView {
public:
	enum Edge { kNone, kTop, kBottom };
	Bar(const char* name, orientation direction, Edge edge);
	void Draw(BRect updateRect) override;
	void FrameResized(float width, float height) override;

private:
	Edge fEdge;
};

// The rounded grey search field above the chat list.
class SearchBox : public BGroupView {
public:
	explicit SearchBox(PlainTextView* field);
	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;

private:
	PlainTextView* fField;
};

// The paper-plane send button.
class SendButton : public BView {
public:
	explicit SendButton(BMessage* message);
	~SendButton() override;
	void SetEnabled(bool enabled);
	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;

private:
	BMessage* fMessage;
	bool fEnabled = false;
};

// The bar over the messages: the chat's name and its status line.
class ChatHeader : public BView {
public:
	ChatHeader();
	void SetChat(int64_t chatId);
	void Refresh();
	void Draw(BRect updateRect) override;
	void FrameResized(float width, float height) override;

private:
	int64_t fChatId = 0;
	std::string fTitle;
	std::string fStatus;
	bool fOnline = false;
};

// The chat list, which shades the row under the mouse.
class ChatListView : public BListView {
public:
	explicit ChatListView(const char* name);
	void MouseMoved(BPoint where, uint32 transit,
		const BMessage* dragMessage) override;
	void ClearHover();
	bool IsHovered(const BListItem* item) const;

private:
	void SetHover(int32 index);

	int32 fHover = -1;
};
