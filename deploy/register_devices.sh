#!/bin/bash
# 批量注册测试设备

SERVER="http://127.0.0.1:8080"

echo "=== 批量注册测试设备 ==="
echo "服务器: $SERVER"
echo ""

register() {
    local device_id=$1
    local name=$2
    local product_key=$3
    local group_id=$4
    
    response=$(curl -s -X POST "$SERVER/api/device" \
        -H "Content-Type: application/json" \
        -d "{\"action\":\"register\",\"device_id\":\"$device_id\",\"name\":\"$name\",\"product_key\":\"$product_key\",\"group_id\":\"$group_id\"}")
    
    if echo "$response" | grep -q "registered"; then
        echo "✅ $device_id ($name)"
    else
        echo "❌ $device_id ($name) - $response"
    fi
}

register "dev_001" "车间1温湿度" "factory_sensor" "3"
register "dev_002" "车间1电表" "smart_meter" "3"
register "dev_003" "车间2温湿度" "factory_sensor" "4"
register "dev_004" "仓库温湿度" "env_monitor" "8"
register "dev_005" "仓库烟感" "factory_sensor" "8"
register "dev_006" "机房温度" "env_monitor" "7"
register "dev_007" "宿舍空调" "smart_meter" "9"
register "dev_008" "宿舍电表" "smart_meter" "9"
register "dev_009" "门禁主机" "factory_sensor" "9"
register "dev_010" "摄像头NVR" "factory_sensor" "9"

echo ""
echo "=== 注册完成 ==="
