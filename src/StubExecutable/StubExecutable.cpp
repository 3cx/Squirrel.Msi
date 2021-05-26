// StubExecutable.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "StubExecutable.h"

#include "semver200.h"

using namespace std;

wchar_t* FindRootAppDir() 
{
	wchar_t* ourDirectory = new wchar_t[MAX_PATH];

	GetModuleFileName(GetModuleHandle(NULL), ourDirectory, MAX_PATH);
	wchar_t* lastSlash = wcsrchr(ourDirectory, L'\\');
	if (!lastSlash) {
		delete[] ourDirectory;
		return NULL;
	}

	// Null-terminate the string at the slash so now it's a directory
	*lastSlash = 0x0;
	return ourDirectory;
}

wchar_t* FindOwnExecutableName() 
{
	wchar_t* ourDirectory = new wchar_t[MAX_PATH];

	GetModuleFileName(GetModuleHandle(NULL), ourDirectory, MAX_PATH);
	wchar_t* lastSlash = wcsrchr(ourDirectory, L'\\');
	if (!lastSlash) {
		delete[] ourDirectory;
		return NULL;
	}

	wchar_t* ret = _wcsdup(lastSlash + 1);
	delete[] ourDirectory;
	return ret;
}


bool FileInUse(const wchar_t* filename) {
	HANDLE fh;
	fh = CreateFile(filename, GENERIC_READ, 0 /* no sharing! exclusive */, NULL, OPEN_EXISTING, 0, NULL);
	if ((fh != NULL) && (fh != INVALID_HANDLE_VALUE))
	{
		// the only open file to filename should be fh.
		// do something
		CloseHandle(fh);
		return false;
	}
	else {
		return true;
	}
}

int DeleteDirectory(const wchar_t* pathname, bool dryRun)
{
	std::wstring str(pathname);
	if (!str.empty())
	{
		while (*str.rbegin() == '\\' || *str.rbegin() == '/')
		{
			str.erase(str.size() - 1);
		}
	}
	replace(str.begin(), str.end(), '/', '\\');

	auto dwAttrib = GetFileAttributes(str.c_str());
	if (dwAttrib == INVALID_FILE_ATTRIBUTES) {
		// File doesn't exist, let's consider it deleted
		return 1;
	}

	if (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)
	{
		HANDLE hFind;
		WIN32_FIND_DATA FindFileData;

		TCHAR DirPath[MAX_PATH];
		TCHAR FileName[MAX_PATH];

		_tcscpy(DirPath, str.c_str());
		_tcscat(DirPath, L"\\*");
		_tcscpy(FileName, str.c_str());
		_tcscat(FileName, L"\\");

		hFind = FindFirstFile(DirPath, &FindFileData);
		if (hFind == INVALID_HANDLE_VALUE) return 0;
		_tcscpy(DirPath, FileName);

		bool bSearch = true;
		while (bSearch)
		{
			if (FindNextFile(hFind, &FindFileData))
			{
				if (!(_tcscmp(FindFileData.cFileName, L".") &&
					_tcscmp(FindFileData.cFileName, L".."))) continue;
				_tcscat(FileName, FindFileData.cFileName);
				if ((FindFileData.dwFileAttributes &
					FILE_ATTRIBUTE_DIRECTORY))
				{
					if (!DeleteDirectory(FileName, dryRun))
					{
						FindClose(hFind);
						return 0;
					}
					_tcscpy(FileName, DirPath);
				}
				else
				{
					if (!dryRun && (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
						_wchmod(FileName, _S_IWRITE);
					if ((dryRun && FileInUse(FileName)) || (!dryRun && !DeleteFile(FileName)))
					{
						FindClose(hFind);
						return 0;
					}
					_tcscpy(FileName, DirPath);
				}
			}
			else
			{
				if (GetLastError() == ERROR_NO_MORE_FILES)
					bSearch = false;
				else
				{
					FindClose(hFind);
					return 0;
				}
			}
		}
		FindClose(hFind);

		return dryRun || (RemoveDirectory(str.c_str()) == true);
	}
	else
	{
		// Can't delete a file with this fuctions
		return 0;
	}
}

int DeleteDirectorySafe(const wchar_t* pathname) {
	if (DeleteDirectory(pathname, true)) {
		return DeleteDirectory(pathname, false);
	}
	else {
		return 0;
	}
}

std::wstring FindLatestAppDir() 
{
	const std::wstring ourDir(FindRootAppDir());
	auto target = ourDir + L"\\app";

	WIN32_FIND_DATA fileInfo = { 0 };
	HANDLE hFile = FindFirstFile((ourDir + L"\\app-*").c_str(), &fileInfo);
	if (hFile == INVALID_HANDLE_VALUE) {
		// No app-* folder found, do nothing and launch app
		return target;
	}

	version::Semver200_version acc("0.0.0");
	std::wstring acc_s;

	do {
		std::wstring appVer = fileInfo.cFileName;
		appVer = appVer.substr(4);   // Skip 'app-'
		if (!(fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			continue;
		}

		std::string s(appVer.begin(), appVer.end());
		version::Semver200_version thisVer(s);

		auto source = ourDir + L"\\" + fileInfo.cFileName;
		if (thisVer > acc) {
			acc = thisVer;
			acc_s = appVer;
			if (DeleteDirectorySafe(target.c_str())) {
				// Proper way to move directories
				MoveFileEx(source.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH);
			}
		}
		else {
			// Old version, not needed
			DeleteDirectorySafe(source.c_str());
		}
	} while (FindNextFile(hFile, &fileInfo));

	FindClose(hFile);
	return target;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	std::wstring appName;
	appName.assign(FindOwnExecutableName());

	std::wstring workingDir(FindLatestAppDir());
	std::wstring fullPath(workingDir + L"\\" + appName);

	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };

	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = nCmdShow;

	std::wstring cmdLine(L"\"");
	cmdLine += fullPath;
	cmdLine += L"\" ";
	cmdLine += lpCmdLine;

	wchar_t* lpCommandLine = _wcsdup(cmdLine.c_str());
	wchar_t* lpCurrentDirectory = _wcsdup(workingDir.c_str());
	if (!CreateProcess(NULL, lpCommandLine, NULL, NULL, true, 0, NULL, lpCurrentDirectory, &si, &pi)) {
		return -1;
	}

	AllowSetForegroundWindow(pi.dwProcessId);
	WaitForInputIdle(pi.hProcess, 5 * 1000);
	return 0;
}
