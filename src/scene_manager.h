#pragma once

enum class AppScene { Boot, Shelf, Settings, Reader };

class SceneManager {
public:
  AppScene Current() const;
  AppScene SettingsReturnScene() const;
  AppScene &CurrentRef();

  void Set(AppScene scene);
  void SetSettingsReturnScene(AppScene scene);
  void OpenSettingsFrom(AppScene return_scene);
  void ReturnFromSettings();
  void EnterShelf();
  void EnterReader();

  bool Is(AppScene scene) const;

private:
  AppScene current_ = AppScene::Boot;
  AppScene settings_return_scene_ = AppScene::Shelf;
};
