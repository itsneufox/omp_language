#pragma dynamic 16384
#include <pp_language>

new failures;
new String:retained;

stock Check(bool:condition, const description[])
{
    if (!condition)
    {
        failures++;
        printf("FAIL: %s", description);
    }
}

stock CheckFormat(const expected[], const template[], OPEN_MP_TAGS:...)
{
    new output[128];
    PP_Format(output, sizeof(output), template, 2);
    Check(!strcmp(output, expected), output);
}

stock ArrayFormat(const template[], OPEN_MP_TAGS:...)
{
    new output[128];
    PP_Format(output, sizeof(output), template, 1);
    return output;
}

stock ForwardFormat(const template[], first)
{
    new output[128];
    PP_Format(output, sizeof(output), template, first, 2);
    return output;
}

stock CheckForward(const expected[], const template[], OPEN_MP_TAGS:...)
{
    Check(!strcmp(ForwardFormat(template, 2), expected), "nested caller formatting");
}


new unique_hook_sequence;

#include <pp_hooks>
hook UniqueHookTest(value)
{
    unique_hook_sequence = value;
    return 0;
}

#include <pp_hooks>
hook ret UniqueHookTest(&ret, value)
{
    unique_hook_sequence += value;
    ret = 31;
    return 1;
}

#include <pp_hooks>
hook UniqueHookTest(value)
{
    unique_hook_sequence += 100 * value;
    return 0;
}

main() {}

public OnGameModeInit()
{
    Check(pawn_call_public("UniqueHookTest", "i", 7) == 31 && unique_hook_sequence == 14, "automatic unique names preserve mixed hook order and stopping");
    CheckFormat("hello", "hello");
    CheckFormat("name=Alice n=42 f=1.25 %", "name=%s n=%d f=%.2f %%", "Alice", 42, 1.25);
    Check(!strcmp(ArrayFormat("%s:%d", "nested", 7), "nested:7"), "array-return variadic formatting");
    CheckFormat("%d remains a template", "%d remains a template");
    CheckForward("Alice has 42", "%s has %d", "Alice", 42);
    Check(!strcmp(Language_Get("en", "tests", "FORMAT", "Alice", 42, 1.25), "Alice has 42 at 1.25%."), "localized variadic formatting");
    Check(!strcmp(Player_Language_Get(INVALID_PLAYER_ID, "tests", "FORMAT", "Bob", 7, 2.5), "Bob has 7 at 2.50%."), "player localization forwarding");
    Check(!strcmp(Language_Get("pt", "tests", "FORMAT", "Alice", 42, 1.25), "Alice has 42 at 1.25%."), "missing translation falls back");
    Check(!strcmp(Language_Get("en", "tests", "PLAIN"), "100% ready"), "escaped percent without arguments");
    new template_result[LANGUAGE_MAX_CONTENT_LENGTH];
    format(template_result, sizeof(template_result), "%s", Language_Get("en", "tests", "TEMPLATE"));
    Check(!strcmp(template_result, "%s owes %d"), "localized template remains unformatted");

    new String:dynamic_name = str_new("Alice");
    new String:dynamic_text = Language_GetString("en", "tests", "DYNAMIC", dynamic_name, 42, 1.25);
    new dynamic_output[128];
    str_get(dynamic_text, dynamic_output);
    Check(!strcmp(dynamic_output, "Alice has 42 at 1.25%."), "PawnPlus dynamic string and positional formatting");
    str_delete(dynamic_text);
    str_delete(dynamic_name);
    dynamic_text = Player_Language_GetString(INVALID_PLAYER_ID, "tests", "FORMAT", "Bob", 7, 2.5);
    str_get(dynamic_text, dynamic_output);
    Check(!strcmp(dynamic_output, "Bob has 7 at 2.50%."), "PawnPlus player formatting fallback");
    str_delete(dynamic_text);
    new Map:values = map_new();
    map_str_add(values, "amount", 42);
    dynamic_text = Language_FormatMap("en", "tests", "NAMED", values);
    str_get(dynamic_text, dynamic_output);
    Check(!strcmp(dynamic_output, "Total: 42"), "named translation values");
    str_delete(dynamic_text);
    map_delete(values);
    new cached_count = Language_CacheSize();
    dynamic_text = Language_Template("en", "tests", "FORMAT");
    str_delete(dynamic_text);
    Check(Language_CacheSize() == cached_count, "repeated translation uses cache");
    Language_ClearCache();
    Check(Language_CacheSize() == 0, "translation cache can be cleared");
    dynamic_text = Language_Template("pt", "tests", "FORMAT");
    str_get(dynamic_text, dynamic_output);
    Check(!strcmp(dynamic_output, "%s has %d at %.2f%%."), "raw fallback template retains percent escapes");
    str_delete(dynamic_text);


    retained = str_acquire(Language_Template("en", "tests", "FORMAT"));
    new String:changed = Language_Template("en", "tests", "FORMAT");
    str_set_format(changed, "changed");
    str_delete(changed);
    changed = Language_Template("en", "tests", "FORMAT");
    str_get(changed, dynamic_output);
    Check(!strcmp(dynamic_output, "%s has %d at %.2f%%."), "caller mutation does not alter cached template");
    str_delete(changed);

    new String:long_name = str_new("");
    str_resize(long_name, 1400, 'x');
    changed = Language_GetString("en", "tests", "LONG", long_name);
    Check(str_len(changed) == 1400, "dynamic result exceeds legacy array limit");
    str_delete(changed);
    str_delete(long_name);

    new File:translation = fopen("languages/en_English/tests.txt", io_write);
    fwrite(translation, "FORMAT Updated %s %d %.2f\n# end\n");
    fclose(translation);
    __LanguageDB_BuildSingleTable("tests.txt", "", "tests");
    Check(Language_CacheSize() == 0, "single-table reload invalidates cache");
    Check(!strcmp(Language_Get("en", "tests", "FORMAT", "Alice", 7, 2.5), "Updated Alice 7 2.50"), "single-table reload returns new content");
    LanguageDB_Build();
    Check(Language_CacheSize() == 0, "full reload invalidates cache");
    SetTimer("FinishTests", 1, false);
    return 1;
}

forward FinishTests();
public FinishTests()
{
    new output[128];
    str_get(retained, output);
    Check(!strcmp(output, "%s has %d at %.2f%%."), "acquired string survives reload and callback boundary");
    str_release(retained);
    printf("PP_LANGUAGE_TEST_RESULT failures=%d", failures);
    SendRconCommand("exit");
    return 1;
}
