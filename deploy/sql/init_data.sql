-- E2 IoT Platform 初始数据 (DML)
-- 用于全流程测试：设备接入、告警、影子、指令下发

USE e2_iot;

-- ========================================
-- 1. 用户
-- ========================================
INSERT INTO users (username, password_hash, display_name, role, status) 
VALUES ('admin', SHA2('admin@123', 256), '系统管理员', 'admin', 'active')
ON DUPLICATE KEY UPDATE username=username;

INSERT INTO users (username, password_hash, display_name, role, status) 
VALUES ('operator', SHA2('operator@123', 256), '运维人员', 'user', 'active')
ON DUPLICATE KEY UPDATE username=username;

-- ========================================
-- 2. 产品
-- ========================================
INSERT INTO products (product_id, product_key, product_name, description) VALUES
('PROD001', 'factory_sensor', '工厂传感器', '温湿度气压传感器'),
('PROD002', 'smart_meter', '智能电表', '电压电流功率监测'),
('PROD003', 'env_monitor', '环境监测站', '综合环境监测设备')
ON DUPLICATE KEY UPDATE product_name=product_name;

-- ========================================
-- 3. 设备分组
-- ========================================
INSERT INTO device_groups (group_id, parent_id, group_name, description, sort_order) VALUES
(1, NULL, '未分组', '未分组设备', 0),
(2, 1, '工厂A', '生产基地A', 1),
(3, 2, '车间1', 'A厂一号车间', 1),
(4, 2, '车间2', 'A厂二号车间', 2),
(5, 2, '车间3', 'A厂三号车间', 3),
(6, 1, '工厂B', '生产基地B', 2),
(7, 6, '车间1', 'B厂一号车间', 1),
(8, 6, '仓库', 'B厂原料仓库', 2),
(9, 1, '研发实验室', '新设备测试', 3)
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
-- 5. 设备（MQTT 认证用 product_key + device_secret）
-- ========================================
INSERT INTO devices (product_key, device_id, device_name, device_type, device_secret, status, online, group_id, report_count) VALUES
('factory_sensor', 'dev_001', '车间1温湿度传感器', 'sensor', 'secret_001', 'active', FALSE, 3, 0),
('factory_sensor', 'dev_002', '车间1气压传感器', 'sensor', 'secret_002', 'active', FALSE, 3, 0),
('factory_sensor', 'dev_003', '车间2温湿度传感器', 'sensor', 'secret_003', 'active', FALSE, 4, 0),
('smart_meter', 'dev_004', '车间1电表', 'meter', 'secret_004', 'active', FALSE, 3, 0),
('smart_meter', 'dev_005', '车间2电表', 'meter', 'secret_005', 'active', FALSE, 4, 0),
('env_monitor', 'dev_006', '仓库环境监测站', 'monitor', 'secret_006', 'active', FALSE, 8, 0),
('env_monitor', 'dev_007', '研发实验室监测站', 'monitor', 'secret_007', 'active', FALSE, 9, 0),
('factory_sensor', 'dev_008', '车间3温度传感器', 'sensor', 'secret_008', 'active', FALSE, 5, 0),
('smart_meter', 'dev_009', '仓库电表', 'meter', 'secret_009', 'active', FALSE, 8, 0),
('factory_sensor', 'dev_010', '宿舍温度传感器', 'sensor', 'secret_010', 'registered', FALSE, 9, 0)
ON DUPLICATE KEY UPDATE device_name=device_name;

