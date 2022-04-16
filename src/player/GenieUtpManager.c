// Copyright (c) 2021-2022 Qinglong<sysu.zqlong@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "osal/os_thread.h"
#include "osal/os_time.h"
#include "cutils/log_helper.h"
#include "cutils/memory_helper.h"
#include "cutils/list.h"
#include "cutils/mlooper.h"

#include "GenieUtpManager.h"

#define TAG "GenieUTP"

#define GENIE_UTP_THREAD_NAME       "GnUtpManager"
#define GENIE_UTP_THREAD_PRIO       OS_THREAD_PRIO_NORMAL
#define GENIE_UTP_THREAD_STACK      (8*1024)

#define GENIE_TTS_THREAD_NAME       "GnTtsWriter"
#define GENIE_TTS_THREAD_PRIO       OS_THREAD_PRIO_HIGH
#define GENIE_TTS_THREAD_STACK      (8*1024)
#define GENIE_TTS_FRAME_TIMEOUT     (10000) // 10s

#define GENIE_EXPECT_SPEECH_TIMEOUT (5000)  // 5s

typedef struct {
    void *handle;
    GnPlayer_Stream_t type;
    GnPlayer_Stream_t stream;
    GnPlayer_State_t state;
    int id;
    bool expectSpeech;
    bool isActive;
    bool isStarted;
} GnPlayer_Priv_t;

typedef struct {
    mlooper_handle looper;
    bool isLooperStarted;
    os_mutex looperLock;

    GnPlayer_Adapter_t adapter;
    GnPlayer_Priv_t ttsPlayer;
    GnPlayer_Priv_t promptPlayer;
    GnPlayer_Priv_t musicPlayer;

    bool isGatewayDisconnected;
    bool isMicphoneStarted;
    bool isSpeakerMuted;
    bool isTtsFrameStarted;
    bool isTtsPlayerStarted;
    bool isCommandPause;
    bool isMusicPausing;
    bool isMusicResuming;

    struct listnode stateListenerList;
    struct listnode playList;
    struct listnode ttsFrameList;
    int ttsId;
    os_thread ttsThread;
    os_mutex ttsLock;
    os_cond ttsCond;
} GnUtpManager_priv_t;

typedef struct {
    void (*stateListener)(GnPlayer_Stream_t stream, GnPlayer_State_t state, bool expectSpeech);
    struct listnode listnode;
} GnPlayer_StateListenerNode_t;

typedef struct {
    GnPlayer_Stream_t stream;
    const char *url;
    int id;
    bool expectSpeech;
    struct listnode listnode;
} GnPlayer_PlayNode_t;

typedef struct {
    int id;
    char *buffer;
    int size;
    bool final;
    struct listnode listnode;
} GnPlayer_TtsFrameNode_t;

static GnUtpManager_priv_t      sGnUtpManager;
static GnUtpManager_Callback_t  sGnCallback;
static bool                     sGnInited = false;

enum {
    WHAT_COMMAND_NEW_TTS_HEADER         = 0,
    WHAT_COMMAND_NEW_TTS_FRAME          = 1,
    WHAT_COMMAND_NEW_PROMPT_URL         = 2,
    WHAT_COMMAND_NEW_MUSIC_URL          = 3,
    WHAT_COMMAND_NEW_PROMPT_WAKEUP_URL  = 4,
    WHAT_COMMAND_PAUSE                  = 5,
    WHAT_COMMAND_RESUME                 = 6,
    WHAT_COMMAND_STOP                   = 7,
    WHAT_COMMAND_STOP_PLAYONCE          = 8,
    WHAT_COMMAND_SUSPEND                = 9,
    WHAT_COMMAND_RESUME_FROM_SUSPEND    = 10,
    WHAT_COMMAND_CHECK_TTS_TIMEOUT      = 11,

    WHAT_STATE_GATEWAY_CHANGED          = 100,
    WHAT_STATE_MICPHONE_CHANGED         = 101,
    WHAT_STATE_SPEAKER_CHANGED          = 102,
    WHAT_STATE_PLAYER_CHANGED           = 103,
};

typedef enum {
    GENIE_PLAYER_DO_SETSOURCE,
    GENIE_PLAYER_DO_PREPARE,
    GENIE_PLAYER_DO_WRITE,
    GENIE_PLAYER_DO_START,
    GENIE_PLAYER_DO_PAUSE,
    GENIE_PLAYER_DO_RESUME,
    GENIE_PLAYER_DO_SUSPEND,
    GENIE_PLAYER_DO_RESUME_FROM_SUSPEND,
    GENIE_PLAYER_DO_RESET,
} GnPlayer_Action_t;

static void GnLooper_Free_PlayNode(GnPlayer_PlayNode_t *node);
static void GnLooper_Free_TtsFrameNode(GnPlayer_TtsFrameNode_t *node);

static const char *GnPlayer_StreamToString(GnPlayer_Stream_t stream)
{
    switch (stream) {
    case GENIE_PLAYER_STREAM_TTS:
        return "Tts";
    case GENIE_PLAYER_STREAM_PROMPT:
        return "Prompt";
    case GENIE_PLAYER_STREAM_MUSIC:
        return "Music";
    case GENIE_PLAYER_STREAM_PROMPT_WAKEUP:
        return "PromptWakeup";
    default:
        return "Invalid";
    }
}

static const char *GnPlayer_StateToString(GnPlayer_State_t state)
{
    switch (state) {
    case GENIE_PLAYER_STATE_IDLE:
        return "Idle";
    case GENIE_PLAYER_STATE_PREPARED:
        return "Prepared";
    case GENIE_PLAYER_STATE_STARTED:
        return "Started";
    case GENIE_PLAYER_STATE_PAUSED:
        return "Paused";
    case GENIE_PLAYER_STATE_RESUMED:
        return "Resumed";
    case GENIE_PLAYER_STATE_NEARLYCOMPLETED:
        return "Nearlycompleted";
    case GENIE_PLAYER_STATE_COMPLETED:
        return "Completed";
    case GENIE_PLAYER_STATE_STOPPED:
        return "Stopped";
    case GENIE_PLAYER_STATE_ERROR:
        return "Error";
    default:
        return "Invalid";
    }
}

static bool GnPlayer_Is_PlayOnceActive()
{
    bool hasPendingPlayOnce = false;
    if (!list_empty(&sGnUtpManager.playList)) {
        GnPlayer_PlayNode_t *node =
            listnode_to_item(list_head(&sGnUtpManager.playList), GnPlayer_PlayNode_t, listnode);
        if (node->stream != GENIE_PLAYER_STREAM_MUSIC)
            hasPendingPlayOnce = true;
    }
    return (hasPendingPlayOnce ||
            sGnUtpManager.ttsPlayer.isActive || sGnUtpManager.promptPlayer.isActive);
}

static bool GnPlayer_Need_ResumeMusic()
{
    return (!GnPlayer_Is_PlayOnceActive() &&
            !sGnUtpManager.isCommandPause &&
            !sGnUtpManager.isGatewayDisconnected &&
            !sGnUtpManager.isMicphoneStarted &&
            !sGnUtpManager.isSpeakerMuted);
}

