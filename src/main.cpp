#include <Geode/Geode.hpp>
#include <Geode/Utils.hpp>
#include <dbghelp.h>
#include <iostream>
#include <psapi.h>
#include <windows.h>
using namespace geode::prelude;
#include "../api/main.hpp"

#define POP_PREV_LINE_STREAM(ss, outVar) \
    do { \
        std::deque<std::string> _lines; \
        std::string _line; \
        while (std::getline(ss, _line)) { \
            _lines.push_back(_line); \
        } \
        if (!_lines.empty()) { \
            outVar = _lines.back(); \
            _lines.pop_back(); \
        } else { \
            outVar = ""; \
        } \
        (ss).str(""); \
        (ss).clear(); \
        for (const auto& l : _lines) { \
            (ss) << l << "\n"; \
        } \
    } while(0)
	
namespace Crash {
inline std::unordered_map<geode::Mod *, ExtraCrashData::ModData *> beforeGeodeLoader;
inline std::unordered_map<geode::Mod *, ExtraCrashData::ModData *> afterGeodeLoader;

inline std::unordered_map<geode::Mod *, ExtraCrashData::ModData *> beforeModRequest;
inline std::unordered_map<geode::Mod *, ExtraCrashData::ModData *> onModRequest;
inline std::unordered_map<geode::Mod *, ExtraCrashData::ModData *> After;
} // namespace Crash

using printGeodeInfo_t = void (*)(std::stringstream &);
printGeodeInfo_t printGeodeInfoOriginal = nullptr;

void vprintGeodeInfoH(std::stringstream &stream) {
	if (Crash::beforeGeodeLoader.size() > 0) {
		std::string lastLine;
		POP_PREV_LINE_STREAM(stream, lastLine);
		std::vector<geode::Mod*> mods;
		for (auto &pair : Crash::beforeGeodeLoader) {
			mods.push_back(pair.first);
		}

		std::sort(mods.begin(), mods.end(), [](geode::Mod* a, geode::Mod* b) {
			const auto &s1 = a->getID();
			const auto &s2 = b->getID();
			return std::lexicographical_compare(
				s1.begin(), s1.end(),
				s2.begin(), s2.end(),
				[](unsigned char c1, unsigned char c2) { return std::tolower(c1) < std::tolower(c2); }
			);
		});
		for (auto mod : mods) {
			stream << fmt::format("== From {} == \n{}\n", mod->getID(), Crash::beforeGeodeLoader[mod]->OnCrash());
		}
		stream << "\n" << lastLine << "\n";
	}
    if (printGeodeInfoOriginal) {
        printGeodeInfoOriginal(stream);
    };
	if (Crash::afterGeodeLoader.size() > 0) {
		std::vector<geode::Mod*> mods;
		for (auto &pair : Crash::afterGeodeLoader) {
			mods.push_back(pair.first);
		}

		std::sort(mods.begin(), mods.end(), [](geode::Mod* a, geode::Mod* b) {
			const auto &s1 = a->getID();
			const auto &s2 = b->getID();
			return std::lexicographical_compare(
				s1.begin(), s1.end(),
				s2.begin(), s2.end(),
				[](unsigned char c1, unsigned char c2) { return std::tolower(c1) < std::tolower(c2); }
			);
		});
		stream << "\n";
		for (auto mod : mods) {
			stream << fmt::format("== From {} == \n{}\n", mod->getID(), Crash::afterGeodeLoader[mod]->OnCrash());
		}
	}
}

void vcrashlog_H(std::stringstream &stream) {
	auto mods = Loader::get()->getAllMods();
	if (mods.empty()) {
		stream << "<None>\n";
	}

	if (Crash::beforeModRequest.size() > 0) {
		std::string lastLine;
		POP_PREV_LINE_STREAM(stream, lastLine);
		std::vector<geode::Mod*> mods;
		for (auto &pair : Crash::beforeModRequest) {
			mods.push_back(pair.first);
		}

		std::sort(mods.begin(), mods.end(), [](geode::Mod* a, geode::Mod* b) {
			const auto &s1 = a->getID();
			const auto &s2 = b->getID();
			return std::lexicographical_compare(
				s1.begin(), s1.end(),
				s2.begin(), s2.end(),	
				[](unsigned char c1, unsigned char c2) { return std::tolower(c1) < std::tolower(c2); }
			);
		});
		for (auto mod : mods) {
			stream << fmt::format("== From {} == \n{}\n", mod->getID(), Crash::beforeModRequest[mod]->OnCrash());
		}
		stream << "\n" << lastLine << "\n";
	}
	
	std::sort(mods.begin(), mods.end(), [](Mod *a, Mod *b) {
		auto const s1 = a->getID();
		auto const s2 = b->getID();
		return std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), [](auto a, auto b) {
			return std::tolower(a) < std::tolower(b);
		});
	});

	using namespace std::string_view_literals;
	std::stringstream extraStream(std::string(""));
	for (auto &mod : mods) {
		std::string Extra = "";
		if (ExtraCrashData::ModData *data = Crash::onModRequest[mod]) {
			Extra = data->OnCrash();
		};
		if (ExtraCrashData::ModData *data = Crash::After[mod]) {
			extraStream << fmt::format("== From {} == \n{}\n", mod->getID(), data->OnCrash());
		};
		stream << fmt::format("{} | [{}] {} {}\n",
		                      mod->isCurrentlyLoading() ? "o"sv : mod->isEnabled()     ? "x"sv
		                                                      : mod->hasLoadProblems() ? "!"sv
		                                                                               : // thank you for this bug report
		                                                          mod->targetsOutdatedVersion() ? "*"sv
		                                                                                        : // thank you very much for this bug report
		                                                          mod->shouldLoad() ? "~"sv
		                                                                            : " "sv,
		                      mod->getVersion().toVString(), mod->getID(), Extra);
	}
	stream << "\n"
	       << extraStream.str();
}

