#include "BangoYap/BangoYapModule.h"

#include "BangoYapCommands.h"
#include "BangoYap/BangoYapSettings.h"

void FBangoYapModule::StartupModule()
{
	BangoYap::RegisterCommands();
	BangoYap::RegisterDefines();
}

void FBangoYapModule::ShutdownModule()
{
	// Bango commands can't be removed; defines can.
	BangoYap::UnregisterDefines();
}

IMPLEMENT_MODULE(FBangoYapModule, BangoYap)
