-- 2026-09-17 device presence 索引
--
-- 背景: device_manager 的在线巡检 (`WHERE online=1 AND last_online < NOW() - INTERVAL n SECOND`)
--       与在线数统计 (`WHERE online=1`) 原先无可用索引 → devices 全表扫描。
-- 影响: 只加索引, 不改列/数据; 回滚见文末。
--
-- 执行前建议备份:
--   mysqldump -h127.0.0.1 -u<user> -p e2_iot devices > devices_bak.sql

ALTER TABLE devices ADD INDEX idx_devices_online_last (online, last_online);

-- 回滚:
-- ALTER TABLE devices DROP INDEX idx_devices_online_last;
