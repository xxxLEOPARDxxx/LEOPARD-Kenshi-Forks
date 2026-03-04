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

static bool ParseJsonSignedIntValue(
    const std::string& text,
    size_t* pos,
    int minValue,
    int maxValue,
    int* valueOut,
    bool* clampedOut)
{
    if (!pos || !valueOut || minValue > maxValue)
    {
        return false;
    }

    SkipJsonWhitespace(text, pos);
    size_t cursor = *pos;
    if (cursor < text.size() && (text[cursor] == '-' || text[cursor] == '+'))
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

    if (cursor < text.size() && !IsJsonLiteralTerminator(text[cursor]))
    {
        return false;
    }

    const std::string numberText = text.substr(*pos, cursor - *pos);
    long parsed = 0;
    try
    {
        parsed = std::stol(numberText);
    }
    catch (...)
    {
        return false;
    }

    bool clamped = false;
    if (parsed < static_cast<long>(minValue))
    {
        parsed = static_cast<long>(minValue);
        clamped = true;
    }
    else if (parsed > static_cast<long>(maxValue))
    {
        parsed = static_cast<long>(maxValue);
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
        else if (key == "enable_execute_kill_sound")
        {
            bool parsedBool = false;
            size_t valuePos = pos;
            if (ParseJsonBoolValue(body, &valuePos, &parsedBool))
            {
                diagnostics->foundEnableExecuteKillSound = true;
                configOut->enableExecuteKillSound = parsedBool;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidEnableExecuteKillSound = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "execute_button_width")
        {
            bool clamped = false;
            int parsedValue = 0;
            size_t valuePos = pos;
            if (ParseJsonSignedIntValue(
                    body,
                    &valuePos,
                    kExecuteButtonWidthMin,
                    kExecuteButtonWidthMax,
                    &parsedValue,
                    &clamped))
            {
                diagnostics->foundExecuteButtonWidthPx = true;
                diagnostics->clampedExecuteButtonWidthPx = clamped;
                configOut->executeButtonWidthPx = parsedValue;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidExecuteButtonWidthPx = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "execute_button_height")
        {
            bool clamped = false;
            int parsedValue = 0;
            size_t valuePos = pos;
            if (ParseJsonSignedIntValue(
                    body,
                    &valuePos,
                    kExecuteButtonHeightMin,
                    kExecuteButtonHeightMax,
                    &parsedValue,
                    &clamped))
            {
                diagnostics->foundExecuteButtonHeightPx = true;
                diagnostics->clampedExecuteButtonHeightPx = clamped;
                configOut->executeButtonHeightPx = parsedValue;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidExecuteButtonHeightPx = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "execute_button_x")
        {
            bool clamped = false;
            int parsedValue = 0;
            size_t valuePos = pos;
            if (ParseJsonSignedIntValue(
                    body,
                    &valuePos,
                    kExecuteButtonOffsetMin,
                    kExecuteButtonOffsetMax,
                    &parsedValue,
                    &clamped))
            {
                diagnostics->foundExecuteButtonOffsetXPx = true;
                diagnostics->clampedExecuteButtonOffsetXPx = clamped;
                configOut->executeButtonOffsetXPx = parsedValue;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidExecuteButtonOffsetXPx = true;
                if (!SkipJsonValue(body, &pos))
                {
                    return RecordConfigSyntaxError(diagnostics, pos);
                }
            }
        }
        else if (key == "execute_button_y")
        {
            bool clamped = false;
            int parsedValue = 0;
            size_t valuePos = pos;
            if (ParseJsonSignedIntValue(
                    body,
                    &valuePos,
                    kExecuteButtonOffsetMin,
                    kExecuteButtonOffsetMax,
                    &parsedValue,
                    &clamped))
            {
                diagnostics->foundExecuteButtonOffsetYPx = true;
                diagnostics->clampedExecuteButtonOffsetYPx = clamped;
                configOut->executeButtonOffsetYPx = parsedValue;
                pos = valuePos;
            }
            else
            {
                diagnostics->invalidExecuteButtonOffsetYPx = true;
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
    PluginConfig parsedConfig = { true, 2000, false, false, false, false, true, 0, 0, 0, 0 };
    ConfigParseDiagnostics diagnostics;
    ResetConfigParseDiagnostics(&diagnostics);

    if (!ParseConfigJson(
            "{\"enabled\":false,"
            "\"enable_execute_kill_sound\":false,"
            "\"execute_button_width\":320,"
            "\"execute_button_height\":44,"
            "\"execute_button_x\":15,"
            "\"execute_button_y\":-6}",
            &parsedConfig,
            &diagnostics))
    {
        return false;
    }
    if (parsedConfig.enabled
        || parsedConfig.enableExecuteKillSound
        || parsedConfig.executeButtonWidthPx != 320
        || parsedConfig.executeButtonHeightPx != 44
        || parsedConfig.executeButtonOffsetXPx != 15
        || parsedConfig.executeButtonOffsetYPx != -6)
    {
        return false;
    }

    const std::string bomJson = std::string("\xEF\xBB\xBF")
        + "{\"enabled\":true,"
          "\"enable_execute_kill_sound\":true,"
          "\"execute_button_width\":0,"
          "\"execute_button_height\":0,"
          "\"execute_button_x\":0,"
          "\"execute_button_y\":0}";
    parsedConfig.enabled = false;
    parsedConfig.enableExecuteKillSound = false;
    parsedConfig.executeButtonWidthPx = 1;
    parsedConfig.executeButtonHeightPx = 1;
    parsedConfig.executeButtonOffsetXPx = 1;
    parsedConfig.executeButtonOffsetYPx = 1;
    ResetConfigParseDiagnostics(&diagnostics);
    if (!ParseConfigJson(bomJson, &parsedConfig, &diagnostics))
    {
        return false;
    }
    if (!parsedConfig.enabled
        || !parsedConfig.enableExecuteKillSound
        || parsedConfig.executeButtonWidthPx != 0
        || parsedConfig.executeButtonHeightPx != 0
        || parsedConfig.executeButtonOffsetXPx != 0
        || parsedConfig.executeButtonOffsetYPx != 0)
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

    parsedConfig.executeButtonOffsetXPx = 0;
    ResetConfigParseDiagnostics(&diagnostics);
    if (!ParseConfigJson("{\"execute_button_x\":900000}", &parsedConfig, &diagnostics))
    {
        return false;
    }
    if (!diagnostics.clampedExecuteButtonOffsetXPx || parsedConfig.executeButtonOffsetXPx != kExecuteButtonOffsetMax)
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
        ErrorLog("Loot-Scoot-Execute WARN: invalid/missing key \"enabled\"; using default");
    }
    if (!diagnostics.foundEnableExecuteKillSound || diagnostics.invalidEnableExecuteKillSound)
    {
        needsWriteBack = true;
        ErrorLog("Loot-Scoot-Execute WARN: invalid/missing key \"enable_execute_kill_sound\"; using default");
    }
    if (!diagnostics.foundExecuteButtonWidthPx || diagnostics.invalidExecuteButtonWidthPx || diagnostics.clampedExecuteButtonWidthPx)
    {
        needsWriteBack = true;
        ErrorLog("Loot-Scoot-Execute WARN: invalid/missing/clamped key \"execute_button_width\"; using normalized value");
    }
    if (!diagnostics.foundExecuteButtonHeightPx || diagnostics.invalidExecuteButtonHeightPx || diagnostics.clampedExecuteButtonHeightPx)
    {
        needsWriteBack = true;
        ErrorLog("Loot-Scoot-Execute WARN: invalid/missing/clamped key \"execute_button_height\"; using normalized value");
    }
    if (!diagnostics.foundExecuteButtonOffsetXPx || diagnostics.invalidExecuteButtonOffsetXPx || diagnostics.clampedExecuteButtonOffsetXPx)
    {
        needsWriteBack = true;
        ErrorLog("Loot-Scoot-Execute WARN: invalid/missing/clamped key \"execute_button_x\"; using normalized value");
    }
    if (!diagnostics.foundExecuteButtonOffsetYPx || diagnostics.invalidExecuteButtonOffsetYPx || diagnostics.clampedExecuteButtonOffsetYPx)
    {
        needsWriteBack = true;
        ErrorLog("Loot-Scoot-Execute WARN: invalid/missing/clamped key \"execute_button_y\"; using normalized value");
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
    out << "  \"enable_execute_kill_sound\": " << (config.enableExecuteKillSound ? "true" : "false") << ",\n";
    out << "  \"execute_button_width\": " << config.executeButtonWidthPx << ",\n";
    out << "  \"execute_button_height\": " << config.executeButtonHeightPx << ",\n";
    out << "  \"execute_button_x\": " << config.executeButtonOffsetXPx << ",\n";
    out << "  \"execute_button_y\": " << config.executeButtonOffsetYPx << "\n";
    out << "}\n";

    return true;
}
