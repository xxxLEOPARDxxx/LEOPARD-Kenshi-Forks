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

    OnOptionsWindowInitForModHub();
    if (ShouldUseHubUiFromModHub())
    {
        return;
    }

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
        "   Enable sleeping bag dismantle",
        g_sleepingBagDismantleEnabled,
        tabID);
    if (sleepingBagToggleLine && self->tooltip)
    {
        sleepingBagToggleLine->setTooltip(
            "Allow hotkey dismantle for sleeping bags and compatible medical variants. Occupied bags are always protected.",
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
        DebugLog("Hotkey action: CRASH AVERTED in CheckInternalBuildingsSafely");
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
        DebugLog("Hotkey action: CRASH AVERTED in CheckMountedBuildingsSafely");
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

static bool IsBaseSleepingBagText(const std::string& value)
{
    return ContainsAsciiInsensitive(value, "sleeping bag")
        || ContainsAsciiInsensitive(value, "sleeping-bag")
        || ContainsAsciiInsensitive(value, "sleeping_bag")
        || ContainsAsciiInsensitive(value, "sleepingbag")
        || ContainsAsciiInsensitive(value, "camp bed")
        || ContainsAsciiInsensitive(value, "camp_bed")
        || ContainsAsciiInsensitive(value, "campbed")
        || ContainsAsciiInsensitive(value, "bedroll");
}

static bool IsMedicalSleepingBagText(const std::string& value)
{
    const bool hasMedical = ContainsAsciiInsensitive(value, "medical");
    const bool hasSleepBagHint = ContainsAsciiInsensitive(value, "sleep")
        || ContainsAsciiInsensitive(value, "bag")
        || ContainsAsciiInsensitive(value, "bedroll")
        || ContainsAsciiInsensitive(value, "camp");

    if (hasMedical && hasSleepBagHint)
    {
        return true;
    }

    // Compatibility IDs used by some sleeping-bag mods.
    return ContainsAsciiInsensitive(value, "medicalbed")
        || ContainsAsciiInsensitive(value, "medical_bed")
        || ContainsAsciiInsensitive(value, "advancedmedicalbed")
        || ContainsAsciiInsensitive(value, "advanced_medical_bed");
}

static bool IsSleepingBagGameData(const GameData* data)
{
    if (!data)
    {
        return false;
    }

    const std::string& name = data->name;
    const std::string& stringId = data->stringID;

    return IsBaseSleepingBagText(name) || IsBaseSleepingBagText(stringId);
}

static bool IsOutsideFurnitureSafely(Building* b)
{
    bool isOutsideFurniture = false;
    __try
    {
        isOutsideFurniture = b->getIsOutsideFurniture();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey action: CRASH AVERTED in IsOutsideFurnitureSafely");
        isOutsideFurniture = false;
    }

    return isOutsideFurniture;
}

static bool IsSleepingBagBuilding(Building* b)
{
    if (!b)
    {
        return false;
    }

    if (b->getSpecialFunction() != BF_BED)
    {
        return false;
    }

    const GameData* data = b->getGameData();
    if (IsSleepingBagGameData(data))
    {
        return true;
    }

    if (!data)
    {
        return false;
    }

    const bool isMedicalSleepingBag = IsMedicalSleepingBagText(data->name) || IsMedicalSleepingBagText(data->stringID);
    if (!isMedicalSleepingBag)
    {
        return false;
    }

    // Keep medical-bed matching scoped to camp-style (outside) furniture.
    return IsOutsideFurnitureSafely(b);
}

static bool CheckSleepingBagOccupiedSafely(const hand& sleepingBagHandle)
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

                if (character->inWhat == sleepingBagHandle)
                {
                    occupied = true;
                    break;
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog("Hotkey action: CRASH AVERTED in CheckSleepingBagOccupiedSafely");
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
        DebugLog("Hotkey action: CRASH AVERTED during dismantle!");
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
    const bool isSleepingBagTarget = (g_sleepingBagDismantleEnabled && IsSleepingBagBuilding(b));
    if (!isWallTarget && !isSleepingBagTarget)
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

    return CheckSleepingBagOccupiedSafely(b->getHandle());
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
        return;
    }

    if (IsDismantleBlockedForTarget(b, isWallTarget))
    {
        return;
    }

    if (IsFailedDismantleCooldownActive())
    {
        return;
    }

    __try
    {
        Building::ConstructionState* buildState = b->getBuildState();
        if (buildState && !buildState->isComplete)
        {
            return;
        }

        const bool dismantleResult = SafelyDismantleTarget(b, sel);
        if (!dismantleResult)
        {
            DebugLog(isWallTarget
                ? "Hotkey action: Dismantle failed - wall may be connected to problematic structures"
                : "Hotkey action: Dismantle failed - sleeping bag may be in an invalid state");
            g_lastFailedDismantleTime = GetTickCount();
            return;
        }

        g_lastFailedDismantleTime = 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        DebugLog(isWallTarget
            ? "Hotkey action: CRASH AVERTED in outer dismantle wrapper!"
            : "Hotkey action: CRASH AVERTED while dismantling sleeping bag");
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

    Building* b = 0;
    bool isWallTarget = false;
    if (TryGetSelectedDismantleTarget(sel, &b, &isWallTarget))
    {
        TryDismantleSelectedBuilding(b, sel, isWallTarget);
        return;
    }

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
