-- E2 IoT Platform 初始数据 (DML)
-- 全流程测试数据：设备接入、告警、影子、指令下发
-- 
-- 数据设计：
--   产品线: 工厂传感器(factory_sensor) / 智能电表(smart_meter) / 环境监测站(env_monitor)
--   分组: 未分组 / 工厂A / 工厂B / 研发实验室（仅一级，无层级）
--   设备: 10台，每台有唯一 device_id + device_secret，用于 MQTT 认证
--   告警规则: 绑定到具体设备，触发阈值合理
--   历史数据: 最近1小时的温度/湿度/电压数据，用于图表展示

USE e2_iot;

-- ========================================
-- 1. 用户
-- ========================================
INSERT INTO users (username, password_hash, display_name, role, status) VALUES
('admin', SHA2('admin@123', 256), '系统管理员', 'admin', 'active'),
('operator', SHA2('operator@123', 256), '运维人员', 'user', 'active')
ON DUPLICATE KEY UPDATE username=username;

-- ========================================
-- 2. 产品
-- ========================================
INSERT INTO products (product_id, product_key, product_name, description) VALUES
('PROD001', 'factory_sensor', '工厂传感器', '温湿度气压传感器，用于车间环境监测'),
('PROD002', 'smart_meter', '智能电表', '电压电流功率监测，用于能耗管理'),
('PROD003', 'env_monitor', '环境监测站', '综合环境监测设备，用于仓库和实验室')
ON DUPLICATE KEY UPDATE product_name=product_name;

-- ========================================
-- 3. 设备分组（仅一级分组，无层级结构）
-- ========================================
INSERT INTO device_groups (group_id, parent_id, group_name, description, sort_order) VALUES
(1, NULL, '未分组', '新接入或未分配的设备', 0),
(2, NULL, '工厂A', '主生产基地', 1),
(3, NULL, '工厂B', '副生产基地', 2),
(4, NULL, '研发实验室', '新设备测试区域', 3)
ON DUPLICATE KEY UPDATE group_name=group_name;

-- ========================================
-- 4. 数据类型
-- ========================================
INSERT INTO data_types (type_name, unit, description) VALUES
('temperature', '°C', '温度'),
('humidity', '%', '湿度'),
('pressure', 'Pa', '气压'),
('voltage', 'V', '电压'),
('current', 'A', '电流'),
('power', 'W', '功率'),
('energy', 'kWh', '电能')
ON DUPLICATE KEY UPDATE type_name=type_name;

-- ========================================
-- 5. 设备
--    MQTT 认证: product_key + device_secret
--    device_secret 对应 MQTT Broker 硬编码值
-- ========================================
INSERT INTO devices (product_key, device_id, device_name, device_type, device_secret, status, online, group_id) VALUES
('factory_sensor', 'dev_001', '工厂A-温湿度-01', 'sensor', 'secret_001', 'active', FALSE, 2),
('factory_sensor', 'dev_002', '工厂A-温湿度-02', 'sensor', 'secret_002', 'active', FALSE, 2),
('factory_sensor', 'dev_003', '工厂A-温湿度-03', 'sensor', 'secret_003', 'active', FALSE, 2),
('smart_meter',    'dev_004', '工厂A-电表-01',   'meter',  'secret_004', 'active', FALSE, 2),
('smart_meter',    'dev_005', '工厂A-电表-02',   'meter',  'secret_005', 'active', FALSE, 2),
('env_monitor',    'dev_006', '工厂B-环境监测-01', 'monitor','secret_006', 'active', FALSE, 3),
('env_monitor',    'dev_007', '实验室-环境监测-01','monitor','secret_007', 'active', FALSE, 4),
('factory_sensor', 'dev_008', '工厂B-温湿度-01', 'sensor', 'secret_008', 'active', FALSE, 3),
('smart_meter',    'dev_009', '工厂B-电表-01',    'meter',  'secret_009', 'active', FALSE, 3),
('factory_sensor', 'dev_010', '实验室-温湿度-01', 'sensor', 'secret_010', 'registered', FALSE, 4)
ON DUPLICATE KEY UPDATE device_name=device_name;

