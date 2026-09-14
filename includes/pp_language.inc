/*
 * pp_language v2.0.0 - PawnPlus localization library.
 * Based on omp_language v1.07 by @KW46 (Kwarde).
 * Contributors: @NebulaGB, @itsneufox, @Trysha-rbrn.
 * See LICENSE for the original GPL-3.0 terms.
 */
#if defined _INC_pp_language
    #endinput
#endif
#define _INC_pp_language

#if !defined __PawnBuild
    #error pp_language requires compiler version 3.10.11
#endif
#if __PawnBuild != 11
    #error pp_language requires compiler version 3.10.11
#endif

/*-- Includes --*/
#include <open.mp>
#include <sscanf2>
#include <FileManager>
#include <pp_hooks>
#if !defined PP_VA_INCLUDED
    #include <pp-va>
#endif

#if defined LANGUAGE_SORT_LANGUAGES
    #undef LANGUAGE_SORT_LANGUAGES

    #if !defined _INC_md_sort
        #tryinclude <md-sort\md-sort>
    #endif
    #if !defined _INC_md_sort
        #tryinclude <md-sort/md-sort>
    #endif
    #if !defined _INC_md_sort
        #tryinclude <md-sort>
    #endif
    #if !defined _INC_md_sort
        #warning Failed to include md-sort.inc : Languages will not be sorted!
        #define LANGUAGE_SORT_LANGUAGES (0)
    #else
        #define LANGUAGE_SORT_LANGUAGES (1)
    #endif
#else
    #define LANGUAGE_SORT_LANGUAGES (0)
#endif

#if LANGUAGE_SORT_LANGUAGES
    #if !defined LANGUAGE_SORT_BY_NAME && !defined LANGUAGE_SORT_BY_CODE
        #error LANGUAGE_SORT_LANGUAGES defined, please also define either LANGUAGE_SORT_BY_NAME or LANGUAGE_SORT_BY_CODE
    #elseif defined LANGUAGE_SORT_BY_NAME && defined LANGUAGE_SORT_BY_CODE
        #error Define either LANGUAGE_SORT_BY_NAME or LANGUAGE_SORT_BY_CODE, not both
    #else
        #if defined LANGUAGE_SORT_BY_NAME
            #define _LANGUAGE_SORT_BY E_LANG_NAME
        #else
            #define _LANGUAGE_SORT_BY E_LANG_CODE
        #endif
    #endif

    #if !defined LANGUAGE_SORT_METHOD
        #define LANGUAGE_SORT_METHOD SORT_ASC
    #endif
#endif

#if !defined gLanguageColours
    #tryinclude <pp_language_colours>
#endif
#if !defined gLanguageColours
    #warning gLanguageColours not defined: Colour names used in language files will not work!
#endif

/*-- Macros/Constants --*/
#if !defined SQLITE_INVALID_HANDLE
    #define SQLITE_INVALID_HANDLE (DB:0)
#endif
#define LANGUAGE_INVALID_ID (-1)

#if !defined LANGUAGE_MAX_CONTENT_LENGTH
    #define LANGUAGE_MAX_CONTENT_LENGTH (1024)
#endif

#if !defined LANGUAGES_MAX
    #define LANGUAGES_MAX (4)
#endif

#if !defined LANGUAGE_DEFAULT
    #define LANGUAGE_DEFAULT "en"
#endif

#if !defined DEFAULT_MESSAGES_FILE_NAME
    #define DEFAULT_MESSAGES_FILE_NAME "_pp_language"
#endif

#if defined LANGUAGE_SQLITE_PERSISTENT_DATA
    #undef LANGUAGE_SQLITE_PERSISTENT_DATA
    #define LANGUAGE_SQLITE_PERSISTENT_DATA (1)

    #if !defined LANGUAGE_SQLITE_PERSISTENT_FILE
        #define LANGUAGE_SQLITE_PERSISTENT_FILE "languages.db"
    #endif
#else
    #define LANGUAGE_SQLITE_PERSISTENT_DATA (0)
#endif

#if !defined DIALOG_SELECT_LANGUAGE
    #if !defined FILTERSCRIPT
        #define DIALOG_SELECT_LANGUAGE (1)
    #else
        #define DIALOG_SELECT_LANGUAGE (2)
    #endif
#endif

#if defined LANGUAGE_PVAR_NAME
    #error Compiler constant LANGUAGE_PVAR_NAME used by pp_language, please undefine
#endif
#define LANGUAGE_PVAR_NAME "__pp_language_selectedLanguage"

#if !defined IsNull
    #define IsNull(%0) ((%0[(%0[0]) == '\1']) == '\0' || (%0[0]) == ucharmax + 1)
#endif

static enum E_LANG_DATA
{
    E_LANG_CODE[3],
    E_LANG_NAME[32]
};

/*-- Variables --*/
static DB:sLanguageDBHandle = SQLITE_INVALID_HANDLE,
    sLanguages[LANGUAGES_MAX][E_LANG_DATA],
    sDefaultLanguage[3], // Even though there is LANGUAGE_DEFAULT, still _may_ need this: If language LANGUAGE_DEFAULT doesn't exist, change default language to language id 0 (and store it in sDefaultLanguage)
    spLanguage[MAX_PLAYERS][3];

// Cache owns copies of keys and template arrays, never caller-owned String handles.
static Map:sLanguageTemplateCache;

stock Language_ClearCache()
{
    if (map_valid(sLanguageTemplateCache)) map_delete(sLanguageTemplateCache);
    sLanguageTemplateCache = map_new();
    return 1;
}

stock Language_CacheSize()
{
    return map_valid(sLanguageTemplateCache) ? map_size(sLanguageTemplateCache) : 0;
}

