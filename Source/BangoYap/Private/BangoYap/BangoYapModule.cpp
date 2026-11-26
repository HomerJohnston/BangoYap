#include "BangoYap/BangoYapModule.h"

#include "BangoYapCommands.h"

void FBangoYapModule::StartupModule()
{
	BangoYap::RegisterCommands();
}

void FBangoYapModule::ShutdownModule()
{
	// Bango commands can't be removed.
}

IMPLEMENT_MODULE(FBangoYapModule, BangoYap)
