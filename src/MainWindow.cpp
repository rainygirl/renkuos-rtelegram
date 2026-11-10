#include "MainWindow.h"

#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <ControlLook.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Notification.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <SplitView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslationUtils.h>

#include <algorithm>
#include <time.h>

namespace {

enum {
	kMsgChatSelected = 'rtSl',
	kMsgSend = 'rtSd',
};

const rgb_color kAccent = { 51, 144, 236, 255 };		// Telegram blue
const rgb_color kOutgoing = { 220, 248, 198, 255 };
const rgb_color kIncoming = { 255, 255, 255, 255 };
const rgb_color kChatBackground = { 214, 228, 238, 255 };
const rgb_color kMuted = { 112, 117, 121, 255 };
const rgb_color kText = { 20, 20, 20, 255 };

float Scale()
{
	return std::max(1.0f, be_plain_font->Size() / 12.0f);
}

std::string ShortTime(int32_t date)
{
	if (date == 0)
		return "";
	time_t when = date;
	time_t now = time(NULL);
	struct tm local;
	struct tm today;
	localtime_r(&when, &local);
	localtime_r(&now, &today);
	char buffer[32];
	if (local.tm_year == today.tm_year && local.tm_yday == today.tm_yday)
		strftime(buffer, sizeof(buffer), "%H:%M", &local);
	else
		strftime(buffer, sizeof(buffer), "%m/%d", &local);
	return buffer;
}

std::string DayLabel(int32_t date)
{
	time_t when = date;
	struct tm local;
	localtime_r(&when, &local);
	char buffer[32];
	strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local);
	return buffer;
}

// Byte length of the UTF-8 character starting at text[i].
size_t CharLength(const std::string& text, size_t i)
{
	unsigned char c = text[i];
	if (c < 0x80)
		return 1;
	if ((c & 0xE0) == 0xC0)
		return 2;
	if ((c & 0xF0) == 0xE0)
		return 3;
	return 4;
}

}  // namespace

// ---------------------------------------------------------------- ChatItem

ChatItem::ChatItem(const ChatInfo& chat)
	:
	fChat(chat)
{
}

void ChatItem::Update(BView* owner, const BFont* font)
{
	BListItem::Update(owner, font);
	font_height height;
	font->GetHeight(&height);
	float line = ceilf(height.ascent + height.descent + height.leading);
	SetHeight(ceilf(line * 2 + 14 * Scale()));
}

void ChatItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	rgb_color background = IsSelected() ? kAccent
		: ui_color(B_LIST_BACKGROUND_COLOR);
	owner->SetHighColor(background);
	owner->FillRect(frame);
	rgb_color text = IsSelected() ? rgb_color{ 255, 255, 255, 255 } : kText;
	rgb_color muted = IsSelected() ? rgb_color{ 225, 235, 250, 255 } : kMuted;

	const float pad = 8 * Scale();
	font_height height;
	owner->GetFontHeight(&height);
	float line = ceilf(height.ascent + height.descent + height.leading);
	float right = frame.right - pad;

	// Time at the top right, unread count under it.
	BFont plain(be_plain_font);
	owner->SetFont(&plain);
	std::string when = ShortTime(fChat.lastDate);
	float whenWidth = owner->StringWidth(when.c_str());
	owner->SetHighColor(muted);
	owner->SetLowColor(background);
	owner->DrawString(when.c_str(),
		BPoint(right - whenWidth, frame.top + pad + height.ascent));

	float badgeWidth = 0;
	if (fChat.unread > 0) {
		std::string count = std::to_string(fChat.unread);
		badgeWidth = std::max(line, owner->StringWidth(count.c_str()) + line / 2);
		BRect badge(right - badgeWidth, frame.top + pad + line + 2,
			right, frame.top + pad + 2 * line + 2);
		owner->SetHighColor(IsSelected() ? rgb_color{ 255, 255, 255, 255 }
			: kAccent);
		owner->FillRoundRect(badge, line / 2, line / 2);
		owner->SetHighColor(IsSelected() ? kAccent
			: rgb_color{ 255, 255, 255, 255 });
		owner->SetLowColor(IsSelected() ? rgb_color{ 255, 255, 255, 255 }
			: kAccent);
		owner->DrawString(count.c_str(),
			BPoint(badge.left + (badgeWidth - owner->StringWidth(count.c_str()))
				/ 2, badge.top + height.ascent));
	}

	BFont bold(be_bold_font);
	owner->SetFont(&bold);
	owner->SetHighColor(text);
	owner->SetLowColor(background);
	std::string title = fChat.title;
	BString clipped(title.c_str());
	bold.TruncateString(&clipped, B_TRUNCATE_END,
		right - whenWidth - pad - (frame.left + pad));
	owner->DrawString(clipped.String(),
		BPoint(frame.left + pad, frame.top + pad + height.ascent));

	owner->SetFont(&plain);
	owner->SetHighColor(muted);
	BString preview(fChat.preview.c_str());
	plain.TruncateString(&preview, B_TRUNCATE_END,
		right - badgeWidth - pad - (frame.left + pad));
	owner->DrawString(preview.String(),
		BPoint(frame.left + pad, frame.top + pad + line + 2 + height.ascent));
}

