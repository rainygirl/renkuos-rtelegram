#include "Widgets.h"

#include "MainWindow.h"
#include "TdService.h"
#include "Theme.h"

#include <LayoutUtils.h>
#include <Message.h>
#include <Window.h>

#include <algorithm>
#include <math.h>

using namespace theme;

// ------------------------------------------------------------ PlainTextView

PlainTextView::PlainTextView(const char* name, const char* placeholder)
	:
	BTextView(name),
	fPlaceholder(placeholder)
{
	SetWordWrap(true);
	SetStylable(false);
	SetViewColor(kWhite);
	SetLowColor(kWhite);
	rgb_color text = kText;
	SetFontAndColor(be_plain_font, B_FONT_ALL, &text);
}

PlainTextView::~PlainTextView()
{
	delete fEnterMessage;
	delete fChangeMessage;
}

void PlainTextView::SetEnterMessage(BMessage* message)
{
	delete fEnterMessage;
	fEnterMessage = message;
}

void PlainTextView::SetChangeMessage(BMessage* message)
{
	delete fChangeMessage;
	fChangeMessage = message;
}

void PlainTextView::SetAutoGrow(int minLines, int maxLines)
{
	fMinLines = minLines;
	fMaxLines = maxLines;
	FitHeight();
}

void PlainTextView::Draw(BRect updateRect)
{
	BTextView::Draw(updateRect);
	if (TextLength() > 0 || fPlaceholder.empty())
		return;
	font_height height;
	GetFontHeight(&height);
	BRect text = TextRect();
	SetHighColor(kMuted);
	SetLowColor(ViewColor());
	DrawString(fPlaceholder.c_str(),
		BPoint(text.left + 1, text.top + height.ascent));
}

void PlainTextView::KeyDown(const char* bytes, int32 numBytes)
{
	if (fEnterMessage != NULL && numBytes == 1 && bytes[0] == B_ENTER
		&& (modifiers() & B_SHIFT_KEY) == 0) {
		if (Window() != NULL)
			Window()->PostMessage(fEnterMessage);
		return;
	}
	BTextView::KeyDown(bytes, numBytes);
}

void PlainTextView::FrameResized(float width, float height)
{
	BTextView::FrameResized(width, height);
	// Rewrapping can change the number of lines.
	FitHeight();
	Invalidate();
}

void PlainTextView::MakeFocus(bool focus)
{
	BTextView::MakeFocus(focus);
	// Keep the placeholder drawn whatever the focus does to the caret.
	Invalidate();
}

void PlainTextView::InsertText(const char* text, int32 length, int32 offset,
	const text_run_array* runs)
{
	bool wasEmpty = TextLength() == 0;
	BTextView::InsertText(text, length, offset, runs);
	if (wasEmpty)
		Invalidate();
	Changed();
}

void PlainTextView::DeleteText(int32 fromOffset, int32 toOffset)
{
	BTextView::DeleteText(fromOffset, toOffset);
	if (TextLength() == 0)
		Invalidate();
	Changed();
}

void PlainTextView::Changed()
{
	if (fChangeMessage != NULL && Window() != NULL)
		Window()->PostMessage(fChangeMessage);
	FitHeight();
}

void PlainTextView::FitHeight()
{
	if (fMaxLines <= 0)
		return;
	int lines = std::max(fMinLines, std::min(fMaxLines, (int)CountLines()));
	// From the font, not LineHeight(): before the view is attached that
	// has no line to measure.
	BFont font;
	GetFontAndColor(0, &font);
	font_height fh;
	font.GetHeight(&fh);
	float line = ceilf(fh.ascent + fh.descent + fh.leading);
	float height = line * lines + 1;
	if (height == fFittedHeight)
		return;
	fFittedHeight = height;
	SetExplicitMinSize(BSize(B_SIZE_UNSET, height));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, height));
	InvalidateLayout();
}

// --------------------------------------------------------------------- Bar

Bar::Bar(const char* name, orientation direction, Edge edge)
	:
	BGroupView(name, direction, 0),
	fEdge(edge)
{
	SetFlags(Flags() | B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE);
	SetViewColor(kWhite);
}

void Bar::Draw(BRect updateRect)
{
	if (fEdge == kNone)
		return;
	BRect bounds = Bounds();
	SetHighColor(kDivider);
	float y = fEdge == kTop ? bounds.top : bounds.bottom;
	StrokeLine(BPoint(bounds.left, y), BPoint(bounds.right, y));
}

void Bar::FrameResized(float width, float height)
{
	BGroupView::FrameResized(width, height);
	Invalidate();
}

// --------------------------------------------------------------- SearchBox

SearchBox::SearchBox(PlainTextView* field)
	:
	BGroupView("search box", B_HORIZONTAL, 0),
	fField(field)
{
	SetFlags(Flags() | B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE);
	SetViewColor(kWhite);
	field->SetViewColor(kSearchBg);
	field->SetLowColor(kSearchBg);
	const float s = theme::Scale();
	GroupLayout()->SetInsets(12 * s, 7 * s, 12 * s, 7 * s);
	GroupLayout()->AddView(field);
}

