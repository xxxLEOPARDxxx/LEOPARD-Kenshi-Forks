static void SkipJsonWhitespace(const std::string& text, size_t* pos)
{
    if (!pos)
    {
        return;
    }

    while (*pos < text.size() && std::isspace(static_cast<unsigned char>(text[*pos])) != 0)
    {
        ++(*pos);
    }
}

static bool IsJsonLiteralTerminator(char c)
{
    return std::isspace(static_cast<unsigned char>(c)) != 0 || c == ',' || c == '}' || c == ']';
}

static void SkipUtf8Bom(const std::string& text, size_t* pos)
{
    if (!pos || *pos != 0 || text.size() < 3)
    {
        return;
    }

    const unsigned char b0 = static_cast<unsigned char>(text[0]);
    const unsigned char b1 = static_cast<unsigned char>(text[1]);
    const unsigned char b2 = static_cast<unsigned char>(text[2]);
    if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF)
    {
        *pos = 3;
    }
}

static bool RecordConfigSyntaxError(ConfigParseDiagnostics* diagnostics, size_t offset)
{
    if (diagnostics)
    {
        diagnostics->syntaxError = true;
        diagnostics->syntaxErrorOffset = offset;
    }
    return false;
}

static bool ParseJsonStringToken(const std::string& text, size_t* pos, std::string* valueOut)
{
    if (!pos || !valueOut)
    {
        return false;
    }

    SkipJsonWhitespace(text, pos);
    if (*pos >= text.size() || text[*pos] != '"')
    {
        return false;
    }

    ++(*pos);
    valueOut->clear();

    while (*pos < text.size())
    {
        const char c = text[*pos];
        if (c == '"')
        {
            ++(*pos);
            return true;
        }

        if (c == '\\')
        {
            ++(*pos);
            if (*pos >= text.size())
            {
                return false;
            }
            valueOut->push_back(text[*pos]);
            ++(*pos);
            continue;
        }

        valueOut->push_back(c);
        ++(*pos);
    }

    return false;
}

static bool ParseJsonBoolValue(const std::string& text, size_t* pos, bool* valueOut)
{
    if (!pos || !valueOut)
    {
        return false;
    }

    SkipJsonWhitespace(text, pos);

    if (*pos + 4 <= text.size() && text.compare(*pos, 4, "true") == 0)
    {
        const size_t end = *pos + 4;
        if (end == text.size() || IsJsonLiteralTerminator(text[end]))
        {
            *valueOut = true;
            *pos = end;
            return true;
        }
    }

    if (*pos + 5 <= text.size() && text.compare(*pos, 5, "false") == 0)
    {
        const size_t end = *pos + 5;
        if (end == text.size() || IsJsonLiteralTerminator(text[end]))
        {
            *valueOut = false;
            *pos = end;
            return true;
        }
    }

    return false;
}

static bool ParseJsonUnsignedValue(const std::string& text, size_t* pos, DWORD* valueOut, bool* clampedOut)
{
    if (!pos || !valueOut)
    {
        return false;
    }

    SkipJsonWhitespace(text, pos);
    size_t cursor = *pos;
    while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
    {
        ++cursor;
    }

    if (cursor == *pos)
    {
        return false;
    }

    if (cursor < text.size() && !IsJsonLiteralTerminator(text[cursor]))
    {
        return false;
    }

    const std::string numberText = text.substr(*pos, cursor - *pos);
    unsigned long parsed = 0;
    try
    {
        parsed = std::stoul(numberText);
    }
    catch (...)
    {
        return false;
    }

    bool clamped = false;
    if (parsed > 600000UL)
    {
        parsed = 600000UL;
        clamped = true;
    }

    *valueOut = static_cast<DWORD>(parsed);
    if (clampedOut)
    {
        *clampedOut = clamped;
    }
    *pos = cursor;
    return true;
}

static bool ParseJsonUnsignedIntValue(
    const std::string& text,
    size_t* pos,
    int maxValue,
    int* valueOut,
    bool* clampedOut)
{
    if (!pos || !valueOut || maxValue < 0)
    {
        return false;
    }

    SkipJsonWhitespace(text, pos);
    size_t cursor = *pos;
    while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
    {
        ++cursor;
    }

    if (cursor == *pos)
    {
        return false;
    }

    if (cursor < text.size() && !IsJsonLiteralTerminator(text[cursor]))
    {
        return false;
    }

    const std::string numberText = text.substr(*pos, cursor - *pos);
    unsigned long parsed = 0;
    try
    {
        parsed = std::stoul(numberText);
    }
    catch (...)
    {
        return false;
    }

    bool clamped = false;
    if (parsed > static_cast<unsigned long>(maxValue))
    {
        parsed = static_cast<unsigned long>(maxValue);
        clamped = true;
    }

    *valueOut = static_cast<int>(parsed);
    if (clampedOut)
    {
        *clampedOut = clamped;
    }
    *pos = cursor;
    return true;
}