// -------------------------------------------------------------- MessageView

MessageView::MessageView()
	:
	BView("messages", B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE)
{
	SetViewColor(kChatBackground);
}

MessageView::~MessageView()
{
	for (auto& entry : fPhotos)
		delete entry.second;
}

void MessageView::AttachedToWindow()
{
	BView::AttachedToWindow();
	SetFont(be_plain_font);
}

void MessageView::SetChat(int64_t chatId, bool isGroup)
{
	fChatId = chatId;
	fIsGroup = isGroup;
	fAtBottom = true;
	fBubbles.clear();
	Reload();
}

void MessageView::Reload()
{
	float oldHeight = fHeight;
	float oldTop = Bounds().top;
	bool wasAtBottom = fAtBottom;
	Layout();
	UpdateScrollBar();
	float visible = Bounds().Height();
	if (wasAtBottom) {
		BView::ScrollTo(BPoint(0, std::max(0.0f, fHeight - visible)));
	} else if (fHeight > oldHeight && oldTop < 40) {
		// Older messages were added above: keep what was on screen there.
		BView::ScrollTo(BPoint(0, oldTop + (fHeight - oldHeight)));
	}
	Invalidate();
}

void MessageView::FileReady(int32_t fileId)
{
	for (const Bubble& bubble : fBubbles) {
		if (bubble.message.photoFile == fileId) {
			Invalidate(bubble.frame);
		}
	}
}

std::vector<std::string> MessageView::Wrap(const std::string& text,
	float width)
{
	std::vector<std::string> lines;
	BFont font;
	GetFont(&font);
	size_t start = 0;
	while (start <= text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
			end = text.size();
		std::string paragraph = text.substr(start, end - start);
		// Greedy, by character, breaking after the last space that fits; a
		// line with no space (Korean, Japanese, a long URL) breaks anywhere.
		std::string line;
		size_t lastSpace = std::string::npos;
		size_t i = 0;
		while (i < paragraph.size()) {
			size_t n = CharLength(paragraph, i);
			std::string candidate = line + paragraph.substr(i, n);
			if (!line.empty() && font.StringWidth(candidate.c_str()) > width) {
				if (lastSpace != std::string::npos) {
					lines.push_back(line.substr(0, lastSpace));
					line = line.substr(lastSpace + 1);
				} else {
					lines.push_back(line);
					line.clear();
				}
				lastSpace = std::string::npos;
				continue;
			}
			if (paragraph[i] == ' ')
				lastSpace = line.size();
			line = candidate;
			i += n;
		}
		lines.push_back(line);
		start = end + 1;
	}
	return lines;
}