-- ========================================
-- 6. 设备影子（初始 desired/reported）
-- ========================================
INSERT INTO device_shadows (product_key, device_id, desired, reported, delta) VALUES
('factory_sensor', 'dev_001', '{"temperature":25,"humidity":60}', '{"temperature":23.5,"humidity":65}', '{}'),
('factory_sensor', 'dev_002', '{"pressure":101325}', '{"pressure":101300}', '{}'),
('factory_sensor', 'dev_003', '{"temperature":24,"humidity":55}', '{"temperature":22.8,"humidity":58}', '{}'),
('smart_meter', 'dev_004', '{"voltage":220}', '{"voltage":219.5,"current":10.2,"power":2239}', '{}'),
('smart_meter', 'dev_005', '{"voltage":220}', '{"voltage":221,"current":8.5,"power":1878}', '{}'),
('env_monitor', 'dev_006', '{"temperature":20,"humidity":50}', '{"temperature":19.2,"humidity":52}', '{}'),
('env_monitor', 'dev_007', '{"temperature":22,"humidity":45}', '{"temperature":21.5,"humidity":47}', '{}'),
('factory_sensor', 'dev_008', '{"temperature":26}', '{"temperature":25.3}', '{}'),
('smart_meter', 'dev_009', '{"voltage":220}', '{"voltage":218,"current":5.1,"power":1112}', '{}'),
('factory_sensor', 'dev_010', '{"temperature":25}', '{}', '{}')
ON DUPLICATE KEY UPDATE desired=VALUES(desired);

-- ========================================
-- 7. 告警规则（设备 + 指标 + 阈值）
-- ========================================
INSERT INTO alert_rules (rule_name, device_id, metric, condition_type, threshold, severity, enabled) VALUES
('dev_001 温度超限', 'dev_001', 'temperature', 'gt', 32.0, 'warning', TRUE),
('dev_001 温度严重超限', 'dev_001', 'temperature', 'gt', 38.0, 'critical', TRUE),
('dev_001 湿度过高', 'dev_001', 'humidity', 'gt', 85.0, 'warning', TRUE),
('dev_003 温度超限', 'dev_003', 'temperature', 'gt', 30.0, 'warning', TRUE),
('dev_004 电压异常', 'dev_004', 'voltage', 'lt', 200.0, 'critical', TRUE),
('dev_006 温度过低', 'dev_006', 'temperature', 'lt', 10.0, 'warning', TRUE),
('dev_006 湿度过高', 'dev_006', 'humidity', 'gt', 90.0, 'warning', TRUE),
('全局温度超限', NULL, 'temperature', 'gt', 40.0, 'critical', TRUE)
ON DUPLICATE KEY UPDATE rule_name=rule_name;

-- ========================================
-- 8. 分表（确保当月+下月分表存在）
-- ========================================
CALL sp_ensure_shard_tables();

-- ========================================
-- 9. 插入一些历史数据点（用于查询测试）
-- ========================================
-- 注意：需要先确保分表存在（上面已调用存储过程）
-- 使用当前月份的分表

SET @v_month = DATE_FORMAT(NOW(), '%Y%m');
SET @v_table = CONCAT('data_', @v_month);

