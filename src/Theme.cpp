#include "Theme.h"

#include "TdService.h"

#include <Bitmap.h>
#include <Font.h>
#include <Gradient.h>
#include <GradientLinear.h>
#include <TranslationUtils.h>
#include <View.h>

#include <algorithm>
#include <ctype.h>
#include <map>
#include <math.h>
#include <time.h>
#include <utility>

namespace theme {

namespace {

const rgb_color kNameColors[] = {
	{ 204, 80, 73, 255 },		// red
	{ 214, 119, 34, 255 },		// orange
	{ 149, 92, 219, 255 },		// violet
	{ 64, 169, 32, 255 },		// green
	{ 48, 158, 186, 255 },		// cyan
	{ 54, 138, 209, 255 },		// blue
	{ 199, 80, 139, 255 },		// pink
};

const rgb_color kAvatarTop[] = {
	{ 255, 136, 94, 255 }, { 255, 205, 106, 255 }, { 130, 177, 255, 255 },
	{ 160, 222, 126, 255 }, { 83, 237, 214, 255 }, { 114, 213, 253, 255 },
	{ 224, 162, 243, 255 },
};

const rgb_color kAvatarBottom[] = {
	{ 255, 81, 106, 255 }, { 255, 168, 92, 255 }, { 102, 95, 255, 255 },
	{ 84, 203, 104, 255 }, { 40, 201, 183, 255 }, { 42, 158, 241, 255 },
	{ 214, 105, 237, 255 },
};

// Photos as loaded, and their round versions per diameter.
std::map<int32_t, BBitmap*> sPhotos;
std::map<std::pair<int32_t, int>, BBitmap*> sRound;

BBitmap* Photo(int32_t fileId)
{
	auto it = sPhotos.find(fileId);
	if (it != sPhotos.end())
		return it->second;
	std::string path = TdService::Get().FilePath(fileId);
	if (path.empty())
		return NULL;
	BBitmap* bitmap = BTranslationUtils::GetBitmap(path.c_str());
	sPhotos[fileId] = bitmap;
	return bitmap;
}

// The photo scaled to a circle `size` pixels across, with an antialiased
// edge in the alpha channel.
BBitmap* RoundPhoto(int32_t fileId, int size)
{
	auto key = std::make_pair(fileId, size);
	auto it = sRound.find(key);
	if (it != sRound.end())
		return it->second;
	BBitmap* source = Photo(fileId);
	if (source == NULL)
		return NULL;
	BRect bounds(0, 0, size - 1, size - 1);
	BBitmap* round = new BBitmap(bounds, B_BITMAP_ACCEPTS_VIEWS, B_RGBA32);
	BView* view = new BView(bounds, "round", 0, 0);
	round->AddChild(view);
	round->Lock();
	// Fill the square from the middle of the photo.
	BRect from = source->Bounds();
	float side = std::min(from.Width(), from.Height());
	BRect square(from.left + (from.Width() - side) / 2,
		from.top + (from.Height() - side) / 2, 0, 0);
	square.right = square.left + side;
	square.bottom = square.top + side;
	view->DrawBitmap(source, square, bounds, B_FILTER_BITMAP_BILINEAR);
	view->Sync();
	round->Unlock();
	round->RemoveChild(view);
	delete view;

	uint8* bits = (uint8*)round->Bits();
	int32 row = round->BytesPerRow();
	float radius = size / 2.0f;
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			float dx = x + 0.5f - radius;
			float dy = y + 0.5f - radius;
			float coverage = radius - sqrtf(dx * dx + dy * dy) + 0.5f;
			coverage = std::min(1.0f, std::max(0.0f, coverage));
			bits[y * row + x * 4 + 3] = (uint8)(coverage * 255);
		}
	}
	sRound[key] = round;
	return round;
}

size_t CharLength(unsigned char c)
{
	if (c < 0x80)
		return 1;
	if ((c & 0xE0) == 0xC0)
		return 2;
	if ((c & 0xF0) == 0xE0)
		return 3;
	return 4;
}

}  // namespace

float Scale()
{
	return std::max(1.0f, be_plain_font->Size() / 12.0f);
}

int ColorIndex(int32_t accent, int64_t id)
{
	if (accent >= 0 && accent < 7)
		return accent;
	return (int)(((id % 7) + 7) % 7);
}

rgb_color NameColor(int index)
{
	return kNameColors[index % 7];
}

void AvatarColors(int index, rgb_color* top, rgb_color* bottom)
{
	*top = kAvatarTop[index % 7];
	*bottom = kAvatarBottom[index % 7];
}