/*-- Functions --*/
// Local functions
static bool:Language_ScanLanguages()
{
    // Scans for languages in `scriptfiles/languages`, adds that language to sLanguages (if dir name is correct: `langCode_langName`)
    // It doesn't check if there are any files within that directory
    // Returns: (bool) true: Language scanned and added | false: Directory "scriptfiles/languages" doesn't exist
    // <!> Function will return true even if no languages exist
    if (!dir_exists("scriptfiles/languages"))
    {
        Print("[pp_language] ERROR: Language directory doesn't exist!");
        return false;
    }

    new
        dir:languagesDir,
        dirItem[35],
        dirItemType,
    #if LANGUAGE_SORT_LANGUAGES
        languages[LANGUAGES_MAX][E_LANG_DATA],
    #endif
        languagesAdded;
    languagesDir = dir_open("scriptfiles/languages");

    while (dir_list(languagesDir, dirItem, dirItemType))
    {
        if (dirItemType == FM_FILE || !strcmp(dirItem, ".") || !strcmp(dirItem, "..")) continue;

        new langCode[3], langName[32];
        if (sscanf(dirItem, "p<_>s[3]s[32]", langCode, langName))
        {
            PrintF("<!>[pp_language]: Invalid directory name '%s' found in 'scriptfiles/languages'!", dirItem);
            continue;
        }
        if (languagesAdded >= LANGUAGES_MAX)
        {
            PrintF("<!>[pp_language]: Cannot add language %s(%s): Maximum languages (%d) reached!", langName, langCode, LANGUAGES_MAX);
            continue; // Print a warning for every language that's not added
        }

        for (new i = 0; i < LANGUAGES_MAX; i++)
        {
        #if LANGUAGE_SORT_LANGUAGES
            if (IsNull(languages[i][E_LANG_CODE]))
            {
                format(languages[i][E_LANG_CODE], 3, langCode);
                format(languages[i][E_LANG_NAME], 32, langName);
                languagesAdded++;
                break;
            }
        #else
            if (IsNull(sLanguages[i][E_LANG_CODE]))
            {
                format(sLanguages[i][E_LANG_CODE], 3, langCode);
                format(sLanguages[i][E_LANG_NAME], 32, langName);
                languagesAdded++;
                break;
            }
        #endif
        }
    }
    dir_close(languagesDir);

#if LANGUAGE_SORT_LANGUAGES
    SortDeepArray(languages, _LANGUAGE_SORT_BY, .order = LANGUAGE_SORT_METHOD);

    for (new i = 0, j = -1; i < LANGUAGES_MAX; i++)
    {
        if (!IsNull(languages[i][E_LANG_CODE]))
        {
            format(sLanguages[++j][E_LANG_CODE], 3, languages[i][E_LANG_CODE]);
            format(sLanguages[j][E_LANG_NAME], 32, languages[i][E_LANG_NAME]);
        }
    }
#endif
    return true;
}