static void GnPlayer_Update_State(GnPlayer_Stream_t stream, GnPlayer_State_t state, bool expectSpeech)
{
    os_mutex_lock(sGnUtpManager.looperLock);
    struct listnode *item;
    list_for_each(item, &sGnUtpManager.stateListenerList) {
        GnPlayer_StateListenerNode_t *node = listnode_to_item(item, GnPlayer_StateListenerNode_t, listnode);
        node->stateListener(stream, state, expectSpeech);
    }
    os_mutex_unlock(sGnUtpManager.looperLock);
}

static void GnPlayer_Do_Action(GnPlayer_Priv_t *player, GnPlayer_Action_t action, int arg1, int arg2, void *data)
{
    GnPlayer_State_t state = player->state;
    GnPlayer_Stream_t type = player->type;

    switch (action) {
    case GENIE_PLAYER_DO_SETSOURCE:
        if (state == GENIE_PLAYER_STATE_IDLE) {
            const char *url = (const char *)data;
            OS_LOGI(TAG, "Player[%s]: setDataSource(%s)", GnPlayer_StreamToString(type), url != NULL ? url : "null");
            sGnUtpManager.adapter.setDataSource(player->handle, url);
            player->isActive = true;
            player->isStarted = false;
        }
        break;
    case GENIE_PLAYER_DO_PREPARE:
        if (state == GENIE_PLAYER_STATE_IDLE) {
            OS_LOGI(TAG, "Player[%s]: prepareAsync()", GnPlayer_StreamToString(type));
            sGnUtpManager.adapter.prepareAsync(player->handle);
            if (type == GENIE_PLAYER_STREAM_TTS) {
                // notify tts writer thread to fill stream data
                os_mutex_lock(sGnUtpManager.ttsLock);
                sGnUtpManager.isTtsPlayerStarted = true;
                os_cond_signal(sGnUtpManager.ttsCond);
                os_mutex_unlock(sGnUtpManager.ttsLock);
            }
        }
        break;
    case GENIE_PLAYER_DO_WRITE:
        if (state < GENIE_PLAYER_STATE_COMPLETED) {
            OS_LOGI(TAG, "Player[%s]: write(size=%d, final=%d)", GnPlayer_StreamToString(type), arg1, arg2);
            sGnUtpManager.adapter.write(player->handle, (char *)data, arg1, !!arg2);
        }
        break;
    case GENIE_PLAYER_DO_START:
    case GENIE_PLAYER_DO_RESUME:
        if (state == GENIE_PLAYER_STATE_PREPARED) {
            OS_LOGI(TAG, "Player[%s]: start()", GnPlayer_StreamToString(type));
            sGnUtpManager.adapter.start(player->handle);
            player->isStarted = true;
        } else if (state == GENIE_PLAYER_STATE_PAUSED) {
            OS_LOGI(TAG, "Player[%s]: resume()", GnPlayer_StreamToString(type));
            sGnUtpManager.adapter.resume(player->handle);
            player->isStarted = true;
            if (type == GENIE_PLAYER_STREAM_MUSIC)
                sGnUtpManager.isMusicResuming = true;
        } else if (state == GENIE_PLAYER_STATE_RESUMED) {
            if (type == GENIE_PLAYER_STREAM_MUSIC)
                GnPlayer_Update_State(GENIE_PLAYER_STREAM_MUSIC, GENIE_PLAYER_STATE_RESUMED, false);
        }
        break;
    case GENIE_PLAYER_DO_RESUME_FROM_SUSPEND:
        if (state == GENIE_PLAYER_STATE_PREPARED) {
            OS_LOGI(TAG, "Player[%s]: start()", GnPlayer_StreamToString(type));
            sGnUtpManager.adapter.start(player->handle);
            player->isStarted = true;
        } else if (state == GENIE_PLAYER_STATE_PAUSED) {
            OS_LOGI(TAG, "Player[%s]: resumeFromSuspend()", GnPlayer_StreamToString(type));
            sGnUtpManager.adapter.resume(player->handle);
            player->isStarted = true;
        }
        break;
    case GENIE_PLAYER_DO_PAUSE:
        if (state == GENIE_PLAYER_STATE_STARTED || state == GENIE_PLAYER_STATE_RESUMED) {
            OS_LOGI(TAG, "Player[%s]: pause()", GnPlayer_StreamToString(type));
            sGnUtpManager.adapter.pause(player->handle);
            if (type == GENIE_PLAYER_STREAM_MUSIC)
                sGnUtpManager.isMusicPausing = true;
        } else if (state == GENIE_PLAYER_STATE_PAUSED) {
            if (type == GENIE_PLAYER_STREAM_MUSIC)
                GnPlayer_Update_State(GENIE_PLAYER_STREAM_MUSIC, GENIE_PLAYER_STATE_PAUSED, false);
        }
        break;
    case GENIE_PLAYER_DO_SUSPEND:
        if (state == GENIE_PLAYER_STATE_STARTED || state == GENIE_PLAYER_STATE_RESUMED) {
            OS_LOGI(TAG, "Player[%s]: suspend()", GnPlayer_StreamToString(type));
            sGnUtpManager.adapter.pause(player->handle);
        }
        break;
    case GENIE_PLAYER_DO_RESET:
        if (state != GENIE_PLAYER_STATE_IDLE) {
            OS_LOGI(TAG, "Player[%s]: reset()", GnPlayer_StreamToString(type));
            if (type == GENIE_PLAYER_STREAM_TTS) {
                // no need filling stream data
                sGnUtpManager.isTtsPlayerStarted = false;
                // remove unused stream data, due to current tts aborted
                os_mutex_lock(sGnUtpManager.ttsLock);
                struct listnode *item, *tmp;
                list_for_each_safe(item, tmp, &sGnUtpManager.ttsFrameList) {
                    GnPlayer_TtsFrameNode_t *node = listnode_to_item(item, GnPlayer_TtsFrameNode_t, listnode);
                    if (node->id == sGnUtpManager.ttsPlayer.id) {
                        list_remove(item);
                        GnLooper_Free_TtsFrameNode(node);
                    } else {
                        break;
                    }
                }
                os_mutex_unlock(sGnUtpManager.ttsLock);
            }
            sGnUtpManager.adapter.reset(player->handle);
        }
        break;
    default:
        break;
    }
}

