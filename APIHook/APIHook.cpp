// APIHook.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include <Windows.h>
#include <stdio.h>
#include <fstream>    //这个头文件是必须要有的
#include <string>
#include <vector>

using std::vector;
using std::string;
using std::wstring;
using std::ofstream;

// 不同Instance共享的该变量
#pragma data_seg("SHARED")
static HHOOK  hhk = NULL; //鼠标钩子句柄
static DWORD injectPid = -1;	// 需要注入的进程id	(使用HOOK注入时需要)
#pragma data_seg()
#pragma comment(linker, "/section:SHARED,rws")
//以上的变量共享哦!
HINSTANCE hinst = NULL; //本dll的实例句柄 (hook.dll)
bool bHook = false; //是否Hook了函数
bool m_bInjected = false; //是否对API进行了Hook
#ifdef _WIN64
BYTE OldCode[12]; //老的系统API入口代码
BYTE NewCode[12]; //要跳转的API代码 (jmp xxxx)
SIZE_T memSize = 12;
#else
BYTE OldCode[5]; //老的系统API入口代码
BYTE NewCode[5]; //要跳转的API代码 (jmp xxxx)
SIZE_T memSize = 5;
#endif // _WIN64
HANDLE hProcess = NULL; //所处进程的句柄
DWORD dwPid;  //所处进程ID
vector<wstring> dllNames;
vector<string> funcNames;
vector<void *> newFuncs;
vector<FARPROC> pOldFuncs;
vector<vector<BYTE>> OldCodes;
vector<vector<BYTE>> NewCodes;

//end of 变量定义

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam){
	LRESULT RetVal = CallNextHookEx(hhk, nCode, wParam, lParam);
	return RetVal;
}

// 当不使用HOOK注入时，pid值不再重要，亦不需要调用安装和卸载钩子的函数
BOOL WINAPI InstallHook(DWORD pid){
	if (hhk) {
		return true;
	}
	injectPid = pid;
	hhk = ::SetWindowsHookEx(WH_KEYBOARD, MouseProc, hinst, 0);

	if (hhk == NULL){
		DWORD errCode = ::GetLastError();
		char err[1024];
		sprintf_s(err, 1024, "hook install failed!:%d", errCode);
		MessageBoxA(NULL, err, "", 0);
		return false;
	}
	MessageBoxW(NULL, L"hook installed!", L"", 0);
	return true;
}

void WINAPI UninstallHook(){
	if (hhk != NULL)
		::UnhookWindowsHookEx(hhk);
	MessageBoxW(NULL, L"Unhooked", L"", 0);
}

size_t indexFind(string value) {
	size_t index = -1;
	size_t temp = 0;
	for (auto itr = funcNames.begin(); itr != funcNames.end(); itr++) {
		if ((*itr) == value) {
			index = temp;
			break;
		}
		++temp;
	}
	return index;
}

void HookOn(size_t index) {
	DWORD dwTemp = 0;
	DWORD dwOldProtect;
	FARPROC pOldFunc = pOldFuncs.at(index);
	vector<BYTE> vNewCode = NewCodes.at(index);
	for (SIZE_T i = 0; i < memSize; i++) {
		NewCode[i] = vNewCode[i];
	}

	//将内存保护模式改为可写,老模式保存入dwOldProtect
	VirtualProtectEx(hProcess, pOldFunc, memSize, PAGE_READWRITE, &dwOldProtect);
	//将所属进程中add的前5个字节改为Jmp Myadd 
	WriteProcessMemory(hProcess, pOldFunc, NewCode, memSize, 0);
	//将内存保护模式改回为dwOldProtect
	VirtualProtectEx(hProcess, pOldFunc, memSize, dwOldProtect, &dwTemp);
}

void HookOn(string szFuncName) {
	size_t index = indexFind(szFuncName);
	if (index == -1) {
		return;
	}
	HookOn(index);
}

void HookOn(){

	for (size_t i = 0; i < funcNames.size(); i++) {
		HookOn(i);
	}

	bHook = true;
}

void HookOff(size_t index) {
	DWORD dwTemp = 0;
	DWORD dwOldProtect;
	FARPROC pOldFunc = pOldFuncs.at(index);
	vector<BYTE> vOldCode = OldCodes.at(index);
	for (SIZE_T i = 0; i < memSize; i++) {
		OldCode[i] = vOldCode[i];
	}

	VirtualProtectEx(hProcess, pOldFunc, memSize, PAGE_READWRITE, &dwOldProtect);
	WriteProcessMemory(hProcess, pOldFunc, OldCode, memSize, 0);
	VirtualProtectEx(hProcess, pOldFunc, memSize, dwOldProtect, &dwTemp);
}

void HookOff(string szFuncName) {
	size_t index = indexFind(szFuncName);
	if (index == -1) {
		return;
	}
	HookOff(index);
}

void HookOff(){//将所属进程中add()的入口代码恢复

	for (size_t i = 0; i < funcNames.size(); i++) {
		HookOff(i);
	}
	
	bHook = false;
}

void WriteToFile(const char *str) {
	ofstream ofile;
	ofile.open(R"(E:\VS\MFC\One\One\test.txt)", ofstream::out | ofstream::app);
	ofile << str << "\n";
	ofile.close();
}

void addInjectInfo(wstring szDllName, string szFuncName, void *szNewFunc) {
	dllNames.push_back(szDllName);
	funcNames.push_back(szFuncName);
	newFuncs.push_back(szNewFunc);
}

void Inject(){

	if (m_bInjected == false){
		m_bInjected = true;

		HMODULE hmod = NULL;
		for (size_t i = 0; i < dllNames.size(); i++) {
			hmod = GetModuleHandle(dllNames.at(i).c_str());
			if (hmod) {
				char c[1024];
				sprintf_s(c, 1024, "module already loaded: %p", hmod);
				WriteToFile(c);
			}
			else {
				char c[1024];
				sprintf_s(c, 1024, "module has been not loaded: %d", GetLastError());
				WriteToFile(c);
			}
			FARPROC pOldFunc = ::GetProcAddress(hmod, funcNames.at(i).c_str());

			if (pOldFunc == NULL) {
				WriteToFile("cannot locate func");
				return;
			}
			else {
				char c[1024];
				sprintf_s(c, 1024, "func exists: %p", pOldFunc);
				WriteToFile(c);
			}

			pOldFuncs.push_back(pOldFunc);
			
			void *NewFunc = newFuncs.at(i);
#ifdef _WIN64
			NewCode[0] = 0x48;
			NewCode[1] = 0xB8;
			NewCode[10] = 0x50;
			NewCode[11] = 0xC3;
			long long temp = (long long)NewFunc;
			int offset = 2;
			SIZE_T size = 8;
#else
			NewCode[0] = 0xe9;
			DWORD temp = (DWORD)NewFunc - (DWORD)pOldFunc - 5;
			int offset = 1;
			SIZE_T size = 4;
#endif // _WIN64
			RtlMoveMemory(OldCode, pOldFunc, memSize);
			RtlMoveMemory(NewCode + offset, &temp, size);
			
			vector<BYTE> vOldCode;
			vector<BYTE> vNewCode;
			vOldCode.resize(memSize);
			vNewCode.resize(memSize);
			for (SIZE_T i = 0; i < memSize; i++) {
				vOldCode[i] = OldCode[i];
				vNewCode[i] = NewCode[i];
			}

			OldCodes.push_back(vOldCode);
			NewCodes.push_back(vNewCode);
		}
		HookOn();

	}
}

bool checkInjectPid(DWORD pid) {
	if (pid == injectPid) {
		return true;
	}
	return false;
}
