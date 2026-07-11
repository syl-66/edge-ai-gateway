"""
weather_tools.py — 外部天气查询工具

这个工具不是直接操作硬件的, 而是调用外部 API。
展示 Agent 的"工具"不限于物理设备, 也可以是网络服务。
"""

import requests
import logging

logger = logging.getLogger("agent.tools.weather")

# 城市名 → 拼音映射 (简单映射, 用于 wttr.in 查询)
CITY_MAP = {
    "北京": "Beijing",
    "上海": "Shanghai",
    "广州": "Guangzhou",
    "深圳": "Shenzhen",
    "杭州": "Hangzhou",
    "成都": "Chengdu",
    "武汉": "Wuhan",
    "南京": "Nanjing",
    "西安": "Xian",
    "重庆": "Chongqing",
}

def get_weather_real(city: str) -> dict:
    """
    从 wttr.in 获取真实天气 (免费, 无需 API Key)。

    如果请求失败, 回退到模拟数据, 保证 Agent 不会因为外部 API 挂了就崩溃。
    """
    city_en = CITY_MAP.get(city, city)

    try:
        url = f"https://wttr.in/{city_en}?format=j1"
        resp = requests.get(url, timeout=5, headers={
            "User-Agent": "EdgeAgent/1.0"
        })

        if resp.status_code == 200:
            data = resp.json()
            current = data.get("current_condition", [{}])[0]

            return {
                "city": city,
                "temperature": float(current.get("temp_C", 0)),
                "humidity": float(current.get("humidity", 50)),
                "description": current.get("weatherDesc", [{}])[0].get("value", "未知"),
                "feels_like": float(current.get("FeelsLikeC", 0)),
                "wind_speed": current.get("windspeedKmph", "N/A"),
                "source": "wttr.in (真实数据)"
            }

        logger.warning(f"wttr.in 返回 {resp.status_code}")

    except requests.Timeout:
        logger.warning(f"天气请求超时: {city}")
    except Exception as e:
        logger.error(f"天气请求异常: {e}")

    # 回退: 模拟数据
    return {
        "city": city,
        "temperature": 25,
        "humidity": 50,
        "description": "晴 (模拟数据)",
        "source": "fallback"
    }

def get_weather_extended(city: str) -> dict:
    """
    扩展版: 返回当日预报 (最高/最低温, 降水概率等)。
    用于 Agent 做更智能的决策, 比如"明天下雨就别开窗了"。
    """
    city_en = CITY_MAP.get(city, city)

    try:
        url = f"https://wttr.in/{city_en}?format=j1"
        resp = requests.get(url, timeout=5, headers={
            "User-Agent": "EdgeAgent/1.0"
        })

        if resp.status_code == 200:
            data = resp.json()

            # 今日天气
            today = data.get("weather", [{}])[0]
            hourly = today.get("hourly", [])

            # 今日最高/最低温
            temps = [float(h.get("tempC", 0)) for h in hourly if h.get("tempC")]
            high = max(temps) if temps else 30
            low  = min(temps) if temps else 20

            # 降水概率
            rain_chances = [
                int(h.get("chanceofrain", 0))
                for h in hourly if h.get("chanceofrain")
            ]
            max_rain = max(rain_chances) if rain_chances else 0

            return {
                "city": city,
                "date": today.get("date", ""),
                "high_temp": high,
                "low_temp": low,
                "avg_temp": round((high + low) / 2, 1),
                "max_rain_chance": max_rain,
                "description": hourly[0].get("weatherDesc", [{}])[0].get("value", "未知") if hourly else "未知",
                "source": "wttr.in"
            }

    except Exception as e:
        logger.error(f"扩展天气查询失败: {e}")

    return {
        "city": city,
        "date": "today",
        "high_temp": 30,
        "low_temp": 20,
        "avg_temp": 25,
        "max_rain_chance": 10,
        "description": "晴 (模拟)",
        "source": "fallback"
    }
