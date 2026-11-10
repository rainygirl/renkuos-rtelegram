// R Telegram -- where things live on disk.
//
// The Telegram API credentials are the user's own, from my.telegram.org. They
// are never compiled in or packaged: the app asks for them on first start and
// keeps them in ~/config/settings/RTelegram/api, readable only by the user.
#pragma once

#include <string>

namespace settings {

struct ApiCredentials {
	int32_t apiId = 0;
	std::string apiHash;

	bool Valid() const { return apiId > 0 && apiHash.size() == 32; }
};

// ~/config/settings/RTelegram, created if missing.
std::string Directory();
// TDLib's database and downloaded files, under Directory().
std::string DatabaseDirectory();
std::string FilesDirectory();

bool LoadCredentials(ApiCredentials* out);
bool SaveCredentials(const ApiCredentials& credentials);

}  // namespace settings
