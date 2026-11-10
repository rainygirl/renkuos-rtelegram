#include "Settings.h"

#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>

#include <stdlib.h>
#include <sys/stat.h>

namespace settings {

std::string Directory()
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return "/boot/home/config/settings/RTelegram";
	path.Append("RTelegram");
	create_directory(path.Path(), 0700);
	return path.Path();
}

std::string DatabaseDirectory()
{
	return Directory() + "/tdlib";
}

std::string FilesDirectory()
{
	return Directory() + "/files";
}

static std::string CredentialsPath()
{
	return Directory() + "/api";
}

// The file holds two lines: the api_id, then the api_hash.
bool LoadCredentials(ApiCredentials* out)
{
	BFile file(CredentialsPath().c_str(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return false;
	char buffer[256];
	ssize_t size = file.Read(buffer, sizeof(buffer) - 1);
	if (size <= 0)
		return false;
	buffer[size] = '\0';
	std::string text(buffer);
	size_t newline = text.find('\n');
	if (newline == std::string::npos)
		return false;
	ApiCredentials credentials;
	credentials.apiId = atoi(text.substr(0, newline).c_str());
	std::string hash = text.substr(newline + 1);
	while (!hash.empty() && (hash.back() == '\n' || hash.back() == '\r'
			|| hash.back() == ' ')) {
		hash.pop_back();
	}
	credentials.apiHash = hash;
	if (!credentials.Valid())
		return false;
	*out = credentials;
	return true;
}

bool SaveCredentials(const ApiCredentials& credentials)
{
	if (!credentials.Valid())
		return false;
	std::string path = CredentialsPath();
	BFile file(path.c_str(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return false;
	std::string text = std::to_string(credentials.apiId) + "\n"
		+ credentials.apiHash + "\n";
	if (file.Write(text.data(), text.size()) != (ssize_t)text.size())
		return false;
	chmod(path.c_str(), 0600);
	return true;
}

}  // namespace settings
