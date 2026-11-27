#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winhttp.h>
#include "resource.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cwctype>
#include <cwchar>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace
{
constexpr wchar_t kWindowClass[] = L"AdhanBarWindow";
constexpr wchar_t kSettingsClass[] = L"AdhanBarSettings";
constexpr wchar_t kMutexName[] = L"Local\\AdhanBar.Native";
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kPrayerResultMessage = WM_APP + 2;
constexpr UINT_PTR kUpdateTimer = 1;
constexpr UINT_PTR kFlashTimer = 2;
constexpr UINT_PTR kPrayerRetryTimer = 3;
constexpr int kWidgetWidthDip = 160;
constexpr UINT kTrayIconId = 1;
constexpr UINT kMenuToggle = 1001;
constexpr UINT kMenuSettings = 1002;
constexpr UINT kMenuRefresh = 1003;
constexpr UINT kMenuExit = 1004;
constexpr int kSettingsCityEdit = 2001;
constexpr int kSettingsOffsetEdit = 2002;
constexpr int kSettingsSave = 2003;
constexpr int kSettingsCancel = 2004;
constexpr int kSettingsColorPreview = 2005;
constexpr int kSettingsChooseColor = 2006;
constexpr int kSettingsOpacityEdit = 2007;
constexpr int kSettingsLanguageCombo = 2008;
constexpr int kSettingsFontSizeEdit = 2009;

enum class UiLanguage
{
    Indonesian,
    English
};

const std::array<std::wstring, 5> kIndonesianPrayerNames{
    L"Subuh", L"Dzuhur", L"Ashar", L"Maghrib", L"Isya"};
const std::array<std::wstring, 5> kEnglishPrayerNames{
    L"Fajr", L"Dhuhr", L"Asr", L"Maghrib", L"Isha"};
const std::array<std::string, 5> kPrayerJsonNames{
    "Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};

HINSTANCE g_instance{};
HICON g_appIcon{};
HWND g_widget{};
HWND g_settingsWindow{};
HWND g_cityEdit{};
HWND g_offsetEdit{};
HWND g_opacityEdit{};
HWND g_colorPreview{};
HWND g_languageCombo{};
HWND g_fontSizeEdit{};
HFONT g_font{};
NOTIFYICONDATAW g_trayData{};
bool g_trayAdded{};
bool g_prayerTimesLoaded{};
bool g_prayerRequestInFlight{};
bool g_settingsLoadFailed{};
bool g_flashActive{};
bool g_flashVisible{};
LONG g_requestGeneration{};
int g_taskbarOffsetPixels{};
int g_backgroundTransparencyPercent{12};
int g_widgetFontSizePt{10};
ULONGLONG g_flashUntil{};
UiLanguage g_language{UiLanguage::English};
COLORREF g_backgroundColor{RGB(30, 30, 36)};
COLORREF g_dialogBackgroundColor{RGB(30, 30, 36)};
int g_lastNotifiedDate{};
int g_lastNotifiedPrayer{-1};
int g_scheduleDate{};
int g_lastRequestDate{};
std::wstring g_city{L"Jakarta"};
std::wstring g_pendingCity;
std::wstring g_settingsPath;
std::wstring g_nextPrayerText{L"Memuat jadwal..."};
std::wstring g_countdownText{L"--:--:--"};
std::wstring g_lastTooltip;
std::array<int, 5> g_prayerMinutes{};
UINT g_taskbarCreatedMessage{};

const wchar_t* UiText(const wchar_t* indonesian, const wchar_t* english)
{
    return g_language == UiLanguage::English ? english : indonesian;
}

const std::wstring& PrayerName(size_t index)
{
    return g_language == UiLanguage::English
        ? kEnglishPrayerNames[index]
        : kIndonesianPrayerNames[index];
}

struct PrayerResult
{
    LONG generation{};
    bool success{};
    bool notifyFailure{};
    int requestDate{};
    std::wstring city;
    std::wstring error;
    std::array<int, 5> minutes{};
};

struct PrayerRequest
{
    HWND window{};
    LONG generation{};
    int requestDate{};
    std::wstring city;
    bool notifyFailure{};
};

std::wstring Utf8ToWide(const std::string& value)
{
    if (value.empty())
        return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0)
        return {};
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty())
        return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

void AppendUtf8(std::string& output, unsigned int codePoint)
{
    if (codePoint <= 0x7F)
    {
        output.push_back(static_cast<char>(codePoint));
    }
    else if (codePoint <= 0x7FF)
    {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    else if (codePoint <= 0xFFFF)
    {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
    else
    {
        output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

int HexDigit(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

bool ReadJsonString(const std::string& json, const std::string& name, std::string& value)
{
    const std::string key = "\"" + name + "\"";
    size_t position = json.find(key);
    if (position == std::string::npos)
        return false;
    position = json.find(':', position + key.size());
    if (position == std::string::npos)
        return false;
    ++position;
    while (position < json.size() && std::isspace(static_cast<unsigned char>(json[position])))
        ++position;
    if (position >= json.size() || json[position++] != '"')
        return false;

    value.clear();
    while (position < json.size())
    {
        char current = json[position++];
        if (current == '"')
            return true;
        if (current != '\\')
        {
            value.push_back(current);
            continue;
        }
        if (position >= json.size())
            return false;

        char escaped = json[position++];
        switch (escaped)
        {
        case '"': value.push_back('"'); break;
        case '\\': value.push_back('\\'); break;
        case '/': value.push_back('/'); break;
        case 'b': value.push_back('\b'); break;
        case 'f': value.push_back('\f'); break;
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        case 'u':
        {
            if (position + 4 > json.size())
                return false;
            unsigned int codePoint = 0;
            for (int digit = 0; digit < 4; ++digit)
            {
                int hex = HexDigit(json[position++]);
                if (hex < 0)
                    return false;
                codePoint = (codePoint << 4) | static_cast<unsigned int>(hex);
            }
            if (codePoint >= 0xD800 && codePoint <= 0xDBFF
                && position + 6 <= json.size()
                && json[position] == '\\'
                && json[position + 1] == 'u')
            {
                position += 2;
                unsigned int lowSurrogate = 0;
                for (int digit = 0; digit < 4; ++digit)
                {
                    int hex = HexDigit(json[position++]);
                    if (hex < 0)
                        return false;
                    lowSurrogate = (lowSurrogate << 4) | static_cast<unsigned int>(hex);
                }
                if (lowSurrogate >= 0xDC00 && lowSurrogate <= 0xDFFF)
                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (lowSurrogate - 0xDC00);
            }
            AppendUtf8(value, codePoint);
            break;
        }
        default:
            return false;
        }
    }
    return false;
}

bool ReadJsonInteger(const std::string& json, const std::string& name, int& value)
{
    const std::string key = "\"" + name + "\"";
    size_t position = json.find(key);
    if (position == std::string::npos)
        return false;
    position = json.find(':', position + key.size());
    if (position == std::string::npos)
        return false;
    ++position;
    while (position < json.size() && std::isspace(static_cast<unsigned char>(json[position])))
        ++position;
    size_t end = position;
    if (end < json.size() && json[end] == '-')
        ++end;
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end])))
        ++end;
    if (end == position || (end == position + 1 && json[position] == '-'))
        return false;

    try
    {
        long long parsed = std::stoll(json.substr(position, end - position));
        if (parsed < INT_MIN || parsed > INT_MAX)
            return false;
        value = static_cast<int>(parsed);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool ParseHexColor(const std::string& value, COLORREF& color)
{
    if (value.size() != 7 || value[0] != '#')
        return false;
    int digits[6]{};
    for (size_t index = 0; index < 6; ++index)
    {
        digits[index] = HexDigit(value[index + 1]);
        if (digits[index] < 0)
            return false;
    }
    color = RGB(
        (digits[0] << 4) | digits[1],
        (digits[2] << 4) | digits[3],
        (digits[4] << 4) | digits[5]);
    return true;
}

std::string HexColorString(COLORREF color)
{
    char value[8]{};
    std::snprintf(value, sizeof(value), "#%02X%02X%02X",
        GetRValue(color), GetGValue(color), GetBValue(color));
    return value;
}

std::string EscapeJsonString(const std::string& value)
{
    std::string result;
    result.reserve(value.size() + 8);
    for (unsigned char character : value)
    {
        switch (character)
        {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (character < 0x20)
            {
                char escaped[7]{};
                std::snprintf(escaped, sizeof(escaped), "\\u%04x", character);
                result += escaped;
            }
            else
            {
                result.push_back(static_cast<char>(character));
            }
        }
    }
    return result;
}

bool ReadWholeFile(const std::wstring& path, std::string& contents)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size{};
    bool success = GetFileSizeEx(file, &size) && size.QuadPart >= 0 && size.QuadPart <= 65536;
    if (success)
    {
        contents.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        success = contents.empty() || (ReadFile(file, contents.data(), static_cast<DWORD>(contents.size()), &read, nullptr) && read == contents.size());
    }
    CloseHandle(file);
    return success;
}

bool WriteWholeFile(const std::wstring& path, const std::string& contents)
{
    std::wstring temporaryPath = path + L".tmp";
    HANDLE file = CreateFileW(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    bool success = WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()), &written, nullptr)
        && written == contents.size();
    if (!FlushFileBuffers(file))
        success = false;
    CloseHandle(file);
    if (!success)
    {
        DeleteFileW(temporaryPath.c_str());
        return false;
    }
    if (!MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(temporaryPath.c_str());
        return false;
    }
    return true;
}

void LoadSettings()
{
    wchar_t localAppData[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppData)))
    {
        g_settingsLoadFailed = true;
        return;
    }

    std::wstring directory = localAppData;
    directory += L"\\AdhanBar";
    g_settingsPath = directory + L"\\settings.json";

    std::string contents;
    if (!ReadWholeFile(g_settingsPath, contents))
    {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
            g_settingsLoadFailed = true;
        return;
    }

    std::string city;
    if (ReadJsonString(contents, "City", city) && !city.empty())
    {
        std::wstring loadedCity = Utf8ToWide(city);
        if (!loadedCity.empty())
            g_city = std::move(loadedCity);
        else
            g_settingsLoadFailed = true;
    }
    else
        g_settingsLoadFailed = true;

    int offset = 0;
    if (ReadJsonInteger(contents, "HorizontalOffsetPixels", offset))
        g_taskbarOffsetPixels = offset;
    else
        g_settingsLoadFailed = true;

    std::string backgroundColor;
    if (ReadJsonString(contents, "BackgroundColor", backgroundColor))
    {
        if (!ParseHexColor(backgroundColor, g_backgroundColor))
            g_settingsLoadFailed = true;
    }
    else if (contents.find("\"BackgroundColor\"") != std::string::npos)
        g_settingsLoadFailed = true;

    int transparency = g_backgroundTransparencyPercent;
    if (ReadJsonInteger(contents, "BackgroundTransparency", transparency))
    {
        if (transparency < 0 || transparency > 100)
            g_settingsLoadFailed = true;
        else
            g_backgroundTransparencyPercent = transparency;
    }
    else if (contents.find("\"BackgroundTransparency\"") != std::string::npos)
        g_settingsLoadFailed = true;
    else
    {
        int legacyOpacity = 0;
        if (ReadJsonInteger(contents, "BackgroundOpacity", legacyOpacity))
        {
            if (legacyOpacity < 0 || legacyOpacity > 100)
                g_settingsLoadFailed = true;
            else
                g_backgroundTransparencyPercent = legacyOpacity;
        }
        else if (contents.find("\"BackgroundOpacity\"") != std::string::npos)
            g_settingsLoadFailed = true;
    }

    std::string language;
    if (ReadJsonString(contents, "Language", language))
    {
        if (language == "en")
            g_language = UiLanguage::English;
        else if (language == "id")
            g_language = UiLanguage::Indonesian;
        else
            g_settingsLoadFailed = true;
    }
    else if (contents.find("\"Language\"") != std::string::npos)
        g_settingsLoadFailed = true;

    int fontSize = g_widgetFontSizePt;
    if (ReadJsonInteger(contents, "FontSizePt", fontSize))
    {
        if (fontSize < 8 || fontSize > 16)
            g_settingsLoadFailed = true;
        else
            g_widgetFontSizePt = fontSize;
    }
    else if (contents.find("\"FontSizePt\"") != std::string::npos)
        g_settingsLoadFailed = true;
}