-- dev_001 温度历史数据
SET @v_ts = UNIX_TIMESTAMP() - 3600;
SET @v_sql = CONCAT('INSERT INTO ', @v_table, ' (device_id, metric, value, ts) VALUES
(''dev_001'', ''temperature'', 22.5, ', @v_ts, '),
(''dev_001'', ''temperature'', 23.1, ', @v_ts + 60, '),
(''dev_001'', ''temperature'', 23.8, ', @v_ts + 120, '),
(''dev_001'', ''temperature'', 24.2, ', @v_ts + 180, '),
(''dev_001'', ''temperature'', 25.0, ', @v_ts + 240, '),
(''dev_001'', ''temperature'', 25.5, ', @v_ts + 300, '),
(''dev_001'', ''temperature'', 26.1, ', @v_ts + 360, '),
(''dev_001'', ''temperature'', 26.8, ', @v_ts + 420, '),
(''dev_001'', ''temperature'', 27.3, ', @v_ts + 480, '),
(''dev_001'', ''temperature'', 28.0, ', @v_ts + 540, ')');
PREPARE stmt FROM @v_sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- dev_001 湿度历史数据
SET @v_ts = UNIX_TIMESTAMP() - 3600;
SET @v_sql = CONCAT('INSERT INTO ', @v_table, ' (device_id, metric, value, ts) VALUES
(''dev_001'', ''humidity'', 65.0, ', @v_ts, '),
(''dev_001'', ''humidity'', 64.2, ', @v_ts + 60, '),
(''dev_001'', ''humidity'', 63.5, ', @v_ts + 120, '),
(''dev_001'', ''humidity'', 62.8, ', @v_ts + 180, '),
(''dev_001'', ''humidity'', 61.5, ', @v_ts + 240, '),
(''dev_001'', ''humidity'', 60.2, ', @v_ts + 300, '),
(''dev_001'', ''humidity'', 59.8, ', @v_ts + 360, '),
(''dev_001'', ''humidity'', 58.5, ', @v_ts + 420, '),
(''dev_001'', ''humidity'', 57.2, ', @v_ts + 480, '),
(''dev_001'', ''humidity'', 56.0, ', @v_ts + 540, ')');
PREPARE stmt FROM @v_sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- dev_004 电表历史数据
SET @v_ts = UNIX_TIMESTAMP() - 1800;
SET @v_sql = CONCAT('INSERT INTO ', @v_table, ' (device_id, metric, value, ts) VALUES
(''dev_004'', ''voltage'', 220.1, ', @v_ts, '),
(''dev_004'', ''voltage'', 219.8, ', @v_ts + 120, '),
(''dev_004'', ''voltage'', 219.5, ', @v_ts + 240, '),
(''dev_004'', ''voltage'', 220.3, ', @v_ts + 360, '),
(''dev_004'', ''voltage'', 220.0, ', @v_ts + 480, '),
(''dev_004'', ''current'', 10.2, ', @v_ts, '),
(''dev_004'', ''current'', 10.5, ', @v_ts + 120, '),
(''dev_004'', ''current'', 9.8, ', @v_ts + 240, '),
(''dev_004'', ''current'', 10.1, ', @v_ts + 360, '),
(''dev_004'', ''current'', 10.3, ', @v_ts + 480, ')');
PREPARE stmt FROM @v_sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- ========================================
-- 10. 插入一些告警记录（用于告警中心测试）
-- rule_id 对应上面插入的告警规则
-- ========================================
INSERT INTO alerts (rule_id, device_id, metric, current_value, threshold, severity, status) VALUES
(1, 'dev_001', 'temperature', 33.5, 32.0, 'warning', 'active'),
(2, 'dev_001', 'temperature', 39.2, 38.0, 'critical', 'active'),
(3, 'dev_001', 'humidity', 87.3, 85.0, 'warning', 'acknowledged'),
(4, 'dev_003', 'temperature', 31.5, 30.0, 'warning', 'active'),
(5, 'dev_004', 'voltage', 195.2, 200.0, 'critical', 'resolved'),
(7, 'dev_006', 'humidity', 92.1, 90.0, 'warning', 'active');

-- 设置已确认告警的确认信息
UPDATE alerts SET acknowledged_by = 1, acknowledged_at = NOW() - INTERVAL 30 MINUTE 
WHERE status = 'acknowledged';

-- 设置已解决告警的解决信息
UPDATE alerts SET acknowledged_by = 1, acknowledged_at = NOW() - INTERVAL 2 HOUR,
    resolved_by = 1, resolved_at = NOW() - INTERVAL 1 HOUR 
WHERE status = 'resolved';

-- ========================================
-- 完成
-- ========================================
SELECT 'E2 IoT Platform test data initialized' AS status;
SELECT CONCAT('Devices: ', COUNT(*)) FROM devices;
SELECT CONCAT('Alert rules: ', COUNT(*)) FROM alert_rules;
SELECT CONCAT('Alerts: ', COUNT(*)) FROM alerts;
