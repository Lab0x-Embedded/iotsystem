-- ===========================================================================
-- E2 IoT Platform — seed_data.sql
--   种子数据: 默认告警规则 + 存储过程 + 设备种子
--
--   调用方式 (在仓库根目录执行):
--
--     mysql -u root < deploy/sql/seed_data.sql
--
--   前提: 先跑过一次 schema init (通常就是 init.sql) 把表结构建好
--   注意: 默认密码 admin@123 对应的账号 users.admin 会在 seed 里建
-- ===========================================================================

USE e2_iot;

-- ----------------------------------------------------------------------
-- 0. 确保 users 表里有 admin (兼容已经/未跑 init.sql 两种情况)
-- ----------------------------------------------------------------------
INSERT IGNORE INTO users (username, password_hash, display_name, role, status)
VALUES ('admin', SHA2('admin@123', 256), '系统管理员', 'admin', 'active');

-- ----------------------------------------------------------------------
-- 1. 默认告警规则  (alert_rules 表)
-- ----------------------------------------------------------------------
INSERT IGNORE INTO alert_rules
       (rule_name, device_id, metric, condition_type, threshold, severity, enabled)
VALUES
  ('temperature-warning',  NULL, 'temperature', 'gt',  30.0, 'warning',  1),
  ('temperature-critical', NULL, 'temperature', 'gt',  35.0, 'critical', 1),
  ('humidity-warning',     NULL, 'humidity',    'gt',  85.0, 'warning',  1),
  ('power-warning',        NULL, 'power',       'gt', 5000.0, 'warning', 1);

-- ----------------------------------------------------------------------
-- 2. 存储过程: 批量注册设备
-- ----------------------------------------------------------------------
DROP PROCEDURE IF EXISTS sp_batch_register_devices;
DELIMITER $$
CREATE PROCEDURE sp_batch_register_devices(
    IN p_product_key  VARCHAR(32),
    IN p_device_type  VARCHAR(32),
    IN p_group_id     INT,
    IN p_count        INT
)
BEGIN
    DECLARE v_i      INT DEFAULT 0;
    DECLARE v_did    VARCHAR(32);
    DECLARE v_secret VARCHAR(512);
    DECLARE v_now    DATETIME DEFAULT NOW();

    WHILE v_i < p_count DO
        SET v_did    = CONCAT('dev_', LEFT(REPLACE(UUID(), '-', ''), 16));
        SET v_secret = TO_BASE64(SHA2(CONCAT(v_did, v_now, RAND()), 256));

        INSERT IGNORE INTO devices
            (product_key, device_id, device_type, device_secret, group_id, status)
        VALUES
            (p_product_key, v_did, p_device_type, v_secret, p_group_id, 'registered');

        INSERT IGNORE INTO device_shadows
            (product_key, device_id, desired, reported, delta)
        VALUES
            (p_product_key, v_did, '{}', '{}', '{}');

        SET v_i = v_i + 1;
    END WHILE;

    SELECT CONCAT(p_count, ' devices registered (product=', p_product_key,
                  ', group=', p_group_id, ')') AS result;
END$$
DELIMITER ;

-- ----------------------------------------------------------------------
-- 3. 设备种子
-- ----------------------------------------------------------------------
CALL sp_batch_register_devices('factory_sensor', 'temperature', 3, 3);   -- 工厂A 车间1: 3台
CALL sp_batch_register_devices('factory_sensor', 'temperature', 4, 3);   -- 工厂A 车间2: 3台
CALL sp_batch_register_devices('factory_sensor', 'humidity',    5, 2);   -- 工厂A 车间3: 2台
CALL sp_batch_register_devices('smart_meter',    'power',       8, 2);   -- 工厂B 仓库 : 2台
CALL sp_batch_register_devices('env_monitor',    'temperature', 9, 2);   -- 研发实验室    : 2台

SELECT CONCAT('Seed summary:  users=', (SELECT COUNT(*) FROM users),
              '  rules=',   (SELECT COUNT(*) FROM alert_rules),
              '  devices=', (SELECT COUNT(*) FROM devices)) AS init_summary;


-- ----------------------------------------------------------------------
-- 6. 存量数据兜底 (原 migrate_v2_backfill.sql 已合并至此, 幂等)
--     兼容: 已经通过 sp_batch_register_devices 注册的设备不会再被改动
-- ----------------------------------------------------------------------
UPDATE devices SET device_type = 'unknown'
 WHERE device_type IS NULL OR device_type = '';

UPDATE devices SET device_secret = LEFT(SHA2(CONCAT(device_id, '@migrate_v2'), 256), 32)
 WHERE device_secret IS NULL OR device_secret = '';

UPDATE devices SET online = 1
 WHERE status = 'active' AND online = 0;

-- ----------------------------------------------------------------------
-- 7. 复核 (debug 用)
-- ----------------------------------------------------------------------
SELECT COUNT(*) AS total_devices,
       SUM(device_secret IS NOT NULL AND device_secret != '') AS with_secret,
       SUM(device_type  IS NOT NULL AND device_type  != '') AS with_type,
       SUM(online) AS online_count,
       SUM(status = 'active') AS active_count
  FROM devices;
