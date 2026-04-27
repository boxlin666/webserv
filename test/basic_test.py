import requests
import pytest

# 配置你的服务器地址
BASE_URL = "http://localhost:8080"

def test_get_index():
    """
    测试基本的 GET 请求是否能正常获取主页
    """
    try:
        response = requests.get(f"{BASE_URL}/index.html")
        assert response.status_code == 200
        # 检查响应头是否符合 42 项目要求的规范
        assert "Content-Length" in response.headers
    except requests.exceptions.ConnectionError:
        pytest.fail("无法连接到服务器，请确保 webserv 已启动。")

def test_404_not_found():
    """
    测试请求不存在的文件时，服务器是否返回 404
    """
    response = requests.get(f"{BASE_URL}/non_existent_file.html")
    assert response.status_code == 404

def test_method_not_allowed():
    """
    测试对静态文件使用不允许的 POST 方法（假设你的配置限制了方法）
    """
    # 假设 /index.html 只允许 GET
    response = requests.post(f"{BASE_URL}/index.html", data={"key": "value"})
    # 根据你的配置，可能是 405 Method Not Allowed
    assert response.status_code == 405