-- ========================================
-- 6. 设备影子
--    desired: 应用层设置的期望状态
--    reported: 设备上报的实际状态
-- ========================================
INSERT INTO device_shadows (product_key, device_id, desired, reported, delta) VALUES
('factory_sensor', 'dev_001', '{"temperature":25,"humidity":60}',        '{"temperature":23.5,"humidity":65}',      '{}'),
('factory_sensor', 'dev_002', '{"temperature":25,"humidity":60}',        '{"temperature":24.1,"humidity":62}',      '{}'),
('factory_sensor', 'dev_003', '{"temperature":24,"humidity":55}',        '{"temperature":22.8,"humidity":58}',      '{}'),
('smart_meter',    'dev_004', '{"voltage":220}',                         '{"voltage":219.5,"current":10.2}',       '{}'),
('smart_meter',    'dev_005', '{"voltage":220}',                         '{"voltage":221.0,"current":8.5}',        '{}'),
('env_monitor',    'dev_006', '{"temperature":20,"humidity":50}',        '{"temperature":19.2,"humidity":52}',      '{}'),
('env_monitor',    'dev_007', '{"temperature":22,"humidity":45}',        '{"temperature":21.5,"humidity":47}',      '{}'),
('factory_sensor', 'dev_008', '{"temperature":26}',                      '{"temperature":25.3}',                   '{}'),
('smart_meter',    'dev_009', '{"voltage":220}',                         '{"voltage":218.0,"current":5.1}',        '{}'),
('factory_sensor', 'dev_010', '{"temperature":25}',                      '{}',                                     '{}')
ON DUPLICATE KEY UPDATE desired=VALUES(desired);

-- ========================================
-- 7. 告警规则
--    绑定到具体设备，阈值符合实际场景
-- ========================================
INSERT INTO alert_rules (rule_name, device_id, metric, condition_type, threshold, severity, enabled) VALUES
('车间1温度过高警告',    'dev_001', 'temperature', 'gt', 32.0, 'warning',  TRUE),
('车间1温度严重超限',    'dev_001', 'temperature', 'gt', 38.0, 'critical', TRUE),
('车间1湿度过高',        'dev_001', 'humidity',    'gt', 85.0, 'warning',  TRUE),
('车间2温度过高警告',    'dev_003', 'temperature', 'gt', 30.0, 'warning',  TRUE),
('车间1电压过低',        'dev_004', 'voltage',     'lt', 200.0,'critical', TRUE),
('仓库温度过低',         'dev_006', 'temperature', 'lt', 10.0, 'warning',  TRUE),
('仓库湿度过高',         'dev_006', 'humidity',    'gt', 90.0, 'warning',  TRUE),
('全局温度极端超限',     NULL,      'temperature', 'gt', 40.0, 'critical', TRUE)
ON DUPLICATE KEY UPDATE rule_name=rule_name;

-- ========================================
-- 8. 创建当月+下月分表
-- ========================================
CALL sp_ensure_shard_tables();

-- ========================================
-- 9. 历史数据点（最近1小时，用于图表展示）
--    dev_001: 温度 + 湿度（每分钟一条，共60条）
--    dev_004: 电压 + 电流（每2分钟一条，共30条）
-- ========================================
SET @v_month = DATE_FORMAT(NOW(), '%Y%m');
SET @v_table = CONCAT('data_reports_', @v_month);
SET @v_ts = UNIX_TIMESTAMP() - 3600;

