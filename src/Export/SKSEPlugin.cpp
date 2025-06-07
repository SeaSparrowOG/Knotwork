#include "StringUtils.h"

#include <SimpleIni.h>

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []()
	{
		SKSE::PluginVersionData v{};

		v.PluginVersion(Plugin::VERSION);
		v.PluginName(Plugin::NAME);
		v.AuthorName("SeaSparrow"sv);
		v.UsesAddressLibrary();
		v.UsesUpdatedStructs();

		return v;
	}();

extern "C" DLLEXPORT bool SKSEAPI
SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = Plugin::VERSION[0];
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION[0];

	if (a_skse->IsEditor()) {
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_6_1130) {
		return false;
	}

	return true;
}

static bool PatchQuests()
{
	auto then = std::chrono::steady_clock::now();
	auto* dh = RE::TESDataHandler::GetSingleton();

	std::string jsonFolder = fmt::format(R"(.\Data\SKSE\Plugins\{})"sv, Plugin::NAME);
	logger::info("  >Settings folder: {}."sv, jsonFolder);
	if (!std::filesystem::exists(jsonFolder)) {
		logger::info("    >No settings folder found."sv);
		return true;
	}

	std::vector<std::string> paths{};
	try {
		for (const auto& entry : std::filesystem::directory_iterator(jsonFolder)) {
			if (entry.is_regular_file() && entry.path().extension() == ".ini") {
				paths.push_back(entry.path().string());
			}
		}

		std::sort(paths.begin(), paths.end());
		logger::info("    >Found {} configuration files."sv, std::to_string(paths.size()));

		size_t totalPatches = 0;
		for (const auto& iniPath : paths) {
			auto configName = iniPath.substr(jsonFolder.size() + 1, iniPath.size() - 1);
			logger::info("    >Reading config: {}"sv, configName);

			CSimpleIniA ini;
			ini.SetUnicode();
			ini.LoadFile(iniPath.c_str());

			std::list<CSimpleIniA::Entry> modNames = std::list<CSimpleIniA::Entry>();
			ini.GetAllSections(modNames);

			for (auto& modName : modNames) {
				auto* name = modName.pItem;
				std::list<CSimpleIniA::Entry> entries = std::list<CSimpleIniA::Entry>();
				ini.GetAllKeys(name, entries);
				if (!dh->LookupModByName(name)) {
					logger::info("        >Mod {} not found, skipping."sv, name);
					continue;
				}
				logger::info("        >Found {} entries for {}"sv, std::to_string(entries.size()), name);

				for (auto& id : entries) {
					try {
						auto rawFormID = StringUtils::is_only_hex(id.pItem, true) ? StringUtils::to_num<RE::FormID>(id.pItem, true) : 0;
						auto* rawForm = dh->LookupForm(rawFormID, name);
						if (!rawForm) {
							logger::warn("          >Could not find form {} in mod {}. This may be normal."sv, id.pItem, name);
							continue;
						}
						else if (rawForm->GetFormType() != RE::FormType::Quest) {
							logger::warn("          >Form {} is not a quest."sv, id.pItem);
							continue;
						}
						auto* questForm = rawForm->As<RE::TESQuest>();
						if (!questForm) {
							logger::warn("          >Failed to cast form {} as a quest, though it appears to be one."sv, id.pItem);
							continue;
						}

						questForm->data.questType = static_cast<RE::QUEST_DATA::Type>(ini.GetLongValue(name, id.pItem));
						logger::info("          >Patching quest {} ({}) to type {}"sv, questForm->GetFormID(), questForm->GetName(), std::to_string(questForm->data.questType.underlying()));
						++totalPatches;
					}
					catch (std::exception& e) {
						logger::warn("          >Config {} failed at {} with error: {}"sv, configName, id.pItem, e.what());
						return false;
					}
				}
			}
		}
		logger::info("    >Patching complete, {} quests patched."sv, std::to_string(totalPatches));
	}
	catch (std::exception& e) {
		logger::warn("    >Failed to read settings folder: {}"sv, e.what());
		return false;
	}

	const auto& questArray = dh->GetFormArray<RE::TESQuest>();

	for (auto* quest : questArray) {
		if (quest->data.questType.underlying() == 0) {
			quest->data.questType = static_cast<RE::QUEST_DATA::Type>(8);
		}
	}

	auto now = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now - then).count();
	logger::info("    >Patching time: {}ns."sv, std::to_string(duration));
	return true;
}

static void MessageEventCallback(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		if (!PatchQuests()) {
			SKSE::stl::report_and_fail("Failed to patch quests. Please refer to the log in Documents/My Games/Skyrim Special Edition/SKSE/Knotwork for more information."sv);
		}
		logger::info("Finished startup tasks, enjoy your game!"sv);
		SECTION_SEPARATOR;
		break;
	default:
		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	SECTION_SEPARATOR;
	logger::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());
	logger::info("Author: SeaSparrow"sv);
	SECTION_SEPARATOR;
	SKSE::Init(a_skse);

	logger::info("Performing startup tasks..."sv);

	const auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(&MessageEventCallback);

	return true;
}