void MessageView::Layout()
{
	fBubbles.clear();
	if (fChatId == 0) {
		fHeight = 0;
		return;
	}
	std::vector<MessageInfo> messages = TdService::Get().Messages(fChatId);
	const float s = Scale();
	const float pad = 8 * s;
	const float width = Bounds().Width();
	const float maxText = std::max(120.0f, width * 0.7f - 2 * pad);
	font_height height;
	GetFontHeight(&height);
	const float line = ceilf(height.ascent + height.descent + height.leading);

	float y = pad;
	std::string lastDay;
	for (const MessageInfo& message : messages) {
		Bubble bubble;
		bubble.message = message;
		std::string day = DayLabel(message.date);
		if (day != lastDay) {
			bubble.day = day;
			lastDay = day;
			y += line + pad;
		}
		std::string text = message.text;
		if (text.empty() && message.photoFile == 0)
			text = message.placeholder;
		bubble.lines = text.empty() ? std::vector<std::string>()
			: Wrap(text, maxText);
		if (fIsGroup && !message.outgoing)
			bubble.name = TdService::Get().SenderName(message);

		float contentWidth = 0;
		for (const std::string& l : bubble.lines)
			contentWidth = std::max(contentWidth, StringWidth(l.c_str()));
		if (!bubble.name.empty()) {
			BFont bold(be_bold_font);
			contentWidth = std::max(contentWidth,
				bold.StringWidth(bubble.name.c_str()));
		}
		float photoHeight = 0;
		float photoWidth = 0;
		if (message.photoFile != 0 && message.photoWidth > 0) {
			photoWidth = std::min(maxText, 280 * s);
			photoHeight = photoWidth * message.photoHeight / message.photoWidth;
			contentWidth = std::max(contentWidth, photoWidth);
		}
		// Room for the time in the bottom right corner.
		float timeWidth = StringWidth("00:00") + pad;
		float bubbleWidth = contentWidth + 2 * pad;
		float bubbleHeight = 2 * pad + bubble.lines.size() * line
			+ (bubble.name.empty() ? 0 : line) + photoHeight
			+ (photoHeight > 0 && !bubble.lines.empty() ? pad : 0);
		if (!bubble.lines.empty()
			&& StringWidth(bubble.lines.back().c_str()) + timeWidth
				> contentWidth) {
			bubbleHeight += line * 0.8f;
		}
		bubbleWidth = std::max(bubbleWidth, timeWidth + 2 * pad);
		float left = message.outgoing ? width - pad - bubbleWidth : pad;
		bubble.frame = BRect(left, y, left + bubbleWidth, y + bubbleHeight);
		if (photoHeight > 0) {
			float top = y + pad + (bubble.name.empty() ? 0 : line);
			bubble.photo = BRect(left + pad, top, left + pad + photoWidth,
				top + photoHeight);
		}
		y += bubbleHeight + pad / 2;
		fBubbles.push_back(bubble);
	}
	fHeight = y + pad;
}

BBitmap* MessageView::Photo(int32_t fileId)
{
	auto it = fPhotos.find(fileId);
	if (it != fPhotos.end())
		return it->second;
	std::string path = TdService::Get().FilePath(fileId);
	if (path.empty())
		return NULL;
	BBitmap* bitmap = BTranslationUtils::GetBitmap(path.c_str());
	fPhotos[fileId] = bitmap;
	return bitmap;
}

