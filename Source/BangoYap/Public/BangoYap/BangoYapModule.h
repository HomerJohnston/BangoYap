#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** The BangoYap runtime module. Registers BangoYap's commands with Bango at startup. */
class BANGOYAP_API FBangoYapModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
