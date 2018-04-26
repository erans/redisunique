# RedisUnique

Written by Eran Sandler ([@erans](https://twitter.com/erans))

This is a simple Redis module which generates unique IDs based on a number of differet algorithms:

* UUIDv1
* UUIDv4
* [Twitter's Snowflake](https://github.com/twitter/snowflake/tree/snowflake-2010) at high scale with some simple guarantees.

This module can also proxy other commands, generating IDs and implanting it inside other commands. For example:
```
127.0.0.1:6379> UNIQUEID.EXEC SET foo $UUIDV4$
1) "a5a3477b-1538-4c85-9485-038f534c35f7"
2) OK

127.0.0.1:6379> GET foo
"a5a3477b-1538-4c85-9485-038f534c35f7"
```

The above command will replace `$UUIDV4$` with a call to `UNIQUEID.UUIDV4` and execute the `SET` command. The returned result includes the unique ID that was generated and the response to the original command.

Supported replacement values:
* $UUIDV1$
* $UUIDV4$
* $SNOWFLAKE$

## Compilation

Just `make` it.

## Usage

`redis-server --loadmodule redisuniquemodule.so [snowflake_region_id] [snowflake_worker_id]`

If `snowflake_region_id` and/or `snowflake_worker_id` are not passed the module will use the default values of 1.

Example:

`redis-server --loadmodule redisuniquemodule.so 1 2`

If you require multiple such server/services in the same region, change the worker_id value.
