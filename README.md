## 物联网设备管理云平台

## 快捷操作

```bash
make build            # 编译
make run              # 启动
make dev              # 编译并启动
make db_init          # 初始化数据库
make register_devices # 注册设备
```

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