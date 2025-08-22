#pragma once
#include <Geode/Geode.hpp>
#include <string>
using namespace geode::prelude;

#ifdef GEODE_IS_ANDROID
#include <dlfcn.h>
#endif

#ifdef GEODE_WINDOWS
#include <dbghelp.h>
#include <psapi.h>
#include <windows.h>
using SymInitialize_t = BOOL(WINAPI *)(HANDLE, PCSTR, BOOL);
using SymSetOptions_t = DWORD(WINAPI *)(DWORD);
using SymFromName_t = BOOL(WINAPI *)(HANDLE, PCSTR, struct _SYMBOL_INFO *);
struct hDbgHelp_fun {
    HMODULE self;
	SymInitialize_t SymInitialize;
	SymSetOptions_t SymSetOptions;
	SymFromName_t SymFromName;
};

#endif

class GeodeHooker {
  public:
	static GeodeHooker *spawn() {
		static GeodeHooker *Object = new GeodeHooker();
		return Object;
	};
	bool m_working = false;
	std::string m_errorMSG = "";

    #ifdef GEODE_WINDOWS
        HANDLE m_process = nullptr;
        hDbgHelp_fun m_functions = hDbgHelp_fun();
        unsigned long long Get(PCSTR address) {
            SYMBOL_INFO *symbol = (SYMBOL_INFO *)calloc(1, sizeof(SYMBOL_INFO) + 256);
            symbol->MaxNameLen = 255;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            if (m_functions.SymFromName(m_process, address, symbol)) {
                unsigned long long Address = symbol->Address;
                free(symbol);
                return Address;
            } else {
                free(symbol);
                return 0;
            };
        };

        void LoadMain() {
            m_functions.self = LoadLibraryA("Dbghelp.dll");
            if (!m_functions.self) {
                m_errorMSG = "Failed to load Dbghelp.dll, System missing dll?";
                return;
            }

            m_functions.SymInitialize = (SymInitialize_t)GetProcAddress(m_functions.self, "SymInitialize");
            m_functions.SymSetOptions = (SymSetOptions_t)GetProcAddress(m_functions.self, "SymSetOptions");
            m_functions.SymFromName = (SymFromName_t)GetProcAddress(m_functions.self, "SymFromName");

            if (!m_functions.SymInitialize || !m_functions.SymSetOptions || !m_functions.SymFromName) {
                m_errorMSG = "Failed to get DbgHelp functions";
                FreeLibrary(m_functions.self);
                return;
            }

            m_process = GetCurrentProcess();
            m_functions.SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
            if (!m_functions.SymInitialize(m_process, nullptr, TRUE)) {
                m_errorMSG = fmt::format("SymInitialize failed with error {}", GetLastError());
                FreeLibrary(m_functions.self);
                return;
            }
            m_working = true;
        };
        ~GeodeHooker() {
            using SymCleanup_t = BOOL(WINAPI *)(HANDLE);
            auto SymCleanup = (SymCleanup_t)GetProcAddress(m_functions.self, "SymCleanup");
            if (SymCleanup)
                SymCleanup(m_process);

            FreeLibrary(m_functions.self);
        }
    #endif
    #ifdef GEODE_IS_ANDROID
        void* m_process = nullptr;
        void LoadMain() {
            void* handle = dlopen(nullptr, RTLD_NOW);
            m_working = true;
            return;
        };
        unsigned long long Get(const char* address) {
            void* ptr = dlsym(m_process, address);
            if (!ptr) {
                return 0;
            }
            return reinterpret_cast<unsigned long long>(ptr);
        }
    #endif
	GeodeHooker() {
        LoadMain();
	};
};