# Crashlog Api
an api used to add more crashlog data


an example is provided here
```cpp
#include <Geode/Geode.hpp>

// where the api is, which can be installed like this
//#include "../Crashlog__api/main.hpp" // for example main.hpp is in Crashlog__api folder
// or
#include <cosmella.extra_crash_data/api/main.hpp> // if you have required it in the mod.json

using namespace geode::prelude;
$on_mod(Loaded) {
	ExtraCrashData::waitforMod([=] {
		ExtraCrashData::CrashlogEvent(
		    [] { return "la mod is complete"; },
		    ExtraCrashData::Message::BeforeMod)
		    .post();
	});
};
``

This is where she makes a mod.