static void:Language_AddDataFromFile(const languageId, const string:file[], const string:subDir[] = "")
{
    // Creates a table either named `file` or `dir-file` and adds content of that file.
    // Doesn't check if given languageId is valid, so misusing it may (will) cause OOB RTEs.
    //  `languageId`: Array ID of sLanguages
    //  `file[]`: Name of the file, including file extension (.txt)
    //  `subDir[]`: Sub directory within the language directory, if any
    // Returns: Nothing
    new fullPath[128], tableName[64], File:languageFile, languageFileContent[LANGUAGE_MAX_CONTENT_LENGTH + 128], languageFileLines, languageFileCurrentLine;

    format(fullPath, sizeof(fullPath), "languages/%s_%s%s%s/%s", sLanguages[languageId][E_LANG_CODE], sLanguages[languageId][E_LANG_NAME], (IsNull(subDir) ? "" : "/"), subDir, file);
    format(tableName, sizeof(tableName), "%s%s%s", subDir, (IsNull(subDir) ? "" : "-"), file);

    for (new i = strlen(tableName); i > 0; i--)
    {
        if (tableName[i] == '.')
        {
            strdel(tableName, i, strlen(tableName));
            break;
        }
    }

    DB_FreeResultSet(
        DB_ExecuteQuery(
            sLanguageDBHandle,
            "CREATE TABLE IF NOT EXISTS `%s` (lang TINYINT NOT NULL, ident VARCHAR(64) NOT NULL, content VARCHAR(%d) NOT NULL, PRIMARY KEY(lang, ident));",
            tableName,
            LANGUAGE_MAX_CONTENT_LENGTH
        )
    );

    languageFile = fopen(fullPath, io_read);

    // Get file lines - used when reading and inserting data; must strip last 2 characters of each line (`\n`) but not from the last file line
    while (fread(languageFile, languageFileContent)) languageFileLines++;
    fseek(languageFile, 0, seek_start);

    while (fread(languageFile, languageFileContent))
    {
        languageFileCurrentLine++;

        // Ignore comments
        if (languageFileContent[0] == '#') continue;

        // Seperate data to variables, ignore invalid lines
        new identifier[64], content[LANGUAGE_MAX_CONTENT_LENGTH + 2], sscanfSpecifier[64];
        format(sscanfSpecifier, sizeof(sscanfSpecifier), "?<SSCANF_QUIET=1>s[64]s[%d]", LANGUAGE_MAX_CONTENT_LENGTH + 2);
        if (sscanf(languageFileContent, sscanfSpecifier, identifier, content)) continue;

        new len = strlen(content);
        if (len > LANGUAGE_MAX_CONTENT_LENGTH) PrintF("<!>[pp_language] Content length of `%s`:`%s` exceeds LANGUAGE_MAX_CONTENT_LENGTH " #LANGUAGE_MAX_CONTENT_LENGTH, tableName, identifier);

        // Strip "\n" (LF) or "\r\n" (CRLF) at end of line
        if (languageFileCurrentLine < languageFileLines)
        {
            if (content[len - 1] == '\n' && content[len - 2] == '\r') content[len - 2] = EOS;
            else content[len - 1] = EOS;
        }

    // Replace colour names with real colours, using variable gLanguageColours
        #if defined gLanguageColours
        new bool:failure = false, loopCount = 0;
        do
        {
            for (new i = 0; i < sizeof(gLanguageColours); i++)
            {
                new findPos = 1; // Start at index 1: If content would start with a colour name but without brackets, string appears to becomes empty

                while ((findPos = strfind(content, gLanguageColours[i][0], false, findPos)) != -1)
                {
                    new colourLen = strlen(gLanguageColours[i][0]), newLen = len - colourLen + 6;
                    if (newLen > LANGUAGE_MAX_CONTENT_LENGTH)
                    {
                        if (loopCount ==
                            1)
                            PrintF(
                                "<!>[pp_language] Cannot replace colour '%s' in `%s`:`%s` since that would exceed LANGUAGE_MAX_CONTENT_LENGTH " #LANGUAGE_MAX_CONTENT_LENGTH,
                                gLanguageColours[i][0],
                                tableName,
                                identifier
                            );
                        findPos += colourLen;
                        failure = true;
                        continue;
                    }
                    new endPos = findPos + strlen(gLanguageColours[i][0]);
                    if (content[findPos - 1] == '{' && content[endPos] == '}')
                    {
                        strdel(content, findPos, endPos);
                        strins(content, gLanguageColours[i][1], findPos);
                    }
                    findPos += 7;
                    len = newLen;
                }
            }
            loopCount++;
            if (!failure) loopCount = 2;
        }
        while (loopCount < 2);
    #endif
        DB_FreeResultSet(
            DB_ExecuteQuery(sLanguageDBHandle, "INSERT INTO `%s` VALUES (%d, '%s', '%q') ON CONFLICT (lang, ident) DO UPDATE SET CONTENT = '%q';", tableName, languageId, identifier, content, content)
        );
    }
    fclose(languageFile);
}

static bool:LanguageDB_Close()
{
    // Closes connection with the SQLite database
    // Returns: (bool) true: closed | false: not closed (there was no connection)
    if (sLanguageDBHandle == SQLITE_INVALID_HANDLE) return false;

    DB_Close(sLanguageDBHandle);
    sLanguageDBHandle = SQLITE_INVALID_HANDLE;
    return true;
}

static DB:LanguageDB_Open()
{
    Language_ClearCache();
    // Opens connection with the SQLite database. If there already was a connection it is closed first.
    // Deletes language database file before opening
    // Returns: (DB) Database handle ID
    if (sLanguageDBHandle != SQLITE_INVALID_HANDLE) LanguageDB_Close();

#if LANGUAGE_SQLITE_PERSISTENT_DATA
    if (fexist(LANGUAGE_SQLITE_PERSISTENT_FILE)) fremove(LANGUAGE_SQLITE_PERSISTENT_FILE);
    sLanguageDBHandle = DB_Open(LANGUAGE_SQLITE_PERSISTENT_FILE);
#else
    sLanguageDBHandle = DB_Open("_language_db", SQLITE_OPEN_MEMORY | SQLITE_OPEN_READWRITE);
#endif

    return sLanguageDBHandle;
}

forward __OnPlayerChangeLanguage(playerid);

public __OnPlayerChangeLanguage(playerid)
{
    // Called by Player_SetLanguage to make sure all scripts using pp_language update a player's language
    GetPVarString(playerid, LANGUAGE_PVAR_NAME, spLanguage[playerid]);
}

// Global functions
Language_Count()
{
    // Returns: (int) Amount of available languages
    new count;
    for (new i = 0; i < LANGUAGES_MAX; i++)
    {
        if (!IsNull(sLanguages[i][E_LANG_CODE]))
        {
            count++;
            continue;
        }
        break;
    }
    return count;
}

stock string:Language_Default()
return sDefaultLanguage;

bool:Language_GetDataFromID(const languageId, string:languageCode[], string:languageName[], const lenLanguageCode = sizeof(languageCode), const lenLanguageName = sizeof(languageName))
{
    // Gets language code and name from language ID.
    //  `languageId`: Language ID
    //  `string:languageCode[]`: (reference) Array to store language code in
    //  `string:languageName[]`: (reference) Array to store language name in
    //  `lenLanguageCode`: Size of languageCode[]
    //  `lenLanguageName`: Size of languageName[]
    // Returns: (bool) true: Data was found | false: Language doesn't exist (invalid array ID, or language simply doesn't exist)
    if (!(0 <= languageId < LANGUAGES_MAX)) return false;
    if (IsNull(sLanguages[languageId][E_LANG_CODE])) return false;

    format(languageCode, lenLanguageCode, sLanguages[languageId][E_LANG_CODE]);
    format(languageName, lenLanguageName, sLanguages[languageId][E_LANG_NAME]);
    return true;
}

Language_GetIDFromData(const string:search[])
{
    // Gets language ID from language code or language name. Must match exactly except for case (case insensitive)
    //  `search[]`: Language code or name to get the ID from
    // Returns: (int) Language ID or LANGUAGE_INVALID_ID if it doesn't exist
    for (new i = 0; i < LANGUAGES_MAX; i++)
    {
        if (!strcmp(sLanguages[i][E_LANG_CODE], search, true) || !strcmp(sLanguages[i][E_LANG_NAME], search, true)) return i;
    }
    return LANGUAGE_INVALID_ID;
}

stock bool:Language_Exists(const string:search[])
{
    // Checks if a language exists
    //  `search[]`: Language code or name to check
    // Returns: (bool) true: Language exists | false: Language does not exist
    return Language_GetIDFromData(search) != LANGUAGE_INVALID_ID;
}

bool:LanguageDB_Build()
{
// (Re-)builds the entire language database for the current script.
    // Returns: (bool) true: Success | false: Fail (Directory "scriptfiles/languages" doesn't exist)
    // <!> Function will return true even if no languages exist
    #if !defined LANGUAGE_NO_BUILD_MESSAGE
    Print("===== Building language database =====");
#endif
    new wipe_sLanguages[LANGUAGES_MAX][E_LANG_DATA];
    sLanguages = wipe_sLanguages;
    LanguageDB_Open(); // Already closes connection if it exists and wipes data.

    if (!Language_ScanLanguages())
    {
        LanguageDB_Close();
        return false;
    }
    new languageCount = Language_Count();
#if !defined LANGUAGE_NO_BUILD_MESSAGE
    PrintF("Found %d language(s):", languageCount);
    for (new i = 0; i < languageCount; i++)
    {
        new languageCode[3], languageName[32];
        Language_GetDataFromID(i, languageCode, languageName);
        PrintF("\t%d: (%s) %s", i, languageCode, languageName);
    }

    Print("> Adding language files to language database");
#endif
    for (new i = 0; i < languageCount; i++)
    {
    #if !defined LANGUAGE_NO_BUILD_MESSAGE
        PrintF(">> Adding data for language \"%s\"...", sLanguages[i][E_LANG_NAME]);
    #endif

        new languageCode[3], languageName[32];
        Language_GetDataFromID(i, languageCode, languageName);

        new dir:languageDirRoot, dirRoot[64], dirItemRoot[64], dirItemTypeRoot;
        format(dirRoot, sizeof(dirRoot), "scriptfiles/languages/%s_%s", languageCode, languageName);
        languageDirRoot = dir_open(dirRoot);
        while (dir_list(languageDirRoot, dirItemRoot, dirItemTypeRoot))
        {
            if (!strcmp(dirItemRoot, ".") || !strcmp(dirItemRoot, "..")) continue;

            if (dirItemTypeRoot == FM_DIR)
            {
                new dir:languageDirSub, dirSub[100], dirItemSub[64], dirItemTypeSub;
                format(dirSub, sizeof(dirSub), "%s/%s", dirRoot, dirItemRoot);
                languageDirSub = dir_open(dirSub);
                while (dir_list(languageDirSub, dirItemSub, dirItemTypeSub))
                {
                    if (dirItemTypeSub == FM_FILE) Language_AddDataFromFile(i, dirItemSub, dirItemRoot);
                }
                dir_close(languageDirSub);
            }
            else Language_AddDataFromFile(i, dirItemRoot);
        }
        dir_close(languageDirRoot);
    }

#if !defined LANGUAGE_NO_BUILD_MESSAGE
    Print("===== Language database built =====");
#endif
    return true;
}

forward __LanguageDB_BuildSingleTable(const string:file[], const string:subDir[], const string:tableName[]);

public __LanguageDB_BuildSingleTable(const string:file[], const string:subDir[], const string:tableName[])
{
    Language_ClearCache();
#if !defined LANGUAGE_NO_BUILD_MESSAGE
    new languageCount, buildCount;
#endif

    for (new i = 0; i < LANGUAGES_MAX; i++)
    {
        if (!IsNull(sLanguages[i][E_LANG_CODE]))
        {
        #if !defined LANGUAGE_NO_BUILD_MESSAGE
            languageCount++;
        #endif

            new fullPath[128];
            format(fullPath, sizeof(fullPath), "languages/%s_%s/%s%s%s", sLanguages[i][E_LANG_CODE], sLanguages[i][E_LANG_NAME], subDir, (!IsNull(subDir) ? "/" : ""), file);

            if (!fexist(fullPath))
            {
                PrintF(
                    "<!>[pp_language] Cannot (re-)build table `%s` for language `%s (%s)`:\n\tFile `%s` does not exist",
                    tableName,
                    sLanguages[i][E_LANG_NAME],
                    sLanguages[i][E_LANG_CODE],
                    fullPath
                );
                continue;
            }

            Language_AddDataFromFile(i, file, subDir);
        #if !defined LANGUAGE_NO_BUILD_MESSAGE
            buildCount++;
        #endif
        }
    }

#if !defined LANGUAGE_NO_BUILD_MESSAGE
    if (buildCount == languageCount) PrintF("[pp_language] Successfully completed (re-)building table `%s`", tableName);
#endif
}

stock void:LanguageDB_BuildSingleTable(const string:file[], const string:subDir[] = "")
{
    // (Re-)builds a single table for all running scripts.
    //  `file[]`: Full name of the file (including file extension)
    //  `subDir[]`: (optional) Directory the file is in. Ommit if it's in root dir.
    // `scriptfiles/languages/en_English/foo.txt` : LanguageDB_BuildSingleTable("foo.txt");
    // `scriptfiles/languages/en_English/global/foo.txt`: LanguageDB_BuildSingleTable("foo.txt", "global");
    // Returns: (nothing)
    new tableName[32];
    format(tableName, sizeof(tableName), "%s%s%s", subDir, (!IsNull(subDir) ? "-" : ""), file);
    for (new i = strlen(tableName); i > 0; i--)
    {
        if (tableName[i] == '.')
        {
            strdel(tableName, i, strlen(tableName));
            break;
        }
    }
#if !defined LANGUAGE_NO_BUILD_MESSAGE
    PrintF("[pp_language] (Re-)building table `%s`", tableName);
#endif
    CallRemoteFunction("__LanguageDB_BuildSingleTable", "sss", file, subDir, tableName);
}

// Returns an independent, unformatted PawnPlus string. Percent placeholders stay intact.
stock String:Language_Template(const languageCode[], const table[], const identifier[])
{
    if (!map_valid(sLanguageTemplateCache)) Language_ClearCache();
    new String:key = str_format("%d:%s%d:%s%d:%s", strlen(languageCode), languageCode, strlen(table), table, strlen(identifier), identifier);
    if (map_has_str_s_key(sLanguageTemplateCache, key))
    {
        new String:cached = map_str_s_get_str_s(sLanguageTemplateCache, key);
        str_delete(key);
        return cached;
    }
    new output[LANGUAGE_MAX_CONTENT_LENGTH],
        DBResult:result = DB_ExecuteQuery(sLanguageDBHandle, "SELECT content FROM `%s` WHERE ident = '%q' AND lang = %d;", table, identifier, Language_GetIDFromData(languageCode));

    // A locale may be intentionally incomplete while it is being translated.  In
    // that case, use the server's default language rather than exposing pp_language's
    // diagnostic marker to the player.  Keep the marker when the identifier is also
    // absent from the default locale so genuinely invalid lookups remain visible.
    if (!DB_GetRowCount(result) && strcmp(languageCode, sDefaultLanguage, true))
    {
        DB_FreeResultSet(result);
        result = DB_ExecuteQuery(sLanguageDBHandle, "SELECT content FROM `%s` WHERE ident = '%q' AND lang = %d;", table, identifier, Language_GetIDFromData(sDefaultLanguage));
    }
    if (DB_GetRowCount(result))
    {
        DB_GetFieldString(result, 0, output);

        // Replace "\n" and "\t" (they are otherwise string literals)
        new findPos = 0;
        while ((findPos = strfind(output, "\\n", false, findPos)) != -1)
        {
            strdel(output, findPos, findPos + 2);
            strins(output, "\n", findPos);
        }
        findPos = 0;
        while ((findPos = strfind(output, "\\t", false, findPos)) != -1)
        {
            strdel(output, findPos, findPos + 2);
            strins(output, "\t", findPos);
        }
    }
    else format(output, sizeof(output), "[%s]%s:%s", languageCode, table, identifier);
    DB_FreeResultSet(result);
    map_str_s_add_str(sLanguageTemplateCache, key, output);
    str_delete(key);
    return str_new(output);
}

// Dynamic APIs use PawnPlus formatting: positional placeholders, %S, variants,
// custom format selectors and locale directives. Returned strings follow normal
// PawnPlus temporary-string ownership; acquire/release to keep across callbacks.
stock String:Language_GetString(const languageCode[], const table[], const identifier[], AnyTag:...)
{
    new String:template = Language_Template(languageCode, table, identifier);
    new List:arguments = PP_GetArgumentReferences(3);
    new result;
    new amx_err:error = pawn_try_call_native("str_format_s", result, "il", template, arguments);
    list_delete(arguments);
    if (error != amx_err_none)
    {
        printf("[pp_language] Formatting failed for %s:%s (error %d)", table, identifier, _:error);
        return template;
    }
    str_delete(template);
    return String:result;
}

stock String:Player_Language_GetString(playerid, const table[], const identifier[], AnyTag:...)
{
    new String:template = Language_Template(Player_GetLanguage(playerid), table, identifier);
    new List:arguments = PP_GetArgumentReferences(3);
    new result;
    new amx_err:error = pawn_try_call_native("str_format_s", result, "il", template, arguments);
    list_delete(arguments);
    if (error != amx_err_none)
    {
        printf("[pp_language] Formatting failed for %s:%s (error %d)", table, identifier, _:error);
        return template;
    }
    str_delete(template);
    return String:result;
}

// Named values can be referenced by PawnPlus expressions in a trusted template.
// The caller owns the map; the formatting environment is restored even on error.
stock String:Language_FormatMap(const languageCode[], const table[], const identifier[], Map:values)
{
    new String:template = Language_Template(languageCode, table, identifier);
    pp_format_env_push(values);
    new result;
    new format_error_text[192];
    new amx_err:error = pawn_try_call_native_msg("str_format_s", result, format_error_text, sizeof(format_error_text), "i", template);
    pp_format_env_pop();
    if (error != amx_err_none)
    {
        printf("[pp_language] Named formatting failed for %s:%s: %s", table, identifier, format_error_text);
        return template;
    }
    str_delete(template);
    return String:result;
}

stock string:Language_Get(const string:languageCode[], const string:table[], const string:identifier[], OPEN_MP_TAGS:...)
{
    new output[LANGUAGE_MAX_CONTENT_LENGTH];
    new String:template = Language_Template(languageCode, table, identifier);
    str_get(template, output);
    str_delete(template);
    if (numargs() > 3)
    {
        PP_Format(output, sizeof(output), output, 3);
    }
    else
    {
        // Some callers retrieve a localized format template and apply
        // its arguments in a surrounding format call.  Formatting it
        // here without variadic arguments corrupts placeholders such
        // as %s and %dh before that caller can use them.  Preserve
        // those templates, while retaining the existing conversion
        // of escaped literal percent signs (%%) for plain strings.
        new bool:has_format_specifier = false;
        for (new format_index = 0; output[format_index] != '\0'; format_index++)
        {
            if (output[format_index] != '%') continue;
            if (output[format_index + 1] == '%')
            {
                format_index++;
                continue;
            }

            has_format_specifier = true;
            break;
        }

        if (!has_format_specifier)
        {
            format(output, sizeof(output), output);
        }
    }
    return output;
}

stock bool:Player_SetLanguage(const playerid, const string:language[])
{
    // Sets the language of a player
    //  `playerid`: Player to set the language for
    //  `language[]`: Language to set, using either language code or language name
    // Returns: (bool) true: Language was changed | false: Language wasn't changed: Language doesn't exist
    if (!IsPlayerConnected(playerid)) return false;

    new languageId = Language_GetIDFromData(language);
    if (languageId == LANGUAGE_INVALID_ID) return false;

    new languageCode[3], languageName[1];
    Language_GetDataFromID(languageId, languageCode, languageName);
    format(spLanguage[playerid], sizeof(spLanguage[]), languageCode);
    SetPVarString(playerid, LANGUAGE_PVAR_NAME, spLanguage[playerid]);
    CallRemoteFunction("__OnPlayerChangeLanguage", "d", playerid);
    return true;
}

stock bool:Player_HasSelectedLanguage(const playerid)
{
    // Checks if a player selected a language
    //  `playerid`: Player to check
    // Returns: (bool) true: Player has selected a language | false: Player did not select a language
    return !IsNull(spLanguage[playerid]);
}

stock string:Player_GetLanguage(const playerid)
{
    // Retrieves the language of a player
    //  `playerid`: Player to get the language from
    // Returns: (string) Language code of player, or the server's default language if no language was selected yet.
    if (!IsPlayerConnected(playerid)) return sDefaultLanguage;

    if (IsNull(spLanguage[playerid]))
    {
        if (GetPVarType(playerid, LANGUAGE_PVAR_NAME) == PLAYER_VARTYPE_STRING) GetPVarString(playerid, LANGUAGE_PVAR_NAME, spLanguage[playerid]);
        else return sDefaultLanguage;
    }
    return spLanguage[playerid];
}

// Format using the arguments of the function calling this helper.
stock string:PPLanguage_Forward(const language[], const table[], const identifier[], first)
{
    new output[LANGUAGE_MAX_CONTENT_LENGTH];
    format(output, sizeof(output), "%s", Language_Get(language, table, identifier));
    PP_Format(output, sizeof(output), output, first, 2);
    return output;
}

stock string:Player_Language_Get(const playerid, const string:table[], const string:identifier[], OPEN_MP_TAGS:...)
{
    // Retrieves a string from the language database, formats it (if needed) and returns it
    //  `playerid`: Player to retrieve the string for (used to determine what language to fetch)
    //  `table[]`: Table to get the content from
    //  `identifier[]`: Identifier from given table to retrieve
    //  `OPEN_MP_TAGS:...` (optional) Used for formatting format specifiers used in retrieved content (formatted by PP_Format)
    // Returns: (string) Retrieved, formatted language string
    //         Missing entries fall back to the server's default language. If the
    //         entry is also missing there, returns `[languageCode]table:identifier`.
    return PPLanguage_Forward(Player_GetLanguage(playerid), table, identifier, 3);
}

stock bool:SendClientLanguageMessage(const playerid, const colour, const string:table[], const string:identifier[], OPEN_MP_TAGS:...)
{
    // Sends a client message to a player using the language system
    //  `playerid`: Player to send the message to
    //  `colour`: Colour of the message
    //  `table[]`: Table to get the content from
    //  `identifier[]`: Identifier from given table to retrieve
    //  `OPEN_MP_TAGS:...` (optional) Used for formatting format specifiers used in retrieved content (formatted by PP_Format)
    // Returns: (bool) Output of SendClientMessage (thus true: message sent, false: message not sent, player not connected)
    return SendClientMessage(playerid, colour, PPLanguage_Forward(Player_GetLanguage(playerid), table, identifier, 4));
}

stock bool:SendClientLanguageMessageToAll(const colour, const string:table[], const string:identifier[], OPEN_MP_TAGS:...)
{
    // Sends a client message to all players using the language system
    //  `colour`: Colour of the message
    //  `table[]`: Table to get the content from
    //  `identifier[]`: Identifier from given table to retrieve
    //  `OPEN_MP_TAGS:...` (optional) Used for formatting format specifiers used in retrieved content (formatted by PP_Format)
    // Returns: (bool) Always returns true
    new fetchedData[LANGUAGES_MAX][LANGUAGE_MAX_CONTENT_LENGTH];

    // Note that I didn't use array size 128 (which is limit for text output in open.mp)
    // Some people may have a script that breaks longer messages in multiple messages, so that's why
    for (new i = 0; i < LANGUAGES_MAX; i++)
    {
        if (IsNull(sLanguages[i][E_LANG_NAME])) break;

        format(fetchedData[i], LANGUAGE_MAX_CONTENT_LENGTH, "%s", PPLanguage_Forward(sLanguages[i][E_LANG_CODE], table, identifier, 3));
    }

    for (new i = 0; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i)) SendClientMessage(i, colour, fetchedData[Language_GetIDFromData(Player_GetLanguage(i))]);
    return true;
}

