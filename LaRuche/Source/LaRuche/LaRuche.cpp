#include "LaRuche.h"
#include "LRWorldActors.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

class FLaRucheModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		// Dans l'editeur, on s'assure que le materiau du jeu existe avant de lancer une partie.
		if (!IsRunningCommandlet())
		{
			FCoreDelegates::OnPostEngineInit.AddLambda([]() { LR::ColorMaterial(); });
		}
#endif
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FLaRucheModule, LaRuche, "LaRuche");