static void *GnPlayer_Tts_WriteThread(void *arg)
{
    while (1) {
        struct listnode *item, *tmp;
        GnPlayer_TtsFrameNode_t *node = NULL;

        os_mutex_lock(sGnUtpManager.ttsLock);
        while (!sGnUtpManager.isTtsPlayerStarted || list_empty(&sGnUtpManager.ttsFrameList))
            os_cond_wait(sGnUtpManager.ttsCond, sGnUtpManager.ttsLock);
        list_for_each_safe(item, tmp, &sGnUtpManager.ttsFrameList) {
            node = listnode_to_item(item, GnPlayer_TtsFrameNode_t, listnode);
            if (node->id >= sGnUtpManager.ttsPlayer.id)
                break;
            list_remove(item);
            GnLooper_Free_TtsFrameNode(node);
        }
        if (!list_empty(&sGnUtpManager.ttsFrameList)) {
            item = list_head(&sGnUtpManager.ttsFrameList);
            node = listnode_to_item(item, GnPlayer_TtsFrameNode_t, listnode);
            list_remove(item);
            while (!sGnUtpManager.isTtsPlayerStarted || node->id != sGnUtpManager.ttsPlayer.id)
                os_cond_wait(sGnUtpManager.ttsCond, sGnUtpManager.ttsLock);
        } else {
            node = NULL;
        }
        os_mutex_unlock(sGnUtpManager.ttsLock);

        if (node != NULL) {
            if (sGnUtpManager.isTtsPlayerStarted)
                GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_WRITE,
                                   node->size, node->final, node->buffer);
            GnLooper_Free_TtsFrameNode(node);
        }
    }
    return NULL;
}

static void GnLooper_Free_PlayNode(GnPlayer_PlayNode_t *node)
{
    if (node->url != NULL)
        OS_FREE(node->url);
    OS_FREE(node);
}

static void GnLooper_Free_TtsFrameNode(GnPlayer_TtsFrameNode_t *node)
{
    if (node->buffer != NULL)
        OS_FREE(node->buffer);
    OS_FREE(node);
}

static void GnLooper_Clear_TtsFrameList()
{
    os_mutex_lock(sGnUtpManager.ttsLock);
    struct listnode *item, *tmp;
    list_for_each_safe(item, tmp, &sGnUtpManager.ttsFrameList) {
        GnPlayer_TtsFrameNode_t *node = listnode_to_item(item, GnPlayer_TtsFrameNode_t, listnode);
        list_remove(item);
        GnLooper_Free_TtsFrameNode(node);
    }
    os_mutex_unlock(sGnUtpManager.ttsLock);
}

static void GnLooper_Clear_PlayList()
{
    struct listnode *item, *tmp;
    list_for_each_safe(item, tmp, &sGnUtpManager.playList) {
        GnPlayer_PlayNode_t *node = listnode_to_item(item, GnPlayer_PlayNode_t, listnode);
        list_remove(item);
        GnLooper_Free_PlayNode(node);
    }
    GnLooper_Clear_TtsFrameList();
}

static void GnLooper_Clear_TtsList()
{
    struct listnode *item, *tmp;
    list_for_each_safe(item, tmp, &sGnUtpManager.playList) {
        GnPlayer_PlayNode_t *node = listnode_to_item(item, GnPlayer_PlayNode_t, listnode);
        if (node->stream == GENIE_PLAYER_STREAM_TTS) {
            list_remove(item);
            GnLooper_Free_PlayNode(node);
        }
    }
    GnLooper_Clear_TtsFrameList();
}

static void GnLooper_Clear_PlayOnceList()
{
    struct listnode *item, *tmp;
    list_for_each_safe(item, tmp, &sGnUtpManager.playList) {
        GnPlayer_PlayNode_t *node = listnode_to_item(item, GnPlayer_PlayNode_t, listnode);
        if (node->stream != GENIE_PLAYER_STREAM_MUSIC &&
            node->stream != GENIE_PLAYER_STREAM_PROMPT_WAKEUP) {
            list_remove(item);
            GnLooper_Free_PlayNode(node);
        }
    }
    GnLooper_Clear_TtsFrameList();
}

static void GnLooper_Clear_PlayOnceList2()
{
    struct listnode *item, *tmp;
    list_for_each_safe(item, tmp, &sGnUtpManager.playList) {
        GnPlayer_PlayNode_t *node = listnode_to_item(item, GnPlayer_PlayNode_t, listnode);
        if (node->stream != GENIE_PLAYER_STREAM_MUSIC) {
            list_remove(item);
            GnLooper_Free_PlayNode(node);
        }
    }
    GnLooper_Clear_TtsFrameList();
}

static void GnLooper_Add_PlayNode(GnPlayer_PlayNode_t *node)
{
    switch (node->stream) {
    case GENIE_PLAYER_STREAM_TTS:
    case GENIE_PLAYER_STREAM_PROMPT:
    case GENIE_PLAYER_STREAM_PROMPT_WAKEUP:
        if (list_empty(&sGnUtpManager.playList)) {
            list_add_tail(&sGnUtpManager.playList, &node->listnode);
        } else {
            struct listnode *item, *tmp;
            bool hasAdded = false;
            list_for_each_safe(item, tmp, &sGnUtpManager.playList) {
                GnPlayer_PlayNode_t *curr = listnode_to_item(item, GnPlayer_PlayNode_t, listnode);
                if (curr->stream == GENIE_PLAYER_STREAM_MUSIC) {
                    list_add_before(&curr->listnode, &node->listnode);
                    hasAdded = true;
                    break;
                } else if (curr->stream == GENIE_PLAYER_STREAM_PROMPT_WAKEUP) {
                    if (node->stream == GENIE_PLAYER_STREAM_PROMPT_WAKEUP) {
                        list_remove(item);
                        GnLooper_Free_PlayNode(curr);
                    }
                }
            }
            if (!hasAdded)
                list_add_tail(&sGnUtpManager.playList, &node->listnode);
        }
        break;
    case GENIE_PLAYER_STREAM_MUSIC:
        list_add_tail(&sGnUtpManager.playList, &node->listnode);
        break;
    default:
        OS_LOGE(TAG, "Unknown player content: stream=%s", GnPlayer_StreamToString(node->stream));
        GnLooper_Free_PlayNode(node);
        break;
    }
}

static void GnLooper_Free_MessageData(struct message *msg)
{
    if (msg->data != NULL) {
        switch (msg->what) {
        case WHAT_COMMAND_NEW_TTS_FRAME:
        case WHAT_COMMAND_NEW_PROMPT_URL:
        case WHAT_COMMAND_NEW_MUSIC_URL:
        case WHAT_COMMAND_NEW_PROMPT_WAKEUP_URL:
            OS_FREE(msg->data);
            break;
        default:
            OS_LOGW(TAG, "Unfree message(%d) data(%p), maybe memory leak", msg->what, msg->data);
            break;
        }
    }
}

static bool GnLooper_Post_Message(int what, int arg1, int arg2, void *data)
{
    struct message *msg = message_obtain(what, arg1, arg2, data);
    if (msg == NULL) return false;
    if (mlooper_post_message(sGnUtpManager.looper, msg) != 0) {
        GnLooper_Free_MessageData(msg);
        OS_FREE(msg);
        return false;
    }
    return true;
}

static bool GnLooper_Post_DelayMessage(int what, int arg1, int arg2, void *data, unsigned long delayMs)
{
    struct message *msg = message_obtain(what, arg1, arg2, data);
    if (msg == NULL) return false;
    if (mlooper_post_message_delay(sGnUtpManager.looper, msg, delayMs) != 0) {
        GnLooper_Free_MessageData(msg);
        OS_FREE(msg);
        return false;
    }
    return true;
}

