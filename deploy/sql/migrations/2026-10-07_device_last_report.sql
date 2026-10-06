-- 设备真实上报追踪: publish_worker 入库成功时刷新
ALTER TABLE devices ADD COLUMN last_report_at DATETIME NULL;
