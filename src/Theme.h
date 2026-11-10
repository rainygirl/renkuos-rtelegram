// R Telegram -- the look: Telegram Desktop's day theme, and the drawing
// that the chat list, the header and the messages share.
#pragma once

#include <GraphicsDefs.h>
#include <Rect.h>

#include <cstdint>
#include <string>

class BView;

namespace theme {

const rgb_color kWhite = { 255, 255, 255, 255 };
const rgb_color kText = { 0, 0, 0, 255 };
const rgb_color kMuted = { 153, 153, 153, 255 };
const rgb_color kAccent = { 64, 167, 227, 255 };		// windowBgActive
const rgb_color kSelected = { 65, 159, 217, 255 };		// a chat row picked
const rgb_color kSelectedMuted = { 230, 243, 252, 255 };
const rgb_color kHover = { 241, 241, 241, 255 };
const rgb_color kBadgeMuted = { 187, 187, 187, 255 };
const rgb_color kDivider = { 231, 231, 231, 255 };
const rgb_color kSearchBg = { 241, 241, 241, 255 };
const rgb_color kPreviewSender = { 54, 138, 209, 255 };
const rgb_color kChecks = { 79, 174, 78, 255 };		// sent/read, in the list

const rgb_color kIncoming = { 255, 255, 255, 255 };
const rgb_color kOutgoing = { 239, 253, 222, 255 };
const rgb_color kIncomingTime = { 160, 172, 182, 255 };
const rgb_color kOutgoingTime = { 98, 172, 85, 255 };
const rgb_color kServiceBg = { 0, 0, 0, 70 };			// date pills

// The chat background: a soft diagonal gradient in the spirit of Telegram's
// default wallpaper, fixed to the visible area.
const rgb_color kWallTop = { 219, 225, 190, 255 };
const rgb_color kWallBottom = { 150, 194, 160, 255 };

// 1 at the default 12 pt system font.
float Scale();

// Telegram's seven peer colours, for names and avatars. `accent` is the
// accent_color_id TDLib reports; ids past the basic seven, and unknown ones,
// fall back to the id.
int ColorIndex(int32_t accent, int64_t id);
rgb_color NameColor(int index);
void AvatarColors(int index, rgb_color* top, rgb_color* bottom);

// Up to two letters from the first two words, upper-cased.
std::string Initials(const std::string& name);

// "14:05" today, "Mon" this week, "06.11.25" before.
std::string ListTime(int32_t date);
std::string ClockTime(int32_t date);
std::string DayLabel(int32_t date);

// A round avatar: the photo if it is downloaded (the download is started if
// not), the coloured circle with initials otherwise.
void DrawAvatar(BView* view, BRect frame, int32_t photoFile, int colorIndex,
	const std::string& name);

// One tick or two, as Telegram marks sent and read messages.
void DrawChecks(BView* view, BPoint rightBaseline, float size, bool read,
	rgb_color color);
float ChecksWidth(float size, bool read);

// A clock face, for a message still being sent.
void DrawClock(BView* view, BPoint rightBaseline, float size,
	rgb_color color);

}  // namespace theme
