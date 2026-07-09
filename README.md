## 物联网设备管理云平台


## 快速开始
- 初始化数据库：
```bash
mysql -u root -p < deploy/sql/init.sql
```

- 启动服务：
```bash
./build/iot-broker --config deploy/config.json
```

- qt client:
```bash
cd client
cmake -B build
cmake --build build
# 启动app
open ./build/IoTDeviceManager.app
```