void MessageView::Draw(BRect updateRect)
{
	const float s = Scale();
	const float pad = 8 * s;
	font_height height;
	GetFontHeight(&height);
	const float line = ceilf(height.ascent + height.descent + height.leading);
	SetDrawingMode(B_OP_ALPHA);

	for (const Bubble& bubble : fBubbles) {
		BRect frame = bubble.frame;
		if (!bubble.day.empty()) {
			float w = StringWidth(bubble.day.c_str()) + 2 * pad;
			BRect pill((Bounds().Width() - w) / 2, frame.top - line - pad / 2,
				(Bounds().Width() + w) / 2, frame.top - pad / 2);
			if (pill.Intersects(updateRect)) {
				SetHighColor(rgb_color{ 0, 0, 0, 40 });
				FillRoundRect(pill, line / 2, line / 2);
				SetHighColor(rgb_color{ 255, 255, 255, 255 });
				DrawString(bubble.day.c_str(),
					BPoint(pill.left + pad, pill.top + height.ascent));
			}
		}
		if (!frame.Intersects(updateRect))
			continue;
		SetHighColor(bubble.message.outgoing ? kOutgoing : kIncoming);
		FillRoundRect(frame, 10 * s, 10 * s);

		float y = frame.top + pad;
		if (!bubble.name.empty()) {
			SetFont(be_bold_font);
			SetHighColor(kAccent);
			DrawString(bubble.name.c_str(),
				BPoint(frame.left + pad, y + height.ascent));
			SetFont(be_plain_font);
			y += line;
		}
		if (bubble.photo.IsValid()) {
			BBitmap* bitmap = Photo(bubble.message.photoFile);
			if (bitmap != NULL) {
				DrawBitmap(bitmap, bitmap->Bounds(), bubble.photo,
					B_FILTER_BITMAP_BILINEAR);
			} else {
				SetHighColor(rgb_color{ 200, 210, 220, 255 });
				FillRect(bubble.photo);
			}
			y = bubble.photo.bottom + pad;
		}
		SetHighColor(kText);
		for (const std::string& l : bubble.lines) {
			DrawString(l.c_str(), BPoint(frame.left + pad, y + height.ascent));
			y += line;
		}
		std::string when = ShortTime(bubble.message.date);
		BFont small(be_plain_font);
		small.SetSize(be_plain_font->Size() * 0.8f);
		SetFont(&small);
		SetHighColor(kMuted);
		DrawString(when.c_str(), BPoint(frame.right - pad
			- small.StringWidth(when.c_str()), frame.bottom - pad / 2));
		SetFont(be_plain_font);
	}
}

void MessageView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	Reload();
}

void MessageView::UpdateScrollBar()
{
	BScrollBar* bar = ScrollBar(B_VERTICAL);
	if (bar == NULL)
		return;
	float visible = Bounds().Height();
	float range = std::max(0.0f, fHeight - visible);
	bar->SetRange(0, range);
	bar->SetProportion(fHeight > 0 ? std::min(1.0f, visible / fHeight) : 1.0f);
	bar->SetSteps(20 * Scale(), visible * 0.9f);
}

void MessageView::ScrollTo(BPoint where)
{
	BView::ScrollTo(where);
	float visible = Bounds().Height();
	fAtBottom = where.y >= fHeight - visible - 4;
	if (where.y < 4 && fChatId != 0 && Window() != NULL)
		Window()->PostMessage(kMsgLoadOlder);
}

// --------------------------------------------------------------- MainWindow

MainWindow::MainWindow()
	:
	BWindow(BRect(80, 80, 980, 700), "R Telegram", B_DOCUMENT_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE)
{
	fChats = new BListView("chats");
	fChats->SetSelectionMessage(new BMessage(kMsgChatSelected));
	BScrollView* chatScroll = new BScrollView("chat scroll", fChats, 0,
		false, true, B_NO_BORDER);

	fTitle = new BStringView("title", "");
	BFont font(be_bold_font);
	font.SetSize(font.Size() * 1.15f);
	fTitle->SetFont(&font);
	fMessages = new MessageView();
	fMessageScroll = new BScrollView("message scroll", fMessages, 0, false,
		true, B_NO_BORDER);
	fCompose = new BTextControl("compose", NULL, "", NULL);
	fCompose->SetMessage(new BMessage(kMsgSend));
	fSend = new BButton("Send", new BMessage(kMsgSend));
	fCompose->SetEnabled(false);
	fSend->SetEnabled(false);

	BSplitView* split = new BSplitView(B_HORIZONTAL, 0);
	BLayoutBuilder::Split<>(split)
		.Add(chatScroll, 0.32f)
		.AddGroup(B_VERTICAL, 0, 0.68f)
			.AddGroup(B_HORIZONTAL)
				.SetInsets(B_USE_SMALL_SPACING)
				.Add(fTitle)
				.AddGlue()
			.End()
			.Add(fMessageScroll)
			.AddGroup(B_HORIZONTAL)
				.SetInsets(B_USE_SMALL_SPACING)
				.Add(fCompose)
				.Add(fSend)
			.End()
		.End();
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(split);

	TdService::Get().AddTarget(BMessenger(this));
	RefreshChats();
}