stock PlayerTextDrawLanguageString(const playerid, PlayerText:textid, const string:table[], const string:identifier[], OPEN_MP_TAGS:...)
{
    // Sets a player-textdraw string using the language system
    //  `playerid`: Player who owns the textdraw
    //  `textid`: The PlayerTextDraw to set the string for
    //  `table[]`: Table to get the content from
    //  `identifier[]`: Identifier from given table to retrieve
    //  `OPEN_MP_TAGS:...` (optional) Used for formatting format specifiers used in retrieved content (formatted by PP_Format)
    // Returns: Always returns 1
    PlayerTextDrawSetString(playerid, textid, PPLanguage_Forward(Player_GetLanguage(playerid), table, identifier, 4));
    return 1;
}

stock TextDrawLanguageStringForPlayer(const playerid, Text:textid, const string:table[], const string:identifier[], OPEN_MP_TAGS:...)
{
    // Sets a textdraw string for a specific player using the language system
    //  `playerid`: Player to show the textdraw to
    //  `textid`: The TextDraw to set the string for
    //  `table[]`: Table to get the content from
    //  `identifier[]`: Identifier from given table to retrieve
    //  `OPEN_MP_TAGS:...` (optional) Used for formatting format specifiers used in retrieved content (formatted by PP_Format)
    // Returns: Always returns 1
    TextDrawSetStringForPlayer(textid, playerid, PPLanguage_Forward(Player_GetLanguage(playerid), table, identifier, 4));
    return 1;
}

