#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <float.h>
#include "BetterLootingPresets.h"

namespace BetterLootingPresets
{
    namespace
    {
        const char* const Group = "BetterLootingPresets";
        const char* const Missing = "\001";

        struct Field
        {
            const char* section;
            const char* key;
            const char* fallback;
        };

        // Fallbacks match the runtime INI loader, including older installs
        // that have never written all keys through the settings menu.
        const Field Fields[] = {
            { "General", "LootRadius", "20.0" },
            { "Rules", "MinValue", "500" },
            { "Rules", "MinValuePerKg", "300.0" },
            { "Rules", "RequiredItems", "" },
            { "Rules", "AllowStolen", "0" },
            { "Behavior", "PickupGroundItems", "0" },
            { "Behavior", "BestItemsFirst", "1" },
            { "Behavior", "PreferBackpack", "1" },
            { "Behavior", "DepositByRules", "0" },
            { "Categories", "Weapons", "1" },
            { "Categories", "Armour", "1" },
            { "Categories", "Materials", "1" },
            { "Categories", "Food", "1" },
            { "Categories", "Medicine", "1" },
            { "Categories", "Tools", "1" },
            { "Categories", "Ammo", "1" },
            { "Categories", "Blueprints", "1" },
            { "Categories", "Books", "1" },
            { "Categories", "Robotics", "1" },
            { "Categories", "Narcotics", "0" },
            { "Categories", "SeveredLimbs", "0" },
            { "Categories", "Other", "1" }
        };
        const size_t FieldCount = sizeof(Fields) / sizeof(Fields[0]);

        struct Values
        {
            std::string fields[FieldCount];
        };

        std::string SlotSection(int slot)
        {
            char text[24];
            sprintf_s(text, "BetterLootingPreset%d", slot + 1);
            return text;
        }

        int ActiveSlot(const char* path)
        {
            char value[3];
            const DWORD length = GetPrivateProfileStringA(Group, "Active", "", value, sizeof(value), path);
            return length == 1 && value[0] >= '1' && value[0] < '1' + SlotCount ? value[0] - '1' : -1;
        }

        enum SlotFormat { Absent, Supported, Unsupported };

        SlotFormat ReadFormat(const char* path, const char* section)
        {
            char format[3];
            const DWORD length = GetPrivateProfileStringA(section, "Format", Missing, format, sizeof(format), path);
            if (length == 1 && format[0] == Missing[0])
                return Absent;
            return length == 1 && format[0] == '1' ? Supported : Unsupported;
        }

        bool ReadValues(const char* path, const char* slotSection, Values& result)
        {
            char buffer[8193];
            for (size_t i = 0; i < FieldCount; ++i)
            {
                const Field& field = Fields[i];
                const char* section = slotSection != NULL ? slotSection : field.section;
                const char* fallback = slotSection != NULL ? Missing : field.fallback;
                const DWORD length = GetPrivateProfileStringA(section, field.key, fallback, buffer,
                    sizeof(buffer), path);
                if (length > 8191 || (slotSection != NULL && length == 1 && buffer[0] == Missing[0]))
                    return false;
                result.fields[i].assign(buffer, length);
            }
            return true;
        }

        float SettingNumber(const std::string& text, float minValue, float maxValue, float fallback)
        {
            const float value = static_cast<float>(std::atof(text.c_str()));
            if (!_finite(value))
                return fallback;
            return value < minValue ? minValue : (value > maxValue ? maxValue : value);
        }

        void NormalizeMenuValues(Values& values)
        {
            // Match MCM's control precision so reopening a loaded preset
            // cannot mark it modified merely by formatting a numeric value.
            char text[64];
            sprintf_s(text, "%.1f", SettingNumber(values.fields[0], 1, 100, 20));
            values.fields[0] = text;
            sprintf_s(text, "%d", static_cast<int>(std::floor(SettingNumber(values.fields[1], 0, 5000, 500) + 0.5f)));
            values.fields[1] = text;
            sprintf_s(text, "%.1f", SettingNumber(values.fields[2], 0, 2000, 300));
            values.fields[2] = text;
            for (size_t i = 4; i < FieldCount; ++i)
                values.fields[i] = std::atoi(values.fields[i].c_str()) != 0 ||
                    _stricmp(values.fields[i].c_str(), "true") == 0 ? "1" : "0";
        }

        bool SameValues(const Values& left, const Values& right)
        {
            for (size_t i = 0; i < FieldCount; ++i)
                if (left.fields[i] != right.fields[i])
                    return false;
            return true;
        }

        bool ReadPresetValues(const char* path, const char* section, Values& values)
        {
            if (!ReadValues(path, section, values))
                return false;
            // Format 1 only writes canonical numeric and toggle values.
            // Reject damaged fields before they can change working rules.
            Values normalized = values;
            NormalizeMenuValues(normalized);
            return SameValues(values, normalized);
        }

        bool WriteValue(const char* path, const char* section, const char* key, const std::string& value)
        {
            // Profile reads remove the matching outer quotes. Keeping our own
            // wrapper preserves quotes that are part of the original value.
            const std::string quoted = "\"" + value + "\"";
            return WritePrivateProfileStringA(section, key, quoted.c_str(), path) != FALSE;
        }

