// R Telegram -- the two windows before the chat list: the API credentials
// (asked once) and the Telegram login (phone, code, password).
#pragma once

#include <Window.h>

class BButton;
class BStringView;
class BTextControl;
class BTextView;

// Asks for api_id and api_hash, saves them, and posts kMsgCredentialsSaved
// to the application.
class CredentialsWindow : public BWindow {
public:
	CredentialsWindow();
	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	BTextControl* fApiId;
	BTextControl* fApiHash;
	BStringView* fStatus;
};

// Shows the step TdService is waiting for and sends what the user types.
class LoginWindow : public BWindow {
public:
	LoginWindow();
	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	void ShowState();

	BStringView* fPrompt;
	BTextView* fHelp;
	BTextControl* fField;
	BButton* fNext;
	BStringView* fError;
};

enum {
	kMsgCredentialsSaved = 'rtCs',
};