stock PlayerText:CreatePlayerLanguageTextDraw(const playerid, Float:x, Float:y, const string:table[], const string:identifier[], OPEN_MP_TAGS:...)
{
    // Creates a player-textdraw using the language system
    //  `playerid`: Player to create the textdraw for
    //  `Float:x`: X-Coordinate
    //  `Float:y`: Y-Coordinate
    //  `table[]`: Table to get the content from
    //  `identifier[]`: Identifier from given table to retrieve
    //  `OPEN_MP_TAGS:...` (optional) Used for formatting format specifiers used in retrieved content (formatted by PP_Format)
    // Returns: PlayerText:The ID of the created textdraw
    return CreatePlayerTextDraw(playerid, x, y, PPLanguage_Forward(Player_GetLanguage(playerid), table, identifier, 5));
}

stock bool:GameLanguageTextForPlayer(const playerid, const string:table[], const string:identifier[], const time, const style, OPEN_MP_TAGS:...)
{
    // Sends a game text to a player using the language system
    //  `playerid`: Player to to show the gametext for
    //  `table[]`: Table to get the content from
    //  `identifier[]`: Identifier from given table to retrieve
    //  `time`: The duration of the text being shown in milliseconds
    //  `style`: The style of text to be displayed
    //  `OPEN_MP_TAGS:...` (optional) Used for formatting format specifiers used in retrieved content (formatted by PP_Format)
    // Returns: (int) output of GameTextForPlayer(), thus 1: Function executed or 0: Player not connected or string (retrieved from table:identifier) is null
    return GameTextForPlayer(playerid, PPLanguage_Forward(Player_GetLanguage(playerid), table, identifier, 5), time, style);
}