void SearchBox::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	float r = bounds.Height() / 2;
	SetHighColor(kSearchBg);
	FillRoundRect(bounds, r, r);
}

void SearchBox::MouseDown(BPoint where)
{
	fField->MakeFocus(true);
}

// -------------------------------------------------------------- SendButton

SendButton::SendButton(BMessage* message)
	:
	BView("send", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	fMessage(message)
{
	SetViewColor(kWhite);
	float size = roundf(34 * theme::Scale());
	SetExplicitMinSize(BSize(size, size));
	SetExplicitMaxSize(BSize(size, size));
}

SendButton::~SendButton()
{
	delete fMessage;
}

void SendButton::SetEnabled(bool enabled)
{
	if (enabled == fEnabled)
		return;
	fEnabled = enabled;
	Invalidate();
}

void SendButton::Draw(BRect updateRect)
{
	BRect b = Bounds();
	float side = std::min(b.Width(), b.Height()) * 0.62f;
	float x = b.left + (b.Width() - side) / 2;
	float y = b.top + (b.Height() - side) / 2;
	auto at = [&](float u, float v) { return BPoint(x + u * side, y + v * side); };
	// A paper plane pointing right, with a notch at the back.
	BPoint plane[] = {
		at(0.06f, 0.10f), at(0.98f, 0.50f), at(0.06f, 0.90f),
		at(0.16f, 0.56f), at(0.58f, 0.50f), at(0.16f, 0.44f),
	};
	SetDrawingMode(B_OP_ALPHA);
	SetHighColor(fEnabled ? kAccent : kBadgeMuted);
	FillPolygon(plane, 6);
}

void SendButton::MouseDown(BPoint where)
{
	if (fEnabled && Window() != NULL)
		Window()->PostMessage(fMessage);
}

// -------------------------------------------------------------- ChatHeader

ChatHeader::ChatHeader()
	:
	BView("header", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
{
	SetViewColor(kWhite);
	font_height height;
	be_plain_font->GetHeight(&height);
	float line = ceilf(height.ascent + height.descent + height.leading);
	float h = roundf(2 * line + 18 * theme::Scale());
	SetExplicitMinSize(BSize(B_SIZE_UNSET, h));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, h));
}

void ChatHeader::SetChat(int64_t chatId)
{
	fChatId = chatId;
	Refresh();
}

void ChatHeader::Refresh()
{
	ChatInfo chat;
	std::string title;
	if (fChatId != 0 && TdService::Get().Chat(fChatId, &chat))
		title = chat.title.empty() ? "Deleted Account" : chat.title;
	bool online = false;
	std::string status = fChatId != 0
		? TdService::Get().ChatStatus(fChatId, &online) : "";
	if (title == fTitle && status == fStatus && online == fOnline)
		return;
	fTitle = title;
	fStatus = status;
	fOnline = online;
	Invalidate();
}

void ChatHeader::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	SetHighColor(kDivider);
	StrokeLine(bounds.LeftBottom(), bounds.RightBottom());
	if (fTitle.empty())
		return;
	const float s = theme::Scale();
	font_height height;
	be_plain_font->GetHeight(&height);
	float line = ceilf(height.ascent + height.descent + height.leading);
	float top = (bounds.Height() - 2 * line) / 2;
	float left = 16 * s;

	BFont bold(be_bold_font);
	SetFont(&bold);
	SetHighColor(kText);
	SetLowColor(kWhite);
	BString title(fTitle.c_str());
	bold.TruncateString(&title, B_TRUNCATE_END, bounds.Width() - 2 * left);
	DrawString(title.String(), BPoint(left, top + height.ascent));

	SetFont(be_plain_font);
	SetHighColor(fOnline ? kAccent : kMuted);
	DrawString(fStatus.c_str(), BPoint(left, top + line + height.ascent));
}

void ChatHeader::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	Invalidate();
}

// ------------------------------------------------------------ ChatListView

ChatListView::ChatListView(const char* name)
	:
	BListView(name)
{
	// Times are right-aligned: a resize moves them, so redraw whole rows,
	// not just the strip the resize exposed.
	SetFlags(Flags() | B_FULL_UPDATE_ON_RESIZE);
	SetViewColor(kWhite);
}

void ChatListView::MouseMoved(BPoint where, uint32 transit,
	const BMessage* dragMessage)
{
	BListView::MouseMoved(where, transit, dragMessage);
	if (transit == B_EXITED_VIEW || transit == B_OUTSIDE_VIEW)
		SetHover(-1);
	else
		SetHover(IndexOf(where));
}

void ChatListView::SetHover(int32 index)
{
	if (index == fHover)
		return;
	if (fHover >= 0 && fHover < CountItems())
		InvalidateItem(fHover);
	fHover = index;
	if (fHover >= 0)
		InvalidateItem(fHover);
}

void ChatListView::ClearHover()
{
	fHover = -1;
}

bool ChatListView::IsHovered(const BListItem* item) const
{
	return fHover >= 0 && ItemAt(fHover) == item;
}