-- dev_001 温度: 22°C → 28°C 缓慢上升
SET @v_sql = CONCAT('INSERT INTO ', @v_table, ' (device_id, metric, value, ts) VALUES ',
    "('dev_001','temperature',22.0,", @v_ts, "),",
    "('dev_001','temperature',22.2,", @v_ts+60, "),",
    "('dev_001','temperature',22.5,", @v_ts+120, "),",
    "('dev_001','temperature',22.9,", @v_ts+180, "),",
    "('dev_001','temperature',23.3,", @v_ts+240, "),",
    "('dev_001','temperature',23.6,", @v_ts+300, "),",
    "('dev_001','temperature',24.0,", @v_ts+360, "),",
    "('dev_001','temperature',24.3,", @v_ts+420, "),",
    "('dev_001','temperature',24.7,", @v_ts+480, "),",
    "('dev_001','temperature',25.0,", @v_ts+540, "),",
    "('dev_001','temperature',25.3,", @v_ts+600, "),",
    "('dev_001','temperature',25.5,", @v_ts+660, "),",
    "('dev_001','temperature',25.8,", @v_ts+720, "),",
    "('dev_001','temperature',26.0,", @v_ts+780, "),",
    "('dev_001','temperature',26.2,", @v_ts+840, "),",
    "('dev_001','temperature',26.5,", @v_ts+900, "),",
    "('dev_001','temperature',26.7,", @v_ts+960, "),",
    "('dev_001','temperature',26.9,", @v_ts+1020, "),",
    "('dev_001','temperature',27.1,", @v_ts+1080, "),",
    "('dev_001','temperature',27.3,", @v_ts+1140, "),",
    "('dev_001','temperature',27.5,", @v_ts+1200, "),",
    "('dev_001','temperature',27.6,", @v_ts+1260, "),",
    "('dev_001','temperature',27.8,", @v_ts+1320, "),",
    "('dev_001','temperature',27.9,", @v_ts+1380, "),",
    "('dev_001','temperature',28.0,", @v_ts+1440, ")");
PREPARE stmt FROM @v_sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- dev_001 湿度: 65% → 55% 缓慢下降
SET @v_sql = CONCAT('INSERT INTO ', @v_table, ' (device_id, metric, value, ts) VALUES ',
    "('dev_001','humidity',65.0,", @v_ts, "),",
    "('dev_001','humidity',64.5,", @v_ts+60, "),",
    "('dev_001','humidity',64.0,", @v_ts+120, "),",
    "('dev_001','humidity',63.6,", @v_ts+180, "),",
    "('dev_001','humidity',63.2,", @v_ts+240, "),",
    "('dev_001','humidity',62.8,", @v_ts+300, "),",
    "('dev_001','humidity',62.4,", @v_ts+360, "),",
    "('dev_001','humidity',62.0,", @v_ts+420, "),",
    "('dev_001','humidity',61.6,", @v_ts+480, "),",
    "('dev_001','humidity',61.2,", @v_ts+540, "),",
    "('dev_001','humidity',60.8,", @v_ts+600, "),",
    "('dev_001','humidity',60.4,", @v_ts+660, "),",
    "('dev_001','humidity',60.0,", @v_ts+720, "),",
    "('dev_001','humidity',59.6,", @v_ts+780, "),",
    "('dev_001','humidity',59.2,", @v_ts+840, "),",
    "('dev_001','humidity',58.8,", @v_ts+900, "),",
    "('dev_001','humidity',58.4,", @v_ts+960, "),",
    "('dev_001','humidity',58.0,", @v_ts+1020, "),",
    "('dev_001','humidity',57.6,", @v_ts+1080, "),",
    "('dev_001','humidity',57.2,", @v_ts+1140, "),",
    "('dev_001','humidity',56.8,", @v_ts+1200, "),",
    "('dev_001','humidity',56.4,", @v_ts+1260, "),",
    "('dev_001','humidity',56.0,", @v_ts+1320, "),",
    "('dev_001','humidity',55.6,", @v_ts+1380, "),",
    "('dev_001','humidity',55.2,", @v_ts+1440, ")");