        bool WriteValues(const char* path, const char* slotSection, const Values& values)
        {
            for (size_t i = 0; i < FieldCount; ++i)
            {
                const Field& field = Fields[i];
                if (!WriteValue(path, slotSection != NULL ? slotSection : field.section, field.key, values.fields[i]))
                    return false;
            }
            return true;
        }

        // Apply affects multiple INI sections. Stage every edit in a sibling
        // file so an error cannot leave a half-applied loadout.
        class IniEdit
        {
        public:
            explicit IniEdit(const std::string& path) : ready(false)
            {
                temporary[0] = '\0';
                char full[MAX_PATH];
                char* filename = NULL;
                const DWORD length = GetFullPathNameA(path.c_str(), MAX_PATH, full, &filename);
                if (length == 0 || length >= MAX_PATH || filename == NULL)
                    return;
                destination = full;
                *filename = '\0';
                if (GetTempFileNameA(full, "BLP", 0, temporary) == 0)
                {
                    temporary[0] = '\0';
                    return;
                }
                if (CopyFileA(destination.c_str(), temporary, FALSE))
                {
                    ready = true;
                    // File attributes are copied too. A read-only INI must
                    // remain untouched, but our scratch file must be removable.
                    SetFileAttributesA(temporary, FILE_ATTRIBUTE_NORMAL);
                    if ((GetFileAttributesA(destination.c_str()) & FILE_ATTRIBUTE_READONLY) != 0)
                        ready = false;
                }
                else
                {
                    ready = GetLastError() == ERROR_FILE_NOT_FOUND;
                }
            }

            ~IniEdit()
            {
                if (temporary[0] != '\0')
                    DeleteFileA(temporary);
            }

            const char* Path() const { return ready ? temporary : NULL; }

            bool Commit()
            {
                WritePrivateProfileStringA(NULL, NULL, NULL, temporary);
                WritePrivateProfileStringA(NULL, NULL, NULL, destination.c_str());
                if (!MoveFileExA(temporary, destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                    return false;
                temporary[0] = '\0';
                return true;
            }

        private:
            char temporary[MAX_PATH];
            std::string destination;
            bool ready;
        };
    }

    State ReadState(const std::string& path)
    {
        const char* ini = path.c_str();
        State state;
        state.active = ActiveSlot(ini);
        state.modified = false;
        for (int i = 0; i < SlotCount; ++i)
        {
            const std::string section = SlotSection(i);
            Slot& slot = state.slots[i];
            const SlotFormat format = ReadFormat(ini, section.c_str());
            slot.occupied = format != Absent;
            Values saved;
            slot.valid = format == Supported && ReadPresetValues(ini, section.c_str(), saved);
            if (slot.occupied)
            {
                char name[192];
                GetPrivateProfileStringA(section.c_str(), "Name", "", name, sizeof(name), ini);
                slot.name = name;
            }
            if (state.active == i)
            {
                if (!slot.valid)
                    state.active = -1;
                else
                {
                    Values current;
                    state.modified = !ReadValues(ini, NULL, current);
                    if (!state.modified)
                    {
                        NormalizeMenuValues(current);
                        state.modified = !SameValues(current, saved);
                    }
                }
            }
        }
        return state;
    }

    Result Execute(const std::string& path, int slot, Operation operation, const std::string& name)
    {
        if (slot < 0 || slot >= SlotCount)
            return InvalidSlot;
        const std::string section = SlotSection(slot);
        const char* slotSection = section.c_str();
        const char* ini = path.c_str();
        Values values;
        if (operation == Save)
        {
            if (name.size() > 160 || name.find_first_of("\r\n") != std::string::npos ||
                name.find('\0') != std::string::npos || !ReadValues(ini, NULL, values))
                return ValueTooLong;
        }
        else
        {
            const SlotFormat format = ReadFormat(ini, slotSection);
            if (format == Absent ||
                (operation == Apply && (format != Supported || !ReadPresetValues(ini, slotSection, values))))
                return EmptySlot;
        }

        if (operation == Save)
            NormalizeMenuValues(values);
        IniEdit edit(path);
        const char* staging = edit.Path();
        if (staging == NULL)
            return IoError;

        bool written = false;
        switch (operation)
        {
        case Save:
            written = WriteValues(staging, slotSection, values) &&
                WriteValue(staging, slotSection, "Name", name) &&
                WritePrivateProfileStringA(slotSection, "Format", "1", staging);
            break;
        case Apply:
            char active[8];
            sprintf_s(active, "%d", slot + 1);
            written = WriteValues(staging, NULL, values) &&
                WritePrivateProfileStringA(Group, "Active", active, staging);
            break;
        case Clear:
            written = WritePrivateProfileStringA(slotSection, NULL, NULL, staging) &&
                (ActiveSlot(ini) != slot || WritePrivateProfileStringA(Group, "Active", NULL, staging));
            break;
        }
        return written && edit.Commit() ? Ok : IoError;
    }
}