stock void:GameLanguageTextForAll(const string:table[], const string:identifier[], const time, const style, OPEN_MP_TAGS:...)
{
    // Shows a game text to all players using the language system
    //  `table[]`: Table to get the content from
    //  `identifier[]`: Identifier from given table to retrieve
    //  `time`: The duration of the text being shown in milliseconds
    //  `style`: The style of text to be displayed
    //  `OPEN_MP_TAGS:...` (optional) Used for formatting format specifiers used in retrieved content (formatted by PP_Format)
    // Returns: Nothing
    new fetchedData[LANGUAGES_MAX][LANGUAGE_MAX_CONTENT_LENGTH];

    // Default LANGUAGE_MAX_CONTENT_LENGTH may be too much for a gametext.
    // Since I couldn't find limits on gametext lengths (except using colour codes beyond the 255th character may crash a client), just going with LANGUAGE_MAX_CONTENT_LENGTH
    for (new i = 0; i < LANGUAGES_MAX; i++)
    {
        if (IsNull(sLanguages[i][E_LANG_NAME])) break;

        format(fetchedData[i], LANGUAGE_MAX_CONTENT_LENGTH, "%s", PPLanguage_Forward(sLanguages[i][E_LANG_CODE], table, identifier, 4));
    }

    for (new i = 0; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i)) GameTextForPlayer(i, fetchedData[Language_GetIDFromData(Player_GetLanguage(i))], time, style);
}

