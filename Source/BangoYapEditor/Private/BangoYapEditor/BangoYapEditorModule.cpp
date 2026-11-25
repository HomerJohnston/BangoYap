#include "Modules/ModuleManager.h"

/** The BangoYap editor module. */
class FBangoYapEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FBangoYapEditorModule, BangoYapEditor)
