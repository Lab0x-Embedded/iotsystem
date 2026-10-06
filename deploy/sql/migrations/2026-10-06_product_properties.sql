-- 轻量物模型：产品级属性白名单
-- 上报校验规则：产品未定义任何属性 = 自由模式（全部放行）；
-- 定义了属性后，未在白名单中的 identifier 拒绝入库（对齐 OneNET 10411 语义）。
CREATE TABLE IF NOT EXISTS product_properties (
    id INT PRIMARY KEY AUTO_INCREMENT,
    product_key VARCHAR(64) NOT NULL,
    identifier VARCHAR(64) NOT NULL,
    prop_type ENUM('number','bool','string') NOT NULL DEFAULT 'number',
    description VARCHAR(255) DEFAULT '',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uk_pk_ident (product_key, identifier)
) ENGINE=InnoDB;
