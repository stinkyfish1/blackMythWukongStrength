#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <chrono>
#include <thread>


std::string WStringToString(const wchar_t* wstr) {
    int bufferSize = WideCharToMultiByte(CP_ACP, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    std::string str(bufferSize, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr, -1, &str[0], bufferSize, nullptr, nullptr);
    return str;
}


DWORD GetProcId(const wchar_t* procName) {
    DWORD procId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 procEntry;
    procEntry.dwSize = sizeof(procEntry);

    if (Process32First(hSnap, &procEntry)) {
        do {
            if (strcmp(procEntry.szExeFile, WStringToString(procName).c_str()) == 0) {
                procId = procEntry.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnap, &procEntry));
    }
    CloseHandle(hSnap);
    return procId;
}

uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName) {
    uintptr_t modBaseAddr = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
    MODULEENTRY32 modEntry;
    modEntry.dwSize = sizeof(modEntry);

    if (Module32First(hSnap, &modEntry)) {
        do {
            if (strcmp(modEntry.szModule, WStringToString(modName).c_str()) == 0) {
                modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
                break;
            }
        } while (Module32Next(hSnap, &modEntry));
    }
    CloseHandle(hSnap);
    return modBaseAddr;
}


uintptr_t FindDMAAddy(HANDLE hProc, uintptr_t ptr, const std::vector<unsigned int>& offsets) {
    uintptr_t addr = ptr;
    for (unsigned int i = 0; i < offsets.size(); ++i) {
        ReadProcessMemory(hProc, (BYTE*)addr, &addr, sizeof(addr), 0);
        addr += offsets[i];
    }
    return addr;
}

int main() {

    std::cout << R"(
                 _   _ ___ _  _______        __
                | \ | |_ _| |/ /_ _\ \      / /
                |  \| || || ' / | | \ \ /\ / / 
                | |\  || || . \ | |  \ V  V /  
                |_| \_|___|_|\_\___|  \_/\_/   
    )" << std::endl;

    DWORD procId = GetProcId(L"b1-Win64-Shipping.exe"); 
    if (procId == 0) {
        std::cout << "Game not running!" << std::endl;
        return 1;
    }

    // Open the process with all access
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);
    if (hProcess == NULL) {
        std::cout << "Failed to open process." << std::endl;
        return 1;
    }

    bool printed = false; // Flag to ensure message is printed only once

    while (true) {
        // Recalculate module base address and strength address in each iteration
        uintptr_t moduleBase = GetModuleBaseAddress(procId, L"b1-Win64-Shipping.exe");
        uintptr_t baseAddress = 0x1D909380; 
        std::vector<unsigned int> offsets = { 0x298, 0x4E8, 0x20, 0x98, 0x48, 0x60, 0x284 };
        uintptr_t strengthAddress = FindDMAAddy(hProcess, moduleBase + baseAddress, offsets);

        float strengthValue;  // Reading as float
        if (ReadProcessMemory(hProcess, (BYTE*)strengthAddress, &strengthValue, sizeof(strengthValue), nullptr)) {
            if (!printed) {
                std::cout << "Current Strength (float): " << strengthValue << std::endl;
                printed = true; // Set flag to true after printing
            }

            float newStrengthValue = 250.0f; 
            if (WriteProcessMemory(hProcess, (BYTE*)strengthAddress, &newStrengthValue, sizeof(newStrengthValue), nullptr)) {
                std::cout << "New Strength Value Set to: " << newStrengthValue << std::endl;
            } else {
                std::cout << "Failed to write new strength value." << std::endl;
            }
        } else {
            std::cout << "Failed to read current strength value." << std::endl;
        }


        // Sleep for a short duration before updating again
        std::this_thread::sleep_for(std::chrono::seconds(1)); 
    }

    CloseHandle(hProcess);
    return 0;
}