static void GnLooper_Handle_PlayerStateChanged(struct message *msg)
{
    GnPlayer_Stream_t type = (GnPlayer_Stream_t)msg->arg1;
    GnPlayer_Stream_t stream = (GnPlayer_Stream_t)msg->arg1;
    GnPlayer_State_t state = (GnPlayer_State_t)msg->arg2;
    GnPlayer_Priv_t *player = NULL;
    bool expectSpeech = false;
    bool needNotifyListener = true;

    OS_LOGI(TAG, "Player[%s]: state changed: [%s]",
            GnPlayer_StreamToString(type), GnPlayer_StateToString(state));

    if (type == GENIE_PLAYER_STREAM_TTS) {
        player = &sGnUtpManager.ttsPlayer;
        expectSpeech = sGnUtpManager.ttsPlayer.expectSpeech;
        if (state != GENIE_PLAYER_STATE_NEARLYCOMPLETED)
            sGnUtpManager.ttsPlayer.state = state;
    } else if (type == GENIE_PLAYER_STREAM_PROMPT) {
        player = &sGnUtpManager.promptPlayer;
        expectSpeech = sGnUtpManager.promptPlayer.expectSpeech;
        if (state != GENIE_PLAYER_STATE_NEARLYCOMPLETED)
            sGnUtpManager.promptPlayer.state = state;
        if (sGnUtpManager.promptPlayer.stream > GENIE_PLAYER_STREAM_PROMPT)
            stream = sGnUtpManager.promptPlayer.stream;
    } else if (type == GENIE_PLAYER_STREAM_MUSIC) {
        player = &sGnUtpManager.musicPlayer;
        expectSpeech = sGnUtpManager.musicPlayer.expectSpeech;
        if (state == GENIE_PLAYER_STATE_STARTED &&
            sGnUtpManager.musicPlayer.state >= GENIE_PLAYER_STATE_STARTED)
            state = GENIE_PLAYER_STATE_RESUMED;
        if (state != GENIE_PLAYER_STATE_NEARLYCOMPLETED)
            sGnUtpManager.musicPlayer.state = state;
        if (sGnUtpManager.musicPlayer.stream > GENIE_PLAYER_STREAM_MUSIC)
            stream = sGnUtpManager.musicPlayer.stream;
    } else {
        return;
    }

    switch (state) {
    case GENIE_PLAYER_STATE_IDLE:
        player->isActive = false;
        player->isStarted = false;
        if (GnPlayer_Need_ResumeMusic()) {
            if (stream == GENIE_PLAYER_STREAM_PROMPT_WAKEUP || expectSpeech) {
                mlooper_remove_message(sGnUtpManager.looper, WHAT_COMMAND_RESUME_FROM_SUSPEND);
                GnLooper_Post_DelayMessage(WHAT_COMMAND_RESUME_FROM_SUSPEND, 0, 0, NULL, GENIE_EXPECT_SPEECH_TIMEOUT);
            } else {
                GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_RESUME_FROM_SUSPEND, 0, 0, NULL);
            }
        }
        break;
    case GENIE_PLAYER_STATE_PREPARED:
        if (type == GENIE_PLAYER_STREAM_TTS) {
            if (!sGnUtpManager.isSpeakerMuted)
                GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_START, 0, 0, NULL);
        } else if (type == GENIE_PLAYER_STREAM_PROMPT) {
            if (!sGnUtpManager.isSpeakerMuted)
                GnPlayer_Do_Action(&sGnUtpManager.promptPlayer, GENIE_PLAYER_DO_START, 0, 0, NULL);
        } else if (type == GENIE_PLAYER_STREAM_MUSIC) {
            if (GnPlayer_Need_ResumeMusic())
                GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_START, 0, 0, NULL);
        }
        break;
    case GENIE_PLAYER_STATE_STARTED:
    case GENIE_PLAYER_STATE_RESUMED:
        if (type == GENIE_PLAYER_STREAM_MUSIC) {
            if (!GnPlayer_Need_ResumeMusic())
                GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_SUSPEND, 0, 0, NULL);
            if (!sGnUtpManager.isMusicResuming && state == GENIE_PLAYER_STATE_RESUMED)
                needNotifyListener = false;
            sGnUtpManager.isMusicResuming = false;
        }
        break;
    case GENIE_PLAYER_STATE_PAUSED:
        player->isStarted = false;
        if (type == GENIE_PLAYER_STREAM_MUSIC) {
            if (!sGnUtpManager.isMusicPausing)
                needNotifyListener = false;
            sGnUtpManager.isMusicPausing = false;
        }
        break;
    case GENIE_PLAYER_STATE_COMPLETED:
    case GENIE_PLAYER_STATE_STOPPED:
    case GENIE_PLAYER_STATE_ERROR:
        if (type == GENIE_PLAYER_STREAM_TTS)
            GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        else if (type == GENIE_PLAYER_STREAM_PROMPT)
            GnPlayer_Do_Action(&sGnUtpManager.promptPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        else if (type == GENIE_PLAYER_STREAM_MUSIC)
            GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        break;
    default:
        break;
    }

    // sync player state to listener
    if (needNotifyListener)
        GnPlayer_Update_State(stream, state, expectSpeech);
}

