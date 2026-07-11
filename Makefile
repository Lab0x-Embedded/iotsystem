.PHONY: build server db_init register_devices client client-dev simulator clean help

# 编译服务端
build:
	@mkdir -p build && cd build && cmake .. && make -j$$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# 启动服务端
server: build
	@./build/iot-broker --config deploy/config.json

# 初始化数据库
db_init:
	@mysql -u root -p < deploy/sql/init.sql

# 注册测试设备
register_devices:
	@python3 deploy/scripts/register_devices.py

# 编译并启动 Qt 客户端
client:
	@cd client && cmake -B build && cmake --build build
	@open client/build/IoTDeviceManager.app

client-dev:
	@cd client && cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
	@client/build/IoTDeviceManager.app/Contents/MacOS/IoTDeviceManager

# 启动 MQTT 模拟器
simulator:
	@python3 deploy/mqtt_simulator.py

# 清理构建文件
clean:
	@rm -rf build/*
	@echo "Build directory cleaned"

# 帮助
help:
	@echo "可用命令:"
	@echo "  make build              - 编译 IoT Broker 服务端"
	@echo "  make server             - 启动服务端"
	@echo "  make client             - 编译并启动 Qt 客户端"
	@echo "  make client-dev         - 启动 Qt 测试客户端"
	@echo "  make db_init            - 初始化 MySQL 数据库"
	@echo "  make register_devices   - 注册测试设备"
	@echo "  make simulator          - 启动 MQTT 设备模拟器"
	@echo "  make clean              - 清理构建目录"
	@echo "  make help               - 查看帮助"