#undef UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <tlhelp32.h>

#include "GL/freeglut.h"
#include "Detours.h"

#include "RestAPI/Core/Client.hpp"
#include "RestAPI/Utils/Utils.hpp"
#include "Config/ConfigManager.hpp"
#include "Utils/Hashes.hpp"

#define DLL_VIMEWORLD_ATTACH 0x888

static RestAPI::Client client;
using json = nlohmann::json;

typedef void(__stdcall* glRotatef_t)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
glRotatef_t glRotatef_p = nullptr;

void __stdcall my_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
	if (angle < 0.0F && x == 0.0F && y == 0.0F && z == 1.0F) {
		GLfloat f = angle / 14;
		if (f > -1.0F) {
			glRotatef_p(f * 0.0F, x, y, z);
			return;
		}
	}

	glRotatef_p(angle, x, y, z);
}

void CheckLicense() {
	while (true) {
		client.getdocument(client.user.name, client.user.password, client.user.session, "");

		if (string(client.user.data["session"]) != client.user.session) exit(0);
		if (string(client.user.data["un_hash"]) != Utils::Hashes::GetUnHash()) ExitProcess(0);
		if (string(client.user.data["re_hash"]) != Utils::Hashes::GetReHash()) exit(0);

		if (client.user.data["features"].empty()) exit(0);
		json features = json::parse(client.user.data["features"].dump());
		if (!features.contains("nohurtcam")) ExitProcess(0);
		if (features["nohurtcam"].get<int>() <= 0) exit(0);

		Sleep(5 * 60000);
	}
}

bool isVimeWorld() {
	PROCESSENTRY32 entry;
	ZeroMemory(&entry, sizeof(PROCESSENTRY32));
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (Process32First(snapshot, &entry) == TRUE)
		while (Process32Next(snapshot, &entry) == TRUE)
			if (entry.th32ProcessID == GetCurrentProcessId() && (_stricmp(entry.szExeFile, "VimeWorld.exe") == 0 ||
				_stricmp(entry.szExeFile, "javaw.exe") == 0))
				return true;

	return false;
}

BOOL APIENTRY DllMain(HINSTANCE handle, DWORD reason, LPVOID reserved) {
	switch (reason) {
		case DLL_VIMEWORLD_ATTACH: {
			client.host = "http://api.destructiqn.com:2086";
			client.user.name = ConfigManager::ParseUsername();
			client.user.password = ConfigManager::ParsePassword();
			client.user.session = reinterpret_cast<const char*>(reserved);

			client.getdocument(client.user.name, client.user.password, client.user.session, Utils::Hashes::GetReHash());
			if (!client.user.data["features"].empty()) {
				json features = json::parse(client.user.data["features"].dump());
				if (features.contains("nohurtcam")) {
					if (features["nohurtcam"].get<int>() > 0) {
						client.foobar(client.user.name, isVimeWorld() ? ConfigManager::ParseUsername(true) : "undefined", "NoHurtCam", RestAPI::Utils::get_ip());

						HANDLE hCheckLicense = CreateThread(nullptr, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(CheckLicense), nullptr, NULL, nullptr);
						if (!hCheckLicense) return FALSE;

						HMODULE handle = GetModuleHandleA("atio6axx.dll");
						bool isAMD = handle ? true : false;

						if (isAMD) {
							typedef PROC (__stdcall* DrvGetProcAddress_t)(LPCSTR);
							DrvGetProcAddress_t DrvGetProcAddress = (DrvGetProcAddress_t)GetProcAddress(handle, "DrvGetProcAddress");
							if (!DrvGetProcAddress) return FALSE;
							
							glRotatef_p = reinterpret_cast<glRotatef_t>(DrvGetProcAddress("glRotatef"));
							if (!glRotatef_p) isAMD = false;
						}
						if (!isAMD) {
							handle = GetModuleHandleA("opengl32.dll");
							if (!handle) return FALSE;

							glRotatef_p = reinterpret_cast<glRotatef_t>(GetProcAddress(handle, "glRotatef"));
						}

						// Installing function interceptors
						DetourRestoreAfterWith();
						DetourTransactionBegin();
						DetourUpdateThread(GetCurrentThread());

						if (!glRotatef_p) return FALSE;
						DetourAttach(reinterpret_cast<void**>(&glRotatef_p), my_glRotatef);

						Beep(750, 500);
						return DetourTransactionCommit() == NO_ERROR;
					}
				}
			}

			return FALSE;
		}
		case DLL_PROCESS_DETACH: {
			// Removing the function interceptors
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());

			DetourDetach(reinterpret_cast <void**>(&glRotatef_p), my_glRotatef);
			return DetourTransactionCommit() == NO_ERROR;
		}
	}
	return TRUE;
}