std::string Initials(const std::string& name)
{
	std::string result;
	int letters = 0;
	bool wordStart = true;
	for (size_t i = 0; i < name.size() && letters < 2;) {
		unsigned char c = name[i];
		size_t n = CharLength(c);
		if (c == ' ') {
			wordStart = true;
		} else if (wordStart) {
			// Skip what is not a letter at a word start: emoji, brackets, #.
			bool ascii = c < 0x80;
			if (!ascii || isalnum(c)) {
				// A four-byte character is an emoji; leave those out.
				if (n < 4) {
					std::string letter = name.substr(i, n);
					if (ascii)
						letter[0] = toupper(letter[0]);
					result += letter;
					letters++;
				}
				wordStart = false;
			}
		}
		i += n;
	}
	return result;
}

std::string ListTime(int32_t date)
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
	else if (now - when < 6 * 24 * 3600)
		strftime(buffer, sizeof(buffer), "%a", &local);
	else
		strftime(buffer, sizeof(buffer), "%d.%m.%y", &local);
	return buffer;
}

std::string ClockTime(int32_t date)
{
	time_t when = date;
	struct tm local;
	localtime_r(&when, &local);
	char buffer[16];
	strftime(buffer, sizeof(buffer), "%H:%M", &local);
	return buffer;
}

std::string DayLabel(int32_t date)
{
	time_t when = date;
	time_t now = time(NULL);
	struct tm local;
	struct tm today;
	localtime_r(&when, &local);
	localtime_r(&now, &today);
	char buffer[48];
	if (local.tm_year == today.tm_year && local.tm_yday == today.tm_yday)
		return "Today";
	if (local.tm_year == today.tm_year)
		strftime(buffer, sizeof(buffer), "%B %e", &local);
	else
		strftime(buffer, sizeof(buffer), "%B %e, %Y", &local);
	std::string text(buffer);
	// %e pads the day with a space.
	size_t twoSpaces = text.find("  ");
	if (twoSpaces != std::string::npos)
		text.erase(twoSpaces, 1);
	return text;
}

void DrawAvatar(BView* view, BRect frame, int32_t photoFile, int colorIndex,
	const std::string& name)
{
	view->PushState();
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	int size = (int)roundf(frame.Width()) + 1;
	BBitmap* photo = photoFile != 0 ? RoundPhoto(photoFile, size) : NULL;
	if (photo != NULL) {
		view->DrawBitmap(photo, frame.LeftTop());
		view->PopState();
		return;
	}
	rgb_color top;
	rgb_color bottom;
	AvatarColors(colorIndex, &top, &bottom);
	BGradientLinear gradient(frame.LeftTop(), frame.LeftBottom());
	gradient.AddColor(top, 0);
	gradient.AddColor(bottom, 255);
	view->FillEllipse(frame, gradient);

	std::string letters = Initials(name);
	BFont font(be_bold_font);
	font.SetSize(frame.Height() * 0.38f);
	view->SetFont(&font);
	font_height height;
	font.GetHeight(&height);
	float width = font.StringWidth(letters.c_str());
	view->SetHighColor(kWhite);
	view->DrawString(letters.c_str(),
		BPoint(frame.left + (frame.Width() - width) / 2,
			frame.top + (frame.Height() + height.ascent - height.descent) / 2));
	view->PopState();
}

float ChecksWidth(float size, bool read)
{
	return size * (read ? 1.25f : 0.9f);
}

void DrawChecks(BView* view, BPoint rightBaseline, float size, bool read,
	rgb_color color)
{
	view->PushState();
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetHighColor(color);
	view->SetPenSize(std::max(1.0f, size * 0.11f));
	view->SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);
	float h = size * 0.62f;
	float y = rightBaseline.y - size * 0.12f;
	auto tick = [&](float right, bool full) {
		float left = right - size * 0.9f;
		if (full) {
			view->StrokeLine(BPoint(left, y - h * 0.45f),
				BPoint(left + size * 0.3f, y));
		}
		view->StrokeLine(BPoint(left + size * 0.3f, y),
			BPoint(right, y - h));
	};
	float right = rightBaseline.x;
	if (read) {
		tick(right, false);
		tick(right - size * 0.35f, true);
	} else {
		tick(right, true);
	}
	view->PopState();
}

void DrawClock(BView* view, BPoint rightBaseline, float size,
	rgb_color color)
{
	view->PushState();
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetHighColor(color);
	view->SetPenSize(std::max(1.0f, size * 0.09f));
	float r = size * 0.36f;
	BPoint center(rightBaseline.x - r, rightBaseline.y - r - size * 0.05f);
	view->StrokeEllipse(center, r, r);
	view->StrokeLine(center, BPoint(center.x, center.y - r * 0.6f));
	view->StrokeLine(center, BPoint(center.x + r * 0.5f, center.y));
	view->PopState();
}

}  // namespace theme