static bool SkipJsonValue(const std::string& text, size_t* pos);

static bool SkipJsonObject(const std::string& text, size_t* pos)
{
    if (!pos || *pos >= text.size() || text[*pos] != '{')
    {
        return false;
    }

    ++(*pos);
    SkipJsonWhitespace(text, pos);
    if (*pos < text.size() && text[*pos] == '}')
    {
        ++(*pos);
        return true;
    }

    while (*pos < text.size())
    {
        std::string ignoredKey;
        if (!ParseJsonStringToken(text, pos, &ignoredKey))
        {
            return false;
        }

        SkipJsonWhitespace(text, pos);
        if (*pos >= text.size() || text[*pos] != ':')
        {
            return false;
        }

        ++(*pos);
        if (!SkipJsonValue(text, pos))
        {
            return false;
        }

        SkipJsonWhitespace(text, pos);
        if (*pos >= text.size())
        {
            return false;
        }

        if (text[*pos] == ',')
        {
            ++(*pos);
            continue;
        }

        if (text[*pos] == '}')
        {
            ++(*pos);
            return true;
        }

        return false;
    }

    return false;
}

static bool SkipJsonArray(const std::string& text, size_t* pos)
{
    if (!pos || *pos >= text.size() || text[*pos] != '[')
    {
        return false;
    }

    ++(*pos);
    SkipJsonWhitespace(text, pos);
    if (*pos < text.size() && text[*pos] == ']')
    {
        ++(*pos);
        return true;
    }

    while (*pos < text.size())
    {
        if (!SkipJsonValue(text, pos))
        {
            return false;
        }

        SkipJsonWhitespace(text, pos);
        if (*pos >= text.size())
        {
            return false;
        }

        if (text[*pos] == ',')
        {
            ++(*pos);
            continue;
        }

        if (text[*pos] == ']')
        {
            ++(*pos);
            return true;
        }

        return false;
    }

    return false;
}

static bool SkipJsonValue(const std::string& text, size_t* pos)
{
    if (!pos)
    {
        return false;
    }

    SkipJsonWhitespace(text, pos);
    if (*pos >= text.size())
    {
        return false;
    }

    const char c = text[*pos];
    if (c == '"')
    {
        std::string ignored;
        return ParseJsonStringToken(text, pos, &ignored);
    }

    if (c == '{')
    {
        return SkipJsonObject(text, pos);
    }

    if (c == '[')
    {
        return SkipJsonArray(text, pos);
    }

    if (c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0)
    {
        size_t cursor = *pos;
        if (text[cursor] == '-')
        {
            ++cursor;
        }

        bool sawDigit = false;
        while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
        {
            sawDigit = true;
            ++cursor;
        }

        if (!sawDigit)
        {
            return false;
        }

        if (cursor < text.size() && text[cursor] == '.')
        {
            ++cursor;
            bool sawFractionDigit = false;
            while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
            {
                sawFractionDigit = true;
                ++cursor;
            }
            if (!sawFractionDigit)
            {
                return false;
            }
        }

        if (cursor < text.size() && (text[cursor] == 'e' || text[cursor] == 'E'))
        {
            ++cursor;
            if (cursor < text.size() && (text[cursor] == '+' || text[cursor] == '-'))
            {
                ++cursor;
            }

            bool sawExponentDigit = false;
            while (cursor < text.size() && std::isdigit(static_cast<unsigned char>(text[cursor])) != 0)
            {
                sawExponentDigit = true;
                ++cursor;
            }
            if (!sawExponentDigit)
            {
                return false;
            }
        }

        *pos = cursor;
        return true;
    }

    if (*pos + 4 <= text.size() && text.compare(*pos, 4, "true") == 0)
    {
        *pos += 4;
        return true;
    }

    if (*pos + 5 <= text.size() && text.compare(*pos, 5, "false") == 0)
    {
        *pos += 5;
        return true;
    }

    if (*pos + 4 <= text.size() && text.compare(*pos, 4, "null") == 0)
    {
        *pos += 4;
        return true;
    }

    return false;
}

