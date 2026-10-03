static DataPanelLine* TryCreateNativeKeybindRow(
    DatapanelGUI* panel,
    int tabID,
    std::string* bindingPtr)
{
    if (!g_fnCreateKeyConfigLine || !panel || !bindingPtr)
    {
        return 0;
    }

    const int candidateTabIds[3] = { tabID, 6, 1 };
    NativeStringObj nativeLabel;
    NativeStringObj nativeBinding;
    InitNativeStringObj(&nativeLabel, kHotkeyNativeLabel);
    InitNativeStringObj(&nativeBinding, *bindingPtr);
    for (int i = 0; i < 3; ++i)
    {
        const int nativeTabId = candidateTabIds[i];
        DataPanelLine* keyLine = 0;

        if (g_fnCreateKeyConfigLineWrapper)
        {
            __try
            {
                keyLine = g_fnCreateKeyConfigLineWrapper(
                    panel,
                    &nativeLabel,
                    &nativeBinding,
                    nativeTabId);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                keyLine = 0;
            }
            if (keyLine)
            {
                return keyLine;
            }
        }

        // Keep native/legacy probes disabled here: they have incompatible signatures
        // and crash immediately; wrapper path is the closest verified game call shape.
    }

    return 0;
}

static void CreateHotkeyModifierOptionLines(DatapanelGUI* panel, int tabID, ToolTip* tooltip)
{
    if (panel == 0 || g_fnCreateCheckboxLine == 0)
    {
        return;
    }

    DataPanelLine_CheckBox* requireCtrlLine = g_fnCreateCheckboxLine(panel, "   Require Ctrl", g_hotkeyRequireCtrl, tabID);
    if (requireCtrlLine && tooltip)
    {
        requireCtrlLine->setTooltip("Require Ctrl to be held with the dismantle hotkey.", tooltip);
    }

    DataPanelLine_CheckBox* requireShiftLine = g_fnCreateCheckboxLine(panel, "   Require Shift", g_hotkeyRequireShift, tabID);
    if (requireShiftLine && tooltip)
    {
        requireShiftLine->setTooltip("Require Shift to be held with the dismantle hotkey.", tooltip);
    }

    DataPanelLine_CheckBox* requireAltLine = g_fnCreateCheckboxLine(panel, "   Require Alt", g_hotkeyRequireAlt, tabID);
    if (requireAltLine && tooltip)
    {
        requireAltLine->setTooltip("Require Alt to be held with the dismantle hotkey.", tooltip);
    }
}

