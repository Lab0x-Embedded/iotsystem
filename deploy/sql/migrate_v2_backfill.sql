# E2 IoT Platform v2 数据补齐
# 用途: 为存量设备补全 device_type / device_secret, 使其能被新代码正确加载和认证
# 运行时机: 在部署新代码前执行一次

USE e2_iot;

-- 1. 补全 device_type (默认 'unknown')
UPDATE devices
   SET device_type = 'unknown'
 WHERE device_type IS NULL
    OR device_type = '';

-- 2. 补全 device_secret (生成一个基于 device_id 的确定性 token)
UPDATE devices
   SET device_secret = LEFT(SHA2(CONCAT(device_id, '@migrate_v2'), 256), 32)
 WHERE device_secret IS NULL
    OR device_secret = '';

-- 3. 同步 online 字段与 status 的一致性
UPDATE devices
   SET online = 1
 WHERE status = 'active'
   AND online = 0;

-- 4. 复核: 列出仍缺失关键字段的设备 (应为空)
SELECT device_id, product_key,
       device_type AS dt,
       device_secret AS ds,
       status, online
  FROM devices
 WHERE device_type IS NULL
    OR device_type = ''
    OR device_secret IS NULL
    OR device_secret = '';

-- 5. 输出统计
SELECT COUNT(*) AS total_devices,
       SUM(device_secret IS NOT NULL AND device_secret != '') AS with_secret,
       SUM(device_type IS NOT NULL AND device_type != '') AS with_type,
       SUM(online) AS online_count,
       SUM(status = 'active') AS active_count
  FROM devices;

SELECT 'migrate_v2_backfill done' AS status;