static bool ParseConfigJson(const std::string& body, PluginConfig* configOut, ConfigParseDiagnostics* diagnostics)
{
    if (!configOut || !diagnostics)
    {
        return false;
    }

    size_t pos = 0;
    SkipUtf8Bom(body, &pos);
    SkipJsonWhitespace(body, &pos);
    if (pos >= body.size() || body[pos] != '{')
    {
        return RecordConfigSyntaxError(diagnostics, pos);
    }

    ++pos;
    SkipJsonWhitespace(body, &pos);
    if (pos < body.size() && body[pos] == '}')
    {
        ++pos;
        SkipJsonWhitespace(body, &pos);
        if (pos == body.size())
        {
            return true;
        }
        return RecordConfigSyntaxError(diagnostics, pos);
    }

    while (pos < body.size())
    {
        std::string key;
        if (!ParseJsonStringToken(body, &pos, &key))
        {
            return RecordConfigSyntaxError(diagnostics, pos);
        }

        SkipJsonWhitespace(body, &pos);
        if (pos >= body.size() || body[pos] != ':')
        {
            return RecordConfigSyntaxError(diagnostics, pos);
        }
        ++pos;

        if (key == "enabled")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundEnabled = true;
                configOut->enabled = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidEnabled = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "pause_debounce_ms")
        {
            DWORD parsedUnsigned = 0;
            bool clamped = false;
            size_t valuePos = pos;
            if (ParseJsonUnsignedValue(body, &valuePos, &parsedUnsigned, &clamped))
            {
                diagnostics->foundPauseDebounceMs = true;
                diagnostics->clampedPauseDebounceMs = diagnostics->clampedPauseDebounceMs || clamped;
                configOut->pauseDebounceMs = parsedUnsigned;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidPauseDebounceMs = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "debug_log_transitions")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundDebugLogTransitions = true;
                configOut->debugLogTransitions = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidDebugLogTransitions = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "enable_delete_all_jobs_selected_member_action")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundEnableDeleteAllJobsSelectedMemberAction = true;
                configOut->enableDeleteAllJobsSelectedMemberAction = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidEnableDeleteAllJobsSelectedMemberAction = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "enable_experimental_single_job_delete")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundEnableExperimentalSingleJobDelete = true;
                configOut->enableExperimentalSingleJobDelete = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidEnableExperimentalSingleJobDelete = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "log_selected_member_job_snapshot")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundLogSelectedMemberJobSnapshot = true;
                configOut->logSelectedMemberJobSnapshot = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidLogSelectedMemberJobSnapshot = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "job_b_gone_panel_has_custom_position")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundJobBGonePanelHasCustomPosition = true;
                configOut->jobBGonePanelHasCustomPosition = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidJobBGonePanelHasCustomPosition = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "job_b_gone_panel_pos_x")
        {
            int parsedInt = 0;
            bool clamped = false;
            size_t valuePos = pos;
            if (ParseJsonUnsignedIntValue(body, &valuePos, kPanelMaxPersistedCoord, &parsedInt, &clamped))
            {
                diagnostics->foundJobBGonePanelPosX = true;
                diagnostics->clampedJobBGonePanelPosX = diagnostics->clampedJobBGonePanelPosX || clamped;
                configOut->jobBGonePanelPosX = parsedInt;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidJobBGonePanelPosX = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "job_b_gone_panel_pos_y")
        {
            int parsedInt = 0;
            bool clamped = false;
            size_t valuePos = pos;
            if (ParseJsonUnsignedIntValue(body, &valuePos, kPanelMaxPersistedCoord, &parsedInt, &clamped))
            {
                diagnostics->foundJobBGonePanelPosY = true;
                diagnostics->clampedJobBGonePanelPosY = diagnostics->clampedJobBGonePanelPosY || clamped;
                configOut->jobBGonePanelPosY = parsedInt;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidJobBGonePanelPosY = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else
        {
            if (!SkipJsonValue(body, &pos))
            {
                return RecordConfigSyntaxError(diagnostics, pos);
            }
        }

        SkipJsonWhitespace(body, &pos);
        if (pos >= body.size())
        {
            return RecordConfigSyntaxError(diagnostics, pos);
        }

        if (body[pos] == ',')
        {
            ++pos;
            SkipJsonWhitespace(body, &pos);
            continue;
        }

        if (body[pos] == '}')
        {
            ++pos;
            break;
        }

        return RecordConfigSyntaxError(diagnostics, pos);
    }

    SkipJsonWhitespace(body, &pos);
    if (pos != body.size())
    {
        return RecordConfigSyntaxError(diagnostics, pos);
    }

    return true;
}

static bool RunInternalSelfChecks()
{
    // Keep this intentionally small: sanity-check parser and state helpers.
    PluginConfig parsedConfig = { true, 2000, false, true, false, true, false, 0, 0 };
    ConfigParseDiagnostics diagnostics;
    ResetConfigParseDiagnostics(&diagnostics);

    if (!ParseConfigJson(
            "{\"enabled\":false,\"pause_debounce_ms\":1234,\"debug_log_transitions\":true,"
            "\"enable_delete_all_jobs_selected_member_action\":false,"
            "\"enable_experimental_single_job_delete\":true,"
            "\"log_selected_member_job_snapshot\":false,"
            "\"job_b_gone_panel_has_custom_position\":true,"
            "\"job_b_gone_panel_pos_x\":321,"
            "\"job_b_gone_panel_pos_y\":654}",
            &parsedConfig,
            &diagnostics))
    {
        return false;
    }
    if (parsedConfig.enabled
        || parsedConfig.pauseDebounceMs != 1234
        || !parsedConfig.debugLogTransitions
        || parsedConfig.enableDeleteAllJobsSelectedMemberAction
        || !parsedConfig.enableExperimentalSingleJobDelete
        || parsedConfig.logSelectedMemberJobSnapshot
        || !parsedConfig.jobBGonePanelHasCustomPosition
        || parsedConfig.jobBGonePanelPosX != 321
        || parsedConfig.jobBGonePanelPosY != 654)
    {
        return false;
    }

    const std::string bomJson = std::string("\xEF\xBB\xBF")
        + "{\"enabled\":true,\"pause_debounce_ms\":2000,\"debug_log_transitions\":false,"
          "\"enable_delete_all_jobs_selected_member_action\":true,"
          "\"enable_experimental_single_job_delete\":false,"
          "\"log_selected_member_job_snapshot\":true,"
          "\"job_b_gone_panel_has_custom_position\":false,"
          "\"job_b_gone_panel_pos_x\":11,"
          "\"job_b_gone_panel_pos_y\":22}";
    parsedConfig.enabled = false;
    parsedConfig.pauseDebounceMs = 1;
    parsedConfig.debugLogTransitions = true;
    parsedConfig.enableDeleteAllJobsSelectedMemberAction = false;
    parsedConfig.enableExperimentalSingleJobDelete = true;
    parsedConfig.logSelectedMemberJobSnapshot = false;
    parsedConfig.jobBGonePanelHasCustomPosition = true;
    parsedConfig.jobBGonePanelPosX = 0;
    parsedConfig.jobBGonePanelPosY = 0;
    ResetConfigParseDiagnostics(&diagnostics);
    if (!ParseConfigJson(bomJson, &parsedConfig, &diagnostics))
    {
        return false;
    }
    if (!parsedConfig.enabled
        || parsedConfig.pauseDebounceMs != 2000
        || parsedConfig.debugLogTransitions
        || !parsedConfig.enableDeleteAllJobsSelectedMemberAction
        || parsedConfig.enableExperimentalSingleJobDelete
        || !parsedConfig.logSelectedMemberJobSnapshot
        || parsedConfig.jobBGonePanelHasCustomPosition
        || parsedConfig.jobBGonePanelPosX != 11
        || parsedConfig.jobBGonePanelPosY != 22)
    {
        return false;
    }

    parsedConfig.enabled = true;
    ResetConfigParseDiagnostics(&diagnostics);
    if (!ParseConfigJson("{\"enabled\":\"nope\"}", &parsedConfig, &diagnostics))
    {
        return false;
    }
    if (!diagnostics.invalidEnabled)
    {
        return false;
    }

    if (!DebounceWindowElapsed(100, 0, 50) || DebounceWindowElapsed(20, 0, 50))
    {
        return false;
    }

    const RuntimeState savedState = g_state;
    g_state.loadInProgress = true;
    g_state.pauseArmed = true;
    g_state.loadSignalSeenAfterArm = true;
    g_state.armTimestampMs = 99;
    g_state.loggedWorldUnavailable = true;
    DisarmPauseAfterLoad();
    const bool disarmedOk =
        !g_state.loadInProgress
        && !g_state.pauseArmed
        && !g_state.loadSignalSeenAfterArm
        && g_state.armTimestampMs == 0
        && !g_state.loggedWorldUnavailable;
    g_state = savedState;
    return disarmedOk;
}

static bool ReadConfigFromFile(
    const std::string& configPath,
    PluginConfig* configOut,
    bool* foundFileOut,
    bool* needsWriteBackOut)
{
    if (!configOut)
    {
        return false;
    }

    if (foundFileOut)
    {
        *foundFileOut = false;
    }
    if (needsWriteBackOut)
    {
        *needsWriteBackOut = false;
    }

    std::ifstream in(configPath.c_str(), std::ios::in | std::ios::binary);
    if (!in)
    {
        if (needsWriteBackOut)
        {
            *needsWriteBackOut = true;
        }
        return true;
    }

    if (foundFileOut)
    {
        *foundFileOut = true;
    }

    const std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ConfigParseDiagnostics diagnostics;
    ResetConfigParseDiagnostics(&diagnostics);
    if (!ParseConfigJson(body, configOut, &diagnostics))
    {
        std::stringstream error;
        error << kPluginName << " ERROR: mod-config.json parse error near byte offset " << diagnostics.syntaxErrorOffset;
        ErrorLog(error.str().c_str());
        return false;
    }

    bool needsWriteBack = false;
    if (!diagnostics.foundEnabled || diagnostics.invalidEnabled)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"enabled\"; using default");
    }
    if (!diagnostics.foundPauseDebounceMs || diagnostics.invalidPauseDebounceMs)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"pause_debounce_ms\"; using default");
    }
    if (diagnostics.clampedPauseDebounceMs)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: \"pause_debounce_ms\" exceeded max; clamped to 600000");
    }
    if (!diagnostics.foundDebugLogTransitions || diagnostics.invalidDebugLogTransitions)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"debug_log_transitions\"; using default");
    }
    if (!diagnostics.foundEnableDeleteAllJobsSelectedMemberAction
        || diagnostics.invalidEnableDeleteAllJobsSelectedMemberAction)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"enable_delete_all_jobs_selected_member_action\"; using default");
    }
    if (!diagnostics.foundEnableExperimentalSingleJobDelete
        || diagnostics.invalidEnableExperimentalSingleJobDelete)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"enable_experimental_single_job_delete\"; using default");
    }
    if (!diagnostics.foundLogSelectedMemberJobSnapshot || diagnostics.invalidLogSelectedMemberJobSnapshot)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"log_selected_member_job_snapshot\"; using default");
    }
    if (!diagnostics.foundJobBGonePanelHasCustomPosition || diagnostics.invalidJobBGonePanelHasCustomPosition)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"job_b_gone_panel_has_custom_position\"; using default");
    }
    if (!diagnostics.foundJobBGonePanelPosX || diagnostics.invalidJobBGonePanelPosX)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"job_b_gone_panel_pos_x\"; using default");
    }
    if (diagnostics.clampedJobBGonePanelPosX)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: \"job_b_gone_panel_pos_x\" exceeded max; clamped");
    }
    if (!diagnostics.foundJobBGonePanelPosY || diagnostics.invalidJobBGonePanelPosY)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: invalid/missing key \"job_b_gone_panel_pos_y\"; using default");
    }
    if (diagnostics.clampedJobBGonePanelPosY)
    {
        needsWriteBack = true;
        ErrorLog("Job-B-Gone WARN: \"job_b_gone_panel_pos_y\" exceeded max; clamped");
    }
    if (needsWriteBackOut)
    {
        *needsWriteBackOut = needsWriteBack;
    }
    return true;
}

