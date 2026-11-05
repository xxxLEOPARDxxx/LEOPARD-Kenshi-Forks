#include "gui/requested_settings.h"
#include <cstring>

RequestedSettingsSnapshot::RequestedSettingsSnapshot()
	: modules_(NULL), moduleCount_(0), ready_(false)
{
	std::memset(&values_, 0, sizeof(values_));
	std::memset(keyCount_, 0, sizeof(keyCount_));
}

static size_t RequestedWidth(const ConfigKey& k)
{
	switch (k.kind)
	{
	case CK_BOOL: return sizeof(bool);
	case CK_INT: return sizeof(int);
	case CK_FLOAT: return sizeof(float);
	case CK_DOUBLE: return sizeof(double);
	default: return k.size;
	}
}

bool RequestedSettingsSnapshot::Capture(const ConfigModule* modules, int count)
{
	ready_ = false;
	modules_ = NULL;
	moduleCount_ = 0;
	std::memset(&values_, 0, sizeof(values_));
	std::memset(keyCount_, 0, sizeof(keyCount_));
	if (!modules || count <= 0 || count > CONFIG_MODULE_MAX) return false;
	// Preflight every bound before StageModule can read any value.
	for (int m = 0; m < count; ++m)
	{
		const ConfigModule& mod = modules[m];
		if (!mod.keys || mod.stateSize > CONFIG_STATE_MAX || (!mod.state && mod.stateSize)) return false;
		int i = 0;
		for (; i < CONFIG_STAGE_MAX && mod.keys[i].name; ++i)
		{
			const ConfigKey& k = mod.keys[i];
			if (k.retired) continue;
			if (k.kind < CK_BOOL || k.kind > CK_CUSTOM) return false;
			if (!k.target && mod.state)
			{
				const size_t width = RequestedWidth(k);
				if (!width || k.offset > mod.stateSize || width > mod.stateSize - k.offset) return false;
			}
		}
		if (i == CONFIG_STAGE_MAX) return false;
		keyCount_[m] = i;
	}
	for (int m = 0; m < count; ++m) StageModule(modules[m], &values_.module[m]);
	modules_ = modules;
	moduleCount_ = count;
	ready_ = true;
	return true;
}

bool RequestedSettingsSnapshot::Copy(SettingsStaging* out) const
{
	if (!ready_ || !out) return false;
	*out = values_;
	return true;
}

bool RequestedSettingsSnapshot::Key(int module, int key, const ConfigModule** m, const ConfigKey** k) const
{
	if (m) *m = NULL;
	if (k) *k = NULL;
	if (!ready_ || !m || !k || module < 0 || module >= moduleCount_ || key < 0 || key >= keyCount_[module]) return false;
	const ConfigKey& row = modules_[module].keys[key];
	if (row.retired) return false;
	*m = &modules_[module];
	*k = &row;
	return true;
}

bool RequestedSettingsSnapshot::CopyModule(int module, ConfigModuleStage* out) const
{
	if (!ready_ || !out || module < 0 || module >= moduleCount_) return false;
	*out = values_.module[module];return true;
}
bool RequestedSettingsSnapshot::Publish(const SettingsStaging& values)
{
	if (!ready_) return false;values_ = values;return true;
}

static RequestedSettingsSnapshot& RequestedStorage()
{
	static RequestedSettingsSnapshot snapshot;
	return snapshot;
}
bool CaptureRequestedSettings() { return RequestedStorage().Capture(kConfigModules, kConfigModuleCount); }
const RequestedSettingsSnapshot& RequestedConfig() { return RequestedStorage(); }
bool PublishRequestedSettings(const SettingsStaging& values) { return RequestedStorage().Publish(values); }
