#include "LoginWindows.h"

#include "Settings.h"
#include "TdService.h"

#include <Application.h>
#include <Button.h>
#include <LayoutBuilder.h>
#include <Roster.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include <math.h>
#include <stdlib.h>

namespace {

enum {
	kMsgSave = 'rtSv',
	kMsgOpenSite = 'rtOs',
	kMsgNext = 'rtNx',
};

// A read-only paragraph. `lines` is how many lines of the plain font it is
// given room for: a BTextView in a layout asks for no height of its own.
BTextView* HelpText(const char* text, int lines)
{
	BTextView* view = new BTextView("help");
	view->SetText(text);
	view->MakeEditable(false);
	view->MakeSelectable(true);
	view->SetWordWrap(true);
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	rgb_color color = ui_color(B_PANEL_TEXT_COLOR);
	view->SetFontAndColor(be_plain_font, B_FONT_ALL, &color);
	font_height height;
	be_plain_font->GetHeight(&height);
	float line = ceilf(height.ascent + height.descent + height.leading);
	view->SetExplicitMinSize(BSize(be_plain_font->Size() * 30, line * lines));
	return view;
}

void OpenUrl(const char* url)
{
	const char* args[] = { url, NULL };
	be_roster->Launch("application/x-vnd.Be.URL.https", 1, args);
}

}  // namespace

CredentialsWindow::CredentialsWindow()
	:
	BWindow(BRect(0, 0, 440, 260), "R Telegram", B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_ZOOMABLE)
{
	fApiId = new BTextControl("api_id", NULL, NULL);
	fApiHash = new BTextControl("api_hash", NULL, NULL);
	fStatus = new BStringView("status", "");
	BButton* site = new BButton("my.telegram.org",
		new BMessage(kMsgOpenSite));
	BButton* save = new BButton("Continue", new BMessage(kMsgSave));
	save->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(HelpText("R Telegram talks to Telegram with your own API "
			"credentials. Sign in at my.telegram.org, open \"API development "
			"tools\", create an application, and copy its api_id and "
			"api_hash here. They are kept in "
			"~/config/settings/RTelegram/api and never leave this machine "
			"except to sign you in.", 8))
		.AddGrid()
			.AddTextControl(fApiId, 0, 0)
			.AddTextControl(fApiHash, 0, 1)
		.End()
		.Add(fStatus)
		.AddGroup(B_HORIZONTAL)
			.Add(site)
			.AddGlue()
			.Add(save)
		.End();
	ResizeToPreferred();
	CenterOnScreen();
}

void CredentialsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgOpenSite:
			OpenUrl("https://my.telegram.org/apps");
			break;
		case kMsgSave: {
			settings::ApiCredentials credentials;
			credentials.apiId = atoi(fApiId->Text());
			credentials.apiHash = fApiHash->Text();
			if (!credentials.Valid()) {
				fStatus->SetText("api_id is a number and api_hash has "
					"32 characters.");
				break;
			}
			if (!settings::SaveCredentials(credentials)) {
				fStatus->SetText("Could not save the credentials.");
				break;
			}
			be_app->PostMessage(kMsgCredentialsSaved);
			Quit();
			break;
		}
		default:
			BWindow::MessageReceived(message);
	}
}

// Only the close box gets here; Quit() after saving does not. Closing
// before the credentials are in leaves nothing to run, so the app ends.
bool CredentialsWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}

LoginWindow::LoginWindow()
	:
	BWindow(BRect(0, 0, 400, 220), "R Telegram", B_TITLED_WINDOW,
		B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_ZOOMABLE)
{
	fPrompt = new BStringView("prompt", "");
	BFont font(be_bold_font);
	font.SetSize(font.Size() * 1.3f);
	fPrompt->SetFont(&font);
	fHelp = HelpText("", 3);
	fField = new BTextControl("field", NULL, "", NULL);
	fNext = new BButton("Next", new BMessage(kMsgNext));
	fNext->MakeDefault(true);
	fError = new BStringView("error", "");
	rgb_color red = { 200, 40, 40, 255 };
	fError->SetHighColor(red);

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(fPrompt)
		.Add(fHelp)
		.Add(fField)
		.Add(fError)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fNext)
		.End();
	TdService::Get().AddTarget(BMessenger(this));
	ShowState();
	ResizeToPreferred();
	CenterOnScreen();
}

void LoginWindow::ShowState()
{
	AuthState state = TdService::Get().State();
	fError->SetText("");
	fField->SetText("");
	fField->TextView()->HideTyping(state == AuthState::kWaitPassword);
	fField->SetEnabled(true);
	fNext->SetEnabled(true);
	switch (state) {
		case AuthState::kStarting:
			fPrompt->SetText("Connecting" B_UTF8_ELLIPSIS);
			fHelp->SetText("");
			fField->SetEnabled(false);
			fNext->SetEnabled(false);
			break;
		case AuthState::kWaitPhone:
			fPrompt->SetText("Your phone number");
			fHelp->SetText("With the country code, for example "
				"+82 10 1234 5678.");
			break;
		case AuthState::kWaitCode:
			fPrompt->SetText("Code");
			fHelp->SetText("Telegram sent a code to your other devices, "
				"or by SMS.");
			break;
		case AuthState::kWaitPassword: {
			fPrompt->SetText("Two-step verification password");
			std::string hint = TdService::Get().PasswordHint();
			fHelp->SetText(hint.empty() ? "" : ("Hint: " + hint).c_str());
			break;
		}
		case AuthState::kWaitRegistration:
			fPrompt->SetText("No account for this number");
			fHelp->SetText("Create the account in the Telegram app on your "
				"phone first, then sign in here.");
			fField->SetEnabled(false);
			fNext->SetEnabled(false);
			break;
		case AuthState::kReady:
			Quit();
			return;
		default:
			fPrompt->SetText("Signing in" B_UTF8_ELLIPSIS);
			fHelp->SetText("");
			fField->SetEnabled(false);
			fNext->SetEnabled(false);
			break;
	}
	fField->MakeFocus(true);
}

void LoginWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgAuthState:
			ShowState();
			break;
		case kMsgError: {
			const char* text = NULL;
			if (message->FindString("text", &text) == B_OK)
				fError->SetText(text);
			fNext->SetEnabled(true);
			break;
		}
		case kMsgNext: {
			std::string text = fField->Text();
			if (text.empty())
				break;
			fNext->SetEnabled(false);
			fError->SetText("");
			switch (TdService::Get().State()) {
				case AuthState::kWaitPhone:
					TdService::Get().SendPhone(text);
					break;
				case AuthState::kWaitCode:
					TdService::Get().SendCode(text);
					break;
				case AuthState::kWaitPassword:
					TdService::Get().SendPassword(text);
					break;
				default:
					break;
			}
			break;
		}
		default:
			BWindow::MessageReceived(message);
	}
}

// Only the close box gets here. Quit() when the login completes does not
// come through QuitRequested(), so signing in leaves the app running; it
// was B_QUIT_ON_WINDOW_CLOSE that made it end there.
bool LoginWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}
