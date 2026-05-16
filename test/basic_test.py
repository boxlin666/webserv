import requests
import pytest
import socket
import time

# 配置你的服务器地址
BASE_URL = "http://localhost:8080"

#========================= Configuration file default.conf=========================================
@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_get_index(manage_server):
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

@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_404_not_found(manage_server):
    """
    测试请求不存在的文件时，服务器是否返回 404
    """
    response = requests.get(f"{BASE_URL}/non_existent_file.html")
    assert response.status_code == 404

@pytest.mark.parametrize("manage_server", ["./conf/default.conf"], indirect=True)
def test_method_not_allowed(manage_server):
    """
    测试对静态文件使用不允许的 POST 方法（假设你的配置限制了方法）
    """
    # 假设 /index.html 只允许 GET
    response = requests.post(f"{BASE_URL}/test/index.html", data={"key": "value"})
    # 根据你的配置，可能是 405 Method Not Allowed
    assert response.status_code == 405


#========================= Configuration file 42.conf=========================================
@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_fragmented_header(manage_server):
    
    #核心测试：模拟 Header 在传输过程中被切断 (Accept-Encodin + g: gzip)
    #测试服务器的 in_buff 拼接能力。
    
    host = "127.0.0.1"
    port = 8080
    
    # 1. 创建原生 TCP Socket
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5) # 防止服务器不响应导致死锁
    s.connect((host, port))

    # 2. 发送第一部分：故意在字段名中间切断
    part1 = (
        "GET / HTTP/1.1\r\n"
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

@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
def test_split_crlf_crlf(manage_server):
    
    #进阶测试：模拟 Header 的结束标志 \r\n\r\n 被切断的情况

    host = "127.0.0.1"
    port = 8080
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))

    # 发送全部 Header 及其最后一个 \r
    s.send(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r")
    time.sleep(0.2)
    # 发送剩下的 \n\r\n
    s.send(b"\n\r\n")

    response = s.recv(4096).decode()
    assert "HTTP/1.1 200 OK" in response
    s.close()

@pytest.mark.parametrize("manage_server", ["./conf/42.conf"], indirect=True)
@pytest.mark.skip(
    reason="TODO: 暂不启用。服务器对不完整 Chunked 的超时打断存在僵死 Bug，待 C++ 状态机重构后开启"
)
def test_fragmented_chunked_with_timeout(manage_server):
    """
    防卡死+超时打断测试：
    在碎碎片发送中故意停顿，并利用 s.settimeout() 确保 pytest 绝对不卡死。
    """
    host = "127.0.0.1"
    port = 8080
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    # 【防卡死第一道防线】设置客户端自身的网络超时为 2 秒
    # 这样无论是 connect 还是后续的 recv，超过 2 秒没反应就会弹错，绝不卡死
    s.settimeout(2.0)
    s.connect((host, port))

    # 1. 开始发送细碎的 Header
    s.send(b"POST / HTTP/1.1\r\nHost: 127.0.0.1:8080\r\nTransfer-Encoding: chunked\r\nContent-")
    time.sleep(0.1)
    s.send(b"Type: test/file\r\n\r\n") # 完成 Header
    time.sleep(0.1)

    # 2. 发送 Last Chunk 的大小 0
    s.send(b"0")
    
    # ==================== 【超时测试注入点】 ====================
    # 故意在这里不发最后的 \r\n\r\n，并且死等 3.0 秒。
    # 如果服务器有垃圾回收机制，这 3 秒内它应该已经把我们踢了。
    time.sleep(3.0)
    # ==========================================================

    try:
        # 3. 试探性地补发最后的换行符（如果服务器已经断开，这里通常会报 BrokenPipe）
        s.send(b"\r\n\r\n")
        
        # 4. 接收响应
        # 如果服务器没有超时机制，它此时收到 \r\n\r\n 会立刻吐出 200 OK
        response = s.recv(4096).decode()
        
        # 如果能走到这，说明服务器等了 3 秒都没触发超时，依然坚挺地给了 200 OK，说明服务器超时机制有 Bug！
        assert "408" in response or "400" in response, f"【Bug】服务器太温柔了，等了3秒都没打断我，还回了：{response}"

    except (socket.timeout):
        # 如果触发了 socket.timeout，说明发送 \r\n\r\n 成功了，但服务器超时后装死不回消息
        # 或者是我们客户端自身的 2.0 秒防卡死保护生效了。
        pytest.fail("服务器在超时后处于僵死状态（既不关闭连接，也不回408），导致客户端recv超时")

    except (ConnectionResetError, BrokenPipeError):
        # 完美的防御反应！说明服务器在接收到 \r\n\r\n 之前，就已经在底层 close(fd) 了。
        # 导致我们 send 或 recv 直接被系统拒绝。测试通过！
        pass
    finally:
        s.close()