PREPARE stmt FROM @v_sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- dev_004 电压: 220V 附近波动
SET @v_ts2 = UNIX_TIMESTAMP() - 1800;
SET @v_sql = CONCAT('INSERT INTO ', @v_table, ' (device_id, metric, value, ts) VALUES ',
    "('dev_004','voltage',220.1,", @v_ts2, "),",
    "('dev_004','voltage',219.8,", @v_ts2+120, "),",
    "('dev_004','voltage',219.5,", @v_ts2+240, "),",
    "('dev_004','voltage',219.9,", @v_ts2+360, "),",
    "('dev_004','voltage',220.2,", @v_ts2+480, "),",
    "('dev_004','voltage',220.0,", @v_ts2+600, "),",
    "('dev_004','voltage',219.7,", @v_ts2+720, "),",
    "('dev_004','voltage',220.3,", @v_ts2+840, "),",
    "('dev_004','voltage',219.6,", @v_ts2+960, "),",
    "('dev_004','voltage',220.1,", @v_ts2+1080, ")");
PREPARE stmt FROM @v_sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- dev_004 电流: 10A 附近波动
SET @v_sql = CONCAT('INSERT INTO ', @v_table, ' (device_id, metric, value, ts) VALUES ',
    "('dev_004','current',10.2,", @v_ts2, "),",
    "('dev_004','current',10.5,", @v_ts2+120, "),",
    "('dev_004','current',9.8,", @v_ts2+240, "),",
    "('dev_004','current',10.1,", @v_ts2+360, "),",
    "('dev_004','current',10.4,", @v_ts2+480, "),",
    "('dev_004','current',10.0,", @v_ts2+600, "),",
    "('dev_004','current',10.3,", @v_ts2+720, "),",
    "('dev_004','current',9.9,", @v_ts2+840, "),",
    "('dev_004','current',10.6,", @v_ts2+960, "),",
    "('dev_004','current',10.2,", @v_ts2+1080, ")");
PREPARE stmt FROM @v_sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- ========================================
-- 10. 告警记录（用于告警中心展示）
--     rule_id 对应上面的告警规则
--     状态: active(未确认) / acknowledged(已确认) / resolved(已解决)
-- ========================================
INSERT INTO alerts (rule_id, device_id, metric, current_value, threshold, severity, status) VALUES
(1, 'dev_001', 'temperature', 33.5, 32.0, 'warning',  'active'),        -- 车间1温度超限
(2, 'dev_001', 'temperature', 39.2, 38.0, 'critical', 'active'),        -- 车间1温度严重超限
(3, 'dev_001', 'humidity',    87.3, 85.0, 'warning',  'acknowledged'),  -- 车间1湿度过高（已确认）
(4, 'dev_003', 'temperature', 31.5, 30.0, 'warning',  'active'),        -- 车间2温度超限
(5, 'dev_004', 'voltage',     195.2, 200.0, 'critical', 'resolved'),    -- 车间1电压过低（已解决）
(7, 'dev_006', 'humidity',    92.1, 90.0, 'warning',  'active');        -- 仓库湿度过高

-- 已确认告警：设置确认人和时间
UPDATE alerts SET acknowledged_by = 1, acknowledged_at = NOW() - INTERVAL 30 MINUTE 
WHERE status = 'acknowledged';

-- 已解决告警：设置确认人、确认时间、解决人、解决时间
UPDATE alerts SET 
    acknowledged_by = 1, acknowledged_at = NOW() - INTERVAL 2 HOUR,
    resolved_by = 1, resolved_at = NOW() - INTERVAL 1 HOUR 
WHERE status = 'resolved';

-- ========================================
-- 完成
-- ========================================
SELECT 'E2 IoT Platform test data initialized' AS status;
SELECT CONCAT('Users: ', COUNT(*)) FROM users;
SELECT CONCAT('Products: ', COUNT(*)) FROM products;
SELECT CONCAT('Groups: ', COUNT(*)) FROM device_groups;
SELECT CONCAT('Devices: ', COUNT(*)) FROM devices;
SELECT CONCAT('Alert rules: ', COUNT(*)) FROM alert_rules;
SELECT CONCAT('Alerts: ', COUNT(*)) FROM alerts;
