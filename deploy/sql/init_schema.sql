-- E2 IoT Platform Database Schema (DDL)
-- MySQL 8.0+ / utf8mb4 / InnoDB
-- 仅表结构，不含数据

-- 创建数据库
CREATE DATABASE IF NOT EXISTS e2_iot 
    CHARACTER SET utf8mb4 
    COLLATE utf8mb4_unicode_ci;

USE e2_iot;

-- 用户表
CREATE TABLE IF NOT EXISTS users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    display_name VARCHAR(128),
    role ENUM('admin', 'user', 'viewer') DEFAULT 'user',
    status ENUM('active', 'disabled') DEFAULT 'active',
    last_login DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_users_username (username),
    INDEX idx_users_status (status)
) ENGINE=InnoDB;

-- 产品表
CREATE TABLE IF NOT EXISTS products (
    id INT PRIMARY KEY AUTO_INCREMENT,
    product_id VARCHAR(64) NOT NULL UNIQUE,
    product_key VARCHAR(64) NOT NULL UNIQUE,
    product_name VARCHAR(128),
    description TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_products_key (product_key)
) ENGINE=InnoDB;

-- 设备分组表
CREATE TABLE IF NOT EXISTS device_groups (
    group_id INT PRIMARY KEY AUTO_INCREMENT,
    parent_id INT,
    group_name VARCHAR(128) NOT NULL,
    description TEXT,
    sort_order INT DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_groups_parent (parent_id),
    FOREIGN KEY (parent_id) REFERENCES device_groups(group_id) ON DELETE SET NULL
) ENGINE=InnoDB;

-- 设备表
CREATE TABLE IF NOT EXISTS devices (
    id INT PRIMARY KEY AUTO_INCREMENT,
    product_key VARCHAR(64) NOT NULL,
    device_id VARCHAR(64) NOT NULL UNIQUE,
    device_name VARCHAR(128),
    device_type VARCHAR(64),
    device_secret VARCHAR(512),
    status ENUM('registered', 'active', 'disabled', 'decommissioned') DEFAULT 'registered',
    online BOOLEAN DEFAULT FALSE,
    last_online DATETIME,
    group_id INT,
    report_count INT DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_devices_product (product_key),
    INDEX idx_devices_device (device_id),
    INDEX idx_devices_group (group_id),
    INDEX idx_devices_status (status),
    FOREIGN KEY (product_key) REFERENCES products(product_key),
    FOREIGN KEY (group_id) REFERENCES device_groups(group_id) ON DELETE SET NULL
) ENGINE=InnoDB;

