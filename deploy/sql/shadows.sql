-- factory_sensor（1~8）

UPDATE device_shadows
SET
desired='{"sample_interval":10,"upload_interval":60,"temp_alarm":35,"humidity_alarm":80}',
reported='{"online":true,"sample_interval":15,"upload_interval":60,"temperature":27.3,"humidity":61.8,"battery":96}',
delta='{"sample_interval":10}'
WHERE id=1;

UPDATE device_shadows
SET
desired='{"sample_interval":10,"upload_interval":60,"temp_alarm":35,"humidity_alarm":80}',
reported='{"online":true,"sample_interval":10,"upload_interval":60,"temperature":28.1,"humidity":58.2,"battery":91}',
delta='{}'
WHERE id=2;

UPDATE device_shadows
SET
desired='{"sample_interval":5,"upload_interval":30,"temp_alarm":40,"humidity_alarm":85}',
reported='{"online":true,"sample_interval":10,"upload_interval":30,"temperature":31.5,"humidity":65.1,"battery":84}',
delta='{"sample_interval":5}'
WHERE id=3;

UPDATE device_shadows
SET
desired='{"sample_interval":15,"upload_interval":120,"temp_alarm":38,"humidity_alarm":85}',
reported='{"online":false,"sample_interval":15,"upload_interval":120,"temperature":29.0,"humidity":56.3,"battery":77}',
delta='{}'
WHERE id=4;

UPDATE device_shadows
SET
desired='{"sample_interval":20,"upload_interval":120,"temp_alarm":45,"humidity_alarm":90}',
reported='{"online":true,"sample_interval":20,"upload_interval":120,"temperature":34.7,"humidity":72.4,"battery":88}',
delta='{}'
WHERE id=5;

UPDATE device_shadows
SET
desired='{"sample_interval":10,"upload_interval":60,"temp_alarm":36,"humidity_alarm":80}',
reported='{"online":true,"sample_interval":30,"upload_interval":60,"temperature":30.2,"humidity":67.9,"battery":65}',
delta='{"sample_interval":10}'
WHERE id=6;

UPDATE device_shadows
SET
desired='{"sample_interval":10,"upload_interval":60,"temp_alarm":35,"humidity_alarm":80}',
reported='{"online":true,"sample_interval":10,"upload_interval":60,"temperature":26.4,"humidity":54.1,"battery":98}',
delta='{}'
WHERE id=7;

UPDATE device_shadows
SET
desired='{"sample_interval":5,"upload_interval":30,"temp_alarm":42,"humidity_alarm":88}',
reported='{"online":true,"sample_interval":5,"upload_interval":30,"temperature":38.9,"humidity":69.5,"battery":73}',
delta='{}'
WHERE id=8;


-- smart_meter（9~10）

UPDATE device_shadows
SET
desired='{"report_interval":300,"power_limit":5000}',
reported='{"online":true,"report_interval":300,"voltage":220.8,"current":6.32,"power":1395.5,"energy":1234.7}',
delta='{}'
WHERE id=9;

UPDATE device_shadows
SET
desired='{"report_interval":300,"power_limit":4500}',
reported='{"online":true,"report_interval":600,"voltage":221.4,"current":5.76,"power":1274.8,"energy":956.3}',
delta='{"report_interval":300}'
WHERE id=10;

-- env_monitor（11~12）
UPDATE device_shadows
SET
desired='{"sample_interval":30,"co2_alarm":1000,"pm25_alarm":75}',
reported='{"online":true,"sample_interval":30,"temperature":24.8,"humidity":48.2,"co2":623,"pm25":18}',
delta='{}'
WHERE id=11;

UPDATE device_shadows
SET
desired='{"sample_interval":30,"co2_alarm":800,"pm25_alarm":50}',
reported='{"online":true,"sample_interval":60,"temperature":26.1,"humidity":52.8,"co2":912,"pm25":63}',
delta='{"sample_interval":30}'
WHERE id=12;