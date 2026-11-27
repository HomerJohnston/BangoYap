#include "BangoNodeDecorator.h"
#include "BangoYapBlockDecorator.h"
#include "BangoYapEditorStyle.h"
#include "BangoYapLineDecorator.h"
#include "Modules/ModuleManager.h"

/**
 * The BangoYap editor module: draws BangoYap's commands in the Bango graph editor like Yap's Flow dialogue node
 * (node decorators; BangoYap Graph Nodes Plan, in the project).
 */
class FBangoYapEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FBangoYapEditorStyle::Initialize();

		const TSharedRef<FBangoYapLineDecorator> LineDecorator = MakeShared<FBangoYapLineDecorator>();
		for (const FString& Command : FBangoYapLineDecorator::GetCommandNames())
		{
			Bango::RegisterNodeDecorator(Command, LineDecorator);
		}

		const TSharedRef<FBangoYapBlockDecorator> BlockDecorator = MakeShared<FBangoYapBlockDecorator>();
		for (const FString& Command : FBangoYapBlockDecorator::GetCommandNames())
		{
			Bango::RegisterNodeDecorator(Command, BlockDecorator);
		}
	}

	virtual void ShutdownModule() override
	{
		// Before this module's code is gone: the graph editor would otherwise keep decorators it can't call.
		if (FModuleManager::Get().IsModuleLoaded(TEXT("BangoScriptsScriptEditor")))
		{
			for (const FString& Command : FBangoYapLineDecorator::GetCommandNames())
			{
				Bango::UnregisterNodeDecorator(Command);
			}
			for (const FString& Command : FBangoYapBlockDecorator::GetCommandNames())
			{
				Bango::UnregisterNodeDecorator(Command);
			}
		}

		FBangoYapEditorStyle::Shutdown();
	}
};

IMPLEMENT_MODULE(FBangoYapEditorModule, BangoYapEditor)
