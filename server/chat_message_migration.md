# 聊天消息客户端标识迁移

消息幂等逻辑依赖 `chat_message.client_msg_id` 和联合唯一索引。升级前请确认目标数据库表结构，并备份数据；以下 SQL 只需在尚未添加字段和索引的目标库执行一次。

```sql
ALTER TABLE chat_message
    ADD COLUMN client_msg_id VARCHAR(64)
        CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
    ADD UNIQUE KEY uk_sender_client_msg_id (sender_id, client_msg_id);
```

旧消息的标识为 NULL；新服务使用客户端消息 ID 识别重试。仅执行迁移不会让旧进程自动写入该字段，需要重新构建并启动新服务。消息去重不保证通知一定送达。
