.PHONY: build run db_init register_devices dev clean help

build:
	@mkdir -p build && cd build && cmake .. && make -j$$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

run: build
	@./build/iot-broker --config deploy/config.json

db_init:
	@mysql -u root -p < deploy/sql/init.sql

register_devices:
	@bash deploy/register_devices.sh

dev: build
	@./build/iot-broker --config deploy/config.json

clean:
	@rm -rf build/*
	@echo "Build directory cleaned"

help:
	@echo "可用命令:"
	@echo "  make build            - 编译项目"
	@echo "  make run              - 启动服务"
	@echo "  make db_init          - 初始化数据库"
	@echo "  make register_devices - 注册测试设备"
	@echo "  make dev              - 编译并启动"
	@echo "  make clean            - 清理构建"
	@echo "  make help             - 查看帮助"