static void GnLooper_Handle_Message(struct message *msg)
{
    GnPlayer_PlayNode_t *node = NULL;
    switch (msg->what) {
    case WHAT_COMMAND_NEW_TTS_HEADER:
        // (WHAT_COMMAND_NEW_TTS_HEADER, expectSpeech, 0, NULL)
        if (sGnUtpManager.isSpeakerMuted) {
            OS_LOGW(TAG, "Speaker is muted, discard tts binary");
            break;
        }
        if (!sGnUtpManager.isTtsFrameStarted) {
            node = OS_CALLOC(1, sizeof(GnPlayer_PlayNode_t));
            if (node != NULL) {
                GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_SUSPEND, 0, 0, NULL);
                node->stream = GENIE_PLAYER_STREAM_TTS;
                node->id = sGnUtpManager.ttsId;
                node->expectSpeech = !!msg->arg1;
                GnLooper_Add_PlayNode(node);
                sGnUtpManager.isTtsFrameStarted = true;
            }
        }
        GnLooper_Post_DelayMessage(WHAT_COMMAND_CHECK_TTS_TIMEOUT, 0, 0, NULL, GENIE_TTS_FRAME_TIMEOUT);
        break;
    case WHAT_COMMAND_NEW_TTS_FRAME:
        // (WHAT_COMMAND_NEW_TTS_FRAME, size, final, msgData)
        if (sGnUtpManager.isSpeakerMuted) {
            OS_LOGW(TAG, "Speaker is muted, discard tts binary");
            if (sGnUtpManager.isTtsFrameStarted) {
                sGnUtpManager.ttsId++;
                sGnUtpManager.isTtsFrameStarted = false;
            }
            mlooper_remove_message(sGnUtpManager.looper, WHAT_COMMAND_CHECK_TTS_TIMEOUT);
            break;
        }
        if (sGnUtpManager.isTtsFrameStarted) {
            GnPlayer_TtsFrameNode_t *ttsnode = OS_MALLOC(sizeof(GnPlayer_TtsFrameNode_t));
            if (ttsnode != NULL) {
                ttsnode->id = sGnUtpManager.ttsId;
                ttsnode->buffer = msg->data;
                ttsnode->size = msg->arg1;
                ttsnode->final = !!msg->arg2;
                os_mutex_lock(sGnUtpManager.ttsLock);
                list_add_tail(&sGnUtpManager.ttsFrameList, &ttsnode->listnode);
                os_cond_signal(sGnUtpManager.ttsCond);
                os_mutex_unlock(sGnUtpManager.ttsLock);
                msg->data = NULL;
                mlooper_remove_message(sGnUtpManager.looper, WHAT_COMMAND_CHECK_TTS_TIMEOUT);
            }
        }
        if (!!msg->arg2) {
            if (sGnUtpManager.isTtsFrameStarted) {
                sGnUtpManager.ttsId++;
                sGnUtpManager.isTtsFrameStarted = false;
            }
        } else {
            GnLooper_Post_DelayMessage(WHAT_COMMAND_CHECK_TTS_TIMEOUT, 0, 0, NULL, GENIE_TTS_FRAME_TIMEOUT);
        }
        break;
    case WHAT_COMMAND_CHECK_TTS_TIMEOUT:
        // (WHAT_COMMAND_CHECK_TTS_TIMEOUT, 0, 0, NULL)
        OS_LOGE(TAG, "TTS frame timeout");
        if (sGnUtpManager.ttsPlayer.id == sGnUtpManager.ttsId) {
            GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        } else {
            struct listnode *item, *tmp;
            list_for_each_safe(item, tmp, &sGnUtpManager.playList) {
                GnPlayer_PlayNode_t *node = listnode_to_item(item, GnPlayer_PlayNode_t, listnode);
                if (node->stream == GENIE_PLAYER_STREAM_TTS && node->id == sGnUtpManager.ttsId) {
                    list_remove(item);
                    GnLooper_Free_PlayNode(node);
                    break;
                }
            }
            os_mutex_lock(sGnUtpManager.ttsLock);
            list_for_each_safe(item, tmp, &sGnUtpManager.ttsFrameList) {
                GnPlayer_TtsFrameNode_t *node = listnode_to_item(item, GnPlayer_TtsFrameNode_t, listnode);
                if (node->id == sGnUtpManager.ttsId) {
                    list_remove(item);
                    GnLooper_Free_TtsFrameNode(node);
                }
            }
            os_mutex_unlock(sGnUtpManager.ttsLock);
        }
        if (sGnUtpManager.isTtsFrameStarted) {
            sGnUtpManager.isTtsFrameStarted = false;
            sGnUtpManager.ttsId++;
        }
        break;
    case WHAT_COMMAND_NEW_PROMPT_URL:
        // (WHAT_COMMAND_NEW_PROMPT_URL, 0, 0, msgData)
        if (sGnUtpManager.isSpeakerMuted) {
            OS_LOGW(TAG, "Speaker is muted, discard prompt:%s", (char *)msg->data);
            break;
        }
        node = OS_CALLOC(1, sizeof(GnPlayer_PlayNode_t));
        if (node != NULL) {
            GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_SUSPEND, 0, 0, NULL);
            node->stream = GENIE_PLAYER_STREAM_PROMPT;
            node->url = msg->data;
            GnLooper_Add_PlayNode(node);
            msg->data = NULL;
        }
        break;
    case WHAT_COMMAND_NEW_MUSIC_URL:
        // (WHAT_COMMAND_NEW_MUSIC_URL, 0, 0, msgData)
        if (sGnUtpManager.isSpeakerMuted) {
            OS_LOGW(TAG, "Speaker is muted, discard music:%s", (char *)msg->data);
            break;
        }
        node = OS_CALLOC(1, sizeof(GnPlayer_PlayNode_t));
        if (node != NULL) {
            GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
            node->stream = GENIE_PLAYER_STREAM_MUSIC;
            node->url = msg->data;
            GnLooper_Add_PlayNode(node);
            msg->data = NULL;
        }
        break;
    case WHAT_COMMAND_NEW_PROMPT_WAKEUP_URL:
        // (WHAT_COMMAND_NEW_PROMPT_WAKEUP_URL, 0, 0, msgData)
        if (sGnUtpManager.isSpeakerMuted) {
            OS_LOGW(TAG, "Speaker is muted, discard wakeup prompt:%s", (char *)msg->data);
            break;
        }
        GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_SUSPEND, 0, 0, NULL);
        GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        GnPlayer_Do_Action(&sGnUtpManager.promptPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        GnLooper_Clear_PlayOnceList();
        node = OS_CALLOC(1, sizeof(GnPlayer_PlayNode_t));
        if (node != NULL) {
            node->stream = GENIE_PLAYER_STREAM_PROMPT_WAKEUP;
            node->url = msg->data;
            GnLooper_Add_PlayNode(node);
            msg->data = NULL;
        }
        break;
    case WHAT_COMMAND_PAUSE:
        // (WHAT_COMMAND_PAUSE, 0, 0, NULL)
        mlooper_remove_message(sGnUtpManager.looper, WHAT_COMMAND_RESUME_FROM_SUSPEND);
        sGnUtpManager.isCommandPause = true;
        GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_PAUSE, 0, 0, NULL);
        break;
    case WHAT_COMMAND_RESUME:
        // (WHAT_COMMAND_RESUME, 0, 0, NULL)
        sGnUtpManager.isCommandPause = false;
        if (GnPlayer_Need_ResumeMusic())
            GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_RESUME, 0, 0, NULL);
        break;
    case WHAT_COMMAND_RESUME_FROM_SUSPEND:
        if (GnPlayer_Need_ResumeMusic())
            GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_RESUME_FROM_SUSPEND, 0, 0, NULL);
        break;
    case WHAT_COMMAND_STOP:
        // (WHAT_COMMAND_STOP, 0, 0, NULL)
        GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        if (sGnUtpManager.promptPlayer.stream != GENIE_PLAYER_STREAM_PROMPT_WAKEUP)
            GnPlayer_Do_Action(&sGnUtpManager.promptPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        GnLooper_Clear_PlayList();
        break;
    case WHAT_COMMAND_STOP_PLAYONCE:
        // (WHAT_COMMAND_STOP_PLAYONCE, 0, 0, NULL)
        GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        if (sGnUtpManager.promptPlayer.stream != GENIE_PLAYER_STREAM_PROMPT_WAKEUP)
            GnPlayer_Do_Action(&sGnUtpManager.promptPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        GnLooper_Clear_PlayList();
        break;
    case WHAT_STATE_GATEWAY_CHANGED:
        // (WHAT_STATE_GATEWAY_CHANGED, active, 0, NULL)
        if (!!msg->arg1) {
            sGnUtpManager.isGatewayDisconnected = false;
            if (GnPlayer_Need_ResumeMusic())
                GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_RESUME_FROM_SUSPEND, 0, 0, NULL);
        } else {
            sGnUtpManager.isGatewayDisconnected = true;
            GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_SUSPEND, 0, 0, NULL);
            GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
            GnLooper_Clear_TtsList();
        }
        break;
    case WHAT_STATE_MICPHONE_CHANGED:
        // (WHAT_STATE_MICPHONE_CHANGED, active, 0, NULL)
        if (!!msg->arg1) {
            if (!sGnUtpManager.isMicphoneStarted) {
                sGnUtpManager.isMicphoneStarted = true;
                GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_SUSPEND, 0, 0, NULL);
                GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
                if (sGnUtpManager.promptPlayer.stream != GENIE_PLAYER_STREAM_PROMPT_WAKEUP)
                    GnPlayer_Do_Action(&sGnUtpManager.promptPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
                GnLooper_Clear_PlayOnceList();
            }
        } else {
            sGnUtpManager.isMicphoneStarted = false;
            if (GnPlayer_Need_ResumeMusic()) {
                mlooper_remove_message(sGnUtpManager.looper, WHAT_COMMAND_RESUME_FROM_SUSPEND);
                GnLooper_Post_DelayMessage(WHAT_COMMAND_RESUME_FROM_SUSPEND, 0, 0, NULL, GENIE_EXPECT_SPEECH_TIMEOUT);
            }
        }
        break;
    case WHAT_STATE_SPEAKER_CHANGED:
        // (WHAT_STATE_SPEAKER_CHANGED, active, 0, NULL)
        if (!!msg->arg1) {
            sGnUtpManager.isSpeakerMuted = true;
            GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_SUSPEND, 0, 0, NULL);
            GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
            GnPlayer_Do_Action(&sGnUtpManager.promptPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
            GnLooper_Clear_PlayOnceList2();
        } else {
            sGnUtpManager.isSpeakerMuted = false;
            if (GnPlayer_Need_ResumeMusic())
                GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_RESUME_FROM_SUSPEND, 0, 0, NULL);
        }
        break;
    case WHAT_STATE_PLAYER_CHANGED:
        // (WHAT_STATE_PLAYER_CHANGED, stream, state, NULL)
        GnLooper_Handle_PlayerStateChanged(msg);
        break;
    default:
        break;
    }

    // prepare next play
    if (!list_empty(&sGnUtpManager.playList) && !sGnUtpManager.isSpeakerMuted &&
        !sGnUtpManager.ttsPlayer.isActive &&
        !sGnUtpManager.promptPlayer.isActive &&
        !sGnUtpManager.musicPlayer.isStarted) {
        struct listnode *front = list_head(&sGnUtpManager.playList);
        node = listnode_to_item(front, GnPlayer_PlayNode_t, listnode);
        if (sGnUtpManager.isMicphoneStarted && node->stream != GENIE_PLAYER_STREAM_PROMPT_WAKEUP)
            goto __out_handler;
        if (sGnUtpManager.isGatewayDisconnected &&
            (node->stream == GENIE_PLAYER_STREAM_MUSIC || node->stream == GENIE_PLAYER_STREAM_TTS))
            goto __out_handler;
        switch (node->stream) {
        case GENIE_PLAYER_STREAM_TTS:
            sGnUtpManager.ttsPlayer.id = node->id;
            sGnUtpManager.ttsPlayer.stream = node->stream;
            sGnUtpManager.ttsPlayer.expectSpeech = node->expectSpeech;
            GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_SETSOURCE, 0, 0, NULL);
            GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_PREPARE, 0, 0, NULL);
            break;
        case GENIE_PLAYER_STREAM_PROMPT:
        case GENIE_PLAYER_STREAM_PROMPT_WAKEUP:
            sGnUtpManager.promptPlayer.stream = node->stream;
            sGnUtpManager.promptPlayer.expectSpeech = node->expectSpeech;
            GnPlayer_Do_Action(&sGnUtpManager.promptPlayer, GENIE_PLAYER_DO_SETSOURCE, 0, 0, (void *)node->url);
            GnPlayer_Do_Action(&sGnUtpManager.promptPlayer, GENIE_PLAYER_DO_PREPARE, 0, 0, NULL);
            break;
        case GENIE_PLAYER_STREAM_MUSIC:
            if (sGnUtpManager.musicPlayer.isActive)
                goto __out_handler;
            sGnUtpManager.isCommandPause = false;
            sGnUtpManager.isMusicPausing = false;
            sGnUtpManager.isMusicResuming = false;
            sGnUtpManager.musicPlayer.stream = node->stream;
            sGnUtpManager.musicPlayer.expectSpeech = node->expectSpeech;
            GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_SETSOURCE, 0, 0, (void *)node->url);
            GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_PREPARE, 0, 0, NULL);
            break;
        default:
            break;
        }
        list_remove(front);
        GnLooper_Free_PlayNode(node);
    }