static void OptionsWindowInitHook(OptionsWindow* self)
{
    if (g_fnOptionsInitOrig)
    {
        g_fnOptionsInitOrig(self);
    }

    if (!self || !self->optionsTab || !g_ptrKenshiGUI || !g_fnCreateDatapanel || !g_fnCreateCheckboxLine)
    {
        return;
    }

    // Настройки Wall-B-Gone - во вкладке MCM (McmModHubBridge.h). Своя
    // запасная вкладка не нужна: она вставала последней и забирала кнопки
    // RE_Kenshi, которые тот вешает на последнюю вкладку окна.
    return;

    if (self->optionsTab->findItemWith(kWallBGoneTabName))
    {
        return;
    }

    MyGUI::TabItem* pluginOptionTab = self->optionsTab->addItem(kWallBGoneTabName);
    if (!pluginOptionTab)
    {
        ErrorLog("Wall-B-Gone: failed to add Wall-B-Gone tab item");
        return;
    }

    DatapanelGUI* pluginOptionPanel = g_fnCreateDatapanel(g_ptrKenshiGUI, kWallBGonePanelName, pluginOptionTab, false);
    if (!pluginOptionPanel)
    {
        ErrorLog("Wall-B-Gone: failed to create wall_b_gone_options datapanel");
        return;
    }

    const int tabID = kWallBGonePanelLineId;
    pluginOptionPanel->vfunc0xc0(tabID);
    pluginOptionPanel->vfunc0xe0(25.0f);

    if (g_fnCreateHeaderLine)
    {
        g_fnCreateHeaderLine(pluginOptionPanel, "[Wall-B-Gone Settings]", "", tabID, false, true);
    }

    DataPanelLine_CheckBox* toggleLine = g_fnCreateCheckboxLine(pluginOptionPanel, "   Enable Wall-B-Gone", g_modEnabled, tabID);
    if (toggleLine && self->tooltip)
    {
        toggleLine->setTooltip("Enable or disable Wall-B-Gone hotkey behavior.", self->tooltip);
    }

    DataPanelLine_CheckBox* sleepingBagToggleLine = g_fnCreateCheckboxLine(
        pluginOptionPanel,
        "   Enable bed and furniture dismantle",
        g_sleepingBagDismantleEnabled,
        tabID);
    if (sleepingBagToggleLine && self->tooltip)
    {
        sleepingBagToggleLine->setTooltip(
            "Allow hotkey dismantle for beds and common furniture. Beds are always protected while occupied.",
            self->tooltip);
    }

    CreateHotkeyModifierOptionLines(pluginOptionPanel, tabID, self->tooltip);

    g_hotkeyRebindButton = 0;
    g_hotkeyResetButton = 0;
    g_hotkeyLabelWidget = 0;
    g_nativeHotkeyBindingActive = false;

    MyGUI::Widget* panelWidget = pluginOptionPanel->getWidget();

    if (g_fnCreateKeyConfigLine)
    {
        SyncNativeBindingFromHotkey();
        DataPanelLine* keyLine = TryCreateNativeKeybindRow(
            pluginOptionPanel,
            tabID,
            &g_hotkeyNativeBinding);

        if (keyLine)
        {
            g_nativeHotkeyBindingActive = true;
            if (self->tooltip)
            {
                keyLine->setTooltip(
                    "Click and press the primary key for Wall-B-Gone. Use the Require Ctrl/Shift/Alt toggles above to build combos.",
                    self->tooltip);
            }
        }
        else if (!keyLine)
        {
            CreateFallbackKeybindControls(panelWidget);
        }
    }
    else
    {
        CreateFallbackKeybindControls(panelWidget);
    }

    pluginOptionTab->setVisible(false);
    self->optionsTab->setItemData(pluginOptionTab, pluginOptionPanel);
}

static void OptionsWindowSaveHook(OptionsWindow* self)
{
    if (g_fnOptionsSaveOrig)
    {
        g_fnOptionsSaveOrig(self);
    }

    if (g_nativeHotkeyBindingActive)
    {
        OIS::KeyCode parsedKey = OIS::KC_UNASSIGNED;
        bool parsedCtrl = false;
        bool parsedShift = false;
        bool parsedAlt = false;
        if (TryParseHotkeyBinding(g_hotkeyNativeBinding, &parsedKey, &parsedCtrl, &parsedShift, &parsedAlt))
        {
            std::string reason;
            if (ValidateHotkey(parsedKey, &reason) == HotkeyValidation_Ok)
            {
                g_hotkeyPrimary = parsedKey;
                g_pendingHotkeyPrimary = parsedKey;
                g_hotkeyRequireCtrl = parsedCtrl;
                g_hotkeyRequireShift = parsedShift;
                g_hotkeyRequireAlt = parsedAlt;
                SyncNativeBindingFromHotkey();
            }
            else
            {
                ErrorLog("Wall-B-Gone: native keybind value rejected by validation; keeping previous key");
                SyncNativeBindingFromHotkey();
            }
        }
        else
        {
            ErrorLog("Wall-B-Gone: native keybind value parse failed; keeping previous key");
            SyncNativeBindingFromHotkey();
        }
    }
    else
    {
        // Fallback capture path updates g_hotkeyPrimary directly.
        g_pendingHotkeyPrimary = g_hotkeyPrimary;
        SyncNativeBindingFromHotkey();
    }

    SaveConfigState();
}