-- 设备分组成员表
CREATE TABLE IF NOT EXISTS device_group_members (
    device_id VARCHAR(64) NOT NULL,
    group_id INT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (device_id, group_id),
    FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE,
    FOREIGN KEY (group_id) REFERENCES device_groups(group_id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 设备影子表
CREATE TABLE IF NOT EXISTS device_shadows (
    id INT PRIMARY KEY AUTO_INCREMENT,
    product_key VARCHAR(64) NOT NULL,
    device_id VARCHAR(64) NOT NULL,
    desired JSON,
    reported JSON,
    delta JSON,
    version INT DEFAULT 1,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_shadow_device (product_key, device_id),
    FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 数据类型表
CREATE TABLE IF NOT EXISTS data_types (
    id INT PRIMARY KEY AUTO_INCREMENT,
    type_name VARCHAR(64) NOT NULL UNIQUE,
    unit VARCHAR(32),
    description TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

-- 数据记录表模板（按月分表）
CREATE TABLE IF NOT EXISTS data_records_template (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    device_id VARCHAR(64) NOT NULL,
    metric VARCHAR(64) NOT NULL,
    value DOUBLE,
    ts BIGINT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_data_device_ts (device_id, ts),
    INDEX idx_data_metric (metric),
    INDEX idx_data_ts (ts)
) ENGINE=InnoDB;

-- 设备最新数据表
CREATE TABLE IF NOT EXISTS device_latest_data (
    id INT PRIMARY KEY AUTO_INCREMENT,
    device_id VARCHAR(64) NOT NULL,
    metric VARCHAR(64) NOT NULL,
    value DOUBLE,
    ts BIGINT NOT NULL,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    UNIQUE KEY uk_latest_device_metric (device_id, metric),
    FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 设备事件表
CREATE TABLE IF NOT EXISTS device_events (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    product_key VARCHAR(64) NOT NULL,
    device_id VARCHAR(64) NOT NULL,
    event_type VARCHAR(64) NOT NULL,
    event_data JSON,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_events_device (device_id),
    INDEX idx_events_type (event_type),
    INDEX idx_events_time (created_at),
    FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 告警规则表
CREATE TABLE IF NOT EXISTS alert_rules (
    rule_id INT PRIMARY KEY AUTO_INCREMENT,
    rule_name VARCHAR(128) NOT NULL,
    product_key VARCHAR(64),
    device_id VARCHAR(64),
    metric VARCHAR(64) NOT NULL,
    condition_type ENUM('gt', 'lt', 'eq', 'ne', 'gte', 'lte') NOT NULL,
    threshold DOUBLE NOT NULL,
    severity ENUM('info', 'warning', 'critical') DEFAULT 'warning',
    enabled BOOLEAN DEFAULT TRUE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_rules_product (product_key),
    INDEX idx_rules_device (device_id),
    INDEX idx_rules_enabled (enabled)
) ENGINE=InnoDB;

-- 告警记录表
CREATE TABLE IF NOT EXISTS alerts (
    id INT PRIMARY KEY AUTO_INCREMENT,
    rule_id INT,
    device_id VARCHAR(64) NOT NULL,
    metric VARCHAR(64) NOT NULL,
    current_value DOUBLE,
    threshold DOUBLE,
    severity ENUM('info', 'warning', 'critical') DEFAULT 'warning',
    status ENUM('active', 'acknowledged', 'resolved') DEFAULT 'active',
    acknowledged_by INT,
    acknowledged_at DATETIME,
    resolved_by INT,
    resolved_at DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_alerts_device (device_id),
    INDEX idx_alerts_status (status),
    INDEX idx_alerts_severity (severity),
    INDEX idx_alerts_time (created_at),
    INDEX idx_alerts_dedup (rule_id, device_id, metric, status),
    FOREIGN KEY (rule_id) REFERENCES alert_rules(rule_id) ON DELETE SET NULL,
    FOREIGN KEY (device_id) REFERENCES devices(device_id) ON DELETE CASCADE,
    FOREIGN KEY (acknowledged_by) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY (resolved_by) REFERENCES users(id) ON DELETE SET NULL
) ENGINE=InnoDB;

-- 存储过程：确保分表存在
DROP PROCEDURE IF EXISTS sp_ensure_shard_tables;
DELIMITER $$
CREATE PROCEDURE sp_ensure_shard_tables()
BEGIN
    SET @v_month = DATE_FORMAT(NOW(), '%Y%m');
    SET @v_table_name = CONCAT('data_reports_', @v_month);
    SET @v_next_month = DATE_FORMAT(DATE_ADD(NOW(), INTERVAL 1 MONTH), '%Y%m');
    SET @v_next_table_name = CONCAT('data_reports_', @v_next_month);
    
    SET @v_sql = CONCAT('CREATE TABLE IF NOT EXISTS `', @v_table_name, 
                       '` LIKE data_records_template');
    PREPARE stmt FROM @v_sql;
    EXECUTE stmt;
    DEALLOCATE PREPARE stmt;
    
    SET @v_sql = CONCAT('CREATE TABLE IF NOT EXISTS `', @v_next_table_name, 
                       '` LIKE data_records_template');
    PREPARE stmt FROM @v_sql;
    EXECUTE stmt;
    DEALLOCATE PREPARE stmt;
    
    SELECT CONCAT('Tables ensured: ', @v_table_name, ', ', @v_next_table_name) AS result;
END$$
DELIMITER ;

-- 存储过程：批量注册设备
DROP PROCEDURE IF EXISTS sp_batch_register_devices;
DELIMITER $$
CREATE PROCEDURE sp_batch_register_devices(
    IN p_product_key VARCHAR(32),
    IN p_device_type VARCHAR(32),
    IN p_group_id INT,
    IN p_count INT
)
BEGIN
    DECLARE v_i INT DEFAULT 0;
    DECLARE v_device_id VARCHAR(32);
    DECLARE v_secret VARCHAR(512);
    
    WHILE v_i < p_count DO
        SET v_device_id = REPLACE(UUID(), '-', '');
        SET v_device_id = CONCAT('dev_', LEFT(v_device_id, 16));
        SET v_secret = TO_BASE64(SHA2(CONCAT(v_device_id, NOW(), RAND()), 256));
        
        INSERT IGNORE INTO devices 
            (product_key, device_id, device_type, device_secret, group_id, status)
        VALUES 
            (p_product_key, v_device_id, p_device_type, v_secret, p_group_id, 'registered');
        
        INSERT IGNORE INTO device_shadows (product_key, device_id, desired, reported, delta)
        VALUES (p_product_key, v_device_id, '{}', '{}', '{}');
        
        SET v_i = v_i + 1;
    END WHILE;
    
    SELECT CONCAT(p_count, ' devices registered') AS result;
END$$
DELIMITER ;

-- 视图：设备统计
CREATE OR REPLACE VIEW v_device_stats AS
SELECT 
    d.product_key,
    COUNT(*) AS total_devices,
    SUM(CASE WHEN d.online = 1 THEN 1 ELSE 0 END) AS online_devices,
    SUM(CASE WHEN d.online = 0 THEN 1 ELSE 0 END) AS offline_devices,
    ROUND(SUM(CASE WHEN d.online = 1 THEN 1 ELSE 0 END) / COUNT(*) * 100, 1) AS online_rate,
    MAX(d.last_online) AS latest_activity
FROM devices d
WHERE d.status != 'decommissioned'
GROUP BY d.product_key;

-- 轻量物模型：产品级属性白名单（见 migrations/2026-10-06_product_properties.sql）
CREATE TABLE IF NOT EXISTS product_properties (
    id INT PRIMARY KEY AUTO_INCREMENT,
    product_key VARCHAR(64) NOT NULL,
    identifier VARCHAR(64) NOT NULL,
    prop_type ENUM('number','bool','string') NOT NULL DEFAULT 'number',
    description VARCHAR(255) DEFAULT '',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_pk_ident (product_key, identifier)
) ENGINE=InnoDB;