__out_handler:
    // debug message
    if (msg->what != WHAT_COMMAND_NEW_TTS_FRAME) {
        if (!list_empty(&sGnUtpManager.playList)) {
            OS_LOGD(TAG, "Playlist dump:");
            struct listnode *item;
            list_for_each(item, &sGnUtpManager.playList) {
                node = listnode_to_item(item, GnPlayer_PlayNode_t, listnode);
                OS_LOGD(TAG, " > stream:[%s], url:[%s]",
                    GnPlayer_StreamToString(node->stream), node->url != NULL ? node->url : "null");
            }
        }
        {
            OS_LOGD(TAG, "State dump:");
            OS_LOGD(TAG, " > Gateway:[%s], Micphone:[%s], Speaker:[%s], Pause:[%s]",
                    sGnUtpManager.isGatewayDisconnected ? "Disconnected" : "Connected",
                    sGnUtpManager.isMicphoneStarted ? "Started" : "Stopped",
                    sGnUtpManager.isSpeakerMuted ? "Muted" : "Unmuted",
                    sGnUtpManager.isCommandPause ? "True" : "False");
            OS_LOGD(TAG, " > TtsPlayer:[%s], PromptPlayer:[%s], MusicPlayer[%s]",
                    GnPlayer_StateToString(sGnUtpManager.ttsPlayer.state),
                    GnPlayer_StateToString(sGnUtpManager.promptPlayer.state),
                    GnPlayer_StateToString(sGnUtpManager.musicPlayer.state));

            int activePlayersCount = 0;
            if (sGnUtpManager.ttsPlayer.isActive)
                activePlayersCount++;
            if (sGnUtpManager.promptPlayer.isActive)
                activePlayersCount++;
            if (sGnUtpManager.musicPlayer.isStarted)
                activePlayersCount++;
            if (activePlayersCount > 1)
                OS_LOGE(TAG, " > Multiple players were active unexpectly");
        }
    }
}

static void GnPlayer_StateListener(GnPlayer_Stream_t stream, GnPlayer_State_t state)
{
    GnLooper_Post_Message(WHAT_STATE_PLAYER_CHANGED, stream, state, NULL);
}

static void GnCallback_OnCommandNewTtsHeader(bool expectSpeech)
{
    GnLooper_Post_Message(WHAT_COMMAND_NEW_TTS_HEADER, expectSpeech, 0, NULL);
}

static void GnCallback_OnCommandNewTtsFrame(char *buffer, int size, bool final)
{
    char *msgData = NULL;
    if (buffer != NULL && size > 0) {
        if ((msgData = OS_MALLOC(size)) == NULL)
            return;
        memcpy(msgData, buffer, size);
    }
    GnLooper_Post_Message(WHAT_COMMAND_NEW_TTS_FRAME, size, final, msgData);
}

