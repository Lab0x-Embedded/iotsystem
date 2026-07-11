#!/usr/bin/env python3
"""
批量注册测试设备脚本 (Python版本)
"""

import json
import sys
from typing import Optional, Dict, Any

import requests
from requests.exceptions import RequestException


class DeviceRegistrar:
    """设备注册器"""

    def __init__(self, server: str, username: str, password: str):
        self.server = server.rstrip('/')
        self.username = username
        self.password = password
        self.token: Optional[str] = None
        self.session = requests.Session()
        self.session.headers.update({"Content-Type": "application/json"})

    def login(self) -> bool:
        """登录并获取Token"""
        print("正在登录...")
        try:
            response = self.session.post(
                f"{self.server}/api/user",
                json={"action": "login", "username": self.username, "password": self.password}
            )
            response.raise_for_status()
            data = response.json()

            self.token = data.get("token")
            if not self.token:
                print(f"❌ 登录失败: 响应中未包含token - {data}")
                return False

            # 将Token添加到后续请求的Header中
            self.session.headers.update({"Authorization": f"Bearer {self.token}"})
            print("✅ 登录成功")
            return True

        except RequestException as e:
            print(f"❌ 登录请求失败: {e}")
            return False
        except json.JSONDecodeError:
            print(f"❌ 登录响应解析失败: {response.text}")
            return False

    def register_device(
        self,
        device_id: str,
        name: str,
        product_key: str,
        group_id: str
    ) -> bool:
        """注册单个设备"""
        if not self.token:
            print("❌ 未登录，请先调用 login()")
            return False

        try:
            payload = {
                "action": "register",
                "device_id": device_id,
                "name": name,
                "product_key": product_key,
                "group_id": group_id
            }

            response = self.session.post(
                f"{self.server}/api/device",
                json=payload
            )
            response.raise_for_status()
            data = response.json()

            # 检查是否注册成功（根据实际API响应调整判断逻辑）
            if data.get("registered") or data.get("code") == 0 or "成功" in str(data):
                print(f"✅ {device_id} ({name})")
                return True
            else:
                print(f"❌ {device_id} ({name}) - {data}")
                return False

        except RequestException as e:
            print(f"❌ {device_id} ({name}) - 请求失败: {e}")
            return False
        except json.JSONDecodeError:
            print(f"❌ {device_id} ({name}) - 响应解析失败: {response.text}")
            return False

    def batch_register(self, devices: list) -> Dict[str, Any]:
        """批量注册设备"""
        print("\n开始批量注册设备...")
        print("-" * 40)

        results = {
            "total": len(devices),
            "success": 0,
            "failed": 0,
            "details": []
        }

        for device in devices:
            success = self.register_device(
                device_id=device["device_id"],
                name=device["name"],
                product_key=device["product_key"],
                group_id=device["group_id"]
            )

            results["details"].append({
                "device_id": device["device_id"],
                "name": device["name"],
                "success": success
            })

            if success:
                results["success"] += 1
            else:
                results["failed"] += 1

        return results


def main():
    """主函数"""
    # 配置
    SERVER = "http://127.0.0.1:8080"
    USERNAME = "admin"
    PASSWORD = "admin@123"

    print("=== 批量注册测试设备 ===")
    print(f"服务器: {SERVER}\n")

    # 创建设备注册器
    registrar = DeviceRegistrar(SERVER, USERNAME, PASSWORD)

    # 登录
    if not registrar.login():
        sys.exit(1)

    # 定义要注册的设备列表
    devices = [
        {"device_id": "dev_001", "name": "车间1温湿度", "product_key": "factory_sensor", "group_id": "3"},
        {"device_id": "dev_002", "name": "车间1电表", "product_key": "smart_meter", "group_id": "3"},
        {"device_id": "dev_003", "name": "车间2温湿度", "product_key": "factory_sensor", "group_id": "4"},
        {"device_id": "dev_004", "name": "仓库温湿度", "product_key": "env_monitor", "group_id": "8"},
        {"device_id": "dev_005", "name": "仓库烟感", "product_key": "factory_sensor", "group_id": "8"},
        {"device_id": "dev_006", "name": "机房温度", "product_key": "env_monitor", "group_id": "7"},
        {"device_id": "dev_007", "name": "宿舍空调", "product_key": "smart_meter", "group_id": "9"},
        {"device_id": "dev_008", "name": "宿舍电表", "product_key": "smart_meter", "group_id": "9"},
        {"device_id": "dev_009", "name": "门禁主机", "product_key": "factory_sensor", "group_id": "9"},
        {"device_id": "dev_010", "name": "摄像头NVR", "product_key": "factory_sensor", "group_id": "9"},
    ]

    # 执行批量注册
    results = registrar.batch_register(devices)

    # 输出统计结果
    print("\n" + "=" * 40)
    print("=== 注册完成 ===")
    print(f"总计: {results['total']} 台设备")
    print(f"✅ 成功: {results['success']} 台")
    print(f"❌ 失败: {results['failed']} 台")

    if results["failed"] > 0:
        print("\n失败设备列表:")
        for detail in results["details"]:
            if not detail["success"]:
                print(f"  - {detail['device_id']} ({detail['name']})")


if __name__ == "__main__":
    main()