static bool SaveConfigToFile(const std::string& configPath, const PluginConfig& config)
{
    std::ofstream out(configPath.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out)
    {
        return false;
    }

    out << "{\n";
    out << "  \"enabled\": " << (config.enabled ? "true" : "false") << ",\n";
    out << "  \"pause_debounce_ms\": " << config.pauseDebounceMs << ",\n";
    out << "  \"debug_log_transitions\": " << (config.debugLogTransitions ? "true" : "false") << ",\n";
    out << "  \"enable_delete_all_jobs_selected_member_action\": "
        << (config.enableDeleteAllJobsSelectedMemberAction ? "true" : "false") << ",\n";
    out << "  \"enable_experimental_single_job_delete\": "
        << (config.enableExperimentalSingleJobDelete ? "true" : "false") << ",\n";
    out << "  \"log_selected_member_job_snapshot\": "
        << (config.logSelectedMemberJobSnapshot ? "true" : "false") << ",\n";
    out << "  \"job_b_gone_panel_has_custom_position\": "
        << (config.jobBGonePanelHasCustomPosition ? "true" : "false") << ",\n";
    out << "  \"job_b_gone_panel_pos_x\": " << config.jobBGonePanelPosX << ",\n";
    out << "  \"job_b_gone_panel_pos_y\": " << config.jobBGonePanelPosY << "\n";
    out << "}\n";

    return true;
}
