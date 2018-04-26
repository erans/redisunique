/*
Copyright (c) 2017 Eran Sandler <eran at sandler dot co dot il>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#include <stdio.h>
#include <uuid/uuid.h>
#include <string.h>

#include "snowflake/snowflake.h"

#include "rmutil/strings.h"
#include "rmutil/alloc.h"
#include "rmutil/util.h"

#include "redismodule.h"

#define SNOWFLAKE_TOKEN "$SNOWFLAKE$"
#define UUIDv1_TOKEN "$UUIDV1$"
#define UUIDv4_TOKEN "$UUIDV4$"

char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

char *generate_uuid1_string() {
  uuid_t u;

  uuid_generate_time(u);
  char *u_str = malloc(36);
  uuid_unparse_lower(u, u_str);

  return u_str;
}

char *generate_uuidv4_string() {
  uuid_t u;

  uuid_generate_random(u);
  char *u_str = malloc(36);
  uuid_unparse_lower(u, u_str);

  return u_str;
}

int SnowflakeGetIdCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  RedisModule_ReplyWithLongLong(ctx, snowflake_id());
  return REDISMODULE_OK;
}

int UUIDv1GetIdCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  char *uuid_str = generate_uuid1_string();
  if(!uuid_str) {
    return REDISMODULE_ERR;
  }
  RedisModule_ReplyWithStringBuffer(ctx, uuid_str, 36);
  free(uuid_str);

  return REDISMODULE_OK;
}

int UUIDv4GetIdCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  char *uuid_str = generate_uuidv4_string();
  if(!uuid_str) {
    return REDISMODULE_ERR;
  }
  RedisModule_ReplyWithStringBuffer(ctx, uuid_str, 36);
  free(uuid_str);

  return REDISMODULE_OK;
}

int ExecCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  int i;
  RedisModuleCallReply *subcall_result = NULL;
  RedisModuleString *subcall_result_str = NULL;
  //int substr_replacement = 0;

  for (i = 0; i < argc; i++) {
    if (RMUtil_StringEqualsC(argv[i], SNOWFLAKE_TOKEN)) {
      subcall_result = RedisModule_Call(ctx, "UNIQUEID.SNOWFLAKE", "", NULL, 0);
      break;
    } else if (RMUtil_StringEqualsC(argv[i], UUIDv1_TOKEN) || RMUtil_StringEqualsC(argv[i], UUIDv4_TOKEN)) {
      if (RMUtil_StringEqualsC(argv[i], UUIDv1_TOKEN)) {
        subcall_result = RedisModule_Call(ctx, "UNIQUEID.UUIDV1", "", NULL, 0);
      } else {
        subcall_result = RedisModule_Call(ctx, "UNIQUEID.UUIDV4", "", NULL, 0);
      }
      break;
    } else {
      // check if the replacement is a substring
      //substr_replacement = 1;
    }
  }

  // if subcall_result is NULL, there wasn't a complete parameter that requires
  // replacment, in that case we should check if there are sub
  if (subcall_result) {
    RMUTIL_ASSERT_NOERROR(ctx, subcall_result);
    subcall_result_str = RedisModule_CreateStringFromCallReply(subcall_result);
    argv[i] = subcall_result_str;
  }

  RedisModuleCallReply *r = RedisModule_Call(
      ctx, RedisModule_StringPtrLen(argv[1], NULL), "v", &argv[2], argc - 2);

  RMUTIL_ASSERT_NOERROR(ctx, r);

  RedisModule_ReplyWithArray(ctx,2);
  if (subcall_result_str) {
    RedisModule_ReplyWithString(ctx, subcall_result_str);
  } else {
    RedisModule_ReplyWithNull(ctx);
  }
  RedisModule_ReplyWithCallReply(ctx, r);

  return REDISMODULE_OK;
}

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if (RedisModule_Init(ctx, "redisunique", 1, REDISMODULE_APIVER_1) ==
      REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  long long snowflake_region_id = 1;
  long long snowflake_worker_id = 1;

  if (argc > 0) {
    if (RedisModule_StringToLongLong(argv[0], &snowflake_region_id) == REDISMODULE_ERR) {
      RedisModule_Log(ctx, "info", "No region_id value set , using default value of 1");
      snowflake_region_id = 1;
    }

    if (RedisModule_StringToLongLong(argv[1], &snowflake_worker_id) == REDISMODULE_ERR) {
      RedisModule_Log(ctx, "info", "No worker_id value set, using default value of 1");
      snowflake_worker_id = 1;
    }
  }

  RedisModule_Log(ctx, "info", "using snowflake region_id: %u  worker_id: %u", snowflake_region_id, snowflake_worker_id);

  if (snowflake_init(snowflake_region_id, snowflake_worker_id) != 1) {
    RedisModule_Log(ctx, "error", "Failed to initialize snowflake");
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "uniqueid.snowflake", SnowflakeGetIdCommand, "readonly",
                                1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "uniqueid.uuidv1", UUIDv1GetIdCommand, "readonly",
                                1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "uniqueid.uuidv4", UUIDv4GetIdCommand, "readonly",
                                1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  if (RedisModule_CreateCommand(ctx, "uniqueid.exec", ExecCommand, "", 1, 1, 1) == REDISMODULE_ERR) {
    return REDISMODULE_ERR;
  }

  return REDISMODULE_OK;
}