stock bool:SendPlayerLanguageMessageToAll(const senderid, const string:table[], const string:identifier[], OPEN_MP_TAGS:...)
{
    // Sends a message in the name of a player to all other players on the server using the language system
    //  `senderid`: The ID of the sender. If invalid, the message will not be sent
    //  `table[]`: Table to get the content from
    //  `identifier[]`: Identifier from given table to retrieve
    //  `OPEN_MP_TAGS:...` (optional) Used for formatting format specifiers used in retrieved content (formatted by PP_Format)
    // Returns: Always returns true
    new fetchedData[LANGUAGES_MAX][LANGUAGE_MAX_CONTENT_LENGTH];

    for (new i = 0; i < LANGUAGES_MAX; i++)
    {
        if (IsNull(sLanguages[i][E_LANG_NAME])) break;

        format(fetchedData[i], LANGUAGE_MAX_CONTENT_LENGTH, "%s", PPLanguage_Forward(sLanguages[i][E_LANG_CODE], table, identifier, 3));
    }

    for (new i = 0; i < MAX_PLAYERS; i++) if (IsPlayerConnected(i)) SendPlayerMessageToPlayer(i, senderid, fetchedData[Language_GetIDFromData(Player_GetLanguage(i))]);
    return true;
}

stock bool:SendPlayerLanguageMessageToPlayer(const playerid, const senderid, const string:table[], const string:identifier[], OPEN_MP_TAGS:...)
{
    // Sends a message in the name of a player to another player on the server using the language system
    //  `playerid`: The ID of the player who will receive the message.
    //  `senderid`: The ID of the sender. If invalid, the message will not be sent
    //  `table[]`: Table to get the content from
    //  `identifier[]`: Identifier from given table to retrieve
    //  `OPEN_MP_TAGS:...` (optional) Used for formatting format specifiers used in retrieved content (formatted by PP_Format)
    // Returns: Output of SendPlayerMessageToPlayer(), thus true: function executed successfully or false: Failed ot execute (specified player does not exist)
    return SendPlayerMessageToPlayer(playerid, senderid, PPLanguage_Forward(Player_GetLanguage(playerid), table, identifier, 4));
}

