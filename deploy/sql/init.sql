-- E2 IoT Platform Database 初始化入口
-- 执行顺序: init_schema.sql → init_data.sql
-- 
-- 用法:
--   mysql -u root -p < deploy/sql/init_schema.sql   # 仅建表
--   mysql -u root -p < deploy/sql/init_data.sql      # 插入测试数据
--   mysql -u root -p < deploy/sql/init.sql           # 全部执行

SOURCE deploy/sql/init_schema.sql;
SOURCE deploy/sql/init_data.sql;
