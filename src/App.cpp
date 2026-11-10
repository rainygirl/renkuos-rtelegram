// R Telegram -- a Telegram client for Haiku: TDLib underneath, the Haiku API
// on top, no Qt.
#include "LoginWindows.h"
#include "MainWindow.h"
#include "Settings.h"
#include "TdService.h"

#include <Application.h>

class App : public BApplication {
public:
	App()
		:
		BApplication("application/x-vnd.rainygirl-RTelegram")
	{
	}

	void ReadyToRun() override
	{
		settings::ApiCredentials credentials;
		if (settings::LoadCredentials(&credentials))
			Start(credentials);
		else
			(new CredentialsWindow())->Show();
	}

	void MessageReceived(BMessage* message) override
	{
		switch (message->what) {
			case kMsgCredentialsSaved: {
				settings::ApiCredentials credentials;
				if (settings::LoadCredentials(&credentials))
					Start(credentials);
				break;
			}
			case kMsgAuthState:
				ShowLoginIfNeeded();
				break;
			default:
				BApplication::MessageReceived(message);
		}
	}

private:
	void Start(const settings::ApiCredentials& credentials)
	{
		if (fStarted)
			return;
		fStarted = true;
		TdService::Get().AddTarget(BMessenger(this));
		TdService::Get().Start(credentials.apiId, credentials.apiHash);
		(new MainWindow())->Show();
	}

	void ShowLoginIfNeeded()
	{
		AuthState state = TdService::Get().State();
		bool needed = state == AuthState::kWaitPhone
			|| state == AuthState::kWaitCode
			|| state == AuthState::kWaitPassword
			|| state == AuthState::kWaitRegistration;
		if (!needed || fLogin.IsValid())
			return;
		LoginWindow* window = new LoginWindow();
		fLogin = BMessenger(window);
		window->Show();
	}

	bool fStarted = false;
	BMessenger fLogin;
};

int main()
{
	App app;
	app.Run();
	return 0;
}