static void GnCallback_OnCommandNewPrompt(const char *url)
{
    char *msgData = NULL;
    if (url != NULL && (msgData = OS_STRDUP(url)) != NULL)
        GnLooper_Post_Message(WHAT_COMMAND_NEW_PROMPT_URL, 0, 0, msgData);
}

static void GnCallback_OnCommandNewMusic(const char *url)
{
    char *msgData = NULL;
    if (url != NULL && (msgData = OS_STRDUP(url)) != NULL)
        GnLooper_Post_Message(WHAT_COMMAND_NEW_MUSIC_URL, 0, 0, msgData);
}

static void GnCallback_OnCommandNewPromptWakeup(const char *url)
{
    char *msgData = NULL;
    if (url != NULL && (msgData = OS_STRDUP(url)) != NULL)
        GnLooper_Post_Message(WHAT_COMMAND_NEW_PROMPT_WAKEUP_URL, 0, 0, msgData);
}

static void GnCallback_OnCommandPause()
{
    GnLooper_Post_Message(WHAT_COMMAND_PAUSE, 0, 0, NULL);
}

static void GnCallback_OnCommandResume()
{
    GnLooper_Post_Message(WHAT_COMMAND_RESUME, 0, 0, NULL);
}

static void GnCallback_OnCommandStop()
{
    GnLooper_Post_Message(WHAT_COMMAND_STOP, 0, 0, NULL);
}

static void GnCallback_OnCommandStopPlayonce()
{
    GnLooper_Post_Message(WHAT_COMMAND_STOP_PLAYONCE, 0, 0, NULL);
}

static void GnCallback_OnGatewayConnected()
{
    GnLooper_Post_Message(WHAT_STATE_GATEWAY_CHANGED, true, 0, NULL);
}

static void GnCallback_OnGatewayDisconnected()
{
    GnLooper_Post_Message(WHAT_STATE_GATEWAY_CHANGED, false, 0, NULL);
}

static void GnCallback_OnMicphoneStarted()
{
    GnLooper_Post_Message(WHAT_STATE_MICPHONE_CHANGED, true, 0, NULL);
}

static void GnCallback_OnMicphoneStopped()
{
    GnLooper_Post_Message(WHAT_STATE_MICPHONE_CHANGED, false, 0, NULL);
}

static void GnCallback_OnSpeakerMuted()
{
    GnLooper_Post_Message(WHAT_STATE_SPEAKER_CHANGED, true, 0, NULL);
}

static void GnCallback_OnSpeakerUnmuted()
{
    GnLooper_Post_Message(WHAT_STATE_SPEAKER_CHANGED, false, 0, NULL);
}

bool GnUtpManager_Init(GnPlayer_Adapter_t *adapter)
{
    if (sGnInited) return true;

    memset(&sGnUtpManager, 0x0, sizeof(sGnUtpManager));

    if (adapter == NULL ||
        adapter->create == NULL ||
        adapter->registerStateListener == NULL ||
        adapter->setDataSource == NULL ||
        adapter->prepareAsync == NULL ||
        adapter->write == NULL ||
        adapter->start == NULL ||
        adapter->pause == NULL ||
        adapter->resume == NULL ||
        adapter->seek == NULL ||
        adapter->stop == NULL ||
        adapter->reset == NULL ||
        adapter->getPosition == NULL ||
        adapter->getDuration == NULL ||
        adapter->destroy == NULL)
        return false;
    memcpy(&sGnUtpManager.adapter, adapter, sizeof(sGnUtpManager.adapter));

    if ((sGnUtpManager.ttsPlayer.handle = adapter->create(GENIE_PLAYER_STREAM_TTS)) == NULL)
        goto __error_init;
    if ((sGnUtpManager.promptPlayer.handle = adapter->create(GENIE_PLAYER_STREAM_PROMPT)) == NULL)
        goto __error_init;
    if ((sGnUtpManager.musicPlayer.handle = adapter->create(GENIE_PLAYER_STREAM_MUSIC)) == NULL)
        goto __error_init;
    if (!adapter->registerStateListener(sGnUtpManager.ttsPlayer.handle, GnPlayer_StateListener))
        goto __error_init;
    if (!adapter->registerStateListener(sGnUtpManager.promptPlayer.handle, GnPlayer_StateListener))
        goto __error_init;
    if (!adapter->registerStateListener(sGnUtpManager.musicPlayer.handle, GnPlayer_StateListener))
        goto __error_init;
    sGnUtpManager.ttsPlayer.type = GENIE_PLAYER_STREAM_TTS;
    sGnUtpManager.promptPlayer.type = GENIE_PLAYER_STREAM_PROMPT;
    sGnUtpManager.musicPlayer.type = GENIE_PLAYER_STREAM_MUSIC;

    struct os_thread_attr thread_attr;
    thread_attr.name = GENIE_UTP_THREAD_NAME;
    thread_attr.priority = GENIE_UTP_THREAD_PRIO;
    thread_attr.stacksize = GENIE_UTP_THREAD_STACK;
    thread_attr.joinable = true;
    sGnUtpManager.looper = mlooper_create(&thread_attr, GnLooper_Handle_Message, GnLooper_Free_MessageData);
    if (sGnUtpManager.looper == NULL)
        goto __error_init;
    if ((sGnUtpManager.looperLock = os_mutex_create()) == NULL)
        goto __error_init;
    if ((sGnUtpManager.ttsLock = os_mutex_create()) == NULL)
        goto __error_init;
    if ((sGnUtpManager.ttsCond = os_cond_create()) == NULL)
        goto __error_init;

    thread_attr.name = GENIE_TTS_THREAD_NAME;
    thread_attr.priority = GENIE_TTS_THREAD_PRIO;
    thread_attr.stacksize = GENIE_TTS_THREAD_STACK;
    sGnUtpManager.ttsThread = os_thread_create(&thread_attr, GnPlayer_Tts_WriteThread, NULL);
    if (sGnUtpManager.ttsThread == NULL)
        goto __error_init;

    list_init(&sGnUtpManager.stateListenerList);
    list_init(&sGnUtpManager.playList);
    list_init(&sGnUtpManager.ttsFrameList);

    sGnCallback.onCommandNewTtsHeader       = GnCallback_OnCommandNewTtsHeader;
    sGnCallback.onCommandNewTtsFrame        = GnCallback_OnCommandNewTtsFrame;
    sGnCallback.onCommandNewPrompt          = GnCallback_OnCommandNewPrompt;
    sGnCallback.onCommandNewMusic           = GnCallback_OnCommandNewMusic;
    sGnCallback.onCommandNewPromptWakeup    = GnCallback_OnCommandNewPromptWakeup;
    sGnCallback.onCommandPause              = GnCallback_OnCommandPause;
    sGnCallback.onCommandResume             = GnCallback_OnCommandResume;
    sGnCallback.onCommandStop               = GnCallback_OnCommandStop;
    sGnCallback.onCommandStopPlayonce       = GnCallback_OnCommandStopPlayonce;
    sGnCallback.onGatewayConnected          = GnCallback_OnGatewayConnected;
    sGnCallback.onGatewayDisconnected       = GnCallback_OnGatewayDisconnected;
    sGnCallback.onMicphoneStarted           = GnCallback_OnMicphoneStarted;
    sGnCallback.onMicphoneStopped           = GnCallback_OnMicphoneStopped;
    sGnCallback.onSpeakerMuted              = GnCallback_OnSpeakerMuted;
    sGnCallback.onSpeakerUnmuted            = GnCallback_OnSpeakerUnmuted;

    sGnInited = true;
    return true;

__error_init:
    if (sGnUtpManager.ttsCond != NULL)      os_cond_destroy(sGnUtpManager.ttsCond);
    if (sGnUtpManager.ttsLock != NULL)      os_mutex_destroy(sGnUtpManager.ttsLock);
    if (sGnUtpManager.looperLock != NULL)   os_mutex_destroy(sGnUtpManager.looperLock);
    if (sGnUtpManager.looper != NULL)       mlooper_destroy(sGnUtpManager.looper);
    if (sGnUtpManager.ttsPlayer.handle != NULL)
        adapter->destroy(sGnUtpManager.ttsPlayer.handle);
    if (sGnUtpManager.promptPlayer.handle != NULL)
        adapter->destroy(sGnUtpManager.promptPlayer.handle);
    if (sGnUtpManager.musicPlayer.handle != NULL)
        adapter->destroy(sGnUtpManager.musicPlayer.handle);
    return false;
}

