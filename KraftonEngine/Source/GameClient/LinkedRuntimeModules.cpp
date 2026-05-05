#include "GameClient/LinkedRuntimeModules.h"

#if IS_GAME_CLIENT
#include "Games/Crossy/CrossyGameModule.h"
#endif

void RegisterLinkedRuntimeModules()
{
#if IS_GAME_CLIENT
	RegisterCrossyGameModule();
#endif
}