using SymInitialize_t = BOOL(WINAPI *)(HANDLE, PCSTR, BOOL);
using SymSetOptions_t = DWORD(WINAPI *)(DWORD);
using SymFromName_t = BOOL(WINAPI *)(HANDLE, PCSTR, struct _SYMBOL_INFO *);

void SetupPrintmodsHook() {
	HMODULE hDbgHelp = LoadLibraryA("Dbghelp.dll");
	if (!hDbgHelp) {
		log::error("Failed to load Dbghelp.dll, System failed to load mod!");
		return;
	}

	auto SymInitialize = (SymInitialize_t)GetProcAddress(hDbgHelp, "SymInitialize");
	auto SymSetOptions = (SymSetOptions_t)GetProcAddress(hDbgHelp, "SymSetOptions");
	auto SymFromName = (SymFromName_t)GetProcAddress(hDbgHelp, "SymFromName");

	if (!SymInitialize || !SymSetOptions || !SymFromName) {
		log::error("Failed to get DbgHelp functions");
		FreeLibrary(hDbgHelp);
		return;
	}

	HANDLE process = GetCurrentProcess();
	SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
	if (!SymInitialize(process, nullptr, TRUE)) {
		log::error("SymInitialize failed with error {}", GetLastError());
		FreeLibrary(hDbgHelp);
		return;
	}

	SYMBOL_INFO *symbol = (SYMBOL_INFO *)calloc(1, sizeof(SYMBOL_INFO) + 256);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	if (SymFromName(process, "crashlog::printMods", symbol)) {
		auto wrap = Mod::get()->hook(
		    reinterpret_cast<void *>(symbol->Address),
		    &vcrashlog_H,
		    "crashlog::printMods");
		log::error("Hooked crashlog::printMods at {:#x}, result: {}", symbol->Address, wrap.isOk());
	} else {
		log::error("Failed to resolve crashlog::printMods (err {})", GetLastError());
	}
	symbol = (SYMBOL_INFO *)calloc(1, sizeof(SYMBOL_INFO) + 256);
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	if (SymFromName(process, "crashlog::printGeodeInfo", symbol)) {
		printGeodeInfoOriginal = reinterpret_cast<printGeodeInfo_t>(symbol->Address);
		auto wrap = Mod::get()->hook(
		    reinterpret_cast<void *>(symbol->Address),
		    &vprintGeodeInfoH,
		    "crashlog::printGeodeInfo");
		log::error("Hooked crashlog::printGeodeInfo at {:#x}, result: {}", symbol->Address, wrap.isOk());
	} else {
		log::error("Failed to resolve crashlog::printGeodeInfo (err {})", GetLastError());
	}

	free(symbol);
	using SymCleanup_t = BOOL(WINAPI *)(HANDLE);
	auto SymCleanup = (SymCleanup_t)GetProcAddress(hDbgHelp, "SymCleanup");
	if (SymCleanup)
		SymCleanup(process);

	FreeLibrary(hDbgHelp);
}

$on_mod(Loaded) {
	SetupPrintmodsHook();

	new EventListener<EventFilter<ExtraCrashData::CrashlogEvent>>(+[](ExtraCrashData::CrashlogEvent *event) {
		//log::debug("event added: {} {} {}", event->sender, event->m_self->OnCrash(), (int)event->m_type);
		switch (event->m_type) {
			case ExtraCrashData::Message::WithMod:
				Crash::onModRequest[event->sender] = event->m_self;
				break;
			case ExtraCrashData::Message::AfterMod:
				Crash::After[event->sender] = event->m_self;
				break;
			case ExtraCrashData::Message::BeforeMod:
				Crash::beforeModRequest[event->sender] = event->m_self;
				break;
			case ExtraCrashData::Message::AfterGeodeInfo:
				Crash::afterGeodeLoader[event->sender] = event->m_self;
				break;
			case ExtraCrashData::Message::BeforeGeodeInfo:
				Crash::beforeGeodeLoader[event->sender] = event->m_self;
				break;
		}
		return ListenerResult::Stop;
	});
	#ifdef __EXTRACRASHDATA_API_TEST__
		Loader::get()->queueInMainThread([] {
			ExtraCrashData::CrashlogEvent(
				[]{return "test string using lambda";}, 
				ExtraCrashData::Message::BeforeMod).post();
		});
	#endif
};