// native PlayerText3D:CreatePlayer3DTextLabel(playerid, const text[], colour, Float:x, Float:y, Float:z, Float:drawDistance, parentPlayerid = INVALID_PLAYER_ID, parentVehicleid = INVALID_VEHICLE_ID, bool:testLOS = false, OPEN_MP_TAGS:...);
// native bool:UpdatePlayer3DTextLabelText(playerid, PlayerText3D:textid, colour, const text[], OPEN_MP_TAGS:...);
stock PlayerText3D:CreatePlayer3DLanguageTextLabel(
    const playerid,
    const string:table[],
    const string:identifier[],
    const colour,
    const Float:x,
    const Float:y,
    const Float:z,
    const Float:drawDistance,
    const parentPlayerid = INVALID_PLAYER_ID,
    const parentVehicleid = INVALID_VEHICLE_ID,
    const bool:testLOS = false,
    OPEN_MP_TAGS:...
)
{
    // Creates a 3D text label for a specific player using the language system
    //  `playerid`: The ID of the player to create the 3D text label for
    //  `table[]`: Table to get the content from
    //  `identifier[]`: Identifier from given table to retrieve
    //  `colour`: The text colour
    //  `Float:x`: X coordinate (or offset if attached)
    //  `Float:y`: Y coordinate (or offset if attached)
    //  `Float:z`: Z coordinate (or offset if attached)
    //  `Float:drawDistance`: The distance where you are able to see the 3D Text Label
    //  `parentPlayerid`: The player you want to attach the 3D Text Label to (None: INVALID_PLAYER_ID)
    //  `parentVehicleid`: The vehicle you want to attach the 3D Text Label to (None: INVALID_VEHICLE_ID)
    //  `testLOS`: Test the line of sight (true) so this text can't be seen through walls - or not (false)
    //  `OPEN_MP_TAGS:...` (optional) Used for formatting format specifiers used in retrieved content (formatted by PP_Format)
    // Returns: Output of CreatePlayer3DTextLabel(): ID of the newly created label or INVALID_3DTEXT_ID if the Player 3D Text Label limit (MAX_3DTEXT_PLAYER) was reached
    return CreatePlayer3DTextLabel(playerid, PPLanguage_Forward(Player_GetLanguage(playerid), table, identifier, 11), colour, x, y, z, drawDistance, parentPlayerid, parentVehicleid, testLOS);
}

stock bool:UpdatePlayer3DLanguageTextLabelText(const playerid, const PlayerText3D:textid, const colour, const string:table[], const string:identifier[], OPEN_MP_TAGS:...)
{
    // Updates a player 3D Text Label's text and colour using the language system
    //  `playerid`: The ID of the player for which the 3D Text Label was created
    //  `textid`: The 3D Text Label you want to update
    //  `colour`: The color the 3D Text Label should have from now on
    //  `table[]`: Table to get the content from
    //  `identifier[]`: Identifier from given table to retrieve
    //  `OPEN_MP_TAGS:...` (optional) Used for formatting format specifiers used in retrieved content (formatted by PP_Format)
    // Returns: Always return true
    UpdatePlayer3DTextLabelText(playerid, textid, colour, PPLanguage_Forward(Player_GetLanguage(playerid), table, identifier, 5));
    return true;
}

stock void:Player_SelectLanguage(const playerid)
{
    // Shows dialog with available languages. When selected, change player's language to that language.
    // If only one language is available, language is set to that language.
    //   `playerid`: Player to show the dialog to
    // Returns: Nothing
    if (!IsPlayerConnected(playerid)) return;

    new languageCount = Language_Count();
    if (languageCount > 1)
    {
        new dialogStr[64 * LANGUAGES_MAX];
        for (new i = 0; i < languageCount; i++)
        {
            new languageCode[3], languageName[32];
            if (Language_GetDataFromID(
                    i,
                    languageCode,
                    languageName
                )) format(dialogStr, sizeof(dialogStr), "%s- %s (%s)%s", dialogStr, languageCode, languageName, ((i == languageCount - 1) ? "" : "\n"));
        }

        ShowPlayerDialog(
            playerid,
            DIALOG_SELECT_LANGUAGE,
            DIALOG_STYLE_LIST,
            Player_Language_Get(playerid, DEFAULT_MESSAGES_FILE_NAME, "DIALOG_TITLE"),
            dialogStr,
            Player_Language_Get(playerid, DEFAULT_MESSAGES_FILE_NAME, "DIALOG_BUTTON_SELECT"),
            Player_Language_Get(playerid, DEFAULT_MESSAGES_FILE_NAME, "DIALOG_BUTTON_CANCEL")
        );
    }
    else
    {
        Player_SetLanguage(playerid, sDefaultLanguage);
        CallRemoteFunction("OnPlayerSelectLanguage", "dd", playerid, -1);
    }
}

/*-- Callbacks --*/
forward OnPlayerSelectLanguage(playerid, response);

#if defined FILTERSCRIPT
    hook OnFilterScriptInit()
#else
    hook OnGameModeInit()
#endif
{
    LanguageDB_Build();

    if (Language_Count() == 0)
    {
        PrintF("\n<!> <!> <!> <!> <!> <!> <!> No languages have been added!\n");
    }
    else
    {
        if (!Language_Exists(LANGUAGE_DEFAULT))
        {
            PrintF(
                "\n<!> <!> <!> <!> <!> <!> <!> The default language \"%s\" (LANGUAGE_DEFAULT) does not exist! Setting default language to \"%s (%s)\"!",
                LANGUAGE_DEFAULT,
                sLanguages[0][E_LANG_NAME],
                sLanguages[0][E_LANG_CODE]
            );
            format(sDefaultLanguage, sizeof(sDefaultLanguage), sLanguages[0][E_LANG_CODE]);
        }
        else format(sDefaultLanguage, sizeof(sDefaultLanguage), LANGUAGE_DEFAULT);
    }

    return 0;
}

hook OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
    if (dialogid == DIALOG_SELECT_LANGUAGE)
    {
        if (response)
        {
            new languageCode[3], languageName[32];
            Language_GetDataFromID(listitem, languageCode, languageName);
            Player_SetLanguage(playerid, languageCode);
            SendClientLanguageMessage(playerid, -1, DEFAULT_MESSAGES_FILE_NAME, "MSG_SELECTED_LANGUAGE", languageName);
        }
        CallRemoteFunction("OnPlayerSelectLanguage", "dd", playerid, response);
        return 1;
    }

    return 0;
}

hook OnPlayerDisconnect(playerid, reason)
{
    spLanguage[playerid][0] = EOS;

    return 0;
}

#if defined FILTERSCRIPT
    hook OnFilterScriptExit()
#else
    hook OnGameModeExit()
#endif
{
    if (map_valid(sLanguageTemplateCache)) map_delete(sLanguageTemplateCache);
    sLanguageTemplateCache = Map:0;
    LanguageDB_Close();
    return 0;
}