// Helper function to check internal buildings safely (SEH wrapper)
// Returns true if there are internal buildings or if an error occurs
static bool CheckInternalBuildingsSafely(Building* b)
{
    bool hasInternal = false;
    __try
    {
        if (b->getNumInternalBuildings() > 0)
        {
            hasInternal = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        WallBGoneDebugLog("Hotkey action: CRASH AVERTED in CheckInternalBuildingsSafely");
        hasInternal = true;
    }
    return hasInternal;
}

static bool UnsafeCheckMounted(Building* b)
{
    lektor<Building*> mountedBuildings;
    int numMounted = b->getMountedBuildings(&mountedBuildings);
    return (numMounted > 0);
}

static bool CheckMountedBuildingsSafely(Building* b)
{
    bool hasMounted = false;
    __try
    {
        if (UnsafeCheckMounted(b))
        {
            hasMounted = true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        WallBGoneDebugLog("Hotkey action: CRASH AVERTED in CheckMountedBuildingsSafely");
        hasMounted = true;
    }
    return hasMounted;
}

static char ToLowerAsciiChar(char value)
{
    if (value >= 'A' && value <= 'Z')
    {
        return static_cast<char>(value + ('a' - 'A'));
    }

    return value;
}

static bool ContainsAsciiInsensitive(const std::string& haystack, const char* needle)
{
    if (!needle || needle[0] == '\0')
    {
        return false;
    }

    const size_t needleLen = std::strlen(needle);
    if (needleLen > haystack.size())
    {
        return false;
    }

    for (size_t start = 0; start + needleLen <= haystack.size(); ++start)
    {
        size_t i = 0;
        for (; i < needleLen; ++i)
        {
            if (ToLowerAsciiChar(haystack[start + i]) != ToLowerAsciiChar(needle[i]))
            {
                break;
            }
        }

        if (i == needleLen)
        {
            return true;
        }
    }

    return false;
}

static bool IsAsciiCompactSeparator(char value)
{
    return value == ' ' || value == '-' || value == '_' || value == '\t';
}

static bool EqualsAsciiInsensitiveCompact(const std::string& value, const char* needle)
{
    if (!needle || needle[0] == '\0')
    {
        return false;
    }

    size_t valueIndex = 0;
    size_t needleIndex = 0;

    while (valueIndex < value.size() && IsAsciiCompactSeparator(value[valueIndex]))
    {
        ++valueIndex;
    }

    while (needle[needleIndex] != '\0' && IsAsciiCompactSeparator(needle[needleIndex]))
    {
        ++needleIndex;
    }

    while (valueIndex < value.size() && needle[needleIndex] != '\0')
    {
        if (IsAsciiCompactSeparator(value[valueIndex]))
        {
            ++valueIndex;
            continue;
        }

        if (IsAsciiCompactSeparator(needle[needleIndex]))
        {
            ++needleIndex;
            continue;
        }

        if (ToLowerAsciiChar(value[valueIndex]) != ToLowerAsciiChar(needle[needleIndex]))
        {
            return false;
        }

        ++valueIndex;
        ++needleIndex;
    }

    while (valueIndex < value.size() && IsAsciiCompactSeparator(value[valueIndex]))
    {
        ++valueIndex;
    }

    while (needle[needleIndex] != '\0' && IsAsciiCompactSeparator(needle[needleIndex]))
    {
        ++needleIndex;
    }

    return valueIndex == value.size() && needle[needleIndex] == '\0';
}

static bool IsFurnitureText(const std::string& value)
{
    return EqualsAsciiInsensitiveCompact(value, "bench")
        || EqualsAsciiInsensitiveCompact(value, "chair")
        || EqualsAsciiInsensitiveCompact(value, "stool")
        || EqualsAsciiInsensitiveCompact(value, "table")
        || EqualsAsciiInsensitiveCompact(value, "throne")
        || EqualsAsciiInsensitiveCompact(value, "sittingbox")
        || EqualsAsciiInsensitiveCompact(value, "sittingpillow")
        || EqualsAsciiInsensitiveCompact(value, "smalltable")
        || EqualsAsciiInsensitiveCompact(value, "metaltable")
        || EqualsAsciiInsensitiveCompact(value, "roundtable")
        || EqualsAsciiInsensitiveCompact(value, "roundbartable");
}

static bool IsBedBuilding(Building* b)
{
    if (!b)
    {
        return false;
    }

    return b->getSpecialFunction() == BF_BED;
}

static bool IsSupportedBedOrFurnitureBuilding(Building* b)
{
    if (!b)
    {
        return false;
    }

    const int specialFunction = b->getSpecialFunction();
    if (specialFunction == BF_BED || specialFunction == BF_CHAIR || specialFunction == BF_THRONE)
    {
        return true;
    }

    const GameData* data = b->getGameData();
    if (!data)
    {
        return false;
    }

    return IsFurnitureText(data->name) || IsFurnitureText(data->stringID);
}

static bool CheckBedOccupiedSafely(const hand& bedHandle)
{
    bool occupied = false;
    __try
    {
        if (!ou)
        {
            occupied = true;
        }
        else
        {
            const ogre_unordered_set<Character*>::type& characters = ou->getCharacterUpdateList();
            for (ogre_unordered_set<Character*>::type::const_iterator it = characters.begin(); it != characters.end(); ++it)
            {
                Character* character = *it;
                if (!character)
                {
                    continue;
                }

                if (character->inSomething != IN_BED)
                {
                    continue;
                }

                if (character->inWhat == bedHandle)
                {
                    occupied = true;
                    break;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        WallBGoneDebugLog("Hotkey action: CRASH AVERTED in CheckBedOccupiedSafely");
        occupied = true;
    }

    return occupied;
}

static bool PerformDismantleLogic(Building* b, const hand& sel)
{
    b->dropMats();
    ou->dynamicDestroyBuilding(sel);
    return true;
}

static bool SafelyDismantleTarget(Building* b, const hand& sel)
{
    bool success = false;
    __try
    {
        success = PerformDismantleLogic(b, sel);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        WallBGoneDebugLog("Hotkey action: CRASH AVERTED during dismantle!");
        success = false;
    }
    return success;
}

static bool IsConfiguredHotkeyPressedThisFrame()
{
    bool hotkeyDown = false;
    bool modifiersSatisfied = false;
    if (key->keyboard)
    {
        hotkeyDown = key->keyboard->isKeyDown(g_hotkeyPrimary);
        modifiersSatisfied = true;

        if (g_hotkeyRequireCtrl && !IsModifierDown(key->keyboard, OIS::KC_LCONTROL))
        {
            modifiersSatisfied = false;
        }

        if (g_hotkeyRequireShift && !IsModifierDown(key->keyboard, OIS::KC_LSHIFT))
        {
            modifiersSatisfied = false;
        }

        if (g_hotkeyRequireAlt && !IsModifierDown(key->keyboard, OIS::KC_LMENU))
        {
            modifiersSatisfied = false;
        }
    }

    const bool hotkeyActive = hotkeyDown && modifiersSatisfied;
    const bool pressedThisFrame = (hotkeyActive && !g_prevHotkeyDown);
    g_prevHotkeyDown = hotkeyActive;
    return pressedThisFrame;
}

static bool TryGetSelectedDismantleTarget(const hand& sel, Building** buildingOut, bool* isWallTargetOut)
{
    if (!buildingOut || !isWallTargetOut)
    {
        return false;
    }

    Building* b = sel.getBuilding();
    if (!b)
    {
        return false;
    }

    const bool isWallTarget = (b->isAWall() != 0);
    const bool isSupportedTarget = (g_sleepingBagDismantleEnabled && IsSupportedBedOrFurnitureBuilding(b));
    if (!isWallTarget && !isSupportedTarget)
    {
        return false;
    }

    *buildingOut = b;
    *isWallTargetOut = isWallTarget;
    return true;
}

static bool IsDismantleBlockedForTarget(Building* b, bool isWallTarget)
{
    if (isWallTarget)
    {
        return CheckInternalBuildingsSafely(b) || CheckMountedBuildingsSafely(b);
    }

    if (IsBedBuilding(b))
    {
        return CheckBedOccupiedSafely(b->getHandle());
    }

    return false;
}

static bool IsFailedDismantleCooldownActive()
{
    if (g_lastFailedDismantleTime == 0)
    {
        return false;
    }

    const DWORD timeSinceLastFailure = GetTickCount() - g_lastFailedDismantleTime;
    return timeSinceLastFailure < DISMANTLE_COOLDOWN_MS;
}

static void TryDismantleSelectedBuilding(Building* b, const hand& sel, bool isWallTarget)
{
    if (!b->canDismantle())
    {
        WallBGoneDebugLog("Hotkey action: skipped - the game says this building cannot be dismantled");
        return;
    }

    if (IsDismantleBlockedForTarget(b, isWallTarget))
    {
        WallBGoneDebugLog(isWallTarget
            ? "Hotkey action: skipped - something is built on or inside this wall"
            : "Hotkey action: skipped - the bed is occupied");
        return;
    }

    if (IsFailedDismantleCooldownActive())
    {
        WallBGoneDebugLog("Hotkey action: skipped - cooldown after a failed dismantle");
        return;
    }

    __try
    {
        Building::ConstructionState* buildState = b->getBuildState();
        if (buildState && !buildState->isComplete)
        {
            WallBGoneDebugLog("Hotkey action: skipped - construction is not complete");
            return;
        }

        const bool dismantleResult = SafelyDismantleTarget(b, sel);
        if (!dismantleResult)
        {
            WallBGoneDebugLog(isWallTarget
            ? "Hotkey action: Dismantle failed - wall may be connected to problematic structures"
            : "Hotkey action: Dismantle failed - bed or furniture may be in an invalid state");
            g_lastFailedDismantleTime = GetTickCount();
            return;
        }

        g_lastFailedDismantleTime = 0;
        WallBGoneDebugLog("Hotkey action: dismantled");
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        WallBGoneDebugLog(isWallTarget
            ? "Hotkey action: CRASH AVERTED in outer dismantle wrapper!"
            : "Hotkey action: CRASH AVERTED while dismantling bed or furniture");
        g_lastFailedDismantleTime = GetTickCount();
    }
}

static void HandleHotkeyAction()
{
    if (g_hotkeyCaptureState == HotkeyCapture_AwaitKey)
    {
        return;
    }

    if (!g_modEnabled)
        return;

    EnsureRuntimeHotkeyValid();

    if (!key || !ou || !ou->player)
        return;

    if (!IsConfiguredHotkeyPressedThisFrame())
        return;

    const hand& sel = ou->player->selectedObject;

    if (g_debugLogging)
    {
        // Что выделено в момент нажатия: без этого «ничего не происходит»
        // не отличить от «выделено не то».
        std::ostringstream note;
        note << "Hotkey action: pressed, selected=";
        Building* const selected = sel.getBuilding();
        RootObject* const root = sel.getRootObject();
        if (selected != 0)
        {
            const GameData* const data = selected->getGameData();
            note << "building '" << (data != 0 ? data->name : std::string("?"))
                 << "' sid='" << (data != 0 ? data->stringID : std::string("?"))
                 << "' wall=" << (selected->isAWall() != 0 ? 1 : 0)
                 << " function=" << selected->getSpecialFunction()
                 << " furniture=" << (IsSupportedBedOrFurnitureBuilding(selected) ? 1 : 0);
        }
        else if (root != 0)
        {
            note << "not a building";
        }
        else
        {
            note << "nothing";
        }
        WallBGoneDebugLog(note.str().c_str());
    }

    Building* b = 0;
    bool isWallTarget = false;
    if (TryGetSelectedDismantleTarget(sel, &b, &isWallTarget))
    {
        TryDismantleSelectedBuilding(b, sel, isWallTarget);
        return;
    }
    WallBGoneDebugLog("Hotkey action: skipped - select a wall, bed, chair or table first");

    RootObject* ro = sel.getRootObject();
    if (ro)
    {
        return;
    }
}

static void (*PlayerInterface_updateUT_orig)(PlayerInterface*) = 0;
static void PlayerInterface_updateUT_hook(PlayerInterface* thisptr)
{
    PlayerInterface_updateUT_orig(thisptr);
    HandleHotkeyAction();
}

static void (*InputHandler_keyDownEvent_orig)(InputHandler*, OIS::KeyCode) = 0;
static void InputHandler_keyDownEvent_hook(InputHandler* thisptr, OIS::KeyCode keyCode)
{
    if (g_hotkeyCaptureState == HotkeyCapture_AwaitKey)
    {
        if (keyCode == OIS::KC_ESCAPE)
        {
            EndHotkeyCapture();
            RefreshHotkeyUiWidgets();
            return;
        }

        std::string validationReason;
        const HotkeyValidationResult validationResult = ValidateHotkey(keyCode, &validationReason);
        if (validationResult != HotkeyValidation_Ok)
        {
            EndHotkeyCapture();
            RefreshHotkeyUiWidgets();
            return;
        }

        g_pendingHotkeyPrimary = keyCode;
        g_hotkeyPrimary = keyCode;
        SyncNativeBindingFromHotkey();
        EndHotkeyCapture();
        RefreshHotkeyUiWidgets();
        SaveConfigState();
        return;
    }

    InputHandler_keyDownEvent_orig(thisptr, keyCode);
}