bool SaveSettings()
{
    if (g_settingsPath.empty())
        return false;
    size_t lastSlash = g_settingsPath.find_last_of(L"\\/");
    if (lastSlash == std::wstring::npos)
        return false;
    if (!CreateDirectoryW(g_settingsPath.substr(0, lastSlash).c_str(), nullptr))
    {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
            return false;
    }

    std::string contents = "{\"City\":\"" + EscapeJsonString(WideToUtf8(g_city))
        + "\",\"HorizontalOffsetPixels\":" + std::to_string(g_taskbarOffsetPixels)
        + ",\"BackgroundColor\":\"" + HexColorString(g_backgroundColor)
        + "\",\"BackgroundTransparency\":" + std::to_string(g_backgroundTransparencyPercent)
        + ",\"BackgroundOpacity\":" + std::to_string(100 - g_backgroundTransparencyPercent)
        + ",\"Language\":\"" + (g_language == UiLanguage::English ? "en" : "id")
        + "\",\"FontSizePt\":" + std::to_string(g_widgetFontSizePt) + "}\n";
    return WriteWholeFile(g_settingsPath, contents);
}

void ShowBalloon(const std::wstring& title, const std::wstring& message, DWORD icon = NIIF_INFO)
{
    if (!g_trayAdded)
        return;
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = g_widget;
    data.uID = kTrayIconId;
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = icon;
    data.uTimeout = 5000;
    wcsncpy_s(data.szInfoTitle, _countof(data.szInfoTitle), title.c_str(), _TRUNCATE);
    wcsncpy_s(data.szInfo, _countof(data.szInfo), message.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void UpdateTrayTooltip(const std::wstring& text)
{
    if (!g_trayAdded || text == g_lastTooltip)
        return;

    g_lastTooltip = text;
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = g_widget;
    data.uID = kTrayIconId;
    data.uFlags = NIF_TIP;
    wcsncpy_s(data.szTip, _countof(data.szTip), text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void AddTrayIcon()
{
    if (g_trayAdded || !g_widget)
        return;

    g_trayData = {};
    g_trayData.cbSize = sizeof(g_trayData);
    g_trayData.hWnd = g_widget;
    g_trayData.uID = kTrayIconId;
    g_trayData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_trayData.uCallbackMessage = kTrayMessage;
    g_trayData.hIcon = g_appIcon;
    wcsncpy_s(g_trayData.szTip, _countof(g_trayData.szTip),
        UiText(L"AdhanBar - Jadwal Sholat", L"AdhanBar - Prayer Times"), _TRUNCATE);
    g_trayAdded = Shell_NotifyIconW(NIM_ADD, &g_trayData) != FALSE;
    if (g_trayAdded)
    {
        g_trayData.uVersion = NOTIFYICON_VERSION;
        Shell_NotifyIconW(NIM_SETVERSION, &g_trayData);
    }
}

void RemoveTrayIcon()
{
    if (g_trayAdded)
    {
        Shell_NotifyIconW(NIM_DELETE, &g_trayData);
        g_trayAdded = false;
    }
}

std::string UrlEncodeCity(const std::wstring& city)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string utf8 = WideToUtf8(city);
    std::string encoded;
    encoded.reserve(utf8.size());
    for (unsigned char character : utf8)
    {
        if ((character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9')
            || character == '-' || character == '_' || character == '.' || character == '~')
        {
            encoded.push_back(static_cast<char>(character));
        }
        else
        {
            encoded.push_back('%');
            encoded.push_back(hex[character >> 4]);
            encoded.push_back(hex[character & 0x0F]);
        }
    }
    return encoded;
}

bool ParsePrayerTime(const std::string& value, int& minutes)
{
    if (value.size() < 5
        || value[0] < '0' || value[0] > '9'
        || value[1] < '0' || value[1] > '9'
        || value[2] != ':'
        || value[3] < '0' || value[3] > '9'
        || value[4] < '0' || value[4] > '9')
        return false;

    int hour = (value[0] - '0') * 10 + value[1] - '0';
    int minute = (value[3] - '0') * 10 + value[4] - '0';
    if (hour > 23 || minute > 59)
        return false;
    minutes = hour * 60 + minute;
    return true;
}

bool AttachToTaskbar();

bool ReadJsonObject(const std::string& json, const std::string& name, std::string& value)
{
    const std::string key = "\"" + name + "\"";
    size_t position = json.find(key);
    if (position == std::string::npos)
        return false;
    position = json.find(':', position + key.size());
    if (position == std::string::npos)
        return false;
    position = json.find_first_not_of(" \t\r\n", position + 1);
    if (position == std::string::npos || json[position] != '{')
        return false;

    size_t objectStart = position++;
    size_t depth = 1;
    bool inString = false;
    bool escaped = false;
    for (; position < json.size(); ++position)
    {
        char current = json[position];
        if (inString)
        {
            if (escaped)
                escaped = false;
            else if (current == '\\')
                escaped = true;
            else if (current == '"')
                inString = false;
            continue;
        }
        if (current == '"')
            inString = true;
        else if (current == '{')
            ++depth;
        else if (current == '}' && --depth == 0)
        {
            value.assign(json, objectStart, position - objectStart + 1);
            return true;
        }
    }
    return false;
}

bool ExtractPrayerSchedule(const std::string& json, std::array<int, 5>& minutes)
{
    std::string timings;
    if (!ReadJsonObject(json, "timings", timings))
        return false;

    for (size_t index = 0; index < kPrayerJsonNames.size(); ++index)
    {
        std::string value;
        if (!ReadJsonString(timings, kPrayerJsonNames[index], value)
            || !ParsePrayerTime(value, minutes[index]))
            return false;
    }
    return true;
}

struct InternetHandle
{
    HINTERNET value{};
    ~InternetHandle()
    {
        if (value)
            WinHttpCloseHandle(value);
    }
};

bool FetchPrayerSchedule(const std::wstring& city, std::array<int, 5>& minutes, std::wstring& error)
{
    InternetHandle session{WinHttpOpen(L"AdhanBar/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.value)
    {
        error = L"Could not initialize Windows networking.";
        return false;
    }
    WinHttpSetTimeouts(session.value, 5000, 5000, 10000, 10000);

    InternetHandle connection{WinHttpConnect(session.value, L"api.aladhan.com", INTERNET_DEFAULT_HTTPS_PORT, 0)};
    if (!connection.value)
    {
        error = L"Could not connect to the prayer-time service.";
        return false;
    }

    std::string encodedCity = UrlEncodeCity(city);
    std::wstring path = L"/v1/timingsByCity?city=";
    path.append(encodedCity.begin(), encodedCity.end());
    path += L"&country=Indonesia&method=20";
    InternetHandle request{WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
    if (!request.value
        || !WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        || !WinHttpReceiveResponse(request.value, nullptr))
    {
        error = L"Could not download prayer times. Check the internet connection.";
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.value,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)
        || status < 200 || status >= 300)
    {
        error = L"The prayer-time service returned an error.";
        return false;
    }

    std::string response;
    for (;;)
    {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available))
        {
            error = L"Could not read the prayer-time response.";
            return false;
        }
        if (available == 0)
            break;
        if (response.size() + available > 1024 * 1024)
        {
            error = L"The prayer-time response was unexpectedly large.";
            return false;
        }

        std::vector<char> buffer(available);
        DWORD downloaded = 0;
        if (!WinHttpReadData(request.value, buffer.data(), available, &downloaded))
        {
            error = L"Could not read the prayer-time response.";
            return false;
        }
        response.append(buffer.data(), downloaded);
    }

    if (!ExtractPrayerSchedule(response, minutes))
    {
        error = L"The prayer-time service returned an invalid schedule.";
        return false;
    }
    return true;
}

std::wstring LocalizePrayerError(const std::wstring& error)
{
    if (g_language == UiLanguage::English)
        return error;
    if (error == L"Could not initialize Windows networking.")
        return L"Tidak dapat memulai koneksi jaringan Windows.";
    if (error == L"Could not connect to the prayer-time service.")
        return L"Tidak dapat terhubung ke layanan jadwal sholat.";
    if (error == L"Could not download prayer times. Check the internet connection.")
        return L"Tidak dapat mengunduh jadwal sholat. Periksa koneksi internet.";
    if (error == L"The prayer-time service returned an error.")
        return L"Layanan jadwal sholat mengembalikan kesalahan.";
    if (error == L"Could not read the prayer-time response.")
        return L"Tidak dapat membaca respons jadwal sholat.";
    if (error == L"The prayer-time response was unexpectedly large.")
        return L"Respons jadwal sholat terlalu besar.";
    if (error == L"The prayer-time service returned an invalid schedule.")
        return L"Layanan jadwal sholat mengembalikan jadwal yang tidak valid.";
    if (error == L"An unexpected error occurred while loading prayer times.")
        return L"Terjadi kesalahan saat memuat jadwal sholat.";
    return error;
}

DWORD WINAPI PrayerRequestThread(void* parameter)
{
    std::unique_ptr<PrayerRequest> request(static_cast<PrayerRequest*>(parameter));
    std::unique_ptr<PrayerResult> result(new PrayerResult);
    result->generation = request->generation;
    result->notifyFailure = request->notifyFailure;
    result->requestDate = request->requestDate;
    result->city = request->city;
    try
    {
        result->success = FetchPrayerSchedule(request->city, result->minutes, result->error);
    }
    catch (const std::exception&)
    {
        result->error = L"An unexpected error occurred while loading prayer times.";
    }
    catch (...)
    {
        result->error = L"An unexpected error occurred while loading prayer times.";
    }

    if (!PostMessageW(request->window, kPrayerResultMessage,
            static_cast<WPARAM>(result->generation), reinterpret_cast<LPARAM>(result.get())))
        return 0;
    result.release();
    return 0;
}

int CurrentDateKey()
{
    SYSTEMTIME now{};
    GetLocalTime(&now);
    return now.wYear * 10000 + now.wMonth * 100 + now.wDay;
}

void StartPrayerRequest(const std::wstring& city, bool notifyFailure = true)
{
    if (!g_widget)
        return;
    PrayerRequest* request = new PrayerRequest;
    request->window = g_widget;
    request->generation = InterlockedIncrement(&g_requestGeneration);
    request->requestDate = CurrentDateKey();
    g_lastRequestDate = request->requestDate;
    request->city = city;
    request->notifyFailure = notifyFailure;
    g_prayerRequestInFlight = true;

    HANDLE thread = CreateThread(nullptr, 0, PrayerRequestThread, request, 0, nullptr);
    if (!thread)
    {
        delete request;
        g_prayerRequestInFlight = false;
        if (notifyFailure)
            ShowBalloon(UiText(L"Jadwal sholat tidak tersedia", L"Prayer times unavailable"),
                UiText(L"Tidak dapat memulai permintaan jaringan.", L"Could not start the network request."),
                NIIF_ERROR);
        return;
    }
    CloseHandle(thread);
}

void StartPrayerFlash()
{
    g_flashActive = true;
    g_flashVisible = true;
    g_flashUntil = GetTickCount64() + 60000;
    if (!SetTimer(g_widget, kFlashTimer, 500, nullptr))
    {
        g_flashActive = false;
        g_flashVisible = true;
        ShowBalloon(UiText(L"Peringatan azan", L"Prayer alert"),
            UiText(L"Widget tidak dapat berkedip.", L"The widget could not start blinking."),
            NIIF_WARNING);
    }
    InvalidateRect(g_widget, nullptr, FALSE);
}

void UpdateCountdown()
{
    int today = CurrentDateKey();
    if (g_scheduleDate != today && g_lastRequestDate != today && !g_prayerRequestInFlight)
        StartPrayerRequest(g_pendingCity.empty() ? g_city : g_pendingCity, false);
    if (!g_prayerTimesLoaded)
        return;

    SYSTEMTIME now{};
    GetLocalTime(&now);
    int currentSeconds = static_cast<int>(now.wHour) * 3600 + static_cast<int>(now.wMinute) * 60 + now.wSecond;
    int nextIndex = 0;
    int remainingSeconds = 0;
    bool foundNext = false;

    for (size_t index = 0; index < g_prayerMinutes.size(); ++index)
    {
        int prayerSeconds = g_prayerMinutes[index] * 60;
        if (prayerSeconds > currentSeconds)
        {
            nextIndex = static_cast<int>(index);
            remainingSeconds = prayerSeconds - currentSeconds;
            foundNext = true;
            break;
        }

        if (currentSeconds / 60 == g_prayerMinutes[index])
        {
            int currentDate = now.wYear * 10000 + now.wMonth * 100 + now.wDay;
            if (g_lastNotifiedDate != currentDate || g_lastNotifiedPrayer != static_cast<int>(index))
            {
                g_lastNotifiedDate = currentDate;
                g_lastNotifiedPrayer = static_cast<int>(index);
                const std::wstring& prayerName = PrayerName(index);
                std::wstring title = g_language == UiLanguage::English
                    ? L"Time for " + prayerName
                    : L"Waktu " + prayerName + L" Tiba";
                std::wstring message = g_language == UiLanguage::English
                    ? L"It's time for " + prayerName + L"."
                    : L"Saatnya menunaikan sholat " + prayerName + L".";
                ShowBalloon(title, message);
                StartPrayerFlash();
            }
        }
    }

    if (!foundNext)
    {
        nextIndex = 0;
        remainingSeconds = 24 * 3600 - currentSeconds + g_prayerMinutes[0] * 60;
    }

    wchar_t prayerText[64]{};
    swprintf_s(prayerText, L"%s  %02d:%02d",
        PrayerName(static_cast<size_t>(nextIndex)).c_str(),
        g_prayerMinutes[static_cast<size_t>(nextIndex)] / 60,
        g_prayerMinutes[static_cast<size_t>(nextIndex)] % 60);
    wchar_t countdown[32]{};
    swprintf_s(countdown, L"%02d:%02d:%02d",
        remainingSeconds / 3600,
        (remainingSeconds / 60) % 60,
        remainingSeconds % 60);

    std::wstring newPrayerText = prayerText;
    std::wstring newCountdownText = countdown;
    bool prayerChanged = newPrayerText != g_nextPrayerText;
    bool changed = prayerChanged || newCountdownText != g_countdownText;
    g_nextPrayerText = std::move(newPrayerText);
    g_countdownText = std::move(newCountdownText);

    std::wstring tooltip = L"AdhanBar - " + g_city + L": " + PrayerName(static_cast<size_t>(nextIndex));
    if (g_language == UiLanguage::English)
        tooltip += L" at " + std::wstring(prayerText) + L", " + countdown + L" left";
    else
        tooltip += L" pukul " + std::wstring(prayerText) + L", tersisa " + countdown;
    if (tooltip.size() > 127)
        tooltip.resize(127);
    UpdateTrayTooltip(tooltip);

    if (prayerChanged && IsWindowVisible(g_widget))
        AttachToTaskbar();
    if (changed && g_widget)
        InvalidateRect(g_widget, nullptr, FALSE);
}

int MeasureTextWidth(const std::wstring& text)
{
    if (!g_widget || !g_font)
        return 0;
    HDC dc = GetDC(g_widget);
    if (!dc)
        return 0;
    HGDIOBJ previousFont = SelectObject(dc, g_font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    SelectObject(dc, previousFont);
    ReleaseDC(g_widget, dc);
    return size.cx;
}

int WidgetWidthPixels()
{
    UINT dpi = g_widget ? GetDpiForWindow(g_widget) : 96;
    if (dpi == 0)
        dpi = 96;
    int minimumWidth = MulDiv(kWidgetWidthDip, static_cast<int>(dpi), 96);
    int prayerWidth = MeasureTextWidth(g_nextPrayerText);
    int countdownWidth = MeasureTextWidth(g_countdownText);
    int requiredWidth = std::max((prayerWidth * 100 + 60) / 61,
        (countdownWidth * 100 + 38) / 39) + MulDiv(12, static_cast<int>(dpi), 96);
    return std::max(minimumWidth, requiredWidth);
}

void SetControlFont(HWND control)
{
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}

HWND CreateSettingsControl(HWND parent, const wchar_t* className, const wchar_t* text,
    DWORD style, int x, int y, int width, int height, int id, DWORD extendedStyle = 0)
{
    UINT dpi = GetDpiForWindow(parent);
    if (dpi == 0)
        dpi = 96;
    HWND control = CreateWindowExW(extendedStyle, className, text, WS_CHILD | WS_VISIBLE | style,
        MulDiv(x, static_cast<int>(dpi), 96),
        MulDiv(y, static_cast<int>(dpi), 96),
        MulDiv(width, static_cast<int>(dpi), 96),
        MulDiv(height, static_cast<int>(dpi), 96),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        g_instance, nullptr);
    if (control)
        SetControlFont(control);
    return control;
}

HFONT CreateWidgetFont(int pointSize)
{
    UINT dpi = GetDpiForWindow(g_widget);
    if (dpi == 0)
        dpi = 96;
    return CreateFontW(-MulDiv(pointSize, static_cast<int>(dpi), 72), 0, 0, 0, FW_SEMIBOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Bahnschrift");
}

void SaveSettingsFromDialog(HWND window)
{
    wchar_t city[256]{};
    wchar_t offsetText[64]{};
    wchar_t transparencyText[32]{};
    wchar_t fontSizeText[16]{};
    GetWindowTextW(g_cityEdit, city, _countof(city));
    GetWindowTextW(g_offsetEdit, offsetText, _countof(offsetText));
    GetWindowTextW(g_opacityEdit, transparencyText, _countof(transparencyText));
    GetWindowTextW(g_fontSizeEdit, fontSizeText, _countof(fontSizeText));
    if (city[0] == L'\0')
    {
        MessageBoxW(window, UiText(L"Masukkan nama kota.", L"Enter a city name."),
            UiText(L"Pengaturan", L"Settings"), MB_OK | MB_ICONWARNING);
        SetFocus(g_cityEdit);
        return;
    }

    wchar_t* end = nullptr;
    long long parsedOffset = _wcstoi64(offsetText, &end, 10);
    while (end && iswspace(*end))
        ++end;
    if (end == offsetText || (end && *end != L'\0') || parsedOffset < INT_MIN || parsedOffset > INT_MAX)
    {
        MessageBoxW(window, UiText(L"Offset harus berupa angka bulat dalam piksel.",
            L"Offset must be a whole number of pixels."),
            UiText(L"Pengaturan", L"Settings"), MB_OK | MB_ICONWARNING);
        SetFocus(g_offsetEdit);
        return;
    }

    wchar_t* transparencyEnd = nullptr;
    long long parsedTransparency = _wcstoi64(transparencyText, &transparencyEnd, 10);
    while (transparencyEnd && iswspace(*transparencyEnd))
        ++transparencyEnd;
    if (transparencyEnd == transparencyText || (transparencyEnd && *transparencyEnd != L'\0')
        || parsedTransparency < 0 || parsedTransparency > 100)
    {
        MessageBoxW(window, UiText(L"Transparansi harus berupa angka antara 0 dan 100.",
            L"Transparency must be a number from 0 to 100."),
            UiText(L"Pengaturan", L"Settings"), MB_OK | MB_ICONWARNING);
        SetFocus(g_opacityEdit);
        return;
    }

    LRESULT languageSelection = SendMessageW(g_languageCombo, CB_GETCURSEL, 0, 0);
    if (languageSelection != 0 && languageSelection != 1)
    {
        MessageBoxW(window, UiText(L"Pilih bahasa.", L"Select a language."),
            UiText(L"Pengaturan", L"Settings"), MB_OK | MB_ICONWARNING);
        SetFocus(g_languageCombo);
        return;
    }

    wchar_t* fontSizeEnd = nullptr;
    long long parsedFontSize = _wcstoi64(fontSizeText, &fontSizeEnd, 10);
    while (fontSizeEnd && iswspace(*fontSizeEnd))
        ++fontSizeEnd;
    if (fontSizeEnd == fontSizeText || (fontSizeEnd && *fontSizeEnd != L'\0')
        || parsedFontSize < 8 || parsedFontSize > 16)
    {
        MessageBoxW(window, UiText(L"Ukuran font harus antara 8 dan 16 pt.",
            L"Font size must be between 8 and 16 pt."),
            UiText(L"Pengaturan", L"Settings"), MB_OK | MB_ICONWARNING);
        SetFocus(g_fontSizeEdit);
        return;
    }

    HFONT newFont = CreateWidgetFont(static_cast<int>(parsedFontSize));
    if (!newFont)
    {
        MessageBoxW(window, UiText(L"Font widget gagal dibuat.", L"Could not create the widget font."),
            UiText(L"Pengaturan", L"Settings"), MB_OK | MB_ICONERROR);
        return;
    }

    std::wstring newCity = city;
    bool cityChanged = newCity != g_city;
    UiLanguage newLanguage = languageSelection == 1 ? UiLanguage::English : UiLanguage::Indonesian;
    g_taskbarOffsetPixels = static_cast<int>(parsedOffset);
    g_backgroundColor = g_dialogBackgroundColor;
    g_backgroundTransparencyPercent = static_cast<int>(parsedTransparency);
    g_language = newLanguage;
    g_widgetFontSizePt = static_cast<int>(parsedFontSize);
    HFONT previousFont = g_font;
    g_font = newFont;
    if (previousFont)
        DeleteObject(previousFont);
    if (!SaveSettings())
        ShowBalloon(UiText(L"Pengaturan tidak tersimpan", L"Settings not saved"),
            UiText(L"Pengaturan widget tidak dapat disimpan.", L"Could not save widget settings."), NIIF_ERROR);

    if (cityChanged)
    {
        g_pendingCity = std::move(newCity);
        StartPrayerRequest(g_pendingCity);
    }
    else if (!g_pendingCity.empty())
    {
        g_pendingCity.clear();
        InterlockedIncrement(&g_requestGeneration);
        g_prayerRequestInFlight = false;
    }
    if (!AttachToTaskbar())
        ShowBalloon(L"AdhanBar",
            UiText(L"Tidak dapat memposisikan widget di taskbar.", L"Could not position the widget on the taskbar."),
            NIIF_WARNING);
    if (g_prayerTimesLoaded)
        UpdateCountdown();
    else
    {
        g_nextPrayerText = UiText(L"Memuat jadwal...", L"Loading prayer times...");
        UpdateTrayTooltip(UiText(L"AdhanBar - Jadwal Sholat", L"AdhanBar - Prayer Times"));
    }
    InvalidateRect(g_widget, nullptr, FALSE);
    DestroyWindow(window);
}

LRESULT CALLBACK SettingsWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        CreateSettingsControl(window, L"STATIC", UiText(L"Nama kota:", L"City:"),
            0, 20, 25, 140, 28, 0);
        const std::wstring& displayedCity = g_pendingCity.empty() ? g_city : g_pendingCity;
        g_cityEdit = CreateSettingsControl(window, L"EDIT", displayedCity.c_str(),
            ES_AUTOHSCROLL | WS_TABSTOP, 175, 25, 240, 28, kSettingsCityEdit, WS_EX_CLIENTEDGE);
        CreateSettingsControl(window, L"STATIC", UiText(L"Geser posisi (px):", L"Position offset (px):"),
            0, 20, 70, 145, 28, 0);
        wchar_t offset[32]{};
        swprintf_s(offset, L"%d", g_taskbarOffsetPixels);
        g_offsetEdit = CreateSettingsControl(window, L"EDIT", offset,
            ES_AUTOHSCROLL | WS_TABSTOP, 175, 70, 120, 28, kSettingsOffsetEdit, WS_EX_CLIENTEDGE);
        CreateSettingsControl(window, L"STATIC", UiText(L"Warna latar:", L"Background color:"),
            0, 20, 110, 145, 28, 0);
        g_colorPreview = CreateSettingsControl(window, L"STATIC", L"",
            SS_OWNERDRAW, 175, 110, 44, 26, kSettingsColorPreview);
        CreateSettingsControl(window, L"BUTTON", UiText(L"Pilih warna...", L"Choose color..."),
            BS_PUSHBUTTON | WS_TABSTOP, 230, 108, 120, 30, kSettingsChooseColor);
        CreateSettingsControl(window, L"STATIC", UiText(L"Transparansi latar (%):", L"Transparency (%):"),
            0, 20, 150, 150, 28, 0);
        wchar_t transparency[16]{};
        swprintf_s(transparency, L"%d", g_backgroundTransparencyPercent);
        g_opacityEdit = CreateSettingsControl(window, L"EDIT", transparency,
            ES_AUTOHSCROLL | WS_TABSTOP, 175, 150, 80, 28, kSettingsOpacityEdit, WS_EX_CLIENTEDGE);
        CreateSettingsControl(window, L"STATIC",
            UiText(L"Bahasa:", L"Language:"), 0, 20, 190, 145, 28, 0);
        g_languageCombo = CreateSettingsControl(window, L"COMBOBOX", L"",
            CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 175, 188, 190, 72, kSettingsLanguageCombo);
        SendMessageW(g_languageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Bahasa Indonesia"));
        SendMessageW(g_languageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
        SendMessageW(g_languageCombo, CB_SETMINVISIBLE, 2, 0);
        SendMessageW(g_languageCombo, CB_SETCURSEL,
            g_language == UiLanguage::English ? 1 : 0, 0);
        CreateSettingsControl(window, L"STATIC",
            UiText(L"Ukuran font (pt):", L"Font size (pt):"), 0, 20, 230, 145, 28, 0);
        wchar_t fontSize[16]{};
        swprintf_s(fontSize, L"%d", g_widgetFontSizePt);
        g_fontSizeEdit = CreateSettingsControl(window, L"EDIT", fontSize,
            ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP, 175, 230, 80, 28,
            kSettingsFontSizeEdit, WS_EX_CLIENTEDGE);
        CreateSettingsControl(window, L"STATIC", L"8-16",
            0, 265, 230, 60, 28, 0);
        CreateSettingsControl(window, L"STATIC",
            UiText(L"Posisi: 0 = tengah area kanan; positif ke kiri, negatif ke kanan.",
                L"Position: 0 = center of right area; positive left, negative right."),
            0, 20, 270, 475, 22, 0);
        CreateSettingsControl(window, L"STATIC",
            UiText(L"100% = latar bening; 0% = solid. Teks tetap terlihat.",
                L"100% = transparent background; 0% = solid. Text stays visible."),
            0, 20, 294, 475, 22, 0);
        CreateSettingsControl(window, L"BUTTON", UiText(L"Simpan", L"Save"),
            BS_DEFPUSHBUTTON | WS_TABSTOP, 330, 340, 78, 30, kSettingsSave);
        CreateSettingsControl(window, L"BUTTON", UiText(L"Batal", L"Cancel"),
            BS_PUSHBUTTON | WS_TABSTOP, 418, 340, 78, 30, kSettingsCancel);
        return 0;
    }

    case WM_DRAWITEM:
        if (wParam == kSettingsColorPreview)
        {
            const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            HBRUSH colorBrush = CreateSolidBrush(g_dialogBackgroundColor);
            FillRect(item->hDC, &item->rcItem, colorBrush);
            DeleteObject(colorBrush);
            FrameRect(item->hDC, &item->rcItem, GetSysColorBrush(COLOR_WINDOWFRAME));
            return TRUE;
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == kSettingsChooseColor)
        {
            static COLORREF customColors[16]{};
            CHOOSECOLORW chooser{};
            chooser.lStructSize = sizeof(chooser);
            chooser.hwndOwner = window;
            chooser.rgbResult = g_dialogBackgroundColor;
            chooser.lpCustColors = customColors;
            chooser.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColorW(&chooser))
            {
                g_dialogBackgroundColor = chooser.rgbResult;
                InvalidateRect(g_colorPreview, nullptr, TRUE);
            }
            return 0;
        }
        if (LOWORD(wParam) == kSettingsSave)
        {
            SaveSettingsFromDialog(window);
            return 0;
        }
        if (LOWORD(wParam) == kSettingsCancel)
        {
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL)
        {
            DestroyWindow(window);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(window);
        return 0;

    case WM_DESTROY:
        g_settingsWindow = nullptr;
        g_cityEdit = nullptr;
        g_offsetEdit = nullptr;
        g_opacityEdit = nullptr;
        g_colorPreview = nullptr;
        g_languageCombo = nullptr;
        g_fontSizeEdit = nullptr;
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void OpenSettingsWindow()
{
    if (g_settingsWindow)
    {
        ShowWindow(g_settingsWindow, SW_SHOWNORMAL);
        SetForegroundWindow(g_settingsWindow);
        return;
    }

    g_dialogBackgroundColor = g_backgroundColor;
    WNDCLASSEXW settingsClass{};
    settingsClass.cbSize = sizeof(settingsClass);
    settingsClass.lpfnWndProc = SettingsWindowProc;
    settingsClass.hInstance = g_instance;
    settingsClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settingsClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    settingsClass.lpszClassName = kSettingsClass;
    RegisterClassExW(&settingsClass);

    g_settingsWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kSettingsClass,
        g_language == UiLanguage::English
            ? L"AdhanBar - Settings"
            : L"AdhanBar - Pengaturan",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 410,
        nullptr, nullptr, g_instance, nullptr);
    if (!g_settingsWindow)
    {
        ShowBalloon(UiText(L"Pengaturan gagal dibuka", L"Could not open settings"),
            UiText(L"Windows tidak dapat membuat jendela pengaturan.",
                L"Windows could not create the settings window."),
            NIIF_ERROR);
        return;
    }
    UINT dpi = GetDpiForWindow(g_widget);
    if (dpi == 0)
        dpi = 96;
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int width = MulDiv(520, static_cast<int>(dpi), 96);
    int height = MulDiv(410, static_cast<int>(dpi), 96);
    SetWindowPos(g_settingsWindow, HWND_TOPMOST,
        workArea.left + ((workArea.right - workArea.left) - width) / 2,
        workArea.top + ((workArea.bottom - workArea.top) - height) / 2,
        width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetForegroundWindow(g_settingsWindow);
    SetFocus(g_cityEdit);
}

void ShowTrayMenu()
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    AppendMenuW(menu, MF_STRING, kMenuToggle,
        IsWindowVisible(g_widget)
            ? UiText(L"Sembunyikan widget", L"Hide widget")
            : UiText(L"Tampilkan widget", L"Show widget"));
    AppendMenuW(menu, MF_STRING, kMenuSettings, UiText(L"Pengaturan widget...", L"Widget settings..."));
    AppendMenuW(menu, MF_STRING, kMenuRefresh, UiText(L"Perbarui jadwal", L"Refresh prayer times"));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, UiText(L"Keluar", L"Exit"));

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(g_widget);
    UINT command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
        cursor.x, cursor.y, 0, g_widget, nullptr);
    DestroyMenu(menu);
    PostMessageW(g_widget, WM_NULL, 0, 0);
    if (command)
        PostMessageW(g_widget, WM_COMMAND, command, 0);
}

bool AttachToTaskbar()
{
    if (!g_widget)
        return false;
    HWND taskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!taskbar)
        return false;

    LONG_PTR oldStyle = GetWindowLongPtrW(g_widget, GWL_STYLE);
    SetWindowLongPtrW(g_widget, GWL_STYLE, (oldStyle & ~static_cast<LONG_PTR>(WS_POPUP)) | WS_CHILD | WS_VISIBLE);
    SetLastError(ERROR_SUCCESS);
    HWND previousParent = SetParent(g_widget, taskbar);
    if (!previousParent && GetLastError() != ERROR_SUCCESS)
    {
        SetWindowLongPtrW(g_widget, GWL_STYLE, oldStyle);
        return false;
    }
    RECT taskbarRect{};
    RECT taskbarClient{};
    if (!GetWindowRect(taskbar, &taskbarRect) || !GetClientRect(taskbar, &taskbarClient))
        return false;

    int width = WidgetWidthPixels();
    int height = taskbarClient.bottom - taskbarClient.top;
    HWND trayArea = FindWindowExW(taskbar, nullptr, L"TrayNotifyWnd", nullptr);
    int rightBoundary = taskbarClient.right;
    if (trayArea)
    {
        RECT trayRect{};
        if (GetWindowRect(trayArea, &trayRect))
        {
            POINT trayPoint{trayRect.left, trayRect.top};
            MapWindowPoints(HWND_DESKTOP, taskbar, &trayPoint, 1);
            rightBoundary = trayPoint.x;
        }
    }

    int taskbarCenter = (taskbarClient.right - taskbarClient.left) / 2;
    int rightSectionCenter = taskbarCenter + (rightBoundary - taskbarCenter) / 2;
    int x = rightSectionCenter - width / 2 - g_taskbarOffsetPixels;
    int y = taskbarClient.top;
    return SetWindowPos(g_widget, HWND_TOP, x, y, width, height, SWP_NOACTIVATE) != FALSE;
}

void ShowFallbackWidget()
{
    if (!g_widget)
        return;

    LONG_PTR style = GetWindowLongPtrW(g_widget, GWL_STYLE);
    SetWindowLongPtrW(g_widget, GWL_STYLE, (style & ~static_cast<LONG_PTR>(WS_CHILD)) | WS_POPUP | WS_VISIBLE);
    SetParent(g_widget, nullptr);

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    UINT dpi = GetDpiForWindow(g_widget);
    if (dpi == 0)
        dpi = 96;
    int width = WidgetWidthPixels();
    int height = MulDiv(26, static_cast<int>(dpi), 96);
    SetWindowPos(g_widget, HWND_TOPMOST, workArea.right - width - 8, workArea.bottom - height - 8,
        width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void RenderWidget(HWND window)
{
    if (!window || !g_font)
        return;

    RECT bounds{};
    if (!GetClientRect(window, &bounds))
        return;
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0)
        return;

    HDC screenDc = GetDC(nullptr);
    if (!screenDc)
        return;

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* imageBits = nullptr;
    void* maskBits = nullptr;
    HBITMAP image = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &imageBits, nullptr, 0);
    HBITMAP mask = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &maskBits, nullptr, 0);
    HDC imageDc = CreateCompatibleDC(screenDc);
    HDC maskDc = CreateCompatibleDC(screenDc);
    if (!image || !mask || !imageBits || !maskBits || !imageDc || !maskDc)
    {
        if (imageDc)
            DeleteDC(imageDc);
        if (maskDc)
            DeleteDC(maskDc);
        if (image)
            DeleteObject(image);
        if (mask)
            DeleteObject(mask);
        ReleaseDC(nullptr, screenDc);
        return;
    }

    HGDIOBJ oldImage = SelectObject(imageDc, image);
    HGDIOBJ oldMask = SelectObject(maskDc, mask);
    bool flashHidden = g_flashActive && !g_flashVisible;
    const unsigned int backgroundAlpha = flashHidden
        ? 0
        : (100 - static_cast<unsigned int>(g_backgroundTransparencyPercent)) * 255 / 100;
    auto* pixels = static_cast<std::uint32_t*>(imageBits);
    auto* maskPixels = static_cast<std::uint32_t*>(maskBits);
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    const unsigned int red = GetRValue(g_backgroundColor);
    const unsigned int green = GetGValue(g_backgroundColor);
    const unsigned int blue = GetBValue(g_backgroundColor);
    const auto premultiply = [backgroundAlpha](unsigned int channel)
    {
        return (channel * backgroundAlpha + 127) / 255;
    };
    const std::uint32_t backgroundPixel =
        (backgroundAlpha << 24)
        | (premultiply(red) << 16)
        | (premultiply(green) << 8)
        | premultiply(blue);
    std::fill(pixels, pixels + pixelCount, backgroundPixel);

    SetBkMode(maskDc, TRANSPARENT);
    SelectObject(maskDc, g_font);
    auto drawText = [&](const std::wstring& text, RECT textBounds, UINT flags, COLORREF color)
    {
        std::fill(maskPixels, maskPixels + pixelCount, 0);
        SetTextColor(maskDc, RGB(255, 255, 255));
        DrawTextW(maskDc, text.c_str(), -1, &textBounds, flags);

        const unsigned int textRed = GetRValue(color);
        const unsigned int textGreen = GetGValue(color);
        const unsigned int textBlue = GetBValue(color);
        for (size_t index = 0; index < pixelCount; ++index)
        {
            std::uint32_t maskPixel = maskPixels[index];
            unsigned int coverage =
                (((maskPixel >> 16) & 0xFF) * 77
                    + ((maskPixel >> 8) & 0xFF) * 150
                    + (maskPixel & 0xFF) * 29 + 128) / 256;
            if (coverage == 0)
                continue;

            std::uint32_t destination = pixels[index];
            unsigned int inverse = 255 - coverage;
            unsigned int alpha = coverage + (((destination >> 24) & 0xFF) * inverse + 127) / 255;
            unsigned int outRed = (textRed * coverage + ((destination >> 16) & 0xFF) * inverse + 127) / 255;
            unsigned int outGreen = (textGreen * coverage + ((destination >> 8) & 0xFF) * inverse + 127) / 255;
            unsigned int outBlue = (textBlue * coverage + (destination & 0xFF) * inverse + 127) / 255;
            pixels[index] = (alpha << 24) | (outRed << 16) | (outGreen << 8) | outBlue;
        }
    };

    if (!flashHidden)
    {
        RECT prayerRect{4, 0, width * 61 / 100, height};
        drawText(g_nextPrayerText, prayerRect,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, RGB(255, 255, 255));
        RECT countdownRect{prayerRect.right, 0, width - 4, height};
        drawText(g_countdownText, countdownRect,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE, RGB(76, 194, 255));
    }

    SIZE size{width, height};
    POINT source{0, 0};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(window, screenDc, nullptr, &size,
        imageDc, &source, 0, &blend, ULW_ALPHA);

    SelectObject(imageDc, oldImage);
    SelectObject(maskDc, oldMask);
    DeleteDC(imageDc);
    DeleteDC(maskDc);
    DeleteObject(image);
    DeleteObject(mask);
    ReleaseDC(nullptr, screenDc);
}

LRESULT CALLBACK WidgetWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (g_taskbarCreatedMessage && message == g_taskbarCreatedMessage)
    {
        g_trayAdded = false;
        g_lastTooltip.clear();
        AddTrayIcon();
        if (!AttachToTaskbar())
            ShowFallbackWidget();
        UpdateCountdown();
        return 0;
    }

    switch (message)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        BeginPaint(window, &paint);
        EndPaint(window, &paint);
        RenderWidget(window);
        return 0;
    }

    case WM_WINDOWPOSCHANGED:
    {
        LRESULT result = DefWindowProcW(window, message, wParam, lParam);
        RenderWidget(window);
        return result;
    }

    case WM_NCHITTEST:
        return HTTRANSPARENT;

    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
        if (!AttachToTaskbar())
            ShowFallbackWidget();
        return 0;

    case WM_DPICHANGED:
    {
        HFONT newFont = CreateWidgetFont(g_widgetFontSizePt);
        if (newFont)
        {
            HFONT previousFont = g_font;
            g_font = newFont;
            if (previousFont)
                DeleteObject(previousFont);
        }
        else
            ShowBalloon(UiText(L"Ukuran font gagal diperbarui.", L"Could not update the widget font."),
                UiText(L"Ukuran font sebelumnya tetap digunakan.", L"The previous font size will remain in use."),
                NIIF_WARNING);
        if (!AttachToTaskbar())
            ShowFallbackWidget();
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }

    case WM_TIMER:
        if (wParam == kUpdateTimer)
        {
            UpdateCountdown();
            return 0;
        }
        if (wParam == kPrayerRetryTimer)
        {
            int today = CurrentDateKey();
            if (!g_prayerRequestInFlight
                && (!g_prayerTimesLoaded || g_scheduleDate != today || !g_pendingCity.empty()))
                StartPrayerRequest(g_pendingCity.empty() ? g_city : g_pendingCity, false);
            return 0;
        }
        if (wParam == kFlashTimer)
        {
            if (!g_flashActive || GetTickCount64() >= g_flashUntil)
            {
                g_flashActive = false;
                g_flashVisible = true;
                KillTimer(window, kFlashTimer);
            }
            else
                g_flashVisible = !g_flashVisible;
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        return 0;

    case kTrayMessage:
        switch (LOWORD(lParam))
        {
        case WM_LBUTTONUP:
            ShowWindow(window, IsWindowVisible(window) ? SW_HIDE : SW_SHOW);
            if (IsWindowVisible(window) && !AttachToTaskbar())
                ShowFallbackWidget();
            return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowTrayMenu();
            return 0;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case kMenuToggle:
            ShowWindow(window, IsWindowVisible(window) ? SW_HIDE : SW_SHOW);
            if (IsWindowVisible(window) && !AttachToTaskbar())
                ShowFallbackWidget();
            return 0;
        case kMenuSettings:
            OpenSettingsWindow();
            return 0;
        case kMenuRefresh:
            StartPrayerRequest(g_pendingCity.empty() ? g_city : g_pendingCity);
            return 0;
        case kMenuExit:
            DestroyWindow(window);
            return 0;
        }
        break;

    case kPrayerResultMessage:
    {
        std::unique_ptr<PrayerResult> result(reinterpret_cast<PrayerResult*>(lParam));
        if (!result
            || result->generation != static_cast<LONG>(wParam)
            || result->generation != InterlockedCompareExchange(&g_requestGeneration, 0, 0))
            return 0;
        g_prayerRequestInFlight = false;
        if (!result->success)
        {
            if (result->notifyFailure)
                ShowBalloon(UiText(L"Jadwal sholat tidak tersedia", L"Prayer times unavailable"),
                    LocalizePrayerError(result->error), NIIF_ERROR);
            return 0;
        }
        bool cityChanged = g_city != result->city;
        g_prayerMinutes = result->minutes;
        g_city = result->city;
        if (g_pendingCity == result->city)
            g_pendingCity.clear();
        g_scheduleDate = result->requestDate;
        g_prayerTimesLoaded = true;
        if (cityChanged && !SaveSettings())
            ShowBalloon(UiText(L"Pengaturan tidak tersimpan", L"Settings not saved"),
                UiText(L"Pengaturan widget tidak dapat disimpan.", L"Could not save widget settings."), NIIF_ERROR);
        UpdateCountdown();
        return 0;
    }

    case WM_DESTROY:
        KillTimer(window, kUpdateTimer);
        KillTimer(window, kFlashTimer);
        KillTimer(window, kPrayerRetryTimer);
        RemoveTrayIcon();
        if (g_font)
        {
            DeleteObject(g_font);
            g_font = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    SetLastError(ERROR_SUCCESS);
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    DWORD mutexError = GetLastError();
    if (!mutex || mutexError == ERROR_ALREADY_EXISTS)
    {
        if (mutex)
            CloseHandle(mutex);
        return 0;
    }

    g_instance = instance;
    LoadSettings();
    g_nextPrayerText = UiText(L"Memuat jadwal...", L"Loading prayer times...");
    g_appIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!g_appIcon)
    {
        MessageBoxW(nullptr, UiText(L"Tidak dapat memuat ikon aplikasi.", L"Could not load the application icon."),
            L"AdhanBar", MB_OK | MB_ICONERROR);
        CloseHandle(mutex);
        return 1;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WidgetWindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = g_appIcon;
    windowClass.hIconSm = g_appIcon;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&windowClass))
    {
        CloseHandle(mutex);
        return 1;
    }

    g_widget = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        kWindowClass,
        L"AdhanBar",
        WS_POPUP,
        0, 0, kWidgetWidthDip, 26,
        nullptr, nullptr, instance, nullptr);
    if (!g_widget)
    {
        MessageBoxW(nullptr,
            UiText(L"Tidak dapat membuat jendela widget sholat.", L"Could not create the prayer widget window."),
            L"AdhanBar", MB_OK | MB_ICONERROR);
        CloseHandle(mutex);
        return 1;
    }

    g_font = CreateWidgetFont(g_widgetFontSizePt);
    if (!g_font)
    {
        MessageBoxW(nullptr, UiText(L"Tidak dapat membuat font widget.", L"Could not create the widget font."),
            L"AdhanBar", MB_OK | MB_ICONERROR);
        DestroyWindow(g_widget);
        CloseHandle(mutex);
        return 1;
    }

    RenderWidget(g_widget);
    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    AddTrayIcon();
    if (!g_trayAdded)
    {
        MessageBoxW(nullptr,
            UiText(L"Tidak dapat membuat ikon system tray.", L"Could not create the system tray icon."),
            L"AdhanBar", MB_OK | MB_ICONERROR);
        DestroyWindow(g_widget);
        CloseHandle(mutex);
        return 1;
    }
    if (g_settingsLoadFailed)
        ShowBalloon(UiText(L"Pengaturan tidak terbaca", L"Settings could not be read"),
            UiText(L"Sebagian pengaturan menggunakan nilai bawaan.", L"Some settings are using their defaults."),
            NIIF_WARNING);
    if (!AttachToTaskbar())
    {
        ShowFallbackWidget();
        ShowBalloon(L"Prayer widget",
            UiText(L"Tidak dapat menempel ke taskbar Windows; widget ditampilkan di atasnya.",
                L"Could not attach to the Windows taskbar; showing it just above."),
            NIIF_WARNING);
    }

    SetTimer(g_widget, kUpdateTimer, 1000, nullptr);
    if (!SetTimer(g_widget, kPrayerRetryTimer, 30000, nullptr))
        ShowBalloon(UiText(L"Jadwal sholat tidak tersedia", L"Prayer times unavailable"),
            UiText(L"Pemutakhiran otomatis tidak dapat dijadwalkan.",
                L"Automatic schedule retries could not be scheduled."),
            NIIF_WARNING);
    ShowWindow(g_widget, SW_SHOW);
    UpdateWindow(g_widget);
    StartPrayerRequest(g_city);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (g_settingsWindow && IsDialogMessageW(g_settingsWindow, &message))
            continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    CloseHandle(mutex);
    return static_cast<int>(message.wParam);
}