void MainWindow::RefreshChats()
{
	int32 selected = fChats->CurrentSelection();
	int64_t selectedId = fOpenChat;
	(void)selected;
	std::vector<ChatInfo> chats = TdService::Get().Chats();
	// Rebuild in place: a list of a few hundred items is cheap to redraw, and
	// keeping BListItems in step with a reordering model is not.
	fChats->SetSelectionMessage(NULL);
	while (BListItem* item = fChats->RemoveItem((int32)0))
		delete item;
	int32 index = -1;
	for (size_t i = 0; i < chats.size(); i++) {
		fChats->AddItem(new ChatItem(chats[i]));
		if (chats[i].id == selectedId)
			index = (int32)i;
	}
	if (index >= 0)
		fChats->Select(index);
	fChats->SetSelectionMessage(new BMessage(kMsgChatSelected));
}

void MainWindow::OpenSelected()
{
	ChatItem* item = dynamic_cast<ChatItem*>(
		fChats->ItemAt(fChats->CurrentSelection()));
	if (item == NULL || item->ChatId() == fOpenChat)
		return;
	if (fOpenChat != 0)
		TdService::Get().CloseChat(fOpenChat);
	fOpenChat = item->ChatId();
	fTitle->SetText(item->Info().title.c_str());
	fMessages->SetChat(fOpenChat, item->Info().isGroup);
	TdService::Get().OpenChat(fOpenChat);
	fCompose->SetEnabled(true);
	fSend->SetEnabled(true);
	fCompose->MakeFocus(true);
}

void MainWindow::SendCompose()
{
	std::string text = fCompose->Text();
	if (fOpenChat == 0 || text.empty())
		return;
	TdService::Get().SendText(fOpenChat, text);
	fCompose->SetText("");
}

void MainWindow::NotifyIncoming(BMessage* message)
{
	int64 chat = 0;
	message->FindInt64("chat", &chat);
	if (IsActive() && chat == fOpenChat)
		return;
	const char* title = NULL;
	const char* text = NULL;
	message->FindString("title", &title);
	message->FindString("text", &text);
	BNotification notification(B_INFORMATION_NOTIFICATION);
	notification.SetGroup("R Telegram");
	notification.SetTitle(title != NULL ? title : "R Telegram");
	notification.SetContent(text != NULL ? text : "");
	notification.Send();
}

void MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgChatsChanged:
			RefreshChats();
			break;
		case kMsgChatSelected:
			OpenSelected();
			break;
		case kMsgMessagesChanged: {
			int64 chat = 0;
			if (message->FindInt64("chat", &chat) == B_OK
				&& chat == fOpenChat) {
				fMessages->Reload();
				if (IsActive())
					TdService::Get().MarkRead(fOpenChat);
			}
			break;
		}
		case kMsgFileReady: {
			int32 file = 0;
			if (message->FindInt32("file", &file) == B_OK)
				fMessages->FileReady(file);
			break;
		}
		case kMsgIncoming:
			NotifyIncoming(message);
			break;
		case kMsgLoadOlder:
			if (fOpenChat != 0)
				TdService::Get().LoadOlder(fOpenChat);
			break;
		case kMsgSend:
			SendCompose();
			break;
		default:
			BWindow::MessageReceived(message);
	}
}

bool MainWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}
