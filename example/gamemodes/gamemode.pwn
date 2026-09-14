#pragma dynamic 10000
#include <pp_language>

main() {}

public OnPlayerConnect(playerid)
{
    new name[MAX_PLAYER_NAME], output[144];
    GetPlayerName(playerid, name, sizeof(name));
    new String:player_name = str_new(name);
    new String:message = Player_Language_GetString(playerid, "pawnplus", "WELCOME", player_name);
    str_get(message, output);
    SendClientMessage(playerid, -1, "%s", output);
    Player_SelectLanguage(playerid);
    return 1;
}

public OnPlayerSelectLanguage(playerid, response)
{
    if (!response) return 1;
    new Map:values = map_new();
    map_str_add(values, "amount", 42);
    new String:message = Language_FormatMap(Player_GetLanguage(playerid), "pawnplus", "TOTAL", values);
    new output[144];
    str_get(message, output);
    SendClientMessage(playerid, -1, "%s", output);
    map_delete(values);
    return 1;
}
