#pragma once
#include "gui/settings_rows.h"

// UI/startup thread only. Owned preferences, never the post-refusal runtime
// configuration. No save/apply, translation, native lookup or file operations.
// Metadata tables must have static lifetime, as kConfigModules does.
class RequestedSettingsSnapshot
{
	SettingsStaging values_;
	const ConfigModule* modules_;
	int moduleCount_, keyCount_[CONFIG_MODULE_MAX];
	bool ready_;
public:
	RequestedSettingsSnapshot();
	bool Capture(const ConfigModule* modules, int count);
	bool Ready() const { return ready_; }
	bool Copy(SettingsStaging* out) const;
	bool CopyModule(int module, ConfigModuleStage* out) const;
	// Main-thread publish after the caller's validation and successful atomic save.
	bool Publish(const SettingsStaging& values);
	bool Key(int module, int key, const ConfigModule** m, const ConfigKey** k) const;
};

// Called once per LoadConfig path, after parsing/clamping and before runtime
// routing overrides or hook failures. Future callbacks read owned copies.
bool CaptureRequestedSettings();
const RequestedSettingsSnapshot& RequestedConfig();
bool PublishRequestedSettings(const SettingsStaging& values);
