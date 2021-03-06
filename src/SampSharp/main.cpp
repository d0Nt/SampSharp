// SampSharp
// Copyright 2017 Tim Potze
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <fstream>
#include <assert.h>
#include <string.h>
#include <iostream>
#include <sampgdk/sampgdk.h>
#include "StringUtil.h"
#include "server.h"
#include "version.h"
#include "pipesvr_win32.h"
#include "dsock_unix.h"
#include "tcp_unix.h"
#include "plugin.h"

using sampgdk::logprintf;

server *svr = NULL;
commsvr *com = NULL;
plugin *plg = NULL;

bool ready;

void print_err(const char* error) {
    logprintf("[SampSharp:ERROR] %s", error);
}

void print_info() {
    logprintf("");
    logprintf("SampSharp Plugin");
    logprintf("----------------");
    logprintf("v%s, (C)2014-2017 Tim Potze", PLUGIN_VERSION_STR);
    logprintf("");
}

bool config_validate() {
    /* check whether gamemodeN values contain acceptable values. */
    for (int i = 0; i < 15; i++) {
        std::ostringstream gamemode_key;
        gamemode_key << "gamemode";
        gamemode_key << i;

        std::string gamemode_value;
        plg->config()->GetOptionAsString(gamemode_key.str(), gamemode_value);
        gamemode_value = StringUtil::TrimString(gamemode_value);

        if (i == 0 && gamemode_value.compare("empty 1") != 0) {
            print_err("Can not load sampsharp if a non-SampSharp gamemode is "
                "set to load.");
            print_err("Please ensure you set 'gamemode0 empty 1' in your "
                "server.cfg file.");
            return false;
        }
        else if (i > 0 && gamemode_value.length() > 0) {
            print_err("Can not load sampsharp if a non-SampSharp gamemode is "
                "set to load.");
            print_err("Please ensure you only specify one script gamemode, "
                "namely 'gamemode0 empty 1' in your server.cfg file.");
            return false;
        }
    }

    return true;
}

#if SAMPSHARP_WINDOWS
void com_pipe() {
    std::string value;
    plg->config()->GetOptionAsString("com_pipe", value);
    com = new pipesvr_win32(value.c_str());
}

void com_tcp() {
    // TODO: Not yet implemented
    com_pipe();
}
#elif SAMPSHARP_LINUX
void com_dsock() {
    std::string value;
    plg->config()->GetOptionAsString("com_dsock", value);
    com = new dsock_unix(value.c_str());
}

void com_tcp() {
    std::string ip, port;
    plg->config()->GetOptionAsString("com_ip", ip);
    plg->config()->GetOptionAsString("com_port", port);
    uint16_t portnum = atoi(port.c_str());

    com = new tcp_unix(ip.c_str(), portnum);
}
#endif

void start_server() {
    std::string type;

    sampgdk_SendRconCommand("loadfs empty");

    print_info();

#if SAMPSHARP_WINDOWS
    com_pipe();
#elif SAMPSHARP_LINUX
    com_dsock();
#endif

    plg->config()->GetOptionAsString("com_type", type);

    if (!type.compare("tcp")) {
        com_tcp();
    }
#if SAMPSHARP_WINDOWS
    else if (!type.compare("pipe")) {
        com_pipe();
    }
#elif SAMPSHARP_LINUX
    else if (!type.compare("dsock")) {
        com_dsock();
    }
#endif

    std::string debug_str;
    plg->config()->GetOptionAsString("com_debug", type);
    bool debug = debug_str == "1" || debug_str == "true";

    if (com) {
        svr = new server(plg, com, debug);
        svr->start();
    }
}

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() {
    return sampgdk::Supports() | SUPPORTS_PROCESS_TICK;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData) {
    if (!sampgdk::Load(ppData)) {
        return false;
    }

    plg = new plugin(ppData);

    /* validate the server config is fit for running SampSharp */
    if (!plg || !(ready = config_validate())) {
        return ready = false;
    }

    return ready = true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload() {
    sampgdk_Unload();

    if (svr) {
        logprintf("Shutting down SampSharp server...");
        delete svr;
    }
    if (com) {
        com->disconnect();
        delete com;
    }
    if (plg) {
        delete plg;
    }

    plg = NULL;
    svr = NULL;
    com = NULL;
}

PLUGIN_EXPORT void PLUGIN_CALL ProcessTick() {
    sampgdk_ProcessTick();
    if (svr) {
        svr->tick();
    }
}

PLUGIN_EXPORT bool PLUGIN_CALL OnPublicCall(AMX *amx, const char *name,
    cell *params, cell *retval) {
    if (!ready) {
        return true;
    }
    if (!svr) {
        start_server();
    }
    if (svr) {
        svr->public_call(amx, name, params, retval);
    }
    return true;
}
