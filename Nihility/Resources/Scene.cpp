#include "Scene.hpp"

std::shared_ptr<Scene> SceneManager::activeScene;

#ifdef NH_DEBUG
EngineState SceneManager::state = EngineState::Editor;
#endif