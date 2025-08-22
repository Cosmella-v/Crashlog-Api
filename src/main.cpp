#include <Geode/Geode.hpp>
#include <Geode/Utils.hpp>
#include <iostream>
using namespace geode::prelude;
#include "../api/main.hpp"
#include "GeodeFunctions.hpp"

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
			stream << fmt::format("== From {} == \n{}\n\n", mod->getID(), Crash::beforeGeodeLoader[mod]->OnCrash());
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
			stream << fmt::format("== From {} == \n{}\n\n", mod->getID(), Crash::afterGeodeLoader[mod]->OnCrash());
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
			stream << fmt::format("== From {} == \n{}\n\n", mod->getID(), Crash::beforeModRequest[mod]->OnCrash());
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
			extraStream << fmt::format("== From {} == \n{}\n\n", mod->getID(), data->OnCrash());
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

void H_crashed(std::string crashinfo) {
  	std::stringstream ss;
    ss << "Geode crashed! This is the extra info file for the developer\n";
    ss << "Please submit this with the crash report to the developer of the mod that caused it.\n";

    ss << "\n== Geode Information ==\n";
    vprintGeodeInfoH(ss);
	
	ss << "\n== Crash Information ==\n";
	ss << crashinfo;

    ss << "\n== Installed Mods ==\n";
    vcrashlog_H(ss);


    std::ofstream actualFile;
    auto const now = std::time(nullptr);
    auto const tm = *std::localtime(&now);
    std::ostringstream oss;

    oss << std::put_time(&tm, "%F_%H-%M-%S");
    actualFile.open(
       (geode::dirs::getGeodeDir() / "crashlogs") / ("Extra_" + oss.str() + ".log"), std::ios::app
    );
    actualFile << ss.rdbuf() << std::flush;
    actualFile.close();
};

void H_crashed() {
	H_crashed("UNKNOWN ERROR");
};

#ifdef GEODE_IS_ANDROID
#include <unwind.h>
#include <dlfcn.h>

inline void androidSignalHandler(int signal, siginfo_t* info, void* context) {
    std::string crashInfo = fmt::format("Android signal {} at address 0x{:X}",
        signal, reinterpret_cast<uintptr_t>(info->si_addr));

	H_crashed(crashInfo);
    std::abort();
}

inline void setupAndroidHandlers() {
    struct sigaction sa;
    sa.sa_sigaction = androidSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
}
#endif


void Hooks() {
	GeodeHooker* Hooker = GeodeHooker::spawn();

	if (!Hooker->m_working) {
		return log::error("Failed to load Hooker, Reason: {}",Hooker->m_errorMSG);
	};
	#ifdef GEODE_IS_ANDROID64
		#define _____Hook_Get(Name, NameAndroid32, NameAndroid64) \
			Hooker->Get(NameAndroid64);
	#elif defined(GEODE_IS_ANDROID32)
		#define _____Hook_Get(Name, NameAndroid32, NameAndroid64) \
			Hooker->Get(NameAndroid32);
	#else
		#define _____Hook_Get(Name, NameAndroid32, NameAndroid64) \
			Hooker->Get(Name);
	#endif

	#define Hook(outvar,Name,Mangled32,Mangled64,fun) \
	auto outvar = _____Hook_Get(Name,Mangled32,Mangled64) \
	if (outvar != 0) { \
		auto wrap = Mod::get()->hook( \
		    reinterpret_cast<void *>(outvar), \
		    &fun, \
		    Name); \
		log::error("Hooked {} at {:#x}, result: {}", Name,outvar, wrap.isOk()); \
	} else { \
		log::error("Failed to resolve {} (unable to find)",Name); \
	} 
	
	Hook(outvar_printMods,"crashlog::printMods","_ZN8crashlog9printModsERNSt6__ndk118basic_stringstreamIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE","_ZN8crashlog9printModsERNSt6__ndk118basic_stringstreamIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE", vcrashlog_H)
	Hook(outvar_printGeodeInfo,"crashlog::printGeodeInfo","_ZN8crashlog14printGeodeInfoERNSt6__ndk118basic_stringstreamIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE","_ZN8crashlog14printGeodeInfoERNSt6__ndk118basic_stringstreamIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE", vprintGeodeInfoH)

	#ifdef GEODE_IS_ANDROID
	//Hook(outvar_otherprintMods,"crashlog::printModsAndroid","_ZN8crashlog9printModsERNSt6__ndk118basic_stringstreamIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE","_ZN8crashlog9printModsERNSt6__ndk118basic_stringstreamIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE", vcrashlog_H)
	setupAndroidHandlers();
	#endif
	if (outvar_printGeodeInfo != 0) {
		printGeodeInfoOriginal = reinterpret_cast<printGeodeInfo_t>(outvar_printGeodeInfo);
	};
	
	// undef macros
	#undef _____Hook_Get
	#undef Hook

	delete Hooker;
}

#define __EXTRACRASHDATA_API_TEST__

$on_mod(Loaded) {
	Hooks();

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