bool GnUtpManager_IsInit()
{
    return sGnInited;
}

bool GnUtpManager_Get_Callback(GnUtpManager_Callback_t **callback)
{
    if (!sGnInited || callback == NULL) {
        OS_LOGE(TAG, "Genie UtpManager is NOT inited");
        return false;
    }
    *callback = &sGnCallback;
    return true;
}

bool GnUtpManager_Register_StateListener(void (*listener)(GnPlayer_Stream_t stream, GnPlayer_State_t state, bool expectSpeech))
{
    if (!sGnInited || listener == NULL) {
        OS_LOGE(TAG, "Genie UtpManager is NOT inited");
        return false;
    }

    os_mutex_lock(sGnUtpManager.looperLock);
    GnPlayer_StateListenerNode_t *node;
    struct listnode *item;
    bool found = false;
    list_for_each(item, &sGnUtpManager.stateListenerList) {
        node = listnode_to_item(item, GnPlayer_StateListenerNode_t, listnode);
        if (node->stateListener == listener)
            found = true;
    }
    if (!found) {
        node = OS_MALLOC(sizeof(GnPlayer_StateListenerNode_t));
        if (node == NULL) {
            os_mutex_unlock(sGnUtpManager.looperLock);
            return false;
        }
        node->stateListener = listener;
        list_add_tail(&sGnUtpManager.stateListenerList, &node->listnode);
    }
    os_mutex_unlock(sGnUtpManager.looperLock);
    return true;
}

void GnUtpManager_Unregister_StateListener(void (*listener)(GnPlayer_Stream_t stream, GnPlayer_State_t state, bool expectSpeech))
{
    if (!sGnInited || listener == NULL) {
        OS_LOGE(TAG, "Genie UtpManager is NOT inited");
        return;
    }

    os_mutex_lock(sGnUtpManager.looperLock);
    struct listnode *item, *tmp;
    list_for_each_safe(item, tmp, &sGnUtpManager.stateListenerList) {
        GnPlayer_StateListenerNode_t *node = listnode_to_item(item, GnPlayer_StateListenerNode_t, listnode);
        if (node->stateListener == listener) {
            list_remove(item);
            OS_FREE(node);
            break;
        }
    }
    os_mutex_unlock(sGnUtpManager.looperLock);
}

bool GnUtpManager_Start()
{
    if (!sGnInited) { OS_LOGE(TAG, "Genie UtpManager is NOT inited"); return false; }
    os_mutex_lock(sGnUtpManager.looperLock);
    if (!sGnUtpManager.isLooperStarted) {
        if (mlooper_start(sGnUtpManager.looper) != 0) {
            os_mutex_unlock(sGnUtpManager.looperLock);
            return false;
        }
        sGnUtpManager.isLooperStarted = true;
    }
    os_mutex_unlock(sGnUtpManager.looperLock);
    return true;
}

bool GnUtpManager_IsActive()
{
    return sGnUtpManager.isLooperStarted;
}

void GnUtpManager_Stop()
{
    if (!sGnInited) { OS_LOGE(TAG, "Genie UtpManager is NOT inited"); return; }
    os_mutex_lock(sGnUtpManager.looperLock);
    if (sGnUtpManager.isLooperStarted) {
        mlooper_stop(sGnUtpManager.looper);
        sGnUtpManager.isLooperStarted = false;
    }
    if (sGnUtpManager.ttsPlayer.state != GENIE_PLAYER_STATE_IDLE) {
        GnPlayer_Do_Action(&sGnUtpManager.ttsPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        sGnUtpManager.ttsPlayer.state = GENIE_PLAYER_STATE_IDLE;
        sGnUtpManager.ttsPlayer.isActive = false;
        sGnUtpManager.ttsPlayer.isStarted = false;
    }
    if (sGnUtpManager.promptPlayer.state != GENIE_PLAYER_STATE_IDLE) {
        GnPlayer_Do_Action(&sGnUtpManager.promptPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        sGnUtpManager.promptPlayer.state = GENIE_PLAYER_STATE_IDLE;
        sGnUtpManager.promptPlayer.isActive = false;
        sGnUtpManager.promptPlayer.isStarted = false;
    }
    if (sGnUtpManager.musicPlayer.state != GENIE_PLAYER_STATE_IDLE) {
        GnPlayer_Do_Action(&sGnUtpManager.musicPlayer, GENIE_PLAYER_DO_RESET, 0, 0, NULL);
        sGnUtpManager.musicPlayer.state = GENIE_PLAYER_STATE_IDLE;
        sGnUtpManager.musicPlayer.isActive = false;
        sGnUtpManager.musicPlayer.isStarted = false;
    }
    {
        os_mutex_lock(sGnUtpManager.ttsLock);
        struct listnode *item, *tmp;
        list_for_each_safe(item, tmp, &sGnUtpManager.ttsFrameList) {
            GnPlayer_TtsFrameNode_t *node = listnode_to_item(item, GnPlayer_TtsFrameNode_t, listnode);
            list_remove(item);
            GnLooper_Free_TtsFrameNode(node);
        }
        os_mutex_unlock(sGnUtpManager.ttsLock);
    }
    mlooper_clear_message(sGnUtpManager.looper);
    sGnUtpManager.isTtsFrameStarted = false;
    sGnUtpManager.isTtsPlayerStarted = false;
    sGnUtpManager.isCommandPause = false;
    sGnUtpManager.isMusicPausing = false;
    sGnUtpManager.isMusicResuming = false;
    os_mutex_unlock(sGnUtpManager.looperLock);
}
