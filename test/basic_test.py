import requests
import pytest
import socket
import time

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
    except requests.exceptons.ConnectionError:
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
    response = requests.post(f"{BASE_URL}/test/index.html", data={"key": "value"})
    # 根据你的配置，可能是 405 Method Not Allowed
    assert response.status_code == 405

"""
def test_fragmented_header():
    
    核心测试：模拟 Header 在传输过程中被切断 (Accept-Encodin + g: gzip)
    测试服务器的 in_buff 拼接能力。
    
    host = "127.0.0.1"
    port = 8080
    
    # 1. 创建原生 TCP Socket
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5) # 防止服务器不响应导致死锁
    s.connect((host, port))

    # 2. 发送第一部分：故意在字段名中间切断
    part1 = (
        "GET /index.html HTTP/1.1\r\n"
        "Host: 127.0.0.1:8080\r\n"
        "User-Agent: Go-http-client/1.1\r\n"
        "Accept-Encodin"  # 故意截断
    )
    s.send(part1.encode())
    
    # 3. 关键：停顿一下，确保服务器的 poll() 触发 POLLIN 
    # 且 read() 读到了这部分不完整的数据
    time.sleep(0.5)

    # 4. 发送第二部分：补全剩下的内容并发送结尾的双换行 (\r\n\r\n)
    part2 = "g: gzip\r\n\r\n"
    s.send(part2.encode())

    # 5. 接收响应
    try:
        response = s.recv(4096).decode()
        assert "HTTP/1.1 200 OK" in response
        assert "Content-Length" in response
    except socket.timeout:
        pytest.fail("服务器在接收碎片化 Header 后超时未响应。")
    finally:
        s.close()

def test_split_crlf_crlf():
    
    进阶测试：模拟 Header 的结束标志 \r\n\r\n 被切断的情况
    
    host = "127.0.0.1"
    port = 8080
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))

    # 发送全部 Header 及其最后一个 \r
    s.send(b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r")
    time.sleep(0.2)
    # 发送剩下的 \n\r\n
    s.send(b"\n\r\n")

    response = s.recv(4096).decode()
    assert "HTTP/1.1 200 OK" in response
    s.close()
    """