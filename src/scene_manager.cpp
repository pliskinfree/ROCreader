#include "scene_manager.h"

AppScene SceneManager::Current() const { return current_; }
AppScene SceneManager::SettingsReturnScene() const { return settings_return_scene_; }
AppScene &SceneManager::CurrentRef() { return current_; }

void SceneManager::Set(AppScene scene) { current_ = scene; }
void SceneManager::SetSettingsReturnScene(AppScene scene) { settings_return_scene_ = scene; }

void SceneManager::OpenSettingsFrom(AppScene return_scene) {
  settings_return_scene_ = return_scene;
  current_ = AppScene::Settings;
}

void SceneManager::ReturnFromSettings() {
  current_ = settings_return_scene_;
}

void SceneManager::EnterShelf() { current_ = AppScene::Shelf; }
void SceneManager::EnterReader() { current_ = AppScene::Reader; }

bool SceneManager::Is(AppScene scene) const { return current_ == scene; }
