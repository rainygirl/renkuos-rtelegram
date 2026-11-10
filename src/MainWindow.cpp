#include "MainWindow.h"

#include "Theme.h"
#include "Widgets.h"

#include <Application.h>
#include <Bitmap.h>
#include <GradientLinear.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <Notification.h>
#include <ScrollBar.h>
#include <ScrollView.h>
#include <Shape.h>
#include <SplitView.h>
#include <TranslationUtils.h>

#include <algorithm>
#include <ctype.h>
#include <math.h>
#include <time.h>

using namespace theme;

namespace {

enum {
	kMsgChatSelected = 'rtSl',
	kMsgSend = 'rtSd',
	kMsgSearch = 'rtSr',
	kMsgSearchEnter = 'rtSe',
	kMsgComposeChanged = 'rtCc',
	kMsgRefreshChats = 'rtRc',
};

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

std::string Lower(std::string text)
{
	for (char& c : text)
		c = tolower((unsigned char)c);
	return text;
}

std::string FirstName(const std::string& name)
{
	size_t space = name.find(' ');
	return space == std::string::npos ? name : name.substr(0, space);
}

float LineHeight(const BFont& font)
{
	font_height height;
	font.GetHeight(&height);
	return ceilf(height.ascent + height.descent + height.leading);
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
	const float s = theme::Scale();
	float line = LineHeight(*be_plain_font);
	SetHeight(roundf(std::max(62 * s, 2 * line + 24 * s)));
}

void ChatItem::DrawItem(BView* owner, BRect frame, bool complete)
{
	const float s = theme::Scale();
	ChatListView* list = dynamic_cast<ChatListView*>(owner);
	bool selected = IsSelected();
	bool hovered = !selected && list != NULL && list->IsHovered(this);
	rgb_color background = selected ? kSelected : hovered ? kHover : kWhite;
	owner->SetDrawingMode(B_OP_COPY);
	owner->SetHighColor(background);
	owner->FillRect(frame);
	owner->SetDrawingMode(B_OP_ALPHA);
	owner->SetLowColor(background);

	rgb_color text = selected ? kWhite : kText;
	rgb_color muted = selected ? kSelectedMuted : kMuted;

	// The avatar.
	float avatar = roundf(46 * s);
	BRect avatarFrame(frame.left + roundf(10 * s),
		frame.top + roundf((frame.Height() - avatar) / 2), 0, 0);
	avatarFrame.right = avatarFrame.left + avatar - 1;
	avatarFrame.bottom = avatarFrame.top + avatar - 1;
	DrawAvatar(owner, avatarFrame, fChat.photoFile,
		ColorIndex(fChat.accent, fChat.id), fChat.title);

	BFont plain(be_plain_font);
	BFont bold(be_bold_font);
	font_height height;
	plain.GetHeight(&height);
	float line = LineHeight(plain);
	float left = avatarFrame.right + roundf(12 * s);
	float right = frame.right - roundf(10 * s);
	float top = frame.top + roundf((frame.Height() - 2 * line - 4 * s) / 2);
	float firstBaseline = top + height.ascent;
	float secondBaseline = top + line + 4 * s + height.ascent;

	// Time, and the ticks of an own last message, on the right.
	owner->SetFont(&plain);
	std::string when = ListTime(fChat.lastDate);
	float whenWidth = plain.StringWidth(when.c_str());
	owner->SetHighColor(muted);
	owner->DrawString(when.c_str(), BPoint(right - whenWidth, firstBaseline));
	float headRight = right - whenWidth;
	if (fChat.lastOutgoing && fChat.lastMessageId != 0) {
		bool read = fChat.lastReadOutbox >= fChat.lastMessageId;
		float size = plain.Size();
		float x = headRight - 4 * s;
		DrawChecks(owner, BPoint(x, firstBaseline), size, read,
			selected ? kWhite : kChecks);
		headRight = x - ChecksWidth(size, read);
	}

	// The name.
	owner->SetFont(&bold);
	owner->SetHighColor(text);
	BString title(fChat.title.c_str());
	bold.TruncateString(&title, B_TRUNCATE_END, headRight - 8 * s - left);
	owner->DrawString(title.String(), BPoint(left, firstBaseline));

	// The unread badge.
	float previewRight = right;
	if (fChat.unread > 0) {
		BFont badgeFont(be_bold_font);
		badgeFont.SetSize(plain.Size() * 0.85f);
		font_height badgeHeight;
		badgeFont.GetHeight(&badgeHeight);
		std::string count = fChat.unread > 9999 ? "9999+"
			: std::to_string(fChat.unread);
		float h = roundf(line * 1.05f);
		float w = std::max(h, badgeFont.StringWidth(count.c_str()) + h * 0.6f);
		BRect badge(right - w, secondBaseline - height.ascent
			+ (line - h) / 2 - 1, right, 0);
		badge.bottom = badge.top + h;
		rgb_color fill = selected ? kWhite : fChat.muted ? kBadgeMuted
			: kAccent;
		owner->SetHighColor(fill);
		owner->FillRoundRect(badge, h / 2, h / 2);
		owner->SetFont(&badgeFont);
		owner->SetHighColor(selected ? kSelected : kWhite);
		owner->DrawString(count.c_str(),
			BPoint(badge.left + (w - badgeFont.StringWidth(count.c_str())) / 2,
				badge.top + (h + badgeHeight.ascent - badgeHeight.descent) / 2));
		previewRight = badge.left - 8 * s;
	}

	// The last message, with its sender in groups.
	owner->SetFont(&plain);
	float x = left;
	std::string sender;
	if (fChat.isGroup && fChat.lastOutgoing)
		sender = "You";
	else if (fChat.isGroup && fChat.lastSenderUser != 0)
		sender = FirstName(TdService::Get().UserName(fChat.lastSenderUser));
	if (!sender.empty()) {
		sender += ": ";
		BString clipped(sender.c_str());
		plain.TruncateString(&clipped, B_TRUNCATE_END, previewRight - x);
		owner->SetHighColor(selected ? kWhite : kPreviewSender);
		owner->DrawString(clipped.String(), BPoint(x, secondBaseline));
		x += plain.StringWidth(clipped.String());
	}
	BString preview(fChat.preview.c_str());
	plain.TruncateString(&preview, B_TRUNCATE_END, previewRight - x);
	owner->SetHighColor(muted);
	owner->DrawString(preview.String(), BPoint(x, secondBaseline));
}

// -------------------------------------------------------------- MessageView

MessageView::MessageView()
	:
	BView("messages", B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE)
{
	// The background is drawn in Draw(); no flash of a view colour first.
	SetViewColor(B_TRANSPARENT_COLOR);
	// Without a layout a BView offers its current size as its minimum, which
	// would keep the compose bar from getting room when it appears.
	SetExplicitMinSize(BSize(200, 100));
	SetExplicitPreferredSize(BSize(400, 300));
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
	// A photo in a bubble or an avatar beside one; either way, redraw.
	Invalidate();
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

namespace {

// Bubble metrics, all scaled.
struct Metrics {
	float s, margin, padX, padY, runGap, groupGap, avatar, avatarGap, radius;
	float line, smallSize;

	explicit Metrics(float scale, float lineHeight)
		:
		s(scale),
		margin(12 * scale),
		padX(10 * scale),
		padY(6 * scale),
		runGap(2 * scale),
		groupGap(8 * scale),
		avatar(roundf(34 * scale)),
		avatarGap(8 * scale),
		radius(10 * scale),
		line(lineHeight),
		smallSize(be_plain_font->Size() * 0.8f)
	{
	}
};

bool SameSender(const MessageInfo& a, const MessageInfo& b)
{
	return a.outgoing == b.outgoing && a.senderUser == b.senderUser
		&& a.senderChat == b.senderChat;
}

// The width the time (and the ticks of an own message) takes in a bubble.
float TimeWidth(const MessageInfo& message, const BFont& small, bool read,
	float s)
{
	float w = small.StringWidth(ClockTime(message.date).c_str());
	if (message.outgoing)
		w += 3 * s + ChecksWidth(small.Size(), read);
	return w;
}

}  // namespace

void MessageView::Layout()
{
	fBubbles.clear();
	if (fChatId == 0) {
		fHeight = 0;
		return;
	}
	std::vector<MessageInfo> messages = TdService::Get().Messages(fChatId);
	ChatInfo chat;
	TdService::Get().Chat(fChatId, &chat);
	BFont plain;
	GetFont(&plain);
	Metrics m(theme::Scale(), LineHeight(plain));
	BFont small(be_plain_font);
	small.SetSize(m.smallSize);
	BFont bold(be_bold_font);

	const float width = Bounds().Width();
	const float indent = fIsGroup ? m.avatar + m.avatarGap : 0;
	const float maxBubble = std::max(120 * m.s,
		std::min(460 * m.s, (width - 2 * m.margin - indent) * 0.8f));
	const float maxText = maxBubble - 2 * m.padX;
	const float pillHeight = m.line + 6 * m.s;

	float y = m.groupGap;
	std::string lastDay;
	for (size_t i = 0; i < messages.size(); i++) {
		const MessageInfo& message = messages[i];
		Bubble bubble;
		bubble.message = message;
		bubble.read = message.outgoing && chat.lastReadOutbox >= message.id;

		std::string day = DayLabel(message.date);
		bool newDay = day != lastDay;
		bool continues = !newDay && i > 0
			&& SameSender(messages[i - 1], message)
			&& message.date - messages[i - 1].date < 600;
		if (newDay) {
			bubble.day = day;
			lastDay = day;
			y += m.groupGap;
			bubble.dayTop = y;
			y += pillHeight + m.groupGap;
		} else {
			y += continues ? m.runGap : m.groupGap;
		}

		std::string text = message.text;
		if (text.empty() && message.photoFile == 0)
			text = message.placeholder;
		bubble.lines = text.empty() ? std::vector<std::string>()
			: Wrap(text, maxText);
		if (fIsGroup && !message.outgoing && !continues) {
			bubble.name = TdService::Get().SenderName(message);
			UserInfo user;
			int32_t accent = -1;
			int64_t id = message.senderUser != 0 ? message.senderUser
				: message.senderChat;
			if (message.senderUser != 0
				&& TdService::Get().User(message.senderUser, &user)) {
				accent = user.accent;
			}
			bubble.nameColor = NameColor(ColorIndex(accent, id));
		}

		float textWidth = 0;
		for (const std::string& l : bubble.lines)
			textWidth = std::max(textWidth, plain.StringWidth(l.c_str()));
		if (!bubble.name.empty()) {
			textWidth = std::max(textWidth, std::min(maxText,
				bold.StringWidth(bubble.name.c_str())));
		}

		float photoWidth = 0;
		float photoHeight = 0;
		if (message.photoFile != 0 && message.photoWidth > 0) {
			photoWidth = std::min(maxBubble - 6 * m.s, 300 * m.s);
			photoHeight = photoWidth * message.photoHeight / message.photoWidth;
			photoHeight = std::min(photoHeight, 400 * m.s);
		}

		float timeWidth = TimeWidth(message, small, bubble.read, m.s);
		float gap = 8 * m.s;
		float bubbleWidth = std::max(textWidth + 2 * m.padX,
			photoWidth > 0 ? photoWidth + 6 * m.s : 0.0f);
		bool timeOwnLine = false;
		if (!bubble.lines.empty()) {
			float last = plain.StringWidth(bubble.lines.back().c_str());
			if (last + gap + timeWidth <= maxText) {
				bubbleWidth = std::max(bubbleWidth,
					last + gap + timeWidth + 2 * m.padX);
			} else {
				timeOwnLine = true;
			}
		} else if (photoWidth == 0) {
			bubbleWidth = std::max(bubbleWidth, timeWidth + 2 * m.padX);
		}

		float height;
		if (photoWidth > 0 && bubble.lines.empty() && bubble.name.empty()) {
			// A bare photo; the time sits on it.
			height = photoHeight + 6 * m.s;
		} else {
			height = 2 * m.padY + bubble.lines.size() * m.line
				+ (bubble.name.empty() ? 0 : m.line + 2 * m.s)
				+ (photoHeight > 0 ? photoHeight + m.padY : 0)
				+ (timeOwnLine ? m.line * 0.85f : 0);
			if (bubble.lines.empty() && photoHeight > 0)
				height -= m.padY;
		}

		float left = message.outgoing ? width - m.margin - bubbleWidth
			: m.margin + indent;
		bubble.frame = BRect(left, y, left + bubbleWidth, y + height);
		if (photoHeight > 0) {
			float top = bubble.name.empty() && bubble.lines.empty()
				? y + 3 * m.s
				: y + m.padY + (bubble.name.empty() ? 0 : m.line + 2 * m.s);
			bubble.photo = BRect(left + 3 * m.s, top,
				left + 3 * m.s + photoWidth, top + photoHeight);
		}
		y += height;
		fBubbles.push_back(bubble);
	}

	// A run's last bubble gets the tail, and in groups the avatar.
	for (size_t i = 0; i < fBubbles.size(); i++) {
		bool last = i + 1 == fBubbles.size()
			|| !fBubbles[i + 1].day.empty()
			|| !SameSender(fBubbles[i].message, fBubbles[i + 1].message)
			|| fBubbles[i + 1].message.date - fBubbles[i].message.date >= 600;
		fBubbles[i].tail = last;
		fBubbles[i].avatar = last && fIsGroup && !fBubbles[i].message.outgoing;
	}
	fHeight = y + m.groupGap;
	// A short chat sits at the bottom, by the compose field.
	float slack = Bounds().Height() - fHeight;
	if (slack > 0) {
		for (Bubble& bubble : fBubbles) {
			bubble.frame.OffsetBy(0, slack);
			if (bubble.photo.IsValid())
				bubble.photo.OffsetBy(0, slack);
			bubble.dayTop += slack;
		}
		fHeight += slack;
	}
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

void MessageView::DrawBubble(const Bubble& bubble)
{
	BFont plain;
	GetFont(&plain);
	Metrics m(theme::Scale(), LineHeight(plain));
	font_height height;
	plain.GetHeight(&height);
	const BRect frame = bubble.frame;
	const MessageInfo& message = bubble.message;
	rgb_color fill = message.outgoing ? kOutgoing : kIncoming;
	float r = m.radius;

	// A faint shadow under the bubble.
	SetHighColor(rgb_color{ 0, 0, 0, 22 });
	FillRoundRect(frame.OffsetByCopy(0, roundf(m.s)), r, r);
	SetHighColor(fill);
	FillRoundRect(frame, r, r);
	if (bubble.tail) {
		// Square the corner on the tail's side and draw the tail out of it.
		float t = 7 * m.s;
		BShape tail;
		if (message.outgoing) {
			FillRect(BRect(frame.right - r, frame.bottom - r, frame.right,
				frame.bottom));
			tail.MoveTo(BPoint(frame.right, frame.bottom - 11 * m.s));
			BPoint curve[3] = {
				BPoint(frame.right, frame.bottom - 4 * m.s),
				BPoint(frame.right + t * 0.4f, frame.bottom),
				BPoint(frame.right + t, frame.bottom),
			};
			tail.BezierTo(curve);
			tail.LineTo(BPoint(frame.right - 1, frame.bottom));
		} else {
			FillRect(BRect(frame.left, frame.bottom - r, frame.left + r,
				frame.bottom));
			tail.MoveTo(BPoint(frame.left, frame.bottom - 11 * m.s));
			BPoint curve[3] = {
				BPoint(frame.left, frame.bottom - 4 * m.s),
				BPoint(frame.left - t * 0.4f, frame.bottom),
				BPoint(frame.left - t, frame.bottom),
			};
			tail.BezierTo(curve);
			tail.LineTo(BPoint(frame.left + 1, frame.bottom));
		}
		tail.Close();
		// Shapes are drawn from the pen position, which the last
		// DrawString() left at the end of its text.
		MovePenTo(B_ORIGIN);
		FillShape(&tail);
	}

	float y = frame.top + m.padY;
	if (!bubble.name.empty()) {
		BFont bold(be_bold_font);
		SetFont(&bold);
		SetHighColor(bubble.nameColor);
		BString name(bubble.name.c_str());
		bold.TruncateString(&name, B_TRUNCATE_END, frame.Width() - 2 * m.padX);
		DrawString(name.String(), BPoint(frame.left + m.padX,
			y + height.ascent));
		SetFont(&plain);
		y += m.line + 2 * m.s;
	}
	if (bubble.photo.IsValid()) {
		BBitmap* bitmap = Photo(message.photoFile);
		if (bitmap != NULL) {
			DrawBitmap(bitmap, bitmap->Bounds(), bubble.photo,
				B_FILTER_BITMAP_BILINEAR);
		} else {
			SetHighColor(rgb_color{ 205, 214, 222, 255 });
			FillRoundRect(bubble.photo, 6 * m.s, 6 * m.s);
		}
		y = bubble.photo.bottom + m.padY;
	}
	SetHighColor(message.text.empty() ? kMuted : kText);
	for (const std::string& l : bubble.lines) {
		DrawString(l.c_str(), BPoint(frame.left + m.padX, y + height.ascent));
		y += m.line;
	}

	// The time, and for own messages the ticks or the clock.
	BFont small(be_plain_font);
	small.SetSize(m.smallSize);
	SetFont(&small);
	std::string when = ClockTime(message.date);
	float timeWidth = TimeWidth(message, small, bubble.read, m.s);
	bool overlay = bubble.photo.IsValid() && bubble.lines.empty()
		&& bubble.name.empty();
	float right = frame.right - (overlay ? 10 * m.s : m.padX);
	float baseline = frame.bottom - (overlay ? 10 * m.s : 5 * m.s);
	rgb_color color = message.outgoing ? kOutgoingTime : kIncomingTime;
	if (overlay) {
		font_height smallHeight;
		small.GetHeight(&smallHeight);
		BRect pill(right - timeWidth - 6 * m.s,
			baseline - smallHeight.ascent - 3 * m.s, right + 6 * m.s,
			baseline + smallHeight.descent + 3 * m.s);
		SetHighColor(rgb_color{ 0, 0, 0, 90 });
		FillRoundRect(pill, pill.Height() / 2, pill.Height() / 2);
		color = kWhite;
	}
	if (message.outgoing) {
		float size = small.Size();
		if (message.sending)
			DrawClock(this, BPoint(right, baseline), size, color);
		else if (!message.failed)
			DrawChecks(this, BPoint(right, baseline), size, bubble.read, color);
		right -= ChecksWidth(size, bubble.read) + 3 * m.s;
	}
	SetHighColor(color);
	DrawString(when.c_str(), BPoint(right - small.StringWidth(when.c_str()),
		baseline));
	SetFont(&plain);
}

void MessageView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	// The wallpaper, fixed to the visible area: ScrollTo() redraws all.
	BGradientLinear wall(bounds.LeftTop(), bounds.RightBottom());
	wall.AddColor(kWallTop, 0);
	wall.AddColor(kWallBottom, 255);
	SetDrawingMode(B_OP_COPY);
	FillRect(updateRect, wall);
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	BFont plain;
	GetFont(&plain);
	Metrics m(theme::Scale(), LineHeight(plain));
	font_height height;
	plain.GetHeight(&height);

	auto pill = [&](const char* text, float top) {
		float w = plain.StringWidth(text) + 20 * m.s;
		float h = m.line + 6 * m.s;
		BRect r(roundf((bounds.Width() - w) / 2), top, 0, top + h);
		r.right = r.left + w;
		if (!r.Intersects(updateRect))
			return;
		SetHighColor(kServiceBg);
		FillRoundRect(r, h / 2, h / 2);
		SetHighColor(kWhite);
		DrawString(text, BPoint(r.left + 10 * m.s,
			r.top + 3 * m.s + height.ascent));
	};

	if (fChatId == 0) {
		pill("Select a chat to start messaging",
			bounds.top + (bounds.Height() - m.line) / 2);
		return;
	}

	for (const Bubble& bubble : fBubbles) {
		if (!bubble.day.empty())
			pill(bubble.day.c_str(), bubble.dayTop);
		BRect extended = bubble.frame;
		extended.InsetBy(-12 * m.s, -2 * m.s);
		if (bubble.avatar)
			extended.left -= m.avatar + m.avatarGap;
		if (!extended.Intersects(updateRect))
			continue;
		DrawBubble(bubble);
		if (bubble.avatar) {
			const MessageInfo& message = bubble.message;
			UserInfo user;
			std::string name = TdService::Get().SenderName(message);
			int32_t photo = 0;
			int32_t accent = -1;
			if (message.senderUser != 0
				&& TdService::Get().User(message.senderUser, &user)) {
				photo = user.photoFile;
				accent = user.accent;
			} else if (message.senderChat != 0) {
				ChatInfo chat;
				if (TdService::Get().Chat(message.senderChat, &chat)) {
					photo = chat.photoFile;
					accent = chat.accent;
				}
			}
			BRect frame(m.margin, bubble.frame.bottom - m.avatar + 1,
				m.margin + m.avatar - 1, bubble.frame.bottom);
			int64_t id = message.senderUser != 0 ? message.senderUser
				: message.senderChat;
			DrawAvatar(this, frame, photo, ColorIndex(accent, id), name);
			SetFont(&plain);
		}
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
	bar->SetSteps(20 * theme::Scale(), visible * 0.9f);
}

void MessageView::ScrollTo(BPoint where)
{
	BView::ScrollTo(where);
	// The wallpaper does not scroll with the messages.
	Invalidate();
	float visible = Bounds().Height();
	fAtBottom = where.y >= fHeight - visible - 4;
	if (where.y < 4 && fChatId != 0 && Window() != NULL)
		Window()->PostMessage(kMsgLoadOlder);
}

// --------------------------------------------------------------- MainWindow

MainWindow::MainWindow()
	:
	BWindow(BRect(80, 80, 980, 700), "R Telegram", B_TITLED_WINDOW,
		B_QUIT_ON_WINDOW_CLOSE)
{
	const float s = theme::Scale();

	// Left: the search field over the chat list.
	fSearch = new PlainTextView("search", "Search");
	fSearch->SetChangeMessage(new BMessage(kMsgSearch));
	fSearch->SetEnterMessage(new BMessage(kMsgSearchEnter));
	fSearch->SetAutoGrow(1, 1);
	Bar* searchBar = new Bar("search bar", B_VERTICAL, Bar::kNone);
	searchBar->GroupLayout()->SetInsets(10 * s, 10 * s, 10 * s, 8 * s);
	searchBar->GroupLayout()->AddView(new SearchBox(fSearch));

	fChats = new ChatListView("chats");
	fChats->SetSelectionMessage(new BMessage(kMsgChatSelected));
	BScrollView* chatScroll = new BScrollView("chat scroll", fChats, 0,
		false, true, B_NO_BORDER);

	// Right: the header, the messages, the compose bar.
	fHeader = new ChatHeader();
	fMessages = new MessageView();
	fMessageScroll = new BScrollView("message scroll", fMessages, 0, false,
		true, B_NO_BORDER);
	fCompose = new PlainTextView("compose",
		"Write a message" B_UTF8_ELLIPSIS);
	fCompose->SetEnterMessage(new BMessage(kMsgSend));
	fCompose->SetChangeMessage(new BMessage(kMsgComposeChanged));
	fCompose->SetAutoGrow(1, 6);
	fSend = new SendButton(new BMessage(kMsgSend));
	fComposeBar = new Bar("compose bar", B_HORIZONTAL, Bar::kTop);
	fComposeBar->GroupLayout()->SetInsets(16 * s, 12 * s, 10 * s, 12 * s);
	fComposeBar->GroupLayout()->SetSpacing(8 * s);
	fComposeBar->GroupLayout()->AddView(fCompose);
	fComposeBar->GroupLayout()->AddView(fSend);

	BSplitView* split = new BSplitView(B_HORIZONTAL, 0);
	BLayoutBuilder::Split<>(split)
		.AddGroup(B_VERTICAL, 0, 0.32f)
			.Add(searchBar)
			.Add(chatScroll)
		.End()
		.AddGroup(B_VERTICAL, 0, 0.68f)
			.Add(fHeader)
			.Add(fMessageScroll)
			.Add(fComposeBar)
		.End();
	split->SetCollapsible(false);
	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(split);

	// Until a chat is open there is nothing to head or to write to.
	fHeader->Hide();
	fComposeBar->Hide();

	TdService::Get().AddTarget(BMessenger(this));
	RefreshChats();
	// Not B_AUTO_UPDATE_SIZE_LIMITS: that pins the window to what the layout
	// reports. Only a floor, then any size at all.
	SetSizeLimits(roundf(560 * s), B_SIZE_UNLIMITED, roundf(360 * s),
		B_SIZE_UNLIMITED);
	ResizeTo(roundf(960 * s), roundf(640 * s));
	CenterOnScreen();
}

void MainWindow::RefreshChats()
{
	std::vector<ChatInfo> chats = TdService::Get().Chats();
	std::string filter = Lower(fSearch->Text());
	// Rebuild in place: a list of a few hundred items is cheap to redraw, and
	// keeping BListItems in step with a reordering model is not.
	fChats->SetSelectionMessage(NULL);
	fChats->ClearHover();
	while (BListItem* item = fChats->RemoveItem((int32)0))
		delete item;
	int32 index = -1;
	for (const ChatInfo& chat : chats) {
		if (!filter.empty()
			&& Lower(chat.title).find(filter) == std::string::npos) {
			continue;
		}
		fChats->AddItem(new ChatItem(chat));
		if (chat.id == fOpenChat)
			index = fChats->CountItems() - 1;
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
	if (fHeader->IsHidden())
		fHeader->Show();
	if (fComposeBar->IsHidden())
		fComposeBar->Show();
	fHeader->SetChat(fOpenChat);
	fMessages->SetChat(fOpenChat, item->Info().isGroup);
	TdService::Get().OpenChat(fOpenChat);
	fCompose->SetText("");
	fCompose->MakeFocus(true);
}

void MainWindow::SendCompose()
{
	std::string text = fCompose->Text();
	// Leading and trailing blank lines are not worth sending.
	size_t first = text.find_first_not_of(" \n");
	if (fOpenChat == 0 || first == std::string::npos)
		return;
	text = text.substr(first, text.find_last_not_of(" \n") - first + 1);
	TdService::Get().SendText(fOpenChat, text);
	fCompose->SetText("");
	fSend->SetEnabled(false);
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
			// Updates come in bursts (hundreds of users at start); rebuild
			// the list once per burst.
			if (!fRefreshPending) {
				fRefreshPending = true;
				BMessage refresh(kMsgRefreshChats);
				BMessageRunner::StartSending(BMessenger(this), &refresh,
					150000, 1);
			}
			break;
		case kMsgRefreshChats:
			fRefreshPending = false;
			RefreshChats();
			fHeader->Refresh();
			break;
		case kMsgChatSelected:
			OpenSelected();
			break;
		case kMsgSearch:
			RefreshChats();
			break;
		case kMsgSearchEnter:
			if (fChats->CountItems() > 0) {
				fChats->Select(0);
				fChats->ScrollToSelection();
			}
			break;
		case kMsgComposeChanged: {
			std::string text = fCompose->Text();
			fSend->SetEnabled(
				text.find_first_not_of(" \n") != std::string::npos);
			break;
		}
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
			if (message->FindInt32("file", &file) == B_OK) {
				fMessages->FileReady(file);
				// Avatars in the list.
				fChats->Invalidate();
			}
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
