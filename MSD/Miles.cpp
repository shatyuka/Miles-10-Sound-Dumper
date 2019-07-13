#include "stdafx.h"
#include "Miles.h"
#include <winreg.h>
#include <iostream>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

void SetupBusVolumes(Driver driver)
{
	MilesBusSetVolumeLevel(MilesProjectGetBus(driver, "Master"), 0.7f);
	MilesBusSetVolumeLevel(MilesProjectGetBus(driver, "Voice_Comm_bus"), 0.7f);
	MilesBusSetVolumeLevel(MilesProjectGetBus(driver, "MUSIC"), 0.7f);
}

std::vector<std::string> GetEventNames(Bank bank)
{
	std::vector<std::string> collectedNames;
	for (int i = 0; i < MilesBankGetEventCount(bank); i++) {
		std::stringstream line;
		line << i << "," << MilesBankGetEventName(bank, i) << std::endl;
		collectedNames.push_back(line.str());
	}

	return collectedNames;
}

bool GetMatchingFile(std::regex reg, std::string* out)
{
	for (const auto& entry : fs::directory_iterator(fs::path("./audio/ship/")))
	{
		if (std::regex_search(entry.path().string(), reg))
		{
			if (out) 
			{
				*out = entry.path().string();
			}
			return true;
		}
	}
	return false;
}

bool GetLocalizedLanguage(std::string* out)
{
	auto reg = std::regex("general_(\\w*).mstr");
	for (const auto& entry : fs::directory_iterator(fs::path("./audio/ship/")))
	{
		std::smatch languageMatch;
		std::string path = entry.path().string();
		if (std::regex_search(path, languageMatch, reg))
		{
			*out = languageMatch[1].str();
			return true;
		}
	}
	return false;
}

bool IsPatched()
{
	return GetMatchingFile(std::regex("_patch_"), 0);
}


Project SetupMiles(void (WINAPI* callback)(int, char*), bool silent)
{ 
	Project project;
	int startup_parameters = 0;

	MilesSetStartupParameters(&startup_parameters);
	MilesAllocTrack(2);
	__int64 startup = MilesStartup(callback);
	if (!silent)
	{
		std::cout << "Start up: " << startup << "\r\n";
	}

	auto output = MilesOutputDirectSound();

	unk a1;
	a1.sound_function = (long long*)output;
	a1.endpointID = NULL; // Default audio Device 
	a1.maybe_sample_rate = 48000;
	a1.channel_count = 2;
	project.driver = MilesDriverCreate((long long*)& a1);

	MilesDriverRegisterBinkAudio(project.driver);
	MilesEventSetStreamingCacheLimit(project.driver, 0x4000000);
	MilesDriverSetMasterVolume(project.driver, 1);
	project.queue = MilesQueueCreate(project.driver);
	MilesEventInfoQueueEnable(project.driver);

	std::string mprj;
	std::string language;
	GetMatchingFile(std::regex("audio.mprj$"), &mprj);
	GetLocalizedLanguage(&language);

	MilesProjectLoad(project.driver, mprj.c_str(), language.c_str(), "audio");

	__int64 status = MilesProjectGetStatus(project.driver);
	while (status == 0) {
		Sleep(500);
		status = MilesProjectGetStatus(project.driver);
	}
	status = MilesProjectGetStatus(project.driver);
	if (!silent) 
	{ 
		std::cout << "status: " << MilesProjectStatusToString(status) << std::endl; 
	}

	std::string mbnk;
	std::string general;
	std::string localized;
	GetMatchingFile(std::regex("general\\.mbnk$"), &mbnk);
	GetMatchingFile(std::regex("general_stream\\.mstr"), &general);
	GetMatchingFile(std::regex("general_\\w*\\.mstr"), &localized);
	project.bank = MilesBankLoad(project.driver, mbnk.c_str(), general.c_str(), localized.c_str(), 0);

	if (IsPatched()) {
		std::string general_patch;
		std::string localized_patch;
		GetMatchingFile(std::regex("general_stream_patch_\\d\\.mstr"), &general_patch);
		GetMatchingFile(std::regex("general_\\w*_patch_\\d\\.mstr"), &localized_patch);
		MilesBankPatch(project.bank, general_patch.c_str(), localized_patch.c_str());
	}

	int bank_status = MilesBankGetStatus(project.bank, 0);
	while (bank_status == 0) {
		bank_status = MilesBankGetStatus(project.bank, 0);
	}
	if (!silent) 
	{ 
		std::cout << "bank_status: " << MilesBankStatusToString(bank_status) << std::endl;
	}

	return project;
}

void StopPlaying(Queue queue) 
{
	MilesQueueEventRun(queue, "STOPNOW");
	MilesQueueSubmit(queue);
}